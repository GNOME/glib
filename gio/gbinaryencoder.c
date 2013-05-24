#include "config.h"

#include "gbinaryencoder.h"
#include "gencoder.h"
#include "gioerror.h"
#include "glibintl.h"

#include <string.h>

struct _GBinaryEncoder
{
  GEncoder parent_instance;
};

struct _GBinaryEncoderClass
{
  GEncoderClass parent_class;
};

G_DEFINE_TYPE (GBinaryEncoder, g_binary_encoder, G_TYPE_ENCODER)

static gboolean
g_binary_encoder_read_from_bytes (GEncoder  *encoder,
                                  GBytes    *buffer,
                                  GError   **error)
{
  GError *internal_error;
  GVariant *v, *entry;
  GVariantIter iter;

  internal_error = NULL;
  v = g_variant_parse (G_VARIANT_TYPE ("a{sv}"),
                       g_bytes_get_data (buffer, NULL),
                       NULL,
                       NULL,
                       &internal_error);
  if (internal_error != NULL)
    {
      g_set_error (error, G_IO_ERROR,
                   G_IO_ERROR_INVALID_DATA,
                   "Unable to parse encoded buffer: %s",
                   internal_error->message);
      g_error_free (internal_error);
      return FALSE;
    }

  g_variant_iter_init (&iter, v);
  while ((entry = g_variant_iter_next_value (&iter)) != NULL)
    {
      GVariant *key = g_variant_get_child_value (entry, 0);
      GVariant *tmp = g_variant_get_child_value (entry, 1);
      GVariant *value;

      value = g_variant_get_variant (tmp);

      g_encoder_add_key (encoder,
                         g_variant_get_string (key, NULL),
                         value);

      g_variant_unref (value);
      g_variant_unref (tmp);
      g_variant_unref (key);
    }

  g_variant_unref (v);

  return TRUE;
}

static GBytes *
g_binary_encoder_write_to_bytes (GEncoder  *encoder,
                                 GError   **error)
{
  GVariant *v = g_encoder_close (encoder);
  char *buf = g_variant_print (v, FALSE);

  return g_bytes_new_take (buf, strlen (buf));
}

static void
g_binary_encoder_class_init (GBinaryEncoderClass *klass)
{
  GEncoderClass *encoder_class = G_ENCODER_CLASS (klass);

  encoder_class->read_from_bytes = g_binary_encoder_read_from_bytes;
  encoder_class->write_to_bytes = g_binary_encoder_write_to_bytes;
}

static void
g_binary_encoder_init (GBinaryEncoder *self)
{
}

GEncoder *
g_binary_encoder_new (void)
{
  return g_object_new (G_TYPE_BINARY_ENCODER, NULL);
}
