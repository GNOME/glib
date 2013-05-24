/**
 * SECTION:gkeyfileencoder
 * @Title: GKeyfileEncoder
 * @Short_Description: Encodes and decodes data to key files
 *
 * #GKeyfileEncoder is a #GEncoder implementation that stores data in
 * specially formatted key files.
 */
#include "config.h"

#include "gkeyfileencoder.h"
#include "gencoder.h"
#include "gioerror.h"
#include "glibintl.h"

#include <string.h>

#define DEFAULT_SECTION_NAME    "General"
#define TYPE_KEY                "Type"

struct _GKeyfileEncoder
{
  GEncoder parent_instance;

  char *section_name;

  GKeyFile *key_file;
};

struct _GKeyfileEncoderClass
{
  GEncoderClass parent_class;
};

enum
{
  PROP_0,

  PROP_SECTION_NAME,

  PROP_LAST
};

static GParamSpec *obj_pspec[PROP_LAST] = { NULL, };

G_DEFINE_TYPE (GKeyfileEncoder, g_keyfile_encoder, G_TYPE_ENCODER)

static void
g_keyfile_encoder_finalize (GObject *gobject)
{
  GKeyfileEncoder *self = G_KEYFILE_ENCODER (gobject);

  g_free (self->section_name);

  if (self->key_file != NULL)
    g_key_file_free (self->key_file);

  G_OBJECT_CLASS (g_keyfile_encoder_parent_class)->finalize (gobject);
};

static void
g_keyfile_encoder_set_property (GObject      *gobject,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GKeyfileEncoder *self = G_KEYFILE_ENCODER (gobject);

  switch (prop_id)
    {
    case PROP_SECTION_NAME:
      g_keyfile_encoder_set_section_name (self, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
g_keyfile_encoder_get_property (GObject    *gobject,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  GKeyfileEncoder *self = G_KEYFILE_ENCODER (gobject);

  switch (prop_id)
    {
    case PROP_SECTION_NAME:
      g_value_set_string (value, self->section_name);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static gboolean
g_keyfile_encoder_read_from_bytes (GEncoder  *encoder,
                                   GBytes    *bytes,
                                   GError   **error)
{
  GKeyfileEncoder *self = G_KEYFILE_ENCODER (encoder);
  GError *internal_error = NULL;
  GKeyFile *key_file;
  gchar **keys;
  gsize keys_len, i;
  gboolean res = TRUE;

  if (self->key_file != NULL)
    g_key_file_free (self->key_file);

  key_file = g_key_file_new ();

  g_key_file_load_from_data (key_file,
                             g_bytes_get_data (bytes, NULL),
                             g_bytes_get_size (bytes),
                             0,
                             &internal_error);
  if (internal_error != NULL)
    goto propagate_error_and_return;

  keys = g_key_file_get_keys (key_file,
                              self->section_name,
                              &keys_len,
                              &internal_error);
  if (internal_error != NULL)
    goto propagate_error_and_return;

  for (i = 0; i < keys_len; i++)
    {
      GError *key_error = NULL;
      char *value_type;
      char *value_str;
      GVariant *value;

      value_type = g_key_file_get_value (key_file,
                                         keys[i],
                                         TYPE_KEY,
                                         &key_error);
      if (key_error != NULL)
        {
          g_set_error (error, G_IO_ERROR,
                       G_IO_ERROR_INVALID_DATA,
                       "Unable to load encoded data: %s",
                       key_error->message);
          g_error_free (key_error);
          res = FALSE;
          break;
        }

      value_str = g_key_file_get_value (key_file,
                                        self->section_name,
                                        keys[i],
                                        &key_error);
      if (key_error != NULL)
        {
          g_set_error (error, G_IO_ERROR,
                       G_IO_ERROR_INVALID_DATA,
                       "Unable to load encoded data: %s",
                       key_error->message);
          g_error_free (key_error);
          g_free (value_type);
          res = FALSE;
          break;
        }

      value = g_variant_parse (G_VARIANT_TYPE (value_type),
                               value_str,
                               NULL,
                               NULL,
                               &key_error);
      if (key_error != NULL)
        {
          g_set_error (error, G_IO_ERROR,
                       G_IO_ERROR_INVALID_DATA,
                       "Unable to load encoded data: %s",
                       key_error->message);
          g_error_free (key_error);
          g_free (value_str);
          g_free (value_type);
          res = FALSE;
          break;
        }

      g_encoder_add_key (encoder, keys[i], value);

      g_variant_unref (value);
      g_free (value_str);
      g_free (value_type);
    }

  g_strfreev (keys);
  g_key_file_unref (key_file);

  return res;

propagate_error_and_return:
  g_propagate_error (error, internal_error);

  return FALSE;
}

static GBytes *
g_keyfile_encoder_write_to_bytes (GEncoder  *encoder,
                                  GError   **error)
{
  GKeyfileEncoder *self = G_KEYFILE_ENCODER (encoder);
  GError *internal_error = NULL;
  char *data;
  gsize len;

  g_encoder_close (encoder);
  if (self->key_file == NULL)
    return NULL;

  data = g_key_file_to_data (self->key_file, &len, &internal_error);
  if (internal_error)
    {
      g_propagate_error (error, internal_error);
      return NULL;
    }

  return g_bytes_new_take (data, len);
}

static void
g_keyfile_encoder_closed (GEncoder *encoder,
                          GVariant *data)
{
  GKeyfileEncoder *self = G_KEYFILE_ENCODER (encoder);
  GVariantIter iter;
  GVariant *entry;

  if (self->key_file != NULL)
    g_key_file_free (self->key_file);

  self->key_file = g_key_file_new ();

  g_variant_iter_init (&iter, data);
  while ((entry = g_variant_iter_next_value (&iter)) != NULL)
    {
      GVariant *key = g_variant_get_child_value (entry, 0);
      GVariant *tmp = g_variant_get_child_value (entry, 1);
      GVariant *value;
      char *value_str;

      value = g_variant_get_variant (tmp);
      value_str = g_variant_print (value, FALSE);

      g_key_file_set_value (self->key_file,
                            self->section_name,
                            g_variant_get_string (key, NULL),
                            value_str);

      g_key_file_set_value (self->key_file,
                            g_variant_get_string (key, NULL),
                            TYPE_KEY,
                            (const char *) g_variant_get_type (value));

      g_free (value_str);
      g_variant_unref (value);
      g_variant_unref (tmp);
      g_variant_unref (key);
      g_variant_unref (entry);
    }
}

static void
g_keyfile_encoder_class_init (GKeyfileEncoderClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GEncoderClass *encoder_class = G_ENCODER_CLASS (klass);

  gobject_class->set_property = g_keyfile_encoder_set_property;
  gobject_class->get_property = g_keyfile_encoder_get_property;
  gobject_class->finalize = g_keyfile_encoder_finalize;

  encoder_class->closed = g_keyfile_encoder_closed;
  encoder_class->read_from_bytes = g_keyfile_encoder_read_from_bytes;
  encoder_class->write_to_bytes = g_keyfile_encoder_write_to_bytes;

  /**
   * GKeyfileEncoder:section-name:
   *
   * The name of the key file section to use when encoding and decoding
   * values.
   *
   * Since: 2.38
   */
  obj_pspec[PROP_SECTION_NAME] =
    g_param_spec_string ("section-name",
                         "Section Name",
                         "The name of the keyfile section to use when encoding and decoding",
                         DEFAULT_SECTION_NAME,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, PROP_LAST, obj_pspec);
}

static void
g_keyfile_encoder_init (GKeyfileEncoder *self)
{
  self->section_name = g_strdup (DEFAULT_SECTION_NAME);
}

/**
 * g_keyfile_encoder_new:
 *
 * Creates a new #GKeyfileEncoder.
 *
 * You can use this class to encode and decode data to and from a
 * specially formated #GKeyFile.
 *
 * Return value: (transfer full): the newly created #GKeyfileEncoder.
 *   Use g_object_unref() to free the resources allocated when done.
 *
 * Since: 2.38
 */
GEncoder *
g_keyfile_encoder_new (void)
{
  return g_object_new (G_TYPE_KEYFILE_ENCODER, NULL);
}

/**
 * g_keyfile_encoder_set_section_name:
 * @encoder: a #GKeyfileEncoder
 * @section_name: the section used for the keys
 *
 * Sets the section name to be used to store the keys.
 *
 * Since: 2.38
 */
void
g_keyfile_encoder_set_section_name (GKeyfileEncoder *encoder,
                                    const char      *section_name)
{
  g_return_if_fail (G_IS_KEYFILE_ENCODER (encoder));
  g_return_if_fail (section_name != NULL);

  if (strcmp (encoder->section_name, section_name) == 0)
    return;

  g_free (encoder->section_name);
  encoder->section_name = g_strdup (section_name);

  g_object_notify_by_pspec (G_OBJECT (encoder), obj_pspec[PROP_SECTION_NAME]);
}

/**
 * g_keyfile_encoder_get_section_name:
 * @encoder: a #GKeyfileEncoder
 *
 * Retrieves the section name set using g_keyfile_encoder_set_section_name().
 *
 * Return value: (transfer none): the section name. The returned string
 *   is owned by the #GKeyfileEncoder and it should not be modified or
 *   freed.
 *
 * Since: 2.38
 */
const char *
g_keyfile_encoder_get_section_name (GKeyfileEncoder *encoder)
{
  g_return_val_if_fail (G_IS_KEYFILE_ENCODER (encoder), NULL);

  return encoder->section_name;
}
