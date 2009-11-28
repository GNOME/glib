/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright (C) 2009 Paolo Borelli
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
 * Author: Paolo Borelli <pborelli@gnome.org>
 */

#include "config.h"
#include "gutf8inputstream.h"
#include "ginputstream.h"
#include "gcancellable.h"
#include "gioerror.h"
#include "glibintl.h"

#include "gioalias.h"

/**
 * SECTION:gutf8inputstream
 * @short_description: Input Stream performing UTF8 validation
 * @include: gio/gio.h
 * @see_also: #GFilterInputStream, #GInputStream
 *
 * utf8 input stream implements #GFilterInputStream and provides
 * UTF8 validation of the data read from a the stream.
 * If the supplied buffer is long enough (see below), the returned
 * data is guaranteed to end at utf8 character boundaries.
 * <note>
 *   <para>
 *     Extra care must be taken when performing "small" reads:
 *     unless you have control of the data being read, you need
 *     to always supply a buffer long at least 6 bytes, otherwise
 *     the returned content may be an incomplete utf8 byte sequence.
 *   </para>
 * </note>
 *
 * To create an utf8 input stream, use g_utf8_input_stream_new().
 *
 **/


#define MAX_UNICHAR_LEN 6

struct _GUtf8InputStreamPrivate {
  /* buffer containing trailing partial character not yet returned */
  char buffer[MAX_UNICHAR_LEN];
  gsize len;

  /* buffer containing partial character returned in a "small read"
   * but not yet validated */
  char small_read_buffer[MAX_UNICHAR_LEN];
  gsize small_read_len;
};

static gssize g_utf8_input_stream_read        (GInputStream          *stream,
                                               void                  *buffer,
                                               gsize                  count,
                                               GCancellable          *cancellable,
                                               GError               **error);

G_DEFINE_TYPE (GUtf8InputStream,
               g_utf8_input_stream,
               G_TYPE_FILTER_INPUT_STREAM)


static void
g_utf8_input_stream_class_init (GUtf8InputStreamClass *klass)
{
  GInputStreamClass *istream_class;

  g_type_class_add_private (klass, sizeof (GUtf8InputStreamPrivate));

  istream_class = G_INPUT_STREAM_CLASS (klass);
  istream_class->read_fn = g_utf8_input_stream_read;
}

static void
g_utf8_input_stream_init (GUtf8InputStream *stream)
{
  stream->priv = G_TYPE_INSTANCE_GET_PRIVATE (stream,
                                              G_TYPE_UTF8_INPUT_STREAM,
                                              GUtf8InputStreamPrivate);
}

/**
 * g_utf8_input_stream_new:
 * @base_stream: a #GInputStream.
 *
 * Creates a new #GUtf8InputStream from the given @base_stream.
 *
 * Returns: a #GInputStream for the given @base_stream.
 *
 * Since: 2.24
 **/
GInputStream *
g_utf8_input_stream_new (GInputStream *base_stream)
{
  GInputStream *stream;

  g_return_val_if_fail (G_IS_INPUT_STREAM (base_stream), NULL);

  stream = g_object_new (G_TYPE_UTF8_INPUT_STREAM,
                         "base-stream", base_stream,
                         NULL);

  return stream;
}

static void
store_remainder (GUtf8InputStream *stream,
                 const char       *remainder,
                 gsize             len)
{
  GUtf8InputStreamPrivate *priv;
  gsize i;

  priv = stream->priv;

  /* we store a remanainder only after having
   * consumed the previous */
  g_assert (priv->len == 0);

  for (i = 0; i < len; ++i)
    priv->buffer[i] = remainder[i];
  priv->len = i;
}

static gssize
get_remainder (GUtf8InputStream *stream,
               char             *buffer,
               gsize             count)
{
  GUtf8InputStreamPrivate *priv;
  gsize i, len;
  gssize res;

  priv = stream->priv;

  g_assert (priv->len < MAX_UNICHAR_LEN);

  len = MIN (count, priv->len);
  for (i = 0; i < len; ++i)
    buffer[i] = priv->buffer[i];
  res = i;

  /* if there is more remainder, move it at the start */
  for (i = 0; i < (priv->len - res); ++i)
    priv->buffer[i] = priv->buffer[res + i];
  priv->len = i;

  return res;
}

static void
store_small_read (GUtf8InputStream *stream,
                  const char       *buffer,
                  gsize             len)
{
  GUtf8InputStreamPrivate *priv;
  gsize i;

  priv = stream->priv;

  /* if we reach MAX_UNICHAR_LEN it is either valid
   * or invalid, so we should already have removed it
   * from the buffer */
  g_assert (priv->small_read_len + len < MAX_UNICHAR_LEN);

  for (i = 0; i < len; ++i)
    priv->small_read_buffer[priv->small_read_len + i] = buffer[i];
  priv->small_read_len += i;
}

/* Combines the current "small read" buffer with the new
 * bytes given, validates the buffer and if needed
 * flushes it.
 *
 * returns:
 * the number of bytes of buffer that are needed to
 * make the current small read buffer valid.
 *
 * -1 if the small read buffer is invalid
 *
 * 0 if it is an incomplete character or if the
 * small read buffer is empty.
 */
static gssize
validate_small_read (GUtf8InputStream *stream,
                     const char       *buffer,
                     gsize             len)
{
  GUtf8InputStreamPrivate *priv;
  gsize i;
  gunichar c;
  char *p;
  gssize res;

  priv = stream->priv;

  if (priv->small_read_len == 0)
    return 0;

  for (i = 0; i < MIN (len, MAX_UNICHAR_LEN - priv->small_read_len); ++i)
    priv->small_read_buffer[priv->small_read_len + i] = buffer[i];

  c = g_utf8_get_char_validated (priv->small_read_buffer, priv->small_read_len + i);
  if (c == (gunichar)-1)
    {
      priv->small_read_len = 0;
      return -1;
    }
  if (c == (gunichar)-2)
    {
      return 0;
    }

  p = g_utf8_next_char (priv->small_read_buffer);
  res = p - (priv->small_read_buffer + priv->small_read_len);

  g_assert (res > 0);

  /* reset the buffer */
  priv->small_read_len = 0;

  return res;
}

static gssize
g_utf8_input_stream_read (GInputStream *stream,
                          void         *buffer,
                          gsize         count,
                          GCancellable *cancellable,
                          GError      **error)
{
  GUtf8InputStream *ustream;
  GUtf8InputStreamPrivate *priv;
  GInputStream *base_stream;
  gsize nvalid, remainder;
  gssize oldread, nread, offset;
  gboolean valid, eof;
  const gchar *end;

  ustream = G_UTF8_INPUT_STREAM (stream);
  priv = ustream->priv;

  /* if we had previous incomplete data put it at the start of the buffer */
  oldread = get_remainder (ustream, buffer, count);

  /* if we have already reached count, it is "small read":
   * store it to validate later */
  if (oldread == count)
    {
      store_small_read (ustream, buffer, oldread);
      return oldread;
    }

  base_stream = g_filter_input_stream_get_base_stream (G_FILTER_INPUT_STREAM (stream));

  nread = g_input_stream_read (base_stream,
                               buffer + oldread,
                               count - oldread,
                               cancellable,
                               error);

  if (nread < 0)
    return -1;

  /* take into account bytes we put in the buffer */
  eof = (nread == 0);
  nread += oldread;

  /* validate previous small reads */
  offset = validate_small_read (ustream, buffer, nread);
  if (offset < 0)
    goto error;

  /* validate */
  valid = g_utf8_validate (buffer + offset, nread - offset, &end);
  nvalid = end - (char *)buffer;

  if (valid)
      return nread;

  remainder = nread - nvalid;

  /* if validation failed in the last bytes and the byte 
   * sequence is an incomplete character and EOF is not reached,
   * try to read further to see if we stopped in the middle
   * of a character */
  if ((remainder < MAX_UNICHAR_LEN) &&
      (!eof) &&
      (g_utf8_get_char_validated ((char *)buffer + nvalid, remainder) == (gunichar)-2))
    {
      if (nvalid == 0)
        {
          /* A "small" read: store it to validate later */
          store_small_read (ustream, buffer, nread);
          return nread;
        }

      store_remainder (ustream, (char *)buffer + nvalid, remainder);

      return nvalid;
    }

error:
  g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
               _("Invalid UTF-8 sequence in input"));
  return -1;
}

#define __G_UTF8_INPUT_STREAM_C__
#include "gioaliasdef.c"
