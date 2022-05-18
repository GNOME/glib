/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GLib Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GLib Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GLib at ftp://ftp.gtk.org/pub/gtk/.
 */

#include <glib.h>
#include <glib/gstdio.h>

#ifdef G_OS_WIN32
#include <fcntl.h>
#include <io.h>
#define pipe(fds) _pipe(fds, 4096, _O_BINARY)
#endif

#ifdef G_OS_WIN32
static gchar *dirname = NULL;
#endif

static void
test_spawn_basics (void)
{
  gboolean result;
  GError *err = NULL;
  gchar *output = NULL;
  gchar *erroutput = NULL;
#ifdef G_OS_WIN32
  int n;
  char buf[100];
  int pipedown[2], pipeup[2];
  gchar **argv = NULL;
  gchar spawn_binary[1000] = {0};
  gchar full_cmdline[1000] = {0};

  g_snprintf (spawn_binary, sizeof (spawn_binary),
              "%s\\spawn-test-win32-gui.exe", dirname);
  g_free (dirname);
#endif

  err = NULL;
  result =
    g_spawn_command_line_sync ("nonexistent_application foo 'bar baz' blah blah",
                               NULL, NULL, NULL, &err);
  g_assert_false (result);
  g_assert_error (err, G_SPAWN_ERROR, G_SPAWN_ERROR_NOENT);
  g_clear_error (&err);

  err = NULL;
  result =
    g_spawn_command_line_async ("nonexistent_application foo bar baz \"blah blah\"",
                                &err);
  g_assert_false (result);
  g_assert_error (err, G_SPAWN_ERROR, G_SPAWN_ERROR_NOENT);
  g_clear_error (&err);

  err = NULL;
#ifdef G_OS_UNIX
  result = g_spawn_command_line_sync ("/bin/sh -c 'echo hello'",
                                      &output, NULL, NULL, &err);
  g_assert_no_error (err);
  g_assert_true (result);
  g_assert_cmpstr (output, ==, "hello\n");

  g_free (output);
  output = NULL;
#endif

  /* Running sort synchronously, collecting its output. 'sort' command
   * is selected because it is non-builtin command on both unix and
   * win32 with well-defined stdout behaviour.
   */
  g_file_set_contents ("spawn-test-created-file.txt",
                       "line first\nline 2\nline last\n", -1, &err);
  g_assert_no_error(err);

  result = g_spawn_command_line_sync ("sort spawn-test-created-file.txt",
                                      &output, &erroutput, NULL, &err);
  g_assert_no_error (err);
  g_assert_true (result);
  g_assert_cmpstr (output, ==, "line 2\nline first\nline last\n");
  g_assert_cmpstr (erroutput, ==, "");

  g_free (output);
  output = NULL;
  g_free (erroutput);
  erroutput = NULL;

  result = g_spawn_command_line_sync ("sort non-existing-file.txt",
                                      NULL, &erroutput, NULL, &err);
  g_assert_no_error (err);
  g_assert_true (result);
  g_assert_true (g_str_has_prefix (erroutput, "sort: "));
  g_assert_nonnull (strstr (erroutput, "No such file or directory"));

  g_free (erroutput);
  erroutput = NULL;
  g_unlink ("spawn-test-created-file.txt");

#ifdef G_OS_WIN32
  g_test_message ("Running spawn-test-win32-gui in various ways.");

  g_test_message ("First asynchronously (without wait).");
  g_snprintf (full_cmdline, sizeof (full_cmdline), "'%s' 1", spawn_binary);
  result = g_spawn_command_line_async (full_cmdline, &err);
  g_assert_no_error (err);
  g_assert_true (result);

  g_test_message ("Now synchronously, collecting its output.");
  g_snprintf (full_cmdline, sizeof (full_cmdline), "'%s' 2", spawn_binary);
  result =
    g_spawn_command_line_sync (full_cmdline, &output, &erroutput, NULL, &err);

  g_assert_no_error (err);
  g_assert_true (result);
  g_assert_cmpstr (output, ==, "This is stdout\r\n");
  g_assert_cmpstr (erroutput, ==, "This is stderr\r\n");

  g_free (output);
  output = NULL;
  g_free (erroutput);
  erroutput = NULL;

  g_test_message ("Now with G_SPAWN_FILE_AND_ARGV_ZERO.");
  g_snprintf (full_cmdline, sizeof (full_cmdline),
              "'%s' this-should-be-argv-zero print_argv0", spawn_binary);
  result = g_shell_parse_argv (full_cmdline, NULL, &argv, &err);
  g_assert_no_error (err);
  g_assert_true (result);

  result = g_spawn_sync (NULL, argv, NULL, G_SPAWN_FILE_AND_ARGV_ZERO,
                         NULL, NULL, &output, NULL, NULL, &err);
  g_assert_no_error (err);
  g_assert_true (result);
  g_assert_cmpstr (output, ==, "this-should-be-argv-zero");

  g_free (output);
  output = NULL;
  g_free (argv);
  argv = NULL;

  g_test_message ("Now talking to it through pipes.");
  g_assert_cmpint (pipe (pipedown), >=, 0);
  g_assert_cmpint (pipe (pipeup), >=, 0);

  g_snprintf (full_cmdline, sizeof (full_cmdline), "'%s' pipes %d %d",
              spawn_binary, pipedown[0], pipeup[1]);

  result = g_shell_parse_argv (full_cmdline, NULL, &argv, &err);
  g_assert_no_error (err);
  g_assert_true (result);

  result = g_spawn_async (NULL, argv, NULL,
                          G_SPAWN_LEAVE_DESCRIPTORS_OPEN |
                          G_SPAWN_DO_NOT_REAP_CHILD,
                          NULL, NULL, NULL,
                          &err);
  g_assert_no_error (err);
  g_assert_true (result);
  g_free (argv);
  argv = NULL;

  g_assert_cmpint (read (pipeup[0], &n, sizeof (n)), ==, sizeof (n));
  g_assert_cmpint (read (pipeup[0], buf, n), ==, n);

  n = strlen ("Bye then");
  g_assert_cmpint (write (pipedown[1], &n, sizeof (n)), !=, -1);
  g_assert_cmpint (write (pipedown[1], "Bye then", n), !=, -1);

  g_assert_cmpint (read (pipeup[0], &n, sizeof (n)), ==, sizeof (n));
  g_assert_cmpint (n, ==, strlen ("See ya"));

  g_assert_cmpint (read (pipeup[0], buf, n), ==, n);

  buf[n] = '\0';
  g_assert_cmpstr (buf, ==, "See ya");
#endif
}

int
main (int   argc,
      char *argv[])
{
#ifdef G_OS_WIN32
  dirname = g_path_get_dirname (argv[0]);
#endif

  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/spawn/basics", test_spawn_basics);

  return g_test_run ();
}
