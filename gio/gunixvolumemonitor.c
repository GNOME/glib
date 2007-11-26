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

#include <string.h>

#include <glib.h>
#include "gunixvolumemonitor.h"
#include "gunixmounts.h"
#include "gunixvolume.h"
#include "gunixdrive.h"
#include "gvolumeprivate.h"

#include "glibintl.h"

struct _GUnixVolumeMonitor {
  GNativeVolumeMonitor parent;

  GUnixMountMonitor *mount_monitor;

  GList *last_mountpoints;
  GList *last_mounts;

  GList *drives;
  GList *volumes;
};

static void mountpoints_changed (GUnixMountMonitor  *mount_monitor,
				 gpointer            user_data);
static void mounts_changed      (GUnixMountMonitor  *mount_monitor,
				 gpointer            user_data);
static void update_drives       (GUnixVolumeMonitor *monitor);
static void update_volumes      (GUnixVolumeMonitor *monitor);

G_DEFINE_TYPE (GUnixVolumeMonitor, g_unix_volume_monitor, G_TYPE_NATIVE_VOLUME_MONITOR);

static void
g_unix_volume_monitor_finalize (GObject *object)
{
  GUnixVolumeMonitor *monitor;
  
  monitor = G_UNIX_VOLUME_MONITOR (object);

  g_signal_handlers_disconnect_by_func (monitor->mount_monitor, mountpoints_changed, monitor);
  g_signal_handlers_disconnect_by_func (monitor->mount_monitor, mounts_changed, monitor);
					
  g_object_unref (monitor->mount_monitor);

  g_list_foreach (monitor->last_mounts, (GFunc)g_unix_mount_free, NULL);
  g_list_free (monitor->last_mounts);

  g_list_foreach (monitor->volumes, (GFunc)g_object_unref, NULL);
  g_list_free (monitor->volumes);
  g_list_foreach (monitor->drives, (GFunc)g_object_unref, NULL);
  g_list_free (monitor->drives);
  
  if (G_OBJECT_CLASS (g_unix_volume_monitor_parent_class)->finalize)
    (*G_OBJECT_CLASS (g_unix_volume_monitor_parent_class)->finalize) (object);
}

static GList *
get_mounted_volumes (GVolumeMonitor *volume_monitor)
{
  GUnixVolumeMonitor *monitor;
  GList *l;
  
  monitor = G_UNIX_VOLUME_MONITOR (volume_monitor);

  l = g_list_copy (monitor->volumes);
  g_list_foreach (l, (GFunc)g_object_ref, NULL);

  return l;
}

static GList *
get_connected_drives (GVolumeMonitor *volume_monitor)
{
  GUnixVolumeMonitor *monitor;
  GList *l;
  
  monitor = G_UNIX_VOLUME_MONITOR (volume_monitor);

  l = g_list_copy (monitor->drives);
  g_list_foreach (l, (GFunc)g_object_ref, NULL);

  return l;
}

static GVolume *
get_volume_for_mountpoint (const char *mountpoint)
{
  GUnixMount *mount;
  GUnixVolume *volume;

  mount = g_get_unix_mount_at (mountpoint, NULL);
  
  /* TODO: Set drive? */
  volume = g_unix_volume_new (mount, NULL);

  return G_VOLUME (volume);
}

static void
g_unix_volume_monitor_class_init (GUnixVolumeMonitorClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GVolumeMonitorClass *monitor_class = G_VOLUME_MONITOR_CLASS (klass);
  GNativeVolumeMonitorClass *native_class = G_NATIVE_VOLUME_MONITOR_CLASS (klass);
  
  gobject_class->finalize = g_unix_volume_monitor_finalize;

  monitor_class->get_mounted_volumes = get_mounted_volumes;
  monitor_class->get_connected_drives = get_connected_drives;

  native_class->priority = 0;
  native_class->get_volume_for_mountpoint = get_volume_for_mountpoint;
}

static void
mountpoints_changed (GUnixMountMonitor *mount_monitor,
		     gpointer user_data)
{
  GUnixVolumeMonitor *unix_monitor = user_data;

  /* Update both to make sure drives are created before volumes */
  update_drives (unix_monitor);
  update_volumes (unix_monitor);
}

static void
mounts_changed (GUnixMountMonitor *mount_monitor,
		gpointer user_data)
{
  GUnixVolumeMonitor *unix_monitor = user_data;

  /* Update both to make sure drives are created before volumes */
  update_drives (unix_monitor);
  update_volumes (unix_monitor);
}

static void
g_unix_volume_monitor_init (GUnixVolumeMonitor *unix_monitor)
{

  unix_monitor->mount_monitor = g_unix_mount_monitor_new ();

  g_signal_connect (unix_monitor->mount_monitor,
		    "mounts_changed", G_CALLBACK (mounts_changed),
		    unix_monitor);
  
  g_signal_connect (unix_monitor->mount_monitor,
		    "mountpoints_changed", G_CALLBACK (mountpoints_changed),
		    unix_monitor);
		    
  update_drives (unix_monitor);
  update_volumes (unix_monitor);

}

/**
 * g_unix_volume_monitor_new:
 * 
 * Returns:  a new #GVolumeMonitor.
 **/
GVolumeMonitor *
g_unix_volume_monitor_new (void)
{
  GUnixVolumeMonitor *monitor;

  monitor = g_object_new (G_TYPE_UNIX_VOLUME_MONITOR, NULL);
  
  return G_VOLUME_MONITOR (monitor);
}

static void
diff_sorted_lists (GList *list1, GList *list2, GCompareFunc compare,
		   GList **added, GList **removed)
{
  int order;
  
  *added = *removed = NULL;
  
  while (list1 != NULL &&
	 list2 != NULL)
    {
      order = (*compare) (list1->data, list2->data);
      if (order < 0)
	{
	  *removed = g_list_prepend (*removed, list1->data);
	  list1 = list1->next;
	}
      else if (order > 0)
	{
	  *added = g_list_prepend (*added, list2->data);
	  list2 = list2->next;
	}
      else
	{ /* same item */
	  list1 = list1->next;
	  list2 = list2->next;
	}
    }

  while (list1 != NULL)
    {
      *removed = g_list_prepend (*removed, list1->data);
      list1 = list1->next;
    }
  while (list2 != NULL)
    {
      *added = g_list_prepend (*added, list2->data);
      list2 = list2->next;
    }
}

/**
 * g_unix_volume_lookup_drive_for_mountpoint: 
 * @monitor:
 * @mountpoint:
 * 
 * Returns:  #GUnixDrive for the given @mountpoint.
 **/
GUnixDrive *
g_unix_volume_monitor_lookup_drive_for_mountpoint (GUnixVolumeMonitor *monitor,
						   const char *mountpoint)
{
  GList *l;

  for (l = monitor->drives; l != NULL; l = l->next)
    {
      GUnixDrive *drive = l->data;

      if (g_unix_drive_has_mountpoint (drive, mountpoint))
	return drive;
    }
  
  return NULL;
}

static GUnixVolume *
find_volume_by_mountpoint (GUnixVolumeMonitor *monitor,
			   const char *mountpoint)
{
  GList *l;

  for (l = monitor->volumes; l != NULL; l = l->next)
    {
      GUnixVolume *volume = l->data;

      if (g_unix_volume_has_mountpoint (volume, mountpoint))
	return volume;
    }
  
  return NULL;
}

static void
update_drives (GUnixVolumeMonitor *monitor)
{
  GList *new_mountpoints;
  GList *removed, *added;
  GList *l;
  GUnixDrive *drive;
  
  new_mountpoints = g_get_unix_mount_points (NULL);
  
  new_mountpoints = g_list_sort (new_mountpoints, (GCompareFunc) g_unix_mount_point_compare);
  
  diff_sorted_lists (monitor->last_mountpoints,
		     new_mountpoints, (GCompareFunc) g_unix_mount_point_compare,
		     &added, &removed);
  
  for (l = removed; l != NULL; l = l->next)
    {
      GUnixMountPoint *mountpoint = l->data;
      
      drive = g_unix_volume_monitor_lookup_drive_for_mountpoint (monitor,
								 g_unix_mount_point_get_mount_path (mountpoint));
      if (drive)
	{
	  g_unix_drive_disconnected (drive);
	  monitor->drives = g_list_remove (monitor->drives, drive);
	  g_signal_emit_by_name (monitor, "drive_disconnected", drive);
	  g_object_unref (drive);
	}
    }
  
  for (l = added; l != NULL; l = l->next)
    {
      GUnixMountPoint *mountpoint = l->data;
      
      drive = g_unix_drive_new (G_VOLUME_MONITOR (monitor), mountpoint);
      if (drive)
	{
	  monitor->drives = g_list_prepend (monitor->drives, drive);
	  g_signal_emit_by_name (monitor, "drive_connected", drive);
	}
    }
  
  g_list_free (added);
  g_list_free (removed);
  g_list_foreach (monitor->last_mountpoints,
		  (GFunc)g_unix_mount_point_free, NULL);
  g_list_free (monitor->last_mountpoints);
  monitor->last_mountpoints = new_mountpoints;
}

static void
update_volumes (GUnixVolumeMonitor *monitor)
{
  GList *new_mounts;
  GList *removed, *added;
  GList *l;
  GUnixVolume *volume;
  GUnixDrive *drive;
  const char *mount_path;
  
  new_mounts = g_get_unix_mounts (NULL);
  
  new_mounts = g_list_sort (new_mounts, (GCompareFunc) g_unix_mount_compare);
  
  diff_sorted_lists (monitor->last_mounts,
		     new_mounts, (GCompareFunc) g_unix_mount_compare,
		     &added, &removed);
  
  for (l = removed; l != NULL; l = l->next)
    {
      GUnixMount *mount = l->data;
      
      volume = find_volume_by_mountpoint (monitor, g_unix_mount_get_mount_path (mount));
      if (volume)
	{
	  g_unix_volume_unmounted (volume);
	  monitor->volumes = g_list_remove (monitor->volumes, volume);
	  g_signal_emit_by_name (monitor, "volume_unmounted", volume);
	  g_object_unref (volume);
	}
    }
  
  for (l = added; l != NULL; l = l->next)
    {
      GUnixMount *mount = l->data;

      mount_path = g_unix_mount_get_mount_path (mount);
      
      drive = g_unix_volume_monitor_lookup_drive_for_mountpoint (monitor,
								 mount_path);
      volume = g_unix_volume_new (mount, drive);
      if (volume)
	{
	  monitor->volumes = g_list_prepend (monitor->volumes, volume);
	  g_signal_emit_by_name (monitor, "volume_mounted", volume);
	}
    }
  
  g_list_free (added);
  g_list_free (removed);
  g_list_foreach (monitor->last_mounts,
		  (GFunc)g_unix_mount_free, NULL);
  g_list_free (monitor->last_mounts);
  monitor->last_mounts = new_mounts;
}
