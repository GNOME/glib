/* GMODULE - GLIB wrapper code for dynamic module loading
 * Copyright (C) 1998 Tim Janik
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#include	"gmodule.h"
#include	"gmoduleconf.h"
#include	<errno.h>
#include	<string.h>


/* We maintain a list of modules, so we can reference count them.
 * That's needed because some platforms don't support refernce counts on
 * modules (http://www.stat.umn.edu/~luke/xls/projects/dlbasics/dlbasics.html).
 * Also, the module for the program itself is kept seperatedly for
 * faster access.
 */


/* --- structures --- */
struct _GModule
{
  gchar	*file_name;
  gpointer handle;
  guint ref_count;
  GModuleDeInit de_init;
  GModule *next;
};


/* --- prototypes --- */
static gpointer		_g_module_open		(const gchar	*file_name,
						 gboolean	 bind_lazy);
static void		_g_module_close		(gpointer	*handle_p,
						 gboolean	 is_unref);
static gpointer		_g_module_self		(void);
static gpointer		_g_module_symbol	(gpointer	*handle_p,
						 const gchar	*symbol_name);
static inline void	g_module_set_error	(const gchar	*error);
static inline GModule*	g_module_find_by_handle (gpointer	 handle);
static inline GModule*	g_module_find_by_name	(const gchar	*name);


/* --- variables --- */
static GModule	*modules = NULL;
static GModule	*main_module = NULL;
static gchar	*module_error = NULL;


/* --- inline functions --- */
static inline GModule*
g_module_find_by_handle (gpointer handle)
{
  GModule *module;

  for (module = modules; module; module = module->next)
    if (handle == module->handle)
      return module;
  return NULL;
}

static inline GModule*
g_module_find_by_name (const gchar *name)
{
  GModule *module;

  for (module = modules; module; module = module->next)
    if (strcmp (name, module->file_name) == 0)
      return module;
  return NULL;
}

static inline void
g_module_set_error (const gchar *error)
{
  if (module_error)
    g_free (module_error);
  if (error)
    module_error = g_strdup (error);
  else
    module_error = NULL;
  errno = 0;
}


/* --- include platform specifc code --- */
#define	CHECK_ERROR(rv)	{ g_module_set_error (NULL); }
#if	(G_MODULE_IMPL == G_MODULE_IMPL_DL)
#include "gmodule-dl.c"
#elif	(G_MODULE_IMPL == G_MODULE_IMPL_DLD)
#include "gmodule-dld.c"
#else
#undef	CHECK_ERROR
#define	CHECK_ERROR(rv)	{ g_module_set_error ("unsupported"); return rv; }
#endif	/* no implementation */


/* --- functions --- */
gboolean
g_module_supported (void)
{
  CHECK_ERROR (FALSE);

  return TRUE;
}

GModule*
g_module_open (const gchar    *file_name,
	       GModuleFlags    flags)
{
  GModule *module;
  gpointer handle;

  CHECK_ERROR (NULL);

  if (!file_name)
    {
      if (!main_module)
	{
	  handle = _g_module_self ();
	  if (handle)
	    {
	      main_module = g_new (GModule, 1);
	      main_module->file_name = NULL;
	      main_module->handle = handle;
	      main_module->ref_count = 1;
	      main_module->de_init = NULL;
	      main_module->next = NULL;
	    }
	}

      return main_module;
    }

  /* we first search the module list by name */
  module = g_module_find_by_name (file_name);
  if (module)
    {
      module->ref_count++;

      return module;
    }

  /* open the module */
  handle = _g_module_open (file_name, (flags & G_MODULE_BIND_LAZY) != 0);
  if (handle)
    {
      gchar *saved_error;
      GModuleCheckInit check_init;
      gboolean check_failed = FALSE;

      /* search the module list by handle, since file names are not unique */
      module = g_module_find_by_handle (handle);
      if (module)
	{
	  _g_module_close (&module->handle, TRUE);
	  module->ref_count++;
	  g_module_set_error (NULL);

	  return module;
	}

      saved_error = module_error;
      module_error = NULL;
      g_module_set_error (NULL);

      module = g_new (GModule, 1);
      module->file_name = g_strdup (file_name);
      module->handle = handle;
      module->ref_count = 0;
      module->de_init = NULL;
      module->next = NULL;

      /* check initialization */
      if (g_module_symbol (module, "g_module_check_init", &check_init))
	check_failed = check_init (module);

      /* should call de_init() on failed initializations also? */
      if (!check_failed)
	g_module_symbol (module, "g_module_de_init", &module->de_init);

      module->ref_count += 1;
      module->next = modules;
      modules = module;

      if (check_failed)
	{
	  g_module_close (module);
	  module = NULL;
	  g_module_set_error ("GModule initialization check failed");
	}
      else
	g_module_set_error (saved_error);
      g_free (saved_error);
    }

  return module;
}

gboolean
g_module_close (GModule        *module)
{
  CHECK_ERROR (FALSE);

  g_return_val_if_fail (module != NULL, FALSE);
  g_return_val_if_fail (module->ref_count > 0, FALSE);

  if (module != main_module)
    module->ref_count--;

  if (!module->ref_count)
    {
      GModule *last;
      GModule *node;

      last = NULL;
      node = modules;
      while (node)
	{
	  if (node == module)
	    {
	      if (last)
		last->next = node->next;
	      else
		modules = node->next;
	      break;
	    }
	  last = node;
	  node = last->next;
	}
      module->next = NULL;

      if (module->de_init)
	module->de_init (module);

      _g_module_close (&module->handle, FALSE);
      g_free (module->file_name);

      g_free (module);
    }

  return module_error == NULL;
}

gchar*
g_module_error (void)
{
  return module_error;
}

gboolean
g_module_symbol (GModule        *module,
		 const gchar    *symbol_name,
		 gconstpointer  *symbol)
{
  if (symbol)
    *symbol = NULL;
  CHECK_ERROR (FALSE);

  g_return_val_if_fail (module != NULL, FALSE);
  g_return_val_if_fail (symbol_name != NULL, FALSE);
  g_return_val_if_fail (symbol != NULL, FALSE);

#ifdef	G_MODULE_NEED_USCORE
  symbol_name = g_strconcat ("_", symbol_name, NULL);
  *symbol = _g_module_symbol (&module->handle, symbol_name);
  g_free (symbol_name);
#else	/* !G_MODULE_NEED_USCORE */
  *symbol = _g_module_symbol (&module->handle, symbol_name);
#endif	/* !G_MODULE_NEED_USCORE */

  if (module_error)
    {
      *symbol = NULL;
      return FALSE;
    }

  return TRUE;
}

gchar*
g_module_name (GModule *module)
{
  g_return_val_if_fail (module != NULL, NULL);

  if (module == main_module)
    return "main";

  return module->file_name;
}
