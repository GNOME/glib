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

#ifndef __G_UNIX_DRIVE_H__
#define __G_UNIX_DRIVE_H__

#include <glib-object.h>
#include <gio/gdrive.h>
#include <gio/gunixmounts.h>
#include <gio/gunixvolumemonitor.h>

G_BEGIN_DECLS

#define G_TYPE_UNIX_DRIVE        (_g_unix_drive_get_type ())
#define G_UNIX_DRIVE(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), G_TYPE_UNIX_DRIVE, GUnixDrive))
#define G_UNIX_DRIVE_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST((k), G_TYPE_UNIX_DRIVE, GUnixDriveClass))
#define G_IS_UNIX_DRIVE(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), G_TYPE_UNIX_DRIVE))
#define G_IS_UNIX_DRIVE_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), G_TYPE_UNIX_DRIVE))

typedef struct _GUnixDriveClass GUnixDriveClass;

struct _GUnixDriveClass {
   GObjectClass parent_class;
};

GType _g_unix_drive_get_type (void) G_GNUC_CONST;

GUnixDrive *_g_unix_drive_new            (GVolumeMonitor *volume_monitor,
					  GUnixMountPoint *mountpoint);
gboolean    _g_unix_drive_has_mountpoint (GUnixDrive     *drive,
					  const char     *mountpoint);
void        _g_unix_drive_set_volume     (GUnixDrive     *drive,
					  GUnixVolume    *volume);
void        _g_unix_drive_unset_volume   (GUnixDrive     *drive,
					  GUnixVolume    *volume);
void        _g_unix_drive_disconnected   (GUnixDrive     *drive);

G_END_DECLS

#endif /* __G_UNIX_DRIVE_H__ */
