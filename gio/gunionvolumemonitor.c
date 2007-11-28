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
#include "gunionvolumemonitor.h"
#include "gvolumeprivate.h"
#include "giomodule.h"
#ifdef G_OS_UNIX
#include "gunixvolumemonitor.h"
#endif
#include "gnativevolumemonitor.h"

#include "glibintl.h"

#include "gioalias.h"

struct _GUnionVolumeMonitor {
  GVolumeMonitor parent;

  GList *monitors;
};

static void g_union_volume_monitor_remove_monitor (GUnionVolumeMonitor *union_monitor,
						   GVolumeMonitor *child_monitor);


#define g_union_volume_monitor_get_type _g_union_volume_monitor_get_type
G_DEFINE_TYPE (GUnionVolumeMonitor, g_union_volume_monitor, G_TYPE_VOLUME_MONITOR);


G_LOCK_DEFINE_STATIC(the_volume_monitor);
static GUnionVolumeMonitor *the_volume_monitor = NULL;

static void
g_union_volume_monitor_finalize (GObject *object)
{
  GUnionVolumeMonitor *monitor;
  
  monitor = G_UNION_VOLUME_MONITOR (object);

  while (monitor->monitors != NULL)
    g_union_volume_monitor_remove_monitor (monitor,
					   monitor->monitors->data);
  
  if (G_OBJECT_CLASS (g_union_volume_monitor_parent_class)->finalize)
    (*G_OBJECT_CLASS (g_union_volume_monitor_parent_class)->finalize) (object);
}

static void
g_union_volume_monitor_dispose (GObject *object)
{
  GUnionVolumeMonitor *monitor;
  
  monitor = G_UNION_VOLUME_MONITOR (object);

  G_LOCK (the_volume_monitor);
  the_volume_monitor = NULL;
  G_UNLOCK (the_volume_monitor);
  
  if (G_OBJECT_CLASS (g_union_volume_monitor_parent_class)->dispose)
    (*G_OBJECT_CLASS (g_union_volume_monitor_parent_class)->dispose) (object);
}

static GList *
get_mounted_volumes (GVolumeMonitor *volume_monitor)
{
  GUnionVolumeMonitor *monitor;
  GVolumeMonitor *child_monitor;
  GList *res;
  GList *l;
  
  monitor = G_UNION_VOLUME_MONITOR (volume_monitor);

  res = NULL;
  
  G_LOCK (the_volume_monitor);

  for (l = monitor->monitors; l != NULL; l = l->next)
    {
      child_monitor = l->data;

      res = g_list_concat (res,
			   g_volume_monitor_get_mounted_volumes (child_monitor));
    }
  
  G_UNLOCK (the_volume_monitor);

  return res;
}

static GList *
get_connected_drives (GVolumeMonitor *volume_monitor)
{
  GUnionVolumeMonitor *monitor;
  GVolumeMonitor *child_monitor;
  GList *res;
  GList *l;
  
  monitor = G_UNION_VOLUME_MONITOR (volume_monitor);

  res = NULL;
  
  G_LOCK (the_volume_monitor);

  for (l = monitor->monitors; l != NULL; l = l->next)
    {
      child_monitor = l->data;

      res = g_list_concat (res,
			   g_volume_monitor_get_connected_drives (child_monitor));
    }
  
  G_UNLOCK (the_volume_monitor);

  return res;
}

static void
g_union_volume_monitor_class_init (GUnionVolumeMonitorClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GVolumeMonitorClass *monitor_class = G_VOLUME_MONITOR_CLASS (klass);
  
  gobject_class->finalize = g_union_volume_monitor_finalize;
  gobject_class->dispose = g_union_volume_monitor_dispose;

  monitor_class->get_mounted_volumes = get_mounted_volumes;
  monitor_class->get_connected_drives = get_connected_drives;
}

static void
child_volume_mounted (GVolumeMonitor *child_monitor,
		      GVolume *child_volume,
		      GUnionVolumeMonitor *union_monitor)
{
  g_signal_emit_by_name (union_monitor,
			 "volume_mounted",
			 child_volume);
}

static void
child_volume_pre_unmount (GVolumeMonitor *child_monitor,
			  GVolume *child_volume,
			  GUnionVolumeMonitor *union_monitor)
{
  g_signal_emit_by_name (union_monitor,
			 "volume_pre_unmount",
			 child_volume);
}

static void
child_volume_unmounted (GVolumeMonitor *child_monitor,
			GVolume *child_volume,
			GUnionVolumeMonitor *union_monitor)
{
  g_signal_emit_by_name (union_monitor,
			 "volume_unmounted",
			 child_volume);
}

static void
child_drive_connected (GVolumeMonitor *child_monitor,
		       GDrive *child_drive,
		       GUnionVolumeMonitor *union_monitor)
{
  g_signal_emit_by_name (union_monitor,
			 "drive_connected",
			 child_drive);
}

static void
child_drive_disconnected (GVolumeMonitor *child_monitor,
			  GDrive *child_drive,
			  GUnionVolumeMonitor *union_monitor)
{
  g_signal_emit_by_name (union_monitor,
			 "drive_disconnected",
			 child_drive);
}

static void
g_union_volume_monitor_add_monitor (GUnionVolumeMonitor *union_monitor,
				    GVolumeMonitor *volume_monitor)
{
  if (g_list_find (union_monitor->monitors, volume_monitor))
    return;

  union_monitor->monitors =
    g_list_prepend (union_monitor->monitors,
		    g_object_ref (volume_monitor));

  g_signal_connect (volume_monitor, "volume_mounted", (GCallback)child_volume_mounted, union_monitor);
  g_signal_connect (volume_monitor, "volume_pre_unmount", (GCallback)child_volume_pre_unmount, union_monitor);
  g_signal_connect (volume_monitor, "volume_unmounted", (GCallback)child_volume_unmounted, union_monitor);
  g_signal_connect (volume_monitor, "drive_connected", (GCallback)child_drive_connected, union_monitor);
  g_signal_connect (volume_monitor, "drive_disconnected", (GCallback)child_drive_disconnected, union_monitor);
}

static void
g_union_volume_monitor_remove_monitor (GUnionVolumeMonitor *union_monitor,
				       GVolumeMonitor *child_monitor)
{
  GList *l;

  l = g_list_find (union_monitor->monitors, child_monitor);
  if (l == NULL)
    return;

  union_monitor->monitors = g_list_delete_link (union_monitor->monitors, l);

  g_signal_handlers_disconnect_by_func (child_monitor, child_volume_mounted, union_monitor);
  g_signal_handlers_disconnect_by_func (child_monitor, child_volume_pre_unmount, union_monitor);
  g_signal_handlers_disconnect_by_func (child_monitor, child_volume_unmounted, union_monitor);
  g_signal_handlers_disconnect_by_func (child_monitor, child_drive_connected, union_monitor);
  g_signal_handlers_disconnect_by_func (child_monitor, child_drive_disconnected, union_monitor);
}

static gpointer
get_default_native_type (gpointer data)
{
  GNativeVolumeMonitorClass *klass;
  GType *monitors;
  guint n_monitors;
  GType native_type;
  GType *ret = (GType *) data;
  int native_prio;
  int i;
      
#ifdef G_OS_UNIX
  /* Ensure GUnixVolumeMonitor type is available */
  {
    volatile GType unix_type;
    /* volatile is required to avoid any G_GNUC_CONST optimizations */
    unix_type = _g_unix_volume_monitor_get_type ();
  }
#endif
      
  /* Ensure vfs in modules loaded */
  g_io_modules_ensure_loaded (GIO_MODULE_DIR);
      
  monitors = g_type_children (G_TYPE_NATIVE_VOLUME_MONITOR, &n_monitors);
  native_type = 0;
  native_prio = -1;
  
  for (i = 0; i < n_monitors; i++)
    {
      klass = G_NATIVE_VOLUME_MONITOR_CLASS (g_type_class_ref (monitors[i]));
      if (klass->priority > native_prio)
        {
	  native_prio = klass->priority;
	  native_type = monitors[i];
	}

      g_type_class_unref (klass);
    }
      
  g_free (monitors);

  *ret = native_type;

  return NULL;
}

static GType
get_native_type (void)
{
  static GOnce once_init = G_ONCE_INIT;
  static GType type = G_TYPE_INVALID;

  g_once (&once_init, get_default_native_type, &type);
  
  return type;
}

static void
g_union_volume_monitor_init (GUnionVolumeMonitor *union_monitor)
{
  GVolumeMonitor *monitor;
  GType *monitors;
  guint n_monitors;
  GType native_type;
  int i;

  native_type = get_native_type ();

  if (native_type != G_TYPE_INVALID)
    {
      monitor = g_object_new (native_type, NULL);
      g_union_volume_monitor_add_monitor (union_monitor, monitor);
      g_object_unref (monitor);
    }
  
  monitors = g_type_children (G_TYPE_VOLUME_MONITOR, &n_monitors);
  
  for (i = 0; i < n_monitors; i++)
    {
      if (monitors[i] == G_TYPE_UNION_VOLUME_MONITOR ||
	  g_type_is_a (monitors[i], G_TYPE_NATIVE_VOLUME_MONITOR))
	continue;
      
      monitor = g_object_new (monitors[i], NULL);
      g_union_volume_monitor_add_monitor (union_monitor, monitor);
      g_object_unref (monitor);
    }
      
  g_free (monitors);
}

static GUnionVolumeMonitor *
g_union_volume_monitor_new (void)
{
  GUnionVolumeMonitor *monitor;

  monitor = g_object_new (G_TYPE_UNION_VOLUME_MONITOR, NULL);
  
  return monitor;
}


/**
 * g_volume_monitor_get:
 * 
 * Gets the volume monitor used by gio.
 *
 * Returns: a reference to the #GVolumeMonitor used by gio. Call
 *    g_object_unref() when done with it.
 **/
GVolumeMonitor *
g_volume_monitor_get (void)
{
  GVolumeMonitor *vm;
  
  G_LOCK (the_volume_monitor);

  if (the_volume_monitor)
    vm = G_VOLUME_MONITOR (g_object_ref (the_volume_monitor));
  else
    {
      the_volume_monitor = g_union_volume_monitor_new ();
      vm = G_VOLUME_MONITOR (the_volume_monitor);
    }
  
  G_UNLOCK (the_volume_monitor);

  return vm;
}

/**
 * g_volume_get_for_mount_path:
 * @mountpoint: a string.
 * 
 * Returns: a #GVolume for given @mountpoint or %NULL.  
 **/
GVolume *
_g_volume_get_for_mount_path (const char *mountpoint)
{
  GType native_type;
  GNativeVolumeMonitorClass *klass;
  GVolume *volume;
  
  native_type = get_native_type ();

  if (native_type == G_TYPE_INVALID)
    return NULL;

  volume = NULL;
  
  klass = G_NATIVE_VOLUME_MONITOR_CLASS (g_type_class_ref (native_type));
  if (klass->get_volume_for_mountpoint)
    volume = klass->get_volume_for_mountpoint (mountpoint);
  
  g_type_class_unref (klass);

  return volume;
}

#define __G_UNION_VOLUME_MONITOR_C__
#include "gioaliasdef.c"
