/* GIO - GLib Input, Output and Streaming Library
 *
 * SPDX-FileCopyrightText: 2009 Red Hat, Inc.
 * SPDX-FileCopyrightText: 2010 Christian Persch
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "gbase64encoder.h"

#include <string.h>

#include <glib.h>

#include "gioenums.h"
#include "gioenumtypes.h"
#include "gioerror.h"
#include "glib.h"
#include "glibintl.h"


/* See g_base64_encode_step docs in ../glib/gbase64.c */
#define BASE64_ENCODING_OUTPUT_SIZE(len, break_lines) \
  (((len) / 3 + 1) * 4 + 4 + ((break_lines) ? ((((len) / 3 + 1) * 4 + 4) / 72 + 1) : 0))

/**
 * GBase64Encoder:
 *
 * `GBase64Encoder` is an implementation of [iface@Gio.Converter]
 * that converts data to base64 encoding.
 *
 * Since: 2.82
 */

struct _GBase64Encoder
{
  GObject parent_instance;

  gboolean break_lines;
  int state[2];
};

typedef enum
{
  PROP_BREAK_LINES = 1,
} GBase64EncoderProperty;

static void
g_base64_encoder_reset (GConverter *converter)
{
  GBase64Encoder *encoder = G_BASE64_ENCODER (converter);

  encoder->state[0] = encoder->state[1] = 0;
}

static GConverterResult
g_base64_encoder_convert (GConverter       *converter,
                          const void       *inbuf,
                          gsize             inbuf_size,
                          void             *outbuf,
                          gsize             outbuf_size,
                          GConverterFlags   flags,
                          gsize            *bytes_read,
                          gsize            *bytes_written,
                          GError          **error)
{
  GBase64Encoder *encoder = G_BASE64_ENCODER (converter);

  if (inbuf_size == 0 || (flags & G_CONVERTER_FLUSH))
    {
      if (outbuf_size < (encoder->break_lines ? 5 : 4))
        {
          g_set_error_literal (error,
                               G_IO_ERROR, G_IO_ERROR_NO_SPACE,
                               "Need more output space");
          return G_CONVERTER_ERROR;
        }

      *bytes_read = 0;
      *bytes_written = g_base64_encode_close (encoder->break_lines, outbuf,
                                              &encoder->state[0],
                                              &encoder->state[1]);

      if (flags & G_CONVERTER_FLUSH)
        return G_CONVERTER_FLUSHED;
      if (flags & G_CONVERTER_INPUT_AT_END)
        return G_CONVERTER_FINISHED;

      return G_CONVERTER_CONVERTED;
    }

  if (outbuf_size < BASE64_ENCODING_OUTPUT_SIZE (inbuf_size, encoder->break_lines))
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NO_SPACE,
                           "Need more output space");
      return G_CONVERTER_ERROR;
    }

  *bytes_read = inbuf_size;
  *bytes_written = g_base64_encode_step (inbuf, inbuf_size,
                                         encoder->break_lines,
                                         outbuf,
                                         &encoder->state[0],
                                         &encoder->state[1]);
  if (flags & G_CONVERTER_INPUT_AT_END)
    return G_CONVERTER_FINISHED;

  return G_CONVERTER_CONVERTED;
}

static void
g_base64_encoder_iface_init (GConverterIface *iface)
{
  iface->convert = g_base64_encoder_convert;
  iface->reset = g_base64_encoder_reset;
}

G_DEFINE_TYPE_WITH_CODE (GBase64Encoder, g_base64_encoder, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_CONVERTER, g_base64_encoder_iface_init))

static void
g_base64_encoder_init (GBase64Encoder *encoder)
{
  encoder->break_lines = FALSE;
  encoder->state[0] = encoder->state[1] = 0;
}

static void
g_base64_encoder_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  GBase64Encoder *encoder = G_BASE64_ENCODER (object);

  switch ((GBase64EncoderProperty) prop_id)
    {
    case PROP_BREAK_LINES:
      encoder->break_lines = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
g_base64_encoder_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  GBase64Encoder *encoder = G_BASE64_ENCODER (object);

  switch ((GBase64EncoderProperty) prop_id)
    {
    case PROP_BREAK_LINES:
      g_value_set_boolean (value, encoder->break_lines);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
g_base64_encoder_class_init (GBase64EncoderClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->get_property = g_base64_encoder_get_property;
  gobject_class->set_property = g_base64_encoder_set_property;

  /**
   * GBase64Encoder:break-lines:
   *
   * Whether to break lines.
   *
   * This is typically used when putting base64-encoded data in emails.
   * It breaks the lines at 72 columns instead of putting all of the text on
   * the same line. This avoids problems with long lines in the email system.
   *
   * Since: 2.82
   */
  g_object_class_install_property (gobject_class,
                                   PROP_BREAK_LINES,
                                   g_param_spec_boolean ("break-lines", NULL, NULL,
                                                         FALSE,
                                                         G_PARAM_READWRITE |
                                                         G_PARAM_CONSTRUCT_ONLY |
                                                         G_PARAM_STATIC_STRINGS));
}

/**
 * g_base64_encoder_new:
 * @break_lines: whether to break long lines
 *
 * Creates a new `GBase64Encoder`.
 *
 * Setting @break_lines to `TRUE` is typically used when putting
 * base64-encoded data in emails. It breaks the lines at 72 columns
 * instead of putting all of the text on the same line. This avoids
 * problems with long lines in the email system.
 *
 * Returns: (transfer full): a new `GBase64Encoder`
 *
 * Since: 2.82
 */
GConverter *
g_base64_encoder_new (gboolean break_lines)
{
  return g_object_new (G_TYPE_BASE64_ENCODER,
                       "break-lines", break_lines,
                       NULL);
}
