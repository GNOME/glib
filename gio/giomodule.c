/* GIO - GLib Input, Output and Streaming Library
 * 
 * Copyright (C) 2006-2007 Red Hat, Inc.
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
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Alexander Larsson <alexl@redhat.com>
 */

#include <config.h>

#include "giomodule.h"
#include "giomodule-priv.h"

#include "gioalias.h"

/**
 * SECTION:giomodule
 * @short_description: Loadable GIO Modules
 * @include: gio.h
 *
 * Provides an interface and default functions for loading and unloading 
 * modules. This is used internally to make gio extensible, but can also
 * be used by other to implement module loading.
 * 
 **/

struct _GIOModule {
  GTypeModule parent_instance;
  
  gchar       *filename;
  GModule     *library;
  
  void (* load)   (GIOModule *module);
  void (* unload) (GIOModule *module);
};

struct _GIOModuleClass
{
  GTypeModuleClass parent_class;

};

static void      g_io_module_finalize      (GObject      *object);
static gboolean  g_io_module_load_module   (GTypeModule  *gmodule);
static void      g_io_module_unload_module (GTypeModule  *gmodule);

G_DEFINE_TYPE (GIOModule, g_io_module, G_TYPE_TYPE_MODULE);

static void
g_io_module_class_init (GIOModuleClass *class)
{
  GObjectClass     *object_class      = G_OBJECT_CLASS (class);
  GTypeModuleClass *type_module_class = G_TYPE_MODULE_CLASS (class);

  object_class->finalize     = g_io_module_finalize;

  type_module_class->load    = g_io_module_load_module;
  type_module_class->unload  = g_io_module_unload_module;
}

static void
g_io_module_init (GIOModule *module)
{
}

static void
g_io_module_finalize (GObject *object)
{
  GIOModule *module = G_IO_MODULE (object);

  g_free (module->filename);

  G_OBJECT_CLASS (g_io_module_parent_class)->finalize (object);
}

static gboolean
g_io_module_load_module (GTypeModule *gmodule)
{
  GIOModule *module = G_IO_MODULE (gmodule);

  if (!module->filename)
    {
      g_warning ("GIOModule path not set");
      return FALSE;
    }

  module->library = g_module_open (module->filename, G_MODULE_BIND_LAZY | G_MODULE_BIND_LOCAL);

  if (!module->library)
    {
      g_printerr ("%s\n", g_module_error ());
      return FALSE;
    }

  /* Make sure that the loaded library contains the required methods */
  if (! g_module_symbol (module->library,
                         "g_io_module_load",
                         (gpointer *) &module->load) ||
      ! g_module_symbol (module->library,
                         "g_io_module_unload",
                         (gpointer *) &module->unload))
    {
      g_printerr ("%s\n", g_module_error ());
      g_module_close (module->library);

      return FALSE;
    }

  /* Initialize the loaded module */
  module->load (module);

  return TRUE;
}

static void
g_io_module_unload_module (GTypeModule *gmodule)
{
  GIOModule *module = G_IO_MODULE (gmodule);

  module->unload (module);

  g_module_close (module->library);
  module->library = NULL;

  module->load   = NULL;
  module->unload = NULL;
}

/**
 * g_io_module_new:
 * @filename: filename of the shared library module.
 * 
 * Creates a new GIOModule that will load the specific
 * shared library when in use.
 * 
 * Returns: a #GIOModule from given @filename, 
 * or %NULL on error.
 **/
GIOModule *
g_io_module_new (const gchar *filename)
{
  GIOModule *module;

  g_return_val_if_fail (filename != NULL, NULL);

  module = g_object_new (G_IO_TYPE_MODULE, NULL);
  module->filename = g_strdup (filename);

  return module;
}

static gboolean
is_valid_module_name (const gchar *basename)
{
#if !defined(G_OS_WIN32) && !defined(G_WITH_CYGWIN)
  return
    g_str_has_prefix (basename, "lib") &&
    g_str_has_suffix (basename, ".so");
#else
  return g_str_has_suffix (basename, ".dll");
#endif
}

/**
 * g_io_modules_load_all_in_directory:
 * @dirname: pathname for a directory containing modules to load.
 * 
 * Loads all the modules in the the specified directory.
 * 
 * Returns: a list of #GIOModules loaded from the directory
 **/
GList *
g_io_modules_load_all_in_directory (const char *dirname)
{
  const gchar *name;
  GDir        *dir;
  GList *modules;

  if (!g_module_supported ())
    return NULL;

  dir = g_dir_open (dirname, 0, NULL);
  if (!dir)
    return NULL;

  modules = NULL;
  while ((name = g_dir_read_name (dir)))
    {
      if (is_valid_module_name (name))
        {
          GIOModule *module;
          gchar     *path;

          path = g_build_filename (dirname, name, NULL);
          module = g_io_module_new (path);

          if (!g_type_module_use (G_TYPE_MODULE (module)))
            {
              g_printerr ("Failed to load module: %s\n", path);
              g_object_unref (module);
              g_free (path);
              continue;
            }
	  
          g_free (path);

          g_type_module_unuse (G_TYPE_MODULE (module));
	  
          modules = g_list_prepend (modules, module);
        }
    }
  
  g_dir_close (dir);

  return modules;
}

G_LOCK_DEFINE_STATIC (loaded_dirs);

void
_g_io_modules_ensure_loaded (void)
{
  GList *modules;
  const char *directory;
  static gboolean loaded_dirs = FALSE;

  
  G_LOCK (loaded_dirs);

  if (!loaded_dirs)
    {
      loaded_dirs = TRUE;
      modules = g_io_modules_load_all_in_directory (GIO_MODULE_DIR);
    }
  
  G_UNLOCK (loaded_dirs);
}

#define __G_IO_MODULE_C__
#include "gioaliasdef.c"
