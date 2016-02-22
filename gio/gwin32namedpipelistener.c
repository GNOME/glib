/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright (C) 2011 Red Hat, Inc.
 * Copyright (C) 2016 NICE s.r.l.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gwin32namedpipelistener.h"
#include "gwin32namedpipeconnection.h"
#include "gasynchelper.h"

#include "glibintl.h"

#include <windows.h>

#define DEFAULT_PIPE_BUF_SIZE 4096

typedef struct
{
  gchar *pipe_name;
  HANDLE handle;
  OVERLAPPED overlapped;
  GObject *source_object;
  gboolean already_connected;
} PipeData;

typedef struct
{
  GPtrArray *named_pipes;
  GMainContext *main_context;
} GWin32NamedPipeListenerPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GWin32NamedPipeListener, g_win32_named_pipe_listener, G_TYPE_OBJECT)

static GQuark source_quark = 0;

static PipeData *
pipe_data_new (const gchar *pipe_name,
               HANDLE       handle,
               GObject     *source_object)
{
  PipeData *data;

  data = g_slice_new0 (PipeData);
  data->pipe_name = g_strdup (pipe_name);
  data->handle = handle;
  data->overlapped.hEvent = CreateEvent (NULL, /* default security attribute */
                                         TRUE, /* manual-reset event */
                                         TRUE, /* initial state = signaled */
                                         NULL); /* unnamed event object */
  if (source_object)
    data->source_object = g_object_ref (source_object);

  return data;
}

static void
pipe_data_free (PipeData *data)
{
  g_free (data->pipe_name);
  CloseHandle (data->handle);
  CloseHandle (data->overlapped.hEvent);
  g_clear_object (&data->source_object);
  g_slice_free (PipeData, data);
}

static void
g_win32_named_pipe_listener_finalize (GObject *object)
{
  GWin32NamedPipeListener *listener = G_WIN32_NAMED_PIPE_LISTENER (object);
  GWin32NamedPipeListenerPrivate *priv;

  priv = g_win32_named_pipe_listener_get_instance_private (listener);

  if (priv->main_context)
    g_main_context_unref (priv->main_context);

  g_ptr_array_free (priv->named_pipes, TRUE);

  G_OBJECT_CLASS (g_win32_named_pipe_listener_parent_class)->finalize (object);
}

static void
g_win32_named_pipe_listener_class_init (GWin32NamedPipeListenerClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = g_win32_named_pipe_listener_finalize;

  source_quark = g_quark_from_static_string ("g-win32-named-pipe-listener-source");
}

static void
g_win32_named_pipe_listener_init (GWin32NamedPipeListener *listener)
{
  GWin32NamedPipeListenerPrivate *priv;

  priv = g_win32_named_pipe_listener_get_instance_private (listener);

  priv->named_pipes = g_ptr_array_new_with_free_func ((GDestroyNotify) pipe_data_free);
}

/**
 * g_win32_named_pipe_listener_new:
 *
 * Creates a new #GWin32NamedPipeListener.
 *
 * Returns: (transfer full): a new #GWin32NamedPipeListener.
 *
 * Since: 2.48
 */
GWin32NamedPipeListener *
g_win32_named_pipe_listener_new (void)
{
  return g_object_new (G_TYPE_WIN32_NAMED_PIPE_LISTENER, NULL);
}

/**
 * g_win32_named_pipe_listener_add_named_pipe:
 * @listener: a #GWin32NamedPipeListener.
 * @pipe_name: a name for the pipe.
 * @source_object: (allow-none): Optional #GObject identifying this source
 * @error: #GError for error reporting, or %NULL to ignore.
 *
 * Adds @named_pipe to the set of named pipes that we try to accept clients
 * from.
 *
 * @source_object will be passed out in the various calls
 * to accept to identify this particular source, which is
 * useful if you're listening on multiple pipes and do
 * different things depending on what pipe is connected to.
 *
 * Returns: %TRUE on success, %FALSE on error.
 *
 * Since: 2.48
 */
gboolean
g_win32_named_pipe_listener_add_named_pipe (GWin32NamedPipeListener  *listener,
                                            const gchar              *pipe_name,
                                            GObject                  *source_object,
                                            GError                  **error)
{
  GWin32NamedPipeListenerPrivate *priv;
  gunichar2 *pipe_namew;
  PipeData *pipe_data;
  HANDLE handle;

  g_return_val_if_fail (G_IS_WIN32_NAMED_PIPE_LISTENER (listener), FALSE);
  g_return_val_if_fail (pipe_name != NULL, FALSE);

  priv = g_win32_named_pipe_listener_get_instance_private (listener);

  pipe_namew = g_utf8_to_utf16 (pipe_name, -1, NULL, NULL, NULL);

  handle = CreateNamedPipeW (pipe_namew,
                             PIPE_ACCESS_DUPLEX |
                             FILE_FLAG_OVERLAPPED,
                             PIPE_TYPE_BYTE |
                             PIPE_READMODE_BYTE |
                             PIPE_WAIT,
                             PIPE_UNLIMITED_INSTANCES,
                             DEFAULT_PIPE_BUF_SIZE,
                             DEFAULT_PIPE_BUF_SIZE,
                             0, NULL);
  g_free (pipe_namew);

  if (handle == INVALID_HANDLE_VALUE)
    {
      int errsv = GetLastError ();
      gchar *emsg = g_win32_error_message (errsv);

      g_set_error (error,
                   G_IO_ERROR,
                   g_io_error_from_win32_error (errsv),
                   _("Error creating named pipe '%s': %s"),
                   pipe_name, emsg);

      g_free (emsg);
      return FALSE;
    }

  pipe_data = pipe_data_new (pipe_name, handle, source_object);

  if (!ConnectNamedPipe (handle, &pipe_data->overlapped))
    {
      switch (GetLastError ())
      {
      case ERROR_IO_PENDING:
        break;
      case ERROR_PIPE_CONNECTED:
        pipe_data->already_connected = TRUE;
        break;
      default:
        {
          int errsv = GetLastError ();
          gchar *emsg = g_win32_error_message (errsv);

          g_set_error (error,
                       G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                       _("Failed to connect named pipe '%s': %s"),
                       pipe_name, emsg);
          g_free (emsg);
          pipe_data_free (pipe_data);

          return FALSE;
        }
      }
    }

  g_ptr_array_add (priv->named_pipes, pipe_data);

  return TRUE;
}

static gboolean
connect_ready (HANDLE   handle,
               gpointer user_data)
{
  GTask *task = user_data;
  GWin32NamedPipeListener *listener = g_task_get_source_object (task);
  GWin32NamedPipeListenerPrivate *priv;
  PipeData *pipe_data = NULL;
  gulong cbret;
  int i;

  priv = g_win32_named_pipe_listener_get_instance_private (listener);

  for (i = 0; i < priv->named_pipes->len; i++)
    {
      PipeData *pdata;

      pdata = priv->named_pipes->pdata[i];
      if (pdata->overlapped.hEvent == handle)
        {
          pipe_data = pdata;
          break;
        }
    }

  g_return_val_if_fail (pipe_data != NULL, FALSE);

  if (!GetOverlappedResult (pipe_data->handle, &pipe_data->overlapped, &cbret, FALSE))
    {
      int errsv = GetLastError ();
      gchar *emsg = g_win32_error_message (errsv);

      g_task_return_new_error (task,
                               G_IO_ERROR,
                               G_IO_ERROR_INVALID_ARGUMENT,
                               _("There was an error querying the named pipe: %s"),
                               emsg);
      g_free (emsg);
    }
  else
    {
      GWin32NamedPipeConnection *connection;

      if (pipe_data->source_object != NULL)
        g_object_set_qdata_full (G_OBJECT (task),
                                 source_quark,
                                 g_object_ref (pipe_data->source_object),
                                 g_object_unref);

      connection = g_object_new (G_TYPE_WIN32_NAMED_PIPE_CONNECTION,
                                 "handle", pipe_data->handle,
                                 "close-handle", FALSE,
                                 NULL);
      g_task_return_pointer (task, connection, g_object_unref);
    }

  g_object_unref (task);

  return FALSE;
}

static GList *
add_sources (GWin32NamedPipeListener *listener,
             GWin32HandleSourceFunc   callback,
             gpointer                 callback_data,
             GCancellable            *cancellable,
             GMainContext            *context)
{
  GWin32NamedPipeListenerPrivate *priv;
  PipeData *data;
  GSource *source;
  GList *sources;
  int i;

  priv = g_win32_named_pipe_listener_get_instance_private (listener);

  sources = NULL;
  for (i = 0; i < priv->named_pipes->len; i++)
    {
      data = priv->named_pipes->pdata[i];

      source = _g_win32_handle_create_source (data->overlapped.hEvent,
                                              cancellable);
      g_source_set_callback (source,
                             (GSourceFunc) callback,
                             callback_data, NULL);
      g_source_attach (source, context);

      sources = g_list_prepend (sources, source);
    }

  return sources;
}

static void
free_sources (GList *sources)
{
  GSource *source;
  while (sources != NULL)
    {
      source = sources->data;
      sources = g_list_delete_link (sources, sources);
      g_source_destroy (source);
      g_source_unref (source);
    }
}

struct AcceptData {
  GWin32NamedPipeListener *listener;
  GMainLoop *loop;
  PipeData *pipe_data;
};

static gboolean
accept_callback (HANDLE   handle,
                 gpointer user_data)
{
  struct AcceptData *data = user_data;
  GWin32NamedPipeListenerPrivate *priv;
  PipeData *pipe_data = NULL;
  int i;

  priv = g_win32_named_pipe_listener_get_instance_private (data->listener);

  for (i = 0; i < priv->named_pipes->len; i++)
    {
      PipeData *pdata;

      pdata = priv->named_pipes->pdata[i];
      if (pdata->overlapped.hEvent == handle)
        {
          pipe_data = pdata;
          break;
        }
    }

  data->pipe_data = pipe_data;
  g_main_loop_quit (data->loop);

  return TRUE;
}

/**
 * g_win32_named_pipe_listener_accept:
 * @listener: a #GWin32NamedPipeListener
 * @source_object: (out) (transfer none) (allow-none): location where #GObject pointer will be stored, or %NULL.
 * @cancellable: (allow-none): optional #GCancellable object, %NULL to ignore.
 * @error: #GError for error reporting, or %NULL to ignore.
 *
 * Blocks waiting for a client to connect to any of the named pipes added
 * to the listener. Returns the #GWin32NamedPipeConnection that was accepted.
 *
 * If @source_object is not %NULL it will be filled out with the source
 * object specified when the corresponding named pipe was added
 * to the listener.
 *
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned.
 *
 * Returns: (transfer full): a #GWin32NamedPipeConnection on success, %NULL on error.
 *
 * Since: 2.48
 */
GWin32NamedPipeConnection *
g_win32_named_pipe_listener_accept (GWin32NamedPipeListener  *listener,
                                    GObject                 **source_object,
                                    GCancellable             *cancellable,
                                    GError                  **error)
{
  GWin32NamedPipeListenerPrivate *priv;
  PipeData *pipe_data = NULL;
  GWin32NamedPipeConnection *connection = NULL;

  g_return_val_if_fail (G_IS_WIN32_NAMED_PIPE_LISTENER (listener), NULL);

  priv = g_win32_named_pipe_listener_get_instance_private (listener);

  if (priv->named_pipes->len == 1)
    {
      gboolean success;

      pipe_data = priv->named_pipes->pdata[0];
      success = pipe_data->already_connected;

      if (!success)
        success = WaitForSingleObject (pipe_data->overlapped.hEvent, INFINITE) == WAIT_OBJECT_0;

      if (!success)
        pipe_data = NULL;
    }
  else
    {
      int i;

      /* First we check if any of the named pipes is already connected and
       * pick the the first one.
       */
      for (i = 0; i < priv->named_pipes->len; i++)
        {
          PipeData *pdata = priv->named_pipes->pdata[i];

          if (pdata->already_connected)
            pipe_data = pdata;
        }

      if (pipe_data == NULL)
        {
          GList *sources;
          struct AcceptData data;
          GMainLoop *loop;

          if (priv->main_context == NULL)
            priv->main_context = g_main_context_new ();

          loop = g_main_loop_new (priv->main_context, FALSE);
          data.loop = loop;
          data.listener = listener;

          sources = add_sources (listener,
                                 accept_callback,
                                 &data,
                                 cancellable,
                                 priv->main_context);
          g_main_loop_run (loop);
          pipe_data = data.pipe_data;
          free_sources (sources);
          g_main_loop_unref (loop);
        }
    }

  if (pipe_data != NULL)
    {
      connection = g_object_new (G_TYPE_WIN32_NAMED_PIPE_CONNECTION,
                                 "handle", pipe_data->handle,
                                 "close-handle", FALSE,
                                 NULL);

      if (source_object)
        *source_object = pipe_data->source_object;
    }

  return connection;
}

/**
 * g_win32_named_pipe_listener_accept_async:
 * @listener: a #GWin32NamedPipeListener
 * @cancellable: (allow-none): a #GCancellable, or %NULL
 * @callback: (scope async): a #GAsyncReadyCallback
 * @user_data: (closure): user data for the callback
 *
 * This is the asynchronous version of g_win32_named_pipe_listener_accept().
 *
 * When the operation is finished @callback will be
 * called. You can then call g_win32_named_pipe_listener_accept_finish()
 * to get the result of the operation.
 *
 * Since: 2.48
 */
void
g_win32_named_pipe_listener_accept_async (GWin32NamedPipeListener *listener,
                                          GCancellable            *cancellable,
                                          GAsyncReadyCallback      callback,
                                          gpointer                 user_data)
{
  GWin32NamedPipeListenerPrivate *priv;
  PipeData *pipe_data;
  GTask *task;
  GList *sources;
  int i;

  task = g_task_new (listener, cancellable, callback, user_data);

  priv = g_win32_named_pipe_listener_get_instance_private (listener);

  /* First we check if any of the named pipes is already connected and pick the
   * the first one.
   */
  for (i = 0; i < priv->named_pipes->len; i++)
    {
      pipe_data = priv->named_pipes->pdata[i];

      if (pipe_data->already_connected)
        {
          GWin32NamedPipeConnection *connection;

          if (pipe_data->source_object)
            g_object_set_qdata_full (G_OBJECT (task),
                                     source_quark,
                                     g_object_ref (pipe_data->source_object),
                                     g_object_unref);

          connection = g_object_new (G_TYPE_WIN32_NAMED_PIPE_CONNECTION,
                                     "handle", pipe_data->handle,
                                     "close-handle", FALSE,
                                     NULL);
          g_task_return_pointer (task, connection, g_object_unref);

          return;
        }
    }

  sources = add_sources (listener,
                         connect_ready,
                         task,
                         cancellable,
                         g_main_context_get_thread_default ());
  g_task_set_task_data (task, sources, (GDestroyNotify) free_sources);
}

/**
 * g_win32_named_pipe_listener_accept_finish:
 * @listener: a #GWin32NamedPipeListener.
 * @result: a #GAsyncResult.
 * @source_object: (out) (transfer none) (allow-none): Optional #GObject identifying this source
 * @error: a #GError location to store the error occurring, or %NULL to ignore.
 *
 * Finishes an async accept operation. See g_win32_named_pipe_listener_accept_async()
 *
 * Returns: (transfer full): a #GWin32NamedPipeConnection on success, %NULL on error.
 *
 * Since: 2.48
 */
GWin32NamedPipeConnection *
g_win32_named_pipe_listener_accept_finish (GWin32NamedPipeListener  *listener,
                                           GAsyncResult             *result,
                                           GObject                 **source_object,
                                           GError                  **error)
{
  g_return_val_if_fail (G_IS_WIN32_NAMED_PIPE_LISTENER (listener), NULL);
  g_return_val_if_fail (g_task_is_valid (result, listener), NULL);

  if (source_object)
    *source_object = g_object_get_qdata (G_OBJECT (result), source_quark);

  return g_task_propagate_pointer (G_TASK (result), error);
}
