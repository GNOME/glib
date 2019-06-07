/* ghmac.h - data hashing functions
 *
 * Copyright (C) 2011  Collabora Ltd.
 * Copyright (C) 2019  Red Hat, Inc.
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
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <string.h>

#include "ghmac.h"

#include "glib/galloca.h"
#include "gatomic.h"
#include "gslice.h"
#include "gmem.h"
#include "gstrfuncs.h"
#include "gtestutils.h"
#include "gtypes.h"
#include "glibintl.h"

/**
 * g_compute_hmac_for_data:
 * @digest_type: a #GChecksumType to use for the HMAC
 * @key: (array length=key_len): the key to use in the HMAC
 * @key_len: the length of the key
 * @data: (array length=length): binary blob to compute the HMAC of
 * @length: length of @data
 *
 * Computes the HMAC for a binary @data of @length. This is a
 * convenience wrapper for g_hmac_new(), g_hmac_get_string()
 * and g_hmac_unref().
 *
 * The hexadecimal string returned will be in lower case.
 *
 * Returns: the HMAC of the binary data as a string in hexadecimal.
 *   The returned string should be freed with g_free() when done using it.
 *
 * Since: 2.30
 */
gchar *
g_compute_hmac_for_data (GChecksumType  digest_type,
                         const guchar  *key,
                         gsize          key_len,
                         const guchar  *data,
                         gsize          length)
{
  GHmac *hmac;
  gchar *retval;

  g_return_val_if_fail (length == 0 || data != NULL, NULL);

  hmac = g_hmac_new (digest_type, key, key_len);
  if (!hmac)
    return NULL;

  g_hmac_update (hmac, data, length);
  retval = g_strdup (g_hmac_get_string (hmac));
  g_hmac_unref (hmac);

  return retval;
}

/**
 * g_compute_hmac_for_bytes:
 * @digest_type: a #GChecksumType to use for the HMAC
 * @key: the key to use in the HMAC
 * @data: binary blob to compute the HMAC of
 *
 * Computes the HMAC for a binary @data. This is a
 * convenience wrapper for g_hmac_new(), g_hmac_get_string()
 * and g_hmac_unref().
 *
 * The hexadecimal string returned will be in lower case.
 *
 * Returns: the HMAC of the binary data as a string in hexadecimal.
 *   The returned string should be freed with g_free() when done using it.
 *
 * Since: 2.50
 */
gchar *
g_compute_hmac_for_bytes (GChecksumType  digest_type,
                          GBytes        *key,
                          GBytes        *data)
{
  gconstpointer byte_data;
  gsize length;
  gconstpointer key_data;
  gsize key_len;

  g_return_val_if_fail (data != NULL, NULL);
  g_return_val_if_fail (key != NULL, NULL);

  byte_data = g_bytes_get_data (data, &length);
  key_data = g_bytes_get_data (key, &key_len);
  return g_compute_hmac_for_data (digest_type, key_data, key_len, byte_data, length);
}


/**
 * g_compute_hmac_for_string:
 * @digest_type: a #GChecksumType to use for the HMAC
 * @key: (array length=key_len): the key to use in the HMAC
 * @key_len: the length of the key
 * @str: the string to compute the HMAC for
 * @length: the length of the string, or -1 if the string is nul-terminated
 *
 * Computes the HMAC for a string.
 *
 * The hexadecimal string returned will be in lower case.
 *
 * Returns: the HMAC as a hexadecimal string.
 *     The returned string should be freed with g_free()
 *     when done using it.
 *
 * Since: 2.30
 */
gchar *
g_compute_hmac_for_string (GChecksumType  digest_type,
                           const guchar  *key,
                           gsize          key_len,
                           const gchar   *str,
                           gssize         length)
{
  g_return_val_if_fail (length == 0 || str != NULL, NULL);

  if (length < 0)
    length = strlen (str);

  return g_compute_hmac_for_data (digest_type, key, key_len,
                                  (const guchar *) str, length);
}
