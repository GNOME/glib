#undef G_DISABLE_ASSERT
#undef G_LOG_DOMAIN

#include <stdio.h>
#include <glib.h>

static int depth = 0;

static void
indent (int extra)
{
  int i = 0;
  while (i < depth)
    {
      fputs ("  ", stdout);
      ++i;
    }
}

static void
start_element_handler  (GMarkupParseContext *context,
                        const gchar         *element_name,
                        const gchar        **attribute_names,
                        const gchar        **attribute_values,
                        gpointer             user_data,
                        GError             **error)
{
  int i;
  
  indent (0);
  printf ("ELEMENT '%s'\n", element_name);

  i = 0;
  while (attribute_names[i] != NULL)
    {
      indent (1);

      printf ("%s=\"%s\"\n",
              attribute_names[i],
              attribute_values[i]);
      
      ++i;
    }
  
  ++depth;
}

static void
end_element_handler    (GMarkupParseContext *context,
                        const gchar         *element_name,
                        gpointer             user_data,
                        GError             **error)
{
  --depth;
  indent (0);
  printf ("END '%s'\n", element_name);
  }

static void
text_handler                      (GMarkupParseContext *context,
                        const gchar         *text,
                        gsize                text_len,
                        gpointer             user_data,
                        GError             **error)
{
  indent (0);
  printf ("TEXT '%.*s'\n", (int)text_len, text);
}


static void
passthrough_handler    (GMarkupParseContext *context,
                        const gchar         *passthrough_text,
                        gsize                text_len,
                        gpointer             user_data,
                        GError             **error)
{
  indent (0);

  printf ("PASS '%.*s'\n", (int)text_len, passthrough_text);
}

static void
error_handler          (GMarkupParseContext *context,
                        GError              *error,
                        gpointer             user_data)
{
  fprintf (stderr, " %s\n", error->message);
}

static const GMarkupParser parser = {
  start_element_handler,
  end_element_handler,
  text_handler,
  passthrough_handler,
  error_handler
};

static const GMarkupParser silent_parser = {
  NULL,
  NULL,
  NULL,
  NULL,
  error_handler
};

static int
test_in_chunks (const gchar *contents,
                gint         length,
                gint         chunk_size)
{
  GMarkupParseContext *context;
  int i = 0;
  
  context = g_markup_parse_context_new (&silent_parser, 0, NULL, NULL);

  while (i < length)
    {
      int this_chunk = MIN (length - i, chunk_size);

      if (!g_markup_parse_context_parse (context,
                                         contents + i,
                                         this_chunk,
                                         NULL))
        {
          g_markup_parse_context_free (context);
          return 1;
        }

      i += this_chunk;
    }
      
  if (!g_markup_parse_context_end_parse (context, NULL))
    {
      g_markup_parse_context_free (context);
      return 1;
    }

  g_markup_parse_context_free (context);

  return 0;
}

static int
test_file (const gchar *filename)
{
  gchar *contents;
  gsize  length;
  GError *error;
  GMarkupParseContext *context;
  
  error = NULL;
  if (!g_file_get_contents (filename,
                            &contents,
                            &length,
                            &error))
    {
      fprintf (stderr, "%s\n", error->message);
      g_error_free (error);
      return 1;
    }

  context = g_markup_parse_context_new (&parser, 0, NULL, NULL);

  if (!g_markup_parse_context_parse (context, contents, length, NULL))
    {
      g_markup_parse_context_free (context);
      return 1;
    }

  if (!g_markup_parse_context_end_parse (context, NULL))
    {
      g_markup_parse_context_free (context);
      return 1;
    }

  g_markup_parse_context_free (context);

  /* A byte at a time */
  if (test_in_chunks (contents, length, 1) != 0)
    return 1;

  /* 2 bytes */
  if (test_in_chunks (contents, length, 2) != 0)
    return 1;

  /*5 bytes */
  if (test_in_chunks (contents, length, 5) != 0)
    return 1;
  
  /* 12 bytes */
  if (test_in_chunks (contents, length, 12) != 0)
    return 1;
  
  /* 1024 bytes */
  if (test_in_chunks (contents, length, 1024) != 0)
    return 1;

  return 0;
}

int
main (int   argc,
      char *argv[])
{
  if (argc > 1)
    return test_file (argv[1]);
  else
    {
      fprintf (stderr, "Give a markup file on the command line\n");
      return 1;
    }
}

