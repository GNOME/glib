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

#include "config.h"

#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "gmemorymonitordbus.h"
#include "gioerror.h"
#include "ginitable.h"
#include "giomodule-priv.h"
#include "glibintl.h"
#include "glib/gstdio.h"
#include "gmemorymonitor.h"
#include "gdbusproxy.h"

static void g_memory_monitor_dbus_iface_init (GMemoryMonitorInterface *iface);
static void g_memory_monitor_dbus_initable_iface_init (GInitableIface *iface);

struct _GMemoryMonitorDbusPrivate
{
  GDBusProxy *proxy;
};

#define g_memory_monitor_dbus_get_type _g_memory_monitor_dbus_get_type
G_DEFINE_TYPE_WITH_CODE (GMemoryMonitorDbus, g_memory_monitor_dbus, G_TYPE_MEMORY_MONITOR,
                         G_ADD_PRIVATE (GMemoryMonitorDbus)
                         G_IMPLEMENT_INTERFACE (G_TYPE_MEMORY_MONITOR,
                                                g_memory_monitor_dbus_iface_init)
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                g_memory_monitor_dbus_initable_iface_init)
                         _g_io_modules_ensure_extension_points_registered ();
                         g_io_extension_point_implement (G_MEMORY_MONITOR_EXTENSION_POINT_NAME,
                                                         g_define_type_id,
                                                         "dbus",
                                                         30))

static void
g_memory_monitor_dbus_init (GMemoryMonitorDbus *dbus)
{
  dbus->priv = g_memory_monitor_dbus_get_instance_private (dbus);
}

static void
proxy_signal_cb (GDBusProxy         *proxy,
                 gchar              *sender_name,
                 gchar              *signal_name,
                 GVariant           *parameters,
                 GMemoryMonitorDbus *dbus)
{
  if (g_strcmp0 (signal_name, "LowMemoryWarning") != 0)
    return;

  g_signal_emit_by_name (dbus, "low-memory-warning");
}

static gboolean
g_memory_monitor_dbus_initable_init (GInitable     *initable,
                                     GCancellable  *cancellable,
                                     GError       **error)
{
  GMemoryMonitorDbus *dbus = G_MEMORY_MONITOR_DBUS (initable);
  GDBusProxy *proxy;
  GInitableIface *parent_iface;
  gchar *name_owner = NULL;

  parent_iface = g_type_interface_peek_parent (G_MEMORY_MONITOR_DBUS_GET_INITABLE_IFACE (initable));
  if (!parent_iface->init (initable, cancellable, error))
    return FALSE;

  proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
                                         G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START | G_DBUS_PROXY_FLAGS_GET_INVALIDATED_PROPERTIES,
                                         NULL,
                                         "org.freedesktop.LowMemoryMonitor",
                                         "/org/freedesktop/LowMemoryMonitor",
                                         "org.freedesktop.LowMemoryMonitor",
                                         cancellable,
                                         error);
  if (!proxy)
    return FALSE;

  name_owner = g_dbus_proxy_get_name_owner (proxy);

  if (!name_owner)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   _("low-memory-monitor not running"));
      g_object_unref (proxy);
      return FALSE;
    }

  g_free (name_owner);

  g_signal_connect (G_OBJECT (proxy), "g-signal",
                    G_CALLBACK (proxy_signal_cb), dbus);
  dbus->priv->proxy = proxy;

  return TRUE;
}

static void
g_memory_monitor_dbus_finalize (GObject *object)
{
  GMemoryMonitorDbus *dbus = G_MEMORY_MONITOR_DBUS (object);

  g_clear_object (&dbus->priv->proxy);

  G_OBJECT_CLASS (g_memory_monitor_dbus_parent_class)->finalize (object);
}

static void
g_memory_monitor_dbus_class_init (GMemoryMonitorDbusClass *nl_class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (nl_class);

  gobject_class->finalize  = g_memory_monitor_dbus_finalize;
}

static void
g_memory_monitor_dbus_iface_init (GMemoryMonitorInterface *monitor_iface)
{
}

static void
g_memory_monitor_dbus_initable_iface_init (GInitableIface *iface)
{
  iface->init = g_memory_monitor_dbus_initable_init;
}
