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
#include <glib.h>
#include <glibintl.h>

#include "gsettingsschema.h"

#include "gvdb/gvdb-reader.h"


G_DEFINE_TYPE (GSettingsSchema, g_settings_schema, G_TYPE_OBJECT)

struct _GSettingsSchemaPrivate
{
  const gchar *gettext_domain;
  const gchar *path;
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

          filename = g_build_filename (*dir, "glib-2.0", "schemas", "gschemas.compiled", NULL);
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

static void
g_settings_schema_finalize (GObject *object)
{
  GSettingsSchema *schema = G_SETTINGS_SCHEMA (object);

  gvdb_table_unref (schema->priv->table);
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

static const gchar *
g_settings_schema_get_string (GSettingsSchema *schema,
                              const gchar     *key)
{
  const gchar *result = NULL;
  GVariant *value;

  if ((value = g_settings_schema_get_value (schema, key, NULL)))
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

  initialise_schema_sources ();

  for (source = schema_sources; source; source = source->next)
    {
      GvdbTable *file = source->data;

      if ((table = gvdb_table_get_table (file, name)))
        break;
    }

  if (table == NULL)
    g_error ("Settings schema '%s' is not installed\n", name);

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

GVariant *
g_settings_schema_get_value (GSettingsSchema  *schema,
                             const gchar      *key,
                             GVariant        **options)
{
  return gvdb_table_get_value (schema->priv->table, key, options);
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
  /* XXX vuntz wants a commit now :) */
  return TRUE;
}
