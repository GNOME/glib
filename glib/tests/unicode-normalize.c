#undef G_DISABLE_ASSERT
#undef G_LOG_DOMAIN

#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *
decode (const gchar *input)
{
  unsigned ch;
  int offset = 0;
  GString *result = g_string_new (NULL);

  do
    {
      g_assert_cmpint (sscanf (input + offset, "%x", &ch), ==, 1);
      g_string_append_unichar (result, ch);

      while (input[offset] && input[offset] != ' ')
	offset++;
      while (input[offset] && input[offset] == ' ')
	offset++;
    }
  while (input[offset]);

  return g_string_free (result, FALSE);
}

const char *names[4] = {
  "NFD",
  "NFC",
  "NFKD",
  "NFKC"
};

static void
test_form (int            line,
	   GNormalizeMode mode,
	   gboolean       do_compat,
	   int            expected,
	   char         **c,
	   char         **raw)
{
  int i;
  gboolean mode_is_compat = (mode == G_NORMALIZE_NFKC ||
			     mode == G_NORMALIZE_NFKD);

  if (mode_is_compat || !do_compat)
    {
      for (i = 0; i < 3; i++)
	{
	  char *result = g_utf8_normalize (c[i], -1, mode);
          g_assert_cmpstr (result, ==, c[expected]);
          g_free (result);
	}
    }
  if (mode_is_compat || do_compat)
    {
      for (i = 3; i < 5; i++)
	{
	  char *result = g_utf8_normalize (c[i], -1, mode);
          g_assert_cmpstr (result, ==, c[expected]);
          g_free (result);
	}
    }
}

static void
process_one (int line, gchar **columns)
{
  char *c[5];
  int i;

  for (i = 0; i < 5; i++)
    {
      c[i] = decode (columns[i]);
      g_assert_nonnull (c[i]);
    }

  test_form (line, G_NORMALIZE_NFD, FALSE, 2, c, columns);
  test_form (line, G_NORMALIZE_NFD, TRUE, 4, c, columns);
  test_form (line, G_NORMALIZE_NFC, FALSE, 1, c, columns);
  test_form (line, G_NORMALIZE_NFC, TRUE, 3, c, columns);
  test_form (line, G_NORMALIZE_NFKD, TRUE, 4, c, columns);
  test_form (line, G_NORMALIZE_NFKC, TRUE, 3, c, columns);

  for (i = 0; i < 5; i++)
    g_free (c[i]);
}

static void
test_unicode_normalize (void)
{
  GIOChannel *in;
  GError *error = NULL;
  gchar *filename = NULL;
  GString *buffer = g_string_new (NULL);
  int line = 1;

  filename = g_test_build_filename (G_TEST_DIST, "NormalizationTest.txt", NULL);
  g_assert_nonnull (filename);

  in = g_io_channel_new_file (filename, "r", &error);
  g_assert_no_error (error);
  g_assert_nonnull (in);
  g_free (filename);

  while (TRUE)
    {
      gsize term_pos;
      gchar **columns;

      if (g_io_channel_read_line_string (in, buffer, &term_pos, &error) != G_IO_STATUS_NORMAL)
	break;

      buffer->str[term_pos] = '\0';

      if (buffer->str[0] == '#') /* Comment */
	goto next;
      if (buffer->str[0] == '@') /* Part */
	{
	  g_test_message ("Processing %s", buffer->str + 1);
	  goto next;
	}

      columns = g_strsplit (buffer->str, ";", -1);
      if (!columns[0])
        {
          g_strfreev (columns);
          goto next;
        }

      process_one (line, columns);
      g_strfreev (columns);

    next:
      g_string_truncate (buffer, 0);
      line++;
    }

  g_assert_no_error (error);

  g_io_channel_unref (in);
  g_string_free (buffer, TRUE);
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/unicode/normalize", test_unicode_normalize);

  return g_test_run ();
}
