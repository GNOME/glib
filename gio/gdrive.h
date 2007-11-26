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

#ifndef __G_DRIVE_H__
#define __G_DRIVE_H__

#include <glib-object.h>
#include <gio/gvolume.h>
#include <gio/gmountoperation.h>

G_BEGIN_DECLS

#define G_TYPE_DRIVE           (g_drive_get_type ())
#define G_DRIVE(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), G_TYPE_DRIVE, GDrive))
#define G_IS_DRIVE(obj)	       (G_TYPE_CHECK_INSTANCE_TYPE ((obj), G_TYPE_DRIVE))
#define G_DRIVE_GET_IFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE ((obj), G_TYPE_DRIVE, GDriveIface))

typedef struct _GDriveIface    GDriveIface;

struct _GDriveIface
{
  GTypeInterface g_iface;

  /* signals */
  void (*changed)            (GVolume *volume);
  
  /* Virtual Table */
  
  char *   (*get_name)    (GDrive         *drive);
  GIcon *  (*get_icon)    (GDrive         *drive);
  gboolean (*has_volumes) (GDrive         *drive);
  GList *  (*get_volumes) (GDrive         *drive);
  gboolean (*is_automounted)(GDrive       *drive);
  gboolean (*can_mount)   (GDrive         *drive);
  gboolean (*can_eject)   (GDrive         *drive);
  void     (*mount)       (GDrive         *drive,
			   GMountOperation *mount_operation,
			   GCancellable   *cancellable,
			   GAsyncReadyCallback callback,
			   gpointer        user_data);
  gboolean (*mount_finish)(GDrive         *drive,
			   GAsyncResult   *result,
			   GError        **error);
  void     (*eject)       (GDrive         *drive,
			   GCancellable   *cancellable,
			   GAsyncReadyCallback callback,
			   gpointer        user_data);
  gboolean (*eject_finish)(GDrive         *drive,
			   GAsyncResult   *result,
			   GError        **error);
};

GType g_drive_get_type (void) G_GNUC_CONST;

char *   g_drive_get_name       (GDrive               *drive);
GIcon *  g_drive_get_icon       (GDrive               *drive);
gboolean g_drive_has_volumes    (GDrive               *drive);
GList  * g_drive_get_volumes    (GDrive               *drive);
gboolean g_drive_is_automounted (GDrive               *drive);
gboolean g_drive_can_mount      (GDrive               *drive);
gboolean g_drive_can_eject      (GDrive               *drive);
void     g_drive_mount          (GDrive               *drive,
				 GMountOperation      *mount_operation,
				 GCancellable         *cancellable,
				 GAsyncReadyCallback   callback,
				 gpointer              user_data);
gboolean g_drive_mount_finish   (GDrive               *drive,
				 GAsyncResult         *result,
				 GError              **error);
void     g_drive_eject          (GDrive               *drive,
				 GCancellable         *cancellable,
				 GAsyncReadyCallback   callback,
				 gpointer              user_data);
gboolean g_drive_eject_finish   (GDrive               *drive,
				 GAsyncResult         *result,
				 GError              **error);

G_END_DECLS

#endif /* __G_DRIVE_H__ */
