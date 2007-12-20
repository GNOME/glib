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
#include <string.h>
#include "gvfs.h"
#include "glocalvfs.h"
#include "giomodule-priv.h"
#include "glibintl.h"

#include "gioalias.h"

/**
 * SECTION:gvfs
 * @short_description: Virtual File System 
 * @include: gio.h
 * 
 * Entry point for using GIO functionality.
 *
 **/

G_DEFINE_TYPE (GVfs, g_vfs, G_TYPE_OBJECT);

static void
g_vfs_class_init (GVfsClass *klass)
{
}

static void
g_vfs_init (GVfs *vfs)
{
}

/**
 * g_vfs_is_active:
 * @vfs: a #GVfs.
 * 
 * Checks if the VFS is active.
 * 
 * Returns: %TRUE if construction of the @vfs was successful and it is now active.
 **/
gboolean
g_vfs_is_active (GVfs *vfs)
{
  GVfsClass *class;

  g_return_val_if_fail (G_IS_VFS (vfs), FALSE);

  class = G_VFS_GET_CLASS (vfs);

  return (* class->is_active) (vfs);
}


/**
 * g_vfs_get_file_for_path:
 * @vfs: a #GVfs.
 * @path: a string containing a VFS path.
 * 
 * Gets a #GFile for @path.
 * 
 * Returns: a #GFile.
 **/
GFile *
g_vfs_get_file_for_path (GVfs       *vfs,
			 const char *path)
{
  GVfsClass *class;
  
  g_return_val_if_fail (G_IS_VFS (vfs), NULL);
  g_return_val_if_fail (path != NULL, NULL);

  class = G_VFS_GET_CLASS (vfs);

  return (* class->get_file_for_path) (vfs, path);
}

/**
 * g_vfs_get_file_for_uri:
 * @vfs: a#GVfs.
 * @uri: a string containing a URI path.
 * 
 * Gets a #GFile for @uri.
 * 
 * This operation never fails, but the returned object
 * might not support any I/O operation if the uri
 * is malformed or if the uri type is not supported.
 * 
 * Returns: a #GFile. 
 * 
 **/
GFile *
g_vfs_get_file_for_uri (GVfs       *vfs,
			const char *uri)
{
  GVfsClass *class;
  
  g_return_val_if_fail (G_IS_VFS (vfs), NULL);
  g_return_val_if_fail (uri != NULL, NULL);

  class = G_VFS_GET_CLASS (vfs);

  return (* class->get_file_for_uri) (vfs, uri);
}

/**
 * g_vfs_get_supported_uri_schemes:
 * @vfs: a #GVfs.
 * 
 * Gets a list of URI schemes supported by @vfs.
 * 
 * Returns: a list of strings.
 **/
const gchar * const *
g_vfs_get_supported_uri_schemes (GVfs *vfs)
{
  GVfsClass *class;
  
  g_return_val_if_fail (G_IS_VFS (vfs), NULL);

  class = G_VFS_GET_CLASS (vfs);

  return (* class->get_supported_uri_schemes) (vfs);
}

/**
 * g_vfs_parse_name:
 * @vfs: a #GVfs.
 * @parse_name: a string to be parsed by the VFS module.
 * 
 * This operation never fails, but the returned object might 
 * not support any I/O operations if the @parse_name cannot 
 * be parsed by the #GVfs module.
 * 
 * Returns: a #GFile for the given @parse_name.
 **/
GFile *
g_vfs_parse_name (GVfs       *vfs,
		  const char *parse_name)
{
  GVfsClass *class;
  
  g_return_val_if_fail (G_IS_VFS (vfs), NULL);
  g_return_val_if_fail (parse_name != NULL, NULL);

  class = G_VFS_GET_CLASS (vfs);

  return (* class->parse_name) (vfs, parse_name);
}

/* Note: This compares in reverse order.
   Higher prio -> sort first
 */
static gint
compare_vfs_type (gconstpointer  a,
		  gconstpointer  b,
		  gpointer       user_data)
{
  GType a_type, b_type;
  char *a_name, *b_name;
  int a_prio, b_prio;
  gint res;
  const char *use_this_monitor;
  GQuark private_q, name_q;

  private_q = g_quark_from_static_string ("gio-prio");
  name_q = g_quark_from_static_string ("gio-name");
  
  use_this_monitor = user_data;
  a_type = *(GType *)a;
  b_type = *(GType *)b;
  a_prio = GPOINTER_TO_INT (g_type_get_qdata (a_type, private_q));
  a_name = g_type_get_qdata (a_type, name_q);
  b_prio = GPOINTER_TO_INT (g_type_get_qdata (b_type, private_q));
  b_name = g_type_get_qdata (b_type, name_q);

  if (a_type == b_type)
    res = 0;
  else if (use_this_monitor != NULL &&
	   strcmp (a_name, use_this_monitor) == 0)
    res = -1;
  else if (use_this_monitor != NULL &&
	   strcmp (b_name, use_this_monitor) == 0)
    res = 1;
  else 
    res = b_prio - a_prio;
  
  return res;
}

static gpointer
get_default_vfs (gpointer arg)
{
  GType *vfs_impls;
  int i;
  guint n_vfs_impls;
  const char *use_this;
  GVfs *vfs;

  use_this = g_getenv ("GIO_USE_VFS");
  
  /* Ensure vfs in modules loaded */
  _g_io_modules_ensure_loaded ();

  vfs_impls = g_type_children (G_TYPE_VFS, &n_vfs_impls);

  g_qsort_with_data (vfs_impls, n_vfs_impls, sizeof (GType),
		     compare_vfs_type, (gpointer)use_this);
  
  for (i = 0; i < n_vfs_impls; i++)
    {
      vfs = g_object_new (vfs_impls[i], NULL);

      if (g_vfs_is_active (vfs))
	break;

      g_object_unref (vfs);
      vfs = NULL;
    }
  
  g_free (vfs_impls);

  return vfs;
}

/**
 * g_vfs_get_default:
 * 
 * Gets the default #GVfs for the system.
 * 
 * Returns: a #GVfs. 
 **/
GVfs *
g_vfs_get_default (void)
{
  static GOnce once_init = G_ONCE_INIT;
  
  return g_once (&once_init, get_default_vfs, NULL);
}

/**
 * g_vfs_get_local:
 * 
 * Gets the local #GVfs for the system.
 * 
 * Returns: a #GVfs.
 **/
GVfs *
g_vfs_get_local (void)
{
  static gsize vfs = 0;

  if (g_once_init_enter (&vfs))
    g_once_init_leave (&vfs, (gsize)_g_local_vfs_new ());

  return G_VFS (vfs);
}

#define __G_VFS_C__
#include "gioaliasdef.c"
