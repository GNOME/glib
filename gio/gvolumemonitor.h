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

#ifndef __G_VOLUME_MONITOR_H__
#define __G_VOLUME_MONITOR_H__

#include <glib-object.h>
#include <gio/gvolume.h>
#include <gio/gdrive.h>

G_BEGIN_DECLS

#define G_TYPE_VOLUME_MONITOR         (g_volume_monitor_get_type ())
#define G_VOLUME_MONITOR(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), G_TYPE_VOLUME_MONITOR, GVolumeMonitor))
#define G_VOLUME_MONITOR_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), G_TYPE_VOLUME_MONITOR, GVolumeMonitorClass))
#define G_VOLUME_MONITOR_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), G_TYPE_VOLUME_MONITOR, GVolumeMonitorClass))
#define G_IS_VOLUME_MONITOR(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), G_TYPE_VOLUME_MONITOR))
#define G_IS_VOLUME_MONITOR_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), G_TYPE_VOLUME_MONITOR))

typedef struct _GVolumeMonitor GVolumeMonitor;
typedef struct _GVolumeMonitorClass GVolumeMonitorClass;

struct _GVolumeMonitor {
  GObject parent;
  gpointer priv;
};

struct _GVolumeMonitorClass {
  GObjectClass parent_class;

  /*< public >*/
  /* signals */
  void (* volume_mounted)	(GVolumeMonitor *volume_monitor,
				 GVolume        *volume);
  void (* volume_pre_unmount)	(GVolumeMonitor *volume_monitor,
				 GVolume	*volume);
  void (* volume_unmounted)	(GVolumeMonitor *volume_monitor,
				 GVolume        *volume);
  void (* drive_connected) 	(GVolumeMonitor *volume_monitor,
				 GDrive	        *drive);
  void (* drive_disconnected)	(GVolumeMonitor *volume_monitor,
				 GDrive         *drive);

  /* Vtable */

  GList * (*get_mounted_volumes)  (GVolumeMonitor *volume_monitor);
  GList * (*get_connected_drives) (GVolumeMonitor *volume_monitor);


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

GType g_volume_monitor_get_type (void) G_GNUC_CONST;

GVolumeMonitor *g_volume_monitor_get                  (void);
GList *         g_volume_monitor_get_mounted_volumes  (GVolumeMonitor *volume_monitor);
GList *         g_volume_monitor_get_connected_drives (GVolumeMonitor *volume_monitor);

G_END_DECLS

#endif /* __G_VOLUME_MONITOR_H__ */
