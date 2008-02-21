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
#include "gfilterinputstream.h"
#include "ginputstream.h"
#include "glibintl.h"

#include "gioalias.h"

/**
 * SECTION:gfilterinputstream
 * @short_description: Filter Input Stream
 * @include: gio/gio.h
 *
 **/

enum {
  PROP_0,
  PROP_BASE_STREAM
};

static void     g_filter_input_stream_set_property (GObject      *object,
                                                    guint         prop_id,
                                                    const GValue *value,
                                                    GParamSpec   *pspec);

static void     g_filter_input_stream_get_property (GObject      *object,
                                                    guint         prop_id,
                                                    GValue       *value,
                                                    GParamSpec   *pspec);
static void     g_filter_input_stream_finalize     (GObject *object);


static gssize   g_filter_input_stream_read         (GInputStream         *stream,
                                                    void                 *buffer,
                                                    gsize                 count,
                                                    GCancellable         *cancellable,
                                                    GError              **error);
static gssize   g_filter_input_stream_skip         (GInputStream         *stream,
                                                    gsize                 count,
                                                    GCancellable         *cancellable,
                                                    GError              **error);
static gboolean g_filter_input_stream_close        (GInputStream         *stream,
                                                    GCancellable         *cancellable,
                                                    GError              **error);
static void     g_filter_input_stream_read_async   (GInputStream         *stream,
                                                    void                 *buffer,
                                                    gsize                 count,
                                                    int                   io_priority,
                                                    GCancellable         *cancellable,
                                                    GAsyncReadyCallback   callback,
                                                    gpointer              user_data);
static gssize   g_filter_input_stream_read_finish  (GInputStream         *stream,
                                                    GAsyncResult         *result,
                                                    GError              **error);
static void     g_filter_input_stream_skip_async   (GInputStream         *stream,
                                                    gsize                 count,
                                                    int                   io_priority,
                                                    GCancellable         *cancellabl,
                                                    GAsyncReadyCallback   callback,
                                                    gpointer              datae);
static gssize   g_filter_input_stream_skip_finish  (GInputStream         *stream,
                                                    GAsyncResult         *result,
                                                    GError              **error);
static void     g_filter_input_stream_close_async  (GInputStream         *stream,
                                                    int                   io_priority,
                                                    GCancellable         *cancellabl,
                                                    GAsyncReadyCallback   callback,
                                                    gpointer              data);
static gboolean g_filter_input_stream_close_finish (GInputStream         *stream,
                                                    GAsyncResult         *result,
                                                    GError              **error);

G_DEFINE_TYPE (GFilterInputStream, g_filter_input_stream, G_TYPE_INPUT_STREAM)


static void
g_filter_input_stream_class_init (GFilterInputStreamClass *klass)
{
  GObjectClass *object_class;
  GInputStreamClass *istream_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->get_property = g_filter_input_stream_get_property;
  object_class->set_property = g_filter_input_stream_set_property;
  object_class->finalize     = g_filter_input_stream_finalize;

  istream_class = G_INPUT_STREAM_CLASS (klass);
  istream_class->read_fn  = g_filter_input_stream_read;
  istream_class->skip  = g_filter_input_stream_skip;
  istream_class->close_fn = g_filter_input_stream_close;

  istream_class->read_async   = g_filter_input_stream_read_async;
  istream_class->read_finish  = g_filter_input_stream_read_finish;
  istream_class->skip_async   = g_filter_input_stream_skip_async;
  istream_class->skip_finish  = g_filter_input_stream_skip_finish;
  istream_class->close_async  = g_filter_input_stream_close_async;
  istream_class->close_finish = g_filter_input_stream_close_finish;

  g_object_class_install_property (object_class,
                                   PROP_BASE_STREAM,
                                   g_param_spec_object ("base-stream",
                                                         P_("The Filter Base Stream"),
                                                         P_("The underlying base stream the io ops will be done on"),
                                                         G_TYPE_INPUT_STREAM,
                                                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | 
                                                         G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB));

}

static void
g_filter_input_stream_set_property (GObject         *object,
                                    guint            prop_id,
                                    const GValue    *value,
                                    GParamSpec      *pspec)
{
  GFilterInputStream *filter_stream;
  GObject *obj;

  filter_stream = G_FILTER_INPUT_STREAM (object);

  switch (prop_id) 
    {
    case PROP_BASE_STREAM:
      obj = g_value_dup_object (value);
      filter_stream->base_stream = G_INPUT_STREAM (obj); 
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }

}

static void
g_filter_input_stream_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  GFilterInputStream *filter_stream;

  filter_stream = G_FILTER_INPUT_STREAM (object);

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
g_filter_input_stream_finalize (GObject *object)
{
  GFilterInputStream *stream;

  stream = G_FILTER_INPUT_STREAM (object);

  g_object_unref (stream->base_stream);

  if (G_OBJECT_CLASS (g_filter_input_stream_parent_class)->finalize)
    (*G_OBJECT_CLASS (g_filter_input_stream_parent_class)->finalize) (object);
}

static void
g_filter_input_stream_init (GFilterInputStream *stream)
{

}

/**
 * g_filter_input_stream_get_base_stream:
 * @stream: a #GFilterInputStream.
 * 
 * Gets the base stream for the filter stream.
 *
 * Returns: a #GInputStream.
 **/
GInputStream *
g_filter_input_stream_get_base_stream (GFilterInputStream *stream)
{
  g_return_val_if_fail (G_IS_FILTER_INPUT_STREAM (stream), NULL);

  return stream->base_stream;
}

static gssize
g_filter_input_stream_read (GInputStream  *stream,
                            void          *buffer,
                            gsize          count,
                            GCancellable  *cancellable,
                            GError       **error)
{
  GFilterInputStream *filter_stream;
  GInputStream       *base_stream;
  gssize              nread;

  filter_stream = G_FILTER_INPUT_STREAM (stream);
  base_stream = filter_stream->base_stream;

  nread = g_input_stream_read (base_stream,
                               buffer,
                               count,
                               cancellable,
                               error);

  return nread;
}

static gssize
g_filter_input_stream_skip (GInputStream  *stream,
                            gsize          count,
                            GCancellable  *cancellable,
                            GError       **error)
{
  GFilterInputStream *filter_stream;
  GInputStream       *base_stream;
  gssize              nskipped;

  filter_stream = G_FILTER_INPUT_STREAM (stream);
  base_stream = filter_stream->base_stream;

  nskipped = g_input_stream_skip (base_stream,
                                  count,
                                  cancellable,
                                  error);
  return nskipped;
}

static gboolean
g_filter_input_stream_close (GInputStream  *stream,
                             GCancellable  *cancellable,
                             GError       **error)
{
  GFilterInputStream *filter_stream;
  GInputStream       *base_stream;
  gboolean            res;

  filter_stream = G_FILTER_INPUT_STREAM (stream);
  base_stream = filter_stream->base_stream;

  res = g_input_stream_close (base_stream,
                              cancellable,
                              error);

  return res;
}

static void
g_filter_input_stream_read_async (GInputStream        *stream,
                                  void                *buffer,
                                  gsize                count,
                                  int                  io_priority,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data)
{
  GFilterInputStream *filter_stream;
  GInputStream       *base_stream;

  filter_stream = G_FILTER_INPUT_STREAM (stream);
  base_stream = filter_stream->base_stream;

  g_input_stream_read_async (base_stream,
                             buffer,
                             count,
                             io_priority,
                             cancellable,
                             callback,
                             user_data);
}

static gssize
g_filter_input_stream_read_finish (GInputStream  *stream,
                                   GAsyncResult  *result,
                                   GError       **error)
{
  GFilterInputStream *filter_stream;
  GInputStream       *base_stream;
  gssize nread;

  filter_stream = G_FILTER_INPUT_STREAM (stream);
  base_stream = filter_stream->base_stream;

  nread = g_input_stream_read_finish (base_stream,
                                      result,
                                      error);
  
  return nread;
}

static void
g_filter_input_stream_skip_async (GInputStream        *stream,
                                  gsize                count,
                                  int                  io_priority,
                                  GCancellable        *cancellable,
                                  GAsyncReadyCallback  callback,
                                  gpointer             user_data)
{
  GFilterInputStream *filter_stream;
  GInputStream       *base_stream;

  filter_stream = G_FILTER_INPUT_STREAM (stream);
  base_stream = filter_stream->base_stream;

  g_input_stream_skip_async (base_stream,
                             count,
                             io_priority,
                             cancellable,
                             callback,
                             user_data);

}

static gssize
g_filter_input_stream_skip_finish (GInputStream  *stream,
                                   GAsyncResult  *result,
                                   GError       **error)
{
  GFilterInputStream *filter_stream;
  GInputStream       *base_stream;
  gssize nskipped;

  filter_stream = G_FILTER_INPUT_STREAM (stream);
  base_stream = filter_stream->base_stream;

  nskipped = g_input_stream_skip_finish (base_stream,
                                         result,
                                         error);

  return nskipped;
}

static void
g_filter_input_stream_close_async (GInputStream        *stream,
                                   int                  io_priority,
                                   GCancellable        *cancellable,
                                   GAsyncReadyCallback  callback,
                                   gpointer             user_data)
{
  GFilterInputStream *filter_stream;
  GInputStream       *base_stream;

  filter_stream = G_FILTER_INPUT_STREAM (stream);
  base_stream = filter_stream->base_stream;

  g_input_stream_close_async (base_stream,
                              io_priority,
                              cancellable,
                              callback,
                              user_data);
}

static gboolean
g_filter_input_stream_close_finish (GInputStream  *stream,
                                    GAsyncResult  *result,
                                    GError       **error)
{
  GFilterInputStream *filter_stream;
  GInputStream       *base_stream;
  gboolean res;

  filter_stream = G_FILTER_INPUT_STREAM (stream);
  base_stream = filter_stream->base_stream;

  res = g_input_stream_close_finish (stream,
                                     result,
                                     error);

  return res;
}

#define __G_FILTER_INPUT_STREAM_C__
#include "gioaliasdef.c"
