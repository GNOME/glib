/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * giochannel.c: IO Channel abstraction
 * Copyright 1998 Owen Taylor
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GLib Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GLib Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GLib at ftp://ftp.gtk.org/pub/gtk/. 
 */

/* 
 * MT safe
 */

#include "config.h"
#include "giochannel.h"

#include <string.h>
#include <errno.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#undef G_DISABLE_DEPRECATED

#include "glib.h"

#include "glibintl.h"

#define G_IO_NICE_BUF_SIZE	1024

static GIOError		g_io_error_get_from_g_error	(GIOStatus    status,
							 GError      *err);
static void		g_io_channel_purge		(GIOChannel  *channel);
static GIOStatus	g_io_channel_fill_buffer	(GIOChannel  *channel,
							 GError     **err);
static GIOStatus	g_io_channel_read_line_backend	(GIOChannel  *channel,
							 gsize       *length,
							 gsize       *terminator_pos,
							 GError     **error);

void
g_io_channel_init (GIOChannel *channel)
{
  channel->ref_count = 1;
  channel->encoding = g_strdup ("UTF-8");
  channel->line_term = NULL;
  channel->buf_size = G_IO_NICE_BUF_SIZE;
  channel->read_cd = (GIConv) -1;
  channel->write_cd = (GIConv) -1;
  channel->read_buf = NULL; /* Lazy allocate buffers */
  channel->encoded_read_buf = NULL;
  channel->write_buf = NULL;
  channel->partial_write_buf[0] = '\0';
  channel->use_buffer = TRUE;
  channel->do_encode = FALSE;
  channel->close_on_unref = FALSE;
}

void 
g_io_channel_ref (GIOChannel *channel)
{
  g_return_if_fail (channel != NULL);

  channel->ref_count++;
}

void 
g_io_channel_unref (GIOChannel *channel)
{
  g_return_if_fail (channel != NULL);

  channel->ref_count--;
  if (channel->ref_count == 0)
    {
      if (channel->close_on_unref)
        g_io_channel_shutdown (channel, TRUE, NULL);
      else
        g_io_channel_purge (channel);
      g_free (channel->encoding);
      if (channel->read_cd != (GIConv) -1)
        g_iconv_close (channel->read_cd);
      if (channel->write_cd != (GIConv) -1)
        g_iconv_close (channel->write_cd);
      g_free (channel->line_term);
      if (channel->read_buf)
        g_string_free (channel->read_buf, TRUE);
      if (channel->write_buf)
        g_string_free (channel->write_buf, TRUE);
      if (channel->encoded_read_buf)
        g_string_free (channel->encoded_read_buf, TRUE);
      channel->funcs->io_free (channel);
    }
}

static GIOError
g_io_error_get_from_g_error (GIOStatus status,
			     GError *err)
{
  switch (status)
    {
      case G_IO_STATUS_NORMAL:
      case G_IO_STATUS_EOF:
        return G_IO_ERROR_NONE;
      case G_IO_STATUS_AGAIN:
        return G_IO_ERROR_AGAIN;
      case G_IO_STATUS_ERROR:
        if (err->domain != G_IO_CHANNEL_ERROR)
          return G_IO_ERROR_UNKNOWN;
        switch (err->code)
          {
            case G_IO_CHANNEL_ERROR_INVAL:
              return G_IO_ERROR_INVAL;
            default:
              return G_IO_ERROR_UNKNOWN;
          }
      default:
        g_assert_not_reached ();
        return G_IO_ERROR_UNKNOWN; /* Keep the compiler happy */
    }
}

/**
 * g_io_channel_read:
 * @channel: a #GIOChannel. 
 * @buf: a buffer to read the data into (which should be at least count bytes long).
 * @count: the number of bytes to read from the #GIOChannel.
 * @bytes_read: returns the number of bytes actually read. 
 * 
 * Reads data from a #GIOChannel. This function is depricated. New code should
 * use g_io_channel_read_chars() instead.
 * 
 * Return value: %G_IO_ERROR_NONE if the operation was successful. 
 **/
GIOError 
g_io_channel_read (GIOChannel *channel, 
		   gchar      *buf, 
		   gsize       count,
		   gsize      *bytes_read)
{
  GError *err = NULL;
  GIOError error;
  GIOStatus status;

  g_return_val_if_fail (channel != NULL, G_IO_ERROR_UNKNOWN);
  g_return_val_if_fail (bytes_read != NULL, G_IO_ERROR_UNKNOWN);

  status = channel->funcs->io_read (channel, buf, count, bytes_read, &err);

  error = g_io_error_get_from_g_error (status, err);

  if (err)
    g_error_free (err);

  return error;
}

/**
 * g_io_channel_write:
 * @channel:  a #GIOChannel.
 * @buf: the buffer containing the data to write. 
 * @count: the number of bytes to write.
 * @bytes_written:  the number of bytes actually written.
 * 
 * Writes data to a #GIOChannel. This function is depricated. New code should
 * use g_io_channel_write_chars() instead.
 * 
 * Return value:  %G_IO_ERROR_NONE if the operation was successful.
 **/
GIOError 
g_io_channel_write (GIOChannel  *channel, 
		    const gchar *buf, 
		    gsize        count,
		    gsize       *bytes_written)
{
  GError *err = NULL;
  GIOError error;
  GIOStatus status;

  g_return_val_if_fail (channel != NULL, G_IO_ERROR_UNKNOWN);
  g_return_val_if_fail (bytes_written != NULL, G_IO_ERROR_UNKNOWN);

  status = channel->funcs->io_write (channel, buf, count, bytes_written, &err);

  error = g_io_error_get_from_g_error (status, err);

  if (err)
    g_error_free (err);

  return error;
}

/**
 * g_io_channel_seek:
 * @channel: a #GIOChannel. 
 * @offset: an offset, in bytes, which is added to the position specified by @type
 * @type: the position in the file, which can be %G_SEEK_CUR (the current
 *        position), %G_SEEK_SET (the start of the file), or %G_SEEK_END (the end of the
 *        file).
 * 
 * Sets the current position in the #GIOChannel, similar to the standard library
 * function fseek(). This function is depricated. New code should
 * use g_io_channel_seek_position() instead.
 * 
 * Return value: %G_IO_ERROR_NONE if the operation was successful.
 **/
GIOError 
g_io_channel_seek  (GIOChannel   *channel,
		    glong         offset, 
		    GSeekType     type)
{
  GError *err = NULL;
  GIOError error;
  GIOStatus status;

  g_return_val_if_fail (channel != NULL, G_IO_ERROR_UNKNOWN);
  g_return_val_if_fail (channel->is_seekable, G_IO_ERROR_UNKNOWN);

  switch (type)
    {
      case G_SEEK_CUR:
      case G_SEEK_SET:
      case G_SEEK_END:
        break;
      default:
        g_warning ("g_io_channel_seek: unknown seek type");
        return G_IO_ERROR_UNKNOWN;
    }

  status = channel->funcs->io_seek (channel, offset, type, &err);

  error = g_io_error_get_from_g_error (status, err);

  if (err)
    g_error_free (err);

  return error;
}

/* The function g_io_channel_new_file() is prototyped in both
 * giounix.c and giowin32.c, so we stick its documentation here.
 */

/**
 * g_io_channel_new_file:
 * @filename: A string containing the name of a file.
 * @mode: One of "r", "w", "a", "r+", "w+", "a+". These have
 *        the same meaning as in fopen().
 * @error: A location to return an error of type %G_IO_FILE_ERROR.
 *
 * Open a file @filename as a #GIOChannel using mode @mode. This
 * channel will be closed when the last reference to it is dropped,
 * so there is no need to call g_io_channel_close() (though doing
 * so will not cause problems, as long as no attempt is made to
 * access the channel after it is closed).
 *
 * Return value: A #GIOChannel on success, %NULL on failure.
 **/

/**
 * g_io_channel_close:
 * @channel: A #GIOChannel
 * 
 * Close an IO channel. Any pending data to be written will be
 * flushed, ignoring errors. The channel will not be freed until the
 * last reference is dropped using g_io_channel_unref(). This
 * function is deprecated: you should use g_io_channel_shutdown()
 * instead.
 **/
void
g_io_channel_close (GIOChannel *channel)
{
  GError *err = NULL;
  
  g_return_if_fail (channel != NULL);

  g_io_channel_purge (channel);

  channel->funcs->io_close (channel, &err);

  if (err)
    { /* No way to return the error */
      g_warning ("Error closing channel: %s", err->message);
      g_error_free (err);
    }
  
  channel->close_on_unref = FALSE; /* Because we already did */
  channel->is_readable = FALSE;
  channel->is_writeable = FALSE;
  channel->is_seekable = FALSE;
}

/**
 * g_io_channel_shutdown:
 * @channel: a #GIOChannel
 * @flush: if %TRUE, flush pending
 * @err: location to store a #GIOChannelError
 * 
 * Close an IO channel. Any pending data to be written will be
 * flushed. The channel will not be freed until the
 * last reference is dropped using g_io_channel_unref().
 *
 * Return value:
 **/
GIOStatus
g_io_channel_shutdown (GIOChannel *channel,
		       gboolean    flush,
		       GError    **err)
{
  GIOStatus status, result;
  GError *tmperr = NULL;
  
  g_return_val_if_fail (channel != NULL, G_IO_STATUS_ERROR);
  g_return_val_if_fail (err == NULL || *err == NULL, G_IO_STATUS_ERROR);

  if (flush && channel->write_buf && channel->write_buf->len > 0)
    {
      GIOFlags flags;
      
      /* Set the channel to blocking, to avoid a busy loop
       */
      flags = g_io_channel_get_flags (channel);
      /* Ignore any errors here, they're irrelevant */
      g_io_channel_set_flags (channel, flags & ~G_IO_FLAG_NONBLOCK, NULL);

      result = g_io_channel_flush (channel, &tmperr);

      if (channel->partial_write_buf[0] != '\0')
        {
          g_warning ("Partial character at end of write buffer not flushed.\n");
          channel->partial_write_buf[0] = '\0';
        }
    }
  else
    result = G_IO_STATUS_NORMAL;

  status = channel->funcs->io_close (channel, err);

  channel->close_on_unref = FALSE; /* Because we already did */
  channel->is_readable = FALSE;
  channel->is_writeable = FALSE;
  channel->is_seekable = FALSE;

  if (status != G_IO_STATUS_NORMAL)
    {
      g_clear_error (&tmperr);
      return status;
    }
  else if (result != G_IO_STATUS_NORMAL)
    {
      g_propagate_error (err, tmperr);
      return result;
    }
  else
    return G_IO_STATUS_NORMAL;
}

/* This function is used for the final flush on close or unref */
static void
g_io_channel_purge (GIOChannel *channel)
{
  GError *err = NULL;
  GIOStatus status;

  g_return_if_fail (channel != NULL);

  if (channel->write_buf && channel->write_buf->len > 0)
    {
      GIOFlags flags;
      
      /* Set the channel to blocking, to avoid a busy loop
       */
      flags = g_io_channel_get_flags (channel);
      g_io_channel_set_flags (channel, flags & ~G_IO_FLAG_NONBLOCK, NULL);

      status = g_io_channel_flush (channel, &err);

      if (err)
	{ /* No way to return the error */
	  g_warning ("Error flushing string: %s", err->message);
	  g_error_free (err);
	}
    }

  /* Flush these in case anyone tries to close without unrefing */

  if (channel->read_buf)
    g_string_truncate (channel->read_buf, 0);
  if (channel->write_buf)
    g_string_truncate (channel->write_buf, 0);
  if (channel->encoding)
    {
      if (channel->encoded_read_buf)
        g_string_truncate (channel->encoded_read_buf, 0);

      if (channel->partial_write_buf[0] != '\0')
        {
          g_warning ("Partial character at end of write buffer not flushed.\n");
          channel->partial_write_buf[0] = '\0';
        }
    }
}

GSource *
g_io_create_watch (GIOChannel  *channel,
		   GIOCondition condition)
{
  g_return_val_if_fail (channel != NULL, NULL);

  return channel->funcs->io_create_watch (channel, condition);
}

guint 
g_io_add_watch_full (GIOChannel    *channel,
		     gint           priority,
		     GIOCondition   condition,
		     GIOFunc        func,
		     gpointer       user_data,
		     GDestroyNotify notify)
{
  GSource *source;
  guint id;
  
  g_return_val_if_fail (channel != NULL, 0);

  source = g_io_create_watch (channel, condition);

  if (priority != G_PRIORITY_DEFAULT)
    g_source_set_priority (source, priority);
  g_source_set_callback (source, (GSourceFunc)func, user_data, notify);

  id = g_source_attach (source, NULL);
  g_source_unref (source);

  return id;
}

guint 
g_io_add_watch (GIOChannel    *channel,
		GIOCondition   condition,
		GIOFunc        func,
		gpointer       user_data)
{
  return g_io_add_watch_full (channel, G_PRIORITY_DEFAULT, condition, func, user_data, NULL);
}

/**
 * g_io_channel_get_buffer_condition:
 * @channel: A #GIOChannel
 *
 * This function returns a #GIOCondition depending on the status of the
 * internal buffers in the #GIOChannel. Only the flags %G_IO_IN and
 * %G_IO_OUT will be set.
 *
 * Return value: A #GIOCondition
 **/
GIOCondition
g_io_channel_get_buffer_condition (GIOChannel *channel)
{
  GIOCondition condition = 0;

  if (channel->encoding)
    {
      if (channel->encoded_read_buf && (channel->encoded_read_buf->len > 0))
        condition |= G_IO_IN; /* Only return if we have full characters */
    }
  else
    {
      if (channel->read_buf && (channel->read_buf->len > 0))
        condition |= G_IO_IN;
    }

  if (channel->write_buf && (channel->write_buf->len < channel->buf_size))
    condition |= G_IO_OUT;

  return condition;
}

/**
 * g_io_channel_error_from_errno:
 * @en: An errno error number, e.g. EINVAL
 *
 * Return value: A #GIOChannelError error number, e.g. %G_IO_CHANNEL_ERROR_INVAL
 **/
GIOChannelError
g_io_channel_error_from_errno (gint en)
{
#ifdef EAGAIN
  g_return_val_if_fail (en != EAGAIN, G_IO_CHANNEL_ERROR_FAILED);
#endif
#ifdef EINTR
  g_return_val_if_fail (en != EINTR, G_IO_CHANNEL_ERROR_FAILED);
#endif

  switch (en)
    {
#ifdef EBADF
    case EBADF:
      g_warning("Invalid file descriptor.\n");
      return G_IO_CHANNEL_ERROR_FAILED;
#endif

#ifdef EFAULT
    case EFAULT:
      g_warning("File descriptor outside valid address space.\n");
      return G_IO_CHANNEL_ERROR_FAILED;
#endif

#ifdef EFBIG
    case EFBIG:
      return G_IO_CHANNEL_ERROR_FBIG;
#endif

#ifdef EINVAL
    case EINVAL:
      return G_IO_CHANNEL_ERROR_INVAL;
#endif

#ifdef EIO
    case EIO:
      return G_IO_CHANNEL_ERROR_IO;
#endif

#ifdef EISDIR
    case EISDIR:
      return G_IO_CHANNEL_ERROR_ISDIR;
#endif

#ifdef ENOSPC
    case ENOSPC:
      return G_IO_CHANNEL_ERROR_NOSPC;
#endif

#ifdef ENXIO
    case ENXIO:
      return G_IO_CHANNEL_ERROR_NXIO;
#endif

#ifdef EOVERFLOW
    case EOVERFLOW:
      return G_IO_CHANNEL_ERROR_OVERFLOW;
#endif

#ifdef EPIPE
    case EPIPE:
      return G_IO_CHANNEL_ERROR_PIPE;
#endif

    default:
      return G_IO_CHANNEL_ERROR_FAILED;
    }
}

/**
 * g_io_channel_set_buffer_size:
 * @channel: a #GIOChannel
 * @size: the size of the buffer. 0 == pick a good size
 *
 * Set the buffer size.
 **/  
void
g_io_channel_set_buffer_size (GIOChannel	*channel,
                              gsize		 size)
{
  g_return_if_fail (channel != NULL);

  if (size == 0)
    size = G_IO_NICE_BUF_SIZE;

  if (size < 10) /* Needs to be larger than the widest char in any encoding */
    size = 10;

  channel->buf_size = size;
}

/**
 * g_io_channel_get_buffer_size:
 * @channel: a #GIOChannel
 *
 * Get the buffer size.
 *
 * Return value: the size of the buffer.
 **/  
gsize
g_io_channel_get_buffer_size (GIOChannel	*channel)
{
  g_return_val_if_fail (channel != NULL, 0);

  return channel->buf_size;
}

/**
 * g_io_channel_set_line_term:
 * @channel: a #GIOChannel
 * @line_term: The line termination string. Use %NULL for auto detect.
 *
 * This sets the string that #GIOChannel uses to determine
 * where in the file a line break occurs.
 **/
void
g_io_channel_set_line_term (GIOChannel	*channel,
                            const gchar	*line_term)
{
  g_return_if_fail (channel != NULL);
  g_return_if_fail (!line_term || line_term[0]); /* Disallow "" */
  g_return_if_fail (!line_term || g_utf8_validate (line_term, -1, NULL));
                   /* Require valid UTF-8 */

  g_free (channel->line_term);
  channel->line_term = g_strdup (line_term);
}

/**
 * g_io_channel_get_line_term:
 * @channel: a #GIOChannel
 *
 * This returns the string that #GIOChannel uses to determine
 * where in the file a line break occurs. A value of %NULL
 * indicates auto detection.
 *
 * Return value: The line termination string. This value
 *   is owned by GLib and must not be freed.
 **/
G_CONST_RETURN gchar*
g_io_channel_get_line_term (GIOChannel	*channel)
{
  g_return_val_if_fail (channel != NULL, 0);

  return channel->line_term;
}

/**
 * g_io_channel_set_flags:
 * @channel: a #GIOChannel
 * @flags: the flags to set on the channel
 * @error: A location to return an error of type #GIOChannelError
 *
 * Return value:
 **/
GIOStatus
g_io_channel_set_flags (GIOChannel *channel,
                        GIOFlags    flags,
                        GError    **error)
{
  g_return_val_if_fail (channel != NULL, G_IO_STATUS_ERROR);
  g_return_val_if_fail ((error == NULL) || (*error == NULL),
			G_IO_STATUS_ERROR);

  return (* channel->funcs->io_set_flags)(channel,
					  flags & G_IO_FLAG_SET_MASK,
					  error);
}

/**
 * g_io_channel_get_flags:
 * @channel: a #GIOChannel
 *
 * Gets the current flags for a #GIOChannel, including read-only
 * flags such as %G_IO_FLAG_IS_READABLE.
 *
 * Return value: the flags which are set on the channel
 **/
GIOFlags
g_io_channel_get_flags (GIOChannel *channel)
{
  GIOFlags flags;

  g_return_val_if_fail (channel != NULL, G_IO_STATUS_ERROR);

  flags = (* channel->funcs->io_get_flags) (channel);

  /* Cross implementation code */

  if (channel->is_seekable)
    flags |= G_IO_FLAG_IS_SEEKABLE;
  if (channel->is_readable)
    flags |= G_IO_FLAG_IS_READABLE;
  if (channel->is_writeable)
    flags |= G_IO_FLAG_IS_WRITEABLE;

  return flags;
}

/**
 * g_io_channel_seek_position:
 * @channel: a #GIOChannel
 * @offset: The offset in bytes from the position specified by @type
 * @type: a #GSeekType. The type %G_SEEK_CUR is only allowed if
 *        the channel has the default encoding or the
 *        encoding %G_IO_CHANNEL_ENCODE_RAW for raw file access.
 * @error: A location to return an error of type #GIOChannelError
 *
 * Replacement for g_io_channel_seek() with the new API.
 *
 * Return value:
 **/
GIOStatus
g_io_channel_seek_position	(GIOChannel* channel,
				 glong       offset,
				 GSeekType   type,
				 GError    **error)
{
  GIOStatus status;

  /* For files, only one of the read and write buffers can contain data.
   * For sockets, both can contain data.
   */

  g_return_val_if_fail (channel != NULL, G_IO_STATUS_ERROR);
  g_return_val_if_fail ((error == NULL) || (*error == NULL),
			G_IO_STATUS_ERROR);
  g_return_val_if_fail (channel->is_seekable, G_IO_STATUS_ERROR);

  switch (type)
    {
      case G_SEEK_CUR: /* The user is seeking relative to the head of the buffer */
        if (channel->use_buffer)
          {
            if (channel->do_encode && channel->encoded_read_buf
                && channel->encoded_read_buf->len > 0)
              {
                g_warning ("Seek type G_SEEK_CUR not allowed for this"
                  " channel's encoding.\n");
                return G_IO_STATUS_ERROR;
              }
          if (channel->read_buf)
            offset -= channel->read_buf->len;
          if (channel->encoded_read_buf)
            {
              g_assert (channel->encoded_read_buf->len == 0 && !channel->do_encode);

              /* If there's anything here, it's because the encoding is UTF-8,
               * so we can just subtract the buffer length, the same as for
               * the unencoded data.
               */

              offset -= channel->encoded_read_buf->len;
            }
          }
        break;
      case G_SEEK_SET:
      case G_SEEK_END:
        break;
      default:
        g_warning ("g_io_channel_seek_position: unknown seek type");
        return G_IO_STATUS_ERROR;
    }

  if (channel->use_buffer)
    {
      status = g_io_channel_flush (channel, error);
      if (status != G_IO_STATUS_NORMAL)
        return status;
    }

  status = channel->funcs->io_seek (channel, offset, type, error);

  if ((status == G_IO_STATUS_NORMAL) && (channel->use_buffer))
    {
      if (channel->read_buf)
        g_string_truncate (channel->read_buf, 0);

      /* Conversion state no longer matches position in file */
      if (channel->read_cd != (GIConv) -1)
        g_iconv (channel->read_cd, NULL, NULL, NULL, NULL);
      if (channel->write_cd != (GIConv) -1)
        g_iconv (channel->write_cd, NULL, NULL, NULL, NULL);

      if (channel->encoded_read_buf)
        {
          g_assert (channel->encoded_read_buf->len == 0 || !channel->do_encode);
          g_string_truncate (channel->encoded_read_buf, 0);
        }

      if (channel->partial_write_buf[0] != '\0')
        {
          g_warning ("Partial character at end of write buffer not flushed.\n");
          channel->partial_write_buf[0] = '\0';
        }
    }

  return status;
}

/**
 * g_io_channel_flush:
 * @channel: a #GIOChannel
 * @error: location to store an error of type #GIOChannelError
 *
 * Flush the write buffer for the GIOChannel.
 *
 * Return value: the status of the operation: One of
 *   G_IO_CHANNEL_NORMAL, G_IO_CHANNEL_AGAIN, or
 *   G_IO_CHANNEL_ERROR.
 **/
GIOStatus
g_io_channel_flush (GIOChannel	*channel,
		    GError     **error)
{
  GIOStatus status;
  gsize this_time = 1, bytes_written = 0;

  g_return_val_if_fail (channel != NULL, G_IO_STATUS_ERROR);
  g_return_val_if_fail ((error == NULL) || (*error == NULL), G_IO_STATUS_ERROR);

  if (channel->write_buf == NULL || channel->write_buf->len == 0)
    return G_IO_STATUS_NORMAL;

  do
    {
      g_assert (this_time > 0);

      status = channel->funcs->io_write (channel,
                                      channel->write_buf->str + bytes_written,
                                      channel->write_buf->len - bytes_written,
                                      &this_time, error);
      bytes_written += this_time;
    }
  while ((bytes_written < channel->write_buf->len)
         && (status == G_IO_STATUS_NORMAL));

  g_string_erase (channel->write_buf, 0, bytes_written);

  return status;
}

/**
 * g_io_channel_set_buffered:
 * @channel: a #GIOChannel
 * @buffered: whether to set the channel buffered or unbuffered
 *
 * The buffering state can only be set if the channel's encoding
 * is %NULL. For any other encoding, the channel must be buffered.
 *
 * The default state of the channel is buffered.
 **/
void
g_io_channel_set_buffered	(GIOChannel *channel,
				 gboolean    buffered)
{
  g_return_if_fail (channel != NULL);

  if (channel->encoding != NULL)
    {
      g_warning ("Need to have NULL encoding to set the buffering state of the "
                 "channel.\n");
      return;
    }

  g_return_if_fail (!channel->read_buf || channel->read_buf->len == 0);
  g_return_if_fail (!channel->write_buf || channel->write_buf->len == 0);

  channel->use_buffer = buffered;
}

/**
 * g_io_channel_get_buffered:
 * @channel: a #GIOChannel
 *
 * Return Value: the buffering state of the channel
 **/
gboolean
g_io_channel_get_buffered	(GIOChannel *channel)
{
  g_return_val_if_fail (channel != NULL, FALSE);

  return channel->use_buffer;
}

/**
 * g_io_channel_set_encoding:
 * @channel: a #GIOChannel
 * @encoding: the encoding type
 * @error: location to store an error of type #GConvertError.
 *
 * Set the encoding for the input/output of the channel. The internal
 * encoding is always UTF-8. The default encoding for the
 * external file is UTF-8.
 *
 * The encoding %NULL is safe to use with binary data.
 * Encodings other than %NULL must use a buffered channel.
 * Encodings other than %NULL and UTF-8 cannot
 * use g_io_channel_seek_position() with seek type %G_SEEK_CUR,
 * and cannot mix reading and writing if the channel is
 * a file without first doing a seek of type %G_SEEK_SET or
 * %G_SEEK_END.
 *
 * The encoding can only be set under three conditions:
 *
 * 1. The channel was just created, and has not been written to
 *    or read from yet.
 *
 * 2. The channel is a file, and the file pointer was just
 *    repositioned by a call to g_io_channel_seek_position().
 *    (This flushes all the internal buffers.)
 *
 * 3. The current encoding is %NULL or UTF-8.
 *
 * Return Value: %G_IO_STATUS_NORMAL if the encoding was succesfully set.
 **/
GIOStatus
g_io_channel_set_encoding (GIOChannel	*channel,
                           const gchar	*encoding,
			   GError      **error)
{
  GIConv read_cd, write_cd;
  gboolean did_encode;

  g_return_val_if_fail (channel != NULL, G_IO_STATUS_ERROR);
  g_return_val_if_fail ((error == NULL) || (*error == NULL), G_IO_STATUS_ERROR);

  /* Make sure the encoded buffers are empty */

  g_return_val_if_fail (!channel->do_encode || !channel->encoded_read_buf ||
			channel->encoded_read_buf->len == 0, G_IO_STATUS_ERROR);
  g_return_val_if_fail (channel->partial_write_buf[0] == '\0', G_IO_STATUS_ERROR);

  if (!channel->use_buffer)
    {
      g_warning ("Need to set the channel buffered before setting the encoding.\n");
      g_warning ("Assuming this is what you meant and acting accordingly.\n");

      channel->use_buffer = TRUE;
    }

  did_encode = channel->do_encode;

  if (!encoding || strcmp (encoding, "UTF8") == 0 || strcmp (encoding, "UTF-8") == 0)
    {
      channel->do_encode = FALSE;
      read_cd = write_cd = (GIConv) -1;
    }
  else
    {
      gint err = 0;
      const gchar *from_enc = NULL, *to_enc = NULL;

      if (channel->is_readable)
        {
          read_cd = g_iconv_open ("UTF-8", encoding);

          if (read_cd == (GIConv) -1)
            {
              err = errno;
              from_enc = "UTF-8";
              to_enc = encoding;
            }
        }
      else
        read_cd = (GIConv) -1;

      if (channel->is_writeable && err == 0)
        {
          write_cd = g_iconv_open (encoding, "UTF-8");

          if (write_cd == (GIConv) -1)
            {
              err = errno;
              from_enc = encoding;
              to_enc = "UTF-8";
            }
        }
      else
        write_cd = (GIConv) -1;

      if (err != 0)
        {
          g_assert (from_enc);
          g_assert (to_enc);

          if (err == EINVAL)
            g_set_error (error, G_CONVERT_ERROR, G_CONVERT_ERROR_NO_CONVERSION,
                         _("Conversion from character set `%s' to `%s' is not supported"),
                         from_enc, to_enc);
          else
            g_set_error (error, G_CONVERT_ERROR, G_CONVERT_ERROR_FAILED,
                         _("Could not open converter from `%s' to `%s': %s"),
                         from_enc, to_enc, strerror (errno));

          if (read_cd != (GIConv) -1)
            g_iconv_close (read_cd);
          if (write_cd != (GIConv) -1)
            g_iconv_close (write_cd);

          return G_IO_STATUS_ERROR;
        }

      channel->do_encode = TRUE;
    }

  /* The encoding is ok, so set the fields in channel */

  if (channel->read_cd != (GIConv) -1)
    g_iconv_close (channel->read_cd);
  if (channel->write_cd != (GIConv) -1)
    g_iconv_close (channel->write_cd);

  if (channel->encoded_read_buf && channel->encoded_read_buf->len > 0)
    {
      g_assert (!did_encode); /* Encoding UTF-8, NULL doesn't use encoded_read_buf */

      /* This is just validated UTF-8, so we can copy it back into read_buf
       * so it can be encoded in whatever the new encoding is.
       */

      g_string_prepend_len (channel->read_buf, channel->encoded_read_buf->str,
                            channel->encoded_read_buf->len);
      g_string_truncate (channel->encoded_read_buf, 0);
    }

  channel->read_cd = read_cd;
  channel->write_cd = write_cd;

  g_free (channel->encoding);
  channel->encoding = g_strdup (encoding);

  return G_IO_STATUS_NORMAL;
}

/**
 * g_io_channel_get_encoding:
 * @channel: a #GIOChannel
 *
 * Get the encoding for the input/output of the channel. The internal
 * encoding is always UTF-8. The encoding %G_IO_CHANNEL_ENCODE_RAW
 * disables encoding and turns off internal buffering. Both
 * %G_IO_CHANNEL_ENCODE_RAW and the default (no encoding, but buffered)
 * are safe to use with binary data.
 *
 * Return value: A string containing the encoding, this string is
 *   owned by GLib and must not be freed.
 **/
G_CONST_RETURN gchar*
g_io_channel_get_encoding (GIOChannel      *channel)
{
  g_return_val_if_fail (channel != NULL, NULL);

  return channel->encoding;
}

static GIOStatus
g_io_channel_fill_buffer (GIOChannel *channel,
                          GError    **err)
{
  gsize read_size, cur_len, oldlen;
  GIOStatus status;

  if (channel->is_seekable && channel->write_buf && channel->write_buf->len > 0)
    {
      status = g_io_channel_flush (channel, err);
      if (status != G_IO_STATUS_NORMAL)
        return status;
    }
  if (channel->is_seekable && channel->partial_write_buf[0] != '\0')
    {
      g_warning ("Partial character at end of write buffer not flushed.\n");
      channel->partial_write_buf[0] = '\0';
    }

  if (!channel->read_buf)
    channel->read_buf = g_string_sized_new (channel->buf_size);

  cur_len = channel->read_buf->len;

  g_string_set_size (channel->read_buf, channel->read_buf->len + channel->buf_size);

  status = channel->funcs->io_read (channel, channel->read_buf->str + cur_len,
                                    channel->buf_size, &read_size, err);

  g_assert ((status == G_IO_STATUS_NORMAL) || (read_size == 0));

  g_string_truncate (channel->read_buf, read_size + cur_len);

  if ((status != G_IO_STATUS_NORMAL)
    && ((status != G_IO_STATUS_EOF) || (channel->read_buf->len == 0)))
    return status;

  g_assert (channel->read_buf->len > 0);

  if (channel->encoded_read_buf)
    oldlen = channel->encoded_read_buf->len;
  else
    {
      oldlen = 0;
      channel->encoded_read_buf = g_string_sized_new (channel->buf_size);
    }

  if (channel->do_encode)
    {
      size_t errnum, inbytes_left, outbytes_left;
      gchar *inbuf, *outbuf;

reencode:

      inbytes_left = channel->read_buf->len;
      outbytes_left = MIN (channel->buf_size / 4,
                           channel->encoded_read_buf->allocated_len
                           - channel->encoded_read_buf->len);

      g_string_set_size (channel->encoded_read_buf,
                         channel->encoded_read_buf->len + outbytes_left);

      inbuf = channel->read_buf->str;
      outbuf = channel->encoded_read_buf->str + channel->encoded_read_buf->len
               - outbytes_left;

      errnum = g_iconv (channel->read_cd, &inbuf, &inbytes_left,
			&outbuf, &outbytes_left);

      g_string_erase (channel->read_buf, 0,
		      channel->read_buf->len - inbytes_left);
      g_string_truncate (channel->encoded_read_buf,
			 channel->encoded_read_buf->len - outbytes_left);

      if (errnum == (size_t) -1)
        {
          switch (errno)
            {
              case EINVAL:
                if ((oldlen == channel->encoded_read_buf->len)
                  && (status == G_IO_STATUS_EOF))
                  status = G_IO_STATUS_EOF;
                else
                  status = G_IO_STATUS_NORMAL;
                break;
              case E2BIG:
                goto reencode;
              case EILSEQ:
                if (oldlen < channel->encoded_read_buf->len)
                  status = G_IO_STATUS_NORMAL;
                else
                  {
                    g_set_error (err, G_CONVERT_ERROR,
                      G_CONVERT_ERROR_ILLEGAL_SEQUENCE,
                      _("Invalid byte sequence in conversion input"));
                    return G_IO_STATUS_ERROR;
                  }
                break;
              default:
                g_set_error (err, G_CONVERT_ERROR, G_CONVERT_ERROR_FAILED,
                  _("Error during conversion: %s"), strerror (errno));
                return G_IO_STATUS_ERROR;
            }
        }
      g_assert ((status != G_IO_STATUS_NORMAL)
               || (channel->encoded_read_buf->len > 0));
    }
  else if (channel->encoding) /* UTF-8 */
    {
      gchar *nextchar, *lastchar;

      nextchar = channel->read_buf->str;
      lastchar = channel->read_buf->str + channel->read_buf->len;

      while (nextchar < lastchar)
        {
          gunichar val_char;

          val_char = g_utf8_get_char_validated (nextchar, lastchar - nextchar);

          switch (val_char)
            {
              case -2:
                /* stop, leave partial character in buffer */
                lastchar = nextchar;
                break;
              case -1:
                if (oldlen > channel->encoded_read_buf->len)
                  status = G_IO_STATUS_NORMAL;
                else
                  {
                    g_set_error (err, G_CONVERT_ERROR,
                      G_CONVERT_ERROR_ILLEGAL_SEQUENCE,
                      _("Invalid byte sequence in conversion input"));
                    status = G_IO_STATUS_ERROR;
                  }
                lastchar = nextchar;
                break;
              default:
                nextchar = g_utf8_next_char (nextchar);
                break;
            }
        }

      if (lastchar > channel->read_buf->str)
        {
          gint copy_len = lastchar - channel->read_buf->str;

          if (!channel->encoded_read_buf)
            channel->encoded_read_buf = g_string_sized_new (channel->buf_size);

          g_string_append_len (channel->encoded_read_buf, channel->read_buf->str,
                               copy_len);
          g_string_erase (channel->read_buf, 0, copy_len);
        }
    }

  return status;
}

/**
 * g_io_channel_read_line:
 * @channel: a #GIOChannel
 * @str_return: The line read from the #GIOChannel, not including the
 *              line terminator. This data should be freed with g_free()
 *              when no longer needed. This
 *              is a null terminated string. If a @length of zero is
 *              returned, this will be %NULL instead.
 * @length: location to store length of the read data, or %NULL
 * @terminator_pos: location to store position of line terminator, or %NULL
 * @error: A location to return an error of type #GConvertError
 *         or #GIOChannelError
 *
 * Read a line, not including the terminating character(s),
 * from a #GIOChannel into a newly allocated string.
 * @length will contain allocated memory if the return
 * is %G_IO_STATUS_NORMAL.
 *
 * Return value: a newly allocated string. Free this string
 *   with g_free() when you are done with it.
 **/
GIOStatus
g_io_channel_read_line (GIOChannel *channel,
                        gchar     **str_return,
                        gsize      *length,
			gsize      *terminator_pos,
		        GError    **error)
{
  GIOStatus status;
  
  g_return_val_if_fail (channel != NULL, G_IO_STATUS_ERROR);
  g_return_val_if_fail (str_return != NULL, G_IO_STATUS_ERROR);
  g_return_val_if_fail ((error == NULL) || (*error == NULL),
			G_IO_STATUS_ERROR);
  g_return_val_if_fail (channel->is_readable, G_IO_STATUS_ERROR);

  status = g_io_channel_read_line_backend (channel, length, terminator_pos, error);

  if (status == G_IO_STATUS_NORMAL)
    {
      GString *use_buf;

      if (channel->encoding)
        use_buf = channel->encoded_read_buf;
      else
        use_buf = channel->read_buf;

      g_assert (use_buf);

      *str_return = g_strndup (use_buf->str, *length);
      g_string_erase (use_buf, 0, *length);
    }
  else
    *str_return = NULL;
  
  return status;
}

/**
 * g_io_channel_read_line_string:
 * @channel: a #GIOChannel
 * @buffer: a #GString into which the line will be written.
 *          If @buffer already contains data, the new data will
 *          be appended to it.
 * @terminator_pos: location to store position of line terminator, or %NULL
 * @error: a location to store an error of type #GConvertError
 *         or #GIOChannelError
 *
 * Read a line from a #GIOChannel, using a #GString as a buffer.
 *
 * Return value:
 **/
GIOStatus
g_io_channel_read_line_string (GIOChannel *channel,
                               GString	  *buffer,
			       gsize      *terminator_pos,
                               GError	 **error)
{
  gsize length;
  GIOStatus status;

  g_return_val_if_fail (channel != NULL, G_IO_STATUS_ERROR);
  g_return_val_if_fail (buffer != NULL, G_IO_STATUS_ERROR);
  g_return_val_if_fail ((error == NULL) || (*error == NULL),
			G_IO_STATUS_ERROR);
  g_return_val_if_fail (channel->is_readable, G_IO_STATUS_ERROR);

  if (buffer->len > 0)
    g_string_truncate (buffer, 0); /* clear out the buffer */

  status = g_io_channel_read_line_backend (channel, &length, terminator_pos, error);

  if (status == G_IO_STATUS_NORMAL)
    {
      GString *use_buf;

      if (channel->encoding)
        use_buf = channel->encoded_read_buf;
      else
        use_buf = channel->read_buf;

      g_assert (use_buf);

      g_string_append_len (buffer, use_buf->str, length);
      g_string_erase (use_buf, 0, length);
    }

  return status;
}


static GIOStatus
g_io_channel_read_line_backend	(GIOChannel *channel,
				 gsize      *length,
				 gsize      *terminator_pos,
				 GError    **error)
{
  GIOStatus status;
  gsize checked_to, line_term_len, line_length, got_term_len;
  GString *use_buf;
  gboolean first_time = TRUE;

  if (!channel->use_buffer)
    {
      /* Can't do a raw read in read_line */
      g_set_error (error, G_CONVERT_ERROR, G_CONVERT_ERROR_FAILED,
                   _("Can't do a raw read in g_io_channel_read_line_string"));
      return G_IO_STATUS_ERROR;
    }

  if (channel->encoding)
    {
      if (!channel->encoded_read_buf)
        channel->encoded_read_buf = g_string_sized_new (channel->buf_size);
      use_buf = channel->encoded_read_buf;
    }
  else
    {
      if (!channel->read_buf)
        channel->read_buf = g_string_sized_new (channel->buf_size);
      use_buf = channel->read_buf;
    }

  status = G_IO_STATUS_NORMAL;

  if (channel->line_term)
    line_term_len = strlen (channel->line_term);
  else
    line_term_len = 3;
    /* This value used for setting checked_to, it's the longest of the four
     * we autodetect for.
     */

  checked_to = 0;

  while (TRUE)
    {
      gchar *nextchar, *lastchar;

      if (!first_time || (use_buf->len == 0))
        {
read_again:
          status = g_io_channel_fill_buffer (channel, error);
          switch (status)
            {
              case G_IO_STATUS_NORMAL:
                if (use_buf->len == 0)
                  /* Can happen when using conversion and only read
                   * part of a character
                   */
                  {
                    first_time = FALSE;
                    continue;
                  }
                break;
              case G_IO_STATUS_EOF:
                if (use_buf->len == 0)
                  {
                    *length = 0;

                    if (channel->encoding && channel->read_buf->len != 0)
                      {
                        g_set_error (error, G_CONVERT_ERROR,
                                     G_CONVERT_ERROR_PARTIAL_INPUT,
                                     "Leftover unconverted data in read buffer");
                        return G_IO_STATUS_ERROR;
                      }
                    else
                      return G_IO_STATUS_EOF;
                  }
                break;
              default:
                *length = 0;
                return status;
            }
        }

      g_assert (use_buf->len > 0);

      first_time = FALSE;

      lastchar = use_buf->str + strlen (use_buf->str);

      for (nextchar = use_buf->str + checked_to; nextchar < lastchar;
           channel->encoding ? nextchar = g_utf8_next_char (nextchar) : nextchar++)
        {
          if (channel->line_term)
            {
              if (strncmp (channel->line_term, nextchar, line_term_len) == 0)
                {
                  line_length = nextchar - use_buf->str;
                  got_term_len = line_term_len;
                  goto done;
                }
            }
          else /* auto detect */
            {
              switch (*nextchar)
                {
                  case '\n': /* unix */
                    line_length = nextchar - use_buf->str;
                    got_term_len = 1;
                    goto done;
                  case '\r': /* Warning: do not use with sockets */
                    line_length = nextchar - use_buf->str;
                    if ((nextchar == lastchar - 1) && (status != G_IO_STATUS_EOF)
                       && (lastchar == use_buf->str + use_buf->len))
                      goto read_again; /* Try to read more data */
                    if ((nextchar < lastchar - 1) && (*(nextchar + 1) == '\n')) /* dos */
                      got_term_len = 2;
                    else /* mac */
                      got_term_len = 1;
                    goto done;
                  case '\xe2': /* Unicode paragraph separator */
                    if (strncmp ("\xe2\x80\xa9", nextchar, 3) == 0)
                      {
                        line_length = nextchar - use_buf->str;
                        got_term_len = 3;
                        goto done;
                      }
                    break;
                  default: /* no match */
                    break;
                }
            }
        }

      g_assert (nextchar == lastchar); /* Valid UTF-8, didn't overshoot */

      /* Also terminate on '\0' */

      line_length = lastchar - use_buf->str;
      if (line_length < use_buf->len)
        {
          got_term_len = 0;
          break;
        }

      /* Check for EOF */

      if (status == G_IO_STATUS_EOF)
        {
          if (channel->encoding && channel->read_buf->len > 0)
            {
              g_set_error (error, G_CONVERT_ERROR, G_CONVERT_ERROR_PARTIAL_INPUT,
                           "Channel terminates in a partial character");
              return G_IO_STATUS_ERROR;
            }
          line_length = use_buf->len;
          got_term_len = 0;
          break;
        }

      if (use_buf->len > line_term_len - 1)
	checked_to = use_buf->len - (line_term_len - 1);
      else
	checked_to = 0;
    }

done:

  if (terminator_pos)
    *terminator_pos = line_length;

  *length = line_length + got_term_len;

  return G_IO_STATUS_NORMAL;
}

/**
 * g_io_channel_read_to_end:
 * @channel: a #GIOChannel
 * @str_return: Location to store a pointer to a string holding
 *              the remaining data in the #GIOChannel. This data should
 *              be freed with g_free() when no longer needed. This
 *              data is terminated by an extra null, but there may be other
 *              nulls in the intervening data.
 * @length: Location to store length of the data
 * @error: A location to return an error of type #GConvertError
 *         or #GIOChannelError
 *
 * Read all the remaining data from the file. Parameters as
 * for g_io_channel_read_line.
 *
 * Return value: One of #G_IO_STATUS_EOF or #G_IO_STATUS_PARTIAL_CHARS
 *               on success
 **/
GIOStatus
g_io_channel_read_to_end (GIOChannel	*channel,
                          gchar        **str_return,
                          gsize		*length,
                          GError       **error)
{
  GIOStatus status;
  GString **use_buf;
    
  g_return_val_if_fail (channel != NULL, G_IO_STATUS_ERROR);
  g_return_val_if_fail (str_return != NULL, G_IO_STATUS_ERROR);
  g_return_val_if_fail (length != NULL, G_IO_STATUS_ERROR);
  g_return_val_if_fail ((error == NULL) || (*error == NULL),
    G_IO_STATUS_ERROR);
  g_return_val_if_fail (channel->is_readable, G_IO_STATUS_ERROR);

  *str_return = NULL;
  *length = 0;

  if (!channel->use_buffer)
    {
      g_set_error (error, G_CONVERT_ERROR, G_CONVERT_ERROR_FAILED,
                   _("Can't do a raw read in g_io_channel_read_to_end"));
      return G_IO_STATUS_ERROR;
    }

  do
    status = g_io_channel_fill_buffer (channel, error);
  while (status == G_IO_STATUS_NORMAL);

  if (status != G_IO_STATUS_EOF)
    return status;

  if (channel->encoding)
    {
      if (channel->read_buf->len > 0)
        {
          g_set_error (error, G_CONVERT_ERROR, G_CONVERT_ERROR_PARTIAL_INPUT,
                       "Channel terminates in a partial character");
          return G_IO_STATUS_ERROR;
        }

      if (!channel->encoded_read_buf)
        {
          if (length)
            *length = 0;
          if (str_return)
            *str_return = g_strdup ("");
        }

      use_buf = &channel->encoded_read_buf;
    }
  else
    use_buf = &channel->read_buf;

  g_assert (*use_buf); /* Created by fill_buffer if it didn't previously exist */

  if (length)
    *length = (*use_buf)->len;

  if (str_return)
    *str_return = g_string_free (*use_buf, FALSE);
  else
    g_string_free (*use_buf, TRUE);
  
  *use_buf = NULL;

  return G_IO_STATUS_NORMAL;
}

/**
 * g_io_channel_read_chars:
 * @channel: a #GIOChannel
 * @buf: a buffer to read data into
 * @count: the size of the buffer. Note that the buffer may
 *         not be complelely filled even if there is data
 *         in the buffer if the remaining data is not a
 *         complete character.
 * @bytes_read: The number of bytes read.
 * @error: A location to return an error of type #GConvertError
 *         or #GIOChannelError.
 *
 * Replacement for g_io_channel_read() with the new API.
 *
 * Return value:
 **/
GIOStatus
g_io_channel_read_chars (GIOChannel	*channel,
                         gchar		*buf,
                         gsize		 count,
			 gsize          *bytes_read,
                         GError        **error)
{
  GIOStatus status;
  GString *use_buf;

  g_return_val_if_fail (channel != NULL, G_IO_STATUS_ERROR);
  g_return_val_if_fail ((error == NULL) || (*error == NULL),
			G_IO_STATUS_ERROR);
  g_return_val_if_fail (bytes_read != NULL, G_IO_STATUS_ERROR);
  g_return_val_if_fail (channel->is_readable, G_IO_STATUS_ERROR);

  if (count == 0)
    {
      *bytes_read = 0;
      return G_IO_STATUS_NORMAL;
    }
  g_return_val_if_fail (buf != NULL, G_IO_STATUS_ERROR);

  if (!channel->use_buffer)
    {
      g_assert (!channel->read_buf || channel->read_buf->len == 0);

      return channel->funcs->io_read (channel, buf, count, bytes_read, error);
    }

  if (channel->encoding)
    {
      if (!channel->encoded_read_buf)
        channel->encoded_read_buf = g_string_sized_new (channel->buf_size);
      use_buf = channel->encoded_read_buf;
    }
  else
    {
      if (!channel->read_buf)
        channel->read_buf = g_string_sized_new (channel->buf_size);
      use_buf = channel->read_buf;
    }

  status = G_IO_STATUS_NORMAL;

  while (use_buf->len < MAX (count, 6) && status == G_IO_STATUS_NORMAL)
    /* Be sure to read in at least one full UTF-8 character
     * (max length 6)
     */
    status = g_io_channel_fill_buffer (channel, error);

  switch (status)
    {
    case G_IO_STATUS_NORMAL:
      g_assert (use_buf->len > 0);
      break;
    case G_IO_STATUS_EOF:
      if (use_buf->len == 0)
	{
	  *bytes_read = 0;

          if (channel->encoding && channel->read_buf->len != 0)
            {
              g_set_error (error, G_CONVERT_ERROR,
                           G_CONVERT_ERROR_PARTIAL_INPUT,
                           "Leftover unconverted data in read buffer");
              return G_IO_STATUS_ERROR;
            }
          else
            return G_IO_STATUS_EOF;
	}
      break;
    case G_IO_STATUS_AGAIN:
      if (use_buf->len > 0)
        break; /* return what we have */
      else
        {
          *bytes_read = 0;
          return G_IO_STATUS_AGAIN;
        }
    default:
      *bytes_read = 0;
      return status;
    }

  *bytes_read = MIN (count, use_buf->len);

  if (channel->encoding && *bytes_read > 0)
    /* Don't validate for NULL encoding, binary safe */
    {
      gchar *nextchar, *prevchar = NULL;

      nextchar = use_buf->str;

      while (nextchar < use_buf->str + *bytes_read)
        {
          prevchar = nextchar;
          nextchar = g_utf8_next_char (nextchar);
        }

      if (nextchar > use_buf->str + *bytes_read)
        *bytes_read = prevchar - use_buf->str;

      g_assert (*bytes_read > 0 || count < 6);
    }

  memcpy (buf, use_buf->str, *bytes_read);
  g_string_erase (use_buf, 0, *bytes_read);

  return G_IO_STATUS_NORMAL;
}

/**
 * g_io_channel_write_chars:
 * @channel: a #GIOChannel
 * @buf: a buffer to write data from
 * @count: the size of the buffer. If -1, the buffer
 *         is taken to be a nul terminated string.
 * @bytes_written: The number of bytes written. This can be nonzero
 *                 even if the return value is not %G_IO_STATUS_NORMAL.
 * @error: A location to return an error of type #GConvertError
 *         or #GIOChannelError
 *
 * Replacement for g_io_channel_write() with the new API.
 *
 * Return value:
 **/
GIOStatus
g_io_channel_write_chars (GIOChannel	*channel,
                          const gchar	*buf,
                          gssize	 count,
			  gsize         *bytes_written,
                          GError       **error)
{
  GIOStatus status;

  g_return_val_if_fail (channel != NULL, G_IO_STATUS_ERROR);
  g_return_val_if_fail (bytes_written != NULL, G_IO_STATUS_ERROR);
  g_return_val_if_fail ((error == NULL) || (*error == NULL),
			G_IO_STATUS_ERROR);
  g_return_val_if_fail (channel->is_writeable, G_IO_STATUS_ERROR);

  if ((count < 0) && buf)
    count = strlen (buf);
  
  if (count == 0)
    return G_IO_STATUS_NORMAL;

  g_return_val_if_fail (buf != NULL, G_IO_STATUS_ERROR);
  g_return_val_if_fail (count > 0, G_IO_STATUS_ERROR);

  /* Raw write case */

  if (!channel->use_buffer)
    {
      g_assert (!channel->write_buf || channel->write_buf->len == 0);
      g_assert (channel->partial_write_buf[0] == '\0');
      return channel->funcs->io_write (channel, buf, count, bytes_written, error);
    }

  /* General case */

  if (channel->is_seekable && ((channel->read_buf && channel->read_buf->len > 0)
    || (channel->encoded_read_buf && channel->encoded_read_buf->len > 0)))
    {
      if (channel->encoded_read_buf && channel->encoded_read_buf->len > 0)
        {
          g_warning("Mixed reading and writing not allowed on encoded files");
          return G_IO_STATUS_ERROR;
        }
      status = g_io_channel_seek_position (channel, 0, G_SEEK_CUR, error);
      if (status != G_IO_STATUS_NORMAL)
        return status;
    }

  if (!channel->write_buf)
    channel->write_buf = g_string_sized_new (channel->buf_size);

  if (!channel->do_encode)
    {
      guint copy_bytes;

      if (channel->encoding)
        { 
          const gchar *badchar;

          if (channel->partial_write_buf[0] != '\0')
            {
              guint partial_chars = strlen (channel->partial_write_buf);
              guint chars_in_buf = MIN (6, partial_chars + count);
              gint skip;

              memcpy (channel->partial_write_buf + partial_chars, buf,
                chars_in_buf - partial_chars);

              g_utf8_validate (channel->partial_write_buf, chars_in_buf, &badchar);

              if (badchar < channel->partial_write_buf + partial_chars)
                {
                  gunichar try_char;
                  gsize left_len = chars_in_buf;

                  g_assert (badchar == channel->partial_write_buf);

                  try_char = g_utf8_get_char_validated (badchar, left_len);

                  switch (try_char)
                    {
                      case -2:
                        g_assert (chars_in_buf < 6);
                        channel->partial_write_buf[chars_in_buf] = '\0';
                        *bytes_written = count;
                        return G_IO_STATUS_NORMAL;
                      case -1:
                        g_set_error (error, G_CONVERT_ERROR,
                                     G_CONVERT_ERROR_ILLEGAL_SEQUENCE,
                                     "Illegal UTF-8 sequence");
                        *bytes_written = 0;
                        return G_IO_STATUS_ERROR;
                      default:
                        g_assert_not_reached ();
                        return G_IO_STATUS_ERROR; /* keep the compiler happy */
                    }
                }

              skip = badchar - (channel->partial_write_buf + partial_chars);

              buf += skip;
              copy_bytes = count - skip;

              g_string_append_len (channel->write_buf, channel->partial_write_buf,
                                  badchar - channel->partial_write_buf);
              channel->partial_write_buf[0] = '\0';
            }
          else
            copy_bytes = count;

          if (!g_utf8_validate (buf, copy_bytes, &badchar))
            {
              gunichar try_char;
              gsize left_len = buf + copy_bytes - badchar;

              try_char = g_utf8_get_char_validated (badchar, left_len);

              switch (try_char)
                {
                  case -2:
                    g_assert (left_len < 6);
                    break;
                  case -1:
                    g_set_error (error, G_CONVERT_ERROR,
                                 G_CONVERT_ERROR_ILLEGAL_SEQUENCE,
                                 "Illegal UTF-8 sequence");
                    *bytes_written = count - copy_bytes;
                    return G_IO_STATUS_ERROR;
                  default:
                    g_assert_not_reached ();
                    return G_IO_STATUS_ERROR; /* Don't confunse the compiler */
                }

              g_string_append (channel->write_buf, channel->partial_write_buf);
              memcpy (channel->partial_write_buf, badchar, left_len);
              channel->partial_write_buf[left_len] = '\0';
              copy_bytes = count - left_len;
            }
          else
            {
              g_string_append (channel->write_buf, channel->partial_write_buf);
              channel->partial_write_buf[0] = '\0';
              copy_bytes = count;
            }
        }
      else
        {
          copy_bytes = count;
          g_assert (channel->partial_write_buf[0] == '\0');
        }

      while (copy_bytes > channel->write_buf->allocated_len - channel->write_buf->len)
        {
          gsize wrote_bytes;

          if (channel->write_buf->len > 0)
            {
              status = channel->funcs->io_write (channel, channel->write_buf->str,
                                                 channel->write_buf->len, &wrote_bytes,
                                                 error);
              g_string_erase (channel->write_buf, 0, wrote_bytes);
            }
          else
            {
              status = channel->funcs->io_write (channel, buf, copy_bytes, &wrote_bytes,
                                                 error);
              copy_bytes -= wrote_bytes;
              buf += wrote_bytes;
            }

          if (status != G_IO_STATUS_NORMAL)
            {
              *bytes_written = count - copy_bytes;
              if (channel->partial_write_buf[0] != '\0')
                {
                  *bytes_written -= strlen (channel->partial_write_buf);
                  channel->partial_write_buf[0] = '\0';
                }
              return status;
            }
        }

      g_string_append_len (channel->write_buf, buf, copy_bytes);
      *bytes_written = count;
    }
  else
    {
      gsize bytes_to_go = count, buf_space, err;
      gchar *outbuf;
      gsize oldlen = channel->write_buf->len;

      if (channel->partial_write_buf[0] != '\0')
        {
          gsize partial_chars = strlen (channel->partial_write_buf);
          gsize partial_buf_size = MIN (6, partial_chars + count);
          guint chars_in_buf = partial_buf_size;
          gchar *inbuf;

          g_assert (partial_chars < 6);

          memcpy (channel->partial_write_buf + partial_chars, buf,
            chars_in_buf - partial_chars);
          inbuf = channel->partial_write_buf;
redo:
          buf_space = MIN (channel->buf_size / 4, channel->write_buf->allocated_len
                           - channel->write_buf->len);
	  g_string_set_size (channel->write_buf, channel->write_buf->len
                             + buf_space);
          outbuf = channel->write_buf->str + channel->write_buf->len - buf_space;

          err = g_iconv (channel->write_cd, &inbuf, &partial_buf_size,
                         &outbuf, &buf_space);

          g_string_truncate (channel->write_buf, channel->write_buf->len - buf_space);

          if (err == (size_t) -1)
            {
              if (errno == E2BIG)
                goto redo;

              if (errno == EINVAL)
                {
                  if (chars_in_buf == partial_buf_size)
                    {
                      /* Didn't convert anything */

                      g_assert (chars_in_buf < 6); /* More of the same character */
                      g_assert (chars_in_buf == count + partial_chars);

                      channel->partial_write_buf[chars_in_buf] = '\0';
                      *bytes_written = count;
                      return G_IO_STATUS_NORMAL;
                    }

                  g_assert (chars_in_buf - partial_buf_size > partial_chars);
                  /* Converted the character of which a part was sitting in
                   * the partial character buffer before the write.
                   */
                }
              else
                {
                  switch (errno)
        	    {
                      case EINVAL:
                      case E2BIG:
                        g_assert_not_reached ();
                      case EILSEQ:
                        g_set_error (error, G_CONVERT_ERROR,
                          G_CONVERT_ERROR_ILLEGAL_SEQUENCE,
                          _("Invalid byte sequence in conversion input"));
                      default:
                        g_set_error (error, G_CONVERT_ERROR, G_CONVERT_ERROR_FAILED,
                          _("Error during conversion: %s"), strerror (errno));
        	    }

                  g_string_truncate (channel->write_buf, oldlen);
                  *bytes_written = 0;
                  channel->partial_write_buf[partial_chars] = '\0';

                  return G_IO_STATUS_ERROR;
                }
            }

          *bytes_written = (chars_in_buf - partial_buf_size) - partial_chars;
          buf += *bytes_written;
          bytes_to_go -= *bytes_written;
          channel->partial_write_buf[0] = '\0';
        }

      do
        {
          buf_space = MIN (channel->write_buf->allocated_len
                           - channel->write_buf->len, channel->buf_size / 4);

	  g_string_set_size (channel->write_buf, channel->write_buf->len + buf_space);
          outbuf = channel->write_buf->str + channel->write_buf->len - buf_space;
    
          err = g_iconv (channel->write_cd, (gchar**) &buf, &bytes_to_go,
			 &outbuf, &buf_space);

          g_string_truncate (channel->write_buf, channel->write_buf->len - buf_space);
          *bytes_written = count - bytes_to_go;

          if (err == (size_t) -1)
            {
              switch (errno)
        	{
                  case EINVAL:
                    {
                      gint bytes_left = count - *bytes_written;

                      g_assert (bytes_left < 6);

                      memcpy (channel->partial_write_buf, buf, bytes_left);
                      channel->partial_write_buf[bytes_left] = '\0';
                    }
                    *bytes_written = count;

                    return G_IO_STATUS_NORMAL;
                  case E2BIG:
                    {
                      gsize wrote_bytes;

                      status = channel->funcs->io_write (channel,
                        channel->write_buf->str, channel->write_buf->len,
                        &wrote_bytes, error);
                      g_string_erase (channel->write_buf, 0, wrote_bytes);

                      if (status != G_IO_STATUS_NORMAL)
                        return status;
                    }
                    continue;
                  case EILSEQ:
                    g_set_error (error, G_CONVERT_ERROR,
                      G_CONVERT_ERROR_ILLEGAL_SEQUENCE,
                      _("Invalid byte sequence in conversion input"));
                    return G_IO_STATUS_ERROR;
                  default:
                    g_set_error (error, G_CONVERT_ERROR, G_CONVERT_ERROR_FAILED,
                      _("Error during conversion: %s"), strerror (errno));
                    return G_IO_STATUS_ERROR;
        	}

            }
        }
      while (bytes_to_go > 0);
    }

  if (channel->write_buf->len > channel->buf_size)
    {
      gsize wrote_bytes;

      status = channel->funcs->io_write (channel, channel->write_buf->str,
                                         channel->write_buf->len, &wrote_bytes, error);

      if (wrote_bytes > 0)
        g_string_erase (channel->write_buf, 0, wrote_bytes);

      return status;
    }
  else
    return G_IO_STATUS_NORMAL;
}

/**
 * g_io_channel_error_quark:
 *
 * Return value: The quark used as %G_IO_CHANNEL_ERROR
 **/
GQuark
g_io_channel_error_quark (void)
{
  static GQuark q = 0;
  if (q == 0)
    q = g_quark_from_static_string ("g-io-channel-error-quark");

  return q;
}
