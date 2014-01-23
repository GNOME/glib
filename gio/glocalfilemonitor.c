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
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Alexander Larsson <alexl@redhat.com>
 */

#include "config.h"

#include "gioenumtypes.h"
#include "glocalfilemonitor.h"
#include "giomodule-priv.h"
#include "gioerror.h"
#include "glibintl.h"

#include <string.h>


enum
{
  PROP_0,
  PROP_FILENAME,
  PROP_FLAGS
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
  GLocalFileMonitor *local_monitor = G_LOCAL_FILE_MONITOR (object);

  switch (property_id)
    {
    case PROP_FILENAME:
      local_monitor->filename = g_value_dup_string (value);
      break;

    case PROP_FLAGS:
      local_monitor->flags = g_value_get_flags (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

void
g_local_file_monitor_start (GLocalFileMonitor *local_monitor)
{
  GLocalFileMonitorClass *class;

  class = G_LOCAL_FILE_MONITOR_GET_CLASS (local_monitor);

  if (class->start)
    class->start (local_monitor);
}

static void
g_local_file_monitor_finalize (GObject *object)
{
  GLocalFileMonitor *local_monitor = G_LOCAL_FILE_MONITOR (object);

  g_free (local_monitor->filename);

  G_OBJECT_CLASS (g_local_file_monitor_parent_class)->finalize (object);
}

static void g_local_file_monitor_class_init (GLocalFileMonitorClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = g_local_file_monitor_set_property;
  gobject_class->finalize = g_local_file_monitor_finalize;

  g_object_class_install_property (gobject_class, 
                                   PROP_FILENAME,
                                   g_param_spec_string ("filename", 
                                                        P_("File name"), 
                                                        P_("File name to monitor"),
                                                        NULL, 
                                                        G_PARAM_CONSTRUCT_ONLY|
                                                        G_PARAM_WRITABLE|
                                                        G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB));

  g_object_class_install_property (gobject_class,
                                   PROP_FLAGS,
                                   g_param_spec_flags ("flags",
						       P_("Monitor flags"),
						       P_("Monitor flags"),
						       G_TYPE_FILE_MONITOR_FLAGS,
						       0,
						       G_PARAM_CONSTRUCT_ONLY|
						       G_PARAM_WRITABLE|
						       G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB));
}

GFileMonitor*
_g_local_file_monitor_new (const char         *pathname,
                           GFileMonitorFlags   flags,
                           GMainContext       *context,
                           gboolean            is_remote_fs,
                           gboolean            do_start,
                           GError            **error)
{
  GFileMonitor *monitor = NULL;
  GType type = G_TYPE_INVALID;

  if (is_remote_fs)
    type = _g_io_module_get_default_type (G_NFS_FILE_MONITOR_EXTENSION_POINT_NAME,
                                          "GIO_USE_FILE_MONITOR",
                                          G_STRUCT_OFFSET (GLocalFileMonitorClass, is_supported));

  if (type == G_TYPE_INVALID)
    type = _g_io_module_get_default_type (G_LOCAL_FILE_MONITOR_EXTENSION_POINT_NAME,
                                          "GIO_USE_FILE_MONITOR",
                                          G_STRUCT_OFFSET (GLocalFileMonitorClass, is_supported));

  if (type != G_TYPE_INVALID)
    monitor = G_FILE_MONITOR (g_object_new (type, "filename", pathname, "flags", flags, "context", context, NULL));
  else
    g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                         _("Unable to find default local file monitor type"));

  if (monitor && do_start)
    g_local_file_monitor_start (G_LOCAL_FILE_MONITOR (monitor));

  return monitor;
}
