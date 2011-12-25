/*
 * Copyright © 2011 Canonical Ltd.
 * All rights reserved.
 *
 * Author: Ryan Lortie <desrt@desrt.ca>
 */

#include "gmenumarkup.h"

#include <gi18n.h>

/**
 * SECTION:gmenumarkup
 * @title: GMenu Markup
 * @short_description: parsing and printing GMenuModel XML
 *
 * The functions here allow to instantiate #GMenuModels by parsing
 * fragments of an XML document.
 * * The XML format for #GMenuModel consists of a toplevel
 * <tag class="starttag">menu</tag> element, which contains one or more
 * <tag class="starttag">item</tag> elements. Each <tag class="starttag">item</tag>
 * element contains <tag class="starttag">attribute</tag> and <tag class="starttag">link</tag>
 * elements with a mandatory name attribute.
 * <tag class="starttag">link</tag> elements have the same content
 * model as <tag class="starttag">menu</tag>.
 *
 * Here is the XML for <xref linkend="menu-example"/>:
 * |[<xi:include xmlns:xi="http://www.w3.org/2001/XInclude" parse="text" href="../../../../gio/menumarkup2.xml"><xi:fallback>FIXME: MISSING XINCLUDE CONTENT</xi:fallback></xi:include>]|
 *
 * The parser also understands a somewhat less verbose format, in which
 * attributes are encoded as actual XML attributes of <tag class="starttag">item</tag>
 * elements, and <tag class="starttag">link</tag> elements are replaced by
 * <tag class="starttag">section</tag> and <tag class="starttag">submenu</tag> elements.
 *
 * Here is how the example looks in this format:
 * |[<xi:include xmlns:xi="http://www.w3.org/2001/XInclude" parse="text" href="../../../../gio/menumarkup.xml"><xi:fallback>FIXME: MISSING XINCLUDE CONTENT</xi:fallback></xi:include>]|
 *
 * The parser can obtaing translations for attribute values using gettext.
 * To make use of this, the <tag class="starttag">menu</tag> element must
 * have a domain attribute which specifies the gettext domain to use, and
 * <tag class="starttag">attribute</tag> elements can be marked for translation
 * with a <literal>translatable="yes"</literal> attribute. It is also possible
 * to specify message context and translator comments, using the context
 * and comments attributes.
 *
 * The following DTD describes the XML format approximately:
 * |[<xi:include xmlns:xi="http://www.w3.org/2001/XInclude" parse="text" href="../../../../gio/menumarkup.dtd"><xi:fallback>FIXME: MISSING XINCLUDE CONTENT</xi:fallback></xi:include>]|
 *
 * To serialize a #GMenuModel into an XML fragment, use
 * g_menu_markup_print_string().
 */

struct frame
{
  GMenu        *menu;
  GMenuItem    *item;
  struct frame *prev;
};

typedef struct
{
  GHashTable *objects;
  struct frame frame;

  /* attributes */
  GQuark        attribute;
  GVariantType *type;
  GString      *string;

  /* translation */
  gchar        *domain;
  gchar        *context;
  gboolean      translatable;
} GMenuMarkupState;

static void
g_menu_markup_push_frame (GMenuMarkupState *state,
                          GMenu            *menu,
                          GMenuItem        *item)
{
  struct frame *new;

  new = g_slice_new (struct frame);
  *new = state->frame;

  state->frame.menu = menu;
  state->frame.item = item;
  state->frame.prev = new;
}

static void
g_menu_markup_pop_frame (GMenuMarkupState *state)
{
  struct frame *prev = state->frame.prev;

  if (state->frame.item)
    {
      g_assert (prev->menu != NULL);
      g_menu_append_item (prev->menu, state->frame.item);
      g_object_unref (state->frame.item);
    }

  state->frame = *prev;

  g_slice_free (struct frame, prev);
}

static void
add_string_attributes (GMenuItem    *item,
                       const gchar **names,
                       const gchar **values)
{
  gint i;

  for (i = 0; names[i]; i++)
    {
      g_menu_item_set_attribute (item, names[i], "s", values[i]);
    }
}

static gboolean
find_id_attribute (const gchar **names,
                   const gchar **values,
                   const gchar **id)
{
  gint i;

  for (i = 0; names[i]; i++)
    {
      if (strcmp (names[i], "id") == 0)
        {
          *id = values[i];
          return TRUE;
        }
    }

  return FALSE;
}

static void
g_menu_markup_start_element (GMarkupParseContext  *context,
                             const gchar          *element_name,
                             const gchar         **attribute_names,
                             const gchar         **attribute_values,
                             gpointer              user_data,
                             GError              **error)
{
  GMenuMarkupState *state = user_data;

#define COLLECT(first, ...) \
  g_markup_collect_attributes (element_name,                                 \
                               attribute_names, attribute_values, error,     \
                               first, __VA_ARGS__, G_MARKUP_COLLECT_INVALID)
#define OPTIONAL   G_MARKUP_COLLECT_OPTIONAL
#define BOOLEAN    G_MARKUP_COLLECT_BOOLEAN
#define STRING     G_MARKUP_COLLECT_STRING

  if (!(state->frame.menu || state->frame.item || state->string))
    {
      /* Can only have <menu> here. */
      if (g_str_equal (element_name, "menu"))
        {
          gchar *id;

          if (COLLECT (STRING, "id", &id))
            {
              GMenu *menu;

              menu = g_menu_new ();
              if (state->objects)
                g_hash_table_insert (state->objects, g_strdup (id), menu);
              g_menu_markup_push_frame (state, menu, NULL);
            }

          return;
        }
    }

  if (state->frame.menu)
    {
      /* Can have '<item>', '<submenu>' or '<section>' here. */
      if (g_str_equal (element_name, "item"))
        {
          GMenuItem *item;

          item = g_menu_item_new (NULL, NULL);
          add_string_attributes (item, attribute_names, attribute_values);
          g_menu_markup_push_frame (state, NULL, item);
          return;
        }

      else if (g_str_equal (element_name, "submenu"))
        {
          GMenuItem *item;
          GMenu *menu;
          gchar *id;

          menu = g_menu_new ();
          item = g_menu_item_new_submenu (NULL, G_MENU_MODEL (menu));
          add_string_attributes (item, attribute_names, attribute_values);
          g_menu_markup_push_frame (state, menu, item);

          if (find_id_attribute (attribute_names, attribute_values, &id))
            {
              if (state->objects)
                g_hash_table_insert (state->objects, g_strdup (id), g_object_ref (menu));
            }
          g_object_unref (menu);

          return;
        }

      else if (g_str_equal (element_name, "section"))
        {
          GMenuItem *item;
          GMenu *menu;
          gchar *id;

          menu = g_menu_new ();
          item = g_menu_item_new_section (NULL, G_MENU_MODEL (menu));
          add_string_attributes (item, attribute_names, attribute_values);
          g_menu_markup_push_frame (state, menu, item);

          if (find_id_attribute (attribute_names, attribute_values, &id))
            {
              if (state->objects)
                g_hash_table_insert (state->objects, g_strdup (id), g_object_ref (menu));
            }
          g_object_unref (menu);

          return;
        }
    }

  if (state->frame.item)
    {
      /* Can have '<attribute>' or '<link>' here. */
      if (g_str_equal (element_name, "attribute"))
        {
          const gchar *typestr;
          const gchar *name;
          const gchar *context;

          if (COLLECT (STRING,             "name", &name,
                       OPTIONAL | BOOLEAN, "translatable", &state->translatable,
                       OPTIONAL | STRING,  "context", &context,
                       OPTIONAL | STRING,  "comments", NULL, /* ignore, just for translators */
                       OPTIONAL | STRING,  "type", &typestr))
            {
              if (typestr && !g_variant_type_string_is_valid (typestr))
                {
                  g_set_error (error, G_VARIANT_PARSE_ERROR,
                               G_VARIANT_PARSE_ERROR_INVALID_TYPE_STRING,
                               "Invalid GVariant type string '%s'", typestr);
                  return;
                }

              state->type = typestr ? g_variant_type_new (typestr) : g_variant_type_copy (G_VARIANT_TYPE_STRING);
              state->string = g_string_new (NULL);
              state->attribute = g_quark_from_string (name);
              state->context = g_strdup (context);

              g_menu_markup_push_frame (state, NULL, NULL);
            }

          return;
        }

      if (g_str_equal (element_name, "link"))
        {
          const gchar *name;
          const gchar *id;

          if (COLLECT (STRING,            "name", &name,
                       STRING | OPTIONAL, "id",   &id))
            {
              GMenu *menu;

              menu = g_menu_new ();
              g_menu_item_set_link (state->frame.item, name, G_MENU_MODEL (menu));
              g_menu_markup_push_frame (state, menu, NULL);

              if (id != NULL && state->objects)
                g_hash_table_insert (state->objects, g_strdup (id), g_object_ref (menu));
              g_object_unref (menu);
            }

          return;
        }
    }

  {
    const GSList *element_stack;

    element_stack = g_markup_parse_context_get_element_stack (context);

    if (element_stack->next)
      g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ELEMENT,
                   _("Element <%s> not allowed inside <%s>"),
                   element_name, (const gchar *) element_stack->next->data);

    else
      g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ELEMENT,
                   _("Element <%s> not allowed at toplevel"), element_name);
  }
}

static void
g_menu_markup_end_element (GMarkupParseContext  *context,
                           const gchar          *element_name,
                           gpointer              user_data,
                           GError              **error)
{
  GMenuMarkupState *state = user_data;

  g_menu_markup_pop_frame (state);

  if (state->string)
    {
      GVariant *value;
      gchar *text;

      text = g_string_free (state->string, FALSE);
      state->string = NULL;

      /* If error is set here, it will follow us out, ending the parse.
       * We still need to free everything, though.
       */
      if ((value = g_variant_parse (state->type, text, NULL, NULL, error)))
        {
          /* Deal with translatable string attributes */
          if (state->domain && state->translatable &&
              g_variant_type_equal (state->type, G_VARIANT_TYPE_STRING))
            {
              const gchar *msgid;
              const gchar *msgstr;

              msgid = g_variant_get_string (value, NULL);
              if (state->context)
                msgstr = g_dpgettext2 (state->domain, state->context, msgid);
              else
                msgstr = g_dgettext (state->domain, msgid);

              if (msgstr != msgid)
                {
                  g_variant_unref (value);
                  value = g_variant_new_string (msgstr);
                }
            }

          g_menu_item_set_attribute_value (state->frame.item, g_quark_to_string (state->attribute), value);
          g_variant_unref (value);
        }

      g_variant_type_free (state->type);
      state->type = NULL;

      g_free (state->context);
      state->context = NULL;

      g_free (text);
    }
}

static void
g_menu_markup_text (GMarkupParseContext  *context,
                    const gchar          *text,
                    gsize                 text_len,
                    gpointer              user_data,
                    GError              **error)
{
  GMenuMarkupState *state = user_data;
  gint i;

  for (i = 0; i < text_len; i++)
    if (!g_ascii_isspace (text[i]))
      {
        if (state->string)
          g_string_append_len (state->string, text, text_len);

        else
          g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                       _("text may not appear inside <%s>"),
                       g_markup_parse_context_get_element (context));
        break;
      }
}

static void
g_menu_markup_error (GMarkupParseContext *context,
                     GError              *error,
                     gpointer             user_data)
{
  GMenuMarkupState *state = user_data;

  while (state->frame.prev)
    {
      struct frame *prev = state->frame.prev;

      state->frame = *prev;

      g_slice_free (struct frame, prev);
    }

  if (state->string)
    g_string_free (state->string, TRUE);

  if (state->type)
    g_variant_type_free (state->type);

  if (state->objects)
    g_hash_table_unref (state->objects);

  g_free (state->context);

  g_slice_free (GMenuMarkupState, state);
}

static GMarkupParser g_menu_subparser =
{
  g_menu_markup_start_element,
  g_menu_markup_end_element,
  g_menu_markup_text,
  NULL,                            /* passthrough */
  g_menu_markup_error
};

/**
 * g_menu_markup_parser_start:
 * @context: a #GMarkupParseContext
 * @domain: (allow-none): translation domain for labels, or %NULL
 * @objects: (allow-none): a #GHashTable for the objects, or %NULL
 *
 * Begin parsing a group of menus in XML form.
 *
 * If @domain is not %NULL, it will be used to translate attributes
 * that are marked as translatable, using gettext().
 *
 * If @objects is specified then it must be a #GHashTable that was
 * created using g_hash_table_new_full() with g_str_hash(),
 * g_str_equal(), g_free() and g_object_unref().
 * Any named menus (ie: <tag class="starttag">menu</tag>,
 * <tag class="starttag">submenu</tag>,
 * <tag class="starttag">section</tag> or <tag class="starttag">link</tag>
 * elements with an id='' attribute) that are encountered while parsing
 * will be added to this table. Each toplevel menu must be named.
 *
 * If @objects is %NULL then an empty hash table will be created.
 *
 * This function should be called from the start_element function for
 * the element representing the group containing the menus.  In other
 * words, the content inside of this element is expected to be a list of
 * menus.
 *
 * Since: 2.32
 */
void
g_menu_markup_parser_start (GMarkupParseContext *context,
                            const gchar         *domain,
                            GHashTable          *objects)
{
  GMenuMarkupState *state;

  g_return_if_fail (context != NULL);

  state = g_slice_new0 (GMenuMarkupState);

  state->domain = g_strdup (domain);

  if (objects != NULL)
    state->objects = g_hash_table_ref (objects);
  else
    state->objects = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);

  g_markup_parse_context_push (context, &g_menu_subparser, state);
}

/**
 * g_menu_markup_parser_end:
 * @context: a #GMarkupParseContext
 *
 * Stop the parsing of a set of menus and return the #GHashTable.
 *
 * The #GHashTable maps strings to #GObject instances.  The parser only
 * adds #GMenu instances to the table, but it may contain other types if
 * a table was provided to g_menu_markup_parser_start().
 *
 * This call should be matched with g_menu_markup_parser_start().
 * See that function for more information
 *
 * Returns: (transfer full): the #GHashTable containing the objects
 *
 * Since: 2.32
 */
GHashTable *
g_menu_markup_parser_end (GMarkupParseContext *context)
{
  GMenuMarkupState *state = g_markup_parse_context_pop (context);
  GHashTable *objects;

  objects = state->objects;

  g_free (state->domain);

  g_slice_free (GMenuMarkupState, state);

  return objects;
}

/**
 * g_menu_markup_parser_start_menu:
 * @context: a #GMarkupParseContext
 * @domain: (allow-none): translation domain for labels, or %NULL
 * @objects: (allow-none): a #GHashTable for the objects, or %NULL
 *
 * Begin parsing the XML definition of a menu.
 *
 * This function should be called from the start_element function for
 * the element representing the menu itself.  In other words, the
 * content inside of this element is expected to be a list of items.
 *
 * If @domain is not %NULL, it will be used to translate attributes
 * that are marked as translatable, using gettext().
 *
 * If @objects is specified then it must be a #GHashTable that was
 * created using g_hash_table_new_full() with g_str_hash(),
 * g_str_equal(), g_free() and g_object_unref().
 * Any named menus (ie: <tag class="starttag">submenu</tag>,
 * <tag class="starttag">section</tag> or <tag class="starttag">link</tag>
 * elements with an id='' attribute) that are encountered while parsing
 * will be added to this table.
 * Note that toplevel <tag class="starttag">menu</tag> is not added to
 * the hash table, even if it has an id attribute.
 *
 * If @objects is %NULL then named menus will not be supported.
 *
 * You should call g_menu_markup_parser_end_menu() from the
 * corresponding end_element function in order to collect the newly
 * parsed menu.
 *
 * Since: 2.32
 */
void
g_menu_markup_parser_start_menu (GMarkupParseContext *context,
                                 const gchar         *domain,
                                 GHashTable          *objects)
{
  GMenuMarkupState *state;

  g_return_if_fail (context != NULL);

  state = g_slice_new0 (GMenuMarkupState);

  if (objects)
    state->objects = g_hash_table_ref (objects);

  state->domain = g_strdup (domain);

  g_markup_parse_context_push (context, &g_menu_subparser, state);

  state->frame.menu = g_menu_new ();
}

/**
 * g_menu_markup_parser_end_menu:
 * @context: a #GMarkupParseContext
 *
 * Stop the parsing of a menu and return the newly-created #GMenu.
 *
 * This call should be matched with g_menu_markup_parser_start_menu().
 * See that function for more information
 *
 * Returns: (transfer full): the newly-created #GMenu
 *
 * Since: 2.32
 */
GMenu *
g_menu_markup_parser_end_menu (GMarkupParseContext *context)
{
  GMenuMarkupState *state = g_markup_parse_context_pop (context);
  GMenu *menu;

  menu = state->frame.menu;

  if (state->objects)
    g_hash_table_unref (state->objects);
  g_free (state->domain);
  g_slice_free (GMenuMarkupState, state);

  return menu;
}

static void
indent_string (GString *string,
               gint     indent)
{
  while (indent--)
    g_string_append_c (string, ' ');
}

/**
 * g_menu_markup_print_string:
 * @string: a #GString
 * @model: the #GMenuModel to print
 * @indent: the intentation level to start at
 * @tabstop: how much to indent each level
 *
 * Print the contents of @model to @string.
 * Note that you have to provide the containing
 * <tag class="starttag">menu</tag> element yourself.
 *
 * Returns: @string
 *
 * Since: 2.32
 */
GString *
g_menu_markup_print_string (GString    *string,
                            GMenuModel *model,
                            gint        indent,
                            gint        tabstop)
{
  gboolean need_nl = FALSE;
  gint i, n;

  if G_UNLIKELY (string == NULL)
    string = g_string_new (NULL);

  n = g_menu_model_get_n_items (model);

  for (i = 0; i < n; i++)
    {
      GMenuAttributeIter *attr_iter;
      GMenuLinkIter *link_iter;
      GString *contents;
      GString *attrs;

      attr_iter = g_menu_model_iterate_item_attributes (model, i);
      link_iter = g_menu_model_iterate_item_links (model, i);
      contents = g_string_new (NULL);
      attrs = g_string_new (NULL);

      while (g_menu_attribute_iter_next (attr_iter))
        {
          const char *name = g_menu_attribute_iter_get_name (attr_iter);
          GVariant *value = g_menu_attribute_iter_get_value (attr_iter);

          if (g_variant_is_of_type (value, G_VARIANT_TYPE_STRING))
            {
              gchar *str;
              str = g_markup_printf_escaped (" %s='%s'", name, g_variant_get_string (value, NULL));
              g_string_append (attrs, str);
              g_free (str);
            }

          else
            {
              gchar *printed;
              gchar *str;
              const gchar *type;

              printed = g_variant_print (value, TRUE);
              type = g_variant_type_peek_string (g_variant_get_type (value));
              str = g_markup_printf_escaped ("<attribute name='%s' type='%s'>%s</attribute>\n", name, type, printed);
              indent_string (contents, indent + tabstop);
              g_string_append (contents, str);
              g_free (printed);
              g_free (str);
            }

          g_variant_unref (value);
        }
      g_object_unref (attr_iter);

      while (g_menu_link_iter_next (link_iter))
        {
          const gchar *name = g_menu_link_iter_get_name (link_iter);
          GMenuModel *menu = g_menu_link_iter_get_value (link_iter);
          gchar *str;

          if (contents->str[0])
            g_string_append_c (contents, '\n');

          str = g_markup_printf_escaped ("<link name='%s'>\n", name);
          indent_string (contents, indent + tabstop);
          g_string_append (contents, str);
          g_free (str);

          g_menu_markup_print_string (contents, menu, indent + 2 * tabstop, tabstop);

          indent_string (contents, indent + tabstop);
          g_string_append (contents, "</link>\n");
          g_object_unref (menu);
        }
      g_object_unref (link_iter);

      if (contents->str[0])
        {
          indent_string (string, indent);
          g_string_append_printf (string, "<item%s>\n", attrs->str);
          g_string_append (string, contents->str);
          indent_string (string, indent);
          g_string_append (string, "</item>\n");
          need_nl = TRUE;
        }

      else
        {
          if (need_nl)
            g_string_append_c (string, '\n');

          indent_string (string, indent);
          g_string_append_printf (string, "<item%s/>\n", attrs->str);
          need_nl = FALSE;
        }

      g_string_free (contents, TRUE);
      g_string_free (attrs, TRUE);
    }

  return string;
}

/**
 * g_menu_markup_print_stderr:
 * @model: a #GMenuModel
 *
 * Print @model to stderr for debugging purposes.
 *
 * This debugging function will be removed in the future.
 */
void
g_menu_markup_print_stderr (GMenuModel *model)
{
  GString *string;

  string = g_string_new ("<menu>\n");
  g_menu_markup_print_string (string, model, 2, 2);
  g_printerr ("%s</menu>\n", string->str);
  g_string_free (string, TRUE);
}
