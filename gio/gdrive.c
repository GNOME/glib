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
#include "gdrive.h"
#include "gsimpleasyncresult.h"
#include "glibintl.h"

/**
 * SECTION:gdrive
 * @short_description: Virtual File System drive management.
 * @include: gio/gdrive.h
 * 
 * #GDrive manages drive operations from GVFS, including volume mounting
 * and ejecting, and getting the drive's name and icon. 
 * 
 **/

static void g_drive_base_init (gpointer g_class);
static void g_drive_class_init (gpointer g_class,
				 gpointer class_data);

GType
g_drive_get_type (void)
{
  static GType drive_type = 0;

  if (! drive_type)
    {
      static const GTypeInfo drive_info =
      {
        sizeof (GDriveIface), /* class_size */
	g_drive_base_init,   /* base_init */
	NULL,		/* base_finalize */
	g_drive_class_init,
	NULL,		/* class_finalize */
	NULL,		/* class_data */
	0,
	0,              /* n_preallocs */
	NULL
      };

      drive_type =
	g_type_register_static (G_TYPE_INTERFACE, I_("GDrive"),
				&drive_info, 0);

      g_type_interface_add_prerequisite (drive_type, G_TYPE_OBJECT);
    }

  return drive_type;
}

static void
g_drive_class_init (gpointer g_class,
		   gpointer class_data)
{
}

static void
g_drive_base_init (gpointer g_class)
{
  static gboolean initialized = FALSE;

  if (! initialized)
    {
      /**
      * GDrive::changed:
      * @volume: a #GVolume.
      * 
      * Emitted when the drive's state has changed.
      * 
      **/
      g_signal_new (I_("changed"),
                    G_TYPE_DRIVE,
                    G_SIGNAL_RUN_LAST,
                    G_STRUCT_OFFSET (GDriveIface, changed),
                    NULL, NULL,
                    g_cclosure_marshal_VOID__VOID,
                    G_TYPE_NONE, 0);

      initialized = TRUE;
    }
}

/**
 * g_drive_get_name:
 * @drive: a #GDrive.
 * 
 * Returns: string containing @drive's name.
 *
 * The returned string should be freed when no longer needed.
 **/
char *
g_drive_get_name (GDrive *drive)
{
  GDriveIface *iface;

  g_return_val_if_fail (G_IS_DRIVE (drive), NULL);

  iface = G_DRIVE_GET_IFACE (drive);

  return (* iface->get_name) (drive);
}

/**
 * g_drive_get_icon:
 * @drive: a #GDrive.
 * 
 * Gets the icon for @drive.
 * 
 * Returns: #GIcon for the @drive.
 **/
GIcon *
g_drive_get_icon (GDrive *drive)
{
  GDriveIface *iface;
  
  g_return_val_if_fail (G_IS_DRIVE (drive), NULL);

  iface = G_DRIVE_GET_IFACE (drive);

  return (* iface->get_icon) (drive);
}

/**
 * g_drive_has_volumes:
 * @drive: a #GDrive.
 * 
 * Checks if a drive has any volumes.
 * 
 * Returns: %TRUE if @drive contains volumes, %FALSE otherwise.
 **/
gboolean
g_drive_has_volumes (GDrive *drive)
{
  GDriveIface *iface;

  g_return_val_if_fail (G_IS_DRIVE (drive), FALSE);

  iface = G_DRIVE_GET_IFACE (drive);

  return (* iface->has_volumes) (drive);
}

/**
 * g_drive_get_volumes:
 * @drive: a #GDrive.
 * 
 * Gets a list of volumes for a drive.
 * 
 * Returns: #GList containing any #GVolume<!---->s on the given @drive.
 * NOTE: Fact-check this.
 **/
GList *
g_drive_get_volumes (GDrive *drive)
{
  GDriveIface *iface;

  g_return_val_if_fail (G_IS_DRIVE (drive), NULL);

  iface = G_DRIVE_GET_IFACE (drive);

  return (* iface->get_volumes) (drive);
}

/**
 * g_drive_is_automounted:
 * @drive: a #GDrive.
 * 
 * Checks if a drive was automatically mounted, e.g. by HAL.
 * 
 * Returns: %TRUE if the drive was automounted. %FALSE otherwise.
 **/
gboolean
g_drive_is_automounted (GDrive *drive)
{
  GDriveIface *iface;

  g_return_val_if_fail (G_IS_DRIVE (drive), FALSE);

  iface = G_DRIVE_GET_IFACE (drive);

  return (* iface->is_automounted) (drive);
}

/**
 * g_drive_can_mount:
 * @drive: a #GDrive.
 * 
 * Checks if a drive can be mounted.
 * 
 * Returns: %TRUE if the @drive can be mounted. %FALSE otherwise.
 **/
gboolean
g_drive_can_mount (GDrive *drive)
{
  GDriveIface *iface;

  g_return_val_if_fail (G_IS_DRIVE (drive), FALSE);

  iface = G_DRIVE_GET_IFACE (drive);

  if (iface->can_mount == NULL)
    return FALSE;

  return (* iface->can_mount) (drive);
}

/**
 * g_drive_can_eject:
 * @drive: pointer to a #GDrive.
 * 
 * Checks if a drive can be ejected.
 * 
 * Returns: %TRUE if the @drive can be ejected. %FALSE otherwise.
 **/
gboolean
g_drive_can_eject (GDrive *drive)
{
  GDriveIface *iface;

  g_return_val_if_fail (G_IS_DRIVE (drive), FALSE);

  iface = G_DRIVE_GET_IFACE (drive);

  if (iface->can_eject == NULL)
    return FALSE;

  return (* iface->can_eject) (drive);
}

/**
 * g_drive_mount:
 * @drive: a #GDrive.
 * @mount_operation: a #GMountOperation.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @callback: a #GAsyncReadyCallback.
 * @user_data: a #gpointer.
 * 
 * Mounts a drive.
 * 
 **/
void
g_drive_mount (GDrive         *drive,
	       GMountOperation *mount_operation,
	       GCancellable *cancellable,
	       GAsyncReadyCallback callback,
	       gpointer         user_data)
{
  GDriveIface *iface;

  g_return_if_fail (G_IS_DRIVE (drive));
  g_return_if_fail (G_IS_MOUNT_OPERATION (mount_operation));

  iface = G_DRIVE_GET_IFACE (drive);

  if (iface->mount == NULL)
    {
      g_simple_async_report_error_in_idle (G_OBJECT (drive), callback, user_data,
					   G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
					   _("drive doesn't implement mount"));
      
      return;
    }
  
  (* iface->mount) (drive, mount_operation, cancellable, callback, user_data);
}

/**
 * g_drive_mount_finish:
 * @drive: pointer to a #GDrive.
 * @result: a #GAsyncResult.
 * @error: a #GError.
 * 
 * Finishes mounting a drive.
 * 
 * Returns: %TRUE, %FALSE if operation failed.
 **/
gboolean
g_drive_mount_finish (GDrive               *drive,
		      GAsyncResult         *result,
		      GError              **error)
{
  GDriveIface *iface;

  g_return_val_if_fail (G_IS_DRIVE (drive), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  if (G_IS_SIMPLE_ASYNC_RESULT (result))
    {
      GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (result);
      if (g_simple_async_result_propagate_error (simple, error))
	return FALSE;
    }
  
  iface = G_DRIVE_GET_IFACE (drive);
  return (* iface->mount_finish) (drive, result, error);
}

/**
 * g_drive_eject:
 * @drive: a #GDrive.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @callback: a #GAsyncReadyCallback.
 * @user_data: a #gpointer.
 * 
 * Ejects a drive.
 * 
 **/
void
g_drive_eject (GDrive         *drive,
	       GCancellable *cancellable,
	       GAsyncReadyCallback  callback,
	       gpointer         user_data)
{
  GDriveIface *iface;

  g_return_if_fail (G_IS_DRIVE (drive));

  iface = G_DRIVE_GET_IFACE (drive);

  if (iface->eject == NULL)
    {
      g_simple_async_report_error_in_idle (G_OBJECT (drive), callback, user_data,
					   G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
					   _("drive doesn't implement eject"));
      
      return;
    }
  
  (* iface->eject) (drive, cancellable, callback, user_data);
}

/**
 * g_drive_eject_finish
 * @drive: a #GDrive.
 * @result: a #GAsyncResult.
 * @error: a #GError.
 * 
 * Finishes ejecting a drive.
 * 
 * Returns: %TRUE if the drive has been ejected successfully,
 * %FALSE otherwise.
 **/
gboolean
g_drive_eject_finish (GDrive               *drive,
		      GAsyncResult         *result,
		      GError              **error)
{
  GDriveIface *iface;

  g_return_val_if_fail (G_IS_DRIVE (drive), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  if (G_IS_SIMPLE_ASYNC_RESULT (result))
    {
      GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (result);
      if (g_simple_async_result_propagate_error (simple, error))
	return FALSE;
    }
  
  iface = G_DRIVE_GET_IFACE (drive);
  
  return (* iface->mount_finish) (drive, result, error);
}
