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


static void
run_tests (void)
{
  GError *err;
  gchar *output = NULL;
  
  printf ("The following errors are supposed to occur:\n");

  err = NULL;
  if (!g_spawn_command_line_sync ("nonexistent_application foo 'bar baz' blah blah",
                                  NULL, NULL, NULL,
                                  &err))
    {
      fprintf (stderr, "Error (normal, supposed to happen): %s\n", err->message);
      g_error_free (err);
    }

  err = NULL;
  if (!g_spawn_command_line_async ("nonexistent_application foo bar baz \"blah blah\"",
                                   &err))
    {
      fprintf (stderr, "Error (normal, supposed to happen): %s\n", err->message);
      g_error_free (err);
    }

  printf ("Errors after this are not supposed to happen:\n");
  
  err = NULL;
#ifdef G_OS_UNIX
  if (!g_spawn_command_line_sync ("/bin/sh -c 'echo hello'",
                                  &output, NULL, NULL,
                                  &err))
    {
      fprintf (stderr, "Error: %s\n", err->message);
      g_error_free (err);
      exit (1);
    }
  else
    {
      g_assert (output != NULL);
      
      if (strcmp (output, "hello\n") != 0)
        {
          printf ("output was '%s', should have been 'hello'\n",
                  output);

          exit (1);
        }

      g_free (output);
    }
#else
#ifdef G_OS_WIN32
  if (!g_spawn_command_line_sync ("ipconfig /all",
                                  &output, NULL, NULL,
                                  &err))
    {
      fprintf (stderr, "Error: %s\n", err->message);
      g_error_free (err);
      exit (1);
    }
  else
    {
      g_assert (output != NULL);
      
      if (strstr (output, "IP Configuration") == 0)
        {
          printf ("output was '%s', should have contained 'IP Configuration'\n",
                  output);

          exit (1);
        }

      g_free (output);
    }

#endif
#endif
}

int
main (int   argc,
      char *argv[])
{
  run_tests ();
  
  return 0;
}
