/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright (C) 2009 Red Hat, Inc.
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

#include <errno.h>
#include <zlib.h>
#include <string.h>

#include "gzlibdecompressor.h"
#include "glib.h"
#include "gioerror.h"
#include "glibintl.h"
#include "gioenums.h"
#include "gioenumtypes.h"

#include "gioalias.h"

enum {
  PROP_0,
  PROP_FORMAT
};

/**
 * SECTION:gzdecompressor
 * @short_description: Zlib decompressor
 * @include: gio/gio.h
 *
 * #GZlibDecompressor is an implementation of #GConverter that
 * decompresses data compressed with zlib.
 */

static void g_zlib_decompressor_iface_init          (GConverterIface *iface);

/**
 * GZlibDecompressor:
 *
 * Zlib decompression
 */
struct _GZlibDecompressor
{
  GObject parent_instance;

  GZlibCompressorFormat format;
  z_stream zstream;
};

G_DEFINE_TYPE_WITH_CODE (GZlibDecompressor, g_zlib_decompressor, G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (G_TYPE_CONVERTER,
						g_zlib_decompressor_iface_init))

static void
g_zlib_decompressor_finalize (GObject *object)
{
  GZlibDecompressor *decompressor;

  decompressor = G_ZLIB_DECOMPRESSOR (object);

  inflateEnd (&decompressor->zstream);

  G_OBJECT_CLASS (g_zlib_decompressor_parent_class)->finalize (object);
}


static void
g_zlib_decompressor_set_property (GObject      *object,
				  guint         prop_id,
				  const GValue *value,
				  GParamSpec   *pspec)
{
  GZlibDecompressor *decompressor;

  decompressor = G_ZLIB_DECOMPRESSOR (object);

  switch (prop_id)
    {
    case PROP_FORMAT:
      decompressor->format = g_value_get_enum (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }

}

static void
g_zlib_decompressor_get_property (GObject    *object,
				  guint       prop_id,
				  GValue     *value,
				  GParamSpec *pspec)
{
  GZlibDecompressor *decompressor;

  decompressor = G_ZLIB_DECOMPRESSOR (object);

  switch (prop_id)
    {
    case PROP_FORMAT:
      g_value_set_enum (value, decompressor->format);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
g_zlib_decompressor_init (GZlibDecompressor *decompressor)
{
}

static void
g_zlib_decompressor_constructed (GObject *object)
{
  GZlibDecompressor *decompressor;
  int res;

  decompressor = G_ZLIB_DECOMPRESSOR (object);

  if (decompressor->format == G_ZLIB_COMPRESSOR_FORMAT_GZIP)
    {
      /* + 16 for gzip */
      res = inflateInit2 (&decompressor->zstream, MAX_WBITS + 16);
    }
  else if (decompressor->format == G_ZLIB_COMPRESSOR_FORMAT_RAW)
    {
      /* Negative for gzip */
      res = inflateInit2 (&decompressor->zstream, -MAX_WBITS);
    }
  else /* ZLIB */
    res = inflateInit (&decompressor->zstream);

  if (res == Z_MEM_ERROR )
    g_error ("GZlibDecompressor: Not enough memory for zlib use");

  if (res != Z_OK)
    g_warning ("unexpected zlib error: %s\n", decompressor->zstream.msg);
}

static void
g_zlib_decompressor_class_init (GZlibDecompressorClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = g_zlib_decompressor_finalize;
  gobject_class->constructed = g_zlib_decompressor_constructed;
  gobject_class->get_property = g_zlib_decompressor_get_property;
  gobject_class->set_property = g_zlib_decompressor_set_property;

  g_object_class_install_property (gobject_class,
				   PROP_FORMAT,
				   g_param_spec_enum ("format",
						      P_("compression format"),
						      P_("The format of the compressed data"),
						      G_TYPE_ZLIB_COMPRESSOR_FORMAT,
						      G_ZLIB_COMPRESSOR_FORMAT_ZLIB,
						      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
						      G_PARAM_STATIC_STRINGS));
}

/**
 * g_zlib_decompressor_new:
 * @format: The format to use for the compressed data
 *
 * Creates a new #GZlibDecompressor.
 *
 * Returns: a new #GZlibDecompressor
 *
 * Since: 2.24
 **/
GZlibDecompressor *
g_zlib_decompressor_new (GZlibCompressorFormat format)
{
  GZlibDecompressor *decompressor;

  decompressor = g_object_new (G_TYPE_ZLIB_DECOMPRESSOR,
			       "format", format,
			       NULL);

  return decompressor;
}

static void
g_zlib_decompressor_reset (GConverter *converter)
{
  GZlibDecompressor *decompressor = G_ZLIB_DECOMPRESSOR (converter);
  int res;

  res = inflateReset (&decompressor->zstream);
  if (res != Z_OK)
    g_warning ("unexpected zlib error: %s\n", decompressor->zstream.msg);
}

static GConverterResult
g_zlib_decompressor_convert (GConverter *converter,
			     const void *inbuf,
			     gsize       inbuf_size,
			     void       *outbuf,
			     gsize       outbuf_size,
			     GConverterFlags flags,
			     gsize      *bytes_read,
			     gsize      *bytes_written,
			     GError    **error)
{
  GZlibDecompressor *decompressor;
  int res;

  decompressor = G_ZLIB_DECOMPRESSOR (converter);

  decompressor->zstream.next_in = (void *)inbuf;
  decompressor->zstream.avail_in = inbuf_size;

  decompressor->zstream.next_out = outbuf;
  decompressor->zstream.avail_out = outbuf_size;

  res = inflate (&decompressor->zstream, Z_NO_FLUSH);

  if (res == Z_DATA_ERROR || res == Z_NEED_DICT)
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
			   _("Invalid compressed data"));
      return G_CONVERTER_ERROR;
    }

  if (res == Z_MEM_ERROR)
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
			   _("Not enough memory"));
      return G_CONVERTER_ERROR;
    }

    if (res == Z_STREAM_ERROR)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
		   _("Internal error: %s"), decompressor->zstream.msg);
      return G_CONVERTER_ERROR;
    }

    if (res == Z_BUF_ERROR)
      {
	if (flags & G_CONVERTER_FLUSH)
	  return G_CONVERTER_FLUSHED;

	/* Z_FINISH not set, so this means no progress could be made */
	/* We do have output space, so this should only happen if we
	   have no input but need some */

	g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_PARTIAL_INPUT,
			     _("Need more input"));
	return G_CONVERTER_ERROR;
      }

  if (res == Z_OK || res == Z_STREAM_END)
    {
      *bytes_read = inbuf_size - decompressor->zstream.avail_in;
      *bytes_written = outbuf_size - decompressor->zstream.avail_out;

      if (res == Z_STREAM_END)
	return G_CONVERTER_FINISHED;
      return G_CONVERTER_CONVERTED;
    }

  g_assert_not_reached ();
}

static void
g_zlib_decompressor_iface_init (GConverterIface *iface)
{
  iface->convert = g_zlib_decompressor_convert;
  iface->reset = g_zlib_decompressor_reset;
}

#define __G_ZLIB_DECOMPRESSOR_C__
#include "gioaliasdef.c"
