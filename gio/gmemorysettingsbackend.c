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
 * Authors: Ryan Lortie <desrt@desrt.ca>
 *          Rodrigo Moya <rodrigo@gnome.org>
 */

#include "config.h"

#include "gmemorysettingsbackend.h"
#include "gsimplepermission.h"
#include "gsettingsbackend.h"
#include "giomodule.h"


#define G_TYPE_MEMORY_SETTINGS_BACKEND  (g_memory_settings_backend_get_type())
#define G_MEMORY_SETTINGS_BACKEND(inst) (G_TYPE_CHECK_INSTANCE_CAST ((inst), \
                                         G_TYPE_MEMORY_SETTINGS_BACKEND,     \
                                         GMemorySettingsBackend))

typedef GSettingsBackendClass GMemorySettingsBackendClass;
typedef struct
{
  GSettingsBackend parent_instance;
  GHashTable *table;
} GMemorySettingsBackend;

G_DEFINE_TYPE_WITH_CODE (GMemorySettingsBackend,
                         g_memory_settings_backend,
                         G_TYPE_SETTINGS_BACKEND,
                         g_io_extension_point_implement (G_SETTINGS_BACKEND_EXTENSION_POINT_NAME,
                                                         g_define_type_id, "memory", 10))

static GHashTable *
g_memory_settings_backend_get_table (GHashTable   *table,
                                     const gchar **key_ptr)
{
  const gchar *key = *key_ptr;
  gchar *prefix;
  gchar *name;
  gint i, j;

  for (i = 2; key[i - 2] != ':' || key[i - 1] != '/'; i++)
    if (key[i - 2] == '\0')
      /* no :/ in the string -- pass through */
      return table;

  for (j = i + 1; key[j - 1] != '/'; j++)
    if (key[j - 1] == '\0')
      /* string of form /some/path:/child -- definitely invalid */
      return NULL;

  /* We now have this situation:
   *
   *   /some/path:/child/item
   *   ^0          ^i    ^j
   *
   * So we can split the string into 3 parts:
   *
   *   prefix   = /some/path:/
   *   child    = child/
   *   *key_ptr = item
   */
  prefix = g_strndup (key, i);
  name = g_strndup (key + i, j - i);

  /* name is either like 'foo' or ':foo'
   *
   * If it doesn't start with a colon, it's in the schema.
   */
  if (name[0] == ':')
    {
      table = g_hash_table_lookup (table, prefix);
      if (table)
        table = g_hash_table_lookup (table, name);
    }
  else
    {
      gpointer value;

      if (!g_hash_table_lookup_extended (table, prefix, NULL, &value))
        g_hash_table_insert (table, prefix,
                             value = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                            g_free, (GDestroyNotify) g_hash_table_unref));

      table = value;

      if (!g_hash_table_lookup_extended (table, prefix, NULL, &value))
        g_hash_table_insert (table, prefix,
                             value = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                            g_free, (GDestroyNotify) g_hash_table_unref));
    }

    if (key[i] == ':' && key[i + 1] == '/')
      {
        gchar *prefix;
        gchar *name;
        gint j;

        i += 2;

        for (j = i; key[j] != '/'; j++)
          /* found a path of the form /a:/b
           * which is never a valid spot for a key.
           */
          if (key[j] == '\0')
            return NULL;

        name = g_strndup ((const gchar *) key + i, j - i);
        prefix = g_strndup (key, i);
        table = g_hash_table_lookup (table, prefix);
        g_free (prefix);
      }

  return table;
}

static GVariant *
g_memory_settings_backend_read (GSettingsBackend   *backend,
                                const gchar        *key,
                                const GVariantType *expected_type,
                                gboolean            default_value)
{
  GMemorySettingsBackend *memory = G_MEMORY_SETTINGS_BACKEND (backend);
  GVariant *value;
  GHashTable *table;

  if (default_value)
    return NULL;

  table = g_memory_settings_backend_get_table (memory->table, &key);
  if (!table)
    return NULL;

  value = g_hash_table_lookup (table, key);

  if (value != NULL)
    g_variant_ref (value);

  return value;
}

static gboolean
g_memory_settings_backend_write (GSettingsBackend *backend,
                                 const gchar      *key,
                                 GVariant         *value,
                                 gpointer          origin_tag)
{
  GMemorySettingsBackend *memory = G_MEMORY_SETTINGS_BACKEND (backend);
  GVariant *old_value;
  GHashTable *table;

  table = g_memory_settings_backend_get_table (memory->table, &key);
  if (!table)
    return FALSE;

  old_value = g_hash_table_lookup (table, key);
  g_variant_ref_sink (value);

  if (old_value == NULL || !g_variant_equal (value, old_value))
    {
      g_hash_table_insert (memory->table, g_strdup (key), value);
      g_settings_backend_changed (backend, key, origin_tag);
    }
  else
    g_variant_unref (value);

  return TRUE;
}

static gboolean
g_memory_settings_backend_write_one (gpointer key,
                                     gpointer value,
                                     gpointer data)
{
  GMemorySettingsBackend *memory = data;
  GHashTable *table;

  table = g_memory_settings_backend_get_table (memory->table, &key);
  if (!table)
    return FALSE;

  if (value != NULL)
    g_hash_table_insert (table, g_strdup (key), g_variant_ref (value));
  else
    g_hash_table_remove (table, key);

  return FALSE;
}

static gboolean
g_memory_settings_backend_write_tree (GSettingsBackend *backend,
                                      GTree            *tree,
                                      gpointer          origin_tag)
{
  g_tree_foreach (tree, g_memory_settings_backend_write_one, backend);
  g_settings_backend_changed_tree (backend, tree, origin_tag);

  return TRUE;
}

static void
g_memory_settings_backend_reset (GSettingsBackend *backend,
                                 const gchar      *key,
                                 gpointer          origin_tag)
{
  GMemorySettingsBackend *memory = G_MEMORY_SETTINGS_BACKEND (backend);
  GHashTable *table;

  table = g_memory_settings_backend_get_table (memory->table, &key);
  if (!table)
    return;

  if (g_hash_table_lookup (table, key))
    {
      g_hash_table_remove (table, key);
      g_settings_backend_changed (backend, key, origin_tag);
    }
}

static gboolean
g_memory_settings_backend_get_writable (GSettingsBackend *backend,
                                        const gchar      *name)
{
  return TRUE;
}

static GPermission *
g_memory_settings_backend_get_permission (GSettingsBackend *backend,
                                          const gchar      *path)
{
  return g_simple_permission_new (TRUE);
}

static gchar **
g_memory_settings_backend_list (GSettingsBackend    *backend,
				const gchar         *path,
				const gchar * const *schema_items)
{
  gchar **result = NULL;
  GHashTable *table;
  GMemorySettingsBackend *memory = G_MEMORY_SETTINGS_BACKEND (backend);

  table = g_memory_settings_backend_get_table (memory->table, &path);
  if (table != NULL)
    {
      GHashTableIter iter;
      gchar *key_name;
      guint i;
      GSList *list = NULL;

      while (g_hash_table_iter_next (&iter, (gpointer *) &key_name, NULL))
        {
	  gboolean in_schema_items = FALSE;

	  for (i = 0; i < g_strv_length ((gchar **) schema_items); i++)
	    {
	      if (g_str_equal (key_name, schema_items[i]))
	        {
		  /* It's in both lists, so return it */
		  list = g_slist_append (list, key_name);
		  in_schema_items = TRUE;
		}
	    }
	  if (!in_schema_items)
	    {
	      /* Has been added by the user */
	      list = g_slist_append (list, key_name);
	    }
	}

      result = g_new0 (gchar *, g_slist_length (list) + 1);
      i = 0;
      while (list != NULL)
        {
	  result[i] = g_strdup ((const gchar *) list->data);
	  i++;

	  list = g_slist_remove (list, list->data);
	}

      result[i] = NULL;
    }

  return result;
}

static gboolean
g_memory_settings_backend_can_insert (GSettingsBackend *backend,
				      const gchar      *path)
{
  return FALSE;
}

static gboolean
g_memory_settings_backend_can_remove (GSettingsBackend *backend,
				      const gchar      *path,
				      const gchar      *id)
{
  return FALSE;
}

static gboolean
g_memory_settings_backend_insert (GSettingsBackend *backend,
				  const gchar      *path,
				  gint              index_,
				  const gchar      *prefix,
				  gchar           **name)
{
  return FALSE;
}

static gboolean
g_memory_settings_backend_remove (GSettingsBackend *backend,
				  const gchar      *path,
				  const gchar      *id)
{
  return FALSE;
}

static void
g_memory_settings_backend_finalize (GObject *object)
{
  GMemorySettingsBackend *memory = G_MEMORY_SETTINGS_BACKEND (object);

  g_hash_table_unref (memory->table);

  G_OBJECT_CLASS (g_memory_settings_backend_parent_class)
    ->finalize (object);
}

static void
g_memory_settings_backend_init (GMemorySettingsBackend *memory)
{
  memory->table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
                                         (GDestroyNotify) g_variant_unref);
}

static void
g_memory_settings_backend_class_init (GMemorySettingsBackendClass *class)
{
  GSettingsBackendClass *backend_class = G_SETTINGS_BACKEND_CLASS (class);
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  backend_class->read = g_memory_settings_backend_read;
  backend_class->write = g_memory_settings_backend_write;
  backend_class->write_tree = g_memory_settings_backend_write_tree;
  backend_class->reset = g_memory_settings_backend_reset;
  backend_class->get_writable = g_memory_settings_backend_get_writable;
  backend_class->get_permission = g_memory_settings_backend_get_permission;
  backend_class->list = g_memory_settings_backend_list;
  backend_class->can_insert = g_memory_settings_backend_can_insert;
  backend_class->can_remove = g_memory_settings_backend_can_remove;
  backend_class->insert = g_memory_settings_backend_insert;
  backend_class->remove = g_memory_settings_backend_remove;
  object_class->finalize = g_memory_settings_backend_finalize;
}
