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

#include <glib.h>
#include <gfileinputstream.h>
#include <gseekable.h>
#include "gsimpleasyncresult.h"
#include "glibintl.h"

#include "gioalias.h"

/**
 * SECTION:gfileinputstream
 * @short_description: file input streaming operations
 * @see_also: #GInputStream, #GDataInputStream, #GSeekable
 * 
 * 
 *
 **/

static void       g_file_input_stream_seekable_iface_init    (GSeekableIface       *iface);
static goffset    g_file_input_stream_seekable_tell          (GSeekable            *seekable);
static gboolean   g_file_input_stream_seekable_can_seek      (GSeekable            *seekable);
static gboolean   g_file_input_stream_seekable_seek          (GSeekable            *seekable,
							      goffset               offset,
							      GSeekType             type,
							      GCancellable         *cancellable,
							      GError              **error);
static gboolean   g_file_input_stream_seekable_can_truncate  (GSeekable            *seekable);
static gboolean   g_file_input_stream_seekable_truncate      (GSeekable            *seekable,
							      goffset               offset,
							      GCancellable         *cancellable,
							      GError              **error);
static void       g_file_input_stream_real_query_info_async  (GFileInputStream     *stream,
							      char                 *attributes,
							      int                   io_priority,
							      GCancellable         *cancellable,
							      GAsyncReadyCallback   callback,
							      gpointer              user_data);
static GFileInfo *g_file_input_stream_real_query_info_finish (GFileInputStream     *stream,
							      GAsyncResult         *result,
							      GError              **error);


G_DEFINE_TYPE_WITH_CODE (GFileInputStream, g_file_input_stream, G_TYPE_INPUT_STREAM,
			 G_IMPLEMENT_INTERFACE (G_TYPE_SEEKABLE,
						g_file_input_stream_seekable_iface_init))

struct _GFileInputStreamPrivate {
  GAsyncReadyCallback outstanding_callback;
};

static void
g_file_input_stream_class_init (GFileInputStreamClass *klass)
{
  g_type_class_add_private (klass, sizeof (GFileInputStreamPrivate));

  klass->query_info_async = g_file_input_stream_real_query_info_async;
  klass->query_info_finish = g_file_input_stream_real_query_info_finish;
}

static void
g_file_input_stream_seekable_iface_init (GSeekableIface *iface)
{
  iface->tell = g_file_input_stream_seekable_tell;
  iface->can_seek = g_file_input_stream_seekable_can_seek;
  iface->seek = g_file_input_stream_seekable_seek;
  iface->can_truncate = g_file_input_stream_seekable_can_truncate;
  iface->truncate = g_file_input_stream_seekable_truncate;
}

static void
g_file_input_stream_init (GFileInputStream *stream)
{
  stream->priv = G_TYPE_INSTANCE_GET_PRIVATE (stream,
					      G_TYPE_FILE_INPUT_STREAM,
					      GFileInputStreamPrivate);
}

/**
 * g_file_input_stream_query_info:
 * @stream: a #GFileInputStream.
 * @attributes: a file attribute query string.
 * @cancellable: optional #GCancellable object, %NULL to ignore. 
 * @error: a #GError location to store the error occuring, or %NULL to 
 * ignore.
 *
 * Queries a file input stream the given @attributes.his function blocks while querying
 * the stream. For the asynchronous (non-blocking) version of this function, see 
 * g_file_input_stream_query_info_async(). While the stream is blocked, 
 * the stream will set the pending flag internally, and any other operations on the 
 * stream will fail with %G_IO_ERROR_PENDING.
 *
 * Returns: a #GFileInfo, or %NULL on error.
 **/
GFileInfo *
g_file_input_stream_query_info (GFileInputStream     *stream,
				   char                 *attributes,
				   GCancellable         *cancellable,
				   GError              **error)
{
  GFileInputStreamClass *class;
  GInputStream *input_stream;
  GFileInfo *info;
  
  g_return_val_if_fail (G_IS_FILE_INPUT_STREAM (stream), NULL);
  
  input_stream = G_INPUT_STREAM (stream);
  
  if (g_input_stream_is_closed (input_stream))
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_CLOSED,
		   _("Stream is already closed"));
      return NULL;
    }
  
  if (g_input_stream_has_pending (input_stream))
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_PENDING,
		   _("Stream has outstanding operation"));
      return NULL;
    }
      
  info = NULL;
  
  g_input_stream_set_pending (input_stream, TRUE);

  if (cancellable)
    g_push_current_cancellable (cancellable);
  
  class = G_FILE_INPUT_STREAM_GET_CLASS (stream);
  if (class->query_info)
    info = class->query_info (stream, attributes, cancellable, error);
  else
    g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
		 _("Stream doesn't support query_info"));

  if (cancellable)
    g_pop_current_cancellable (cancellable);
  
  g_input_stream_set_pending (input_stream, FALSE);
  
  return info;
}

static void
async_ready_callback_wrapper (GObject *source_object,
			      GAsyncResult *res,
			      gpointer      user_data)
{
  GFileInputStream *stream = G_FILE_INPUT_STREAM (source_object);

  g_input_stream_set_pending (G_INPUT_STREAM (stream), FALSE);
  if (stream->priv->outstanding_callback)
    (*stream->priv->outstanding_callback) (source_object, res, user_data);
  g_object_unref (stream);
}

/**
 * g_file_input_stream_query_info_async:
 * @stream: a #GFileInputStream.
 * @attributes: a file attribute query string.
 * @io_priority: the i/o priority of the request.
 * @cancellable: optional #GCancellable object, %NULL to ignore. 
 * @callback: callback to call when the request is satisfied
 * @user_data: the data to pass to callback function
 * 
 * Queries the stream information asynchronously. For the synchronous version
 * of this function, see g_file_input_stream_query_info(). 
 * 
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be set
 *  
 **/
void
g_file_input_stream_query_info_async (GFileInputStream     *stream,
					 char                 *attributes,
					 int                   io_priority,
					 GCancellable         *cancellable,
					 GAsyncReadyCallback   callback,
					 gpointer              user_data)
{
  GFileInputStreamClass *klass;
  GInputStream *input_stream;

  g_return_if_fail (G_IS_FILE_INPUT_STREAM (stream));

  input_stream = G_INPUT_STREAM (stream);
 
  if (g_input_stream_is_closed (input_stream))
    {
      g_simple_async_report_error_in_idle (G_OBJECT (stream),
					   callback,
					   user_data,
					   G_IO_ERROR, G_IO_ERROR_CLOSED,
					   _("Stream is already closed"));
      return;
    }
  
  if (g_input_stream_has_pending (input_stream))
    {
      g_simple_async_report_error_in_idle (G_OBJECT (stream),
					   callback,
					   user_data,
					   G_IO_ERROR, G_IO_ERROR_PENDING,
					   _("Stream has outstanding operation"));
      return;
    }

  klass = G_FILE_INPUT_STREAM_GET_CLASS (stream);

  g_input_stream_set_pending (input_stream, TRUE);
  stream->priv->outstanding_callback = callback;
  g_object_ref (stream);
  klass->query_info_async (stream, attributes, io_priority, cancellable,
			      async_ready_callback_wrapper, user_data);
}

/**
 * g_file_input_stream_query_info_finish:
 * @stream: a #GFileInputStream.
 * @result: a #GAsyncResult.
 * @error: a #GError location to store the error occuring, or %NULL to 
 * ignore.
 * 
 * Finishes an asynchronous info query operation.
 * 
 * Returns: #GFileInfo. 
 **/
GFileInfo *
g_file_input_stream_query_info_finish (GFileInputStream     *stream,
					  GAsyncResult         *result,
					  GError              **error)
{
  GSimpleAsyncResult *simple;
  GFileInputStreamClass *class;

  g_return_val_if_fail (G_IS_FILE_INPUT_STREAM (stream), NULL);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), NULL);

  if (G_IS_SIMPLE_ASYNC_RESULT (result))
    {
      simple = G_SIMPLE_ASYNC_RESULT (result);
      if (g_simple_async_result_propagate_error (simple, error))
	return NULL;
    }

  class = G_FILE_INPUT_STREAM_GET_CLASS (stream);
  return class->query_info_finish (stream, result, error);
}

/**
 * g_file_input_stream_tell:
 * @stream: a #GFileInputStream.
 * 
 * Gets the current position in the stream.
 * 
 * Returns: a #goffset with the position in the stream.
 **/
goffset
g_file_input_stream_tell (GFileInputStream  *stream)
{
  GFileInputStreamClass *class;
  goffset offset;

  g_return_val_if_fail (G_IS_FILE_INPUT_STREAM (stream), 0);

  class = G_FILE_INPUT_STREAM_GET_CLASS (stream);

  offset = 0;
  if (class->tell)
    offset = class->tell (stream);

  return offset;
}

static goffset
g_file_input_stream_seekable_tell (GSeekable *seekable)
{
  return g_file_input_stream_tell (G_FILE_INPUT_STREAM (seekable));
}

/**
 * g_file_input_stream_can_seek:
 * @stream: a #GFileInputStream.
 * 
 * Checks if a file input stream can be seeked.
 * 
 * Returns: %TRUE if stream can be seeked. %FALSE otherwise.
 **/
gboolean
g_file_input_stream_can_seek (GFileInputStream  *stream)
{
  GFileInputStreamClass *class;
  gboolean can_seek;

  g_return_val_if_fail (G_IS_FILE_INPUT_STREAM (stream), FALSE);

  class = G_FILE_INPUT_STREAM_GET_CLASS (stream);

  can_seek = FALSE;
  if (class->seek)
    {
      can_seek = TRUE;
      if (class->can_seek)
	can_seek = class->can_seek (stream);
    }
  
  return can_seek;
}

static gboolean
g_file_input_stream_seekable_can_seek (GSeekable *seekable)
{
  return g_file_input_stream_can_seek (G_FILE_INPUT_STREAM (seekable));
}

/**
 * g_file_input_stream_seek:
 * @stream: a #GFileInputStream.
 * @offset: a #goffset to seek.
 * @type: a #GSeekType.
 * @cancellable: optional #GCancellable object, %NULL to ignore. 
 * @error: a #GError location to store the error occuring, or %NULL to 
 * ignore.
 * 
 * Seeks in the file input stream.
 * 
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be set.
 * 
 * Returns: %TRUE if the stream was successfully seeked to the position.
 * %FALSE on error.
 **/
gboolean
g_file_input_stream_seek (GFileInputStream  *stream,
			  goffset            offset,
			  GSeekType          type,
			  GCancellable      *cancellable,
			  GError           **error)
{
  GFileInputStreamClass *class;
  GInputStream *input_stream;
  gboolean res;

  g_return_val_if_fail (G_IS_FILE_INPUT_STREAM (stream), FALSE);

  input_stream = G_INPUT_STREAM (stream);
  class = G_FILE_INPUT_STREAM_GET_CLASS (stream);

  if (g_input_stream_is_closed (input_stream))
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_CLOSED,
		   _("Stream is already closed"));
      return FALSE;
    }
  
  if (g_input_stream_has_pending (input_stream))
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_PENDING,
		   _("Stream has outstanding operation"));
      return FALSE;
    }
  
  if (!class->seek)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
		   _("Seek not supported on stream"));
      return FALSE;
    }

  g_input_stream_set_pending (input_stream, TRUE);
  
  if (cancellable)
    g_push_current_cancellable (cancellable);
  
  res = class->seek (stream, offset, type, cancellable, error);
  
  if (cancellable)
    g_pop_current_cancellable (cancellable);

  g_input_stream_set_pending (input_stream, FALSE);
  
  return res;
}

static gboolean
g_file_input_stream_seekable_seek (GSeekable  *seekable,
				   goffset     offset,
				   GSeekType   type,
				   GCancellable  *cancellable,
				   GError    **error)
{
  return g_file_input_stream_seek (G_FILE_INPUT_STREAM (seekable),
				   offset, type, cancellable, error);
}

static gboolean
g_file_input_stream_seekable_can_truncate (GSeekable  *seekable)
{
  return FALSE;
}

static gboolean
g_file_input_stream_seekable_truncate (GSeekable  *seekable,
				       goffset     offset,
				       GCancellable  *cancellable,
				       GError    **error)
{
  g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
	       _("Truncate not allowed on input stream"));
  return FALSE;
}

/********************************************
 *   Default implementation of async ops    *
 ********************************************/

typedef struct {
  char *attributes;
  GFileInfo *info;
} QueryInfoAsyncData;

static void
query_info_data_free (QueryInfoAsyncData *data)
{
  if (data->info)
    g_object_unref (data->info);
  g_free (data->attributes);
  g_free (data);
}

static void
query_info_async_thread (GSimpleAsyncResult *res,
		       GObject *object,
		       GCancellable *cancellable)
{
  GFileInputStreamClass *class;
  GError *error = NULL;
  QueryInfoAsyncData *data;
  GFileInfo *info;
  
  data = g_simple_async_result_get_op_res_gpointer (res);

  info = NULL;
  
  class = G_FILE_INPUT_STREAM_GET_CLASS (object);
  if (class->query_info)
    info = class->query_info (G_FILE_INPUT_STREAM (object), data->attributes, cancellable, &error);
  else
    g_set_error (&error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
		 _("Stream doesn't support query_info"));

  if (info == NULL)
    {
      g_simple_async_result_set_from_error (res, error);
      g_error_free (error);
    }
  else
    data->info = info;
}

static void
g_file_input_stream_real_query_info_async (GFileInputStream     *stream,
					      char                 *attributes,
					      int                   io_priority,
					      GCancellable         *cancellable,
					      GAsyncReadyCallback   callback,
					      gpointer              user_data)
{
  GSimpleAsyncResult *res;
  QueryInfoAsyncData *data;

  data = g_new0 (QueryInfoAsyncData, 1);
  data->attributes = g_strdup (attributes);
  
  res = g_simple_async_result_new (G_OBJECT (stream), callback, user_data, g_file_input_stream_real_query_info_async);
  g_simple_async_result_set_op_res_gpointer (res, data, (GDestroyNotify)query_info_data_free);
  
  g_simple_async_result_run_in_thread (res, query_info_async_thread, io_priority, cancellable);
  g_object_unref (res);
}

static GFileInfo *
g_file_input_stream_real_query_info_finish (GFileInputStream     *stream,
					       GAsyncResult         *res,
					       GError              **error)
{
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (res);
  QueryInfoAsyncData *data;

  g_assert (g_simple_async_result_get_source_tag (simple) == g_file_input_stream_real_query_info_async);

  data = g_simple_async_result_get_op_res_gpointer (simple);
  if (data->info)
    return g_object_ref (data->info);
  
  return NULL;
}

#define __G_FILE_INPUT_STREAM_C__
#include "gioaliasdef.c"

