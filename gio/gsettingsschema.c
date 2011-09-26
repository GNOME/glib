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
 */

#include "config.h"

#include "gsettingsschema.h"
#include "gvdb/gvdb-reader.h"

#include <glibintl.h>

G_DEFINE_TYPE (GSettingsSchema, g_settings_schema, G_TYPE_OBJECT)

struct _GSettingsSchemaPrivate
{
  const gchar *gettext_domain;
  const gchar *path;
  GQuark *items;
  gint n_items;
  GvdbTable *table;
  gchar *name;
};

static GSList *schema_sources;

static void
initialise_schema_sources (void)
{
  static gsize initialised;

  if G_UNLIKELY (g_once_init_enter (&initialised))
    {
      const gchar * const *dir;
      const gchar *path;

      for (dir = g_get_system_data_dirs (); *dir; dir++)
        {
          gchar *filename;
          GvdbTable *table;

          filename = g_build_filename (*dir, "glib-2.0", "schemas",
                                       "gschemas.compiled", NULL);
          table = gvdb_table_new (filename, TRUE, NULL);

          if (table != NULL)
            schema_sources = g_slist_prepend (schema_sources, table);

          g_free (filename);
        }

      schema_sources = g_slist_reverse (schema_sources);

      if ((path = g_getenv ("GSETTINGS_SCHEMA_DIR")) != NULL)
        {
          gchar *filename;
          GvdbTable *table;

          filename = g_build_filename (path, "gschemas.compiled", NULL);
          table = gvdb_table_new (filename, TRUE, NULL);

          if (table != NULL)
            schema_sources = g_slist_prepend (schema_sources, table);

          g_free (filename);
        }

      g_once_init_leave (&initialised, TRUE);
    }
}

static gboolean
steal_item (gpointer key,
            gpointer value,
            gpointer user_data)
{
  gchar ***ptr = user_data;

  *(*ptr)++ = (gchar *) key;

  return TRUE;
}

static const gchar * const *non_relocatable_schema_list;
static const gchar * const *relocatable_schema_list;
static gsize schema_lists_initialised;

static void
ensure_schema_lists (void)
{
  if (g_once_init_enter (&schema_lists_initialised))
    {
      GHashTable *single, *reloc;
      const gchar **ptr;
      GSList *source;
      gchar **list;
      gint i;

      initialise_schema_sources ();

      /* We use hash tables to avoid duplicate listings for schemas that
       * appear in more than one file.
       */
      single = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
      reloc = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

      for (source = schema_sources; source; source = source->next)
        {
          list = gvdb_table_list (source->data, "");

          g_assert (list != NULL);

          for (i = 0; list[i]; i++)
            {
              if (!g_hash_table_lookup (single, list[i]) &&
                  !g_hash_table_lookup (reloc, list[i]))
                {
                  GvdbTable *table;

                  table = gvdb_table_get_table (source->data, list[i]);
                  g_assert (table != NULL);

                  if (gvdb_table_has_value (table, ".path"))
                    g_hash_table_insert (single, g_strdup (list[i]), NULL);
                  else
                    g_hash_table_insert (reloc, g_strdup (list[i]), NULL);

                  gvdb_table_unref (table);
                }
            }

          g_strfreev (list);
        }

      ptr = g_new (const gchar *, g_hash_table_size (single) + 1);
      non_relocatable_schema_list = ptr;
      g_hash_table_foreach_steal (single, steal_item, &ptr);
      g_hash_table_unref (single);
      *ptr = NULL;

      ptr = g_new (const gchar *, g_hash_table_size (reloc) + 1);
      relocatable_schema_list = ptr;
      g_hash_table_foreach_steal (reloc, steal_item, &ptr);
      g_hash_table_unref (reloc);
      *ptr = NULL;

      g_once_init_leave (&schema_lists_initialised, TRUE);
    }
}

/**
 * g_settings_list_schemas:
 *
 * Gets a list of the #GSettings schemas installed on the system.  The
 * returned list is exactly the list of schemas for which you may call
 * g_settings_new() without adverse effects.
 *
 * This function does not list the schemas that do not provide their own
 * paths (ie: schemas for which you must use
 * g_settings_new_with_path()).  See
 * g_settings_list_relocatable_schemas() for that.
 *
 * Returns: (element-type utf8) (transfer none):  a list of #GSettings
 *   schemas that are available.  The list must not be modified or
 *   freed.
 *
 * Since: 2.26
 **/
const gchar * const *
g_settings_list_schemas (void)
{
  ensure_schema_lists ();

  return non_relocatable_schema_list;
}

/**
 * g_settings_list_relocatable_schemas:
 *
 * Gets a list of the relocatable #GSettings schemas installed on the
 * system.  These are schemas that do not provide their own path.  It is
 * usual to instantiate these schemas directly, but if you want to you
 * can use g_settings_new_with_path() to specify the path.
 *
 * The output of this function, tTaken together with the output of
 * g_settings_list_schemas() represents the complete list of all
 * installed schemas.
 *
 * Returns: (element-type utf8) (transfer none): a list of relocatable
 *   #GSettings schemas that are available.  The list must not be
 *   modified or freed.
 *
 * Since: 2.28
 **/
const gchar * const *
g_settings_list_relocatable_schemas (void)
{
  ensure_schema_lists ();

  return relocatable_schema_list;
}

static void
g_settings_schema_finalize (GObject *object)
{
  GSettingsSchema *schema = G_SETTINGS_SCHEMA (object);

  gvdb_table_unref (schema->priv->table);
  g_free (schema->priv->items);
  g_free (schema->priv->name);

  G_OBJECT_CLASS (g_settings_schema_parent_class)
    ->finalize (object);
}

static void
g_settings_schema_init (GSettingsSchema *schema)
{
  schema->priv = G_TYPE_INSTANCE_GET_PRIVATE (schema, G_TYPE_SETTINGS_SCHEMA,
                                              GSettingsSchemaPrivate);
}

static void
g_settings_schema_class_init (GSettingsSchemaClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = g_settings_schema_finalize;

  g_type_class_add_private (class, sizeof (GSettingsSchemaPrivate));
}

const gchar *
g_settings_schema_get_string (GSettingsSchema *schema,
                              const gchar     *key)
{
  const gchar *result = NULL;
  GVariant *value;

  if ((value = gvdb_table_get_raw_value (schema->priv->table, key)))
    {
      result = g_variant_get_string (value, NULL);
      g_variant_unref (value);
    }

  return result;
}

GSettingsSchema *
g_settings_schema_new (const gchar *name)
{
  GSettingsSchema *schema;
  GvdbTable *table = NULL;
  GSList *source;

  g_return_val_if_fail (name != NULL, NULL);

  initialise_schema_sources ();

  for (source = schema_sources; source; source = source->next)
    {
      GvdbTable *file = source->data;

      if ((table = gvdb_table_get_table (file, name)))
        break;
    }

  if (table == NULL)
    {
      g_critical ("Settings schema '%s' is not installed\n", name);
      return NULL;
    }

  schema = g_object_new (G_TYPE_SETTINGS_SCHEMA, NULL);
  schema->priv->name = g_strdup (name);
  schema->priv->table = table;
  schema->priv->path =
    g_settings_schema_get_string (schema, ".path");
  schema->priv->gettext_domain =
    g_settings_schema_get_string (schema, ".gettext-domain");

  if (schema->priv->gettext_domain)
    bind_textdomain_codeset (schema->priv->gettext_domain, "UTF-8");

  return schema;
}

GVariantIter *
g_settings_schema_get_value (GSettingsSchema *schema,
                             const gchar     *key)
{
  GVariantIter *iter;
  GVariant *value;

  value = gvdb_table_get_raw_value (schema->priv->table, key);

  if G_UNLIKELY (value == NULL)
    g_error ("Settings schema '%s' does not contain a key named '%s'",
             schema->priv->name, key);

  iter = g_variant_iter_new (value);
  g_variant_unref (value);

  return iter;
}

const gchar *
g_settings_schema_get_path (GSettingsSchema *schema)
{
  return schema->priv->path;
}

const gchar *
g_settings_schema_get_gettext_domain (GSettingsSchema *schema)
{
  return schema->priv->gettext_domain;
}

gboolean
g_settings_schema_has_key (GSettingsSchema *schema,
                           const gchar     *key)
{
  return gvdb_table_has_value (schema->priv->table, key);
}

const GQuark *
g_settings_schema_list (GSettingsSchema *schema,
                        gint            *n_items)
{
  gint i, j;

  if (schema->priv->items == NULL)
    {
      gchar **list;
      gint len;

      list = gvdb_table_list (schema->priv->table, "");
      len = list ? g_strv_length (list) : 0;

      schema->priv->items = g_new (GQuark, len);
      j = 0;

      for (i = 0; i < len; i++)
        if (list[i][0] != '.')
          schema->priv->items[j++] = g_quark_from_string (list[i]);
      schema->priv->n_items = j;

      g_strfreev (list);
    }

  *n_items = schema->priv->n_items;
  return schema->priv->items;
}
