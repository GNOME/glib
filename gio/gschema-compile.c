#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <glob.h>

#include "gvdb/gvdb-builder.h"

typedef struct
{
  GHashTable *schemas;
  gchar *schemalist_domain;

  GHashTable *schema;
  gchar *schema_domain;

  GString *string;

  GvdbItem *key;
  GVariant *value;
  GVariant *min, *max;
  GVariant *strings;
  gchar *l10n;
  GVariantType *type;
} ParseState;

static void
start_element (GMarkupParseContext  *context,
               const gchar          *element_name,
               const gchar         **attribute_names,
               const gchar         **attribute_values,
               gpointer              user_data,
               GError              **error)
{
  ParseState *state = user_data;
  const GSList *element_stack;
  const gchar *container;

  element_stack = g_markup_parse_context_get_element_stack (context);
  container = element_stack->next ? element_stack->next->data : NULL;

#define COLLECT(first, ...) \
  g_markup_collect_attributes (element_name,                                 \
                               attribute_names, attribute_values, error,     \
                               first, __VA_ARGS__, G_MARKUP_COLLECT_INVALID)
#define OPTIONAL   G_MARKUP_COLLECT_OPTIONAL
#define STRDUP     G_MARKUP_COLLECT_STRDUP
#define STRING     G_MARKUP_COLLECT_STRING
#define NO_ARGS()  COLLECT (G_MARKUP_COLLECT_INVALID, NULL)

  if (container == NULL)
    {
      if (strcmp (element_name, "schemalist") == 0)
        {
          COLLECT (OPTIONAL | STRDUP,
                   "gettext-domain",
                   &state->schemalist_domain);
          return;
        }
    }
  else if (strcmp (container, "schemalist") == 0)
    {
      if (strcmp (element_name, "schema") == 0)
        {
          const gchar *id, *path;

          if (COLLECT (STRING, "id", &id,
                       OPTIONAL | STRING, "path", &path,
                       OPTIONAL | STRDUP, "gettext-domain",
                                          &state->schema_domain))
            {
              if (!g_hash_table_lookup (state->schemas, id))
                {
                  state->schema = gvdb_hash_table_new (state->schemas, id);

                  if (path != NULL)
                    gvdb_hash_table_insert_string (state->schema,
                                                   ".path", path);
                }
              else
                g_set_error (error, G_MARKUP_ERROR,
                             G_MARKUP_ERROR_INVALID_CONTENT,
                             "<schema id='%s'> already specified", id);
            }
          return;
        }
    }
  else if (strcmp (container, "schema") == 0)
    {
      if (strcmp (element_name, "key") == 0)
        {
          const gchar *name, *type;

          if (COLLECT (STRING, "name", &name, STRING, "type", &type))
            {
              if (!g_hash_table_lookup (state->schema, name))
                state->key = gvdb_hash_table_insert (state->schema, name);

              else
                g_set_error (error, G_MARKUP_ERROR,
                             G_MARKUP_ERROR_INVALID_CONTENT,
                             "<key name='%s'> already specified", name);

              if (g_variant_type_string_is_valid (type))
                state->type = g_variant_type_new (type);

              else
                g_set_error (error, G_MARKUP_ERROR,
                             G_MARKUP_ERROR_INVALID_CONTENT,
                             "invalid GVariant type string");
            }

          return;
        }
    }
  else if (strcmp (container, "key") == 0)
    {
      if (strcmp (element_name, "default") == 0)
        {
          if (COLLECT (STRDUP | OPTIONAL, "l10n", &state->l10n))
            {
              if (state->l10n != NULL)
                {
                  if (!g_hash_table_lookup (state->schema, ".gettext-domain"))
                    {   
                      const gchar *domain = state->schema_domain ?
                                            state->schema_domain :
                                            state->schemalist_domain;

                      if (domain == NULL)
                        {
                          g_set_error (error, G_MARKUP_ERROR,
                                       G_MARKUP_ERROR_INVALID_CONTENT,
                                       "l10n requested, but no "
                                       "gettext domain given");
                          return;
                        }

                      gvdb_hash_table_insert_string (state->schema,
			                             ".gettext-domain",
						     domain);

                      /* XXX: todo: check l10n category validity */
                    }
                }

              state->string = g_string_new (NULL);
            }

          return;
        }
      else if (strcmp (element_name, "summary") == 0)
        {
          NO_ARGS ();
          return;
        }
      else if (strcmp (element_name, "description") == 0)
        {
          NO_ARGS ();
          return;
        }
      else if (strcmp (element_name, "range") == 0)
        {
          NO_ARGS ();
          return;
        }
    }
  else if (strcmp (container, "range") == 0)
    {
      if (strcmp (element_name, "choice") == 0)
        {
          gchar *value;

          if (COLLECT (STRDUP, "value", &value))
            {
            }

          return;
        }
      else if (strcmp (element_name, "min") == 0)
        {
          NO_ARGS ();
          return;
        }
      else if (strcmp (element_name, "max") == 0)
        {
          NO_ARGS ();
          return;
        }
    }
  else if (strcmp (container, "choice") == 0)
    {
    }

  if (container)
    g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ELEMENT, 
                 "Element <%s> not allowed inside <%s>\n",
                 element_name, container);
  else
    g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ELEMENT, 
                 "Element <%s> not allowed at toplevel\n", element_name);
}

static void
end_element (GMarkupParseContext  *context,
             const gchar          *element_name,
             gpointer              user_data,
             GError              **error)
{
  ParseState *state = user_data;

  if (strcmp (element_name, "default") == 0)
    {
      state->value = g_variant_parse (state->type, state->string->str,
                                      state->string->str + state->string->len,
                                      NULL, error);
    }

  else if (strcmp (element_name, "key") == 0)
    {
      gvdb_item_set_value (state->key, state->value);
    }
}

static void
text (GMarkupParseContext  *context,
      const gchar          *text,
      gsize                 text_len,
      gpointer              user_data,
      GError              **error)
{
  ParseState *state = user_data;
  gsize i;

  for (i = 0; i < text_len; i++)
    if (!g_ascii_isspace (text[i]))
      {
        if (state->string)
          g_string_append_len (state->string, text, text_len);

        else
          g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                       "text may not appear inside <%s>\n",
                       g_markup_parse_context_get_element (context));

        break;
      }
}

static GHashTable *
parse_gschema_files (gchar  **files,
                     GError **error)
{
  GMarkupParser parser = { start_element, end_element, text };
  GMarkupParseContext *context;
  ParseState state = { 0, };
  const gchar *filename;

  context = g_markup_parse_context_new (&parser,
                                        G_MARKUP_PREFIX_ERROR_POSITION,
                                        &state, NULL);
  state.schemas = gvdb_hash_table_new (NULL, NULL);

  while ((filename = *files++))
    {
      gchar *contents;
      gsize size;

      if (!g_file_get_contents (filename, &contents, &size, error))
        return FALSE;

      if (!g_markup_parse_context_parse (context, contents, size, error))
        {
          g_prefix_error (error, "%s: ", filename);
          return FALSE;
        }

      if (!g_markup_parse_context_end_parse (context, error))
        {
          g_prefix_error (error, "%s: ", filename);
          return FALSE;
        }
    }

  return state.schemas;
}

int
main (int argc, char **argv)
{
  GError *error = NULL;
  GHashTable *table;
  glob_t matched;
  gint status;

  if (argc != 2)
    {
      fprintf (stderr, "you should give exactly one directory name\n");
      return 1;
    }

  if (chdir (argv[1]))
    {
      perror ("chdir");
      return 1;
    }

  status = glob ("*.gschema", 0, NULL, &matched);

  if (status == GLOB_ABORTED)
    {
      perror ("glob");
      return 1;
    }
  else if (status == GLOB_NOMATCH)
    {
      fprintf (stderr, "no schema files found\n");
      return 1;
    }
  else if (status != 0)
    {
      fprintf (stderr, "unknown glob error\n");
      return 1;
    }

  if (!(table = parse_gschema_files (matched.gl_pathv, &error)) ||
      !gvdb_table_write_contents (table, "compiled", &error))
    {
      fprintf (stderr, "%s\n", error->message);
      return 1;
    }

  return 0;
}
