/* GIO - GLib Input, Output and Streaming Library
 * 
 * Copyright (C) 2006-2007 Red Hat, Inc.
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
 * Author: Alexander Larsson <alexl@redhat.com>
 */

#include <config.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <poll.h>

#include <glib.h>
#include <glib/gstdio.h>
#include "gioerror.h"
#include "gsocketoutputstream.h"
#include "gcancellable.h"
#include "gsimpleasyncresult.h"
#include "gasynchelper.h"
#include "glibintl.h"

/**
 * SECTION:gsocketoutputstream
 * @short_description: Socket Output Stream
 * @see_also: #GOutputStream.
 *
 * #GSocketOutputStream implements #GOutputStream for writing to a socket, including
 * asynchronous operations.
 **/

G_DEFINE_TYPE (GSocketOutputStream, g_socket_output_stream, G_TYPE_OUTPUT_STREAM);


struct _GSocketOutputStreamPrivate {
  int fd;
  gboolean close_fd_at_close;
};

static gssize   g_socket_output_stream_write        (GOutputStream              *stream,
						     const void                 *buffer,
						     gsize                       count,
						     GCancellable               *cancellable,
						     GError                    **error);
static gboolean g_socket_output_stream_close        (GOutputStream              *stream,
						     GCancellable               *cancellable,
						     GError                    **error);
static void     g_socket_output_stream_write_async  (GOutputStream              *stream,
						     const void                 *buffer,
						     gsize                       count,
						     int                         io_priority,
						     GCancellable               *cancellable,
						     GAsyncReadyCallback         callback,
						     gpointer                    data);
static gssize   g_socket_output_stream_write_finish (GOutputStream              *stream,
						     GAsyncResult               *result,
						     GError                    **error);
static void     g_socket_output_stream_close_async  (GOutputStream              *stream,
						     int                         io_priority,
						     GCancellable               *cancellable,
						     GAsyncReadyCallback         callback,
						     gpointer                    data);
static gboolean g_socket_output_stream_close_finish (GOutputStream              *stream,
						     GAsyncResult               *result,
						     GError                    **error);


static void
g_socket_output_stream_finalize (GObject *object)
{
  GSocketOutputStream *stream;
  
  stream = G_SOCKET_OUTPUT_STREAM (object);

  if (G_OBJECT_CLASS (g_socket_output_stream_parent_class)->finalize)
    (*G_OBJECT_CLASS (g_socket_output_stream_parent_class)->finalize) (object);
}

static void
g_socket_output_stream_class_init (GSocketOutputStreamClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GOutputStreamClass *stream_class = G_OUTPUT_STREAM_CLASS (klass);
  
  g_type_class_add_private (klass, sizeof (GSocketOutputStreamPrivate));
  
  gobject_class->finalize = g_socket_output_stream_finalize;

  stream_class->write = g_socket_output_stream_write;
  stream_class->close = g_socket_output_stream_close;
  stream_class->write_async = g_socket_output_stream_write_async;
  stream_class->write_finish = g_socket_output_stream_write_finish;
  stream_class->close_async = g_socket_output_stream_close_async;
  stream_class->close_finish = g_socket_output_stream_close_finish;
}

static void
g_socket_output_stream_init (GSocketOutputStream *socket)
{
  socket->priv = G_TYPE_INSTANCE_GET_PRIVATE (socket,
					      G_TYPE_SOCKET_OUTPUT_STREAM,
					      GSocketOutputStreamPrivate);
}


/**
 * g_socket_output_stream_new:
 * @fd: socket's file descriptor.
 * @close_fd_at_close: a #gboolean.
 * 
 * Creates a new socket output stream for @fd. If @close_fd_at_close
 * is %TRUE, the socket will be closed when the output stream is destroyed.
 * 
 * Returns: #GOutputStream. If @close_fd_at_close is %TRUE, then
 * @fd will be closed when the #GOutputStream is closed.
 **/
GOutputStream *
g_socket_output_stream_new (int fd,
			   gboolean close_fd_at_close)
{
  GSocketOutputStream *stream;

  g_return_val_if_fail (fd != -1, NULL);

  stream = g_object_new (G_TYPE_SOCKET_OUTPUT_STREAM, NULL);

  stream->priv->fd = fd;
  stream->priv->close_fd_at_close = close_fd_at_close;
  
  return G_OUTPUT_STREAM (stream);
}

static gssize
g_socket_output_stream_write (GOutputStream *stream,
			      const void    *buffer,
			      gsize          count,
			      GCancellable  *cancellable,
			      GError       **error)
{
  GSocketOutputStream *socket_stream;
  gssize res;
  struct pollfd poll_fds[2];
  int poll_ret;
  int cancel_fd;

  socket_stream = G_SOCKET_OUTPUT_STREAM (stream);

  cancel_fd = g_cancellable_get_fd (cancellable);
  if (cancel_fd != -1)
    {
      do
	{
	  poll_fds[0].events = POLLOUT;
	  poll_fds[0].fd = socket_stream->priv->fd;
	  poll_fds[1].events = POLLIN;
	  poll_fds[1].fd = cancel_fd;
	  poll_ret = poll (poll_fds, 2, -1);
	}
      while (poll_ret == -1 && errno == EINTR);
      
      if (poll_ret == -1)
	{
	  g_set_error (error, G_IO_ERROR,
		       g_io_error_from_errno (errno),
		       _("Error writing to socket: %s"),
		       g_strerror (errno));
	  return -1;
	}
    }
      
  while (1)
    {
      if (g_cancellable_set_error_if_cancelled (cancellable, error))
	return -1;

      res = write (socket_stream->priv->fd, buffer, count);
      if (res == -1)
	{
	  if (errno == EINTR)
	    continue;
	  
	  g_set_error (error, G_IO_ERROR,
		       g_io_error_from_errno (errno),
		       _("Error writing to socket: %s"),
		       g_strerror (errno));
	}
      
      break;
    }
  
  return res;
}

static gboolean
g_socket_output_stream_close (GOutputStream *stream,
			      GCancellable  *cancellable,
			      GError       **error)
{
  GSocketOutputStream *socket_stream;
  int res;

  socket_stream = G_SOCKET_OUTPUT_STREAM (stream);

  if (!socket_stream->priv->close_fd_at_close)
    return TRUE;
  
  while (1)
    {
      /* This might block during the close. Doesn't seem to be a way to avoid it though. */
      res = close (socket_stream->priv->fd);
      if (res == -1)
	{
	  g_set_error (error, G_IO_ERROR,
		       g_io_error_from_errno (errno),
		       _("Error closing socket: %s"),
		       g_strerror (errno));
	}
      break;
    }

  return res != -1;
}

typedef struct {
  gsize count;
  const void *buffer;
  GAsyncReadyCallback callback;
  gpointer user_data;
  GCancellable *cancellable;
  GSocketOutputStream *stream;
} WriteAsyncData;

static gboolean
write_async_cb (WriteAsyncData *data,
		GIOCondition condition,
		int fd)
{
  GSimpleAsyncResult *simple;
  GError *error = NULL;
  gssize count_written;

  while (1)
    {
      if (g_cancellable_set_error_if_cancelled (data->cancellable, &error))
	{
	  count_written = -1;
	  break;
	}
      
      count_written = write (data->stream->priv->fd, data->buffer, data->count);
      if (count_written == -1)
	{
	  if (errno == EINTR)
	    continue;
	  
	  g_set_error (&error, G_IO_ERROR,
		       g_io_error_from_errno (errno),
		       _("Error reading from socket: %s"),
		       g_strerror (errno));
	}
      break;
    }

  simple = g_simple_async_result_new (G_OBJECT (data->stream),
				      data->callback,
				      data->user_data,
				      g_socket_output_stream_write_async);
  
  g_simple_async_result_set_op_res_gssize (simple, count_written);

  if (count_written == -1)
    {
      g_simple_async_result_set_from_error (simple, error);
      g_error_free (error);
    }

  /* Complete immediately, not in idle, since we're already in a mainloop callout */
  g_simple_async_result_complete (simple);
  g_object_unref (simple);

  return FALSE;
}

static void
g_socket_output_stream_write_async (GOutputStream      *stream,
				    const void         *buffer,
				    gsize               count,
				    int                 io_priority,
				    GCancellable       *cancellable,
				    GAsyncReadyCallback callback,
				    gpointer            user_data)
{
  GSource *source;
  GSocketOutputStream *socket_stream;
  WriteAsyncData *data;

  socket_stream = G_SOCKET_OUTPUT_STREAM (stream);

  data = g_new0 (WriteAsyncData, 1);
  data->count = count;
  data->buffer = buffer;
  data->callback = callback;
  data->user_data = user_data;
  data->cancellable = cancellable;
  data->stream = socket_stream;

  source = _g_fd_source_new (socket_stream->priv->fd,
			     POLLOUT,
			     cancellable);
  
  g_source_set_callback (source, (GSourceFunc)write_async_cb, data, g_free);
  g_source_attach (source, NULL);
  
  g_source_unref (source);
}

static gssize
g_socket_output_stream_write_finish (GOutputStream *stream,
				     GAsyncResult *result,
				     GError **error)
{
  GSimpleAsyncResult *simple;
  gssize nwritten;

  simple = G_SIMPLE_ASYNC_RESULT (result);
  g_assert (g_simple_async_result_get_source_tag (simple) == g_socket_output_stream_write_async);
  
  nwritten = g_simple_async_result_get_op_res_gssize (simple);
  return nwritten;
}

typedef struct {
  GOutputStream *stream;
  GAsyncReadyCallback callback;
  gpointer user_data;
} CloseAsyncData;

static gboolean
close_async_cb (CloseAsyncData *data)
{
  GSocketOutputStream *socket_stream;
  GSimpleAsyncResult *simple;
  GError *error = NULL;
  gboolean result;
  int res;

  socket_stream = G_SOCKET_OUTPUT_STREAM (data->stream);

  if (!socket_stream->priv->close_fd_at_close)
    {
      result = TRUE;
      goto out;
    }
  
  while (1)
    {
      res = close (socket_stream->priv->fd);
      if (res == -1)
	{
	  g_set_error (&error, G_IO_ERROR,
		       g_io_error_from_errno (errno),
		       _("Error closing socket: %s"),
		       g_strerror (errno));
	}
      break;
    }
  
  result = res != -1;
  
 out:
  simple = g_simple_async_result_new (G_OBJECT (data->stream),
				      data->callback,
				      data->user_data,
				      g_socket_output_stream_close_async);

  if (!result)
    {
      g_simple_async_result_set_from_error (simple, error);
      g_error_free (error);
    }

  /* Complete immediately, not in idle, since we're already in a mainloop callout */
  g_simple_async_result_complete (simple);
  g_object_unref (simple);
  
  return FALSE;
}

static void
g_socket_output_stream_close_async (GOutputStream       *stream,
				    int                 io_priority,
				    GCancellable       *cancellable,
				    GAsyncReadyCallback callback,
				    gpointer            user_data)
{
  GSource *idle;
  CloseAsyncData *data;

  data = g_new0 (CloseAsyncData, 1);

  data->stream = stream;
  data->callback = callback;
  data->user_data = user_data;
  
  idle = g_idle_source_new ();
  g_source_set_callback (idle, (GSourceFunc)close_async_cb, data, g_free);
  g_source_attach (idle, NULL);
  g_source_unref (idle);
}

static gboolean
g_socket_output_stream_close_finish (GOutputStream              *stream,
				     GAsyncResult              *result,
				     GError                   **error)
{
  /* Failures handled in generic close_finish code */
  return TRUE;
}
