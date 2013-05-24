/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright (C) 2013  Emmanuele Bassi <ebassi@gnome.org>
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
 */

/**
 * SECTION:gencoder
 * @Title: GEncoder
 * @Short_Description: Encodes and decodes key and value pairs
 *
 * #GEncoder is an abstract class that provides an API to store (encoder) and
 * retrieve (decode) key, value pairs from memory or disk.
 *
 * Implementations of #GEncoder are required to provide the code to read a
 * data storage in the form of a #GBytes and place its contents into the
 * #GEncoder, and the code to write the contents of the #GEncoder into a
 * #GBytes. It is not necessary for a #GEncoder to provide both encoding
 * and decoding: if the #GEncoder sub-class provides only the implementation
 * of the #GEncoderClass.read_from_bytes() virtual function it is called a
 * "decoder"; alternatively, if it only provides the implementation of the
 * the #GEncoderClass.write_to_bytes() virtual function it is called an
 * "encoder".
 */

#include "config.h"

#include "gencoder.h"
#include "glibintl.h"

#include <string.h>

#define G_ENCODER_PRIVATE(obj)  (&G_STRUCT_MEMBER (GEncoderPrivate, (obj), g_encoder_private_offset))

typedef struct _GEncoderPrivate GEncoderPrivate;
typedef struct _EncoderValue    EncoderValue;

#define ENCODER_LOCKED  1
#define ENCODER_CLOSED  2

struct _GEncoderPrivate
{
  GHashTable *values;
  GVariant *encoded;

  gint flags;
};

static gint g_encoder_private_offset = 0;

G_DEFINE_ABSTRACT_TYPE (GEncoder, g_encoder, G_TYPE_OBJECT)

static inline void
g_encoder_lock (GEncoder *encoder)
{
  GEncoderPrivate *priv = G_ENCODER_PRIVATE (encoder);

  g_bit_lock (&priv->flags, ENCODER_LOCKED);
}

static inline void
g_encoder_unlock (GEncoder *encoder)
{
  GEncoderPrivate *priv = G_ENCODER_PRIVATE (encoder);

  g_bit_unlock (&priv->flags, ENCODER_LOCKED);
}

static inline gboolean
g_encoder_is_closed (GEncoder *encoder)
{
  return (G_ENCODER_PRIVATE (encoder)->flags & ENCODER_CLOSED) != FALSE;
}

static void
g_encoder_finalize (GObject *gobject)
{
  GEncoderPrivate *priv = G_ENCODER_PRIVATE (gobject);

  if (priv->values != NULL)
    g_hash_table_unref (priv->values);

  if (priv->encoded != NULL)
    g_variant_unref (priv->encoded);

  G_OBJECT_CLASS (g_encoder_parent_class)->finalize (gobject);
}

static void
g_encoder_real_closed (GEncoder *encoder,
                       GVariant *variant)
{
}

static void
g_encoder_real_value_encoded (GEncoder   *encoder,
                              const char *key,
                              GVariant   *value)
{
}

static gboolean
g_encoder_real_read_from_bytes (GEncoder  *encoder,
                                GBytes    *bytes,
                                GError   **error)
{
  return FALSE;
}

static GBytes *
g_encoder_real_write_to_bytes (GEncoder  *encoder,
                               GError   **error)
{
  return NULL;
}

static void
g_encoder_class_init (GEncoderClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  klass->closed = g_encoder_real_closed;
  klass->value_encoded = g_encoder_real_value_encoded;
  klass->read_from_bytes = g_encoder_real_read_from_bytes;
  klass->write_to_bytes = g_encoder_real_write_to_bytes;

  gobject_class->finalize = g_encoder_finalize;

  g_type_class_add_private (klass, sizeof (GEncoderPrivate));
  g_encoder_private_offset = g_type_class_get_instance_private_offset (klass);
}

static void
g_encoder_init (GEncoder *self)
{
  GEncoderPrivate *priv = G_ENCODER_PRIVATE (self);

  priv->values =
    g_hash_table_new_full (g_str_hash, g_str_equal,
                           g_free,
                           (GDestroyNotify) g_variant_unref);
}

static inline void
g_encoder_value_encoded (GEncoder *encoder,
                         const char *key,
                         GVariant *value)
{
  G_ENCODER_GET_CLASS (encoder)->value_encoded (encoder, key, value);
}

static inline gboolean
g_encoder_add_key_value (GEncoder   *encoder,
                         const char *key,
                         GVariant   *value)
{
  GEncoderPrivate *priv = G_ENCODER_PRIVATE (encoder);
  gboolean res = TRUE;

  g_encoder_lock (encoder);

  if (g_hash_table_lookup (priv->values, key) != NULL)
    res = FALSE;

  g_hash_table_replace (priv->values, g_strdup (key), value);

  g_encoder_value_encoded (encoder, key, value);

  g_encoder_unlock (encoder);

  return res;
}

gboolean
g_encoder_add_key (GEncoder   *encoder,
                   const char *key,
                   GVariant   *value)
{
  g_return_val_if_fail (G_IS_ENCODER (encoder), FALSE);
  g_return_val_if_fail (key != NULL, FALSE);
  g_return_val_if_fail (value != NULL, FALSE);
  g_return_val_if_fail (!g_encoder_is_closed (encoder), FALSE);

  return g_encoder_add_key_value (encoder, key, g_variant_ref (value));
}

/**
 * g_encoder_add_key_data:
 * @encoder: a #GEncoder
 * @key: the key for the data
 * @value: (array length=value_len): the data to store for @key
 * @value_len: the length of the @value array
 *
 * Stores an array of bytes inside @encoder, replacing the current
 * value for @key if necessary.
 *
 * The @encoder makes a copy of the passed @value.
 *
 * Return value: %TRUE if the key was newly added, and %FALSE
 *   if the value was replaced.
 *
 * Since: 2.38
 */
gboolean
g_encoder_add_key_data (GEncoder     *encoder,
                        const char   *key,
                        const guint8 *value,
                        gsize         value_len)
{
  GVariant *ev;

  g_return_val_if_fail (G_IS_ENCODER (encoder), FALSE);
  g_return_val_if_fail (key != NULL, FALSE);
  g_return_val_if_fail (!g_encoder_is_closed (encoder), FALSE);

  ev = g_variant_new_fixed_array (G_VARIANT_TYPE ("y"), value, value_len, sizeof (guint8));
  g_variant_ref_sink (ev);

  return g_encoder_add_key_value (encoder, key, ev);
}

gboolean
g_encoder_add_key_string (GEncoder   *encoder,
                          const char *key,
                          const char *value)
{
  GVariant *ev;

  g_return_val_if_fail (G_IS_ENCODER (encoder), FALSE);
  g_return_val_if_fail (key != NULL, FALSE);
  g_return_val_if_fail (!g_encoder_is_closed (encoder), FALSE);

  ev = g_variant_new_string (value);
  g_variant_ref_sink (ev);

  return g_encoder_add_key_value (encoder, key, ev);
}

gboolean
g_encoder_add_key_int64 (GEncoder   *encoder,
                         const char *key,
                         gint64      value)
{
  GVariant *ev;

  g_return_val_if_fail (G_IS_ENCODER (encoder), FALSE);
  g_return_val_if_fail (key != NULL, FALSE);
  g_return_val_if_fail (!g_encoder_is_closed (encoder), FALSE);

  ev = g_variant_new_int64 (value);
  g_variant_ref_sink (ev);

  return g_encoder_add_key_value (encoder, key, ev);
}

gboolean
g_encoder_add_key_int32 (GEncoder   *encoder,
                         const char *key,
                         int         value)
{
  GVariant *ev;

  g_return_val_if_fail (G_IS_ENCODER (encoder), FALSE);
  g_return_val_if_fail (key != NULL, FALSE);
  g_return_val_if_fail (!g_encoder_is_closed (encoder), FALSE);

  ev = g_variant_new_int32 (value);
  g_variant_ref_sink (ev);

  return g_encoder_add_key_value (encoder, key, ev);
}

gboolean
g_encoder_add_key_double (GEncoder   *encoder,
                          const char *key,
                          double      value)
{
  GVariant *ev;

  g_return_val_if_fail (G_IS_ENCODER (encoder), FALSE);
  g_return_val_if_fail (key != NULL, FALSE);
  g_return_val_if_fail (!g_encoder_is_closed (encoder), FALSE);

  ev = g_variant_new_double (value);
  g_variant_ref_sink (ev);

  return g_encoder_add_key_value (encoder, key, ev);
}

gboolean
g_encoder_add_key_bool (GEncoder   *encoder,
                        const char *key,
                        gboolean    value)
{
  GVariant *ev;

  g_return_val_if_fail (G_IS_ENCODER (encoder), FALSE);
  g_return_val_if_fail (key != NULL, FALSE);
  g_return_val_if_fail (!g_encoder_is_closed (encoder), FALSE);

  ev = g_variant_new_boolean (value);
  g_variant_ref_sink (ev);

  return g_encoder_add_key_value (encoder, key, ev);
}

gboolean
g_encoder_get_key_data (GEncoder    *encoder,
                        const char  *key,
                        guint8     **res,
                        gsize       *res_len)
{
  char *byte_string;
  GVariant *ev;
  gsize len;

  g_encoder_lock (encoder);

  ev = g_hash_table_lookup (G_ENCODER_PRIVATE (encoder)->values, key);
  if (ev == NULL || !g_variant_is_of_type (ev, G_VARIANT_TYPE_BYTESTRING))
    {
      g_encoder_unlock (encoder);

      if (res != NULL)
        *res = NULL;

      if (res_len != NULL)
        *res_len = 0;

      return FALSE;
    }

  byte_string = g_variant_dup_bytestring (ev, &len);
  if (res != NULL)
    *res = (guint8 *) byte_string;
  else
    g_free (byte_string);

  if (res_len != NULL)
    *res_len = len;

  g_encoder_unlock (encoder);

  return TRUE;
}

gboolean
g_encoder_get_key_string (GEncoder    *encoder,
                          const char  *key,
                          char       **res)
{
  const char *str;
  GVariant *ev;
  gsize len;

  g_encoder_lock (encoder);

  ev = g_hash_table_lookup (G_ENCODER_PRIVATE (encoder)->values, key);
  if (ev == NULL || !g_variant_is_of_type (ev, G_VARIANT_TYPE_STRING))
    {
      g_encoder_unlock (encoder);

      if (res != NULL)
        *res = NULL;

      return FALSE;
    }

  str = g_variant_get_string (ev, &len);
  if (res != NULL)
    *res = g_strndup (str, len);

  g_encoder_unlock (encoder);

  return TRUE;
}

gboolean
g_encoder_get_key_int64 (GEncoder   *encoder,
                         const char *key,
                         gint64     *res)
{
  GVariant *ev;

  g_encoder_lock (encoder);

  ev = g_hash_table_lookup (G_ENCODER_PRIVATE (encoder)->values, key);
  if (ev == NULL || !g_variant_is_of_type (ev, G_VARIANT_TYPE_INT64))
    {
      g_encoder_unlock (encoder);
      return FALSE;
    }

  if (res != NULL)
    *res = g_variant_get_int64 (ev);

  g_encoder_unlock (encoder);

  return TRUE;
}

gboolean
g_encoder_get_key_int32 (GEncoder   *encoder,
                         const char *key,
                         gint32     *res)
{
  GVariant *ev;

  g_encoder_lock (encoder);

  ev = g_hash_table_lookup (G_ENCODER_PRIVATE (encoder)->values, key);
  if (ev == NULL || !g_variant_is_of_type (ev, G_VARIANT_TYPE_INT32))
    {
      g_encoder_unlock (encoder);

      if (res != NULL)
        *res = 0;

      return FALSE;
    }

  if (res != NULL)
    *res = g_variant_get_int32 (ev);

  g_encoder_unlock (encoder);

  return TRUE;
}

gboolean
g_encoder_get_key_double (GEncoder   *encoder,
                          const char *key,
                          double     *res)
{
  GVariant *ev;

  g_encoder_lock (encoder);

  ev = g_hash_table_lookup (G_ENCODER_PRIVATE (encoder)->values, key);
  if (ev == NULL || !g_variant_is_of_type (ev, G_VARIANT_TYPE_DOUBLE))
    {
      g_encoder_unlock (encoder);

      if (res != NULL)
        *res = 0;

      return FALSE;
    }

  if (res != NULL)
    *res = g_variant_get_double (ev);

  g_encoder_unlock (encoder);

  return TRUE;
}

gboolean
g_encoder_get_key_bool (GEncoder   *encoder,
                        const char *key,
                        gboolean   *res)
{
  GVariant *ev;

  g_encoder_lock (encoder);

  ev = g_hash_table_lookup (G_ENCODER_PRIVATE (encoder)->values, key);
  if (ev == NULL || !g_variant_is_of_type (ev, G_VARIANT_TYPE_BOOLEAN))
    {
      g_encoder_unlock (encoder);

      if (res != NULL)
        *res = FALSE;

      return FALSE;
    }

  if (res != NULL)
    *res = g_variant_get_boolean (ev);

  g_encoder_unlock (encoder);

  return TRUE;
}

/**
 * g_encoder_has_key:
 * @encoder: a #GEncoder
 * @key: the key to look up
 *
 * Checks whether @key is set inside @encoder.
 *
 * It is usually more performant to call any of the  g_encoder_get_key_*
 * family of functions without checking for the existence of the key
 * beforehand.
 *
 * Return value: %TRUE if the key exists inside @encoder, and %FALSE
 *   otherwise.
 *
 * Since: 2.38
 */
gboolean
g_encoder_has_key (GEncoder   *encoder,
                   const char *key)
{
  gboolean res;

  g_return_val_if_fail (G_IS_ENCODER (encoder), FALSE);
  g_return_val_if_fail (key != NULL, FALSE);

  g_encoder_lock (encoder);

  res = g_hash_table_lookup (G_ENCODER_PRIVATE (encoder)->values, key) != NULL;

  g_encoder_unlock (encoder);

  return res;
}

/**
 * g_encoder_close:
 * @encoder: a #GEncoder
 *
 * Closes a @encoder.
 *
 * It is not possible to add or modify a key in @encoder after calling
 * this function.
 *
 * This function should only be called when writing #GEncoder sub-classes
 * from the implementation of the #GEncoderClass.write_to_bytes() virtual
 * function.
 *
 * Return value: (transfer none): The encoded representation of @encoder,
 *   stored inside a #GVariant with type 'a{sv}'. The returned #GVariant
 *   is owned by the #GEncoder and should not be modified or freed.
 *
 * Since: 2.38
 */
GVariant *
g_encoder_close (GEncoder *encoder)
{
  GVariantBuilder builder;
  GEncoderPrivate *priv;
  GHashTableIter iter;
  gpointer key, value;
  GVariant *res;

  g_return_val_if_fail (G_IS_ENCODER (encoder), NULL);

  priv = G_ENCODER_PRIVATE (encoder);

  g_encoder_lock (encoder);

  priv->flags |= ENCODER_CLOSED;

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a{sv}"));

  if (priv->values == NULL)
    goto out;

  g_hash_table_iter_init (&iter, priv->values);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      GVariant *vkey = g_variant_new_string (key);
      GVariant *vvalue = g_variant_new_variant (value);

      g_variant_builder_open (&builder, G_VARIANT_TYPE ("{sv}"));
      g_variant_builder_add_value (&builder, vkey);
      g_variant_builder_add_value (&builder, vvalue);
      g_variant_builder_close (&builder);
    }

out:
  res = g_variant_builder_end (&builder);
  priv->encoded = g_variant_ref_sink (res);

  g_encoder_unlock (encoder);

  G_ENCODER_GET_CLASS (encoder)->closed (encoder, res);

  return res;
}

/**
 * g_encoder_read_from_bytes:
 * @encoder: a #GEncoder
 * @bytes: a data buffer
 * @error: (allow-none): return location for a #GError, or %NULL
 *
 * Reads the contents of @bytes and decodes them into an @encoder.
 *
 * This function calls the #GEncoderClass.read_from_bytes virtual
 * function implementation for the @encode class.
 *
 * Return value: %TRUE if the @bytes buffer was successfully read,
 *   and %FALSE otherwise.
 *
 * Since: 2.38
 */
gboolean
g_encoder_read_from_bytes (GEncoder  *encoder,
                           GBytes    *bytes,
                           GError   **error)
{
  GEncoderPrivate *priv;

  g_return_val_if_fail (G_IS_ENCODER (encoder), FALSE);
  g_return_val_if_fail (bytes != NULL, FALSE);

  priv = G_ENCODER_PRIVATE (encoder);

  priv->flags &= ~ENCODER_CLOSED;

  if (priv->values != NULL)
    g_hash_table_unref (priv->values);

  priv->values =
    g_hash_table_new_full (g_str_hash, g_str_equal,
                           g_free,
                           (GDestroyNotify) g_variant_unref);

  return G_ENCODER_GET_CLASS (encoder)->read_from_bytes (encoder, bytes, error);
}

/**
 * g_encoder_write_to_bytes:
 * @encode: a #GEncoder
 * @error: (allow-none): return location for a #GError, or %NULL
 *
 * Encodes the contents of @encoder and writes them into a #GBytes
 * buffer.
 *
 * This function calls the #GEncoderClass.write_to_bytes virtual
 * function implementation for the @encode class.
 *
 * Return value: (transfer full): a #GBytes containing the encoded
 *   contents of @encoder, or %NULL
 *
 * Since: 2.38
 */
GBytes *
g_encoder_write_to_bytes (GEncoder  *encoder,
                          GError   **error)
{
  g_return_val_if_fail (G_IS_ENCODER (encoder), FALSE);

  return G_ENCODER_GET_CLASS (encoder)->write_to_bytes (encoder, error);
}
