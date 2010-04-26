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

#include "gkeyfilesettingsbackend.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "gioerror.h"
#include "giomodule.h"
#include "gfile.h"
#include "gfileinfo.h"
#include "gfilemonitor.h"

#include "gioalias.h"

G_DEFINE_TYPE (GKeyfileSettingsBackend,
               g_keyfile_settings_backend,
               G_TYPE_SETTINGS_BACKEND)

struct _GKeyfileSettingsBackendPrivate
{
  GHashTable   *table;
  GKeyFile     *keyfile;
  gboolean      writable;
  gchar        *file_path;
  gchar        *checksum;
  GFileMonitor *monitor;
};

static GVariant *
g_keyfile_settings_backend_read (GSettingsBackend   *backend,
                                 const gchar        *key,
                                 const GVariantType *expected_type,
                                 gboolean            default_value)
{
  GKeyfileSettingsBackend *kf_backend = G_KEYFILE_SETTINGS_BACKEND (backend);
  GVariant *value;

  if (default_value)
    return NULL;

  value = g_hash_table_lookup (kf_backend->priv->table, key);

  if (value != NULL)
    g_variant_ref (value);

  return value;
}

static gboolean
g_keyfile_settings_backend_write_one (const gchar             *key,
                                      GVariant                *value,
                                      GKeyfileSettingsBackend *kf_backend)
{
  const gchar *slash;
  const gchar *base_key;
  gchar       *path;

  g_hash_table_replace (kf_backend->priv->table,
                        g_strdup (key), g_variant_ref (value));

  slash = strrchr (key, '/');
  g_assert (slash != NULL);
  base_key = (slash + 1);
  path = g_strndup (key, slash - key + 1);

  g_key_file_set_string (kf_backend->priv->keyfile,
                         path, base_key, g_variant_print (value, TRUE));

  g_free (path);

  return FALSE;
}

static void
g_keyfile_settings_backend_keyfile_write (GKeyfileSettingsBackend *kf_backend)
{
  gchar *dirname;
  gchar *contents;
  gsize  length;
  GFile *file;

  dirname = g_path_get_dirname (kf_backend->priv->file_path);
  if (!g_file_test (dirname, G_FILE_TEST_IS_DIR))
    g_mkdir_with_parents (dirname, 0700);
  g_free (dirname);

  contents = g_key_file_to_data (kf_backend->priv->keyfile, &length, NULL);

  file = g_file_new_for_path (kf_backend->priv->file_path);
  g_file_replace_contents (file, contents, length,
                           NULL, FALSE, G_FILE_CREATE_REPLACE_DESTINATION,
                           NULL, NULL, NULL);
  g_object_unref (file);

  g_free (kf_backend->priv->checksum);
  kf_backend->priv->checksum = g_compute_checksum_for_string (G_CHECKSUM_SHA256, contents, length);

  g_free (contents);
}

static gboolean
g_keyfile_settings_backend_write (GSettingsBackend *backend,
                                  const gchar      *key,
                                  GVariant         *value,
                                  gpointer          origin_tag)
{
  GKeyfileSettingsBackend *kf_backend = G_KEYFILE_SETTINGS_BACKEND (backend);

  g_keyfile_settings_backend_write_one (key, value, kf_backend);
  g_keyfile_settings_backend_keyfile_write (kf_backend);

  g_settings_backend_changed (backend, key, origin_tag);

  return TRUE;
}

static gboolean
g_keyfile_settings_backend_write_keys (GSettingsBackend *backend,
                                       GTree            *tree,
                                       gpointer          origin_tag)
{
  GKeyfileSettingsBackend *kf_backend = G_KEYFILE_SETTINGS_BACKEND (backend);

  g_tree_foreach (tree, (GTraverseFunc) g_keyfile_settings_backend_write_one, backend);
  g_keyfile_settings_backend_keyfile_write (kf_backend);

  g_settings_backend_changed_tree (backend, tree, origin_tag);

  return TRUE;
}

static void
g_keyfile_settings_backend_reset_path (GSettingsBackend *backend,
                                       const gchar      *path,
                                       gpointer         origin_tag)
{
  GKeyfileSettingsBackend *kf_backend = G_KEYFILE_SETTINGS_BACKEND (backend);
  GPtrArray  *reset_array;
  GList      *hash_keys;
  GList      *l;
  gboolean    changed;
  gchar     **groups = NULL;
  gsize       groups_nb = 0;
  int         i;

  reset_array = g_ptr_array_new_with_free_func (g_free);

  hash_keys = g_hash_table_get_keys (kf_backend->priv->table);
  for (l = hash_keys; l != NULL; l = l->next)
    {
      if (g_str_has_prefix (l->data, path))
        {
          g_hash_table_remove (kf_backend->priv->table, l->data);
          g_ptr_array_add (reset_array, g_strdup (l->data));
        }
    }
  g_list_free (hash_keys);

  changed = FALSE;
  groups = g_key_file_get_groups (kf_backend->priv->keyfile, &groups_nb);
  for (i = 0; i < groups_nb; i++)
    {
      if (g_str_has_prefix (groups[i], path))
        changed = g_key_file_remove_group (kf_backend->priv->keyfile, groups[i], NULL) || changed;
    }
  g_strfreev (groups);

  if (changed)
    g_keyfile_settings_backend_keyfile_write (kf_backend);

  if (reset_array->len > 0)
    {
      /* the array has to be NULL-terminated */
      g_ptr_array_add (reset_array, NULL);
      g_settings_backend_keys_changed (G_SETTINGS_BACKEND (kf_backend),
                                       "",
                                       (const gchar **) reset_array->pdata,
                                       origin_tag);
    }

  g_ptr_array_free (reset_array, TRUE);
}

static void
g_keyfile_settings_backend_reset (GSettingsBackend *backend,
                                  const gchar      *key,
                                  gpointer          origin_tag)
{
  GKeyfileSettingsBackend *kf_backend = G_KEYFILE_SETTINGS_BACKEND (backend);
  gboolean     had_key;
  const gchar *slash;
  const gchar *base_key;
  gchar       *path;

  had_key = g_hash_table_lookup_extended (kf_backend->priv->table, key, NULL, NULL);
  if (had_key)
    g_hash_table_remove (kf_backend->priv->table, key);

  slash = strrchr (key, '/');
  g_assert (slash != NULL);
  base_key = (slash + 1);
  path = g_strndup (key, slash - key + 1);

  if (g_key_file_remove_key (kf_backend->priv->keyfile, path, base_key, NULL))
    g_keyfile_settings_backend_keyfile_write (kf_backend);

  g_free (path);

  if (had_key)
    g_settings_backend_changed (G_SETTINGS_BACKEND (kf_backend), key, origin_tag);
}

static gboolean
g_keyfile_settings_backend_get_writable (GSettingsBackend *backend,
                                         const gchar      *name)
{
  GKeyfileSettingsBackend *kf_backend = G_KEYFILE_SETTINGS_BACKEND (backend);

  return kf_backend->priv->writable;
}

static void
g_keyfile_settings_backend_keyfile_reload (GKeyfileSettingsBackend *kf_backend)
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

  if (!g_file_get_contents (kf_backend->priv->file_path,
                            &contents, &length, NULL))
    {
      contents = g_strdup ("");
      length = 0;
    }

  new_checksum = g_compute_checksum_for_string (G_CHECKSUM_SHA256, contents, length);

  if (g_strcmp0 (kf_backend->priv->checksum, new_checksum) == 0)
    {
      g_free (new_checksum);
      return;
    }

  if (kf_backend->priv->checksum != NULL)
    g_free (kf_backend->priv->checksum);
  kf_backend->priv->checksum = new_checksum;

  if (kf_backend->priv->keyfile != NULL)
    g_key_file_free (kf_backend->priv->keyfile);

  kf_backend->priv->keyfile = g_key_file_new ();

  /* we just silently ignore errors: there's not much we can do about them */
  if (length > 0)
    g_key_file_load_from_data (kf_backend->priv->keyfile, contents, length,
                               G_KEY_FILE_KEEP_COMMENTS|G_KEY_FILE_KEEP_TRANSLATIONS, NULL);

  loaded_keys = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  changed_array = g_ptr_array_new_with_free_func (g_free);

  /* Load keys from the keyfile */
  groups = g_key_file_get_groups (kf_backend->priv->keyfile, &groups_nb);
  for (i = 0; i < groups_nb; i++)
    {
      gchar **keys = NULL;
      gsize   keys_nb = 0;
      int     j;

      keys = g_key_file_get_keys (kf_backend->priv->keyfile, groups[i], &keys_nb, NULL);
      if (keys == NULL)
        continue;

      for (j = 0; j < keys_nb; j++)
        {
          gchar    *value;
          gchar    *full_key;
          GVariant *old_variant;
          GVariant *variant;

          value = g_key_file_get_string (kf_backend->priv->keyfile, groups[i], keys[j], NULL);
          if (value == NULL)
            continue;

          variant = g_variant_new_parsed (value);
          g_free (value);

          if (variant == NULL)
            continue;

          full_key = g_strjoin ("", groups[i], keys[j], NULL),
          g_hash_table_insert (loaded_keys, full_key, GINT_TO_POINTER(TRUE));

          old_variant = g_hash_table_lookup (kf_backend->priv->table, full_key);

          if (old_variant == NULL || !g_variant_equal (old_variant, variant))
            {
              g_ptr_array_add (changed_array, g_strdup (full_key));
              g_hash_table_replace (kf_backend->priv->table,
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
  keys_l = g_hash_table_get_keys (kf_backend->priv->table);
  for (l = keys_l; l != NULL; l = l->next)
    {
      gchar *key = l->data;

      if (g_hash_table_lookup_extended (loaded_keys, key, NULL, NULL))
        continue;

      g_ptr_array_add (changed_array, g_strdup (key));
      g_hash_table_remove (kf_backend->priv->table, key);
    }
  g_list_free (keys_l);

  if (changed_array->len > 0)
    {
      /* the array has to be NULL-terminated */
      g_ptr_array_add (changed_array, NULL);
      g_settings_backend_keys_changed (G_SETTINGS_BACKEND (kf_backend),
                                       "",
                                       (const gchar **) changed_array->pdata,
                                       NULL);
    }

  g_hash_table_unref (loaded_keys);
  g_ptr_array_free (changed_array, TRUE);
}

static gboolean
g_keyfile_settings_backend_keyfile_writable (GFile *file)
{
  GFileInfo *fileinfo;
  GError    *error;
  gboolean   writable = FALSE;

  error = NULL;
  fileinfo = g_file_query_info (file, "access::*",
                                G_FILE_QUERY_INFO_NONE, NULL, &error);

  if (fileinfo == NULL)
    {
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
        {
          GFile *parent;

          parent = g_file_get_parent (file);
          if (parent)
            {
              writable = g_keyfile_settings_backend_keyfile_writable (parent);
              g_object_unref (parent);
            }
        }

      g_error_free (error);

      return writable;
    }

  /* We don't want to mark the backend as writable if the file is not readable,
   * since it means we won't be able to load the content of the file, and we'll
   * lose data. */
  writable =
        g_file_info_get_attribute_boolean (fileinfo, G_FILE_ATTRIBUTE_ACCESS_CAN_READ) &&
        g_file_info_get_attribute_boolean (fileinfo, G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE);
  g_object_unref (fileinfo);

  return writable;
}

static void
g_keyfile_settings_backend_keyfile_changed (GFileMonitor      *monitor,
                                            GFile             *file,
                                            GFile             *other_file,
                                            GFileMonitorEvent  event_type,
                                            gpointer           user_data)
{
  GKeyfileSettingsBackend *kf_backend;

  if (event_type != G_FILE_MONITOR_EVENT_CHANGED &&
      event_type != G_FILE_MONITOR_EVENT_CREATED &&
      event_type != G_FILE_MONITOR_EVENT_DELETED &&
      event_type != G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED)
    return;

  kf_backend = G_KEYFILE_SETTINGS_BACKEND (user_data);

  if (event_type == G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED)
    {
      gboolean writable;

      writable = g_keyfile_settings_backend_keyfile_writable (file);

      if (kf_backend->priv->writable == writable)
        return;

      kf_backend->priv->writable = writable;
      if (!writable)
        return;
      /* else: reload the file since it was possibly not readable before */
    }

  g_keyfile_settings_backend_keyfile_reload (kf_backend);
}

static void
g_keyfile_settings_backend_finalize (GObject *object)
{
  GKeyfileSettingsBackend *kf_backend = G_KEYFILE_SETTINGS_BACKEND (object);

  g_hash_table_unref (kf_backend->priv->table);
  kf_backend->priv->table = NULL;

  g_key_file_free (kf_backend->priv->keyfile);
  kf_backend->priv->keyfile = NULL;

  g_free (kf_backend->priv->file_path);
  kf_backend->priv->file_path = NULL;

  g_free (kf_backend->priv->checksum);
  kf_backend->priv->checksum = NULL;

  g_file_monitor_cancel (kf_backend->priv->monitor);
  g_object_unref (kf_backend->priv->monitor);
  kf_backend->priv->monitor = NULL;

  G_OBJECT_CLASS (g_keyfile_settings_backend_parent_class)
    ->finalize (object);
}

static void
g_keyfile_settings_backend_init (GKeyfileSettingsBackend *kf_backend)
{
  kf_backend->priv = G_TYPE_INSTANCE_GET_PRIVATE (kf_backend,
                                                  G_TYPE_KEYFILE_SETTINGS_BACKEND,
                                                  GKeyfileSettingsBackendPrivate);
  kf_backend->priv->table =
    g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
                           (GDestroyNotify) g_variant_unref);

  kf_backend->priv->keyfile = NULL;
  kf_backend->priv->writable = FALSE;
  kf_backend->priv->file_path = NULL;
  kf_backend->priv->checksum = NULL;
  kf_backend->priv->monitor = NULL;
}

static void
g_keyfile_settings_backend_class_init (GKeyfileSettingsBackendClass *class)
{
  GSettingsBackendClass *backend_class = G_SETTINGS_BACKEND_CLASS (class);
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = g_keyfile_settings_backend_finalize;

  backend_class->read = g_keyfile_settings_backend_read;
  backend_class->write = g_keyfile_settings_backend_write;
  backend_class->write_keys = g_keyfile_settings_backend_write_keys;
  backend_class->reset = g_keyfile_settings_backend_reset;
  backend_class->reset_path = g_keyfile_settings_backend_reset_path;
  backend_class->get_writable = g_keyfile_settings_backend_get_writable;
  /* No need to implement subscribed/unsubscribe: the only point would be to
   * stop monitoring the file when there's no GSettings anymore, which is no
   * big win. */

  g_type_class_add_private (class, sizeof (GKeyfileSettingsBackendPrivate));
}

static GKeyfileSettingsBackend *
g_keyfile_settings_backend_new (const gchar *filename)
{
  GKeyfileSettingsBackend *kf_backend;
  GFile *file;

  kf_backend = g_object_new (G_TYPE_KEYFILE_SETTINGS_BACKEND, NULL);
  kf_backend->priv->file_path = g_strdup (filename);

  file = g_file_new_for_path (kf_backend->priv->file_path);

  kf_backend->priv->writable = g_keyfile_settings_backend_keyfile_writable (file);

  kf_backend->priv->monitor = g_file_monitor_file (file, G_FILE_MONITOR_SEND_MOVED, NULL, NULL);
  g_signal_connect (kf_backend->priv->monitor, "changed",
                    (GCallback)g_keyfile_settings_backend_keyfile_changed, kf_backend);

  g_object_unref (file);

  g_keyfile_settings_backend_keyfile_reload (kf_backend);

  return kf_backend;
}

/**
 * g_settings_backend_setup_keyfile:
 * @context: a context string (not %NULL or "")
 * @filename: a filename
 *
 * Sets up a keyfile for use with #GSettings.
 *
 * If you create a #GSettings with its context property set to @context
 * then the settings will be stored in the keyfile at @filename.  See
 * g_settings_new_with_context().
 *
 * The keyfile must be setup before any settings objects are created
 * for the named context.
 *
 * It is not possible to specify a keyfile for the default context.
 *
 * If the path leading up to @filename does not exist, it will be
 * recursively created with user-only permissions.  If the keyfile is
 * not writable, any #GSettings objects created using @context will
 * return %FALSE for any calls to g_settings_is_writable() and any
 * attempts to write will fail.
 *
 * Since: 2.26
 */
void
g_settings_backend_setup_keyfile (const gchar *context,
                                  const gchar *filename)
{
  GKeyfileSettingsBackend *kf_backend;

  kf_backend = g_keyfile_settings_backend_new (filename);
  g_settings_backend_setup (context, G_SETTINGS_BACKEND (kf_backend));
  g_object_unref (kf_backend);
}

#define __G_KEYFILE_SETTINGS_BACKEND_C__
#include "gioaliasdef.c"
