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
 * Author: Christian Kellner <gicmo@gnome.org>
 */

#include "config.h"
#include "gmemoryoutputstream.h"
#include "goutputstream.h"
#include "gseekable.h"
#include "gsimpleasyncresult.h"
#include "gioerror.h"
#include "string.h"
#include "glibintl.h"

#include "gioalias.h"

/**
 * SECTION:gmemoryoutputstream
 * @short_description: Streaming output operations on memory chunks
 * @include: gio/gio.h
 * @see_also: #GMemoryInputStream
 *
 * #GMemoryOutputStream is a class for using arbitrary
 * memory chunks as output for GIO streaming output operations.
 *
 */

#define MIN_ARRAY_SIZE  16

struct _GMemoryOutputStreamPrivate {
  
  gpointer       data;
  gsize          len;
  gsize          valid_len; /* The part of data that has been written to */

  goffset        pos;

  GReallocFunc   realloc_fn;
  GDestroyNotify destroy;
};

static void     g_memory_output_stream_finalize     (GObject      *object);

static gssize   g_memory_output_stream_write       (GOutputStream *stream,
                                                    const void    *buffer,
                                                    gsize          count,
                                                    GCancellable  *cancellable,
                                                    GError       **error);

static gboolean g_memory_output_stream_close       (GOutputStream  *stream,
                                                    GCancellable   *cancellable,
                                                    GError        **error);

static void     g_memory_output_stream_write_async  (GOutputStream        *stream,
                                                     const void           *buffer,
                                                     gsize                 count,
                                                     int                   io_priority,
                                                     GCancellable         *cancellable,
                                                     GAsyncReadyCallback   callback,
                                                     gpointer              data);
static gssize   g_memory_output_stream_write_finish (GOutputStream        *stream,
                                                     GAsyncResult         *result,
                                                     GError              **error);
static void     g_memory_output_stream_close_async  (GOutputStream        *stream,
                                                     int                   io_priority,
                                                     GCancellable         *cancellable,
                                                     GAsyncReadyCallback   callback,
                                                     gpointer              data);
static gboolean g_memory_output_stream_close_finish (GOutputStream        *stream,
                                                     GAsyncResult         *result,
                                                     GError              **error);

static void     g_memory_output_stream_seekable_iface_init (GSeekableIface  *iface);
static goffset  g_memory_output_stream_tell                (GSeekable       *seekable);
static gboolean g_memory_output_stream_can_seek            (GSeekable       *seekable);
static gboolean g_memory_output_stream_seek                (GSeekable       *seekable,
                                                           goffset          offset,
                                                           GSeekType        type,
                                                           GCancellable    *cancellable,
                                                           GError         **error);
static gboolean g_memory_output_stream_can_truncate        (GSeekable       *seekable);
static gboolean g_memory_output_stream_truncate            (GSeekable       *seekable,
                                                           goffset          offset,
                                                           GCancellable    *cancellable,
                                                           GError         **error);

G_DEFINE_TYPE_WITH_CODE (GMemoryOutputStream, g_memory_output_stream, G_TYPE_OUTPUT_STREAM,
                         G_IMPLEMENT_INTERFACE (G_TYPE_SEEKABLE,
                                                g_memory_output_stream_seekable_iface_init))


static void
g_memory_output_stream_class_init (GMemoryOutputStreamClass *klass)
{
  GOutputStreamClass *ostream_class;
  GObjectClass *gobject_class;

  g_type_class_add_private (klass, sizeof (GMemoryOutputStreamPrivate));

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = g_memory_output_stream_finalize;

  ostream_class = G_OUTPUT_STREAM_CLASS (klass);

  ostream_class->write_fn = g_memory_output_stream_write;
  ostream_class->close_fn = g_memory_output_stream_close;
  ostream_class->write_async  = g_memory_output_stream_write_async;
  ostream_class->write_finish = g_memory_output_stream_write_finish;
  ostream_class->close_async  = g_memory_output_stream_close_async;
  ostream_class->close_finish = g_memory_output_stream_close_finish;
}

static void
g_memory_output_stream_finalize (GObject *object)
{
  GMemoryOutputStream        *stream;
  GMemoryOutputStreamPrivate *priv;

  stream = G_MEMORY_OUTPUT_STREAM (object);
  priv = stream->priv;
  
  if (priv->destroy)
    priv->destroy (priv->data);

  G_OBJECT_CLASS (g_memory_output_stream_parent_class)->finalize (object);
}

static void
g_memory_output_stream_seekable_iface_init (GSeekableIface *iface)
{
  iface->tell         = g_memory_output_stream_tell;
  iface->can_seek     = g_memory_output_stream_can_seek;
  iface->seek         = g_memory_output_stream_seek;
  iface->can_truncate = g_memory_output_stream_can_truncate;
  iface->truncate_fn  = g_memory_output_stream_truncate;
}


static void
g_memory_output_stream_init (GMemoryOutputStream *stream)
{
  stream->priv = G_TYPE_INSTANCE_GET_PRIVATE (stream,
                                              G_TYPE_MEMORY_OUTPUT_STREAM,
                                              GMemoryOutputStreamPrivate);
}

/**
 * g_memory_output_stream_new:
 * @data: pointer to a chunk of memory to use, or %NULL
 * @len: the size of @data
 * @realloc_fn: a function with realloc() semantics to be called when 
 *     @data needs to be grown, or %NULL
 * @destroy: a function to be called on @data when the stream is finalized,
 *     or %NULL
 *
 * Creates a new #GMemoryOutputStream. 
 *
 * If @data is non-%NULL, the stream  will use that for its internal storage.
 * If @realloc_fn is non-%NULL, it will be used for resizing the internal
 * storage when necessary. To construct a fixed-size output stream, 
 * pass %NULL as @realloc_fn.
 * |[
 * /&ast; a stream that can grow &ast;/
 * stream = g_memory_output_stream_new (NULL, 0, realloc, free);
 *
 * /&ast; a fixed-size stream &ast;/
 * data = malloc (200);
 * stream2 = g_memory_output_stream_new (data, 200, NULL, free);
 * ]|
 *
 * Return value: A newly created #GMemoryOutputStream object.
 **/
GOutputStream *
g_memory_output_stream_new (gpointer       data,
                            gsize          len,
                            GReallocFunc   realloc_fn,
                            GDestroyNotify destroy)
{
  GOutputStream *stream;
  GMemoryOutputStreamPrivate *priv;

  stream = g_object_new (G_TYPE_MEMORY_OUTPUT_STREAM, NULL);

  priv = G_MEMORY_OUTPUT_STREAM (stream)->priv;

  priv->data = data;
  priv->len = len;
  priv->realloc_fn = realloc_fn;
  priv->destroy = destroy;

  priv->pos = 0;
  priv->valid_len = 0;

  return stream;
}

/**
 * g_memory_output_stream_get_data:
 * @ostream: a #GMemoryOutputStream
 *
 * Gets any loaded data from the @ostream. 
 *
 * Note that the returned pointer may become invalid on the next 
 * write or truncate operation on the stream. 
 * 
 * Returns: pointer to the stream's data
 **/
gpointer
g_memory_output_stream_get_data (GMemoryOutputStream *ostream)
{
  g_return_val_if_fail (G_IS_MEMORY_OUTPUT_STREAM (ostream), NULL);

  return ostream->priv->data;
}

/**
 * g_memory_output_stream_get_size:
 * @ostream: a #GMemoryOutputStream
 *
 * Gets the size of the currently allocated data area (availible from
 * g_memory_output_stream_get_data()). If the stream isn't
 * growable (no realloc was passed to g_memory_output_stream_new()) then
 * this is the maximum size of the stream and further writes
 * will return %G_IO_ERROR_NO_SPACE.
 *
 * Note that for growable streams the returned size may become invalid on
 * the next write or truncate operation on the stream.
 *
 * If you want the number of bytes currently written to the stream, use
 * g_memory_output_stream_get_data_size().
 * 
 * Returns: the number of bytes allocated for the data buffer
 */
gsize
g_memory_output_stream_get_size (GMemoryOutputStream *ostream)
{
  g_return_val_if_fail (G_IS_MEMORY_OUTPUT_STREAM (ostream), 0);
  
  return ostream->priv->len;
}

/**
 * g_memory_output_stream_get_data_size:
 * @ostream: a #GMemoryOutputStream
 *
 * Returns the number of bytes from the start up
 * to including the last byte written in the stream
 * that has not been truncated away.
 * 
 * Returns: the number of bytes written to the stream
 *
 * Since: 2.18
 */
gsize
g_memory_output_stream_get_data_size (GMemoryOutputStream *ostream)
{
  g_return_val_if_fail (G_IS_MEMORY_OUTPUT_STREAM (ostream), 0);
  
  return ostream->priv->valid_len;
}


static gboolean
array_check_boundary (GMemoryOutputStream  *stream,
                      goffset               size,
                      GError              **error)
{
  if (size > G_MAXUINT)
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_FAILED,
                           _("Reached maximum data array limit"));

      return FALSE;
    }

  return TRUE;
}

static gboolean
array_resize (GMemoryOutputStream  *ostream,
              gsize                 size,
	      gboolean              allow_partial,
              GError              **error)
{
  GMemoryOutputStreamPrivate *priv;
  gpointer data;
  gsize len;

  priv = ostream->priv;

  if (!array_check_boundary (ostream, size, error))
    return FALSE;

  if (priv->len == size)
    return TRUE;

  if (!priv->realloc_fn)
    {
      if (allow_partial &&
	  priv->pos < priv->len)
	return TRUE; /* Short write */
      
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_NO_SPACE,
                           _("Memory output stream not resizable"));
      return FALSE;
    }

  len = priv->len;
  data = priv->realloc_fn (priv->data, size);

  if (size > 0 && !data) 
    {
      if (allow_partial &&
	  priv->pos < priv->len)
	return TRUE; /* Short write */
      
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_NO_SPACE,
                           _("Failed to resize memory output stream"));
      return FALSE;
    }

  if (size > len)
    memset ((guint8 *)data + len, 0, size - len);

  priv->data = data;
  priv->len = size;
  
  if (priv->len < priv->valid_len)
    priv->valid_len = priv->len;
  
  return TRUE;
}

static gint
g_nearest_pow (gint num)
{
  gint n = 1;

  while (n < num)
    n <<= 1;

  return n;
}

static gssize
g_memory_output_stream_write (GOutputStream  *stream,
                              const void     *buffer,
                              gsize           count,
                              GCancellable   *cancellable,
                              GError        **error)
{
  GMemoryOutputStream        *ostream;
  GMemoryOutputStreamPrivate *priv;
  guint8   *dest;
  gsize new_size;

  ostream = G_MEMORY_OUTPUT_STREAM (stream);
  priv = ostream->priv;

  if (count == 0)
    return 0;

  if (priv->pos + count > priv->len) 
    {
      /* At least enought to fit the write, rounded up
	 for greater than linear growth */
      new_size = g_nearest_pow (priv->pos + count);
      new_size = MAX (new_size, MIN_ARRAY_SIZE);
      
      if (!array_resize (ostream, new_size, TRUE, error))
	return -1;
    }

  /* Make sure we handle short writes if the array_resize
     only added part of the required memory */
  count = MIN (count, priv->len - priv->pos);
  
  dest = (guint8 *)priv->data + priv->pos;
  memcpy (dest, buffer, count); 
  priv->pos += count;
  
  if (priv->pos > priv->valid_len)
    priv->valid_len = priv->pos;

  return count;
}

static gboolean
g_memory_output_stream_close (GOutputStream  *stream,
                              GCancellable   *cancellable,
                              GError        **error)
{
  return TRUE;
}

static void
g_memory_output_stream_write_async (GOutputStream       *stream,
                                    const void          *buffer,
                                    gsize                count,
                                    int                  io_priority,
                                    GCancellable        *cancellable,
                                    GAsyncReadyCallback  callback,
                                    gpointer             data)
{
  GSimpleAsyncResult *simple;
  gssize nwritten;

  nwritten = g_memory_output_stream_write (stream,
                                           buffer,
                                           count,
                                           cancellable,
                                           NULL);
 

  simple = g_simple_async_result_new (G_OBJECT (stream),
                                      callback,
                                      data,
                                      g_memory_output_stream_write_async);
  
  g_simple_async_result_set_op_res_gssize (simple, nwritten); 
  g_simple_async_result_complete_in_idle (simple);
  g_object_unref (simple);
}

static gssize
g_memory_output_stream_write_finish (GOutputStream  *stream,
                                     GAsyncResult   *result,
                                     GError        **error)
{
  GSimpleAsyncResult *simple;
  gssize nwritten;

  simple = G_SIMPLE_ASYNC_RESULT (result);
  
  g_warn_if_fail (g_simple_async_result_get_source_tag (simple) == 
                  g_memory_output_stream_write_async);

  nwritten = g_simple_async_result_get_op_res_gssize (simple);

  return nwritten;
}

static void
g_memory_output_stream_close_async (GOutputStream       *stream,
                                    int                  io_priority,
                                    GCancellable        *cancellable,
                                    GAsyncReadyCallback  callback,
                                    gpointer             data)
{
  GSimpleAsyncResult *simple;

  simple = g_simple_async_result_new (G_OBJECT (stream),
                                      callback,
                                      data,
                                      g_memory_output_stream_close_async);


  /* will always return TRUE */
  g_memory_output_stream_close (stream, cancellable, NULL);
  
  g_simple_async_result_complete_in_idle (simple);
  g_object_unref (simple);
}

static gboolean
g_memory_output_stream_close_finish (GOutputStream  *stream,
                                     GAsyncResult   *result,
                                     GError        **error)
{
  GSimpleAsyncResult *simple;

  simple = G_SIMPLE_ASYNC_RESULT (result);

  g_warn_if_fail (g_simple_async_result_get_source_tag (simple) == 
                  g_memory_output_stream_close_async);

  return TRUE;
}

static goffset
g_memory_output_stream_tell (GSeekable *seekable)
{
  GMemoryOutputStream *stream;
  GMemoryOutputStreamPrivate *priv;

  stream = G_MEMORY_OUTPUT_STREAM (seekable);
  priv = stream->priv;

  return priv->pos;
}

static gboolean
g_memory_output_stream_can_seek (GSeekable *seekable)
{
  return TRUE;
}

static gboolean
g_memory_output_stream_seek (GSeekable    *seekable,
                            goffset        offset,
                            GSeekType      type,
                            GCancellable  *cancellable,
                            GError       **error)
{
  GMemoryOutputStream        *stream;
  GMemoryOutputStreamPrivate *priv;
  goffset absolute;

  stream = G_MEMORY_OUTPUT_STREAM (seekable);
  priv = stream->priv;

  switch (type) 
    {
    case G_SEEK_CUR:
      absolute = priv->pos + offset;
      break;

    case G_SEEK_SET:
      absolute = offset;
      break;

    case G_SEEK_END:
      absolute = priv->len + offset;
      break;
  
    default:
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_INVALID_ARGUMENT,
                           _("Invalid GSeekType supplied"));

      return FALSE;
    }

  if (absolute < 0) 
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_INVALID_ARGUMENT,
                           _("Invalid seek request"));
      return FALSE;
    }

  if (!array_check_boundary (stream, absolute, error)) 
    return FALSE;  

  priv->pos = absolute;

  return TRUE;
}

static gboolean
g_memory_output_stream_can_truncate (GSeekable *seekable)
{
  GMemoryOutputStream *ostream;
  GMemoryOutputStreamPrivate *priv;

  ostream = G_MEMORY_OUTPUT_STREAM (seekable);
  priv = ostream->priv;

  return priv->realloc_fn != NULL;
}

static gboolean
g_memory_output_stream_truncate (GSeekable     *seekable,
                                 goffset        offset,
                                 GCancellable  *cancellable,
                                 GError       **error)
{
  GMemoryOutputStream *ostream;
  GMemoryOutputStreamPrivate *priv;

  ostream = G_MEMORY_OUTPUT_STREAM (seekable);
  priv = ostream->priv;
 
  if (!array_resize (ostream, offset, FALSE, error))
    return FALSE;

  return TRUE;
}

#define __G_MEMORY_OUTPUT_STREAM_C__
#include "gioaliasdef.c"
