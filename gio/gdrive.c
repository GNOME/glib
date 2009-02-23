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

#include "config.h"
#include "gdrive.h"
#include "gsimpleasyncresult.h"
#include "gasyncresult.h"
#include "gioerror.h"
#include "glibintl.h"

#include "gioalias.h"

/**
 * SECTION:gdrive
 * @short_description: Virtual File System drive management
 * @include: gio/gio.h
 * 
 * #GDrive - this represent a piece of hardware connected to the machine.
 * It's generally only created for removable hardware or hardware with
 * removable media. 
 *
 * #GDrive is a container class for #GVolume objects that stem from
 * the same piece of media. As such, #GDrive abstracts a drive with
 * (or without) removable media and provides operations for querying
 * whether media is available, determing whether media change is
 * automatically detected and ejecting the media.
 *
 * If the #GDrive reports that media isn't automatically detected, one
 * can poll for media; typically one should not do this periodically
 * as a poll for media operation is potententially expensive and may
 * spin up the drive creating noise.
 *
 * For porting from GnomeVFS note that there is no equivalent of
 * #GDrive in that API.
 **/

static void g_drive_base_init (gpointer g_class);
static void g_drive_class_init (gpointer g_class,
				 gpointer class_data);

GType
g_drive_get_type (void)
{
  static volatile gsize g_define_type_id__volatile = 0;

  if (g_once_init_enter (&g_define_type_id__volatile))
    {
      const GTypeInfo drive_info =
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
      GType g_define_type_id =
	g_type_register_static (G_TYPE_INTERFACE, I_("GDrive"),
				&drive_info, 0);

      g_type_interface_add_prerequisite (g_define_type_id, G_TYPE_OBJECT);

      g_once_init_leave (&g_define_type_id__volatile, g_define_type_id);
    }

  return g_define_type_id__volatile;
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
      * @drive: a #GDrive.
      * 
      * Emitted when the drive's state has changed.
      **/
      g_signal_new (I_("changed"),
                    G_TYPE_DRIVE,
                    G_SIGNAL_RUN_LAST,
                    G_STRUCT_OFFSET (GDriveIface, changed),
                    NULL, NULL,
                    g_cclosure_marshal_VOID__VOID,
                    G_TYPE_NONE, 0);

      /**
      * GDrive::disconnected:
      * @drive: a #GDrive.
      * 
      * This signal is emitted when the #GDrive have been
      * disconnected. If the recipient is holding references to the
      * object they should release them so the object can be
      * finalized.
      **/
      g_signal_new (I_("disconnected"),
                    G_TYPE_DRIVE,
                    G_SIGNAL_RUN_LAST,
                    G_STRUCT_OFFSET (GDriveIface, disconnected),
                    NULL, NULL,
                    g_cclosure_marshal_VOID__VOID,
                    G_TYPE_NONE, 0);

      /**
      * GDrive::eject-button:
      * @drive: a #GDrive.
      * 
      * Emitted when the physical eject button (if any) of a drive has
      * been pressed.
      **/
      g_signal_new (I_("eject-button"),
                    G_TYPE_DRIVE,
                    G_SIGNAL_RUN_LAST,
                    G_STRUCT_OFFSET (GDriveIface, eject_button),
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
 * Gets the name of @drive.
 *
 * Returns: a string containing @drive's name. The returned 
 *     string should be freed when no longer needed.
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
 *    Free the returned object with g_object_unref().
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
 * Check if @drive has any mountable volumes.
 * 
 * Returns: %TRUE if the @drive contains volumes, %FALSE otherwise.
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
 * Get a list of mountable volumes for @drive.
 *
 * The returned list should be freed with g_list_free(), after
 * its elements have been unreffed with g_object_unref().
 * 
 * Returns: #GList containing any #GVolume objects on the given @drive.
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
 * g_drive_is_media_check_automatic:
 * @drive: a #GDrive.
 * 
 * Checks if @drive is capabable of automatically detecting media changes.
 * 
 * Returns: %TRUE if the @drive is capabable of automatically detecting 
 *     media changes, %FALSE otherwise.
 **/
gboolean
g_drive_is_media_check_automatic (GDrive *drive)
{
  GDriveIface *iface;

  g_return_val_if_fail (G_IS_DRIVE (drive), FALSE);

  iface = G_DRIVE_GET_IFACE (drive);

  return (* iface->is_media_check_automatic) (drive);
}

/**
 * g_drive_is_media_removable:
 * @drive: a #GDrive.
 * 
 * Checks if the @drive supports removable media.
 * 
 * Returns: %TRUE if @drive supports removable media, %FALSE otherwise.
 **/
gboolean
g_drive_is_media_removable (GDrive *drive)
{
  GDriveIface *iface;

  g_return_val_if_fail (G_IS_DRIVE (drive), FALSE);

  iface = G_DRIVE_GET_IFACE (drive);

  return (* iface->is_media_removable) (drive);
}

/**
 * g_drive_has_media:
 * @drive: a #GDrive.
 * 
 * Checks if the @drive has media. Note that the OS may not be polling
 * the drive for media changes; see g_drive_is_media_check_automatic()
 * for more details.
 * 
 * Returns: %TRUE if @drive has media, %FALSE otherwise.
 **/
gboolean
g_drive_has_media (GDrive *drive)
{
  GDriveIface *iface;

  g_return_val_if_fail (G_IS_DRIVE (drive), FALSE);

  iface = G_DRIVE_GET_IFACE (drive);

  return (* iface->has_media) (drive);
}

/**
 * g_drive_can_eject:
 * @drive: a #GDrive.
 * 
 * Checks if a drive can be ejected.
 * 
 * Returns: %TRUE if the @drive can be ejected, %FALSE otherwise.
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
 * g_drive_can_poll_for_media:
 * @drive: a #GDrive.
 * 
 * Checks if a drive can be polled for media changes.
 * 
 * Returns: %TRUE if the @drive can be polled for media changes,
 *     %FALSE otherwise.
 **/
gboolean
g_drive_can_poll_for_media (GDrive *drive)
{
  GDriveIface *iface;

  g_return_val_if_fail (G_IS_DRIVE (drive), FALSE);

  iface = G_DRIVE_GET_IFACE (drive);

  if (iface->poll_for_media == NULL)
    return FALSE;

  return (* iface->can_poll_for_media) (drive);
}

/**
 * g_drive_eject:
 * @drive: a #GDrive.
 * @flags: flags affecting the unmount if required for eject
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @callback: a #GAsyncReadyCallback, or %NULL.
 * @user_data: user data to pass to @callback
 * 
 * Asynchronously ejects a drive.
 *
 * When the operation is finished, @callback will be called.
 * You can then call g_drive_eject_finish() to obtain the
 * result of the operation.
 **/
void
g_drive_eject (GDrive              *drive,
	       GMountUnmountFlags   flags,
	       GCancellable        *cancellable,
	       GAsyncReadyCallback  callback,
	       gpointer             user_data)
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
  
  (* iface->eject) (drive, flags, cancellable, callback, user_data);
}

/**
 * g_drive_eject_finish:
 * @drive: a #GDrive.
 * @result: a #GAsyncResult.
 * @error: a #GError, or %NULL
 * 
 * Finishes ejecting a drive.
 * 
 * Returns: %TRUE if the drive has been ejected successfully,
 *     %FALSE otherwise.
 **/
gboolean
g_drive_eject_finish (GDrive        *drive,
		      GAsyncResult  *result,
		      GError       **error)
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
  
  return (* iface->eject_finish) (drive, result, error);
}

/**
 * g_drive_poll_for_media:
 * @drive: a #GDrive.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @callback: a #GAsyncReadyCallback, or %NULL.
 * @user_data: user data to pass to @callback
 * 
 * Asynchronously polls @drive to see if media has been inserted or removed.
 * 
 * When the operation is finished, @callback will be called.
 * You can then call g_drive_poll_for_media_finish() to obtain the
 * result of the operation.
 **/
void
g_drive_poll_for_media (GDrive              *drive,
                        GCancellable        *cancellable,
                        GAsyncReadyCallback  callback,
                        gpointer             user_data)
{
  GDriveIface *iface;

  g_return_if_fail (G_IS_DRIVE (drive));

  iface = G_DRIVE_GET_IFACE (drive);

  if (iface->poll_for_media == NULL)
    {
      g_simple_async_report_error_in_idle (G_OBJECT (drive), callback, user_data,
					   G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
					   _("drive doesn't implement polling for media"));
      
      return;
    }
  
  (* iface->poll_for_media) (drive, cancellable, callback, user_data);
}

/**
 * g_drive_poll_for_media_finish:
 * @drive: a #GDrive.
 * @result: a #GAsyncResult.
 * @error: a #GError, or %NULL
 * 
 * Finishes an operation started with g_drive_poll_for_media() on a drive.
 * 
 * Returns: %TRUE if the drive has been poll_for_mediaed successfully,
 *     %FALSE otherwise.
 **/
gboolean
g_drive_poll_for_media_finish (GDrive        *drive,
                               GAsyncResult  *result,
                               GError       **error)
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
  
  return (* iface->poll_for_media_finish) (drive, result, error);
}

/**
 * g_drive_get_identifier:
 * @drive: a #GDrive
 * @kind: the kind of identifier to return
 *
 * Gets the identifier of the given kind for @drive.
 *
 * Returns: a newly allocated string containing the
 *     requested identfier, or %NULL if the #GDrive
 *     doesn't have this kind of identifier.
 */
char *
g_drive_get_identifier (GDrive     *drive,
			const char *kind)
{
  GDriveIface *iface;

  g_return_val_if_fail (G_IS_DRIVE (drive), NULL);
  g_return_val_if_fail (kind != NULL, NULL);

  iface = G_DRIVE_GET_IFACE (drive);

  if (iface->get_identifier == NULL)
    return NULL;
  
  return (* iface->get_identifier) (drive, kind);
}

/**
 * g_drive_enumerate_identifiers:
 * @drive: a #GDrive
 *
 * Gets the kinds of identifiers that @drive has. 
 * Use g_drive_get_identifer() to obtain the identifiers
 * themselves.
 *
 * Returns: a %NULL-terminated array of strings containing
 *     kinds of identifiers. Use g_strfreev() to free.
 */
char **
g_drive_enumerate_identifiers (GDrive *drive)
{
  GDriveIface *iface;

  g_return_val_if_fail (G_IS_DRIVE (drive), NULL);
  iface = G_DRIVE_GET_IFACE (drive);

  if (iface->enumerate_identifiers == NULL)
    return NULL;
  
  return (* iface->enumerate_identifiers) (drive);
}


#define __G_DRIVE_C__
#include "gioaliasdef.c"
