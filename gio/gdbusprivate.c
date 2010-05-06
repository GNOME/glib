/* GDBus - GLib D-Bus Library
 *
 * Copyright (C) 2008-2009 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: David Zeuthen <davidz@redhat.com>
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>

#ifdef G_OS_UNIX
#include <gio/gunixconnection.h>
#include <gio/gunixfdmessage.h>
#include "gunixcredentialsmessage.h"
#include <unistd.h>
#endif

#include "giotypes.h"
#include "gsocket.h"
#include "gdbusprivate.h"
#include "gdbusmessage.h"
#include "gdbuserror.h"
#include "gdbusintrospection.h"
#include "gasyncresult.h"
#include "gsimpleasyncresult.h"
#include "ginputstream.h"
#include "giostream.h"
#include "gsocketcontrolmessage.h"

#ifdef G_OS_UNIX
#include <unistd.h>
#include "gunixfdmessage.h"
#include "gunixconnection.h"
#endif

#include "glibintl.h"

/* ---------------------------------------------------------------------------------------------------- */

static gchar *
hexdump (const gchar *data, gsize len, guint indent)
{
 guint n, m;
 GString *ret;

 ret = g_string_new (NULL);

 for (n = 0; n < len; n += 16)
   {
     g_string_append_printf (ret, "%*s%04x: ", indent, "", n);

     for (m = n; m < n + 16; m++)
       {
         if (m > n && (m%4) == 0)
           g_string_append_c (ret, ' ');
         if (m < len)
           g_string_append_printf (ret, "%02x ", (guchar) data[m]);
         else
           g_string_append (ret, "   ");
       }

     g_string_append (ret, "   ");

     for (m = n; m < len && m < n + 16; m++)
       g_string_append_c (ret, g_ascii_isprint (data[m]) ? data[m] : '.');

     g_string_append_c (ret, '\n');
   }

 return g_string_free (ret, FALSE);
}

/* ---------------------------------------------------------------------------------------------------- */

/* Unfortunately ancillary messages are discarded when reading from a
 * socket using the GSocketInputStream abstraction. So we provide a
 * very GInputStream-ish API that uses GSocket in this case (very
 * similar to GSocketInputStream).
 */

typedef struct
{
  GSocket *socket;
  GCancellable *cancellable;

  void *buffer;
  gsize count;

  GSocketControlMessage ***messages;
  gint *num_messages;

  GSimpleAsyncResult *simple;

  gboolean from_mainloop;
} ReadWithControlData;

static void
read_with_control_data_free (ReadWithControlData *data)
{
  g_object_unref (data->socket);
  if (data->cancellable != NULL)
    g_object_unref (data->cancellable);
  g_free (data);
}

static gboolean
_g_socket_read_with_control_messages_ready (GSocket      *socket,
                                            GIOCondition  condition,
                                            gpointer      user_data)
{
  ReadWithControlData *data = user_data;
  GError *error;
  gssize result;
  GInputVector vector;

  error = NULL;
  vector.buffer = data->buffer;
  vector.size = data->count;
  result = g_socket_receive_message (data->socket,
                                     NULL, /* address */
                                     &vector,
                                     1,
                                     data->messages,
                                     data->num_messages,
                                     NULL,
                                     data->cancellable,
                                     &error);
  if (result >= 0)
    {
      g_simple_async_result_set_op_res_gssize (data->simple, result);
    }
  else
    {
      g_assert (error != NULL);
      g_simple_async_result_set_from_error (data->simple, error);
      g_error_free (error);
    }

  if (data->from_mainloop)
    g_simple_async_result_complete (data->simple);
  else
    g_simple_async_result_complete_in_idle (data->simple);

  return FALSE;
}

static void
_g_socket_read_with_control_messages (GSocket                 *socket,
                                      void                    *buffer,
                                      gsize                    count,
                                      GSocketControlMessage ***messages,
                                      gint                    *num_messages,
                                      gint                     io_priority,
                                      GCancellable            *cancellable,
                                      GAsyncReadyCallback      callback,
                                      gpointer                 user_data)
{
  ReadWithControlData *data;

  data = g_new0 (ReadWithControlData, 1);
  data->socket = g_object_ref (socket);
  data->cancellable = cancellable != NULL ? g_object_ref (cancellable) : NULL;
  data->buffer = buffer;
  data->count = count;
  data->messages = messages;
  data->num_messages = num_messages;

  data->simple = g_simple_async_result_new (G_OBJECT (socket),
                                            callback,
                                            user_data,
                                            _g_socket_read_with_control_messages);

  if (!g_socket_condition_check (socket, G_IO_IN))
    {
      GSource *source;
      data->from_mainloop = TRUE;
      source = g_socket_create_source (data->socket,
                                       G_IO_IN | G_IO_HUP | G_IO_ERR,
                                       cancellable);
      g_source_set_callback (source,
                             (GSourceFunc) _g_socket_read_with_control_messages_ready,
                             data,
                             (GDestroyNotify) read_with_control_data_free);
      g_source_attach (source, g_main_context_get_thread_default ());
      g_source_unref (source);
    }
  else
    {
      _g_socket_read_with_control_messages_ready (data->socket, G_IO_IN, data);
      read_with_control_data_free (data);
    }
}

static gssize
_g_socket_read_with_control_messages_finish (GSocket       *socket,
                                             GAsyncResult  *result,
                                             GError       **error)
{
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (result);

  g_return_val_if_fail (G_IS_SOCKET (socket), -1);
  g_warn_if_fail (g_simple_async_result_get_source_tag (simple) == _g_socket_read_with_control_messages);

  if (g_simple_async_result_propagate_error (simple, error))
      return -1;
  else
    return g_simple_async_result_get_op_res_gssize (simple);
}

/* ---------------------------------------------------------------------------------------------------- */

G_LOCK_DEFINE_STATIC (shared_thread_lock);

typedef struct
{
  gint num_users;
  GThread *thread;
  GMainContext *context;
  GMainLoop *loop;
} SharedThreadData;

static SharedThreadData *shared_thread_data = NULL;

static gpointer
shared_thread_func (gpointer data)
{
  g_main_context_push_thread_default (shared_thread_data->context);
  g_main_loop_run (shared_thread_data->loop);
  g_main_context_pop_thread_default (shared_thread_data->context);
  return NULL;
}

typedef void (*GDBusSharedThreadFunc) (gpointer user_data);

typedef struct
{
  GDBusSharedThreadFunc func;
  gpointer              user_data;
  gboolean              done;
} CallerData;

static gboolean
invoke_caller (gpointer user_data)
{
  CallerData *data = user_data;
  data->func (data->user_data);
  data->done = TRUE;
  return FALSE;
}

static void
_g_dbus_shared_thread_ref (GDBusSharedThreadFunc func,
                           gpointer              user_data)
{
  GError *error;
  GSource *idle_source;
  CallerData *data;

  G_LOCK (shared_thread_lock);

  if (shared_thread_data != NULL)
    {
      shared_thread_data->num_users += 1;
      goto have_thread;
    }

  shared_thread_data = g_new0 (SharedThreadData, 1);
  shared_thread_data->num_users = 1;

  error = NULL;
  shared_thread_data->context = g_main_context_new ();
  shared_thread_data->loop = g_main_loop_new (shared_thread_data->context, FALSE);
  shared_thread_data->thread = g_thread_create (shared_thread_func,
                                                NULL,
                                                TRUE,
                                                &error);
  g_assert_no_error (error);

 have_thread:

  data = g_new0 (CallerData, 1);
  data->func = func;
  data->user_data = user_data;
  data->done = FALSE;

  idle_source = g_idle_source_new ();
  g_source_set_priority (idle_source, G_PRIORITY_DEFAULT);
  g_source_set_callback (idle_source,
                         invoke_caller,
                         data,
                         NULL);
  g_source_attach (idle_source, shared_thread_data->context);
  g_source_unref (idle_source);

  /* wait for the user code to run.. hmm.. probably use a condition variable instead */
  while (!data->done)
    g_thread_yield ();

  g_free (data);

  G_UNLOCK (shared_thread_lock);
}

static void
_g_dbus_shared_thread_unref (void)
{
  /* TODO: actually destroy the shared thread here */
#if 0
  G_LOCK (shared_thread_lock);
  g_assert (shared_thread_data != NULL);
  shared_thread_data->num_users -= 1;
  if (shared_thread_data->num_users == 0)
    {
      g_main_loop_quit (shared_thread_data->loop);
      //g_thread_join (shared_thread_data->thread);
      g_main_loop_unref (shared_thread_data->loop);
      g_main_context_unref (shared_thread_data->context);
      g_free (shared_thread_data);
      shared_thread_data = NULL;
      G_UNLOCK (shared_thread_lock);
    }
  else
    {
      G_UNLOCK (shared_thread_lock);
    }
#endif
}

/* ---------------------------------------------------------------------------------------------------- */

struct GDBusWorker
{
  volatile gint                       ref_count;
  gboolean                            stopped;
  GIOStream                          *stream;
  GDBusCapabilityFlags                capabilities;
  GCancellable                       *cancellable;
  GDBusWorkerMessageReceivedCallback  message_received_callback;
  GDBusWorkerDisconnectedCallback     disconnected_callback;
  gpointer                            user_data;

  GThread                            *thread;

  /* if not NULL, stream is GSocketConnection */
  GSocket *socket;

  /* used for reading */
  GMutex                             *read_lock;
  gchar                              *read_buffer;
  gsize                               read_buffer_allocated_size;
  gsize                               read_buffer_cur_size;
  gsize                               read_buffer_bytes_wanted;
  GUnixFDList                        *read_fd_list;
  GSocketControlMessage             **read_ancillary_messages;
  gint                                read_num_ancillary_messages;

  /* used for writing */
  GMutex                             *write_lock;
  GQueue                             *write_queue;
  gboolean                            write_is_pending;
};

struct _MessageToWriteData ;
typedef struct _MessageToWriteData MessageToWriteData;

static void message_to_write_data_free (MessageToWriteData *data);

static GDBusWorker *
_g_dbus_worker_ref (GDBusWorker *worker)
{
  g_atomic_int_inc (&worker->ref_count);
  return worker;
}

static void
_g_dbus_worker_unref (GDBusWorker *worker)
{
  if (g_atomic_int_dec_and_test (&worker->ref_count))
    {
      _g_dbus_shared_thread_unref ();

      g_object_unref (worker->stream);

      g_mutex_free (worker->read_lock);
      g_object_unref (worker->cancellable);
      if (worker->read_fd_list != NULL)
        g_object_unref (worker->read_fd_list);

      g_mutex_free (worker->write_lock);
      g_queue_foreach (worker->write_queue,
                       (GFunc) message_to_write_data_free,
                       NULL);
      g_queue_free (worker->write_queue);
      g_free (worker);
    }
}

static void
_g_dbus_worker_emit_disconnected (GDBusWorker  *worker,
                                  gboolean      remote_peer_vanished,
                                  GError       *error)
{
  if (!worker->stopped)
    worker->disconnected_callback (worker, remote_peer_vanished, error, worker->user_data);
}

static void
_g_dbus_worker_emit_message (GDBusWorker  *worker,
                             GDBusMessage *message)
{
  if (!worker->stopped)
    worker->message_received_callback (worker, message, worker->user_data);
}

static void _g_dbus_worker_do_read_unlocked (GDBusWorker *worker);

/* called in private thread shared by all GDBusConnection instances (without read-lock held) */
static void
_g_dbus_worker_do_read_cb (GInputStream  *input_stream,
                           GAsyncResult  *res,
                           gpointer       user_data)
{
  GDBusWorker *worker = user_data;
  GError *error;
  gssize bytes_read;

  g_mutex_lock (worker->read_lock);

  /* If already stopped, don't even process the reply */
  if (worker->stopped)
    goto out;

  error = NULL;
  if (worker->socket == NULL)
    bytes_read = g_input_stream_read_finish (g_io_stream_get_input_stream (worker->stream),
                                             res,
                                             &error);
  else
    bytes_read = _g_socket_read_with_control_messages_finish (worker->socket,
                                                              res,
                                                              &error);
  if (worker->read_num_ancillary_messages > 0)
    {
      gint n;
      for (n = 0; n < worker->read_num_ancillary_messages; n++)
        {
          GSocketControlMessage *control_message = G_SOCKET_CONTROL_MESSAGE (worker->read_ancillary_messages[n]);

          if (FALSE)
            {
            }
#ifdef G_OS_UNIX
          else if (G_IS_UNIX_FD_MESSAGE (control_message))
            {
              GUnixFDMessage *fd_message;
              gint *fds;
              gint num_fds;

              fd_message = G_UNIX_FD_MESSAGE (control_message);
              fds = g_unix_fd_message_steal_fds (fd_message, &num_fds);
              if (worker->read_fd_list == NULL)
                {
                  worker->read_fd_list = g_unix_fd_list_new_from_array (fds, num_fds);
                }
              else
                {
                  gint n;
                  for (n = 0; n < num_fds; n++)
                    {
                      /* TODO: really want a append_steal() */
                      g_unix_fd_list_append (worker->read_fd_list, fds[n], NULL);
                      close (fds[n]);
                    }
                }
              g_free (fds);
            }
          else if (G_IS_UNIX_CREDENTIALS_MESSAGE (control_message))
            {
              /* do nothing */
            }
#endif
          else
            {
              if (error == NULL)
                {
                  g_set_error (&error,
                               G_IO_ERROR,
                               G_IO_ERROR_FAILED,
                               "Unexpected ancillary message of type %s received from peer",
                               g_type_name (G_TYPE_FROM_INSTANCE (control_message)));
                  _g_dbus_worker_emit_disconnected (worker, TRUE, error);
                  g_error_free (error);
                  g_object_unref (control_message);
                  n++;
                  while (n < worker->read_num_ancillary_messages)
                    g_object_unref (worker->read_ancillary_messages[n++]);
                  g_free (worker->read_ancillary_messages);
                  goto out;
                }
            }
          g_object_unref (control_message);
        }
      g_free (worker->read_ancillary_messages);
    }

  if (bytes_read == -1)
    {
      _g_dbus_worker_emit_disconnected (worker, TRUE, error);
      g_error_free (error);
      goto out;
    }

#if 0
  g_debug ("read %d bytes (is_closed=%d blocking=%d condition=0x%02x) stream %p, %p",
           (gint) bytes_read,
           g_socket_is_closed (g_socket_connection_get_socket (G_SOCKET_CONNECTION (worker->stream))),
           g_socket_get_blocking (g_socket_connection_get_socket (G_SOCKET_CONNECTION (worker->stream))),
           g_socket_condition_check (g_socket_connection_get_socket (G_SOCKET_CONNECTION (worker->stream)),
                                     G_IO_IN | G_IO_OUT | G_IO_HUP),
           worker->stream,
           worker);
#endif

  /* TODO: hmm, hmm... */
  if (bytes_read == 0)
    {
      g_set_error (&error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Underlying GIOStream returned 0 bytes on an async read");
      _g_dbus_worker_emit_disconnected (worker, TRUE, error);
      g_error_free (error);
      goto out;
    }

  worker->read_buffer_cur_size += bytes_read;
  if (worker->read_buffer_bytes_wanted == worker->read_buffer_cur_size)
    {
      /* OK, got what we asked for! */
      if (worker->read_buffer_bytes_wanted == 16)
        {
          gssize message_len;
          /* OK, got the header - determine how many more bytes are needed */
          error = NULL;
          message_len = g_dbus_message_bytes_needed ((guchar *) worker->read_buffer,
                                                     16,
                                                     &error);
          if (message_len == -1)
            {
              g_warning ("_g_dbus_worker_do_read_cb: error determing bytes needed: %s", error->message);
              _g_dbus_worker_emit_disconnected (worker, FALSE, error);
              g_error_free (error);
              goto out;
            }

          worker->read_buffer_bytes_wanted = message_len;
          _g_dbus_worker_do_read_unlocked (worker);
        }
      else
        {
          GDBusMessage *message;
          error = NULL;

          /* TODO: use connection->priv->auth to decode the message */

          message = g_dbus_message_new_from_blob ((guchar *) worker->read_buffer,
                                                  worker->read_buffer_cur_size,
                                                  &error);
          if (message == NULL)
            {
              _g_dbus_worker_emit_disconnected (worker, FALSE, error);
              g_error_free (error);
              goto out;
            }

          if (worker->read_fd_list != NULL)
            {
              g_dbus_message_set_unix_fd_list (message, worker->read_fd_list);
              worker->read_fd_list = NULL;
            }

          if (G_UNLIKELY (_g_dbus_debug_message ()))
            {
              gchar *s;
              g_print ("========================================================================\n"
                       "GDBus-debug:Message:\n"
                       "  <<<< RECEIVED D-Bus message (%" G_GSIZE_FORMAT " bytes)\n",
                       worker->read_buffer_cur_size);
              s = g_dbus_message_print (message, 2);
              g_print ("%s", s);
              g_free (s);
              s = hexdump (worker->read_buffer, worker->read_buffer_cur_size, 2);
              g_print ("%s\n", s);
              g_free (s);
            }

          /* yay, got a message, go deliver it */
          _g_dbus_worker_emit_message (worker, message);
          g_object_unref (message);

          /* start reading another message! */
          worker->read_buffer_bytes_wanted = 0;
          worker->read_buffer_cur_size = 0;
          _g_dbus_worker_do_read_unlocked (worker);
        }
    }
  else
    {
      /* didn't get all the bytes we requested - so repeat the request... */
      _g_dbus_worker_do_read_unlocked (worker);
    }

 out:
  g_mutex_unlock (worker->read_lock);

  /* gives up the reference acquired when calling g_input_stream_read_async() */
  _g_dbus_worker_unref (worker);
}

/* called in private thread shared by all GDBusConnection instances (with read-lock held) */
static void
_g_dbus_worker_do_read_unlocked (GDBusWorker *worker)
{
  /* if bytes_wanted is zero, it means start reading a message */
  if (worker->read_buffer_bytes_wanted == 0)
    {
      worker->read_buffer_cur_size = 0;
      worker->read_buffer_bytes_wanted = 16;
    }

  /* ensure we have a (big enough) buffer */
  if (worker->read_buffer == NULL || worker->read_buffer_bytes_wanted > worker->read_buffer_allocated_size)
    {
      /* TODO: 4096 is randomly chosen; might want a better chosen default minimum */
      worker->read_buffer_allocated_size = MAX (worker->read_buffer_bytes_wanted, 4096);
      worker->read_buffer = g_realloc (worker->read_buffer, worker->read_buffer_allocated_size);
    }

  if (worker->socket == NULL)
    g_input_stream_read_async (g_io_stream_get_input_stream (worker->stream),
                               worker->read_buffer + worker->read_buffer_cur_size,
                               worker->read_buffer_bytes_wanted - worker->read_buffer_cur_size,
                               G_PRIORITY_DEFAULT,
                               worker->cancellable,
                               (GAsyncReadyCallback) _g_dbus_worker_do_read_cb,
                               _g_dbus_worker_ref (worker));
  else
    {
      worker->read_ancillary_messages = NULL;
      worker->read_num_ancillary_messages = 0;
      _g_socket_read_with_control_messages (worker->socket,
                                            worker->read_buffer + worker->read_buffer_cur_size,
                                            worker->read_buffer_bytes_wanted - worker->read_buffer_cur_size,
                                            &worker->read_ancillary_messages,
                                            &worker->read_num_ancillary_messages,
                                            G_PRIORITY_DEFAULT,
                                            worker->cancellable,
                                            (GAsyncReadyCallback) _g_dbus_worker_do_read_cb,
                                            _g_dbus_worker_ref (worker));
    }
}

/* called in private thread shared by all GDBusConnection instances (without read-lock held) */
static void
_g_dbus_worker_do_read (GDBusWorker *worker)
{
  g_mutex_lock (worker->read_lock);
  _g_dbus_worker_do_read_unlocked (worker);
  g_mutex_unlock (worker->read_lock);
}

/* ---------------------------------------------------------------------------------------------------- */

struct _MessageToWriteData
{
  GDBusMessage *message;
  gchar        *blob;
  gsize         blob_size;
};

static void
message_to_write_data_free (MessageToWriteData *data)
{
  g_object_unref (data->message);
  g_free (data->blob);
  g_free (data);
}

/* ---------------------------------------------------------------------------------------------------- */

/* called in private thread shared by all GDBusConnection instances (with write-lock held) */
static gboolean
write_message (GDBusWorker         *worker,
               MessageToWriteData  *data,
               GError             **error)
{
  gboolean ret;

  g_return_val_if_fail (data->blob_size > 16, FALSE);

  ret = FALSE;

  /* First, the initial 16 bytes - special case UNIX sockets here
   * since it may involve writing an ancillary message with file
   * descriptors
   */
#ifdef G_OS_UNIX
  {
    GOutputVector vector;
    GSocketControlMessage *message;
    GUnixFDList *fd_list;
    gssize bytes_written;

    fd_list = g_dbus_message_get_unix_fd_list (data->message);

    message = NULL;
    if (fd_list != NULL)
      {
        if (!G_IS_UNIX_CONNECTION (worker->stream))
          {
            g_set_error (error,
                         G_IO_ERROR,
                         G_IO_ERROR_INVALID_ARGUMENT,
                         "Tried sending a file descriptor on unsupported stream of type %s",
                         g_type_name (G_TYPE_FROM_INSTANCE (worker->stream)));
            goto out;
          }
        else if (!(worker->capabilities & G_DBUS_CAPABILITY_FLAGS_UNIX_FD_PASSING))
          {
            g_set_error_literal (error,
                                 G_IO_ERROR,
                                 G_IO_ERROR_INVALID_ARGUMENT,
                                 "Tried sending a file descriptor but remote peer does not support this capability");
            goto out;
          }
        message = g_unix_fd_message_new_with_fd_list (fd_list);
      }

    vector.buffer = data->blob;
    vector.size = 16;

    bytes_written = g_socket_send_message (worker->socket,
                                           NULL, /* address */
                                           &vector,
                                           1,
                                           message != NULL ? &message : NULL,
                                           message != NULL ? 1 : 0,
                                           G_SOCKET_MSG_NONE,
                                           worker->cancellable,
                                           error);
    if (bytes_written == -1)
      {
        g_prefix_error (error, _("Error writing first 16 bytes of message to socket: "));
        if (message != NULL)
          g_object_unref (message);
        goto out;
      }
    if (message != NULL)
      g_object_unref (message);

    if (bytes_written < 16)
      {
        /* TODO: I think this needs to be handled ... are we guaranteed that the ancillary
         * messages are sent?
         */
        g_assert_not_reached ();
      }
  }
#else
  /* write the first 16 bytes (guaranteed to return an error if everything can't be written) */
  if (!g_output_stream_write_all (g_io_stream_get_output_stream (worker->stream),
                                  (const gchar *) data->blob,
                                  16,
                                  NULL, /* bytes_written */
                                  worker->cancellable, /* cancellable */
                                  error))
    goto out;
#endif

  /* Then write the rest of the message (guaranteed to return an error if everything can't be written) */
  if (!g_output_stream_write_all (g_io_stream_get_output_stream (worker->stream),
                                  (const gchar *) data->blob + 16,
                                  data->blob_size - 16,
                                  NULL, /* bytes_written */
                                  worker->cancellable, /* cancellable */
                                  error))
    goto out;

  ret = TRUE;

  if (G_UNLIKELY (_g_dbus_debug_message ()))
    {
      gchar *s;
      g_print ("========================================================================\n"
               "GDBus-debug:Message:\n"
               "  >>>> SENT D-Bus message (%" G_GSIZE_FORMAT " bytes)\n",
               data->blob_size);
      s = g_dbus_message_print (data->message, 2);
      g_print ("%s", s);
      g_free (s);
      s = hexdump (data->blob, data->blob_size, 2);
      g_print ("%s\n", s);
      g_free (s);
    }

 out:
  return ret;
}

/* ---------------------------------------------------------------------------------------------------- */

/* called in private thread shared by all GDBusConnection instances (without write-lock held) */
static gboolean
write_message_in_idle_cb (gpointer user_data)
{
  GDBusWorker *worker = user_data;
  gboolean more_writes_are_pending;
  MessageToWriteData *data;
  GError *error;

  g_mutex_lock (worker->write_lock);

  data = g_queue_pop_head (worker->write_queue);
  g_assert (data != NULL);

  error = NULL;
  if (!write_message (worker,
                      data,
                      &error))
    {
      /* TODO: handle */
      _g_dbus_worker_emit_disconnected (worker, TRUE, error);
      g_error_free (error);
    }
  message_to_write_data_free (data);

  more_writes_are_pending = (g_queue_get_length (worker->write_queue) > 0);

  worker->write_is_pending = more_writes_are_pending;
  g_mutex_unlock (worker->write_lock);

  return more_writes_are_pending;
}

/* ---------------------------------------------------------------------------------------------------- */

/* can be called from any thread - steals blob */
void
_g_dbus_worker_send_message (GDBusWorker    *worker,
                             GDBusMessage   *message,
                             gchar          *blob,
                             gsize           blob_len)
{
  MessageToWriteData *data;

  g_return_if_fail (G_IS_DBUS_MESSAGE (message));
  g_return_if_fail (blob != NULL);
  g_return_if_fail (blob_len > 16);

  data = g_new0 (MessageToWriteData, 1);
  data->message = g_object_ref (message);
  data->blob = blob; /* steal! */
  data->blob_size = blob_len;

  g_mutex_lock (worker->write_lock);
  g_queue_push_tail (worker->write_queue, data);
  if (!worker->write_is_pending)
    {
      GSource *idle_source;

      worker->write_is_pending = TRUE;

      idle_source = g_idle_source_new ();
      g_source_set_priority (idle_source, G_PRIORITY_DEFAULT);
      g_source_set_callback (idle_source,
                             write_message_in_idle_cb,
                             _g_dbus_worker_ref (worker),
                             (GDestroyNotify) _g_dbus_worker_unref);
      g_source_attach (idle_source, shared_thread_data->context);
      g_source_unref (idle_source);
    }
  g_mutex_unlock (worker->write_lock);
}

/* ---------------------------------------------------------------------------------------------------- */

static void
_g_dbus_worker_thread_begin_func (gpointer user_data)
{
  GDBusWorker *worker = user_data;

  worker->thread = g_thread_self ();

  /* begin reading */
  _g_dbus_worker_do_read (worker);
}

GDBusWorker *
_g_dbus_worker_new (GIOStream                          *stream,
                    GDBusCapabilityFlags                capabilities,
                    GDBusWorkerMessageReceivedCallback  message_received_callback,
                    GDBusWorkerDisconnectedCallback     disconnected_callback,
                    gpointer                            user_data)
{
  GDBusWorker *worker;

  g_return_val_if_fail (G_IS_IO_STREAM (stream), NULL);
  g_return_val_if_fail (message_received_callback != NULL, NULL);
  g_return_val_if_fail (disconnected_callback != NULL, NULL);

  worker = g_new0 (GDBusWorker, 1);
  worker->ref_count = 1;

  worker->read_lock = g_mutex_new ();
  worker->message_received_callback = message_received_callback;
  worker->disconnected_callback = disconnected_callback;
  worker->user_data = user_data;
  worker->stream = g_object_ref (stream);
  worker->capabilities = capabilities;
  worker->cancellable = g_cancellable_new ();

  worker->write_lock = g_mutex_new ();
  worker->write_queue = g_queue_new ();

  if (G_IS_SOCKET_CONNECTION (worker->stream))
    worker->socket = g_socket_connection_get_socket (G_SOCKET_CONNECTION (worker->stream));

  _g_dbus_shared_thread_ref (_g_dbus_worker_thread_begin_func, worker);

  return worker;
}

/* This can be called from any thread - frees worker - guarantees no callbacks
 * will ever be issued again
 */
void
_g_dbus_worker_stop (GDBusWorker *worker)
{
  /* If we're called in the worker thread it means we are called from
   * a worker callback. This is fine, we just can't lock in that case since
   * we're already holding the lock...
   */
  if (g_thread_self () != worker->thread)
    g_mutex_lock (worker->read_lock);
  worker->stopped = TRUE;
  if (g_thread_self () != worker->thread)
    g_mutex_unlock (worker->read_lock);

  g_cancellable_cancel (worker->cancellable);
  _g_dbus_worker_unref (worker);
}

#define G_DBUS_DEBUG_AUTHENTICATION (1<<0)
#define G_DBUS_DEBUG_MESSAGE        (1<<1)
#define G_DBUS_DEBUG_ALL            0xffffffff
static gint _gdbus_debug_flags = 0;

gboolean
_g_dbus_debug_authentication (void)
{
  _g_dbus_initialize ();
  return (_gdbus_debug_flags & G_DBUS_DEBUG_AUTHENTICATION) != 0;
}

gboolean
_g_dbus_debug_message (void)
{
  _g_dbus_initialize ();
  return (_gdbus_debug_flags & G_DBUS_DEBUG_MESSAGE) != 0;
}

/*
 * _g_dbus_initialize:
 *
 * Does various one-time init things such as
 *
 *  - registering the G_DBUS_ERROR error domain
 *  - parses the G_DBUS_DEBUG environment variable
 */
void
_g_dbus_initialize (void)
{
  static volatile gsize initialized = 0;

  if (g_once_init_enter (&initialized))
    {
      volatile GQuark g_dbus_error_domain;
      const gchar *debug;

      g_dbus_error_domain = G_DBUS_ERROR;

      debug = g_getenv ("G_DBUS_DEBUG");
      if (debug != NULL)
        {
          gchar **tokens;
          guint n;
          tokens = g_strsplit (debug, ",", 0);
          for (n = 0; tokens[n] != NULL; n++)
            {
              if (g_strcmp0 (tokens[n], "authentication") == 0)
                _gdbus_debug_flags |= G_DBUS_DEBUG_AUTHENTICATION;
              else if (g_strcmp0 (tokens[n], "message") == 0)
                _gdbus_debug_flags |= G_DBUS_DEBUG_MESSAGE;
              else if (g_strcmp0 (tokens[n], "all") == 0)
                _gdbus_debug_flags |= G_DBUS_DEBUG_ALL;
            }
          g_strfreev (tokens);
        }

      g_once_init_leave (&initialized, 1);
    }
}

/* ---------------------------------------------------------------------------------------------------- */

gchar *
_g_dbus_compute_complete_signature (GDBusArgInfo **args,
                                    gboolean       include_parentheses)
{
  GString *s;
  guint n;

  if (include_parentheses)
    s = g_string_new ("(");
  else
    s = g_string_new ("");
  if (args != NULL)
    for (n = 0; args[n] != NULL; n++)
      g_string_append (s, args[n]->signature);

  if (include_parentheses)
    g_string_append_c (s, ')');

  return g_string_free (s, FALSE);
}
