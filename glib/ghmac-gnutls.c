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

#include <dlfcn.h>
#include <string.h>

#include "ghmac.h"

#include "glib/galloca.h"
#include "gatomic.h"
#include "gslice.h"
#include "gmem.h"
#include "gstrfuncs.h"
#include "gchecksumprivate.h"
#include "gtestutils.h"
#include "gthread.h"
#include "gtypes.h"
#include "glibintl.h"

#ifndef USE_GNUTLS
#error "build configuration error"
#endif

typedef gpointer gnutls_hmac_hd_t;

struct _GHmac
{
  int ref_count;
  GChecksumType digest_type;
  gnutls_hmac_hd_t hmac;
  gchar *digest_str;
};

typedef enum
{
  GNUTLS_MAC_MD5 = 2,
  GNUTLS_MAC_SHA1 = 3,
  GNUTLS_MAC_SHA256 = 6,
  GNUTLS_MAC_SHA384 = 7,
  GNUTLS_MAC_SHA512 = 8,
} gnutls_mac_algorithm_t;

/* Why are we dlopening GnuTLS instead of linking to it directly? Because we
 * want to be able to build GLib as a static library without depending on a
 * static build of GnuTLS. QEMU depends on static linking with GLib, but Fedora
 * does not ship a static build of GnuTLS, and this allows us to avoid changing
 * that.
 */
static int              (*gnutls_hmac_init)   (gnutls_hmac_hd_t *dig, gnutls_mac_algorithm_t algorithm, const void *key, size_t keylen);
static gnutls_hmac_hd_t (*gnutls_hmac_copy)   (gnutls_hmac_hd_t handle);
static void             (*gnutls_hmac_deinit) (gnutls_hmac_hd_t handle, void *digest);
static int              (*gnutls_hmac)        (gnutls_hmac_hd_t handle, const void *ptext, size_t ptext_len);
static void             (*gnutls_hmac_output) (gnutls_hmac_hd_t handle, void *digest);
static const char *     (*gnutls_strerror)    (int error);

static gsize gnutls_initialize_attempted = 0;
static gboolean gnutls_initialize_successful = FALSE;

static void
initialize_gnutls (void)
{
  gpointer libgnutls;

  libgnutls = dlopen ("libgnutls.so.30", RTLD_LAZY | RTLD_GLOBAL);
  if (!libgnutls)
    {
      g_warning ("Cannot use GHmac: failed to load libgnutls.so.30: %s", dlerror ());
      return;
    }

  gnutls_hmac_init = dlsym (libgnutls, "gnutls_hmac_init");
  if (!gnutls_hmac_init)
    {
      g_warning ("Cannot use GHmac: failed to load gnutls_hmac_init: %s", dlerror ());
      return;
    }

  gnutls_hmac_copy = dlsym (libgnutls, "gnutls_hmac_copy");
  if (!gnutls_hmac_copy)
    {
      g_warning ("Cannot use GHmac: failed to load gnutls_hmac_copy: %s", dlerror ());
      return;
    }

  gnutls_hmac_deinit = dlsym (libgnutls, "gnutls_hmac_deinit");
  if (!gnutls_hmac_deinit)
    {
      g_warning ("Cannot use GHmac: failed to load gnutls_hmac_deinit: %s", dlerror ());
      return;
    }

  gnutls_hmac = dlsym (libgnutls, "gnutls_hmac");
  if (!gnutls_hmac)
    {
      g_warning ("Cannot use GHmac: failed to load gnutls_hmac: %s", dlerror ());
      return;
    }

  gnutls_hmac_output = dlsym (libgnutls, "gnutls_hmac_output");
  if (!gnutls_hmac_output)
    {
      g_warning ("Cannot use GHmac: failed to load gnutls_hmac_output: %s", dlerror ());
      return;
    }

  gnutls_strerror = dlsym (libgnutls, "gnutls_strerror");
  if (!gnutls_strerror)
    {
      g_warning ("Cannot use GHmac: failed to load gnutls_strerror: %s", dlerror ());
      return;
    }

  gnutls_initialize_successful = TRUE;
}

GHmac *
g_hmac_new (GChecksumType  digest_type,
            const guchar  *key,
            gsize          key_len)
{
  gnutls_mac_algorithm_t algo;
  GHmac *hmac;
  int ret;

  if (g_once_init_enter (&gnutls_initialize_attempted))
    {
      initialize_gnutls ();
      g_once_init_leave (&gnutls_initialize_attempted, 1);
    }

  if (!gnutls_initialize_successful)
    return NULL;

  hmac = g_new0 (GHmac, 1);
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
