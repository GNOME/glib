/*
 * Copyright © 2010 Codethink Limited
 * Copyright © 2015 Canonical Limited
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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Allison Lortie <desrt@desrt.ca>
 */

#include "config.h"

#include "gsimplepermission.h"
#include "gsettingsbackendinternal.h"
#include "giomodule.h"


#define G_TYPE_MEMORY_SETTINGS_BACKEND (g_memory_settings_backend_get_type ())
G_DECLARE_FINAL_TYPE(GMemorySettingsBackend, g_memory_settings_backend, G, MEMORY_SETTINGS_BACKEND, GSettingsBackend)

struct _GMemorySettingsBackend
{
  GSettingsBackend parent_instance;

  GSettingsBackendChangeset *database;
  GMutex lock;
};

G_DEFINE_TYPE_WITH_CODE (GMemorySettingsBackend,
                         g_memory_settings_backend,
                         G_TYPE_SETTINGS_BACKEND,
                         g_io_extension_point_implement (G_SETTINGS_BACKEND_EXTENSION_POINT_NAME,
                                                         g_define_type_id, "memory", 10))

static GVariant *
g_memory_settings_backend_read_simple (GSettingsBackend   *backend,
                                       const gchar        *key,
                                       const GVariantType *expected_type)
{
  GMemorySettingsBackend *self = G_MEMORY_SETTINGS_BACKEND (backend);
  GVariant *value = NULL;

  g_mutex_lock (&self->lock);

  g_settings_backend_changeset_get (self->database, key, &value);

  g_mutex_unlock (&self->lock);

  return value;
}

static gboolean
g_memory_settings_backend_write (GSettingsBackend *backend,
                                 const gchar      *key,
                                 GVariant         *value,
                                 gpointer          origin_tag)
{
  GMemorySettingsBackend *self = G_MEMORY_SETTINGS_BACKEND (backend);

  g_mutex_lock (&self->lock);

  g_settings_backend_changeset_set (self->database, key, value);

  g_mutex_unlock (&self->lock);

  g_settings_backend_changed (backend, key, origin_tag);

  return TRUE;
}

static gboolean
g_memory_settings_backend_write_changeset (GSettingsBackend          *backend,
                                           GSettingsBackendChangeset *changeset,
                                           gpointer                   origin_tag)
{
  GMemorySettingsBackend *self = G_MEMORY_SETTINGS_BACKEND (backend);

  g_mutex_lock (&self->lock);

  g_settings_backend_changeset_change (self->database, changeset);

  g_mutex_unlock (&self->lock);

  g_settings_backend_changeset_applied (backend, changeset, origin_tag);

  return TRUE;
}

static void
g_memory_settings_backend_finalize (GObject *object)
{
  GMemorySettingsBackend *self = G_MEMORY_SETTINGS_BACKEND (object);

  g_settings_backend_changeset_unref (self->database);
  g_mutex_clear (&self->lock);

  G_OBJECT_CLASS (g_memory_settings_backend_parent_class)->finalize (object);
}

static void
g_memory_settings_backend_init (GMemorySettingsBackend *self)
{
  self->database = g_settings_backend_changeset_new_database (NULL);
  g_mutex_init (&self->lock);
}

static void
g_memory_settings_backend_class_init (GMemorySettingsBackendClass *class)
{
  GSettingsBackendClass *backend_class = G_SETTINGS_BACKEND_CLASS (class);
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  backend_class->read_simple = g_memory_settings_backend_read_simple;
  backend_class->write = g_memory_settings_backend_write;
  backend_class->write_changeset = g_memory_settings_backend_write_changeset;
  object_class->finalize = g_memory_settings_backend_finalize;
}

/**
 * g_memory_settings_backend_new:
 *
 * Creates a memory-backed #GSettingsBackend.
 *
 * This backend allows changes to settings, but does not write them
 * to any backing storage, so the next time you run your application,
 * the memory backend will start out with the default values again.
 *
 * Returns: (transfer full): a newly created #GSettingsBackend
 *
 * Since: 2.28
 */
GSettingsBackend *
g_memory_settings_backend_new (void)
{
  return g_object_new (G_TYPE_MEMORY_SETTINGS_BACKEND, NULL);
}
