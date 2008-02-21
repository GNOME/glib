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
#include "gfilteroutputstream.h"
#include "goutputstream.h"
#include "glibintl.h"

#include "gioalias.h"

/**
 * SECTION:gfilteroutputstream
 * @short_description: Filter Output Stream
 * @include: gio/gio.h
 *
 **/

enum {
  PROP_0,
  PROP_BASE_STREAM
};

static void     g_filter_output_stream_set_property (GObject      *object,
                                                     guint         prop_id,
                                                     const GValue *value,
                                                     GParamSpec   *pspec);

static void     g_filter_output_stream_get_property (GObject    *object,
                                                     guint       prop_id,
                                                     GValue     *value,
                                                     GParamSpec *pspec);
static void     g_filter_output_stream_dispose      (GObject *object);


static gssize   g_filter_output_stream_write        (GOutputStream *stream,
                                                     const void    *buffer,
                                                     gsize          count,
                                                     GCancellable  *cancellable,
                                                     GError       **error);
static gboolean g_filter_output_stream_flush        (GOutputStream    *stream,
                                                     GCancellable  *cancellable,
                                                     GError          **error);
static gboolean g_filter_output_stream_close        (GOutputStream  *stream,
                                                     GCancellable   *cancellable,
                                                     GError        **error);
static void     g_filter_output_stream_write_async  (GOutputStream        *stream,
                                                     const void           *buffer,
                                                     gsize                 count,
                                                     int                   io_priority,
                                                     GCancellable         *cancellable,
                                                     GAsyncReadyCallback   callback,
                                                     gpointer              data);
static gssize   g_filter_output_stream_write_finish (GOutputStream        *stream,
                                                     GAsyncResult         *result,
                                                     GError              **error);
static void     g_filter_output_stream_flush_async  (GOutputStream        *stream,
                                                     int                   io_priority,
                                                     GCancellable         *cancellable,
                                                     GAsyncReadyCallback   callback,
                                                     gpointer              data);
static gboolean g_filter_output_stream_flush_finish (GOutputStream        *stream,
                                                     GAsyncResult         *result,
                                                     GError              **error);
static void     g_filter_output_stream_close_async  (GOutputStream        *stream,
                                                     int                   io_priority,
                                                     GCancellable         *cancellable,
                                                     GAsyncReadyCallback   callback,
                                                     gpointer              data);
static gboolean g_filter_output_stream_close_finish (GOutputStream        *stream,
                                                     GAsyncResult         *result,
                                                     GError              **error);



G_DEFINE_TYPE (GFilterOutputStream, g_filter_output_stream, G_TYPE_OUTPUT_STREAM)



static void
g_filter_output_stream_class_init (GFilterOutputStreamClass *klass)
{
  GObjectClass *object_class;
  GOutputStreamClass *ostream_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->get_property = g_filter_output_stream_get_property;
  object_class->set_property = g_filter_output_stream_set_property;
  object_class->dispose      = g_filter_output_stream_dispose;
    
  ostream_class = G_OUTPUT_STREAM_CLASS (klass);
  ostream_class->write_fn = g_filter_output_stream_write;
  ostream_class->flush = g_filter_output_stream_flush;
  ostream_class->close_fn = g_filter_output_stream_close;
  ostream_class->write_async  = g_filter_output_stream_write_async;
  ostream_class->write_finish = g_filter_output_stream_write_finish;
  ostream_class->flush_async  = g_filter_output_stream_flush_async;
  ostream_class->flush_finish = g_filter_output_stream_flush_finish;
  ostream_class->close_async  = g_filter_output_stream_close_async;
  ostream_class->close_finish = g_filter_output_stream_close_finish;

  g_object_class_install_property (object_class,
                                   PROP_BASE_STREAM,
                                   g_param_spec_object ("base-stream",
                                                         P_("The Filter Base Stream"),
                                                         P_("The underlying base stream the io ops will be done on"),
                                                         G_TYPE_OUTPUT_STREAM,
                                                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | 
                                                         G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB));

}

static void
g_filter_output_stream_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  GFilterOutputStream *filter_stream;
  GObject *obj;

  filter_stream = G_FILTER_OUTPUT_STREAM (object);

  switch (prop_id) 
    {
    case PROP_BASE_STREAM:
      obj = g_value_dup_object (value);
      filter_stream->base_stream = G_OUTPUT_STREAM (obj);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }

}

static void
g_filter_output_stream_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  GFilterOutputStream *filter_stream;

  filter_stream = G_FILTER_OUTPUT_STREAM (object);

  switch (prop_id)
    {
    case PROP_BASE_STREAM:
      g_value_set_object (value, filter_stream->base_stream);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }

}

static void
g_filter_output_stream_dispose (GObject *object)
{
  GFilterOutputStream *stream;

  stream = G_FILTER_OUTPUT_STREAM (object);

  G_OBJECT_CLASS (g_filter_output_stream_parent_class)->dispose (object);
  
  if (stream->base_stream)
    {
      g_object_unref (stream->base_stream);
      stream->base_stream = NULL;
    }
}


static void
g_filter_output_stream_init (GFilterOutputStream *stream)
{
}

/**
 * g_filter_output_stream_get_base_stream:
 * @stream: a #GFilterOutputStream.
 * 
 * Gets the base stream for the filter stream.
 *
 * Returns: a #GOutputStream.
 **/
GOutputStream *
g_filter_output_stream_get_base_stream (GFilterOutputStream *stream)
{
  g_return_val_if_fail (G_IS_FILTER_OUTPUT_STREAM (stream), NULL);

  return stream->base_stream;
}

static gssize
g_filter_output_stream_write (GOutputStream  *stream,
                              const void     *buffer,
                              gsize           count,
                              GCancellable   *cancellable,
                              GError        **error)
{
  GFilterOutputStream *filter_stream;
  gssize nwritten;

  filter_stream = G_FILTER_OUTPUT_STREAM (stream);

  nwritten = g_output_stream_write (filter_stream->base_stream,
                                    buffer,
                                    count,
                                    cancellable,
                                    error);

  return nwritten;
}

static gboolean
g_filter_output_stream_flush (GOutputStream  *stream,
                              GCancellable   *cancellable,
                              GError        **error)
{
  GFilterOutputStream *filter_stream;
  gboolean res;

  filter_stream = G_FILTER_OUTPUT_STREAM (stream);

  res = g_output_stream_flush (filter_stream->base_stream,
                               cancellable,
                               error);

  return res;
}

static gboolean
g_filter_output_stream_close (GOutputStream  *stream,
                              GCancellable   *cancellable,
                              GError        **error)
{
  GFilterOutputStream *filter_stream;
  gboolean res;

  filter_stream = G_FILTER_OUTPUT_STREAM (stream);

  res = g_output_stream_close (filter_stream->base_stream,
                               cancellable,
                               error);

  return res;
}

static void
g_filter_output_stream_write_async (GOutputStream       *stream,
                                    const void          *buffer,
                                    gsize                count,
                                    int                  io_priority,
                                    GCancellable        *cancellable,
                                    GAsyncReadyCallback  callback,
                                    gpointer             data)
{
  GFilterOutputStream *filter_stream;

  filter_stream = G_FILTER_OUTPUT_STREAM (stream);

  g_output_stream_write_async (filter_stream->base_stream,
                               buffer,
                               count,
                               io_priority,
                               cancellable,
                               callback,
                               data);

}

static gssize
g_filter_output_stream_write_finish (GOutputStream  *stream,
                                     GAsyncResult   *result,
                                     GError        **error)
{
  GFilterOutputStream *filter_stream;
  gssize nwritten;

  filter_stream = G_FILTER_OUTPUT_STREAM (stream);

  nwritten = g_output_stream_write_finish (filter_stream->base_stream,
                                           result,
                                           error);

  return nwritten;
}

static void
g_filter_output_stream_flush_async (GOutputStream       *stream,
                                    int                  io_priority,
                                    GCancellable        *cancellable,
                                    GAsyncReadyCallback  callback,
                                    gpointer             data)
{
  GFilterOutputStream *filter_stream;

  filter_stream = G_FILTER_OUTPUT_STREAM (stream);

  g_output_stream_flush_async (filter_stream->base_stream,
                               io_priority,
                               cancellable,
                               callback,
                               data);
}

static gboolean
g_filter_output_stream_flush_finish (GOutputStream  *stream,
                                     GAsyncResult   *result,
                                     GError        **error)
{
  GFilterOutputStream *filter_stream;
  gboolean res;

  filter_stream = G_FILTER_OUTPUT_STREAM (stream);

  res = g_output_stream_flush_finish (filter_stream->base_stream,
                                      result,
                                      error);

  return res;
}

static void
g_filter_output_stream_close_async (GOutputStream       *stream,
                                    int                  io_priority,
                                    GCancellable        *cancellable,
                                    GAsyncReadyCallback  callback,
                                    gpointer             data)
{
  GFilterOutputStream *filter_stream;

  filter_stream = G_FILTER_OUTPUT_STREAM (stream);

  g_output_stream_close_async (filter_stream->base_stream,
                               io_priority,
                               cancellable,
                               callback,
                               data);
}

static gboolean
g_filter_output_stream_close_finish (GOutputStream  *stream,
                                     GAsyncResult   *result,
                                     GError        **error)
{
  GFilterOutputStream *filter_stream;
  gboolean res;

  filter_stream = G_FILTER_OUTPUT_STREAM (stream);

  res = g_output_stream_close_finish (filter_stream->base_stream,
                                      result,
                                      error);

  return res;
}

#define __G_FILTER_OUTPUT_STREAM_C__
#include "gioaliasdef.c"
