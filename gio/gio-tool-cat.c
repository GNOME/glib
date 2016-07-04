/*
 * Copyright 2015 Red Hat, Inc.
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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Matthias Clasen <mclasen@redhat.com>
 */

#include "config.h"

#include <gio/gio.h>
#include <gi18n.h>
#include <errno.h>

#ifdef G_OS_WIN32
#include <io.h>
#endif

#ifndef STDOUT_FILENO
#define STDOUT_FILENO 1
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "gio-tool.h"


static const GOptionEntry entries[] = {
  { NULL }
};

static gboolean
cat (GFile *file)
{
  GInputStream *in;
  char buffer[1024 * 8 + 1];
  char *p;
  gssize res;
  gboolean close_res;
  GError *error;
  gboolean success;

  error = NULL;
  in = (GInputStream *) g_file_read (file, NULL, &error);
  if (in == NULL)
    {
      print_file_error (file, error->message);
      g_error_free (error);
      return FALSE;
    }

  success = TRUE;
  while (1)
    {
      res = g_input_stream_read (in, buffer, sizeof (buffer) - 1, NULL, &error);
      if (res > 0)
        {
          gssize written;

          p = buffer;
          while (res > 0)
            {
              written = write (STDOUT_FILENO, p, res);

              if (written == -1 && errno != EINTR)
                {
                  print_file_error (file, "error writing to stdout");
                  success = FALSE;
                  goto out;
                }
              res -= written;
              p += written;
            }
        }
      else if (res < 0)
        {
          print_file_error (file, error->message);
          g_error_free (error);
          error = NULL;
          success = FALSE;
          break;
        }
      else if (res == 0)
        break;
    }

 out:
 close_res = g_input_stream_close (in, NULL, &error);
  if (!close_res)
    {
      print_file_error (file, error->message);
      g_error_free (error);
      success = FALSE;
    }

  return success;
}

int
handle_cat (int argc, char *argv[], gboolean do_help)
{
  GOptionContext *context;
  gchar *param;
  GError *error = NULL;
  int i;
  gboolean res;
  GFile *file;

  g_set_prgname ("gio cat");
  /* Translators: commandline placeholder */
  param = g_strdup_printf ("%s...", _("LOCATION"));
  context = g_option_context_new (param);
  g_free (param);
  g_option_context_set_help_enabled (context, FALSE);
  g_option_context_set_summary (context,
      _("Concatenate files and print to standard output."));
  g_option_context_set_description (context,
      _("gio cat works just like the traditional cat utility, but using GIO\n"
        "locations instead of local files: for example, you can use something\n"
        "like smb://server/resource/file.txt as location."));
  g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);

  if (do_help)
    {
      show_help (context, NULL);
      return 0;
    }

  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      show_help (context, error->message);
      g_error_free (error);
      return 1;
    }

  if (argc < 2)
    {
      show_help (context, _("No files given"));
      return 1;
    }

  g_option_context_free (context);

  res = TRUE;
  for (i = 1; i < argc; i++)
    {
      file = g_file_new_for_commandline_arg (argv[i]);
      res &= cat (file);
      g_object_unref (file);
    }

  return res ? 0 : 2;
}
