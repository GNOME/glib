/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 * GObject introspection: IDL generator
 *
 * Copyright (C) 2005 Matthias Clasen
 * Copyright (C) 2008,2009 Red Hat, Inc.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
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

#include <locale.h>

#include <glib.h>
#include <glib/gstdio.h>
#include <gmodule.h>
#include <girepository.h>

#include "girwriter-private.h"
#include "gitypelib-internal.h"

int
main (int argc, char *argv[])
{
  GIRepository *repository = NULL;
  gboolean shlib = FALSE;
  gchar *output = NULL;
  gchar **includedirs = NULL;
  gboolean show_all = FALSE;
  gchar **input = NULL;
  GOptionContext *context;
  GError *error = NULL;
  gboolean needs_prefix;
  gboolean show_version = FALSE;
  gint i;
  GOptionEntry options[] =
    {
      { "shlib", 0, 0, G_OPTION_ARG_NONE, &shlib, "handle typelib embedded in shlib", NULL },
      { "output", 'o', 0, G_OPTION_ARG_FILENAME, &output, "output file", "FILE" },
      { "includedir", 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &includedirs, "include directories in GIR search path", NULL },
      { "all", 0, 0, G_OPTION_ARG_NONE, &show_all, "show all available information", NULL, },
      { "version", 0, 0, G_OPTION_ARG_NONE, &show_version, "show program's version number and exit", NULL },
      { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &input, NULL, NULL },
      G_OPTION_ENTRY_NULL
    };

  g_log_set_always_fatal (G_LOG_LEVEL_WARNING | G_LOG_LEVEL_CRITICAL);

  setlocale (LC_ALL, "");

  context = g_option_context_new ("");
  g_option_context_add_main_entries (context, options, NULL);
  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_fprintf (stderr, "failed to parse: %s\n", error->message);
      g_error_free (error);
      return 1;
    }

  if (show_version)
    {
      g_printf ("gi-decompile-typelib %u.%u.%u\n",
                GLIB_MAJOR_VERSION, GLIB_MINOR_VERSION, GLIB_MICRO_VERSION);
      return 0;
    }

  if (!input)
    {
      g_fprintf (stderr, "no input files\n");

      return 1;
    }

  repository = gi_repository_new ();

  if (includedirs != NULL)
    for (i = 0; includedirs[i]; i++)
      gi_repository_prepend_search_path (repository, includedirs[i]);

  for (i = 0; input[i]; i++)
    {
      const char *namespace;
      GMappedFile *mfile = NULL;
      GBytes *bytes = NULL;
      GITypelib *typelib;

      error = NULL;
      mfile = g_mapped_file_new (input[i], FALSE, &error);
      if (!mfile)
	g_error ("failed to read '%s': %s", input[i], error->message);

      bytes = g_mapped_file_get_bytes (mfile);
      g_clear_pointer (&mfile, g_mapped_file_unref);

      if (input[i + 1] && output)
	needs_prefix = TRUE;
      else
	needs_prefix = FALSE;

      typelib = gi_typelib_new_from_bytes (bytes, &error);
      if (!typelib)
	g_error ("failed to create typelib '%s': %s", input[i], error->message);

      namespace = gi_repository_load_typelib (repository, typelib, 0, &error);
      if (namespace == NULL)
	g_error ("failed to load typelib: %s", error->message);

      gi_ir_writer_write (output, namespace, needs_prefix, show_all);

      /* when writing to stdout, stop after the first module */
      if (input[i + 1] && !output)
	{
	  g_fprintf (stderr, "warning, %d modules omitted\n",
		     g_strv_length (input) - 1);

	  break;
	}
    }

  g_clear_object (&repository);

  return 0;
}
