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

#ifndef __G_VOLUME_H__
#define __G_VOLUME_H__

#include <glib-object.h>
#include <gio/gfile.h>

G_BEGIN_DECLS

#define G_TYPE_VOLUME            (g_volume_get_type ())
#define G_VOLUME(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), G_TYPE_VOLUME, GVolume))
#define G_IS_VOLUME(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), G_TYPE_VOLUME))
#define G_VOLUME_GET_IFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), G_TYPE_VOLUME, GVolumeIface))

/* GVolume typedef is in gfile.h due to include order issues */
/**
 * GDrive:
 * 
 * Opaque drive object.
 **/
typedef struct _GDrive          GDrive; /* Dummy typedef */
typedef struct _GVolumeIface    GVolumeIface;

/**
 * GVolumeIface:
 * @g_iface: The parent interface.
 * @changed: Changed signal that is emitted when the volume's state has changed.
 * @get_root: Gets a #GFile to the root directory of the #GVolume.
 * @get_name: Gets a string containing the name of the #GVolume.
 * @get_icon: Gets a #GIcon for the #GVolume.
 * @get_drive: Gets a #GDrive the volume is located on.
 * @can_unmount: Checks if a #GVolume can be unmounted.
 * @can_eject: Checks if a #GVolume can be ejected.
 * @unmount: Starts unmounting a #GVolume.
 * @unmount_finish: Finishes an unmounting operation.
 * @eject: Starts ejecting a #GVolume.
 * @eject_finish: Finishes an eject operation.
 * 
 * Interface for implementing operations for mounted volumes.
 **/
struct _GVolumeIface
{
  GTypeInterface g_iface;

  /* signals */

  void (*changed) (GVolume *volume);
  
  /* Virtual Table */

  GFile *  (*get_root)       (GVolume         *volume);
  char *   (*get_name)       (GVolume         *volume);
  GIcon *  (*get_icon)       (GVolume         *volume);
  GDrive * (*get_drive)      (GVolume         *volume);
  gboolean (*can_unmount)    (GVolume         *volume);
  gboolean (*can_eject)      (GVolume         *volume);
  void     (*unmount)        (GVolume         *volume,
			      GCancellable    *cancellable,
			      GAsyncReadyCallback callback,
			      gpointer         user_data);
  gboolean (*unmount_finish) (GVolume         *volume,
			      GAsyncResult    *result,
			      GError         **error);
  void     (*eject)          (GVolume         *volume,
			      GCancellable    *cancellable,
			      GAsyncReadyCallback callback,
			      gpointer         user_data);
  gboolean (*eject_finish)   (GVolume         *volume,
			      GAsyncResult    *result,
			      GError         **error);
};

GType g_volume_get_type (void) G_GNUC_CONST;

GFile   *g_volume_get_root       (GVolume              *volume);
char *   g_volume_get_name       (GVolume              *volume);
GIcon *  g_volume_get_icon       (GVolume              *volume);
GDrive * g_volume_get_drive      (GVolume              *volume);
gboolean g_volume_can_unmount    (GVolume              *volume);
gboolean g_volume_can_eject      (GVolume              *volume);
void     g_volume_unmount        (GVolume              *volume,
				  GCancellable         *cancellable,
				  GAsyncReadyCallback   callback,
				  gpointer              user_data);
gboolean g_volume_unmount_finish (GVolume              *volume,
				  GAsyncResult         *result,
				  GError              **error);
void     g_volume_eject          (GVolume              *volume,
				  GCancellable         *cancellable,
				  GAsyncReadyCallback   callback,
				  gpointer              user_data);
gboolean g_volume_eject_finish   (GVolume              *volume,
				  GAsyncResult         *result,
				  GError              **error);

G_END_DECLS

#endif /* __G_VOLUME_H__ */
