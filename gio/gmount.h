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

#ifndef __G_MOUNT_H__
#define __G_MOUNT_H__

#include <glib-object.h>
#include <gio/gfile.h>

G_BEGIN_DECLS

#define G_TYPE_MOUNT            (g_mount_get_type ())
#define G_MOUNT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), G_TYPE_MOUNT, GMount))
#define G_IS_MOUNT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), G_TYPE_MOUNT))
#define G_MOUNT_GET_IFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), G_TYPE_MOUNT, GMountIface))

/* GMount typedef is in gfile.h due to include order issues */
/**
 * GVolume:
 * 
 * Opaque mountable volume object.
 **/
typedef struct _GVolume GVolume; /* Dummy typedef */

/**
 * GDrive:
 * 
 * Opaque drive object.
 **/
typedef struct _GDrive          GDrive; /* Dummy typedef */

typedef struct _GMountIface    GMountIface;

/**
 * GMountIface:
 * @g_iface: The parent interface.
 * @changed: Changed signal that is emitted when the mount's state has changed.
 * @get_root: Gets a #GFile to the root directory of the #GMount.
 * @get_name: Gets a string containing the name of the #GMount.
 * @get_icon: Gets a #GIcon for the #GMount.
 * @get_volume: Gets a #GVolume the mount is located on. Returns %NULL if the #GMount is not associated with a #GVolume.
 * @get_drive: Gets a #GDrive the volume of the mount is located on. Returns %NULL if the #GMount is not associated with a #GDrive or a #GVolume. This is convenience method for getting the #GVolume and using that to get the #GDrive.
 * @can_unmount: Checks if a #GMount can be unmounted.
 * @unmount: Starts unmounting a #GMount.
 * @unmount_finish: Finishes an unmounting operation.
 * 
 * Interface for implementing operations for mounts.
 **/
struct _GMountIface
{
  GTypeInterface g_iface;

  /* signals */

  void (*changed) (GMount *mount);
  
  /* Virtual Table */

  GFile *            (*get_root)             (GMount         *mount);
  char *             (*get_name)             (GMount         *mount);
  GIcon *            (*get_icon)             (GMount         *mount);
  GVolume *          (*get_volume)           (GMount         *mount);
  GDrive *           (*get_drive)            (GMount         *mount);
  gboolean           (*can_unmount)          (GMount         *mount);
  void               (*unmount)              (GMount         *mount,
                                              GCancellable    *cancellable,
                                              GAsyncReadyCallback callback,
                                              gpointer         user_data);
  gboolean           (*unmount_finish)       (GMount         *mount,
                                              GAsyncResult    *result,
                                              GError         **error);

  /*< private >*/
  /* Padding for future expansion */
  void (*_g_reserved1) (void);
  void (*_g_reserved2) (void);
  void (*_g_reserved3) (void);
  void (*_g_reserved4) (void);
  void (*_g_reserved5) (void);
  void (*_g_reserved6) (void);
  void (*_g_reserved7) (void);
  void (*_g_reserved8) (void);
};

GType g_mount_get_type (void) G_GNUC_CONST;

GFile *            g_mount_get_root             (GMount              *mount);
char *             g_mount_get_name             (GMount              *mount);
GIcon *            g_mount_get_icon             (GMount              *mount);
GVolume *          g_mount_get_volume           (GMount              *mount);
GDrive *           g_mount_get_drive            (GMount              *mount);
gboolean           g_mount_can_unmount          (GMount              *mount);
void               g_mount_unmount              (GMount              *mount,
                                                 GCancellable         *cancellable,
                                                 GAsyncReadyCallback   callback,
                                                 gpointer              user_data);
gboolean           g_mount_unmount_finish       (GMount              *mount,
                                                 GAsyncResult         *result,
                                                 GError              **error);

G_END_DECLS

#endif /* __G_MOUNT_H__ */
