/* GObject introspection: Typelib compiler
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

#include <errno.h>
#include <string.h>

#include <glib.h>
#include <glib/gstdio.h>

#ifdef G_OS_WIN32
#include <io.h>
#include <fcntl.h>
#endif

#include "girmodule.h"
#include "girnode.h"
#include "girparser.h"
#include "gtypelib.h"

gboolean code = FALSE;
gboolean no_init = FALSE;
gchar **includedirs = NULL;
gchar **input = NULL;
gchar *output = NULL;
gchar *mname = NULL;
gchar *shlib = NULL;
gboolean include_cwd = FALSE;
gboolean debug = FALSE;
gboolean verbose = FALSE;

static gchar *
format_output (GTypelib *typelib)
{
  GString *result;
  guint i;

  result = g_string_sized_new (6 * typelib->len);

  g_string_append_printf (result, "#include <stdlib.h>\n");
  g_string_append_printf (result, "#include <girepository.h>\n\n");
  
  g_string_append_printf (result, "const unsigned char _G_TYPELIB[] = \n{");

  for (i = 0; i < typelib->len; i++)
    {
      if (i > 0)
	g_string_append (result, ", ");

      if (i % 10 == 0)
	g_string_append (result, "\n\t");
      
      g_string_append_printf (result, "0x%.2x", typelib->data[i]);      
    }

  g_string_append_printf (result, "\n};\n\n");
  g_string_append_printf (result, "const gsize _G_TYPELIB_SIZE = %u;\n\n",
			  (guint)typelib->len);

  if (!no_init)
    {
      g_string_append_printf (result,
			      "__attribute__((constructor)) void "
			      "register_typelib (void);\n\n");
      g_string_append_printf (result,
			      "__attribute__((constructor)) void\n"
			      "register_typelib (void)\n"
			      "{\n"
			      "\tGTypelib *typelib;\n"
			      "\ttypelib = g_typelib_new_from_const_memory (_G_TYPELIB, _G_TYPELIB_SIZE);\n"
			      "\tg_irepository_load_typelib (NULL, typelib, G_IREPOSITORY_LOAD_FLAG_LAZY, NULL);\n"
			      "}\n\n");
    }

  return g_string_free (result, FALSE);
}

static void
write_out_typelib (gchar *prefix,
		   GTypelib *typelib)
{
  FILE *file;

  if (output == NULL)
    {
      file = stdout;
#ifdef G_OS_WIN32
      setmode (fileno (file), _O_BINARY);
#endif
    }
  else
    {
      gchar *filename;

      if (prefix)
	filename = g_strdup_printf ("%s-%s", prefix, output);  
      else
	filename = g_strdup (output);
      file = g_fopen (filename, "wb");

      if (file == NULL)
	{
	  g_fprintf (stderr, "failed to open '%s': %s\n",
		     filename, g_strerror (errno));
	  g_free (filename);

	  return;
	}

      g_free (filename);
    }

  if (!code)
    fwrite (typelib->data, 1, typelib->len, file);
  else
    {
      gchar *code;

      code = format_output (typelib);
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
  { "code", 0, 0, G_OPTION_ARG_NONE, &code, "emit C code", NULL },
  { "no-init", 0, 0, G_OPTION_ARG_NONE, &no_init, "do not create _init() function", NULL },
  { "includedir", 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &includedirs, "include directories in GIR search path", NULL }, 
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
  GIrParser *parser;
  GList *m, *modules;
  gint i;
  g_typelib_check_sanity ();

  context = g_option_context_new ("");
  g_option_context_add_main_entries (context, options, NULL);
  g_option_context_parse (context, &argc, &argv, &error);
  g_option_context_free (context);

  logged_levels = G_LOG_LEVEL_MASK & ~(G_LOG_LEVEL_MESSAGE|G_LOG_LEVEL_DEBUG);
  if (debug)
    logged_levels = logged_levels | G_LOG_LEVEL_DEBUG;
  if (verbose)
    logged_levels = logged_levels | G_LOG_LEVEL_MESSAGE;
  g_log_set_always_fatal (G_LOG_LEVEL_WARNING | G_LOG_LEVEL_CRITICAL);

  g_log_set_default_handler (log_handler, NULL);

  if (!input) 
    { 
      g_fprintf (stderr, "no input files\n"); 

      return 1;
    }

  g_debug ("[parsing] start, %d includes", 
	   includedirs ? g_strv_length (includedirs) : 0);

  g_type_init ();

  if (includedirs != NULL)
    for (i = 0; includedirs[i]; i++)
      g_irepository_prepend_search_path (includedirs[i]);

  parser = g_ir_parser_new ();

  g_ir_parser_set_includes (parser, (const char*const*) includedirs);

  modules = NULL;
  for (i = 0; input[i]; i++)
    {
      GList *mods;
      mods = g_ir_parser_parse_file (parser, input[i], &error);
      
      if (mods == NULL) 
	{
	  g_fprintf (stderr, "error parsing file %s: %s\n", 
		     input[i], error->message);
      
	  return 1;
	}

      modules = g_list_concat (modules, mods);
    }

  g_debug ("[parsing] done");

  g_debug ("[building] start");

  for (m = modules; m; m = m->next)
    {
      GIrModule *module = m->data;
      gchar *prefix;
      GTypelib *typelib;

      if (mname && strcmp (mname, module->name) != 0)
	continue;
      if (shlib)
	{
          if (module->shared_library)
	    g_free (module->shared_library);
          module->shared_library = g_strdup (shlib);
	}

      g_debug ("[building] module %s", module->name);

      typelib = g_ir_module_build_typelib (module, modules);
      if (typelib == NULL)
	{
	  g_error ("Failed to build typelib for module '%s'\n", module->name);

	  continue;
	}
      if (!g_typelib_validate (typelib, &error))
	g_error ("Invalid typelib for module '%s': %s", 
		 module->name, error->message);

      if (!mname && (m->next || m->prev) && output)
	prefix = module->name;
      else
	prefix = NULL;

      write_out_typelib (prefix, typelib);
      g_typelib_free (typelib);
      typelib = NULL;

      /* when writing to stdout, stop after the first module */
      if (m->next && !output && !mname)
	{
	  g_warning ("%d modules omitted\n", g_list_length (modules) - 1);

	  break;
	}
    }

  g_debug ("[building] done");

#if 0
  /* No point */
  g_ir_parser_free (parser);
#endif  

  return 0; 
}
