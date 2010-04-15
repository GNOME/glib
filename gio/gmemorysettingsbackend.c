/*
 * Copyright © 2010 Codethink Limited
 * Copyright © 2010 Novell, Inc.
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
 * Authors: Vincent Untz <vuntz@gnome.org>
 *          Ryan Lortie <desrt@desrt.ca>
 */

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "giomodule.h"
#include "gfile.h"
#include "gfilemonitor.h"

#include "gmemorysettingsbackend.h"

G_DEFINE_TYPE_WITH_CODE (GMemorySettingsBackend,
                         g_memory_settings_backend,
                         G_TYPE_SETTINGS_BACKEND,
                         g_io_extension_point_implement (G_SETTINGS_BACKEND_EXTENSION_POINT_NAME,
                                                         g_define_type_id, "memory", 0))

struct _GMemorySettingsBackendPrivate
{
  GHashTable   *table;
  GKeyFile     *keyfile;
  gchar        *keyfile_path;
  gchar        *keyfile_checksum;
  GFileMonitor *keyfile_monitor;
};

static GVariant *
g_memory_settings_backend_read (GSettingsBackend   *backend,
                                const gchar        *key,
                                const GVariantType *expected_type)
{
  GVariant *value;

  GMemorySettingsBackend *memory = G_MEMORY_SETTINGS_BACKEND (backend);

  value = g_hash_table_lookup (memory->priv->table, key);

  if (value != NULL)
    g_variant_ref (value);

  return value;
}

static gboolean
g_memory_settings_backend_write_one (const gchar            *key,
                                     GVariant               *value,
                                     GMemorySettingsBackend *memory)
{
  const gchar *slash;
  const gchar *base_key;
  gchar       *path;

  g_hash_table_replace (memory->priv->table,
                        g_strdup (key), g_variant_ref (value));

  slash = strrchr (key, '/');
  g_assert (slash != NULL);
  base_key = (slash + 1);
  path = g_strndup (key, slash - key + 1);

  g_key_file_set_string (memory->priv->keyfile,
                         path, base_key, g_variant_print (value, TRUE));

  g_free (path);

  return FALSE;
}

static void
g_memory_settings_backend_keyfile_write (GMemorySettingsBackend *memory)
{
  gchar *dirname;
  gchar *contents;
  gsize  length;
  GFile *file;

  dirname = g_path_get_dirname (memory->priv->keyfile_path);
  if (!g_file_test (dirname, G_FILE_TEST_IS_DIR))
    g_mkdir_with_parents (dirname, 0700);
  g_free (dirname);

  contents = g_key_file_to_data (memory->priv->keyfile, &length, NULL);

  file = g_file_new_for_path (memory->priv->keyfile_path);
  g_file_replace_contents (file, contents, length,
                           NULL, FALSE, G_FILE_CREATE_REPLACE_DESTINATION,
                           NULL, NULL, NULL);
  g_object_unref (file);

  g_free (memory->priv->keyfile_checksum);
  memory->priv->keyfile_checksum = g_compute_checksum_for_string (G_CHECKSUM_SHA256, contents, length);

  g_free (contents);
}

static gboolean
g_memory_settings_backend_write (GSettingsBackend *backend,
                                 const gchar      *key,
                                 GVariant         *value,
                                 gpointer          origin_tag)
{
  GMemorySettingsBackend *memory = G_MEMORY_SETTINGS_BACKEND (backend);

  g_memory_settings_backend_write_one (key, value, memory);
  g_memory_settings_backend_keyfile_write (memory);

  g_settings_backend_changed (backend, key, origin_tag);

  return TRUE;
}

static gboolean
g_memory_settings_backend_write_keys (GSettingsBackend *backend,
                                      GTree            *tree,
                                      gpointer          origin_tag)
{
  GMemorySettingsBackend *memory = G_MEMORY_SETTINGS_BACKEND (backend);

  g_tree_foreach (tree, (GTraverseFunc) g_memory_settings_backend_write_one, backend);
  g_memory_settings_backend_keyfile_write (memory);

  g_settings_backend_changed_tree (backend, tree, origin_tag);

  return TRUE;
}

static void
g_memory_settings_backend_reset_path (GSettingsBackend *backend,
                                      const gchar      *path,
                                      gpointer         origin_tag)
{
  GMemorySettingsBackend *memory = G_MEMORY_SETTINGS_BACKEND (backend);
  GPtrArray  *reset_array;
  GList      *hash_keys;
  GList      *l;
  gboolean    changed;
  gchar     **groups = NULL;
  gsize       groups_nb = 0;
  int         i;

  reset_array = g_ptr_array_new_with_free_func (g_free);

  hash_keys = g_hash_table_get_keys (memory->priv->table);
  for (l = hash_keys; l != NULL; l = l->next)
    {
      if (g_str_has_prefix (l->data, path))
        {
          g_hash_table_remove (memory->priv->table, l->data);
          g_ptr_array_add (reset_array, g_strdup (l->data));
        }
    }
  g_list_free (hash_keys);

  changed = FALSE;
  groups = g_key_file_get_groups (memory->priv->keyfile, &groups_nb);
  for (i = 0; i < groups_nb; i++)
    {
      if (g_str_has_prefix (groups[i], path))
        changed = g_key_file_remove_group (memory->priv->keyfile, groups[i], NULL) || changed;
    }
  g_strfreev (groups);

  if (changed)
    g_memory_settings_backend_keyfile_write (memory);

  if (reset_array->len > 0)
    {
      /* the array has to be NULL-terminated */
      g_ptr_array_add (reset_array, NULL);
      g_settings_backend_keys_changed (G_SETTINGS_BACKEND (memory),
                                       "",
                                       (const gchar **) reset_array->pdata,
                                       origin_tag);
    }

  g_ptr_array_free (reset_array, TRUE);
}

static void
g_memory_settings_backend_reset (GSettingsBackend *backend,
                                 const gchar      *key,
                                 gpointer          origin_tag)
{
  GMemorySettingsBackend *memory = G_MEMORY_SETTINGS_BACKEND (backend);
  gboolean     had_key;
  const gchar *slash;
  const gchar *base_key;
  gchar       *path;

  had_key = g_hash_table_lookup_extended (memory->priv->table, key, NULL, NULL);
  if (had_key)
    g_hash_table_remove (memory->priv->table, key);

  slash = strrchr (key, '/');
  g_assert (slash != NULL);
  base_key = (slash + 1);
  path = g_strndup (key, slash - key + 1);

  if (g_key_file_remove_key (memory->priv->keyfile, path, base_key, NULL))
    g_memory_settings_backend_keyfile_write (memory);

  g_free (path);

  if (had_key)
    g_settings_backend_changed (G_SETTINGS_BACKEND (memory), key, origin_tag);
}

static gboolean
g_memory_settings_backend_get_writable (GSettingsBackend *backend,
                                        const gchar      *name)
{
  return TRUE;
}

static gboolean
g_memory_settings_backend_supports_context (const gchar *context)
{
  return TRUE;
}

static void
g_memory_settings_backend_keyfile_reload (GMemorySettingsBackend *memory)
{
  gchar       *contents = NULL;
  gsize        length = 0;
  gchar       *new_checksum;
  GHashTable  *loaded_keys;
  GPtrArray   *changed_array;
  gchar      **groups = NULL;
  gsize        groups_nb = 0;
  int          i;
  GList       *keys_l = NULL;
  GList       *l;

  if (!g_file_get_contents (memory->priv->keyfile_path,
                            &contents, &length, NULL))
    {
      contents = g_strdup ("");
      length = 0;
    }

  new_checksum = g_compute_checksum_for_string (G_CHECKSUM_SHA256, contents, length);

  if (g_strcmp0 (memory->priv->keyfile_checksum, new_checksum) == 0)
    {
      g_free (new_checksum);
      return;
    }

  if (memory->priv->keyfile_checksum != NULL)
    g_free (memory->priv->keyfile_checksum);
  memory->priv->keyfile_checksum = new_checksum;

  if (memory->priv->keyfile != NULL)
    g_key_file_free (memory->priv->keyfile);

  memory->priv->keyfile = g_key_file_new ();

  /* we just silently ignore errors: there's not much we can do about them */
  if (length > 0)
    g_key_file_load_from_data (memory->priv->keyfile, contents, length,
                               G_KEY_FILE_KEEP_COMMENTS|G_KEY_FILE_KEEP_TRANSLATIONS, NULL);

  loaded_keys = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  changed_array = g_ptr_array_new_with_free_func (g_free);

  /* Load keys from the keyfile */
  groups = g_key_file_get_groups (memory->priv->keyfile, &groups_nb);
  for (i = 0; i < groups_nb; i++)
    {
      gchar **keys = NULL;
      gsize   keys_nb = 0;
      int     j;

      keys = g_key_file_get_keys (memory->priv->keyfile, groups[i], &keys_nb, NULL);
      if (keys == NULL)
        continue;

      for (j = 0; j < keys_nb; j++)
        {
          gchar    *value;
          gchar    *full_key;
          GVariant *old_variant;
          GVariant *variant;

          value = g_key_file_get_string (memory->priv->keyfile, groups[i], keys[j], NULL);
          if (value == NULL)
            continue;

          variant = g_variant_new_parsed (value);
          g_free (value);

          if (variant == NULL)
            continue;

          full_key = g_strjoin ("", groups[i], keys[j], NULL),
          g_hash_table_insert (loaded_keys, full_key, GINT_TO_POINTER(TRUE));

          old_variant = g_hash_table_lookup (memory->priv->table, full_key);

          if (old_variant == NULL || !g_variant_equal (old_variant, variant))
            {
              g_ptr_array_add (changed_array, g_strdup (full_key));
              g_hash_table_replace (memory->priv->table,
                                    g_strdup (full_key),
                                    g_variant_ref_sink (variant));
            }
          else
            g_variant_unref (variant);
        }

      g_strfreev (keys);
    }
  g_strfreev (groups);

  /* Remove keys that were in the hashtable but not in the keyfile */
  keys_l = g_hash_table_get_keys (memory->priv->table);
  for (l = keys_l; l != NULL; l = l->next)
    {
      gchar *key = l->data;

      if (g_hash_table_lookup_extended (loaded_keys, key, NULL, NULL))
        continue;

      g_ptr_array_add (changed_array, g_strdup (key));
      g_hash_table_remove (memory->priv->table, key);
    }
  g_list_free (keys_l);

  if (changed_array->len > 0)
    {
      /* the array has to be NULL-terminated */
      g_ptr_array_add (changed_array, NULL);
      g_settings_backend_keys_changed (G_SETTINGS_BACKEND (memory),
                                       "",
                                       (const gchar **) changed_array->pdata,
                                       NULL);
    }

  g_hash_table_unref (loaded_keys);
  g_ptr_array_free (changed_array, TRUE);
}

static void
g_memory_settings_backend_keyfile_changed (GFileMonitor      *monitor,
                                           GFile             *file,
                                           GFile             *other_file,
                                           GFileMonitorEvent  event_type,
                                           gpointer           user_data)
{
  GMemorySettingsBackend *memory;

  if (event_type != G_FILE_MONITOR_EVENT_CHANGED &&
      event_type != G_FILE_MONITOR_EVENT_CREATED &&
      event_type != G_FILE_MONITOR_EVENT_DELETED)
    return;

  memory = G_MEMORY_SETTINGS_BACKEND (user_data);
  g_memory_settings_backend_keyfile_reload (memory);
}

static void
g_memory_settings_backend_finalize (GObject *object)
{
  GMemorySettingsBackend *memory = G_MEMORY_SETTINGS_BACKEND (object);

  g_hash_table_unref (memory->priv->table);
  memory->priv->table = NULL;

  g_key_file_free (memory->priv->keyfile);
  memory->priv->keyfile = NULL;

  g_free (memory->priv->keyfile_path);
  memory->priv->keyfile_path = NULL;

  g_free (memory->priv->keyfile_checksum);
  memory->priv->keyfile_checksum = NULL;

  g_file_monitor_cancel (memory->priv->keyfile_monitor);
  g_object_unref (memory->priv->keyfile_monitor);
  memory->priv->keyfile_monitor = NULL;

  G_OBJECT_CLASS (g_memory_settings_backend_parent_class)
    ->finalize (object);
}

static void
g_memory_settings_backend_constructed (GObject *object)
{
  GMemorySettingsBackend *memory = G_MEMORY_SETTINGS_BACKEND (object);
  const gchar            *envvar_path;
  gchar                  *path;
  gchar                  *context;
  GFile                  *file;

  envvar_path = g_getenv ("GSETTINGS_MEMORY_BACKEND_STORE");
  if (envvar_path != NULL && envvar_path[0] != '\0')
    path = g_strdup (envvar_path);
  else
    path = g_build_filename (g_get_user_config_dir (),
                             "gsettings", "store", NULL);

  context = NULL;
  g_object_get (memory, "context", &context, NULL);
  if (context && context[0] != '\0')
    {
      memory->priv->keyfile_path = g_strdup_printf ("%s.%s", path, context);
      g_free (context);
      g_free (path);
    }
  else
    memory->priv->keyfile_path = path;

  file = g_file_new_for_path (memory->priv->keyfile_path);
  memory->priv->keyfile_monitor = g_file_monitor_file (file, G_FILE_MONITOR_SEND_MOVED, NULL, NULL);
  g_signal_connect (memory->priv->keyfile_monitor, "changed",
                    (GCallback)g_memory_settings_backend_keyfile_changed, memory);
  g_object_unref (file);

  g_memory_settings_backend_keyfile_reload (memory);
}

static void
g_memory_settings_backend_init (GMemorySettingsBackend *memory)
{
  memory->priv = G_TYPE_INSTANCE_GET_PRIVATE (memory,
                                              G_TYPE_MEMORY_SETTINGS_BACKEND,
                                              GMemorySettingsBackendPrivate);
  memory->priv->table =
    g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
                           (GDestroyNotify) g_variant_unref);

  memory->priv->keyfile = NULL;
  memory->priv->keyfile_path = NULL;
  memory->priv->keyfile_checksum = NULL;
  memory->priv->keyfile_monitor = NULL;
}

static void
g_memory_settings_backend_class_init (GMemorySettingsBackendClass *class)
{
  GSettingsBackendClass *backend_class = G_SETTINGS_BACKEND_CLASS (class);
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->constructed = g_memory_settings_backend_constructed;
  object_class->finalize = g_memory_settings_backend_finalize;

  backend_class->read = g_memory_settings_backend_read;
  backend_class->write = g_memory_settings_backend_write;
  backend_class->write_keys = g_memory_settings_backend_write_keys;
  backend_class->reset = g_memory_settings_backend_reset;
  backend_class->reset_path = g_memory_settings_backend_reset_path;
  backend_class->get_writable = g_memory_settings_backend_get_writable;
  /* No need to implement subscribed/unsubscribe: the only point would be to
   * stop monitoring the file when there's no GSettings anymore, which is no
   * big win. */
  backend_class->supports_context = g_memory_settings_backend_supports_context;

  g_type_class_add_private (class, sizeof (GMemorySettingsBackendPrivate));
}
