#include <glib.h>

/*
 * Example XML for the parser.
 */
static const gchar foo_xml_example[] =
"<foo bar='baz' bir='boz'>"
"   <bar>bar text 1</bar> "
"   <bar>bar text 2</bar> "
"   foo text              "
"<!-- nothing -->         "
"</foo>                   ";

static void foo_parser_start_element (GMarkupParseContext  *context,
                                      const gchar          *element_name,
                                      const gchar         **attribute_names,
                                      const gchar         **attribute_values,
                                      gpointer              user_data,
                                      GError              **error);
static void foo_parser_end_element   (GMarkupParseContext  *context,
                                      const gchar          *element_name,
                                      gpointer              user_data,
                                      GError              **error);
static void foo_parser_characters    (GMarkupParseContext  *context,
                                      const gchar          *text,
                                      gsize                 text_len,
                                      gpointer              user_data,
                                      GError              **error);
static void foo_parser_passthrough   (GMarkupParseContext  *context,
                                      const gchar          *passthrough_text,
                                      gsize                 text_len,
                                      gpointer              user_data,
                                      GError              **error);
static void foo_parser_error         (GMarkupParseContext  *context,
                                      GError               *error,
                                      gpointer              user_data);

/*
 * Parser
 */
static const GMarkupParser foo_xml_parser = {
  foo_parser_start_element,
  foo_parser_end_element,
  foo_parser_characters,
  foo_parser_passthrough,
  foo_parser_error
};

/*
 * Called for opening tags like <foo bar="baz">
 */
static void
foo_parser_start_element (GMarkupParseContext *context,
                          const gchar         *element_name,
                          const gchar        **attribute_names,
                          const gchar        **attribute_values,
                          gpointer             user_data,
                          GError             **error)
{
  gsize i;

  g_print ("element: <%s>\n", element_name);

  for (i = 0; attribute_names[i]; i++)
    {
      g_print ("attribute: %s = \"%s\"\n", attribute_names[i],
                                           attribute_values[i]);
    }
}

/*
 * Called for closing tags like </foo>
 */
static void
foo_parser_end_element (GMarkupParseContext *context,
                        const gchar         *element_name,
                        gpointer             user_data,
                        GError             **error)
{
  g_print ("attribute: </%s>\n", element_name);
}

/*
 * Called for character data. Text is not nul-terminated
 */
static void
foo_parser_characters (GMarkupParseContext *context,
                       const gchar         *text,
                       gsize                text_len,
                       gpointer             user_data,
                       GError             **error)
{
  g_print ("text: [%s]\n", text);
}

/*
 * Called for strings that should be re-saved verbatim in this same
 * position, but are not otherwise interpretable. At the moment this
 * includes comments and processing instructions. Text is not
 * nul-terminated.
 */
static void
foo_parser_passthrough    (GMarkupParseContext  *context,
                           const gchar          *passthrough_text,
                           gsize                 text_len,
                           gpointer              user_data,
                           GError              **error)
{
  g_print ("passthrough: %s\n", passthrough_text);
}

/*
 * Called when any parsing method encounters an error. The GError should not be
 * freed.
 */
static void
foo_parser_error          (GMarkupParseContext  *context,
                           GError               *error,
                           gpointer              user_data)
{
  g_printerr ("ERROR: %s\n", error->message);
}

int main ()
{
  GMarkupParseContext *context;
  gboolean success = FALSE;
  glong len;

  len = g_utf8_strlen (foo_xml_example, -1);
  g_print ("Parsing: %s\n", foo_xml_example);
  g_print ("(%ld UTF8 characters)\n", len);

  context = g_markup_parse_context_new (&foo_xml_parser, 0, NULL, NULL);

  success = g_markup_parse_context_parse (context, foo_xml_example, len, NULL);

  g_markup_parse_context_free (context);

  if (success)
    {
      g_print ("DONE\n");
      return 0;
    }
  else
    {
      g_printerr ("ERROR\n");
      return 1;
    }
}
