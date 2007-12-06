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
#include <string.h>

#include <glib.h>
#include <glib/gstdio.h>

#include "gidlmodule.h"
#include "gidlnode.h"
#include "gidlparser.h"
#include "gmetadata.h"

gboolean raw = FALSE;
gboolean no_init = FALSE;
gchar **input = NULL;
gchar *output = NULL;
gchar *mname = NULL;
gchar *shlib = NULL;
gboolean debug = FALSE;
gboolean verbose = FALSE;

static gchar *
format_output (GMetadata *metadata)
{
  GString *result;
  gint i;

  result = g_string_sized_new (6 * metadata->len);

  g_string_append_printf (result, "#include <stdlib.h>\n");
  g_string_append_printf (result, "#include <girepository.h>\n\n");
  
  g_string_append_printf (result, "const unsigned char _G_METADATA[] = \n{");

  for (i = 0; i < metadata->len; i++)
    {
      if (i > 0)
	g_string_append (result, ", ");

      if (i % 10 == 0)
	g_string_append (result, "\n\t");
      
      g_string_append_printf (result, "0x%.2x", metadata->data[i]);      
    }

  g_string_append_printf (result, "\n};\n\n");
  g_string_append_printf (result, "const gsize _G_METADATA_SIZE = %u;\n\n",
			  (guint)metadata->len);

  if (!no_init)
    {
      g_string_append_printf (result,
			      "__attribute__((constructor)) void\n"
			      "register_metadata (void)\n"
			      "{\n"
			      "\tGMetadata *metadata;\n"
			      "\tmetadata = g_metadata_new_from_const_memory (_G_METADATA, _G_METADATA_SIZE);\n"
			      "\tg_irepository_register (NULL, metadata);\n"
			      "}\n\n");

      g_string_append_printf (result,
			      "__attribute__((destructor)) void\n"
			      "unregister_metadata (void)\n"
			      "{\n"
			      "\tg_irepository_unregister (NULL, \"%s\");\n"
			      "}\n",
			      g_metadata_get_namespace (metadata));
    }

  return g_string_free (result, FALSE);
}

static void
write_out_metadata (gchar *prefix,
		    GMetadata *metadata)
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
    fwrite (metadata->data, 1, metadata->len, file);
  else
    {
      gchar *code;

      code = format_output (metadata);
      fputs (code, file);
      g_free (code);
    }

  if (output != NULL)
    fclose (file);    
}

GLogLevelFlags logged_levels;

static void log_handler (const gchar *log_domain,
			 GLogLevelFlags log_level,
			 const gchar *message,
			 gpointer user_data)
{
  
  if (log_level & logged_levels)
    g_log_default_handler (log_domain, log_level, message, user_data);
}

static GOptionEntry options[] = 
{
  { "raw", 0, 0, G_OPTION_ARG_NONE, &raw, "emit raw metadata", NULL },
  { "code", 0, G_OPTION_FLAG_REVERSE, G_OPTION_ARG_NONE, &raw, "emit C code", NULL },
  { "no-init", 0, 0, G_OPTION_ARG_NONE, &no_init, "do not create _init() function", NULL },
  { "output", 'o', 0, G_OPTION_ARG_FILENAME, &output, "output file", "FILE" }, 
  { "module", 'm', 0, G_OPTION_ARG_STRING, &mname, "module to compile", "NAME" }, 
  { "shared-library", 'l', 0, G_OPTION_ARG_FILENAME, &shlib, "shared library", "FILE" }, 
  { "debug", 0, 0, G_OPTION_ARG_NONE, &debug, "show debug messages", NULL }, 
  { "verbose", 0, 0, G_OPTION_ARG_NONE, &verbose, "show verbose messages", NULL }, 
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

  logged_levels = G_LOG_LEVEL_MASK & ~(G_LOG_LEVEL_MESSAGE|G_LOG_LEVEL_DEBUG);
  if (debug)
    logged_levels = logged_levels | G_LOG_LEVEL_DEBUG;
  if (verbose)
    logged_levels = logged_levels | G_LOG_LEVEL_MESSAGE;

  g_log_set_default_handler (log_handler, NULL);

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
      GMetadata *metadata;

      if (mname && strcmp (mname, module->name) != 0)
	continue;
      if (shlib)
	{
          if (module->shared_library)
	    g_free (module->shared_library);
          module->shared_library = g_strdup (shlib);
	}
      metadata = g_idl_module_build_metadata (module, modules);
      if (metadata == NULL)
	{
	  g_error ("Failed to build metadata for module '%s'\n", module->name);

	  continue;
	}
      if (!g_metadata_validate (metadata, &error))
	g_error ("Invalid metadata for module '%s': %s", 
		 module->name, error->message);


      if (!mname && (m->next || m->prev) && output)
	prefix = module->name;
      else
	prefix = NULL;

      write_out_metadata (prefix, metadata);
      g_metadata_free (metadata);
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
