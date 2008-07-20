/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

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
 *         David Zeuthen <davidz@redhat.com>
 */

#include <config.h>

#include <string.h>

#include "gmount.h"
#include "gmountprivate.h"
#include "gsimpleasyncresult.h"
#include "glibintl.h"

#include "gioalias.h"

/**
 * SECTION:gmount
 * @short_description: Mount management
 * @include: gio/gio.h
 * @see also: GVolume, GUnixMount
 *
 * The #GMount interface represents user-visible mounts. Note, when 
 * porting from GnomeVFS, #GMount is the moral equivalent of #GnomeVFSVolume.
 *
 * #GMount is a "mounted" filesystem that you can access. Mounted is in
 * quotes because it's not the same as a unix mount, it might be a gvfs
 * mount, but you can still access the files on it if you use GIO. Might or
 * might not be related to a volume object.
 * 
 * Unmounting a #GMount instance is an asynchronous operation. For
 * more information about asynchronous operations, see #GAsyncReady
 * and #GSimpleAsyncReady. To unmount a #GMount instance, first call
 * g_mount_unmount() with (at least) the #GMount instance and a
 * #GAsyncReadyCallback.  The callback will be fired when the
 * operation has resolved (either with success or failure), and a
 * #GAsyncReady structure will be passed to the callback.  That
 * callback should then call g_mount_unmount_finish() with the #GMount
 * and the #GAsyncReady data to see if the operation was completed
 * successfully.  If an @error is present when g_mount_unmount_finish() 
 * is called, then it will be filled with any error information.
 **/

static void g_mount_base_init (gpointer g_class);
static void g_mount_class_init (gpointer g_class,
                                gpointer class_data);

GType
g_mount_get_type (void)
{
  static volatile gsize g_define_type_id__volatile = 0;

  if (g_once_init_enter (&g_define_type_id__volatile))
    {
      const GTypeInfo mount_info =
      {
        sizeof (GMountIface), /* class_size */
	g_mount_base_init,   /* base_init */
	NULL,		/* base_finalize */
	g_mount_class_init,
	NULL,		/* class_finalize */
	NULL,		/* class_data */
	0,
	0,              /* n_preallocs */
	NULL
      };
      GType g_define_type_id =
	g_type_register_static (G_TYPE_INTERFACE, I_("GMount"),
				&mount_info, 0);

      g_type_interface_add_prerequisite (g_define_type_id, G_TYPE_OBJECT);

      g_once_init_leave (&g_define_type_id__volatile, g_define_type_id);
    }

  return g_define_type_id__volatile;
}

static void
g_mount_class_init (gpointer g_class,
                    gpointer class_data)
{
}

static void
g_mount_base_init (gpointer g_class)
{
  static gboolean initialized = FALSE;

  if (! initialized)
    {
     /**
      * GMount::changed:
      * 
      * Emitted when the mount has been changed.
      **/
      g_signal_new (I_("changed"),
                    G_TYPE_MOUNT,
                    G_SIGNAL_RUN_LAST,
                    G_STRUCT_OFFSET (GMountIface, changed),
                    NULL, NULL,
                    g_cclosure_marshal_VOID__VOID,
                    G_TYPE_NONE, 0);

     /**
      * GMount::unmounted:
      * 
      * This signal is emitted when the #GMount have been
      * unmounted. If the recipient is holding references to the
      * object they should release them so the object can be
      * finalized.
      **/
      g_signal_new (I_("unmounted"),
                    G_TYPE_MOUNT,
                    G_SIGNAL_RUN_LAST,
                    G_STRUCT_OFFSET (GMountIface, unmounted),
                    NULL, NULL,
                    g_cclosure_marshal_VOID__VOID,
                    G_TYPE_NONE, 0);

      initialized = TRUE;
    }
}

/**
 * g_mount_get_root:
 * @mount: a #GMount.
 * 
 * Gets the root directory on @mount.
 * 
 * Returns: a #GFile.
 **/
GFile *
g_mount_get_root (GMount *mount)
{
  GMountIface *iface;

  g_return_val_if_fail (G_IS_MOUNT (mount), NULL);

  iface = G_MOUNT_GET_IFACE (mount);

  return (* iface->get_root) (mount);
}

/**
 * g_mount_get_name:
 * @mount: a #GMount.
 * 
 * Gets the name of @mount.
 * 
 * Returns: the name for the given @mount. The returned string should 
 * be freed when no longer needed.
 **/
char *
g_mount_get_name (GMount *mount)
{
  GMountIface *iface;

  g_return_val_if_fail (G_IS_MOUNT (mount), NULL);

  iface = G_MOUNT_GET_IFACE (mount);

  return (* iface->get_name) (mount);
}

/**
 * g_mount_get_icon:
 * @mount: a #GMount.
 * 
 * Gets the icon for @mount.
 * 
 * Returns: a #GIcon.
 **/
GIcon *
g_mount_get_icon (GMount *mount)
{
  GMountIface *iface;

  g_return_val_if_fail (G_IS_MOUNT (mount), NULL);

  iface = G_MOUNT_GET_IFACE (mount);

  return (* iface->get_icon) (mount);
}

/**
 * g_mount_get_uuid:
 * @mount: a #GMount.
 * 
 * Gets the UUID for the @mount. The reference is typically based on
 * the file system UUID for the mount in question and should be
 * considered an opaque string. Returns %NULL if there is no UUID
 * available.
 * 
 * Returns: the UUID for @mount or %NULL if no UUID can be computed.
 **/
char *
g_mount_get_uuid (GMount *mount)
{
  GMountIface *iface;

  g_return_val_if_fail (G_IS_MOUNT (mount), NULL);

  iface = G_MOUNT_GET_IFACE (mount);

  return (* iface->get_uuid) (mount);
}

/**
 * g_mount_get_volume:
 * @mount: a #GMount.
 * 
 * Gets the volume for the @mount.
 * 
 * Returns: a #GVolume or %NULL if @mount is not associated with a volume.
 **/
GVolume *
g_mount_get_volume (GMount *mount)
{
  GMountIface *iface;

  g_return_val_if_fail (G_IS_MOUNT (mount), NULL);

  iface = G_MOUNT_GET_IFACE (mount);

  return (* iface->get_volume) (mount);
}

/**
 * g_mount_get_drive:
 * @mount: a #GMount.
 * 
 * Gets the drive for the @mount.
 *
 * This is a convenience method for getting the #GVolume and then
 * using that object to get the #GDrive.
 * 
 * Returns: a #GDrive or %NULL if @mount is not associated with a volume or a drive.
 **/
GDrive *
g_mount_get_drive (GMount *mount)
{
  GMountIface *iface;

  g_return_val_if_fail (G_IS_MOUNT (mount), NULL);

  iface = G_MOUNT_GET_IFACE (mount);

  return (* iface->get_drive) (mount);
}

/**
 * g_mount_can_unmount: 
 * @mount: a #GMount.
 * 
 * Checks if @mount can be mounted.
 * 
 * Returns: %TRUE if the @mount can be unmounted.
 **/
gboolean
g_mount_can_unmount (GMount *mount)
{
  GMountIface *iface;

  g_return_val_if_fail (G_IS_MOUNT (mount), FALSE);

  iface = G_MOUNT_GET_IFACE (mount);

  return (* iface->can_unmount) (mount);
}

/**
 * g_mount_can_eject: 
 * @mount: a #GMount.
 * 
 * Checks if @mount can be eject.
 * 
 * Returns: %TRUE if the @mount can be ejected.
 **/
gboolean
g_mount_can_eject (GMount *mount)
{
  GMountIface *iface;

  g_return_val_if_fail (G_IS_MOUNT (mount), FALSE);

  iface = G_MOUNT_GET_IFACE (mount);

  return (* iface->can_eject) (mount);
}

/**
 * g_mount_unmount:
 * @mount: a #GMount.
 * @flags: flags affecting the operation
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @callback: a #GAsyncReadyCallback, or %NULL.
 * @user_data: user data passed to @callback.
 * 
 * Unmounts a mount. This is an asynchronous operation, and is 
 * finished by calling g_mount_unmount_finish() with the @mount 
 * and #GAsyncResults data returned in the @callback.
 **/
void
g_mount_unmount (GMount *mount,
                 GMountUnmountFlags flags,
                 GCancellable *cancellable,
                 GAsyncReadyCallback callback,
                 gpointer user_data)
{
  GMountIface *iface;

  g_return_if_fail (G_IS_MOUNT (mount));
  
  iface = G_MOUNT_GET_IFACE (mount);

  if (iface->unmount == NULL)
    {
      g_simple_async_report_error_in_idle (G_OBJECT (mount),
					   callback, user_data,
					   G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
					   /* Translators: This is an error
					    * message for mount objects that
					    * don't implement unmount. */
					   _("mount doesn't implement unmount"));

      return;
    }
  
  (* iface->unmount) (mount, flags, cancellable, callback, user_data);
}

/**
 * g_mount_unmount_finish:
 * @mount: a #GMount.
 * @result: a #GAsyncResult.
 * @error: a #GError location to store the error occuring, or %NULL to 
 * ignore.
 * 
 * Finishes unmounting a mount. If any errors occurred during the operation, 
 * @error will be set to contain the errors and %FALSE will be returned.
 * 
 * Returns: %TRUE if the mount was successfully unmounted. %FALSE otherwise.
 **/
gboolean
g_mount_unmount_finish (GMount       *mount,
                        GAsyncResult  *result,
                        GError       **error)
{
  GMountIface *iface;

  g_return_val_if_fail (G_IS_MOUNT (mount), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  if (G_IS_SIMPLE_ASYNC_RESULT (result))
    {
      GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (result);
      if (g_simple_async_result_propagate_error (simple, error))
        return FALSE;
    }
  
  iface = G_MOUNT_GET_IFACE (mount);
  return (* iface->unmount_finish) (mount, result, error);
}


/**
 * g_mount_eject:
 * @mount: a #GMount.
 * @flags: flags affecting the unmount if required for eject
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @callback: a #GAsyncReadyCallback, or %NULL.
 * @user_data: user data passed to @callback.
 * 
 * Ejects a mount. This is an asynchronous operation, and is 
 * finished by calling g_mount_eject_finish() with the @mount 
 * and #GAsyncResults data returned in the @callback.
 **/
void
g_mount_eject (GMount *mount,
               GMountUnmountFlags flags,
               GCancellable *cancellable,
               GAsyncReadyCallback callback,
               gpointer user_data)
{
  GMountIface *iface;

  g_return_if_fail (G_IS_MOUNT (mount));
  
  iface = G_MOUNT_GET_IFACE (mount);

  if (iface->eject == NULL)
    {
      g_simple_async_report_error_in_idle (G_OBJECT (mount),
					   callback, user_data,
					   G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
					   /* Translators: This is an error
					    * message for mount objects that
					    * don't implement eject. */
					   _("mount doesn't implement eject"));
      
      return;
    }
  
  (* iface->eject) (mount, flags, cancellable, callback, user_data);
}

/**
 * g_mount_eject_finish:
 * @mount: a #GMount.
 * @result: a #GAsyncResult.
 * @error: a #GError location to store the error occuring, or %NULL to 
 * ignore.
 * 
 * Finishes ejecting a mount. If any errors occurred during the operation, 
 * @error will be set to contain the errors and %FALSE will be returned.
 * 
 * Returns: %TRUE if the mount was successfully ejected. %FALSE otherwise.
 **/
gboolean
g_mount_eject_finish (GMount       *mount,
                      GAsyncResult  *result,
                      GError       **error)
{
  GMountIface *iface;

  g_return_val_if_fail (G_IS_MOUNT (mount), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  if (G_IS_SIMPLE_ASYNC_RESULT (result))
    {
      GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (result);
      if (g_simple_async_result_propagate_error (simple, error))
        return FALSE;
    }
  
  iface = G_MOUNT_GET_IFACE (mount);
  return (* iface->eject_finish) (mount, result, error);
}

/**
 * g_mount_remount:
 * @mount: a #GMount.
 * @flags: flags affecting the operation
 * @mount_operation: a #GMountOperation or %NULL to avoid user interaction.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @callback: a #GAsyncReadyCallback, or %NULL.
 * @user_data: user data passed to @callback.
 * 
 * Remounts a mount. This is an asynchronous operation, and is 
 * finished by calling g_mount_remount_finish() with the @mount 
 * and #GAsyncResults data returned in the @callback.
 *
 * Remounting is useful when some setting affecting the operation
 * of the volume has been changed, as these may need a remount to
 * take affect. While this is semantically equivalent with unmounting
 * and then remounting not all backends might need to actually be
 * unmounted.
 **/
void
g_mount_remount (GMount *mount,
                 GMountMountFlags flags,
                 GMountOperation *mount_operation,
                 GCancellable *cancellable,
                 GAsyncReadyCallback callback,
                 gpointer user_data)
{
  GMountIface *iface;

  g_return_if_fail (G_IS_MOUNT (mount));
  
  iface = G_MOUNT_GET_IFACE (mount);

  if (iface->remount == NULL)
    { 
     g_simple_async_report_error_in_idle (G_OBJECT (mount),
					   callback, user_data,
					   G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
					   /* Translators: This is an error
					    * message for mount objects that
					    * don't implement remount. */
					   _("mount doesn't implement remount"));
      
      return;
    }
  
  (* iface->remount) (mount, flags, mount_operation, cancellable, callback, user_data);
}

/**
 * g_mount_remount_finish:
 * @mount: a #GMount.
 * @result: a #GAsyncResult.
 * @error: a #GError location to store the error occuring, or %NULL to 
 * ignore.
 * 
 * Finishes remounting a mount. If any errors occurred during the operation, 
 * @error will be set to contain the errors and %FALSE will be returned.
 * 
 * Returns: %TRUE if the mount was successfully remounted. %FALSE otherwise.
 **/
gboolean
g_mount_remount_finish (GMount       *mount,
                        GAsyncResult  *result,
                        GError       **error)
{
  GMountIface *iface;

  g_return_val_if_fail (G_IS_MOUNT (mount), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  if (G_IS_SIMPLE_ASYNC_RESULT (result))
    {
      GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (result);
      if (g_simple_async_result_propagate_error (simple, error))
        return FALSE;
    }
  
  iface = G_MOUNT_GET_IFACE (mount);
  return (* iface->remount_finish) (mount, result, error);
}


#define __G_MOUNT_C__
#include "gioaliasdef.c"
