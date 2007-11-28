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
#include "gunixdrive.h"
#include "gunixvolume.h"
#include "gdriveprivate.h"
#include "gthemedicon.h"
#include "gvolumemonitor.h"
#include "glibintl.h"

#include "gioalias.h"

struct _GUnixDrive {
  GObject parent;

  GUnixVolume *volume; /* owned by volume monitor */
  char *name;
  char *icon;
  char *mountpoint;
  GUnixMountType guessed_type;
};

static void g_unix_volume_drive_iface_init (GDriveIface *iface);

G_DEFINE_TYPE_WITH_CODE (GUnixDrive, g_unix_drive, G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (G_TYPE_DRIVE,
						g_unix_volume_drive_iface_init))

static void
g_unix_drive_finalize (GObject *object)
{
  GUnixDrive *drive;
  
  drive = G_UNIX_DRIVE (object);

  if (drive->volume)
    g_unix_volume_unset_drive (drive->volume, drive);
  
  g_free (drive->name);
  g_free (drive->icon);
  g_free (drive->mountpoint);

  if (G_OBJECT_CLASS (g_unix_drive_parent_class)->finalize)
    (*G_OBJECT_CLASS (g_unix_drive_parent_class)->finalize) (object);
}

static void
g_unix_drive_class_init (GUnixDriveClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = g_unix_drive_finalize;
}

static void
g_unix_drive_init (GUnixDrive *unix_drive)
{
}

static char *
type_to_icon (GUnixMountType type)
{
  const char *icon_name = NULL;
  
  switch (type)
    {
    case G_UNIX_MOUNT_TYPE_HD:
      icon_name = "drive-harddisk";
      break;
    case G_UNIX_MOUNT_TYPE_FLOPPY:
    case G_UNIX_MOUNT_TYPE_ZIP:
    case G_UNIX_MOUNT_TYPE_JAZ:
    case G_UNIX_MOUNT_TYPE_MEMSTICK:
      icon_name = "drive-removable-media";
      break;
    case G_UNIX_MOUNT_TYPE_CDROM:
      icon_name = "drive-optical";
      break;
    case G_UNIX_MOUNT_TYPE_NFS:
      /* TODO: Would like a better icon here... */
      icon_name = "drive-removable-media";
      break;
    case G_UNIX_MOUNT_TYPE_CAMERA:
      icon_name = "camera-photo";
      break;
    case G_UNIX_MOUNT_TYPE_IPOD:
      icon_name = "multimedia-player";
      break;
    case G_UNIX_MOUNT_TYPE_UNKNOWN:
    default:
      icon_name = "drive-removable-media";
      break;
    }
  return g_strdup (icon_name);
}

/**
 * g_unix_drive_new:
 * @volume_monitor: a #GVolumeMonitor.
 * @mountpoint: a #GUnixMountPoint.
 * 
 * Returns: a #GUnixDrive for the given #GUnixMountPoint.
 **/
GUnixDrive *
g_unix_drive_new (GVolumeMonitor *volume_monitor,
		  GUnixMountPoint *mountpoint)
{
  GUnixDrive *drive;
  
  if (!(g_unix_mount_point_is_user_mountable (mountpoint) ||
	g_str_has_prefix (g_unix_mount_point_get_device_path (mountpoint), "/vol/")) ||
      g_unix_mount_point_is_loopback (mountpoint))
    return NULL;
  
  drive = g_object_new (G_TYPE_UNIX_DRIVE, NULL);

  drive->guessed_type = g_unix_mount_point_guess_type (mountpoint);
  
  /* TODO: */
  drive->mountpoint = g_strdup (g_unix_mount_point_get_mount_path (mountpoint));
  drive->icon = type_to_icon (drive->guessed_type);
  drive->name = g_strdup (_("Unknown drive"));
  
  return drive;
}

/**
 * g_unix_drive_disconnected:
 * @drive:
 * 
 **/
void
g_unix_drive_disconnected (GUnixDrive *drive)
{
  if (drive->volume)
    {
      g_unix_volume_unset_drive (drive->volume, drive);
      drive->volume = NULL;
    }
}

/**
 * g_unix_drive_set_volume:
 * @drive:
 * @volume:
 *  
 **/
void
g_unix_drive_set_volume (GUnixDrive     *drive,
			 GUnixVolume    *volume)
{
  if (drive->volume == volume)
    return;
  
  if (drive->volume)
    g_unix_volume_unset_drive (drive->volume, drive);
  
  drive->volume = volume;
  
  /* TODO: Emit changed in idle to avoid locking issues */
  g_signal_emit_by_name (drive, "changed");
}

/**
 * g_unix_drive_unset_volume:
 * @drive:
 * @volume:
 *
 **/
void
g_unix_drive_unset_volume (GUnixDrive     *drive,
			   GUnixVolume    *volume)
{
  if (drive->volume == volume)
    {
      drive->volume = NULL;
      /* TODO: Emit changed in idle to avoid locking issues */
      g_signal_emit_by_name (drive, "changed");
    }
}

static GIcon *
g_unix_drive_get_icon (GDrive *drive)
{
  GUnixDrive *unix_drive = G_UNIX_DRIVE (drive);

  return g_themed_icon_new (unix_drive->icon);
}

static char *
g_unix_drive_get_name (GDrive *drive)
{
  GUnixDrive *unix_drive = G_UNIX_DRIVE (drive);
  
  return g_strdup (unix_drive->name);
}

static gboolean
g_unix_drive_is_automounted (GDrive *drive)
{
  /* TODO */
  return FALSE;
}

static gboolean
g_unix_drive_can_mount (GDrive *drive)
{
  /* TODO */
  return TRUE;
}

static gboolean
g_unix_drive_can_eject (GDrive *drive)
{
  /* TODO */
  return FALSE;
}

static GList *
g_unix_drive_get_volumes (GDrive *drive)
{
  GList *l;
  GUnixDrive *unix_drive = G_UNIX_DRIVE (drive);

  l = NULL;
  if (unix_drive->volume)
    l = g_list_prepend (l, g_object_ref (unix_drive->volume));

  return l;
}

static gboolean
g_unix_drive_has_volumes (GDrive *drive)
{
  GUnixDrive *unix_drive = G_UNIX_DRIVE (drive);

  return unix_drive->volume != NULL;
}


gboolean
g_unix_drive_has_mountpoint (GUnixDrive *drive,
			     const char  *mountpoint)
{
  return strcmp (drive->mountpoint, mountpoint) == 0;
}

static void
g_unix_drive_mount (GDrive         *drive,
		    GMountOperation *mount_operation,
		    GCancellable *cancellable,
		    GAsyncReadyCallback callback,
		    gpointer        user_data)
{
  /* TODO */
}


static gboolean
g_unix_drive_mount_finish (GDrive *drive,
			   GAsyncResult *result,
			   GError **error)
{
  return TRUE;
}

static void
g_unix_drive_eject (GDrive         *drive,
		    GCancellable *cancellable,
		    GAsyncReadyCallback callback,
		    gpointer        user_data)
{
  /* TODO */
}

static gboolean
g_unix_drive_eject_finish (GDrive *drive,
			   GAsyncResult *result,
			   GError **error)
{
  return TRUE;
}

static void
g_unix_volume_drive_iface_init (GDriveIface *iface)
{
  iface->get_name = g_unix_drive_get_name;
  iface->get_icon = g_unix_drive_get_icon;
  iface->has_volumes = g_unix_drive_has_volumes;
  iface->get_volumes = g_unix_drive_get_volumes;
  iface->is_automounted = g_unix_drive_is_automounted;
  iface->can_mount = g_unix_drive_can_mount;
  iface->can_eject = g_unix_drive_can_eject;
  iface->mount = g_unix_drive_mount;
  iface->mount_finish = g_unix_drive_mount_finish;
  iface->eject = g_unix_drive_eject;
  iface->eject_finish = g_unix_drive_eject_finish;
}
