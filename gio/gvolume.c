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
#include "gvolume.h"
#include "gvolumeprivate.h"
#include "gsimpleasyncresult.h"
#include "glibintl.h"

/**
 * SECTION:gvolume
 * @short_description: mounted volume management
 * 
 * Class for managing mounted volumes.
 * 
 * Unmounting volumes is an asynchronous operation. For more information about
 * asynchronous operations, see #GAsyncReady and #GSimpleAsyncReady. To unmount a volume, 
 * first call g_volume_unmount() with (at least) the volume and a #GAsyncReadyCallback.
 * The callback will be fired when the operation has resolved (either with success or failure), 
 * and a #GAsyncReady structure will be passed to the callback. 
 * That callback should then call g_volume_unmount_finish() with
 * the volume and the #GAsyncReady data to see if the operation was completed successfully.
 * If an @error is present when g_volume_unmount_finish() is called, then it will
 * be filled with any error information.
 * 
 * Ejecting volumes is also an asynchronous operation. 
 * To eject a volume, call g_volume_eject() with (at least) the volume to eject 
 * and a #GAsyncReadyCallback. The callback will be fired when the eject operation
 * has resolved (either with success or failure), and a #GAsyncReady structure will
 * be passed to the callback. That callback should then call g_volume_eject_finish()
 * with the volume and the #GAsyncReady data to determine if the operation was completed
 * successfully. If an @error is present when g_volume_eject_finish() is called, then
 * it will be filled with any error information. 
 *
 **/

static void g_volume_base_init (gpointer g_class);
static void g_volume_class_init (gpointer g_class,
				 gpointer class_data);

GType
g_volume_get_type (void)
{
  static GType volume_type = 0;

  if (! volume_type)
    {
      static const GTypeInfo volume_info =
      {
        sizeof (GVolumeIface), /* class_size */
	g_volume_base_init,   /* base_init */
	NULL,		/* base_finalize */
	g_volume_class_init,
	NULL,		/* class_finalize */
	NULL,		/* class_data */
	0,
	0,              /* n_preallocs */
	NULL
      };

      volume_type =
	g_type_register_static (G_TYPE_INTERFACE, I_("GVolume"),
				&volume_info, 0);

      g_type_interface_add_prerequisite (volume_type, G_TYPE_OBJECT);
    }

  return volume_type;
}

static void
g_volume_class_init (gpointer g_class,
		   gpointer class_data)
{
}

static void
g_volume_base_init (gpointer g_class)
{
  static gboolean initialized = FALSE;

  if (! initialized)
    {
     /**
      * GVolume::changed:
      * 
      * Emitted when the volume has been changed.
      **/
      g_signal_new (I_("changed"),
                    G_TYPE_VOLUME,
                    G_SIGNAL_RUN_LAST,
                    G_STRUCT_OFFSET (GVolumeIface, changed),
                    NULL, NULL,
                    g_cclosure_marshal_VOID__VOID,
                    G_TYPE_NONE, 0);

      initialized = TRUE;
    }
}

/**
 * g_volume_get_root:
 * @volume: a #GVolume.
 * 
 * Gets the root directory on @volume.
 * 
 * Returns a #GFile.
 **/
GFile *
g_volume_get_root (GVolume *volume)
{
  GVolumeIface *iface;

  g_return_val_if_fail (G_IS_VOLUME (volume), NULL);

  iface = G_VOLUME_GET_IFACE (volume);

  return (* iface->get_root) (volume);
}

/**
 * g_volume_get_name:
 * @volume: a #GVolume.
 * 
 * Gets the name of @volume.
 * 
 * Returns the name for the given @volume. The returned string should 
 * be freed when no longer needed.
 **/
char *
g_volume_get_name (GVolume *volume)
{
  GVolumeIface *iface;

  g_return_val_if_fail (G_IS_VOLUME (volume), NULL);

  iface = G_VOLUME_GET_IFACE (volume);

  return (* iface->get_name) (volume);
}

/**
 * g_volume_get_icon:
 * @volume: a #GVolume.
 * 
 * Gets the icon for @volume.
 * 
 * Returns: a #GIcon.
 **/
GIcon *
g_volume_get_icon (GVolume *volume)
{
  GVolumeIface *iface;

  g_return_val_if_fail (G_IS_VOLUME (volume), NULL);

  iface = G_VOLUME_GET_IFACE (volume);

  return (* iface->get_icon) (volume);
}
  
/**
 * g_volume_get_drive:
 * @volume: a #GVolume.
 * 
 * Gets the drive for the @volume.
 * 
 * Returns: a #GDrive.
 **/
GDrive *
g_volume_get_drive (GVolume *volume)
{
  GVolumeIface *iface;

  g_return_val_if_fail (G_IS_VOLUME (volume), NULL);

  iface = G_VOLUME_GET_IFACE (volume);

  return (* iface->get_drive) (volume);
}

/**
 * g_volume_can_unmount: 
 * @volume: a #GVolume.
 * 
 * Checks if @volume can be mounted.
 * 
 * Returns %TRUE if the @volume can be unmounted.
 **/
gboolean
g_volume_can_unmount (GVolume *volume)
{
  GVolumeIface *iface;

  g_return_val_if_fail (G_IS_VOLUME (volume), FALSE);

  iface = G_VOLUME_GET_IFACE (volume);

  return (* iface->can_unmount) (volume);
}

/**
 * g_volume_can_eject:
 * @volume: a #GVolume.
 * 
 * Checks if @volume can be ejected.
 * 
 * Returns %TRUE if the @volume can be ejected.
 **/
gboolean
g_volume_can_eject (GVolume *volume)
{
  GVolumeIface *iface;

  g_return_val_if_fail (G_IS_VOLUME (volume), FALSE);

  iface = G_VOLUME_GET_IFACE (volume);

  return (* iface->can_eject) (volume);
}

/**
 * g_volume_unmount:
 * @volume: a #GVolume.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @callback: a #GAsyncReadyCallback.
 * @user_data: user data passed to @callback.
 * 
 * Unmounts a volume. This is an asynchronous operation, and is finished by calling 
 * g_volume_unmount_finish() with the @volume and #GAsyncResults data returned in the 
 * @callback.
 * 
 **/
void
g_volume_unmount (GVolume *volume,
		  GCancellable *cancellable,
		  GAsyncReadyCallback callback,
		  gpointer user_data)
{
  GVolumeIface *iface;

  g_return_if_fail (G_IS_VOLUME (volume));
  
  iface = G_VOLUME_GET_IFACE (volume);

  if (iface->unmount == NULL)
    {
      g_simple_async_report_error_in_idle (G_OBJECT (volume),
					   callback, user_data,
					   G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
					   _("volume doesn't implement unmount"));
      
      return;
    }
  
  (* iface->unmount) (volume, cancellable, callback, user_data);
}

/**
 * g_volume_unmount_finish:
 * @volume: a #GVolume.
 * @result: a #GAsyncResult.
 * @error: a #GError location to store the error occuring, or %NULL to 
 * ignore.
 * 
 * Finishes unmounting a volume. If any errors occured during the operation, 
 * @error will be set to contain the errors and %FALSE will be returned.
 * 
 * Returns: %TRUE if the volume was successfully unmounted. %FALSE otherwise.
 **/
gboolean
g_volume_unmount_finish (GVolume              *volume,
			 GAsyncResult         *result,
			 GError              **error)
{
  GVolumeIface *iface;

  g_return_val_if_fail (G_IS_VOLUME (volume), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  if (G_IS_SIMPLE_ASYNC_RESULT (result))
    {
      GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (result);
      if (g_simple_async_result_propagate_error (simple, error))
	return FALSE;
    }
  
  iface = G_VOLUME_GET_IFACE (volume);
  return (* iface->unmount_finish) (volume, result, error);
}

/**
 * g_volume_eject:
 * @volume: a #GVolume.
 * @cancellable: optional #GCancellable object, %NULL to ignore. 
 * @callback: a #GAsyncReadyCallback.
 * @user_data: user data passed to @callback.
 * 
 * Ejects a volume. This is an asynchronous operation, and is finished
 * by calling g_volume_eject_finish() from the @callback with the @volume and 
 * #GAsyncResults returned in the callback.
 * 
 **/
void
g_volume_eject (GVolume         *volume,
		GCancellable *cancellable,
		GAsyncReadyCallback  callback,
		gpointer         user_data)
{
  GVolumeIface *iface;

  g_return_if_fail (G_IS_VOLUME (volume));

  iface = G_VOLUME_GET_IFACE (volume);

  if (iface->eject == NULL)
    {
      g_simple_async_report_error_in_idle (G_OBJECT (volume),
					   callback, user_data,
					   G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
					   _("volume doesn't implement eject"));
      
      return;
    }
  
  (* iface->eject) (volume, cancellable, callback, user_data);
}

/**
 * g_volume_eject_finish:
 * @volume: a #GVolume.
 * @result: a #GAsyncResult.
 * @error: a #GError location to store the error occuring, or %NULL to 
 * ignore.
 * 
 * Finishes ejecting the volume. If any errors occured during the operation, 
 * @error will be set to contain the errors and %FALSE will be returned.
 * 
 * Returns: %TRUE if the volume was successfully ejected. %FALSE otherwise.
 **/
gboolean
g_volume_eject_finish (GVolume              *volume,
		       GAsyncResult         *result,
		       GError              **error)
{
  GVolumeIface *iface;

  g_return_val_if_fail (G_IS_VOLUME (volume), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  if (G_IS_SIMPLE_ASYNC_RESULT (result))
    {
      GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (result);
      if (g_simple_async_result_propagate_error (simple, error))
	return FALSE;
    }
  
  iface = G_VOLUME_GET_IFACE (volume);
  return (* iface->eject_finish) (volume, result, error);
}
