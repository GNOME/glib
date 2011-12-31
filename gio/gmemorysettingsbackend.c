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

#include "gsettingsbackendinternal.h"
#include "gsimplepermission.h"
#include "giomodule.h"

#include <string.h>


#define G_TYPE_MEMORY_SETTINGS_BACKEND  (g_memory_settings_backend_get_type())
#define G_MEMORY_SETTINGS_BACKEND(inst) (G_TYPE_CHECK_INSTANCE_CAST ((inst), \
                                         G_TYPE_MEMORY_SETTINGS_BACKEND,     \
                                         GMemorySettingsBackend))

typedef struct
{
  GSList *removes;
  GSList *adds;
} Directory;

static void
directory_free (gpointer data)
{
  Directory *d = data;

  g_slist_free_full (d->removes, g_free);
  g_slist_free_full (d->adds, g_free);

  g_slice_free (Directory, d);
}


typedef GSettingsBackendClass GMemorySettingsBackendClass;
typedef struct
{
  GSettingsBackend parent_instance;
  GHashTable *directories;
  GHashTable *table;
  guint64 unique_id;
} GMemorySettingsBackend;

G_DEFINE_TYPE_WITH_CODE (GMemorySettingsBackend,
                         g_memory_settings_backend,
                         G_TYPE_SETTINGS_BACKEND,
                         g_io_extension_point_implement (G_SETTINGS_BACKEND_EXTENSION_POINT_NAME,
                                                         g_define_type_id, "memory", 10))

static gboolean
g_memory_settings_backend_verify (GMemorySettingsBackend *memory,
                                  const gchar            *path)
{
  const gchar *end = path;

  /* Given the path '/a/b:/:c/d/e:/f/g' we need to verify that each
   * explicit item exists.  An explicit item is one that is in an
   * explicit directory (ie: directory ending with ':/').  In this case,
   * we have to check:
   *
   *   item ':c/' exists in explicit directory '/a/b:/'
   *
   * and
   *
   *   'f/' exists in '/a/b:/:c/d/e:/'.
   *
   * Items in explicit directories must be directories themselves (ie:
   * they must be terminated with '/').
   *
   * We do this by looking for the first ':/' then the '/' to follow it
   * to grab the directory name and the item name, then repeating until
   * we have found all instances of ':/'.
   *
   * It is possible to directly nest explicit dirs, so we also have to
   * take care to properly handle cases like '/a/b:/:c:/d/' by checking
   * both that ':c:/' exists in '/a/b:/' and also that 'd/' exists in
   * '/a/b:/:c:/' (ie: the 'item' in one iteration can become the 'dir'
   * in the next).
   *
   * We also have to consider being asked for an explicit directory
   * itself.  These implicitly exist just the same as normal items --
   * it's the items in explicit directories that must exist explicitly.
   * Therefore, if we find ':/' at the end of the string, we should not
   * treat it specially.
   */
  while (TRUE)
    {
      const gchar *end_item;
      gchar *dir, *item;
      gboolean exists;
      Directory *d;

      end = strstr (end, ":/");

      /* No more ':/' instances */
      if (end == NULL)
        return TRUE;

      /* Skip past the ':/' which is part of the explicit dir name */
      end += 2;

      /* If ':/' was at the end of the string, we're done */
      if (*end != '\0')
        return TRUE;

      /* Find the end of the item name */
      end_item = strchr (end, '/');

      /* No trailing slash, so surely this can't exist */
      if (end_item == NULL)
        return FALSE;

      /* Skip past the '/' which is part of the item name */
      end_item++;

      dir = g_strndup (path, end - path);
      item = g_strndup (end, end_item - end);

      /* We assume that items starting with a character other than ':'
       * already exist (unless explicitly deleted) and that items
       * starting with ':' only exist if they have been explicitly
       * added.
       */
      exists = item[0] != ':';

      d = g_hash_table_lookup (memory->directories, dir);
      if (d != NULL)
        {
          GSList *list;

          /* For assumed-existing items, we want to search in the
           * removes list.  For assumed-non-existing items, we want to
           * search in the adds list.
           */
          if (exists)
            list = d->removes;
          else
            list = d->adds;

          while (list)
            if (g_str_equal (list->data, item))
              break;

          /* If we found it in the list we were looking for, flip our
           * initial assumption.
           */
          if (list)
            exists = !exists;
        }

      g_free (item);
      g_free (dir);

      if (!exists)
        return FALSE;
    }
}


static GVariant *
g_memory_settings_backend_read (GSettingsBackend   *backend,
                                const gchar        *key,
                                const GVariantType *expected_type,
                                gboolean            default_value)
{
  GMemorySettingsBackend *memory = G_MEMORY_SETTINGS_BACKEND (backend);
  GVariant *value;

  if (default_value)
    return NULL;

  value = g_hash_table_lookup (memory->table, key);

  if (value != NULL)
    g_variant_ref (value);

  return value;
}

static gboolean
g_memory_settings_backend_write (GSettingsBackend *backend,
                                 const gchar      *key,
                                 GVariant         *value)
{
  GMemorySettingsBackend *memory = G_MEMORY_SETTINGS_BACKEND (backend);
  GVariant *old_value;

  if (!g_memory_settings_backend_verify (memory, key))
    return FALSE;

  old_value = g_hash_table_lookup (memory->table, key);
  g_variant_ref_sink (value);

  if (old_value == NULL || !g_variant_equal (value, old_value))
    {
      g_hash_table_insert (memory->table, g_strdup (key), value);
      g_settings_backend_changed (backend, key);
    }
  else
    g_variant_unref (value);

  return TRUE;
}

static void
g_memory_settings_backend_reset (GSettingsBackend *backend,
                                 const gchar      *key)
{
  GMemorySettingsBackend *memory = G_MEMORY_SETTINGS_BACKEND (backend);

  if (g_hash_table_lookup (memory->table, key))
    {
      g_hash_table_remove (memory->table, key);
      g_settings_backend_changed (backend, key);
    }
}

static gboolean
g_memory_settings_backend_get_writable (GSettingsBackend *backend,
                                        const gchar      *name)
{
  GMemorySettingsBackend *memory = G_MEMORY_SETTINGS_BACKEND (backend);

  return g_memory_settings_backend_verify (memory, name);
}

static GPermission *
g_memory_settings_backend_get_permission (GSettingsBackend *backend,
                                          const gchar      *path)
{
  return g_simple_permission_new (TRUE);
}

static gboolean
g_memory_settings_backend_check_exists (GSettingsBackend *backend,
                                        const gchar      *path)
{
  GMemorySettingsBackend *memory = G_MEMORY_SETTINGS_BACKEND (backend);
  gboolean exists;

  exists = g_memory_settings_backend_verify (memory, path);

  return exists;
}

static gchar **
g_memory_settings_backend_list (GSettingsBackend    *backend,
                                const gchar         *dir,
                                const gchar * const *schema_items)
{
  GMemorySettingsBackend *memory = G_MEMORY_SETTINGS_BACKEND (backend);
  GPtrArray *array;
  Directory *d;
  GSList *node;

  d = g_hash_table_lookup (memory->directories, dir);

  /* fast path: by default, just return the schema items */
  if (d == NULL)
    return g_strdupv ((gchar **) schema_items);

  array = g_ptr_array_new ();
  while (schema_items)
    {
      const gchar *item = *schema_items++;

      /* check if this item was removed */
      for (node = d->removes; node; node = node->next)
        if (g_str_equal (node->data, item))
          break;

      /* if not removed, put it in the result */
      if (!node)
        g_ptr_array_add (array, g_strdup (item));
    }

  for (node = d->adds; node; node = node->next)
    g_ptr_array_add (array, g_strdup (node->data));

  g_ptr_array_add (array, NULL);

  return (gchar **) g_ptr_array_free (array, FALSE);
}

static gboolean
g_memory_settings_backend_can_insert (GSettingsBackend *backend,
                                      const gchar      *dir,
                                      const gchar      *before)
{
  GMemorySettingsBackend *memory = G_MEMORY_SETTINGS_BACKEND (backend);

  return g_memory_settings_backend_verify (memory, dir);
}

static gboolean
g_memory_settings_backend_insert (GSettingsBackend  *backend,
                                  const gchar       *dir,
                                  const gchar       *before,
                                  gchar            **item)
{
  GMemorySettingsBackend *memory = G_MEMORY_SETTINGS_BACKEND (backend);
  GSList **ptr;
  Directory *d;
  gchar *id;

  if (!g_memory_settings_backend_verify (memory, dir))
    return FALSE;

  d = g_hash_table_lookup (memory->directories, dir);

  if (d == NULL)
    {
      d = g_slice_new0 (Directory);
      g_hash_table_insert (memory->directories, g_strdup (dir), d);
    }

  id = g_strdup_printf (":%"G_GINT64_FORMAT, memory->unique_id++);

  for (ptr = &d->adds; ptr; ptr = &(*ptr)->next)
    if (before && g_str_equal ((*ptr)->data, before))
      break;

  *ptr = g_slist_prepend (*ptr, id);

  if (item != NULL)
    *item = g_strdup (id);

  return TRUE;
}

static gboolean
g_memory_settings_backend_can_remove (GSettingsBackend *backend,
                                      const gchar      *dir,
                                      const gchar      *item)
{
  GMemorySettingsBackend *memory = G_MEMORY_SETTINGS_BACKEND (backend);

  return g_memory_settings_backend_verify (memory, dir);
}

static void
g_memory_settings_backend_clear (GMemorySettingsBackend *memory,
                                 const gchar            *dir,
                                 const gchar            *item)
{
  GHashTableIter iter;
  gchar *prefix;
  gpointer key;

  prefix = g_strconcat (dir, item, NULL);

  /* slow and steady... */
  g_hash_table_iter_init (&iter, memory->table);
  while (g_hash_table_iter_next (&iter, &key, NULL))
    if (g_str_has_prefix (key, prefix))
      g_hash_table_iter_remove (&iter);

  g_hash_table_iter_init (&iter, memory->directories);
  while (g_hash_table_iter_next (&iter, &key, NULL))
    if (g_str_has_prefix (key, prefix))
      g_hash_table_iter_remove (&iter);

  g_free (prefix);
}

static gboolean
g_memory_settings_backend_remove (GSettingsBackend *backend,
                                  const gchar      *dir,
                                  const gchar      *item)
{
  GMemorySettingsBackend *memory = G_MEMORY_SETTINGS_BACKEND (backend);
  Directory *d;

  if (!g_memory_settings_backend_verify (memory, dir))
    return FALSE;

  d = g_hash_table_lookup (memory->directories, dir);
  if (d == NULL)
    {
      d = g_slice_new0 (Directory);
      g_hash_table_insert (memory->directories, g_strdup (dir), d);
    }

  /* If the item starts with ':' that we just have to remove it from the
   * 'adds' list.  If it starts with a non-':' then we need to add a
   * remove entry for it.
   */
  if (item[0] == ':')
    {
      GSList **ptr;

      for (ptr = &d->adds; ptr; ptr = &(*ptr)->next)
        if (g_str_equal ((*ptr)->data, item))
          break;

      g_free ((*ptr)->data);
      *ptr = g_slist_delete_link (*ptr, *ptr);
    }
  else
    {
      GSList *node;

      for (node = d->removes; node; node = node->next)
        if (g_str_equal (node->data, item))
          break;

      if (!node)
        d->removes = g_slist_prepend (d->removes, g_strdup (item));
    }

  /* Remove all subitems that may have been stored */
  g_memory_settings_backend_clear (memory, dir, item);

  return TRUE;
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
  memory->directories = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, directory_free);
}

static void
g_memory_settings_backend_class_init (GMemorySettingsBackendClass *class)
{
  GSettingsBackendClass *backend_class = G_SETTINGS_BACKEND_CLASS (class);
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  backend_class->read = g_memory_settings_backend_read;
  backend_class->write = g_memory_settings_backend_write;
  backend_class->reset = g_memory_settings_backend_reset;
  backend_class->get_writable = g_memory_settings_backend_get_writable;
  backend_class->get_permission = g_memory_settings_backend_get_permission;
  backend_class->check_exists = g_memory_settings_backend_check_exists;
  backend_class->list = g_memory_settings_backend_list;
  backend_class->can_insert = g_memory_settings_backend_can_insert;
  backend_class->insert = g_memory_settings_backend_insert;
  backend_class->can_remove = g_memory_settings_backend_can_remove;
  backend_class->remove = g_memory_settings_backend_remove;
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
