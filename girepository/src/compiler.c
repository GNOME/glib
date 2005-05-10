/* GObject introspection: Metadata compiler
 *
 * Copyright (C) 2005 Matthias Clasen
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

#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>

#include <glib.h>
#include <glib/gstdio.h>

#include "gidlmodule.h"
#include "gidlnode.h"
#include "gidlparser.h"

gboolean raw = FALSE;
gboolean no_init = FALSE;
gchar **input = NULL;
gchar *output = NULL;
gchar *mname = NULL;

static gchar *
format_output (guchar *metadata,
	       gsize   len)
{
  GString *result;
  gint i;

  result = g_string_sized_new (6 * len);

  g_string_append_printf (result, "const unsigned char _G_METADATA[] = \n{");

  for (i = 0; i < len; i++)
    {
      if (i > 0)
	g_string_append (result, ", ");

      if (i % 10 == 0)
	g_string_append (result, "\n\t");
      
      g_string_append_printf (result, "0x%.2x", metadata[i]);      
    }

  g_string_append_printf (result, "\n};\n\n");

  if (!no_init)
    {
      g_string_append_printf (result,
			      "void\n"
			      "register_metadata (void) __attribute__((constructor))\n"
			      "{\n"
			      "\tg_irepository_register (NULL, _G_METADATA);\n"
			      "}\n\n");

      g_string_append_printf (result,
			      "void\n"
			      "unregister_metadata (void) __attribute__((destructor))\n"
			      "{\n"
			      "\tg_irepository_unregister (NULL, _G_METADATA);\n"
			      "}\n");
    }

  return g_string_free (result, FALSE);
}

static void
write_out_metadata (gchar *prefix,
		    gchar *metadata,
		    gsize  len)
{
  FILE *file;

  if (output == NULL)
    file = stdout;
  else
    {
      gchar *filename;

      if (prefix)
	filename = g_strdup_printf ("%s-%s", prefix, output);  
      else
	filename = g_strdup (output);
      file = g_fopen (filename, "w");

      if (file == NULL)
	{
	  g_fprintf (stderr, "failed to open '%s': %s\n",
		     filename, g_strerror (errno));
	  g_free (filename);

	  return;
	}

      g_free (filename);
    }

  if (raw)
    fwrite (metadata, 1, len, file);
  else
    {
      gchar *code;

      code = format_output (metadata, len);
      fputs (code, file);
      g_free (code);
    }

  if (output != NULL)
    fclose (file);    
}

static GOptionEntry options[] = 
{
  { "raw", 0, 0, G_OPTION_ARG_NONE, &raw, "emit raw metadata", NULL },
  { "code", 0, G_OPTION_FLAG_REVERSE, G_OPTION_ARG_NONE, &raw, "emit C code", NULL },
  { "no-init", 0, 0, G_OPTION_ARG_NONE, &no_init, "do not create _init() function", NULL },
  { "output", 'o', 0, G_OPTION_ARG_FILENAME, &output, "output file", "FILE" }, 
  { "module", 'm', 0, G_OPTION_ARG_STRING, &mname, "module to compile", "NAME" }, 
  { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &input, NULL, NULL },
  { NULL, }
};

int
main (int argc, char ** argv)
{
  GOptionContext *context;
  GError *error = NULL;
  GList *m, *modules; 
  gint i;

  g_metadata_check_sanity ();

  context = g_option_context_new ("");
  g_option_context_add_main_entries (context, options, NULL);
  g_option_context_parse (context, &argc, &argv, &error);
  g_option_context_free (context);

  if (!input) 
    { 
      g_fprintf (stderr, "no input files\n"); 

      return 1;
    }

  modules = NULL;
  for (i = 0; input[i]; i++)
    {
      GList *mods;
      mods = g_idl_parse_file (input[i], &error);
      
      if (mods == NULL) 
	{
	  g_fprintf (stderr, "error parsing file %s: %s\n", 
		     input[i], error->message);
      
	  return 1;
	}

      modules = g_list_concat (modules, mods);
    }

  for (m = modules; m; m = m->next)
    {
      GIdlModule *module = m->data;
      gchar *prefix;
      guchar *metadata;
      gsize len;

      if (mname && strcmp (mname, module->name) != 0)
	continue;

      g_idl_module_build_metadata (module, modules, &metadata, &len);
      if (metadata == NULL)
	{
	  g_error ("failed to build metadata for module '%s'\n", module->name);

	  continue;
	}

      if (!mname && (m->next || m->prev) && output)
	prefix = module->name;
      else
	prefix = NULL;

      write_out_metadata (prefix, metadata, len);
      g_free (metadata);
      metadata = NULL;

      /* when writing to stdout, stop after the first module */
      if (m->next && !output && !mname)
	{
	  g_warning ("%d modules omitted\n", g_list_length (modules) - 1);

	  break;
	}
    }
             
  return 0; 
}
