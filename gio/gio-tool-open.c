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

#include "gio-tool.h"


static const GOptionEntry entries[] = {
  { NULL }
};

int
handle_open (int argc, char *argv[], gboolean do_help)
{
  GOptionContext *context;
  gchar *param;
  GError *error = NULL;
  int i;
  gboolean success;
  gboolean res;

  g_set_prgname ("gio open");

  /* Translators: commandline placeholder */
  param = g_strdup_printf ("%s...", _("LOCATION"));
  context = g_option_context_new (param);
  g_free (param);
  g_option_context_set_help_enabled (context, FALSE);
  g_option_context_set_summary (context,
      _("Open files with the default application that\n"
        "is registered to handle files of this type."));
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
      show_help (context, _("No files to open"));
      return 1;
    }

  g_option_context_free (context);

  success = TRUE;
  for (i = 1; i < argc; i++)
    {
      GFile *file;
      char *uri;

      file = g_file_new_for_commandline_arg (argv[i]);
      uri = g_file_get_uri (file);
      res = g_app_info_launch_default_for_uri (uri, NULL, &error);

      if (!res)
	{
          print_file_error (file, error->message);
	  g_clear_error (&error);
	  success = FALSE;
	}

      g_object_unref (file);
      g_free (uri);
    }

  return success ? 0 : 2;
}
