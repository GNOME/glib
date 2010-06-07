
/* -*- Mode: C; c-file-style: "gnu"; -*- */
/* GObject introspection: IDL generator
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

static const guchar *
load_typelib (const gchar  *filename,
	      GModule     **dlhandle,
	      gsize        *len)
{
  guchar *typelib;
  gsize *typelib_size;
  GModule *handle;

  handle = g_module_open (filename, G_MODULE_BIND_LOCAL|G_MODULE_BIND_LAZY);
  if (handle == NULL)
    {
      g_printerr ("Could not load typelib from '%s': %s\n",
		  filename, g_module_error ());
      return NULL;
    }

  if (!g_module_symbol (handle, "_G_TYPELIB", (gpointer *) &typelib))
    {
      g_printerr ("Could not load typelib from '%s': %s\n",
		  filename, g_module_error ());
      return NULL;
    }

  if (!g_module_symbol (handle, "_G_TYPELIB_SIZE", (gpointer *) &typelib_size))
    {
      g_printerr ("Could not load typelib from '%s': %s\n",
		  filename, g_module_error ());
      return NULL;
    }

  *len = *typelib_size;

  if (dlhandle)
    *dlhandle = handle;

  return typelib;
}

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
  GTypelib *data;
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

  g_type_init ();

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
      GModule *dlhandle = NULL;
      const guchar *typelib;
      gsize len;
      const char *namespace;

      if (!shlib)
	{
	  if (!g_file_get_contents (input[i], (gchar **)&typelib, &len, &error))
	    {
	      g_fprintf (stderr, "failed to read '%s': %s\n",
			 input[i], error->message);
	      g_clear_error (&error);
	      continue;
	    }
	}
      else
	{
	  typelib = load_typelib (input[i], &dlhandle, &len);
	  if (!typelib)
	    {
	      g_fprintf (stderr, "failed to load typelib from '%s'\n",
			 input[i]);
	      continue;
	    }
	}

      if (input[i + 1] && output)
	needs_prefix = TRUE;
      else
	needs_prefix = FALSE;

      data = g_typelib_new_from_const_memory (typelib, len);
      {
        GError *error = NULL;
        if (!g_typelib_validate (data, &error)) {
          g_printerr ("typelib not valid: %s\n", error->message);
          g_clear_error (&error);
	  return 1;
        }
      }
      namespace = g_irepository_load_typelib (g_irepository_get_default (), data, 0,
					      &error);
      if (namespace == NULL)
	{
	  g_printerr ("failed to load typelib: %s\n", error->message);
	  return 1;
	}

      gir_writer_write (output, namespace, needs_prefix, show_all);

      if (dlhandle)
	{
	  g_module_close (dlhandle);
	  dlhandle = NULL;
	}

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
