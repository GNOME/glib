/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 * GObject introspection: IDL generator
 *
 * Copyright (C) 2005 Matthias Clasen
 * Copyright (C) 2008,2009 Red Hat, Inc.
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

#include <glib.h>
#include <glib/gstdio.h>
#include <gmodule.h>

#include "girwriter.h"
#include "girepository.h"
#include "gitypelib-internal.h"

int
main (int argc, char *argv[])
{
  gboolean shlib = FALSE;
  gchar *output = NULL;
  gchar **includedirs = NULL;
  gboolean show_all = FALSE;
  gchar **input = NULL;
  GOptionContext *context;
  GError *error = NULL;
  gboolean needs_prefix;
  gint i;
  GOptionEntry options[] =
    {
      { "shlib", 0, 0, G_OPTION_ARG_NONE, &shlib, "handle typelib embedded in shlib", NULL },
      { "output", 'o', 0, G_OPTION_ARG_FILENAME, &output, "output file", "FILE" },
      { "includedir", 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &includedirs, "include directories in GIR search path", NULL },
      { "all", 0, 0, G_OPTION_ARG_NONE, &show_all, "show all available information", NULL, },
      { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &input, NULL, NULL },
      { NULL, }
    };

  g_log_set_always_fatal (G_LOG_LEVEL_WARNING | G_LOG_LEVEL_CRITICAL);

  g_typelib_check_sanity ();

  context = g_option_context_new ("");
  g_option_context_add_main_entries (context, options, NULL);
  g_option_context_parse (context, &argc, &argv, &error);

  if (!input)
    {
      g_fprintf (stderr, "no input files\n");

      return 1;
    }

  if (includedirs != NULL)
    for (i = 0; includedirs[i]; i++)
      g_irepository_prepend_search_path (includedirs[i]);

  for (i = 0; input[i]; i++)
    {
      GError *error = NULL;
      const char *namespace;
      GMappedFile *mfile;
      GITypelib *typelib;

      mfile = g_mapped_file_new (input[i], FALSE, &error);
      if (!mfile)
	g_error ("failed to read '%s': %s", input[i], error->message);

      if (input[i + 1] && output)
	needs_prefix = TRUE;
      else
	needs_prefix = FALSE;

      typelib = g_typelib_new_from_mapped_file (mfile, &error);
      if (!typelib)
	g_error ("failed to create typelib '%s': %s", input[i], error->message);

      namespace = g_irepository_load_typelib (g_irepository_get_default (), typelib, 0,
					      &error);
      if (namespace == NULL)
	g_error ("failed to load typelib: %s", error->message);
      
      gir_writer_write (output, namespace, needs_prefix, show_all);

      /* when writing to stdout, stop after the first module */
      if (input[i + 1] && !output)
	{
	  g_fprintf (stderr, "warning, %d modules omitted\n",
		     g_strv_length (input) - 1);

	  break;
	}
    }

  return 0;
}
