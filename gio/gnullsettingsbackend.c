/*
 * Copyright © 2010 Codethink Limited
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the licence, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Ryan Lortie <desrt@desrt.ca>
 */

#include "config.h"

#include "gnullsettingsbackend.h"

#define G_TYPE_NULL_SETTINGS_BACKEND    (g_null_settings_backend_get_type ())
#define G_NULL_SETTINGS_BACKEND(inst)   (G_TYPE_CHECK_INSTANCE_CAST ((inst), \
                                         G_TYPE_NULL_SETTINGS_BACKEND,       \
                                         GNullSettingsBackend))


typedef GSettingsBackendClass GNullSettingsBackendClass;
typedef GSettingsBackend      GNullSettingsBackend;

G_DEFINE_TYPE (GNullSettingsBackend,
               g_null_settings_backend,
               G_TYPE_SETTINGS_BACKEND)

static GVariant *
g_null_settings_backend_read (GSettingsBackend   *backend,
                              const gchar        *key,
                              const GVariantType *expected_type)
{
  return NULL;
}

static gboolean
g_null_settings_backend_write (GSettingsBackend *backend,
                               const gchar      *key,
                               GVariant         *value,
                               gpointer          origin_tag)
{
  return FALSE;
}

static gboolean
g_null_settings_backend_write_keys (GSettingsBackend *backend,
                                    GTree            *tree,
                                    gpointer          origin_tag)
{
  return FALSE;
}

static void
g_null_settings_backend_reset (GSettingsBackend *backend,
                               const gchar      *key,
                               gpointer          origin_tag)
{
}

static void
g_null_settings_backend_reset_path (GSettingsBackend *backend,
                                    const gchar      *path,
                                    gpointer          origin_tag)
{
}

static gboolean
g_null_settings_backend_get_writable (GSettingsBackend *backend,
                                        const gchar      *name)
{
  return FALSE;
}

static void
g_null_settings_backend_init (GNullSettingsBackend *memory)
{
}

static void
g_null_settings_backend_class_init (GNullSettingsBackendClass *class)
{
  GSettingsBackendClass *backend_class = G_SETTINGS_BACKEND_CLASS (class);

  backend_class->read = g_null_settings_backend_read;
  backend_class->write = g_null_settings_backend_write;
  backend_class->write_keys = g_null_settings_backend_write_keys;
  backend_class->reset = g_null_settings_backend_reset;
  backend_class->reset_path = g_null_settings_backend_reset_path;
  backend_class->get_writable = g_null_settings_backend_get_writable;
}

GSettingsBackend *
g_null_settings_backend_new (void)
{
  return g_object_new (G_TYPE_NULL_SETTINGS_BACKEND, NULL);
}
