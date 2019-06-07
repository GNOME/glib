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
#include <gnutls/crypto.h>

#include "ghmac.h"

#include "glib/galloca.h"
#include "gatomic.h"
#include "gslice.h"
#include "gmem.h"
#include "gstrfuncs.h"
#include "gchecksumprivate.h"
#include "gtestutils.h"
#include "gtypes.h"
#include "glibintl.h"

#ifndef HAVE_GNUTLS
#error "build configuration error"
#endif

struct _GHmac
{
  int ref_count;
  GChecksumType digest_type;
  gnutls_hmac_hd_t hmac;
  gchar *digest_str;
};

GHmac *
g_hmac_new (GChecksumType  digest_type,
            const guchar  *key,
            gsize          key_len)
{
  gnutls_mac_algorithm_t algo;
  GHmac *hmac = g_new0 (GHmac, 1);
  int ret;

  hmac->ref_count = 1;
  hmac->digest_type = digest_type;

  switch (digest_type)
    {
    case G_CHECKSUM_MD5:
      algo = GNUTLS_MAC_MD5;
      break;
    case G_CHECKSUM_SHA1:
      algo = GNUTLS_MAC_SHA1;
      break;
    case G_CHECKSUM_SHA256:
      algo = GNUTLS_MAC_SHA256;
      break;
    case G_CHECKSUM_SHA384:
      algo = GNUTLS_MAC_SHA384;
      break;
    case G_CHECKSUM_SHA512:
      algo = GNUTLS_MAC_SHA512;
      break;
    default:
      g_free (hmac);
      g_return_val_if_reached (NULL);
    }

  ret = gnutls_hmac_init (&hmac->hmac, algo, key, key_len);
  if (ret != 0)
    {
      /* There is no way to report an error here, but one possible cause of
       * failure is that the requested digest may be disabled by FIPS mode.
       */
      g_free (hmac);
      return NULL;
    }

  return hmac;
}

GHmac *
g_hmac_copy (const GHmac *hmac)
{
  GHmac *copy;

  g_return_val_if_fail (hmac != NULL, NULL);

  copy = g_new0 (GHmac, 1);
  copy->ref_count = 1;
  copy->digest_type = hmac->digest_type;
  copy->hmac = gnutls_hmac_copy (hmac->hmac);

  /* g_hmac_copy is not allowed to fail, so we'll have to crash on error. */
  if (!copy->hmac)
    g_error ("gnutls_hmac_copy failed");

  return copy;
}

GHmac *
g_hmac_ref (GHmac *hmac)
{
  g_return_val_if_fail (hmac != NULL, NULL);

  g_atomic_int_inc (&hmac->ref_count);

  return hmac;
}

void
g_hmac_unref (GHmac *hmac)
{
  g_return_if_fail (hmac != NULL);

  if (g_atomic_int_dec_and_test (&hmac->ref_count))
    {
      gnutls_hmac_deinit (hmac->hmac, NULL);
      g_free (hmac->digest_str);
      g_free (hmac);
    }
}


void
g_hmac_update (GHmac        *hmac,
               const guchar *data,
               gssize        length)
{
  int ret;

  g_return_if_fail (hmac != NULL);
  g_return_if_fail (length == 0 || data != NULL);

  if (length == -1)
    length = strlen ((const char *)data);

  /* g_hmac_update is not allowed to fail, so we'll have to crash on error. */
  ret = gnutls_hmac (hmac->hmac, data, length);
  if (ret != 0)
    g_error ("gnutls_hmac failed: %s", gnutls_strerror (ret));
}

const gchar *
g_hmac_get_string (GHmac *hmac)
{
  guint8 *buffer;
  gsize digest_len;

  g_return_val_if_fail (hmac != NULL, NULL);

  if (hmac->digest_str)
    return hmac->digest_str;

  digest_len = g_checksum_type_get_length (hmac->digest_type);
  buffer = g_alloca (digest_len);

  gnutls_hmac_output (hmac->hmac, buffer);
  hmac->digest_str = gchecksum_digest_to_string (buffer, digest_len);
  return hmac->digest_str;
}


void
g_hmac_get_digest (GHmac  *hmac,
                   guint8 *buffer,
                   gsize  *digest_len)
{
  g_return_if_fail (hmac != NULL);

  gnutls_hmac_output (hmac->hmac, buffer);
  *digest_len = g_checksum_type_get_length (hmac->digest_type);
}
