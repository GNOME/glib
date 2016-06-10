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
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Alexander Larsson <alexl@redhat.com>
 */

#include "config.h"
#include <string.h>
#include "gvfs.h"
#include "glib-private.h"
#include "glocalvfs.h"
#include "gresourcefile.h"
#include "giomodule-priv.h"
#include "glibintl.h"


/**
 * SECTION:gvfs
 * @short_description: Virtual File System
 * @include: gio/gio.h
 *
 * Entry point for using GIO functionality.
 *
 */

/**
 * GVfsURILookupFunc:
 * @vfs: A #GVfs
 * @uri: A URI to look up
 * @user_data: User data passed to g_vfs_register_uri_scheme().
 *
 * Returns: (transfer full): A #GFile for the passed in @uri.
 *
 * Since: 2.50
 */

typedef struct {
  GVfsURILookupFunc func;
  gpointer user_data;
  GDestroyNotify destroy;
} GVfsURILookupFuncClosure;

struct _GVfsPrivate {
  GHashTable *additional_schemes;
};

G_DEFINE_TYPE_WITH_PRIVATE (GVfs, g_vfs, G_TYPE_OBJECT);

static void
g_vfs_dispose (GObject *object)
{
  GVfs *vfs = G_VFS (object);
  GVfsPrivate *priv = g_vfs_get_instance_private (vfs);

  g_clear_pointer (&priv->additional_schemes, g_hash_table_destroy);

  G_OBJECT_CLASS (g_vfs_parent_class)->dispose (object);
}

static void
g_vfs_class_init (GVfsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->dispose = g_vfs_dispose;
}

static GFile *
resource_get_file_for_uri (GVfs *vfs, const char *uri, gpointer user_data)
{
  return _g_resource_file_new (uri);
}

static void
g_vfs_uri_lookup_func_closure_free (GVfsURILookupFuncClosure *closure)
{
  if (closure->destroy)
    closure->destroy (closure->user_data);
  g_free (closure);
}

static void
g_vfs_init (GVfs *vfs)
{
  GVfsPrivate *priv = g_vfs_get_instance_private (vfs);

  priv->additional_schemes = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                    g_free, (GDestroyNotify) g_vfs_uri_lookup_func_closure_free);

  g_vfs_register_uri_scheme (vfs, "resource", resource_get_file_for_uri, NULL, NULL);
}

/**
 * g_vfs_is_active:
 * @vfs: a #GVfs.
 *
 * Checks if the VFS is active.
 *
 * Returns: %TRUE if construction of the @vfs was successful
 *     and it is now active.
 */
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
 * Returns: (transfer full): a #GFile.
 *     Free the returned object with g_object_unref().
 */
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

static GFile *
get_file_for_uri_internal (GVfs *vfs, const char *uri)
{
  GVfsPrivate *priv = g_vfs_get_instance_private (vfs);
  char *scheme;
  GVfsURILookupFuncClosure *closure;

  scheme = g_uri_parse_scheme (uri);
  if (scheme == NULL)
    return NULL;

  closure = g_hash_table_lookup (priv->additional_schemes, scheme);
  g_free (scheme);

  if (closure)
    return closure->func (vfs, uri, closure->user_data);
  else
    return NULL;
}

/**
 * g_vfs_get_file_for_uri:
 * @vfs: a#GVfs.
 * @uri: a string containing a URI
 *
 * Gets a #GFile for @uri.
 *
 * This operation never fails, but the returned object
 * might not support any I/O operation if the URI
 * is malformed or if the URI scheme is not supported.
 *
 * Returns: (transfer full): a #GFile.
 *     Free the returned object with g_object_unref().
 */
GFile *
g_vfs_get_file_for_uri (GVfs       *vfs,
                        const char *uri)
{
  GVfsClass *class;
  GFile *ret;
 
  g_return_val_if_fail (G_IS_VFS (vfs), NULL);
  g_return_val_if_fail (uri != NULL, NULL);

  class = G_VFS_GET_CLASS (vfs);

  ret = get_file_for_uri_internal (vfs, uri);
  if (ret)
    return ret;

  return (* class->get_file_for_uri) (vfs, uri);
}

/**
 * g_vfs_get_supported_uri_schemes:
 * @vfs: a #GVfs.
 *
 * Gets a list of URI schemes supported by @vfs.
 *
 * Returns: (transfer none): a %NULL-terminated array of strings.
 *     The returned array belongs to GIO and must
 *     not be freed or modified.
 */
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
 * Returns: (transfer full): a #GFile for the given @parse_name.
 *     Free the returned object with g_object_unref().
 */
GFile *
g_vfs_parse_name (GVfs       *vfs,
                  const char *parse_name)
{
  GVfsClass *class;

  g_return_val_if_fail (G_IS_VFS (vfs), NULL);
  g_return_val_if_fail (parse_name != NULL, NULL);

  class = G_VFS_GET_CLASS (vfs);

  if (g_str_has_prefix (parse_name, "resource:"))
    return _g_resource_file_new (parse_name);

  return (* class->parse_name) (vfs, parse_name);
}

/**
 * g_vfs_get_default:
 *
 * Gets the default #GVfs for the system.
 *
 * Returns: (transfer none): a #GVfs.
 */
GVfs *
g_vfs_get_default (void)
{
  if (GLIB_PRIVATE_CALL (g_check_setuid) ())
    return g_vfs_get_local ();
  return _g_io_module_get_default (G_VFS_EXTENSION_POINT_NAME,
                                   "GIO_USE_VFS",
                                   (GIOModuleVerifyFunc)g_vfs_is_active);
}

/**
 * g_vfs_get_local:
 *
 * Gets the local #GVfs for the system.
 *
 * Returns: (transfer none): a #GVfs.
 */
GVfs *
g_vfs_get_local (void)
{
  static gsize vfs = 0;

  if (g_once_init_enter (&vfs))
    g_once_init_leave (&vfs, (gsize)_g_local_vfs_new ());

  return G_VFS (vfs);
}

/**
 * g_vfs_register_uri_scheme:
 * @vfs: A #GVfs
 * @scheme: The scheme to register a URI handler on.
 * @func: The lookup function to register.
 * @user_data: (nullable): User data to call the lookup function with.
 * @destroy: (nullable): Called when the closure is destroyed.
 *
 * Registers the scheme @scheme so that when URIs with this scheme are
 * looked up, the registered @func is called. There is currently no way
 * to unregister a scheme. It is undefined if two pieces of code try to
 * register the same scheme.
 *
 * Since: 2.50
 */
void
g_vfs_register_uri_scheme (GVfs              *vfs,
                           const char        *scheme,
                           GVfsURILookupFunc  func,
                           gpointer           user_data,
                           GDestroyNotify     destroy)
{
  GVfsPrivate *priv = g_vfs_get_instance_private (vfs);
  GVfsURILookupFuncClosure *closure;

  closure = g_new (GVfsURILookupFuncClosure, 1);
  closure->func = func;
  closure->user_data = user_data;
  closure->destroy = destroy;

  g_hash_table_replace (priv->additional_schemes, g_strdup (scheme), closure);
}

