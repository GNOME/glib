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

#ifndef __G_DIRECTORY_MONITOR_H__
#define __G_DIRECTORY_MONITOR_H__

#include <glib-object.h>
#include <gio/gfile.h>
#include <gio/gfilemonitor.h>

G_BEGIN_DECLS

#define G_TYPE_DIRECTORY_MONITOR         (g_directory_monitor_get_type ())
#define G_DIRECTORY_MONITOR(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), G_TYPE_DIRECTORY_MONITOR, GDirectoryMonitor))
#define G_DIRECTORY_MONITOR_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), G_TYPE_DIRECTORY_MONITOR, GDirectoryMonitorClass))
#define G_IS_DIRECTORY_MONITOR(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), G_TYPE_DIRECTORY_MONITOR))
#define G_IS_DIRECTORY_MONITOR_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), G_TYPE_DIRECTORY_MONITOR))
#define G_DIRECTORY_MONITOR_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), G_TYPE_DIRECTORY_MONITOR, GDirectoryMonitorClass))

typedef struct _GDirectoryMonitorClass	 GDirectoryMonitorClass;
typedef struct _GDirectoryMonitorPrivate GDirectoryMonitorPrivate;

struct _GDirectoryMonitor
{
  GObject parent;

  /*< private >*/
  GDirectoryMonitorPrivate *priv;
};

struct _GDirectoryMonitorClass
{
  GObjectClass parent_class;
  
  /* Signals */
  void (* changed) (GDirectoryMonitor* monitor,
		    GFile *child,
		    GFile *other_file,
		    GFileMonitorEvent event_type);
  
  /* Virtual Table */
  gboolean	(*cancel)(GDirectoryMonitor* monitor);

  /* Padding for future expansion */
  void (*_g_reserved1) (void);
  void (*_g_reserved2) (void);
  void (*_g_reserved3) (void);
  void (*_g_reserved4) (void);
  void (*_g_reserved5) (void);
};

GType g_directory_monitor_get_type (void) G_GNUC_CONST;

gboolean g_directory_monitor_cancel         (GDirectoryMonitor *monitor);
gboolean g_directory_monitor_is_cancelled   (GDirectoryMonitor *monitor);
void     g_directory_monitor_set_rate_limit (GDirectoryMonitor *monitor,
					     int                limit_msecs);

/* For implementations */
void g_directory_monitor_emit_event (GDirectoryMonitor      *monitor,
				     GFile                  *child,
				     GFile                  *other_file,
				     GFileMonitorEvent       event_type);

G_END_DECLS

#endif /* __G_DIRECTORY_MONITOR_H__ */
