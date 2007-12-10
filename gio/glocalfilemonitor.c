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

#include "glocalfilemonitor.h"
#include "giomodule-priv.h"

#include <string.h>

#include "gioalias.h"

enum
{
  PROP_0,
  PROP_FILENAME
};

G_DEFINE_ABSTRACT_TYPE (GLocalFileMonitor, g_local_file_monitor, G_TYPE_FILE_MONITOR)

static void
g_local_file_monitor_init (GLocalFileMonitor* local_monitor)
{
}

static void
g_local_file_monitor_set_property (GObject      *object,
                                   guint         property_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  switch (property_id)
  {
    case PROP_FILENAME:
      /* Do nothing */
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static GObject *
g_local_file_monitor_constructor (GType                  type,
                                  guint                  n_construct_properties,
                                  GObjectConstructParam *construct_properties)
{
  GObject *obj;
  GLocalFileMonitorClass *klass;
  GObjectClass *parent_class;
  GLocalFileMonitor *local_monitor;
  const gchar *filename = NULL;
  gint i;
  
  klass = G_LOCAL_FILE_MONITOR_CLASS (g_type_class_peek (G_TYPE_LOCAL_FILE_MONITOR));
  parent_class = G_OBJECT_CLASS (g_type_class_peek_parent (klass));
  obj = parent_class->constructor (type,
                                   n_construct_properties,
                                   construct_properties);

  local_monitor = G_LOCAL_FILE_MONITOR (obj);

  for (i = 0; i < n_construct_properties; i++)
    {
      if (strcmp ("filename", g_param_spec_get_name (construct_properties[i].pspec)) == 0)
        {
          g_warn_if_fail (G_VALUE_HOLDS_STRING (construct_properties[i].value));
          filename = g_value_get_string (construct_properties[i].value);
          break;
        }
    }

  g_warn_if_fail (filename != NULL);

  local_monitor->filename = g_strdup (filename);
  return obj;
}

static void
g_local_file_monitor_finalize (GObject *object)
{
  GLocalFileMonitor *local_monitor = G_LOCAL_FILE_MONITOR (object);
  if (local_monitor->filename)
    {
      g_free (local_monitor->filename);
      local_monitor->filename = NULL;
    }

  if (G_OBJECT_CLASS (g_local_file_monitor_parent_class)->finalize)
    (*G_OBJECT_CLASS (g_local_file_monitor_parent_class)->finalize) (object);
}

static void
g_local_file_monitor_class_init (GLocalFileMonitorClass* klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = g_local_file_monitor_set_property;
  gobject_class->finalize = g_local_file_monitor_finalize;
  gobject_class->constructor = g_local_file_monitor_constructor;

  g_object_class_install_property (gobject_class, PROP_FILENAME,
      g_param_spec_string ("filename", "File name", "File name to monitor",
          NULL, G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));
}

static gint
_compare_monitor_class_by_prio (gconstpointer a,
                                gconstpointer b,
                                gpointer      user_data)
{
  GType *type1 = (GType *) a, *type2 = (GType *) b;
  GLocalFileMonitorClass *klass1, *klass2;
  gint ret;

  klass1 = G_LOCAL_FILE_MONITOR_CLASS (g_type_class_ref (*type1));
  klass2 = G_LOCAL_FILE_MONITOR_CLASS (g_type_class_ref (*type2));

  ret = klass1->prio - klass2->prio;

  g_type_class_unref (klass1);
  g_type_class_unref (klass2);

  return ret;
}

extern GType _g_inotify_file_monitor_get_type (void);

static gpointer
get_default_local_file_monitor (gpointer data)
{
  GType *monitor_impls, chosen_type;
  guint n_monitor_impls;
  gint i;
  GType *ret = (GType *) data;

#if defined(HAVE_SYS_INOTIFY_H) || defined(HAVE_LINUX_INOTIFY_H)
  /* Register Inotify monitor */
  _g_inotify_file_monitor_get_type ();
#endif

  _g_io_modules_ensure_loaded ();
  
  monitor_impls = g_type_children (G_TYPE_LOCAL_FILE_MONITOR,
                                   &n_monitor_impls);

  chosen_type = G_TYPE_INVALID;

  g_qsort_with_data (monitor_impls,
                     n_monitor_impls,
                     sizeof (GType),
                     _compare_monitor_class_by_prio,
                     NULL);

  for (i = n_monitor_impls - 1; i >= 0 && chosen_type == G_TYPE_INVALID; i--)
    {    
      GLocalFileMonitorClass *klass;

      klass = G_LOCAL_FILE_MONITOR_CLASS (g_type_class_ref (monitor_impls[i]));

      if (klass->is_supported())
        chosen_type = monitor_impls[i];

      g_type_class_unref (klass);
    }

  g_free (monitor_impls);

  *ret = chosen_type;

  return NULL;
}

/**
 * g_local_file_monitor_new:
 * @pathname: path name to monitor.
 * @flags: #GFileMonitorFlags.
 * 
 * Returns: a new #GFileMonitor for the given @pathname. 
 **/
GFileMonitor*
_g_local_file_monitor_new (const char        *pathname,
			   GFileMonitorFlags  flags)
{
  static GOnce once_init = G_ONCE_INIT;
  static GType monitor_type = G_TYPE_INVALID;

  g_once (&once_init, get_default_local_file_monitor, &monitor_type);

  if (monitor_type != G_TYPE_INVALID)
    return G_FILE_MONITOR (g_object_new (monitor_type, "filename", pathname, NULL));

  return NULL;
}

#define __G_LOCAL_FILE_MONITOR_C__
#include "gioaliasdef.c"
