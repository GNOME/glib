/* GIO - GLib Input, Output and Streaming Library
 *
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

#include "gwin32namedpipeclient.h"
#include "gwin32namedpipeconnection.h"

#include "gtask.h"
#include "glibintl.h"

#include <Windows.h>

/**
 * SECTION:gwin32namedpipeclient
 * @short_description: Helper for connecting to a named pipe
 * @include: gio/gio.h
 * @see_also: #GWin32NamedPipeListener
 *
 * #GWin32NamedPipeClient is a lightweight high-level utility class for
 * connecting to a named pipe.
 *
 * You create a #GWin32NamedPipeClient object, set any options you want, and then
 * call a sync or async connect operation, which returns a #GWin32NamedPipeConnection
 * on success.
 *
 * As #GWin32NamedPipeClient is a lightweight object, you don't need to
 * cache it. You can just create a new one any time you need one.
 *
 * Since: 2.48
 */

typedef struct
{
  guint timeout;
} GWin32NamedPipeClientPrivate;

enum
{
  PROP_0,
  PROP_TIMEOUT,
  LAST_PROP
};

G_DEFINE_TYPE_WITH_PRIVATE (GWin32NamedPipeClient, g_win32_named_pipe_client, G_TYPE_OBJECT)

static GParamSpec *props[LAST_PROP];

static void
g_win32_named_pipe_client_get_property (GObject    *object,
                                        guint       prop_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
  GWin32NamedPipeClient *client = G_WIN32_NAMED_PIPE_CLIENT (object);
  GWin32NamedPipeClientPrivate *priv;

  priv = g_win32_named_pipe_client_get_instance_private (client);

  switch (prop_id)
    {
    case PROP_TIMEOUT:
      g_value_set_uint (value, priv->timeout);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
g_win32_named_pipe_client_set_property (GObject      *object,
                                        guint         prop_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  GWin32NamedPipeClient *client = G_WIN32_NAMED_PIPE_CLIENT (object);
  GWin32NamedPipeClientPrivate *priv;

  priv = g_win32_named_pipe_client_get_instance_private (client);

  switch (prop_id)
    {
    case PROP_TIMEOUT:
      priv->timeout = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
g_win32_named_pipe_client_class_init (GWin32NamedPipeClientClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = g_win32_named_pipe_client_get_property;
  object_class->set_property = g_win32_named_pipe_client_set_property;

  props[PROP_TIMEOUT] =
   g_param_spec_uint ("timeout",
                      P_("Timeout"),
                      P_("The timeout in milliseconds to wait"),
                      0,
                      NMPWAIT_WAIT_FOREVER,
                      NMPWAIT_WAIT_FOREVER,
                      G_PARAM_READWRITE |
                      G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, LAST_PROP, props);
}

static void
g_win32_named_pipe_client_init (GWin32NamedPipeClient *self)
{
}

/**
 * g_win32_named_pipe_client_new:
 *
 * Creates a new #GWin32NamedPipeClient.
 *
 * Returns: a #GWin32NamedPipeClient.
 *     Free the returned object with g_object_unref().
 *
 * Since: 2.48
 */
GWin32NamedPipeClient *
g_win32_named_pipe_client_new (void)
{
  return g_object_new (G_TYPE_WIN32_NAMED_PIPE_CLIENT, NULL);
}

/**
 * g_win32_named_pipe_client_connect:
 * @client: a #GWin32NamedPipeClient.
 * @pipe_name: a pipe name.
 * @cancellable: (allow-none): optional #GCancellable object, %NULL to ignore.
 * @error: #GError for error reporting, or %NULL to ignore.
 *
 * Waits until the pipe is available or the default timeout experies.
 *
 * When the pipe is available, a new #GWin32NamedPipeConnection is constructed
 * and returned.  The caller owns this new object and must drop their
 * reference to it when finished with it.
 *
 * Returns: (transfer full): a #GWin32NamedPipeConnection on success, %NULL on error.
 *
 * Since: 2.48
 */
GWin32NamedPipeConnection *
g_win32_named_pipe_client_connect (GWin32NamedPipeClient  *client,
                                   const gchar            *pipe_name,
                                   GCancellable           *cancellable,
                                   GError                **error)
{
  GWin32NamedPipeClientPrivate *priv;
  HANDLE handle = INVALID_HANDLE_VALUE;
  GWin32NamedPipeConnection *connection = NULL;
  gunichar2 *pipe_namew;

  g_return_val_if_fail (G_IS_WIN32_NAMED_PIPE_CLIENT (client), NULL);
  g_return_val_if_fail (pipe_name != NULL, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  priv = g_win32_named_pipe_client_get_instance_private (client);

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return NULL;

  pipe_namew = g_utf8_to_utf16 (pipe_name, -1, NULL, NULL, NULL);

  if (WaitNamedPipeW (pipe_namew, priv->timeout))
    {
      handle = CreateFileW (pipe_namew,
                            GENERIC_READ | GENERIC_WRITE,
                            0,
                            NULL,
                            OPEN_EXISTING,
                            0,
                            NULL);

      if (g_cancellable_set_error_if_cancelled (cancellable, error))
          goto end;

      connection = g_object_new (G_TYPE_WIN32_NAMED_PIPE_CONNECTION,
                                 "handle", handle,
                                 "close-handle", TRUE,
                                 NULL);
    }
  else
    {
      int errsv;
      gchar *err;

      errsv = GetLastError ();
      err = g_win32_error_message (errsv);
      g_set_error_literal (error, G_IO_ERROR,
                           g_io_error_from_win32_error (errsv),
                           err);
      g_free (err);
    }

end:
  g_free (pipe_namew);

  return connection;
}

static void
client_connect_thread (GTask        *task,
                       gpointer      source_object,
                       gpointer      task_data,
                       GCancellable *cancellable)
{
  GWin32NamedPipeClient *client = G_WIN32_NAMED_PIPE_CLIENT (source_object);
  const gchar *pipe_name = (const gchar *)task_data;
  GWin32NamedPipeConnection *connection;
  GError *error = NULL;

  connection = g_win32_named_pipe_client_connect (client, pipe_name,
                                                  cancellable, &error);

  if (connection == NULL)
    g_task_return_error (task, error);
  else
    g_task_return_pointer (task, connection, (GDestroyNotify)g_object_unref);

  g_object_unref(task);
}

/**
 * g_win32_named_pipe_client_connect_async:
 * @client: a #GWin32NamedPipeClient
 * @pipe_name: a pipe name.
 * @cancellable: (allow-none): a #GCancellable, or %NULL
 * @callback: (scope async): a #GAsyncReadyCallback
 * @user_data: (closure): user data for the callback
 *
 * This is the asynchronous version of g_win32_named_pipe_client_connect().
 *
 * When the operation is finished @callback will be
 * called. You can then call g_win32_named_pipe_client_connect_finish() to get
 * the result of the operation.
 *
 * Since: 2.48
 */
void
g_win32_named_pipe_client_connect_async (GWin32NamedPipeClient  *client,
                                         const gchar            *pipe_name,
                                         GCancellable           *cancellable,
                                         GAsyncReadyCallback     callback,
                                         gpointer                user_data)
{
  GTask *task;

  g_return_if_fail (G_IS_WIN32_NAMED_PIPE_CLIENT (client));
  g_return_if_fail (pipe_name != NULL);

  task = g_task_new (client, cancellable, callback, user_data);
  g_task_set_task_data (task, g_strdup (pipe_name), g_free);

  g_task_run_in_thread (task, client_connect_thread);
}

/**
 * g_win32_named_pipe_client_connect_finish:
 * @client: a #GWin32NamedPipeClient.
 * @result: a #GAsyncResult.
 * @error: a #GError location to store the error occurring, or %NULL to
 * ignore.
 *
 * Finishes an async connect operation. See g_win32_named_pipe_client_connect_async()
 *
 * Returns: (transfer full): a #GWin32NamedPipeConnection on success, %NULL on error.
 *
 * Since: 2.48
 */
GWin32NamedPipeConnection *
g_win32_named_pipe_client_connect_finish (GWin32NamedPipeClient  *client,
                                          GAsyncResult           *result,
                                          GError                **error)
{
  g_return_val_if_fail (G_IS_WIN32_NAMED_PIPE_CLIENT (client), NULL);
  g_return_val_if_fail (g_task_is_valid (result, client), NULL);

  return g_task_propagate_pointer (G_TASK (result), error);
}
