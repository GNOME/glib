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
#include "gidlcompilercontext.h"

gboolean no_init = FALSE;
gchar **input = NULL;
gchar *output = NULL;
gchar *mname = NULL;
gchar *shlib = NULL;
gboolean debug = FALSE;
gboolean verbose = FALSE;

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
  GList *c;
  gint entry_id;

  FILE *file;
  GIdlCompilerContext *ctx;

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
      GError *err = NULL;


      if (mname && strcmp (mname, module->name) != 0)
	continue;
      if (shlib)
	{
          if (module->shared_library)
	    g_free (module->shared_library);
          module->shared_library = g_strdup (shlib);
	}

      if (!mname && (m->next || m->prev) && output)
	prefix = module->name;
      else
	prefix = NULL;

      ctx = g_idl_compiler_context_new (module->name, &err);
      if (err != NULL) 
	{
	  g_fprintf (stderr, "Error creating new compiler context: %s",
		     err->message);

	  return 1;
	}

      /* This is making sure all the types
       * that have local directory entries are already
       * in the entries database.
       *
       * A method of finding out if an external reference is
       * needed
       */
      for (c = module->entries; c; c = c->next)
        {
          GIdlNode *node = (GIdlNode*) c->data;

          g_idl_compiler_add_entry (ctx, node);
        }

      for (c = module->entries; c; c = c->next)
        {
          GIdlNode *node = (GIdlNode*) c->data;

          entry_id = g_idl_compiler_get_entry_id (ctx, node->name);

          g_idl_compiler_write_node (node, entry_id, ctx);
        }

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

      g_idl_compiler_context_finalize (ctx, file, module->shared_library, &err);

      g_idl_compiler_context_destroy (ctx);

      /* when writing to stdout, stop after the first module */
      if (m->next && !output && !mname)
	{
	  g_warning ("%d modules omitted\n", g_list_length (modules) - 1);

	  break;
	}
    }
             
  return 0; 
}
