/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
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

/*
 * Modified by the GLib Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GLib Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GLib at ftp://ftp.gtk.org/pub/gtk/. 
 */

#undef G_DISABLE_ASSERT
#undef G_LOG_DOMAIN

#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>


typedef struct _TestResult TestResult;

struct _TestResult
{
  gint argc;
  const gchar **argv;
};

static const gchar *
test_command_lines[] =
{
  /*  0 */ "foo bar",
  /*  1 */ "foo 'bar'",
  /*  2 */ "foo \"bar\"",
  /*  3 */ "foo '' 'bar'",
  /*  4 */ "foo \"bar\"'baz'blah'foo'\\''blah'\"boo\"",
  /*  5 */ "foo \t \tblah\tfoo\t\tbar  baz",
  /*  6 */ "foo '    spaces more spaces lots of     spaces in this   '  \t",
  /*  7 */ "foo \\\nbar",
  /*  8 */ "foo '' ''",
  /*  9 */ "foo \\\" la la la",
  /* 10 */ "foo \\ foo woo woo\\ ",
  /* 11 */ "foo \"yada yada \\$\\\"\"",
  /* 12 */ "foo \"c:\\\\\"",
  NULL
};

static const gchar *result0[] = { "foo", "bar", NULL };
static const gchar *result1[] = { "foo", "bar", NULL };
static const gchar *result2[] = { "foo", "bar", NULL };
static const gchar *result3[] = { "foo", "", "bar", NULL };
static const gchar *result4[] = { "foo", "barbazblahfoo'blahboo", NULL };
static const gchar *result5[] = { "foo", "blah", "foo", "bar", "baz", NULL };
static const gchar *result6[] = { "foo", "    spaces more spaces lots of     spaces in this   ", NULL };
static const gchar *result7[] = { "foo", "bar", NULL };
static const gchar *result8[] = { "foo", "", "", NULL };
static const gchar *result9[] = { "foo", "\"", "la", "la", "la", NULL };
static const gchar *result10[] = { "foo", " foo", "woo", "woo ", NULL };
static const gchar *result11[] = { "foo", "yada yada $\"", NULL };
static const gchar *result12[] = { "foo", "c:\\", NULL };

static const TestResult
correct_results[] =
{
  { G_N_ELEMENTS (result0) - 1, result0 },
  { G_N_ELEMENTS (result1) - 1, result1 },
  { G_N_ELEMENTS (result2) - 1, result2 },
  { G_N_ELEMENTS (result3) - 1, result3 },
  { G_N_ELEMENTS (result4) - 1, result4 },
  { G_N_ELEMENTS (result5) - 1, result5 },
  { G_N_ELEMENTS (result6) - 1, result6 },
  { G_N_ELEMENTS (result7) - 1, result7 },
  { G_N_ELEMENTS (result8) - 1, result8 },
  { G_N_ELEMENTS (result9) - 1, result9 },
  { G_N_ELEMENTS (result10) - 1, result10 },
  { G_N_ELEMENTS (result11) - 1, result11 },
  { G_N_ELEMENTS (result12) - 1, result12 }
};

static void
print_test (const gchar *cmdline, gint argc, gchar **argv,
            const TestResult *result)
{
  gint i;
  
  fprintf (stderr, "Command line was: '%s'\n", cmdline);

  fprintf (stderr, "Expected result (%d args):\n", result->argc);
  
  i = 0;
  while (result->argv[i])
    {
      fprintf (stderr, " %3d '%s'\n", i, result->argv[i]);
      ++i;
    }  

  fprintf (stderr, "Actual result (%d args):\n", argc);
  
  i = 0;
  while (argv[i])
    {
      fprintf (stderr, " %3d '%s'\n", i, argv[i]);
      ++i;
    }
}

static void
do_argv_test (const gchar *cmdline, const TestResult *result)
{
  gint argc;
  gchar **argv;
  GError *err;
  gint i;

  err = NULL;
  if (!g_shell_parse_argv (cmdline, &argc, &argv, &err))
    {
      fprintf (stderr, "Error parsing command line that should work fine: %s\n",
               err->message);
      
      exit (1);
    }
  
  if (argc != result->argc)
    {
      fprintf (stderr, "Expected and actual argc don't match\n");
      print_test (cmdline, argc, argv, result);
      exit (1);
    }

  i = 0;
  while (argv[i])
    {
      if (strcmp (argv[i], result->argv[i]) != 0)
        {
          fprintf (stderr, "Expected and actual arg %d do not match\n", i);
          print_test (cmdline, argc, argv, result);
          exit (1);
        }
      
      ++i;
    }

  if (argv[i] != NULL)
    {
      fprintf (stderr, "argv didn't get NULL-terminated\n");
      exit (1);
    }
  g_strfreev (argv);
}

static void
run_tests (void)
{
  gint i;
  
  i = 0;
  while (test_command_lines[i])
    {
      do_argv_test (test_command_lines[i], &correct_results[i]);
      ++i;
    }
}

static gboolean any_test_failed = FALSE;

#define CHECK_STRING_RESULT(expression, expected_value) \
 check_string_result (#expression, __FILE__, __LINE__, expression, expected_value)

static void
check_string_result (const char *expression,
		     const char *file_name,
		     int line_number,
		     char *result,
		     const char *expected)
{
  gboolean match;
  
  if (expected == NULL)
    match = result == NULL;
  else
    match = result != NULL && strcmp (result, expected) == 0;
  
  if (!match)
    {
      if (!any_test_failed)
	fprintf (stderr, "\n");
      
      fprintf (stderr, "FAIL: check failed in %s, line %d\n", file_name, line_number);
      fprintf (stderr, "      evaluated: %s\n", expression);
      fprintf (stderr, "       expected: %s\n", expected == NULL ? "NULL" : expected);
      fprintf (stderr, "            got: %s\n", result == NULL ? "NULL" : result);
      
      any_test_failed = TRUE;
    }
  
  g_free (result);
}

static char *
test_shell_unquote (const char *str)
{
  char *result;
  GError *error;

  error = NULL;
  result = g_shell_unquote (str, &error);
  if (error == NULL)
    return result;

  /* Leaks the error, which is no big deal and easy to fix if we
   * decide it matters.
   */

  if (error->domain != G_SHELL_ERROR)
    return g_strdup ("error in domain other than G_SHELL_ERROR");

  /* It would be nice to check the error message too, but that's
   * localized, so it's too much of a pain.
   */
  switch (error->code)
    {
    case G_SHELL_ERROR_BAD_QUOTING:
      return g_strdup ("G_SHELL_ERROR_BAD_QUOTING");
    case G_SHELL_ERROR_EMPTY_STRING:
      return g_strdup ("G_SHELL_ERROR_EMPTY_STRING");
    case G_SHELL_ERROR_FAILED:
      return g_strdup ("G_SHELL_ERROR_FAILED");
    default:
      return g_strdup ("bad error code in G_SHELL_ERROR domain");
    }
}

int
main (int   argc,
      char *argv[])
{
  run_tests ();
  
  CHECK_STRING_RESULT (g_shell_quote (""), "''");
  CHECK_STRING_RESULT (g_shell_quote ("a"), "'a'");
  CHECK_STRING_RESULT (g_shell_quote ("("), "'('");
  CHECK_STRING_RESULT (g_shell_quote ("'"), "''\\'''");
  CHECK_STRING_RESULT (g_shell_quote ("'a"), "''\\''a'");
  CHECK_STRING_RESULT (g_shell_quote ("a'"), "'a'\\'''");
  CHECK_STRING_RESULT (g_shell_quote ("a'a"), "'a'\\''a'");
  
  CHECK_STRING_RESULT (test_shell_unquote (""), "");
  CHECK_STRING_RESULT (test_shell_unquote ("a"), "a");
  CHECK_STRING_RESULT (test_shell_unquote ("'a'"), "a");
  CHECK_STRING_RESULT (test_shell_unquote ("'('"), "(");
  CHECK_STRING_RESULT (test_shell_unquote ("''\\'''"), "'");
  CHECK_STRING_RESULT (test_shell_unquote ("''\\''a'"), "'a");
  CHECK_STRING_RESULT (test_shell_unquote ("'a'\\'''"), "a'");
  CHECK_STRING_RESULT (test_shell_unquote ("'a'\\''a'"), "a'a");

  CHECK_STRING_RESULT (test_shell_unquote ("\\\\"), "\\");
  CHECK_STRING_RESULT (test_shell_unquote ("\\\n"), "");

  CHECK_STRING_RESULT (test_shell_unquote ("'\\''"), "G_SHELL_ERROR_BAD_QUOTING");

#if defined (_MSC_VER) && (_MSC_VER <= 1200)
  /* using \x22 instead of \" to work around a msvc 5.0, 6.0 compiler bug */
  CHECK_STRING_RESULT (test_shell_unquote ("\x22\\\x22\""), "\"");
#else
  CHECK_STRING_RESULT (test_shell_unquote ("\"\\\"\""), "\"");
#endif

  CHECK_STRING_RESULT (test_shell_unquote ("\""), "G_SHELL_ERROR_BAD_QUOTING");
  CHECK_STRING_RESULT (test_shell_unquote ("'"), "G_SHELL_ERROR_BAD_QUOTING");

  CHECK_STRING_RESULT (test_shell_unquote ("\x22\\\\\""), "\\");
  CHECK_STRING_RESULT (test_shell_unquote ("\x22\\`\""), "`");
  CHECK_STRING_RESULT (test_shell_unquote ("\x22\\$\""), "$");
  CHECK_STRING_RESULT (test_shell_unquote ("\x22\\\n\""), "\n");

  CHECK_STRING_RESULT (test_shell_unquote ("\"\\'\""), "\\'");
  CHECK_STRING_RESULT (test_shell_unquote ("\x22\\\r\""), "\\\r");
  CHECK_STRING_RESULT (test_shell_unquote ("\x22\\n\""), "\\n");

  return any_test_failed ? 1 : 0;
}
