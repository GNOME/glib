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
#include "gunixvolume.h"
#include "gunixdrive.h"
#include "gvolumeprivate.h"
#include "gvolumemonitor.h"
#include "gthemedicon.h"
#include "glibintl.h"

#include "gioalias.h"

struct _GUnixVolume {
  GObject parent;

  GUnixDrive *drive; /* owned by volume monitor */
  char *name;
  char *icon;
  char *mountpoint;
};

static void g_unix_volume_volume_iface_init (GVolumeIface *iface);

G_DEFINE_TYPE_WITH_CODE (GUnixVolume, g_unix_volume, G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (G_TYPE_VOLUME,
						g_unix_volume_volume_iface_init))


static void
g_unix_volume_finalize (GObject *object)
{
  GUnixVolume *volume;
  
  volume = G_UNIX_VOLUME (object);

  if (volume->drive)
    g_unix_drive_unset_volume (volume->drive, volume);
    
  g_assert (volume->drive == NULL);
  g_free (volume->name);
  g_free (volume->icon);
  g_free (volume->mountpoint);
  
  if (G_OBJECT_CLASS (g_unix_volume_parent_class)->finalize)
    (*G_OBJECT_CLASS (g_unix_volume_parent_class)->finalize) (object);
}

static void
g_unix_volume_class_init (GUnixVolumeClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = g_unix_volume_finalize;
}

static void
g_unix_volume_init (GUnixVolume *unix_volume)
{
}

static char *
get_filesystem_volume_name (const char *fs_type)
{
  /* TODO: add translation table from gnome-vfs */
  return g_strdup_printf (_("%s volume"), fs_type);
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
      icon_name = "media-floppy";
      break;
    case G_UNIX_MOUNT_TYPE_CDROM:
      icon_name = "media-optical";
      break;
    case G_UNIX_MOUNT_TYPE_NFS:
      /* TODO: Would like a better icon here... */
      icon_name = "drive-harddisk";
      break;
    case G_UNIX_MOUNT_TYPE_MEMSTICK:
      icon_name = "media-flash";
      break;
    case G_UNIX_MOUNT_TYPE_CAMERA:
      icon_name = "camera-photo";
      break;
    case G_UNIX_MOUNT_TYPE_IPOD:
      icon_name = "multimedia-player";
      break;
    case G_UNIX_MOUNT_TYPE_UNKNOWN:
    default:
      icon_name = "drive-harddisk";
      break;
    }
  return g_strdup (icon_name);
}

/**
 * g_unix_volume_new:
 * @mount: 
 * @drive:
 * 
 * Returns: a #GUnixVolume.
 * 
 **/
GUnixVolume *
g_unix_volume_new (GUnixMount *mount,
		   GUnixDrive *drive)
{
  GUnixVolume *volume;
  GUnixMountType type;
  const char *mount_path;
  char *volume_name;
  
  mount_path = g_unix_mount_get_mount_path (mount);
  
  /* No drive for volume. Ignore internal things */
  if (drive == NULL && g_unix_mount_is_system_internal (mount))
    return NULL;
  
  volume = g_object_new (G_TYPE_UNIX_VOLUME, NULL);
  volume->drive = drive;
  if (drive)
    g_unix_drive_set_volume (drive, volume);
  volume->mountpoint = g_strdup (mount_path);

  type = g_unix_mount_guess_type (mount);
  
  volume->icon = type_to_icon (type);
					  
  volume_name = NULL;
  if (mount_path)
    {
      if (strcmp (mount_path, "/") == 0)
	volume_name = g_strdup (_("Filesystem root"));
      else
	volume_name = g_filename_display_basename (mount_path);
    }
      
  if (volume_name == NULL)
    {
      if (g_unix_mount_get_fs_type (mount) != NULL)
	volume_name = g_strdup (get_filesystem_volume_name (g_unix_mount_get_fs_type (mount)));
    }
  
  if (volume_name == NULL)
    {
      /* TODO: Use volume size as name? */
      volume_name = g_strdup (_("Unknown volume"));
    }
  
  volume->name = volume_name;

  return volume;
}

/**
 * g_unix_volume_unmounted:
 * @volume: 
 * 
 **/
void
g_unix_volume_unmounted (GUnixVolume *volume)
{
  if (volume->drive)
    {
      g_unix_drive_unset_volume (volume->drive, volume);
      volume->drive = NULL;
      g_signal_emit_by_name (volume, "changed");
    }
}

/**
 * g_unix_volume_unset_drive:
 * @volume:
 * @drive:
 *   
 **/
void
g_unix_volume_unset_drive (GUnixVolume   *volume,
			   GUnixDrive    *drive)
{
  if (volume->drive == drive)
    {
      volume->drive = NULL;
      /* TODO: Emit changed in idle to avoid locking issues */
      g_signal_emit_by_name (volume, "changed");
    }
}

static GFile *
g_unix_volume_get_root (GVolume *volume)
{
  GUnixVolume *unix_volume = G_UNIX_VOLUME (volume);

  return g_file_new_for_path (unix_volume->mountpoint);
}

static GIcon *
g_unix_volume_get_icon (GVolume *volume)
{
  GUnixVolume *unix_volume = G_UNIX_VOLUME (volume);

  return g_themed_icon_new (unix_volume->icon);
}

static char *
g_unix_volume_get_name (GVolume *volume)
{
  GUnixVolume *unix_volume = G_UNIX_VOLUME (volume);
  
  return g_strdup (unix_volume->name);
}

/**
 * g_unix_volume_has_mountpoint:
 * @volume:
 * @mountpoint:
 * 
 * Returns: 
 **/
gboolean
g_unix_volume_has_mountpoint (GUnixVolume *volume,
			      const char  *mountpoint)
{
  return strcmp (volume->mountpoint, mountpoint) == 0;
}

static GDrive *
g_unix_volume_get_drive (GVolume *volume)
{
  GUnixVolume *unix_volume = G_UNIX_VOLUME (volume);

  if (unix_volume->drive)
    return G_DRIVE (g_object_ref (unix_volume->drive));
  
  return NULL;
}

static gboolean
g_unix_volume_can_unmount (GVolume *volume)
{
  /* TODO */
  return FALSE;
}

static gboolean
g_unix_volume_can_eject (GVolume *volume)
{
  /* TODO */
  return FALSE;
}

static void
g_unix_volume_unmount (GVolume         *volume,
		       GAsyncReadyCallback callback,
		       gpointer         user_data)
{
  /* TODO */
}

static gboolean
g_unix_volume_unmount_finish (GVolume *volume,
			      GAsyncResult *result,
			      GError **error)
{
  return TRUE;
}

static void
g_unix_volume_eject (GVolume         *volume,
		     GAsyncReadyCallback callback,
		     gpointer         user_data)
{
  /* TODO */
}

static gboolean
g_unix_volume_eject_finish (GVolume *volume,
			    GAsyncResult *result,
			    GError **error)
{
  return TRUE;
}

static void
g_unix_volume_volume_iface_init (GVolumeIface *iface)
{
  iface->get_root = g_unix_volume_get_root;
  iface->get_name = g_unix_volume_get_name;
  iface->get_icon = g_unix_volume_get_icon;
  iface->get_drive = g_unix_volume_get_drive;
  iface->can_unmount = g_unix_volume_can_unmount;
  iface->can_eject = g_unix_volume_can_eject;
  iface->unmount = g_unix_volume_unmount;
  iface->unmount_finish = g_unix_volume_unmount_finish;
  iface->eject = g_unix_volume_eject;
  iface->eject_finish = g_unix_volume_eject_finish;
}
