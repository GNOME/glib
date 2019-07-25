/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright 2016 Red Hat, Inc.
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

#include "gmemorymonitorportal.h"
#include "ginitable.h"
#include "giomodule-priv.h"
#include "xdp-dbus.h"
#include "gportalsupport.h"

static GInitableIface *initable_parent_iface;
static void g_memory_monitor_portal_iface_init (GMemoryMonitorInterface *iface);
static void g_memory_monitor_portal_initable_iface_init (GInitableIface *iface);

struct _GMemoryMonitorPortalPrivate
{
  GDBusProxy *proxy;
};

#define g_memory_monitor_portal_get_type _g_memory_monitor_portal_get_type
G_DEFINE_TYPE_WITH_CODE (GMemoryMonitorPortal, g_memory_monitor_portal, G_TYPE_MEMORY_MONITOR,
                         G_ADD_PRIVATE (GMemoryMonitorPortal)
                         G_IMPLEMENT_INTERFACE (G_TYPE_MEMORY_MONITOR,
                                                g_memory_monitor_portal_iface_init)
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                g_memory_monitor_portal_initable_iface_init)
                         _g_io_modules_ensure_extension_points_registered ();
                         g_io_extension_point_implement (G_MEMORY_MONITOR_EXTENSION_POINT_NAME,
                                                         g_define_type_id,
                                                         "portal",
                                                         40))

static void
g_memory_monitor_portal_init (GMemoryMonitorPortal *nm)
{
  nm->priv = g_memory_monitor_portal_get_instance_private (nm);
}

static void
proxy_signal (GDBusProxy            *proxy,
              const char            *sender,
              const char            *signal,
              GVariant              *parameters,
              GMemoryMonitorPortal *nm)
{
  if (strcmp (signal, "low-memory-warning") != 0)
    return;

  g_signal_emit_by_name (nm, "low-memory-warning");
}

static gboolean
g_memory_monitor_portal_initable_init (GInitable     *initable,
                                        GCancellable  *cancellable,
                                        GError       **error)
{
  GMemoryMonitorPortal *nm = G_MEMORY_MONITOR_PORTAL (initable);
  GDBusProxy *proxy;
  gchar *name_owner = NULL;

  if (!glib_should_use_portal ())
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, "Not using portals");
      return FALSE;
    }

  proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                         G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES,
                                         NULL,
                                         "org.freedesktop.portal.Desktop",
                                         "/org/freedesktop/portal/desktop",
                                         "org.freedesktop.portal.MemoryMonitor",
                                         cancellable,
                                         error);
  if (!proxy)
    return FALSE;

  name_owner = g_dbus_proxy_get_name_owner (proxy);

  if (!name_owner)
    {
      g_object_unref (proxy);
      g_set_error (error,
                   G_DBUS_ERROR,
                   G_DBUS_ERROR_NAME_HAS_NO_OWNER,
                   "Desktop portal not found");
      return FALSE;
    }

  g_free (name_owner);

  g_signal_connect (proxy, "g-signal", G_CALLBACK (proxy_signal), nm);

  nm->priv->proxy = proxy;

  if (!initable_parent_iface->init (initable, cancellable, error))
    return FALSE;

  return TRUE;
}

static void
g_memory_monitor_portal_finalize (GObject *object)
{
  GMemoryMonitorPortal *nm = G_MEMORY_MONITOR_PORTAL (object);

  g_clear_object (&nm->priv->proxy);

  G_OBJECT_CLASS (g_memory_monitor_portal_parent_class)->finalize (object);
}

static void
g_memory_monitor_portal_class_init (GMemoryMonitorPortalClass *nl_class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (nl_class);

  gobject_class->finalize  = g_memory_monitor_portal_finalize;
}

static void
g_memory_monitor_portal_iface_init (GMemoryMonitorInterface *monitor_iface)
{
}

static void
g_memory_monitor_portal_initable_iface_init (GInitableIface *iface)
{
  initable_parent_iface = g_type_interface_peek_parent (iface);

  iface->init = g_memory_monitor_portal_initable_init;
}
