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
#include <dl.h>


/* some flags are missing on some systems, so we provide
 * harmless defaults.
 */
#ifndef	DYNAMIC_PATH
#define	DYNAMIC_PATH	0
#endif	/* DYNAMIC_PATH */

/* should we have BIND_TOGETHER here as well? */
#define	CONST_BIND_FLAGS	(BIND_NONFATAL | BIND_VERBOSE)


/* --- functions --- */
static gpointer
_g_module_open (const gchar    *file_name,
		gboolean        bind_lazy)
{
  gpointer handle;

  handle = shl_load (file_name, CONST_BIND_FLAGS | (bind_lazy ? BIND_DEFERRED : BIND_IMMEDIATE), 0);
  if (!handle)
    g_module_set_error (g_strerror (errno));

  return handle;
}

static gpointer
_g_module_self (void)
{
  gpointer handle;

  handle = PROG_HANDLE;
  if (!handle)
    g_module_set_error (g_strerror (errno));

  return handle;
}

static void
_g_module_close (gpointer        *handle_p,
		 gboolean         is_unref)
{
  if (!is_unref)
    {
      if (shl_unload (*handle_p) != 0)
	g_module_set_error (g_strerror (errno));
    }
}

static gpointer
_g_module_symbol (gpointer       *handle_p,
		  const gchar    *symbol_name)
{
  gpointer p = NULL;

  /* should we restrict lookups to TYPE_PROCEDURE?
   */
  if (shl_findsym (handle_p, symbol_name, TYPE_UNDEFINED, &p) != 0 || p == NULL)
    g_module_set_error (g_strerror (errno));
  
  return p;
}
