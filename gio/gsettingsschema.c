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

/**
 * SECTION:gsettingsschema
 * @short_description: schema information for settings
 *
 * The #GSettingsSchema class provides schema information (i.e. types,
 * default values and descriptions) for keys in settings.
 *
 * Schema information is required to use #GSettings.
 *
 * The source format for GSettings schemas is an XML format that can
 * be described with the following DTD:
 * |[<![CDATA[
 * <!ELEMENT schemalist (schema*) >
 * <!ATTLIST schemalist gettext-domain #IMPLIED >
 *
 * <!ELEMENT schema (key*) >
 * <!ATTLIST schema id             #REQUIRED
 *                  path           #IMPLIED
 *                  gettext-domain #IMPLIED >
 *
 * <!ELEMENT key (default|summary?|description?|range?|choices?) >
 * <!ATTLIST key name #REQUIRED
 *               type #REQUIRED >
 *
 * <!ELEMENT default (#PCDATA) >
 * <!ATTLIST default l10n #IMPLIED >
 *
 * <!ELEMENT summary (#PCDATA) >
 * <!ELEMENT description (#PCDATA) >
 *
 * <!ELEMENT range (min,max)  >
 * <!ELEMENT min (#PCDATA) >
 * <!ELEMENT max (#PCDATA) >
 *
 * <!ELEMENT choices (choice+) >
 * <!ELEMENT choice (alias?) >
 * <!ATTLIST choice value #REQUIRED >
 * <!ELEMENT choice (alias?) >
 * <!ELEMENT alias EMPTY >
 * <!ATTLIST alias value #REQUIRED >
 * ]]>
 * ]|
 */

G_DEFINE_TYPE (GSettingsSchema, g_settings_schema, G_TYPE_OBJECT)

struct _GSettingsSchemaPrivate
{
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
      gchar *path;

      for (dir = g_get_system_data_dirs (); *dir; dir++)
        {
          gchar *filename;
          GvdbTable *table;

          filename = g_build_filename (*dir, "glib-2.0/schemas", "gschemas.compiled", NULL);
          table = gvdb_table_new (filename, TRUE, NULL);

          if (table != NULL)
            schema_sources = g_slist_prepend (schema_sources, table);

          g_free (filename);
        }

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

      schema_sources = g_slist_reverse (schema_sources);

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

/**
 * g_settings_schema_new:
 * @name: the name of the schema
 * @returns: a newly created #GSettingsSchema object
 *
 * Creates a new #GSettingsSchema object for the schema
 * with the specified name. A settings schema with this name
 * must have been installed, see gschema-compile.
 *
 * Since: 2.26
 */
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

  return schema;
}

/**
 * g_settings_schema_get_value:
 * @schema: a #GSettingsSchema oject
 * @key: the key to get the value for
 * @options: return location for options, or %NULL
 * @returns: a #GVariant holding the default value for @key, or %NULL
 *
 * Gets the default value that is defined for @key
 * in @schema. If @options is not %NULL, then it will be set either
 * to %NULL in the case of no options or a #GVariant containing a
 * dictionary mapping strings to variants.
 *
 * You should call g_variant_unref() on the returned value when
 * it is no longer needed.
 *
 * Since: 2.26
 */
GVariant *
g_settings_schema_get_value (GSettingsSchema  *schema,
                             const gchar      *key,
                             GVariant        **options)
{
  return gvdb_table_get_value (schema->priv->table, key, options);
}

/**
 * g_settings_schema_get_path:
 * @schema: a #GSettingsSchema object
 * @returns: the path of the schema, or %NULL
 *
 * Returns the prefix which is prepended to keys described in the schema
 * when storing them in a #GSettingsBackend. Some schemas can be stored
 * at different prefixes, in which case this function will return %NULL.
 *
 * Since: 2.26
 */
const gchar *
g_settings_schema_get_path (GSettingsSchema *schema)
{
  const gchar *result;
  GVariant *value;

  value = gvdb_table_get_value (schema->priv->table, ".path", NULL);

  if (value && g_variant_is_of_type (value, G_VARIANT_TYPE_STRING))
    {
      result = g_variant_get_string (value, NULL);
      g_variant_unref (value);
    }
  else
    result = NULL;

  return result;
}
