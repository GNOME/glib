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

#include <config.h>
#include "gmemoryoutputstream.h"
#include "goutputstream.h"
#include "gseekable.h"
#include "gsimpleasyncresult.h"
#include "string.h"
#include "glibintl.h"

/**
 * SECTION:gmemoryoutputstream
 * @short_description: streaming output operations on memory chunks
 * @see_also: #GMemoryInputStream
 *
 * #GMemoryOutputStream is a class for using arbitrary
 * memory chunks as output for GIO streaming output operations.
 *
 */

struct _GMemoryOutputStreamPrivate {
  
  GByteArray *data;
  goffset     pos;

  guint       max_size;
  guint       free_data : 1;
};

enum {
  PROP_0,
  PROP_DATA,
  PROP_FREE_ARRAY,
  PROP_SIZE_LIMIT
};

static void     g_memory_output_stream_finalize     (GObject         *object);

static void     g_memory_output_stream_set_property (GObject      *object,
                                                     guint         prop_id,
                                                     const GValue *value,
                                                     GParamSpec   *pspec);

static void     g_memory_output_stream_get_property (GObject    *object,
                                                     guint       prop_id,
                                                     GValue     *value,
                                                     GParamSpec *pspec);

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
  gobject_class->get_property = g_memory_output_stream_get_property;
  gobject_class->set_property = g_memory_output_stream_set_property;

  ostream_class = G_OUTPUT_STREAM_CLASS (klass);

  ostream_class->write = g_memory_output_stream_write;
  ostream_class->close = g_memory_output_stream_close;
  ostream_class->write_async  = g_memory_output_stream_write_async;
  ostream_class->write_finish = g_memory_output_stream_write_finish;
  ostream_class->close_async  = g_memory_output_stream_close_async;
  ostream_class->close_finish = g_memory_output_stream_close_finish;

  g_object_class_install_property (gobject_class,
                                   PROP_DATA,
                                   g_param_spec_pointer ("data",
                                                         P_("Data byte array"),
                                                         P_("The byte array used as internal storage."),
                                                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT | 
                                                         G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB));

  g_object_class_install_property (gobject_class,
                                   PROP_FREE_ARRAY,
                                   g_param_spec_boolean ("free-array",
                                                         P_("Free array data"),
                                                         P_("Wether or not the interal array should be free on close."),
                                                         FALSE,
                                                         G_PARAM_READWRITE |
                                                         G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB));
  g_object_class_install_property (gobject_class,
                                   PROP_SIZE_LIMIT,
                                   g_param_spec_uint ("size-limit",
                                                      P_("Limit"),
                                                      P_("Maximum amount of bytes that can be written to the stream."),
                                                      0,
                                                      G_MAXUINT,
                                                      0,
                                                      G_PARAM_READWRITE |
                                                      G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB));


}

static void
g_memory_output_stream_finalize (GObject *object)
{
  GMemoryOutputStream        *stream;

  stream = G_MEMORY_OUTPUT_STREAM (object);
  
  if (stream->priv->free_data)
    g_byte_array_free (stream->priv->data, TRUE);
    
  if (G_OBJECT_CLASS (g_memory_output_stream_parent_class)->finalize)
    (*G_OBJECT_CLASS (g_memory_output_stream_parent_class)->finalize) (object);
}

static void
g_memory_output_stream_seekable_iface_init (GSeekableIface *iface)
{
  iface->tell         = g_memory_output_stream_tell;
  iface->can_seek     = g_memory_output_stream_can_seek;
  iface->seek         = g_memory_output_stream_seek;
  iface->can_truncate = g_memory_output_stream_can_truncate;
  iface->truncate     = g_memory_output_stream_truncate;
}


static void
g_memory_output_stream_init (GMemoryOutputStream *stream)
{
  GMemoryOutputStreamPrivate *priv;

  stream->priv = G_TYPE_INSTANCE_GET_PRIVATE (stream,
                                              G_TYPE_MEMORY_OUTPUT_STREAM,
                                              GMemoryOutputStreamPrivate);

  priv = stream->priv;
  priv->data = NULL; 
}

/**
 * g_memory_output_stream_new:
 * @data: a #GByteArray.
 *
 * Creates a new #GMemoryOutputStream. If @data is non-%NULL it will use
 * that for its internal storage otherwise it will create a new #GByteArray.
 * In both cases the internal #GByteArray can later be accessed through the 
 * "data" property.
 *
 * Note: The new stream will not take ownership of the supplied
 * @data so you have to free it yourself after use or explicitly
 * ask for it be freed on close by setting the "free-array" 
 * property to $TRUE.
 *
 * Return value: A newly created #GMemoryOutputStream object.
 **/
GOutputStream *
g_memory_output_stream_new (GByteArray *data)
{
  GOutputStream *stream;

  if (data == NULL) {
    stream = g_object_new (G_TYPE_MEMORY_OUTPUT_STREAM, NULL);
  } else {
    stream = g_object_new (G_TYPE_MEMORY_OUTPUT_STREAM,
                           "data", data,
                           NULL);
  }

  return stream;
}

/**
 * g_memory_output_stream_set_free_data:
 * @ostream:
 * @free_data:
 * 
 **/
void
g_memory_output_stream_set_free_data (GMemoryOutputStream *ostream,
				      gboolean free_data)
{
  GMemoryOutputStreamPrivate *priv;

  g_return_if_fail (G_IS_MEMORY_OUTPUT_STREAM (ostream));

  priv = ostream->priv;

  priv->free_data = free_data;
}

/**
 * g_memory_output_stream_set_max_size:
 * @ostream:
 * @max_size:
 * 
 **/
void
g_memory_output_stream_set_max_size (GMemoryOutputStream *ostream,
                                     guint                max_size)
{
  GMemoryOutputStreamPrivate *priv;
  
  g_return_if_fail (G_IS_MEMORY_OUTPUT_STREAM (ostream));

  priv = ostream->priv;

  priv->max_size = max_size;

  if (priv->max_size > 0 &&
      priv->max_size < priv->data->len) {

    g_byte_array_set_size (priv->data, priv->max_size);

    if (priv->pos > priv->max_size) {
      priv->pos = priv->max_size;
    }
  }

  g_object_notify (G_OBJECT (ostream), "size-limit");
}

static void
g_memory_output_stream_set_property (GObject         *object,
                                     guint            prop_id,
                                     const GValue    *value,
                                     GParamSpec      *pspec)
{
  GMemoryOutputStream *ostream;
  GMemoryOutputStreamPrivate *priv;
  GByteArray *data;
  guint       max_size;

  ostream = G_MEMORY_OUTPUT_STREAM (object);
  priv = ostream->priv;

  switch (prop_id)
    {
    case PROP_DATA:

      if (priv->data && priv->free_data) {
        g_byte_array_free (priv->data, TRUE);
      }

      data = g_value_get_pointer (value);

      if (data == NULL) {
        data = g_byte_array_new (); 
        priv->free_data = TRUE;
      } else {
        priv->free_data = FALSE;
      }
 
      priv->data = data;
      priv->pos  = 0;
      g_object_notify (G_OBJECT (ostream), "data");
      break;

    case PROP_FREE_ARRAY:
      priv->free_data = g_value_get_boolean (value);
      break;

    case PROP_SIZE_LIMIT:
      max_size = g_value_get_uint (value);
      g_memory_output_stream_set_max_size (ostream, max_size);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
g_memory_output_stream_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  GMemoryOutputStream *ostream;
  GMemoryOutputStreamPrivate *priv;

  ostream = G_MEMORY_OUTPUT_STREAM (object);
  priv = ostream->priv;

  switch (prop_id)
    {
    case PROP_DATA:
      g_value_set_pointer (value, priv->data);
      break;

    case PROP_FREE_ARRAY:
      g_value_set_boolean (value, priv->free_data);
      break;

    case PROP_SIZE_LIMIT:
      g_value_set_uint (value, priv->max_size);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/**
 * g_memory_output_stream_get_data:
 * @ostream:
 * 
 * Returns: #GByteArray of the stream's data.
 **/
GByteArray *
g_memory_output_stream_get_data (GMemoryOutputStream *ostream)
{
  g_return_val_if_fail (G_IS_MEMORY_OUTPUT_STREAM (ostream), NULL);

  return ostream->priv->data;
}


static gboolean
array_check_boundary (GMemoryOutputStream  *stream,
                      goffset               size,
                      GError              **error)
{
  GMemoryOutputStreamPrivate *priv;

  priv = stream->priv;

  if (! priv->max_size) {
    return TRUE;
  }

  if (priv->max_size < size || size > G_MAXUINT) {

    g_set_error (error,
                 G_IO_ERROR,
                 G_IO_ERROR_FAILED,
                 "Reached maximum data array limit");

    return FALSE;
  }

  return TRUE; 
}

static gssize
array_resize (GMemoryOutputStream  *stream,
              goffset               size,
              GError              **error)
{
  GMemoryOutputStreamPrivate *priv;
  guint old_len;

  priv = stream->priv;

  if (! array_check_boundary (stream, size, error)) 
    return -1;
  

  if (priv->data->len == size) 
    return priv->data->len - priv->pos;
  

  old_len = priv->data->len;
  g_byte_array_set_size (priv->data, size);

  if (size > old_len && priv->pos > old_len) 
    memset (priv->data->data + priv->pos, 0, size - old_len);
  

  return priv->data->len - priv->pos;
}

static gssize
g_memory_output_stream_write (GOutputStream *stream,
                              const void    *buffer,
                              gsize          count,
                              GCancellable  *cancellable,
                              GError       **error)
{
  GMemoryOutputStream        *ostream;
  GMemoryOutputStreamPrivate *priv;
  gsize     new_size;
  gssize    n;
  guint8   *dest;

  ostream = G_MEMORY_OUTPUT_STREAM (stream);
  priv = ostream->priv;

  /* count < 0 is ensured by GOutputStream */

  n = MIN (count, priv->data->len - priv->pos);

  if (n < 1)
    {
      new_size = priv->pos + count;

      if (priv->max_size > 0)
        {
          new_size = MIN (new_size, priv->max_size);
        }

      n = array_resize (ostream, new_size, error);

      if (n == 0) 
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_FAILED,
                       "Reached maximum data array limit");
          return -1;
        }
      else if (n < 0)
        {
          return -1;
        }
    }

  dest = priv->data->data + priv->pos;
  memcpy (dest, buffer, n); 
  priv->pos += n;

  return n;
}

static gboolean
g_memory_output_stream_close (GOutputStream  *stream,
                              GCancellable   *cancellable,
                              GError        **error)
{
  GMemoryOutputStream        *ostream;
  GMemoryOutputStreamPrivate *priv;

  ostream = G_MEMORY_OUTPUT_STREAM (stream);
  priv = ostream->priv;

  return TRUE;
}

static void
g_memory_output_stream_write_async  (GOutputStream        *stream,
                                     const void           *buffer,
                                     gsize                 count,
                                     int                   io_priority,
                                     GCancellable         *cancellable,
                                     GAsyncReadyCallback   callback,
                                     gpointer              data)
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
g_memory_output_stream_write_finish (GOutputStream        *stream,
                                     GAsyncResult         *result,
                                     GError              **error)
{
  GSimpleAsyncResult *simple;
  gssize nwritten;

  simple = G_SIMPLE_ASYNC_RESULT (result);
  
  g_assert (g_simple_async_result_get_source_tag (simple) == 
            g_memory_output_stream_write_async);

  nwritten = g_simple_async_result_get_op_res_gssize (simple);
  return nwritten;
}

static void
g_memory_output_stream_close_async  (GOutputStream        *stream,
                                     int                   io_priority,
                                     GCancellable         *cancellable,
                                     GAsyncReadyCallback   callback,
                                     gpointer              data)
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
g_memory_output_stream_close_finish (GOutputStream        *stream,
                                     GAsyncResult         *result,
                                     GError              **error)
{
  GSimpleAsyncResult *simple;

  simple = G_SIMPLE_ASYNC_RESULT (result);

  g_assert (g_simple_async_result_get_source_tag (simple) == 
            g_memory_output_stream_close_async);


  return TRUE;
}

static goffset
g_memory_output_stream_tell (GSeekable  *seekable)
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
g_memory_output_stream_seek (GSeekable      *seekable,
                            goffset          offset,
                            GSeekType        type,
                            GCancellable    *cancellable,
                            GError         **error)
{
  GMemoryOutputStream        *stream;
  GMemoryOutputStreamPrivate *priv;
  goffset absolute;

  stream = G_MEMORY_OUTPUT_STREAM (seekable);
  priv = stream->priv;

  switch (type) {

    case G_SEEK_CUR:
      absolute = priv->pos + offset;
      break;

    case G_SEEK_SET:
      absolute = offset;
      break;

    case G_SEEK_END:
      absolute = priv->data->len + offset;
      break;
  
    default:
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_ARGUMENT,
                   "Invalid GSeekType supplied");

      return FALSE;
  }

  if (absolute < 0) {
    g_set_error (error,
                 G_IO_ERROR,
                 G_IO_ERROR_INVALID_ARGUMENT,
                 "Invalid seek request");
    return FALSE;
  }

  if (! array_check_boundary (stream, absolute, error)) 
    return FALSE;  

  priv->pos = absolute;

  return TRUE;
}

static gboolean
g_memory_output_stream_can_truncate (GSeekable *seekable)
{
  return TRUE;
}

static gboolean
g_memory_output_stream_truncate (GSeekable      *seekable,
                                 goffset         offset,
                                 GCancellable   *cancellable,
                                 GError         **error)
{
  GMemoryOutputStream *ostream;
  GMemoryOutputStreamPrivate *priv;

  ostream = G_MEMORY_OUTPUT_STREAM (seekable);
  priv = ostream->priv;
 
  if (array_resize (ostream, offset, error) < 0)
      return FALSE;

  return TRUE;
}

/* vim: ts=2 sw=2 et */
