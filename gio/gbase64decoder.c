/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright © 2009 Red Hat, Inc.
 * Copyright © 2010 Christian Persch
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
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
 *         Christian Persch <chpe@gnome.org>
 */

#include "config.h"

#include "gbase64decoder.h"

#include <string.h>

#include <glib.h>

#include "gioerror.h"
#include "glibintl.h"
#include "gioenums.h"
#include "gioenumtypes.h"

/**
 * GBase64Decoder:
 *
 * `GBase64Decoder` is an implementation of `GConverter` that
 * converts data from base64 encoding.
 */

struct _GBase64Decoder
{
  GObject parent_instance;

  int state;
  guint save;
};

struct _GBase64DecoderClass
{
  GObjectClass parent_class;
};

static void
g_base64_decoder_reset (GConverter *converter)
{
  GBase64Decoder *decoder = G_BASE64_DECODER (converter);

  decoder->state = 0;
  decoder->save = 0;
}

static GConverterResult
g_base64_decoder_convert (GConverter *converter,
                          const void *inbuf,
                          gsize       inbuf_size,
                          void       *outbuf,
                          gsize       outbuf_size,
                          GConverterFlags flags,
                          gsize      *bytes_read,
                          gsize      *bytes_written,
                          GError    **error)
{
  GBase64Decoder *decoder = G_BASE64_DECODER (converter);

  if (outbuf_size < ((inbuf_size / 4) * 3 + 3))
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_NO_SPACE,
                  "Not enough space in dest");
      return G_CONVERTER_ERROR;
    }

  *bytes_read = inbuf_size;
  *bytes_written = g_base64_decode_step (inbuf, inbuf_size, outbuf,
                                         &decoder->state, &decoder->save);

  if (flags & G_CONVERTER_FLUSH)
    return G_CONVERTER_FLUSHED;
  if (*bytes_read == inbuf_size && (flags & G_CONVERTER_INPUT_AT_END))
    return G_CONVERTER_FINISHED;

  return G_CONVERTER_CONVERTED;
}

static void
g_base64_decoder_iface_init (GConverterIface *iface)
{
  iface->convert = g_base64_decoder_convert;
  iface->reset = g_base64_decoder_reset;
}

G_DEFINE_TYPE_WITH_CODE (GBase64Decoder, g_base64_decoder, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_CONVERTER,
                                                g_base64_decoder_iface_init))

static void
g_base64_decoder_init (GBase64Decoder *decoder)
{
  decoder->state = 0;
  decoder->save = 0;
}

static void
g_base64_decoder_class_init (GBase64DecoderClass *klass)
{
}

/**
 * g_base64_decoder_new:
 *
 * Creates a new `GBase64Decoder`.
 *
 * Returns: a new `GBase64Decoder`
 *
 * Since: 2.82
 */
GConverter *
g_base64_decoder_new (void)
{
  return g_object_new (G_TYPE_BASE64_DECODER, NULL);
}
