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

#include "gsettingsschema-internal.h"
#include "gsettings.h"

#include "gvdb/gvdb-reader.h"
#include "strinfo.c"

#include <glibintl.h>
#include <string.h>

struct _GSettingsSchema
{
  const gchar *gettext_domain;
  const gchar *path;
  GQuark *items;
  gint n_items;
  GvdbTable *table;
  gchar *name;

  gint ref_count;
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

GSettingsSchema *
g_settings_schema_ref (GSettingsSchema *schema)
{
  g_atomic_int_inc (&schema->ref_count);

  return schema;
}

void
g_settings_schema_unref (GSettingsSchema *schema)
{
  if (g_atomic_int_dec_and_test (&schema->ref_count))
    {
      gvdb_table_unref (schema->table);
      g_free (schema->items);
      g_free (schema->name);

      g_slice_free (GSettingsSchema, schema);
    }
}

const gchar *
g_settings_schema_get_string (GSettingsSchema *schema,
                              const gchar     *key)
{
  const gchar *result = NULL;
  GVariant *value;

  if ((value = gvdb_table_get_raw_value (schema->table, key)))
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
    g_error ("Settings schema '%s' is not installed\n", name);

  schema = g_slice_new0 (GSettingsSchema);
  schema->ref_count = 1;
  schema->name = g_strdup (name);
  schema->table = table;
  schema->path =
    g_settings_schema_get_string (schema, ".path");
  schema->gettext_domain =
    g_settings_schema_get_string (schema, ".gettext-domain");

  if (schema->gettext_domain)
    bind_textdomain_codeset (schema->gettext_domain, "UTF-8");

  return schema;
}

GVariantIter *
g_settings_schema_get_value (GSettingsSchema *schema,
                             const gchar     *key)
{
  GVariantIter *iter;
  GVariant *value;

  value = gvdb_table_get_raw_value (schema->table, key);

  if G_UNLIKELY (value == NULL)
    g_error ("Settings schema '%s' does not contain a key named '%s'",
             schema->name, key);

  iter = g_variant_iter_new (value);
  g_variant_unref (value);

  return iter;
}

const gchar *
g_settings_schema_get_path (GSettingsSchema *schema)
{
  return schema->path;
}

const gchar *
g_settings_schema_get_gettext_domain (GSettingsSchema *schema)
{
  return schema->gettext_domain;
}

gboolean
g_settings_schema_has_key (GSettingsSchema *schema,
                           const gchar     *key)
{
  return gvdb_table_has_value (schema->table, key);
}

const GQuark *
g_settings_schema_list (GSettingsSchema *schema,
                        gint            *n_items)
{
  gint i, j;

  if (schema->items == NULL)
    {
      gchar **list;
      gint len;

      list = gvdb_table_list (schema->table, "");
      len = list ? g_strv_length (list) : 0;

      schema->items = g_new (GQuark, len);
      j = 0;

      for (i = 0; i < len; i++)
        if (list[i][0] != '.')
          schema->items[j++] = g_quark_from_string (list[i]);
      schema->n_items = j;

      g_strfreev (list);
    }

  *n_items = schema->n_items;
  return schema->items;
}

const gchar *
g_settings_schema_get_name (GSettingsSchema *schema)
{
  return schema->name;
}

static inline void
endian_fixup (GVariant **value)
{
#if G_BYTE_ORDER == G_BIG_ENDIAN
  GVariant *tmp;

  tmp = g_variant_byteswap (*value);
  g_variant_unref (*value);
  *value = tmp;
#endif
}

void
g_settings_schema_key_init (GSettingsSchemaKey *key,
                            GSettingsSchema    *schema,
                            const gchar        *name)
{
  GVariantIter *iter;
  GVariant *data;
  guchar code;

  memset (key, 0, sizeof *key);

  iter = g_settings_schema_get_value (schema, name);

  key->schema = g_settings_schema_ref (schema);
  key->default_value = g_variant_iter_next_value (iter);
  endian_fixup (&key->default_value);
  key->type = g_variant_get_type (key->default_value);
  key->name = g_intern_string (name);

  while (g_variant_iter_next (iter, "(y*)", &code, &data))
    {
      switch (code)
        {
        case 'l':
          /* translation requested */
          g_variant_get (data, "(y&s)", &key->lc_char, &key->unparsed);
          break;

        case 'e':
          /* enumerated types... */
          key->is_enum = TRUE;
          goto choice;

        case 'f':
          /* flags... */
          key->is_flags = TRUE;
          goto choice;

        choice: case 'c':
          /* ..., choices, aliases */
          key->strinfo = g_variant_get_fixed_array (data, &key->strinfo_length, sizeof (guint32));
          break;

        case 'r':
          g_variant_get (data, "(**)", &key->minimum, &key->maximum);
          endian_fixup (&key->minimum);
          endian_fixup (&key->maximum);
          break;

        default:
          g_warning ("unknown schema extension '%c'", code);
          break;
        }

      g_variant_unref (data);
    }

  g_variant_iter_free (iter);
}

void
g_settings_schema_key_clear (GSettingsSchemaKey *key)
{
  if (key->minimum)
    g_variant_unref (key->minimum);

  if (key->maximum)
    g_variant_unref (key->maximum);

  g_variant_unref (key->default_value);

  g_settings_schema_unref (key->schema);
}

gboolean
g_settings_schema_key_type_check (GSettingsSchemaKey *key,
                                  GVariant           *value)
{
  g_return_val_if_fail (value != NULL, FALSE);

  return g_variant_is_of_type (value, key->type);
}

gboolean
g_settings_schema_key_range_check (GSettingsSchemaKey *key,
                                   GVariant           *value)
{
  if (key->minimum == NULL && key->strinfo == NULL)
    return TRUE;

  if (g_variant_is_container (value))
    {
      gboolean ok = TRUE;
      GVariantIter iter;
      GVariant *child;

      g_variant_iter_init (&iter, value);
      while (ok && (child = g_variant_iter_next_value (&iter)))
        {
          ok = g_settings_schema_key_range_check (key, child);
          g_variant_unref (child);
        }

      return ok;
    }

  if (key->minimum)
    {
      return g_variant_compare (key->minimum, value) <= 0 &&
             g_variant_compare (value, key->maximum) <= 0;
    }

  return strinfo_is_string_valid (key->strinfo, key->strinfo_length,
                                  g_variant_get_string (value, NULL));
}

GVariant *
g_settings_schema_key_range_fixup (GSettingsSchemaKey *key,
                                   GVariant           *value)
{
  const gchar *target;

  if (g_settings_schema_key_range_check (key, value))
    return g_variant_ref (value);

  if (key->strinfo == NULL)
    return NULL;

  if (g_variant_is_container (value))
    {
      GVariantBuilder builder;
      GVariantIter iter;
      GVariant *child;

      g_variant_iter_init (&iter, value);
      g_variant_builder_init (&builder, g_variant_get_type (value));

      while ((child = g_variant_iter_next_value (&iter)))
        {
          GVariant *fixed;

          fixed = g_settings_schema_key_range_fixup (key, child);
          g_variant_unref (child);

          if (fixed == NULL)
            {
              g_variant_builder_clear (&builder);
              return NULL;
            }

          g_variant_builder_add_value (&builder, fixed);
          g_variant_unref (fixed);
        }

      return g_variant_ref_sink (g_variant_builder_end (&builder));
    }

  target = strinfo_string_from_alias (key->strinfo, key->strinfo_length,
                                      g_variant_get_string (value, NULL));
  return target ? g_variant_ref_sink (g_variant_new_string (target)) : NULL;
}


GVariant *
g_settings_schema_key_get_translated_default (GSettingsSchemaKey *key)
{
  const gchar *translated;
  GError *error = NULL;
  const gchar *domain;
  GVariant *value;

  domain = g_settings_schema_get_gettext_domain (key->schema);

  if (key->lc_char == '\0')
    /* translation not requested for this key */
    return NULL;

  if (key->lc_char == 't')
    translated = g_dcgettext (domain, key->unparsed, LC_TIME);
  else
    translated = g_dgettext (domain, key->unparsed);

  if (translated == key->unparsed)
    /* the default value was not translated */
    return NULL;

  /* try to parse the translation of the unparsed default */
  value = g_variant_parse (key->type, translated, NULL, NULL, &error);

  if (value == NULL)
    {
      g_warning ("Failed to parse translated string `%s' for "
                 "key `%s' in schema `%s': %s", key->unparsed, key->name,
                 g_settings_schema_get_name (key->schema), error->message);
      g_warning ("Using untranslated default instead.");
      g_error_free (error);
    }

  else if (!g_settings_schema_key_range_check (key, value))
    {
      g_warning ("Translated default `%s' for key `%s' in schema `%s' "
                 "is outside of valid range", key->unparsed, key->name,
                 g_settings_schema_get_name (key->schema));
      g_variant_unref (value);
      value = NULL;
    }

  return value;
}

gint
g_settings_schema_key_to_enum (GSettingsSchemaKey *key,
                               GVariant           *value)
{
  gboolean it_worked;
  guint result;

  it_worked = strinfo_enum_from_string (key->strinfo, key->strinfo_length,
                                        g_variant_get_string (value, NULL),
                                        &result);

  /* 'value' can only come from the backend after being filtered for validity,
   * from the translation after being filtered for validity, or from the schema
   * itself (which the schema compiler checks for validity).  If this assertion
   * fails then it's really a bug in GSettings or the schema compiler...
   */
  g_assert (it_worked);

  return result;
}

GVariant *
g_settings_schema_key_from_enum (GSettingsSchemaKey *key,
                                 gint                value)
{
  const gchar *string;

  string = strinfo_string_from_enum (key->strinfo, key->strinfo_length, value);

  if (string == NULL)
    return NULL;

  return g_variant_new_string (string);
}

guint
g_settings_schema_key_to_flags (GSettingsSchemaKey *key,
                                GVariant           *value)
{
  GVariantIter iter;
  const gchar *flag;
  guint result;

  result = 0;
  g_variant_iter_init (&iter, value);
  while (g_variant_iter_next (&iter, "&s", &flag))
    {
      gboolean it_worked;
      guint flag_value;

      it_worked = strinfo_enum_from_string (key->strinfo, key->strinfo_length, flag, &flag_value);
      /* as in g_settings_to_enum() */
      g_assert (it_worked);

      result |= flag_value;
    }

  return result;
}

GVariant *
g_settings_schema_key_from_flags (GSettingsSchemaKey *key,
                                  guint               value)
{
  GVariantBuilder builder;
  gint i;

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("as"));

  for (i = 0; i < 32; i++)
    if (value & (1u << i))
      {
        const gchar *string;

        string = strinfo_string_from_enum (key->strinfo, key->strinfo_length, 1u << i);

        if (string == NULL)
          {
            g_variant_builder_clear (&builder);
            return NULL;
          }

        g_variant_builder_add (&builder, "s", string);
      }

  return g_variant_builder_end (&builder);
}
