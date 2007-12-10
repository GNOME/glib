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

#include "glocaldirectorymonitor.h"
#include "gunixmounts.h"
#include "gdirectorymonitor.h"
#include "giomodule-priv.h"

#include <string.h>

#include "gioalias.h"

enum
{
  PROP_0,
  PROP_DIRNAME
};

static gboolean g_local_directory_monitor_cancel (GDirectoryMonitor* monitor);
static void mounts_changed (GUnixMountMonitor *mount_monitor, gpointer user_data);

G_DEFINE_ABSTRACT_TYPE (GLocalDirectoryMonitor, g_local_directory_monitor, G_TYPE_DIRECTORY_MONITOR)

static void
g_local_directory_monitor_finalize (GObject* object)
{
  GLocalDirectoryMonitor* local_monitor;
  local_monitor = G_LOCAL_DIRECTORY_MONITOR (object);

  g_free (local_monitor->dirname);

  if (local_monitor->mount_monitor)
    {
      g_signal_handlers_disconnect_by_func (local_monitor->mount_monitor, mounts_changed, local_monitor);
      g_object_unref (local_monitor->mount_monitor);
      local_monitor->mount_monitor = NULL;
    }

  if (G_OBJECT_CLASS (g_local_directory_monitor_parent_class)->finalize)
    (*G_OBJECT_CLASS (g_local_directory_monitor_parent_class)->finalize) (object);
}

static void
g_local_directory_monitor_set_property (GObject      *object,
                                        guint         property_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
  switch (property_id)
  {
    case PROP_DIRNAME:
      /* Do nothing */
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static GObject *
g_local_directory_monitor_constructor (GType                  type,
                                       guint                  n_construct_properties,
                                       GObjectConstructParam *construct_properties)
{
  GObject *obj;
  GLocalDirectoryMonitorClass *klass;
  GObjectClass *parent_class;
  GLocalDirectoryMonitor *local_monitor;
  const gchar *dirname = NULL;
  gint i;
  
  klass = G_LOCAL_DIRECTORY_MONITOR_CLASS (g_type_class_peek (G_TYPE_LOCAL_DIRECTORY_MONITOR));
  parent_class = G_OBJECT_CLASS (g_type_class_peek_parent (klass));
  obj = parent_class->constructor (type,
                                   n_construct_properties,
                                   construct_properties);

  local_monitor = G_LOCAL_DIRECTORY_MONITOR (obj);

  for (i = 0; i < n_construct_properties; i++)
    {
      if (strcmp ("dirname", g_param_spec_get_name (construct_properties[i].pspec)) == 0)
        {
          g_warn_if_fail (G_VALUE_HOLDS_STRING (construct_properties[i].value));
          dirname = g_value_get_string (construct_properties[i].value);
          break;
        }
    }

  local_monitor->dirname = g_strdup (dirname);

  if (!klass->mount_notify)
    {
#ifdef G_OS_WIN32
      g_warning ("G_OS_WIN32: no mount emulation");
#else
      GUnixMount *mount;
      
      /* Emulate unmount detection */
      
      mount = g_get_unix_mount_at (local_monitor->dirname, NULL);
      
      local_monitor->was_mounted = mount != NULL;
      
      if (mount)
        g_unix_mount_free (mount);

      local_monitor->mount_monitor = g_unix_mount_monitor_new ();
      g_signal_connect (local_monitor->mount_monitor, "mounts_changed",
        G_CALLBACK (mounts_changed), local_monitor);
#endif
    }

  return obj;
}

static void
g_local_directory_monitor_class_init (GLocalDirectoryMonitorClass* klass)
{
  GObjectClass* gobject_class = G_OBJECT_CLASS (klass);
  GDirectoryMonitorClass *dir_monitor_class = G_DIRECTORY_MONITOR_CLASS (klass);
  
  gobject_class->finalize = g_local_directory_monitor_finalize;
  gobject_class->set_property = g_local_directory_monitor_set_property;
  gobject_class->constructor = g_local_directory_monitor_constructor;

  dir_monitor_class->cancel = g_local_directory_monitor_cancel;

  g_object_class_install_property (gobject_class, PROP_DIRNAME,
    g_param_spec_string ("dirname", "Directory name", "Directory to monitor",
        NULL, G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));

  klass->mount_notify = FALSE;
}

static void
g_local_directory_monitor_init (GLocalDirectoryMonitor* local_monitor)
{
}

static void
mounts_changed (GUnixMountMonitor *mount_monitor,
                gpointer           user_data)
{
  GLocalDirectoryMonitor *local_monitor = user_data;
  GUnixMount *mount;
  gboolean is_mounted;
  GFile *file;
  
  /* Emulate unmount detection */
#ifdef G_OS_WIN32
  mount = NULL;
  g_warning ("G_OS_WIN32: no mount emulation");
#else  
  mount = g_get_unix_mount_at (local_monitor->dirname, NULL);
  
  is_mounted = mount != NULL;
  
  if (mount)
    g_unix_mount_free (mount);
#endif

  if (local_monitor->was_mounted != is_mounted)
    {
      if (local_monitor->was_mounted && !is_mounted)
        {
          file = g_file_new_for_path (local_monitor->dirname);
          g_directory_monitor_emit_event (G_DIRECTORY_MONITOR (local_monitor),
                                          file, NULL,
                                          G_FILE_MONITOR_EVENT_UNMOUNTED);
          g_object_unref (file);
        }
      local_monitor->was_mounted = is_mounted;
    }
}

static gint
_compare_monitor_class_by_prio (gconstpointer a,
                                gconstpointer b,
                                gpointer      user_data)
{
  GType *type1 = (GType *) a, *type2 = (GType *) b;
  GLocalDirectoryMonitorClass *klass1, *klass2;
  gint ret;

  klass1 = G_LOCAL_DIRECTORY_MONITOR_CLASS (g_type_class_ref (*type1));
  klass2 = G_LOCAL_DIRECTORY_MONITOR_CLASS (g_type_class_ref (*type2));

  ret = klass1->prio - klass2->prio;

  g_type_class_unref (klass1);
  g_type_class_unref (klass2);

  return ret;
}

extern GType _g_inotify_directory_monitor_get_type (void);

static gpointer
get_default_local_directory_monitor (gpointer data)
{
  GType *monitor_impls, chosen_type;
  guint n_monitor_impls;
  GType *ret = (GType *) data;
  gint i;

#if defined(HAVE_SYS_INOTIFY_H) || defined(HAVE_LINUX_INOTIFY_H)
  /* Register Inotify monitor */
  _g_inotify_directory_monitor_get_type ();
#endif

  _g_io_modules_ensure_loaded ();
  
  monitor_impls = g_type_children (G_TYPE_LOCAL_DIRECTORY_MONITOR,
                                   &n_monitor_impls);

  chosen_type = G_TYPE_INVALID;

  g_qsort_with_data (monitor_impls,
                     n_monitor_impls,
                     sizeof (GType),
                     _compare_monitor_class_by_prio,
                     NULL);

  for (i = n_monitor_impls - 1; i >= 0 && chosen_type == G_TYPE_INVALID; i--)
    {    
      GLocalDirectoryMonitorClass *klass;

      klass = G_LOCAL_DIRECTORY_MONITOR_CLASS (g_type_class_ref (monitor_impls[i]));

      if (klass->is_supported())
        chosen_type = monitor_impls[i];

      g_type_class_unref (klass);
    }

  g_free (monitor_impls);
  *ret = chosen_type;

  return NULL;
}

/**
 * g_local_directory_monitor_new:
 * @dirname: filename of the directory to monitor.
 * @flags: #GFileMonitorFlags.
 * 
 * Returns: new #GDirectoryMonitor for the given @dirname.
 **/
GDirectoryMonitor*
_g_local_directory_monitor_new (const char*       dirname,
				GFileMonitorFlags flags)
{
  static GOnce once_init = G_ONCE_INIT;
  static GType monitor_type = G_TYPE_INVALID;

  g_once (&once_init, get_default_local_directory_monitor, &monitor_type);

  if (monitor_type != G_TYPE_INVALID)
    return G_DIRECTORY_MONITOR (g_object_new (monitor_type, "dirname", dirname, NULL));

  return NULL;
}

static gboolean
g_local_directory_monitor_cancel (GDirectoryMonitor* monitor)
{
  GLocalDirectoryMonitor *local_monitor = G_LOCAL_DIRECTORY_MONITOR (monitor);

  if (local_monitor->mount_monitor)
    {
      g_signal_handlers_disconnect_by_func (local_monitor->mount_monitor, mounts_changed, local_monitor);
      g_object_unref (local_monitor->mount_monitor);
      local_monitor->mount_monitor = NULL;
    }

  return TRUE;
}

#define __G_LOCAL_DIRECTORY_MONITOR_C__
#include "gioaliasdef.c"
