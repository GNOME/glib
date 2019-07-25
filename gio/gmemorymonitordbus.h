/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright 2019 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __G_MEMORY_MONITOR_DBUS_H__
#define __G_MEMORY_MONITOR_DBUS_H__

#include <gmemorymonitor.h>

G_BEGIN_DECLS

#define G_TYPE_MEMORY_MONITOR_DBUS         (_g_memory_monitor_dbus_get_type ())
#define G_MEMORY_MONITOR_DBUS(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), G_TYPE_MEMORY_MONITOR_DBUS, GMemoryMonitorDbus))
#define G_MEMORY_MONITOR_DBUS_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), G_TYPE_MEMORY_MONITOR_DBUS, GMemoryMonitorDbusClass))
#define G_IS_MEMORY_MONITOR_DBUS(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), G_TYPE_MEMORY_MONITOR_DBUS))
#define G_IS_MEMORY_MONITOR_DBUS_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), G_TYPE_MEMORY_MONITOR_DBUS))
#define G_MEMORY_MONITOR_DBUS_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), G_TYPE_MEMORY_MONITOR_DBUS, GMemoryMonitorDbusClass))
#define G_MEMORY_MONITOR_DBUS_GET_INITABLE_IFACE(o) (G_TYPE_INSTANCE_GET_INTERFACE ((o), G_TYPE_INITABLE, GInitable))


typedef struct _GMemoryMonitorDbus        GMemoryMonitorDbus;
typedef struct _GMemoryMonitorDbusClass   GMemoryMonitorDbusClass;
typedef struct _GMemoryMonitorDbusPrivate GMemoryMonitorDbusPrivate;

struct _GMemoryMonitorDbus {
  GObject parent_instance;

  GMemoryMonitorDbusPrivate *priv;
};

struct _GMemoryMonitorDbusClass {
  GObjectClass parent_class;
};

GType _g_memory_monitor_dbus_get_type (void);

G_END_DECLS

#endif /* __G_MEMORY_MONITOR_DBUS_H__ */
