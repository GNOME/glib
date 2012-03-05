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

#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>

#include <glib.h>
#include <glib/gstdio.h>
#include "gioerror.h"
#include "gsimpleasyncresult.h"
#include "gunixinputstream.h"
#include "gcancellable.h"
#include "gasynchelper.h"
#include "gfiledescriptorbased.h"
#include "glibintl.h"


/**
 * SECTION:gunixinputstream
 * @short_description: Streaming input operations for UNIX file descriptors
 * @include: gio/gunixinputstream.h
 * @see_also: #GInputStream
 *
 * #GUnixInputStream implements #GInputStream for reading from a UNIX
 * file descriptor, including asynchronous operations. (If the file
 * descriptor refers to a socket or pipe, this will use poll() to do
 * asynchronous I/O. If it refers to a regular file, it will fall back
 * to doing asynchronous I/O in another thread.)
 *
 * Note that <filename>&lt;gio/gunixinputstream.h&gt;</filename> belongs
 * to the UNIX-specific GIO interfaces, thus you have to use the
 * <filename>gio-unix-2.0.pc</filename> pkg-config file when using it.
 */

enum {
  PROP_0,
  PROP_FD,
  PROP_CLOSE_FD
};

static void g_unix_input_stream_pollable_iface_init (GPollableInputStreamInterface *iface);
static void g_unix_input_stream_file_descriptor_based_iface_init (GFileDescriptorBasedIface *iface);

G_DEFINE_TYPE_WITH_CODE (GUnixInputStream, g_unix_input_stream, G_TYPE_INPUT_STREAM,
			 G_IMPLEMENT_INTERFACE (G_TYPE_POLLABLE_INPUT_STREAM,
						g_unix_input_stream_pollable_iface_init)
			 G_IMPLEMENT_INTERFACE (G_TYPE_FILE_DESCRIPTOR_BASED,
						g_unix_input_stream_file_descriptor_based_iface_init)
			 )

struct _GUnixInputStreamPrivate {
  int fd;
  guint close_fd : 1;
  guint is_pipe_or_socket : 1;
};

static void     g_unix_input_stream_set_property (GObject              *object,
						  guint                 prop_id,
						  const GValue         *value,
						  GParamSpec           *pspec);
static void     g_unix_input_stream_get_property (GObject              *object,
						  guint                 prop_id,
						  GValue               *value,
						  GParamSpec           *pspec);
static gssize   g_unix_input_stream_read         (GInputStream         *stream,
						  void                 *buffer,
						  gsize                 count,
						  GCancellable         *cancellable,
						  GError              **error);
static gboolean g_unix_input_stream_close        (GInputStream         *stream,
						  GCancellable         *cancellable,
						  GError              **error);
static void     g_unix_input_stream_read_async   (GInputStream         *stream,
						  void                 *buffer,
						  gsize                 count,
						  int                   io_priority,
						  GCancellable         *cancellable,
						  GAsyncReadyCallback   callback,
						  gpointer              data);
static gssize   g_unix_input_stream_read_finish  (GInputStream         *stream,
						  GAsyncResult         *result,
						  GError              **error);
static void     g_unix_input_stream_skip_async   (GInputStream         *stream,
						  gsize                 count,
						  int                   io_priority,
						  GCancellable         *cancellable,
						  GAsyncReadyCallback   callback,
						  gpointer              data);
static gssize   g_unix_input_stream_skip_finish  (GInputStream         *stream,
						  GAsyncResult         *result,
						  GError              **error);
static void     g_unix_input_stream_close_async  (GInputStream         *stream,
						  int                   io_priority,
						  GCancellable         *cancellable,
						  GAsyncReadyCallback   callback,
						  gpointer              data);
static gboolean g_unix_input_stream_close_finish (GInputStream         *stream,
						  GAsyncResult         *result,
						  GError              **error);

static gboolean g_unix_input_stream_pollable_is_readable   (GPollableInputStream *stream);
static GSource *g_unix_input_stream_pollable_create_source (GPollableInputStream *stream,
							    GCancellable         *cancellable);

static void
g_unix_input_stream_finalize (GObject *object)
{
  G_OBJECT_CLASS (g_unix_input_stream_parent_class)->finalize (object);
}

static void
g_unix_input_stream_class_init (GUnixInputStreamClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GInputStreamClass *stream_class = G_INPUT_STREAM_CLASS (klass);
  
  g_type_class_add_private (klass, sizeof (GUnixInputStreamPrivate));

  gobject_class->get_property = g_unix_input_stream_get_property;
  gobject_class->set_property = g_unix_input_stream_set_property;
  gobject_class->finalize = g_unix_input_stream_finalize;

  stream_class->read_fn = g_unix_input_stream_read;
  stream_class->close_fn = g_unix_input_stream_close;
  stream_class->read_async = g_unix_input_stream_read_async;
  stream_class->read_finish = g_unix_input_stream_read_finish;
  if (0)
    {
      /* TODO: Implement instead of using fallbacks */
      stream_class->skip_async = g_unix_input_stream_skip_async;
      stream_class->skip_finish = g_unix_input_stream_skip_finish;
    }
  stream_class->close_async = g_unix_input_stream_close_async;
  stream_class->close_finish = g_unix_input_stream_close_finish;

  /**
   * GUnixInputStream:fd:
   *
   * The file descriptor that the stream reads from.
   *
   * Since: 2.20
   */
  g_object_class_install_property (gobject_class,
				   PROP_FD,
				   g_param_spec_int ("fd",
						     P_("File descriptor"),
						     P_("The file descriptor to read from"),
						     G_MININT, G_MAXINT, -1,
						     G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));

  /**
   * GUnixInputStream:close-fd:
   *
   * Whether to close the file descriptor when the stream is closed.
   *
   * Since: 2.20
   */
  g_object_class_install_property (gobject_class,
				   PROP_CLOSE_FD,
				   g_param_spec_boolean ("close-fd",
							 P_("Close file descriptor"),
							 P_("Whether to close the file descriptor when the stream is closed"),
							 TRUE,
							 G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));
}

static void
g_unix_input_stream_pollable_iface_init (GPollableInputStreamInterface *iface)
{
  iface->is_readable = g_unix_input_stream_pollable_is_readable;
  iface->create_source = g_unix_input_stream_pollable_create_source;
}

static void
g_unix_input_stream_file_descriptor_based_iface_init (GFileDescriptorBasedIface *iface)
{
  iface->get_fd = (int (*) (GFileDescriptorBased *))g_unix_input_stream_get_fd;
}

static void
g_unix_input_stream_set_property (GObject         *object,
				  guint            prop_id,
				  const GValue    *value,
				  GParamSpec      *pspec)
{
  GUnixInputStream *unix_stream;
  
  unix_stream = G_UNIX_INPUT_STREAM (object);

  switch (prop_id)
    {
    case PROP_FD:
      unix_stream->priv->fd = g_value_get_int (value);
      if (lseek (unix_stream->priv->fd, 0, SEEK_CUR) == -1 && errno == ESPIPE)
	unix_stream->priv->is_pipe_or_socket = TRUE;
      else
	unix_stream->priv->is_pipe_or_socket = FALSE;
      break;
    case PROP_CLOSE_FD:
      unix_stream->priv->close_fd = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
g_unix_input_stream_get_property (GObject    *object,
				  guint       prop_id,
				  GValue     *value,
				  GParamSpec *pspec)
{
  GUnixInputStream *unix_stream;

  unix_stream = G_UNIX_INPUT_STREAM (object);

  switch (prop_id)
    {
    case PROP_FD:
      g_value_set_int (value, unix_stream->priv->fd);
      break;
    case PROP_CLOSE_FD:
      g_value_set_boolean (value, unix_stream->priv->close_fd);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
g_unix_input_stream_init (GUnixInputStream *unix_stream)
{
  unix_stream->priv = G_TYPE_INSTANCE_GET_PRIVATE (unix_stream,
						   G_TYPE_UNIX_INPUT_STREAM,
						   GUnixInputStreamPrivate);

  unix_stream->priv->fd = -1;
  unix_stream->priv->close_fd = TRUE;
}

/**
 * g_unix_input_stream_new:
 * @fd: a UNIX file descriptor
 * @close_fd: %TRUE to close the file descriptor when done
 * 
 * Creates a new #GUnixInputStream for the given @fd. 
 *
 * If @close_fd is %TRUE, the file descriptor will be closed 
 * when the stream is closed.
 * 
 * Returns: a new #GUnixInputStream
 **/
GInputStream *
g_unix_input_stream_new (gint     fd,
			 gboolean close_fd)
{
  GUnixInputStream *stream;

  g_return_val_if_fail (fd != -1, NULL);

  stream = g_object_new (G_TYPE_UNIX_INPUT_STREAM,
			 "fd", fd,
			 "close-fd", close_fd,
			 NULL);

  return G_INPUT_STREAM (stream);
}

/**
 * g_unix_input_stream_set_close_fd:
 * @stream: a #GUnixInputStream
 * @close_fd: %TRUE to close the file descriptor when done
 *
 * Sets whether the file descriptor of @stream shall be closed
 * when the stream is closed.
 *
 * Since: 2.20
 */
void
g_unix_input_stream_set_close_fd (GUnixInputStream *stream,
				  gboolean          close_fd)
{
  g_return_if_fail (G_IS_UNIX_INPUT_STREAM (stream));

  close_fd = close_fd != FALSE;
  if (stream->priv->close_fd != close_fd)
    {
      stream->priv->close_fd = close_fd;
      g_object_notify (G_OBJECT (stream), "close-fd");
    }
}

/**
 * g_unix_input_stream_get_close_fd:
 * @stream: a #GUnixInputStream
 *
 * Returns whether the file descriptor of @stream will be
 * closed when the stream is closed.
 *
 * Return value: %TRUE if the file descriptor is closed when done
 *
 * Since: 2.20
 */
gboolean
g_unix_input_stream_get_close_fd (GUnixInputStream *stream)
{
  g_return_val_if_fail (G_IS_UNIX_INPUT_STREAM (stream), FALSE);

  return stream->priv->close_fd;
}

/**
 * g_unix_input_stream_get_fd:
 * @stream: a #GUnixInputStream
 *
 * Return the UNIX file descriptor that the stream reads from.
 *
 * Return value: The file descriptor of @stream
 *
 * Since: 2.20
 */
gint
g_unix_input_stream_get_fd (GUnixInputStream *stream)
{
  g_return_val_if_fail (G_IS_UNIX_INPUT_STREAM (stream), -1);
  
  return stream->priv->fd;
}

static gssize
g_unix_input_stream_read (GInputStream  *stream,
			  void          *buffer,
			  gsize          count,
			  GCancellable  *cancellable,
			  GError       **error)
{
  GUnixInputStream *unix_stream;
  gssize res = -1;
  GPollFD poll_fds[2];
  int nfds;
  int poll_ret;

  unix_stream = G_UNIX_INPUT_STREAM (stream);

  poll_fds[0].fd = unix_stream->priv->fd;
  poll_fds[0].events = G_IO_IN;
  if (unix_stream->priv->is_pipe_or_socket &&
      g_cancellable_make_pollfd (cancellable, &poll_fds[1]))
    nfds = 2;
  else
    nfds = 1;

  while (1)
    {
      poll_fds[0].revents = poll_fds[1].revents = 0;
      do
	poll_ret = g_poll (poll_fds, nfds, -1);
      while (poll_ret == -1 && errno == EINTR);

      if (poll_ret == -1)
	{
          int errsv = errno;

	  g_set_error (error, G_IO_ERROR,
		       g_io_error_from_errno (errsv),
		       _("Error reading from file descriptor: %s"),
		       g_strerror (errsv));
	  break;
	}

      if (g_cancellable_set_error_if_cancelled (cancellable, error))
	break;

      if (!poll_fds[0].revents)
	continue;

      res = read (unix_stream->priv->fd, buffer, count);
      if (res == -1)
	{
          int errsv = errno;

	  if (errsv == EINTR || errsv == EAGAIN)
	    continue;

	  g_set_error (error, G_IO_ERROR,
		       g_io_error_from_errno (errsv),
		       _("Error reading from file descriptor: %s"),
		       g_strerror (errsv));
	}

      break;
    }

  if (nfds == 2)
    g_cancellable_release_fd (cancellable);
  return res;
}

static gboolean
g_unix_input_stream_close (GInputStream  *stream,
			   GCancellable  *cancellable,
			   GError       **error)
{
  GUnixInputStream *unix_stream;
  int res;

  unix_stream = G_UNIX_INPUT_STREAM (stream);

  if (!unix_stream->priv->close_fd)
    return TRUE;
  
  while (1)
    {
      /* This might block during the close. Doesn't seem to be a way to avoid it though. */
      res = close (unix_stream->priv->fd);
      if (res == -1)
        {
          int errsv = errno;

          g_set_error (error, G_IO_ERROR,
                       g_io_error_from_errno (errsv),
                       _("Error closing file descriptor: %s"),
                       g_strerror (errsv));
        }
      break;
    }
  
  return res != -1;
}

typedef struct {
  gsize count;
  void *buffer;
  GAsyncReadyCallback callback;
  gpointer user_data;
  GCancellable *cancellable;
  GUnixInputStream *stream;
} ReadAsyncData;

static gboolean
read_async_cb (int            fd,
               GIOCondition   condition,
               ReadAsyncData *data)
{
  GSimpleAsyncResult *simple;
  GError *error = NULL;
  gssize count_read;

  /* We know that we can read from fd once without blocking */
  while (1)
    {
      if (g_cancellable_set_error_if_cancelled (data->cancellable, &error))
	{
	  count_read = -1;
	  break;
	}
      count_read = read (data->stream->priv->fd, data->buffer, data->count);
      if (count_read == -1)
	{
          int errsv = errno;

	  if (errsv == EINTR || errsv == EAGAIN)
	    return TRUE;
	  
	  g_set_error (&error, G_IO_ERROR,
		       g_io_error_from_errno (errsv),
		       _("Error reading from file descriptor: %s"),
		       g_strerror (errsv));
	}
      break;
    }

  simple = g_simple_async_result_new (G_OBJECT (data->stream),
				      data->callback,
				      data->user_data,
				      g_unix_input_stream_read_async);

  g_simple_async_result_set_op_res_gssize (simple, count_read);

  if (count_read == -1)
    g_simple_async_result_take_error (simple, error);

  /* Complete immediately, not in idle, since we're already in a mainloop callout */
  g_simple_async_result_complete (simple);
  g_object_unref (simple);

  return FALSE;
}

static void
g_unix_input_stream_read_async (GInputStream        *stream,
				void                *buffer,
				gsize                count,
				int                  io_priority,
				GCancellable        *cancellable,
				GAsyncReadyCallback  callback,
				gpointer             user_data)
{
  GSource *source;
  GUnixInputStream *unix_stream;
  ReadAsyncData *data;

  unix_stream = G_UNIX_INPUT_STREAM (stream);

  if (!unix_stream->priv->is_pipe_or_socket)
    {
      G_INPUT_STREAM_CLASS (g_unix_input_stream_parent_class)->
	read_async (stream, buffer, count, io_priority,
		    cancellable, callback, user_data);
      return;
    }

  data = g_new0 (ReadAsyncData, 1);
  data->count = count;
  data->buffer = buffer;
  data->callback = callback;
  data->user_data = user_data;
  data->cancellable = cancellable;
  data->stream = unix_stream;

  source = _g_fd_source_new (unix_stream->priv->fd,
			     G_IO_IN,
			     cancellable);
  g_source_set_name (source, "GUnixInputStream");
  
  g_source_set_callback (source, (GSourceFunc)read_async_cb, data, g_free);
  g_source_attach (source, g_main_context_get_thread_default ());
 
  g_source_unref (source);
}

static gssize
g_unix_input_stream_read_finish (GInputStream  *stream,
				 GAsyncResult  *result,
				 GError       **error)
{
  GUnixInputStream *unix_stream = G_UNIX_INPUT_STREAM (stream);
  GSimpleAsyncResult *simple;
  gssize nread;

  if (!unix_stream->priv->is_pipe_or_socket)
    {
      return G_INPUT_STREAM_CLASS (g_unix_input_stream_parent_class)->
	read_finish (stream, result, error);
    }

  simple = G_SIMPLE_ASYNC_RESULT (result);
  g_warn_if_fail (g_simple_async_result_get_source_tag (simple) == g_unix_input_stream_read_async);
  
  nread = g_simple_async_result_get_op_res_gssize (simple);
  return nread;
}

static void
g_unix_input_stream_skip_async (GInputStream        *stream,
				gsize                count,
				int                  io_priority,
				GCancellable        *cancellable,
				GAsyncReadyCallback  callback,
				gpointer             data)
{
  g_warn_if_reached ();
  /* TODO: Not implemented */
}

static gssize
g_unix_input_stream_skip_finish  (GInputStream  *stream,
				  GAsyncResult  *result,
				  GError       **error)
{
  g_warn_if_reached ();
  return 0;
  /* TODO: Not implemented */
}


typedef struct {
  GInputStream *stream;
  GAsyncReadyCallback callback;
  gpointer user_data;
} CloseAsyncData;

static void
close_async_data_free (gpointer _data)
{
  CloseAsyncData *data = _data;

  g_free (data);
}

static gboolean
close_async_cb (CloseAsyncData *data)
{
  GUnixInputStream *unix_stream;
  GSimpleAsyncResult *simple;
  GError *error = NULL;
  gboolean result;
  int res;

  unix_stream = G_UNIX_INPUT_STREAM (data->stream);

  if (!unix_stream->priv->close_fd)
    {
      result = TRUE;
      goto out;
    }
  
  while (1)
    {
      res = close (unix_stream->priv->fd);
      if (res == -1)
        {
          int errsv = errno;

          g_set_error (&error, G_IO_ERROR,
                       g_io_error_from_errno (errsv),
                       _("Error closing file descriptor: %s"),
                       g_strerror (errsv));
        }
      break;
    }
  
  result = res != -1;

 out:
  simple = g_simple_async_result_new (G_OBJECT (data->stream),
				      data->callback,
				      data->user_data,
				      g_unix_input_stream_close_async);

  if (!result)
    g_simple_async_result_take_error (simple, error);

  /* Complete immediately, not in idle, since we're already in a mainloop callout */
  g_simple_async_result_complete (simple);
  g_object_unref (simple);
  
  return FALSE;
}

static void
g_unix_input_stream_close_async (GInputStream        *stream,
				 int                  io_priority,
				 GCancellable        *cancellable,
				 GAsyncReadyCallback  callback,
				 gpointer             user_data)
{
  GSource *idle;
  CloseAsyncData *data;

  data = g_new0 (CloseAsyncData, 1);

  data->stream = stream;
  data->callback = callback;
  data->user_data = user_data;
  
  idle = g_idle_source_new ();
  g_source_set_callback (idle, (GSourceFunc)close_async_cb, data, close_async_data_free);
  g_source_attach (idle, g_main_context_get_thread_default ());
  g_source_unref (idle);
}

static gboolean
g_unix_input_stream_close_finish (GInputStream  *stream,
				  GAsyncResult  *result,
				  GError       **error)
{
  /* Failures handled in generic close_finish code */
  return TRUE;
}

static gboolean
g_unix_input_stream_pollable_is_readable (GPollableInputStream *stream)
{
  GUnixInputStream *unix_stream = G_UNIX_INPUT_STREAM (stream);
  GPollFD poll_fd;
  gint result;

  poll_fd.fd = unix_stream->priv->fd;
  poll_fd.events = G_IO_IN;

  do
    result = g_poll (&poll_fd, 1, 0);
  while (result == -1 && errno == EINTR);

  return poll_fd.revents != 0;
}

static GSource *
g_unix_input_stream_pollable_create_source (GPollableInputStream *stream,
					    GCancellable         *cancellable)
{
  GUnixInputStream *unix_stream = G_UNIX_INPUT_STREAM (stream);
  GSource *inner_source, *pollable_source;

  pollable_source = g_pollable_source_new (G_OBJECT (stream));

  inner_source = _g_fd_source_new (unix_stream->priv->fd, G_IO_IN, cancellable);
  g_source_set_dummy_callback (inner_source);
  g_source_add_child_source (pollable_source, inner_source);
  g_source_unref (inner_source);

  return pollable_source;
}
