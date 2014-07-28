/*
 * Copyright © 2010 Codethink Limited
 * Copyright © 2013 Canonical Limited
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

#define _(x)(x)

#include <gio/gio.h>
#include "gvdb/gvdb-builder.h"
#include "strinfo.c"

/* Forward declarations {{{1 */
typedef struct _FileRef                                     FileRef;
typedef struct _Enum                                        Enum;
typedef struct _Schema                                      Schema;
typedef struct _Override                                    Override;
typedef struct _Key                                         Key;
typedef struct _Dir                                         Dir;

static Schema *         dir_resolve_schema              (Dir            *dir,
                                                         const gchar    *id,
                                                         const gchar    *detail,
                                                         const gchar    *purpose,
                                                         const gchar    *caller,
                                                         GError        **error);
static Enum *           dir_resolve_enum                (Dir            *dir,
                                                         const gchar    *id,
                                                         gboolean        is_flags,
                                                         const gchar    *for_key,
                                                         const gchar    *of_schema,
                                                         GError        **error);
static Enum *           schema_resolve_enum             (Schema         *schema,
                                                         const gchar    *id,
                                                         gboolean        is_flags,
                                                         const gchar    *for_key,
                                                         GError        **error);

static gboolean         dir_add_enum                    (Dir            *dir,
                                                         GMarkupReader  *reader,
                                                         const gchar    *id,
                                                         Enum           *enum_,
                                                         GError        **error);

static gboolean         dir_add_schema                  (Dir            *dir,
                                                         GMarkupReader  *reader,
                                                         const gchar    *id,
                                                         Schema         *schema,
                                                         GError        **error);

static gboolean         schema_add_key                  (Schema         *schema,
                                                         GMarkupReader  *reader,
                                                         const gchar    *name,
                                                         Key            *key,
                                                         GError        **error);

/* <enum> and <flags> {{{1 */

struct _Enum
{
  Dir          *dir;
  gchar        *id;
  gboolean      is_flags;
  GString      *strinfo;
};

static void
enum_free (gpointer data)
{
  Enum *enum_ = data;

  g_string_free (enum_->strinfo, TRUE);
  g_free (enum_->id);

  g_slice_free (Enum, enum_);
}

static gboolean
enum_parse_value (GMarkupReader  *reader,
                  GCancellable   *cancellable,
                  Enum           *enum_,
                  GError        **error)
{
  const gchar *nick, *valuestr;
  gint64 value;
  gchar *end;

  if (!g_markup_reader_collect_attributes (reader, error,
                                           G_MARKUP_COLLECT_STRING, "nick", &nick,
                                           G_MARKUP_COLLECT_STRING, "value", &valuestr,
                                           G_MARKUP_COLLECT_INVALID))
    return FALSE;

  if (nick[0] == '\0' || nick[1] == '\0')
    {
      g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                   "nick must be a minimum of 2 characters");
      return FALSE;
    }

  value = g_ascii_strtoll (valuestr, &end, 0);
  if (*end || enum_->is_flags ?
                (value > G_MAXUINT32 || value < 0) :
                (value > G_MAXINT32 || value < G_MININT32))
    {
      g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT, "invalid numeric value");
      return FALSE;
    }

  if (strinfo_builder_contains (enum_->strinfo, nick))
    {
      g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                   "<value nick='%s'/> already specified", nick);
      return FALSE;
    }

  if (strinfo_builder_contains_value (enum_->strinfo, value))
    {
      g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT, "value='%s' already specified", valuestr);
      return FALSE;
    }

  /* Silently drop the null case if it is mentioned.
   * It is properly denoted with an empty array.
   */
  if (enum_->is_flags && value == 0)
    return TRUE;

  if (enum_->is_flags && (value & (value - 1)))
    {
      g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT, "flags values must have at most 1 bit set");
      return FALSE;
    }

  /* Since we reject exact duplicates of value='' and we only allow one
   * bit to be set, it's not possible to have overlaps.
   *
   * If we loosen the one-bit-set restriction we need an overlap check.
   */

  strinfo_builder_append_item (enum_->strinfo, nick, value);

  return g_markup_reader_expect_end (reader, NULL, error);
}

static gboolean
enum_parse (GMarkupReader  *reader,
            GCancellable   *cancellable,
            Dir            *dir,
            GError        **error)
{
  Enum *enum_;

  enum_ = g_slice_new0 (Enum);
  enum_->is_flags = g_markup_reader_is_start_element (reader, "flags");
  enum_->strinfo = g_string_new (NULL);

  g_assert (enum_->is_flags || g_markup_reader_is_start_element (reader, "enum"));

  if (!g_markup_reader_collect_attributes (reader, error,
                                           G_MARKUP_COLLECT_STRDUP, "id", &enum_->id,
                                           G_MARKUP_COLLECT_INVALID))
    goto error;

  if (!g_markup_reader_collect_elements (reader, cancellable, enum_, error, "value", enum_parse_value, NULL))
    goto error;

  if (!dir_add_enum (dir, reader, enum_->id, enum_, error))
    goto error;

  return TRUE;

error:
  enum_free (enum_);

  return FALSE;
}

/* <key> {{{1 */

struct _Key
{
  Schema       *schema;
  const gchar  *name;

  gchar        *type_string;
  GVariantType *type;
  gchar        *enum_name;
  Enum         *enum_;
  gchar        *flags_name;
  Enum         *flags;

  gchar         l10n;
  gchar        *l10n_context;
  gchar        *default_text;
  GVariant     *default_value;

  GString      *strinfo;
  gboolean      is_enum;
  gboolean      is_flags;

  GVariant     *minimum;
  GVariant     *maximum;

  gboolean      has_choices;
  gboolean      has_aliases;
  gboolean      is_override;

  gboolean      checked;
  GVariant     *serialised;
};

static void
key_free (gpointer data)
{
  Key *key = data;

  g_slice_free (Key, key);
}

static gboolean
key_resolve (Key     *key,
             GError **error)
{
  if (key->enum_name)
    {
      key->enum_ = schema_resolve_enum (key->schema, key->enum_name, FALSE, key->name, error);
      if (key->enum_ == NULL)
        return FALSE;
    }

  if (key->flags_name)
    {
      key->flags = schema_resolve_enum (key->schema, key->flags_name, TRUE, key->name, error);
      if (key->flags == NULL)
        return FALSE;
    }

  return TRUE;
}

static gboolean
key_parse_default (GMarkupReader  *reader,
                   GCancellable   *cancellable,
                   Key            *key,
                   GError        **error)
{
  if (key->default_value)
    {
      g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT, "<default/> must be specified exactly once");
      return FALSE;
    }

  if (!g_markup_reader_collect_attributes (reader, error,
                                           G_MARKUP_COLLECT_STRDUP | G_MARKUP_COLLECT_OPTIONAL,
                                             "l10n", &key->l10n,
                                           G_MARKUP_COLLECT_STRDUP | G_MARKUP_COLLECT_OPTIONAL,
                                             "context", &key->l10n_context,
                                           G_MARKUP_COLLECT_INVALID))
    return FALSE;

  key->default_text = g_markup_reader_collect_text (reader, cancellable, error);

  return key->default_text != NULL;
}

static gboolean
key_parse_range (GMarkupReader  *reader,
                 GCancellable   *cancellable,
                 Key            *key,
                 GError        **error)
{
  const gchar *min_str, *max_str;
  const struct {
    const gchar  type;
    const gchar *min;
    const gchar *max;
  } table[] = {
    { 'y',                    "0",                  "255" },
    { 'n',               "-32768",                "32767" },
    { 'q',                    "0",                "65535" },
    { 'i',          "-2147483648",           "2147483647" },
    { 'u',                    "0",           "4294967295" },
    { 'x', "-9223372036854775808",  "9223372036854775807" },
    { 't',                    "0", "18446744073709551615" },
    { 'd',                 "-inf",                  "inf" },
  };
  gboolean type_ok = FALSE;
  gint i;

  if (!g_markup_reader_collect_attributes (reader, error,
                                           G_MARKUP_COLLECT_STRING | G_MARKUP_COLLECT_OPTIONAL, "min", &min_str,
                                           G_MARKUP_COLLECT_STRING | G_MARKUP_COLLECT_OPTIONAL, "max", &max_str,
                                           G_MARKUP_COLLECT_INVALID))
    return FALSE;

  if (key->minimum)
    {
      g_set_error_literal (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                           "<range/> already specified for this key");
      return FALSE;
    }

  for (i = 0; i < G_N_ELEMENTS (table); i++)
    if (*(char *) key->type == table[i].type)
      {
        min_str = min_str ? min_str : table[i].min;
        max_str = max_str ? max_str : table[i].max;
        type_ok = TRUE;
        break;
      }

  if (!type_ok)
    {
      g_set_error (error, G_MARKUP_ERROR,
                  G_MARKUP_ERROR_INVALID_CONTENT,
                  "<range> not allowed for keys of type '%s'", key->type_string);
      return FALSE;
    }

  key->minimum = g_variant_parse (key->type, min_str, NULL, NULL, error);
  if (key->minimum == NULL)
    return FALSE;

  key->maximum = g_variant_parse (key->type, max_str, NULL, NULL, error);
  if (key->maximum == NULL)
    return FALSE;

  if (g_variant_compare (key->minimum, key->maximum) > 0)
    {
      g_set_error (error, G_MARKUP_ERROR,
                   G_MARKUP_ERROR_INVALID_CONTENT,
                   "<range> specified minimum is greater than maxmimum");
      return FALSE;
    }

  return g_markup_reader_expect_end (reader, cancellable, error);
}

static gboolean
key_parse_choice (GMarkupReader  *reader,
                  GCancellable   *cancellable,
                  Key            *key,
                  GError        **error)
{
  const gchar *value;

  if (!g_markup_reader_collect_attributes (reader, error,
                                           G_MARKUP_COLLECT_STRING, "value", &value,
                                           G_MARKUP_COLLECT_INVALID))
    return FALSE;

  if (strinfo_builder_contains (key->strinfo, value))
    {
      g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                   "<choice value='%s'/> already given", value);
      return FALSE;
    }

  strinfo_builder_append_item (key->strinfo, value, 0);
  key->has_choices = TRUE;

  return g_markup_reader_expect_end (reader, cancellable, error);
}

static gboolean
key_parse_choices (GMarkupReader  *reader,
                   GCancellable   *cancellable,
                   Key            *key,
                   GError        **error)
{
  if (!g_markup_reader_collect_attributes (reader, error, G_MARKUP_COLLECT_INVALID, NULL))
    return FALSE;

  if (!key->type_string || (!g_str_equal (key->type_string, "s") &&
                            !g_str_equal (key->type_string, "as") &&
                            !g_str_equal (key->type_string, "ms")))
    {
      g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                   "<choices> only allowed for keys with type 's', 'as' or 'ms'");
      return FALSE;
    }

  key->strinfo = g_string_new (NULL);

  if (!g_markup_reader_collect_elements (reader, cancellable, key, error, "choice", key_parse_choice, NULL))
    return FALSE;

  if (!key->has_choices)
    {
      g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                   "<choices> must contain at least one <choice>");
      return FALSE;
    }

  return TRUE;
}

static gboolean
key_parse_aliases (GMarkupReader  *reader,
                   GCancellable   *cancellable,
                   Key            *key,
                   GError        **error)
{
  return TRUE;
}

static gboolean
ignore_text (GMarkupReader  *reader,
             GCancellable   *cancellable,
             gpointer        user_data,
             GError        **error)
{
  while (g_markup_reader_advance (reader, cancellable, error) && g_markup_reader_is_text (reader))
    ;

  if (g_markup_reader_is_end_element (reader))
    return TRUE;

  g_markup_reader_unexpected (reader, error);

  return FALSE;
}

static gboolean
key_parse (GMarkupReader  *reader,
           GCancellable   *cancellable,
           Schema         *schema,
           GError        **error)
{
  Key *key;

  g_assert (g_markup_reader_is_start_element (reader, "key"));

  key = g_slice_new0 (Key);
  key->schema = schema;

  if (!g_markup_reader_collect_attributes (reader, error,
                                           G_MARKUP_COLLECT_STRDUP,
                                             "name", &key->name,
                                           G_MARKUP_COLLECT_STRDUP | G_MARKUP_COLLECT_OPTIONAL,
                                             "type", &key->type_string,
                                           G_MARKUP_COLLECT_STRDUP | G_MARKUP_COLLECT_OPTIONAL,
                                             "enum", &key->enum_name,
                                           G_MARKUP_COLLECT_STRDUP | G_MARKUP_COLLECT_OPTIONAL,
                                             "flags", &key->flags_name,
                                           G_MARKUP_COLLECT_INVALID))
    goto error;

  if ((key->type_string != NULL) + (key->enum_name != NULL) + (key->flags_name != NULL) != 1)
    {
      g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_MISSING_ATTRIBUTE,
                   _("exactly one of 'type', 'enum' or 'flags' must be specified as an attribute to <key>"));
      return FALSE;
    }

  if (key->type_string)
    {
      if (!g_variant_type_string_is_valid (key->type_string))
        {
          g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                       _("invalid GVariant type string '%s'"), key->type_string);
          return FALSE;
        }

      key->type = g_variant_type_new (key->type_string);
    }
  else if (key->enum_name)
    key->type = g_variant_type_copy (G_VARIANT_TYPE_STRING);
  else /* flags */
    key->type = g_variant_type_copy (G_VARIANT_TYPE_STRING_ARRAY);

  if (!g_markup_reader_collect_elements (reader, cancellable, key, error,
                                         "summary",     ignore_text,
                                         "description", ignore_text,
                                         "default",     key_parse_default,
                                         "range",       key_parse_range,
                                         "choices",     key_parse_choices,
                                         "aliases",     key_parse_aliases,
                                         NULL))
    goto error;

  if (!schema_add_key (schema, reader, key->name, key, error))
    goto error;

  return TRUE;

error:
  return FALSE;
}

/* <schema> {{{1 */

struct _Override
{
  Schema       *schema;
  gchar        *name;

  gchar        *text;
  gchar        *context;
  gchar        *l10n;
};

static void
override_free (gpointer data)
{
  Override *override = data;

  g_free (override->name);
  g_free (override->text);
  g_free (override->context);
  g_free (override->l10n);

  g_slice_free (Override, override);
}

struct _Schema
{
  Dir          *dir;
  gchar        *id;

  gboolean      has_translated;
  gboolean      resolved;

  gchar        *gettext_domain;
  gchar        *path;

  const gchar  *extends_name;
  Schema       *extends;
  const gchar  *list_of_name;
  Schema       *list_of;

  GHashTable   *children_names;
  GHashTable   *children;
  GHashTable   *keys;
  GHashTable   *overrides;
};

static void
schema_free (gpointer data)
{
  Schema *schema = data;

  g_slice_free (Schema, schema);
}

static gboolean
schema_resolve (Schema  *schema,
                GError **error)
{
  static GSList *now_resolving; /* we don't unwind this properly in case of error */
  GSList me = { schema };

  if (schema->resolved)
    return TRUE;

  if (g_slist_find (now_resolving, schema))
    {
      GSList *node;
      GString *str;

      /* We have a reference cycle. */
      str = g_string_new ("Reference cycle detected: '%s'");
      for (node = now_resolving; node; node = node->next)
        {
          Schema *s = node->data;

          g_string_append_printf (str, "<- '%s'", s->id);

          /* Stop once we get back to ourselves. */
          if (s == schema)
            break;
        }

      g_set_error_literal (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, str->str);
      g_string_free (str, TRUE);
      return FALSE;
    }

  me.next = now_resolving;
  now_resolving = &me;

  if (schema->extends_name)
    {
      schema->extends = dir_resolve_schema (schema->dir, schema->extends_name,
                                            "extends", "reference", schema->id,
                                            error);
      if (!schema->extends)
        return FALSE;
    }

  if (schema->list_of_name)
    {
      schema->list_of = dir_resolve_schema (schema->dir, schema->list_of_name,
                                            "list-of", "reference", schema->id,
                                            error);
      if (!schema->list_of)
        return FALSE;
    }

  if (schema->children_names)
    {
      GHashTableIter iter;
      gpointer name, id;

      schema->children = g_hash_table_new (g_str_hash, g_str_equal);

      g_hash_table_iter_init (&iter, schema->children_names);
      while (g_hash_table_iter_next (&iter, &name, &id))
        {
          Schema *child;

          child = dir_resolve_schema (schema->dir, id,
                                      name, "child", schema->id,
                                      error);
          if (!child)
            return FALSE;

          g_hash_table_insert (schema->children, name, child);
        }
    }

  if (schema->keys)
    {
      GHashTableIter iter;
      gpointer key;

      g_hash_table_iter_init (&iter, schema->keys);
      while (g_hash_table_iter_next (&iter, NULL, &key))
        if (!key_resolve (key, error))
          return FALSE;
    }

  now_resolving = me.next;

  return TRUE;
}

static Enum *
schema_resolve_enum (Schema       *schema,
                     const gchar  *id,
                     gboolean      is_flags,
                     const gchar  *for_key,
                     GError      **error)
{
  return dir_resolve_enum (schema->dir, id, is_flags, for_key, schema->id, error);
}

static gboolean
schema_add_key (Schema         *schema,
                GMarkupReader  *reader,
                const gchar    *name,
                Key            *key,
                GError        **error)
{
  if (g_hash_table_contains (schema->keys, name))
    {
      g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                   "<key name='%s'/> already defined in <schema id='%s'/>", name, schema->id);
      return FALSE;
    }

  g_hash_table_insert (schema->keys, g_strdup (name), key);

  return TRUE;
}

static gboolean
schema_parse_child (GMarkupReader  *reader,
                    GCancellable   *cancellable,
                    Schema         *schema,
                    GError        **error)
{
  const gchar *name, *schema_id;

  if (!g_markup_reader_collect_attributes (reader, error,
                                           G_MARKUP_COLLECT_STRING, "name", &name,
                                           G_MARKUP_COLLECT_STRING, "schema", &schema_id,
                                           G_MARKUP_COLLECT_INVALID))
    return FALSE;

  if (g_hash_table_contains (schema->children_names, name))
    {
      g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                   "<child name='%s'/> appears twice in <schema id='%s'/>",
                   name, schema->id);
      return FALSE;
    }

  g_hash_table_insert (schema->children_names, g_strdup (name), g_strdup (schema_id));

  if (!g_markup_reader_expect_end (reader, cancellable, error))
    return FALSE;

  return TRUE;
}

static gboolean
schema_parse_override (GMarkupReader  *reader,
                       GCancellable   *cancellable,
                       Schema         *schema,
                       GError        **error)
{
  Override *override;

  override = g_slice_new0 (Override);
  override->schema = schema;

  if (!g_markup_reader_collect_attributes (reader, error,
                                           G_MARKUP_COLLECT_STRING, "name", &override->name,
                                           G_MARKUP_COLLECT_STRING, "l10n", &override->l10n,
                                           G_MARKUP_COLLECT_STRING, "context", &override->context,
                                           G_MARKUP_COLLECT_INVALID))
    goto error;

  if (schema->overrides == NULL)
    {
      g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                   "<override name='%s'/> appears within <schema id='%s'/> that is not extending another",
                   override->name, schema->id);
      goto error;
    }

  if (g_hash_table_contains (schema->overrides, override->name))
    {
      g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                   "<override name='%s'/> appears twice in <schema id='%s'/>", override->name, schema->id);
      return FALSE;
    }

  override->text = g_markup_reader_collect_text (reader, cancellable, error);
  if (!override->text)
    goto error;

  g_hash_table_insert (schema->overrides, g_strdup (override->name), override);

  return TRUE;

error:
  override_free (override);
  return FALSE;
}

static gboolean
schema_parse (GMarkupReader  *reader,
              GCancellable   *cancellable,
              Dir            *dir,
              GError        **error)
{
  Schema *schema;

  g_assert (g_markup_reader_is_start_element (reader, "schema"));

  schema = g_slice_new0 (Schema);
  schema->dir = dir;
  schema->keys = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, key_free);
  schema->children_names = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

  if (!g_markup_reader_collect_attributes (reader, error,
                                           G_MARKUP_COLLECT_STRDUP,
                                             "id", &schema->id,
                                           G_MARKUP_COLLECT_STRDUP | G_MARKUP_COLLECT_OPTIONAL,
                                             "path", &schema->path,
                                           G_MARKUP_COLLECT_STRDUP | G_MARKUP_COLLECT_OPTIONAL,
                                             "gettext-domain", &schema->gettext_domain,
                                           G_MARKUP_COLLECT_STRDUP | G_MARKUP_COLLECT_OPTIONAL,
                                             "extends", &schema->extends_name,
                                           G_MARKUP_COLLECT_STRDUP | G_MARKUP_COLLECT_OPTIONAL,
                                             "list-of", &schema->list_of_name,
                                           G_MARKUP_COLLECT_INVALID))
    goto error;

  if (schema->extends_name)
    schema->overrides = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, override_free);

  if (!g_markup_reader_collect_elements (reader, cancellable, schema, error,
                                         "key",      key_parse,
                                         "child",    schema_parse_child,
                                         "override", schema_parse_override,
                                         NULL))
    goto error;

  if (!dir_add_schema (dir, reader, schema->id, schema, error))
    goto error;

  return TRUE;

error:
  return FALSE;
}

/* Directory handling {{{1 */

struct _Dir
{
  Dir          *parent_dir;
  const gchar  *path;
  GHashTable   *excludes;
  gboolean      parsed;

  /* temporarily set while parsing <schemalist> */
  gchar        *gettext_domain;

  GHashTable   *schemas;
  GHashTable   *enums;
};

static gboolean
dir_add_enum (Dir            *dir,
              GMarkupReader  *reader,
              const gchar    *id,
              Enum           *enum_,
              GError        **error)
{
  if (g_hash_table_contains (dir->enums, id))
    {
      g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                   "<enum id='%s'/> or <flags id='%s'/> already defined in directory %s",
                   id, id, dir->path);
      return FALSE;
    }

  g_hash_table_insert (dir->enums, g_strdup (id), enum_);

  return TRUE;
}

static gboolean
dir_add_schema (Dir            *dir,
                GMarkupReader  *reader,
                const gchar    *id,
                Schema         *schema,
                GError        **error)
{
  if (g_hash_table_contains (dir->schemas, id))
    {
      g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                   "<schema id='%s'/> already defined in directory %s", id, dir->path);
      return FALSE;
    }

  g_hash_table_insert (dir->schemas, g_strdup (id), schema);

  return TRUE;
}

static gboolean
dir_parse_schemalist (GMarkupReader  *reader,
                      GCancellable   *cancellable,
                      Dir            *dir,
                      GError        **error)
{
  gboolean ok;

  if (!g_markup_reader_collect_attributes (reader, error,
                                           G_MARKUP_COLLECT_STRDUP | G_MARKUP_COLLECT_OPTIONAL,
                                             "gettext-domain", &dir->gettext_domain,
                                           G_MARKUP_COLLECT_INVALID))
    return FALSE;

  ok = g_markup_reader_collect_elements (reader, cancellable, dir, error,
                                         "schema", schema_parse, "enum", enum_parse, "flags", enum_parse, NULL);

  g_free (dir->gettext_domain);
  dir->gettext_domain = NULL;

  return ok;
}

static gboolean
dir_parse_one_file (Dir          *dir,
                    const gchar  *filename,
                    GCancellable *cancellable,
                    GError      **error)
{
  GFileInputStream *stream;
  GMarkupReader *reader;
  GFile *file;
  gboolean ok;

  file = g_file_new_for_path (filename);
  stream = g_file_read (file, cancellable, error);
  g_object_unref (file);

  if (!stream)
    return FALSE;

  reader = g_markup_reader_new (G_INPUT_STREAM (stream),
                                G_MARKUP_PREFIX_ERROR_POSITION |
                                G_MARKUP_TREAT_CDATA_AS_TEXT |
                                G_MARKUP_IGNORE_QUALIFIED |
                                G_MARKUP_IGNORE_PASSTHROUGH);
  g_object_unref (stream);

  ok = g_markup_reader_collect_elements (reader, cancellable, dir, error, "schemalist", dir_parse_schemalist, NULL);

  g_object_unref (reader);

  if (!ok)
    g_prefix_error (error, "%s: ", filename);

  return ok;
}

static Dir *compile_dir;

static gboolean
dir_parse (Dir           *dir,
           GCancellable  *cancellable,
           GError       **error)
{
  GError *local_error = NULL;
  gboolean ok = TRUE;
  const gchar *name;
  GDir *dirp;

  g_assert (!dir->parsed);

  dirp = g_dir_open (dir->path, 0, &local_error);

  dir->schemas = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, schema_free);
  dir->enums = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, enum_free);

  if (!dirp)
    {
      /* If we get a G_FILE_ERROR_NOENT then ignore it, unless it was
       * for the toplevel directory (ie: the one we were asked to
       * compile).
       */
      if (g_error_matches (local_error, G_FILE_ERROR, G_FILE_ERROR_NOENT) && dir != compile_dir)
        {
          g_error_free (local_error);
          return TRUE;
        }

      g_propagate_error (error, local_error);
      return FALSE;
    }

  while (ok && (name = g_dir_read_name (dirp)))
    {
      gchar *fullname;

      if (!g_str_has_suffix (name, ".xml"))
        continue;

      if (dir->excludes && g_hash_table_contains (dir->excludes, name))
        continue;

      fullname = g_build_filename (dir->path, name, NULL);
      ok = dir_parse_one_file (dir, fullname, cancellable, error);
      g_free (fullname);
    }

  g_dir_close (dirp);

  dir->parsed = ok;

  return ok;
}

static Dir *
dir_new (const gchar *path,
         Dir         *parent_dir)
{
  Dir *dir;

  dir = g_slice_new0 (Dir);
  dir->path = path;
  dir->parent_dir = parent_dir;

  return dir;
}

static void
setup_compile_dir (const gchar         *directory,
                   const gchar * const *excluded)
{
  const gchar * const * system_dirs;
  gint i, n;

  /* If we are compiling a system directory we want to include all of
   * the directories that come before it.
   *
   * If we are compiling a non-system directory then we want to include
   * all of the system directories before it.
   *
   * For example, of XDG_DATA_DIRS=/a:/b:/c then:
   *
   *  - for building /a, we want a path of /a, /b, /c
   *
   *  - for building /b, we want a path of /b, /c
   *
   *  - for building /c, we want a path of /c
   *
   *  - for building /x we want a path of /x, /a, /b, /c
   */
  system_dirs = g_get_system_data_dirs ();
  n = g_strv_length ((gchar **) system_dirs);

  /* We're building a linked list -- start at the end */
  for (i = n - 1; i >= 0; i--)
    {
      /* If we see our own directory then stop -- we don't want any more
       * system dirs.  We will deal with our directory below.
       */
      if (g_str_equal (system_dirs[i], directory))
        break;

      compile_dir = dir_new (system_dirs[i], compile_dir);
    }

  compile_dir = dir_new (directory, compile_dir);
}

Schema *
dir_resolve_schema (Dir          *dir,
                    const gchar  *id,
                    const gchar  *detail,
                    const gchar  *purpose,
                    const gchar  *caller,
                    GError      **error)
{
  Schema *schema = NULL;
  Dir *d;

  g_assert (dir);

  for (d = dir; d; d = d->parent_dir)
    {
      if (!dir->parsed)
        if (!dir_parse (dir, NULL, error))
          return FALSE;

      schema = g_hash_table_lookup (dir->schemas, id);

      if (schema)
        break;
    }

  if (!schema)
    {
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                   "Unable to locate schema '%s' needed for '%s' %s of %s",
                   id, detail, purpose, caller);
      return NULL;
    }

  if (!schema_resolve (schema, error))
    return NULL;

  return schema;
}

Enum *
dir_resolve_enum (Dir          *dir,
                  const gchar  *id,
                  gboolean      is_flags,
                  const gchar  *for_key,
                  const gchar  *of_schema,
                  GError      **error)
{
  Enum *enum_ = NULL;
  Dir *d;

  g_assert (dir);

  for (d = dir; d; d = d->parent_dir)
    {
      if (!dir->parsed)
        if (!dir_parse (dir, NULL, error))
          return FALSE;

      enum_ = g_hash_table_lookup (dir->enums, id);

      if (enum_)
        break;
    }

  if (!enum_ || enum_->is_flags != is_flags)
    {
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                   "Unable to locate <%s id='%s'/> needed for key '%s' of schema '%s'",
                   is_flags ? "flags" : "enum", id, for_key, of_schema);
      return NULL;
    }

  return enum_;
}

static gboolean
dir_resolve (Dir     *dir,
             GError **error)
{
  GHashTableIter iter;
  gpointer schema;

  if (!dir_parse (dir, NULL, error))
    return FALSE;

  g_hash_table_iter_init (&iter, dir->schemas);
  while (g_hash_table_iter_next (&iter, NULL, &schema))
    if (!schema_resolve (schema, error))
      return FALSE;

  return TRUE;
}

static GVariant *
key_compile (Key     *key,
             GError **error)
{
  GVariantBuilder builder;

  g_variant_builder_init (&builder, G_VARIANT_TYPE_TUPLE);

  return g_variant_builder_end (&builder);
}

static GHashTable *
schema_compile (Schema  *schema,
                GError **error)
{
  GHashTable *compiled_schema;
  GvdbItem *root_item;
  GHashTableIter iter;
  gpointer key, name;
  gpointer id;

  compiled_schema = gvdb_hash_table_new (NULL, NULL);
  root_item = gvdb_hash_table_insert (compiled_schema, "");

  if (schema->path)
    gvdb_hash_table_insert_string (compiled_schema, ".path", schema->path);

  if (schema->list_of)
    gvdb_hash_table_insert_string (compiled_schema, ".list-of", schema->list_of_name);

  if (schema->extends)
    gvdb_hash_table_insert_string (compiled_schema, ".extends", schema->extends_name);

  /* Only store the gettext domain if a key was actually translated */
  if (schema->has_translated)
    gvdb_hash_table_insert_string (compiled_schema, ".gettext-domain", schema->gettext_domain);

  g_hash_table_iter_init (&iter, schema->keys);
  while (g_hash_table_iter_next (&iter, &name, &key))
    {
      GVariant *compiled_key;
      GvdbItem *key_item;

      compiled_key = key_compile (key, error);
      if (!compiled_key)
        goto error;

      key_item = gvdb_hash_table_insert (compiled_schema, name);
      gvdb_item_set_parent (key_item, root_item);
      gvdb_item_set_value (key_item, compiled_key);
    }

  g_hash_table_iter_init (&iter, schema->children);
  while (g_hash_table_iter_next (&iter, &name, &id))
    {
      GvdbItem *child_item;

      child_item = gvdb_hash_table_insert (compiled_schema, name);
      gvdb_item_set_parent (child_item, root_item);
      gvdb_item_set_value (child_item, g_variant_new_string (id));
    }

  return compiled_schema;
error:
  g_assert_not_reached ();
}

static GHashTable *
dir_compile (Dir     *dir,
             GError **error)
{
  GHashTable *compiled_dir;
  GvdbItem *root_item;
  GHashTableIter iter;
  gpointer schema, id;

  compiled_dir = gvdb_hash_table_new (NULL, NULL);
  root_item = gvdb_hash_table_insert (compiled_dir, "");

  g_hash_table_iter_init (&iter, dir->schemas);
  while (g_hash_table_iter_next (&iter, &id, &schema))
    {
      GHashTable *compiled_schema;
      GvdbItem *schema_item;

      compiled_schema = schema_compile (schema, error);
      if (!compiled_schema)
        goto error;

      schema_item = gvdb_hash_table_insert (compiled_dir, id);
      gvdb_item_set_parent (schema_item, root_item);
      gvdb_item_set_hash_table (schema_item, compiled_schema);
    }

  return compiled_dir;
error:
  g_assert_not_reached ();
}

int
main (void)
{
  GError *error = NULL;
  setup_compile_dir ("/home/desrt/.cache/jhbuild/install/share/glib-2.0/schemas", NULL);
  if (!dir_resolve (compile_dir, &error))
    g_printerr ("%s\n", error->message);
  return error ? 1 : 0;
}

/* Epilogue {{{1 */

/* vim:set foldmethod=marker: */

