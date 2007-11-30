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
#include "gvolumemonitor.h"
#include "glibintl.h"

#include "gioalias.h"

/**
 * SECTION:gvolumemonitor
 * @short_description: Volume Monitor
 * @see_also: #GDirectoryMonitor, #GFileMonitor
 * 
 * Monitors a mounted volume for changes.
 **/

G_DEFINE_TYPE (GVolumeMonitor, g_volume_monitor, G_TYPE_OBJECT);

enum {
  VOLUME_MOUNTED,
  VOLUME_PRE_UNMOUNT,
  VOLUME_UNMOUNTED,
  DRIVE_CONNECTED,
  DRIVE_DISCONNECTED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };


static void
g_volume_monitor_finalize (GObject *object)
{
  GVolumeMonitor *monitor;

  monitor = G_VOLUME_MONITOR (object);

  if (G_OBJECT_CLASS (g_volume_monitor_parent_class)->finalize)
    (*G_OBJECT_CLASS (g_volume_monitor_parent_class)->finalize) (object);
}

static void
g_volume_monitor_class_init (GVolumeMonitorClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  
  gobject_class->finalize = g_volume_monitor_finalize;

  /**
   * GVolumeMonitor::volume-mounted:   
   * @volume_monitor: The volume monitor emitting the signal.
   * @volume: the volume that was mounted.
   *
   * Emitted when a volume is mounted.
   **/
  signals[VOLUME_MOUNTED] = g_signal_new (I_("volume_mounted"),
					  G_TYPE_VOLUME_MONITOR,
					  G_SIGNAL_RUN_LAST,
					  G_STRUCT_OFFSET (GVolumeMonitorClass, volume_mounted),
					  NULL, NULL,
					  g_cclosure_marshal_VOID__OBJECT,
					  G_TYPE_NONE, 1, G_TYPE_VOLUME);
  /**
   * GVolumeMonitor::volume-pre-unmount:
   * @volume_monitor: The volume monitor emitting the signal.
   * @volume: the volume that is being unmounted.
   *
   * Emitted when a volume is about to be unmounted.
   **/ 
  signals[VOLUME_PRE_UNMOUNT] = g_signal_new (I_("volume_pre_unmount"),
					      G_TYPE_VOLUME_MONITOR,
					      G_SIGNAL_RUN_LAST,
					      G_STRUCT_OFFSET (GVolumeMonitorClass, volume_pre_unmount),
					      NULL, NULL,
					      g_cclosure_marshal_VOID__OBJECT,
					      G_TYPE_NONE, 1, G_TYPE_VOLUME);
  /**
   * GVolumeMonitor::volume-unmounted:
   * @volume_monitor: The volume monitor emitting the signal.
   * @volume: the volume that was unmounted.
   * 
   * Emitted when a volume is unmounted.
   **/  
  signals[VOLUME_UNMOUNTED] = g_signal_new (I_("volume_unmounted"),
					    G_TYPE_VOLUME_MONITOR,
					    G_SIGNAL_RUN_LAST,
					    G_STRUCT_OFFSET (GVolumeMonitorClass, volume_unmounted),
					    NULL, NULL,
					    g_cclosure_marshal_VOID__OBJECT,
					    G_TYPE_NONE, 1, G_TYPE_VOLUME);
  /**
   * GVolumeMonitor::drive-connected:
   * @volume_monitor: The volume monitor emitting the signal.
   * @drive: a #GDrive that was connected.
   * 
   * Emitted when a drive is connected to the system.
   **/
  signals[DRIVE_CONNECTED] = g_signal_new (I_("drive_connected"),
					   G_TYPE_VOLUME_MONITOR,
					   G_SIGNAL_RUN_LAST,
					   G_STRUCT_OFFSET (GVolumeMonitorClass, drive_connected),
					   NULL, NULL,
					   g_cclosure_marshal_VOID__OBJECT,
					   G_TYPE_NONE, 1, G_TYPE_DRIVE);
  
  /**
   * GVolumeMonitor::drive-disconnected:
   * @volume_monitor: The volume monitor emitting the signal.
   * @drive: a #GDrive that was disconnected.
   * 
   * Emitted when a drive is disconnected from the system.
   **/  
  signals[DRIVE_DISCONNECTED] = g_signal_new (I_("drive_disconnected"),
					      G_TYPE_VOLUME_MONITOR,
					      G_SIGNAL_RUN_LAST,
					      G_STRUCT_OFFSET (GVolumeMonitorClass, drive_disconnected),
					      NULL, NULL,
					      g_cclosure_marshal_VOID__OBJECT,
					      G_TYPE_NONE, 1, G_TYPE_DRIVE);
}

static void
g_volume_monitor_init (GVolumeMonitor *monitor)
{
}

/**
 * g_volume_monitor_get_mounted_volumes:
 * @volume_monitor: a #GVolumeMonitor.
 * 
 * Gets a list of volumes mounted on the computer.
 * 
 * Returns: a #GList of mounted #GVolumes.
 **/
GList *
g_volume_monitor_get_mounted_volumes (GVolumeMonitor *volume_monitor)
{
  GVolumeMonitorClass *class;

  g_return_val_if_fail (G_IS_VOLUME_MONITOR (volume_monitor), NULL);

  class = G_VOLUME_MONITOR_GET_CLASS (volume_monitor);

  return class->get_mounted_volumes (volume_monitor);
}

/**
 * g_volume_monitor_get_connected_drives:
 * @volume_monitor: a #GVolumeMonitor.
 * 
 * Gets a list of drives connected to the computer.
 * 
 * Returns: a #GList of connected #GDrives. 
 **/
GList *
g_volume_monitor_get_connected_drives (GVolumeMonitor *volume_monitor)
{
  GVolumeMonitorClass *class;

  g_return_val_if_fail (G_IS_VOLUME_MONITOR (volume_monitor), NULL);

  class = G_VOLUME_MONITOR_GET_CLASS (volume_monitor);

  return class->get_connected_drives (volume_monitor);
}

#define __G_VOLUME_MONITOR_C__
#include "gioaliasdef.c"
