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
  { G_N_ELEMENTS (result11) - 1, result11 }
};

static void
print_test (const gchar *cmdline, gint argc, gchar **argv,
            const TestResult *result)
{
  gint i;
  
  printf ("\nCommand line was: '%s'\n", cmdline);

  printf ("Expected result (%d args):\n", result->argc);
  
  i = 0;
  while (result->argv[i])
    {
      printf (" %3d '%s'\n", i, result->argv[i]);

      ++i;
    }  

  printf ("Actual result (%d args):\n", argc);
  
  i = 0;
  while (argv[i])
    {
      printf (" %3d '%s'\n", i, argv[i]);

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
}

static void
run_tests (void)
{
  gint i;
  
  i = 0;
  while (test_command_lines[i])
    {
      printf ("g_shell_parse_argv() test %d - ", i);
      do_argv_test (test_command_lines[i], &correct_results[i]);
      printf ("ok (%s)\n", test_command_lines[i]);
      
      ++i;
    }
}

int
main (int   argc,
      char *argv[])
{
  run_tests ();
  
  return 0;
}


