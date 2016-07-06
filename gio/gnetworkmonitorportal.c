/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright 2016 Red Hat, Inc.
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
 */

#include "config.h"

#include "gnetworkmonitorportal.h"
#include "ginitable.h"
#include "giomodule-priv.h"
#include "gnetworkmonitor.h"
#include "xdp-dbus.h"
#include "gportalsupport.h"


static void g_network_monitor_portal_iface_init (GNetworkMonitorInterface *iface);
static void g_network_monitor_portal_initable_iface_init (GInitableIface *iface);

enum
{
  PROP_0,
  PROP_NETWORK_AVAILABLE,
  PROP_NETWORK_METERED,
  PROP_CONNECTIVITY
};

struct _GNetworkMonitorPortalPrivate
{
  GXdpNetworkMonitor *proxy;
  gboolean network_available;
};

G_DEFINE_TYPE_WITH_CODE (GNetworkMonitorPortal, g_network_monitor_portal, G_TYPE_NETWORK_MONITOR_BASE,
                         G_ADD_PRIVATE (GNetworkMonitorPortal)
                         G_IMPLEMENT_INTERFACE (G_TYPE_NETWORK_MONITOR,
                                                g_network_monitor_portal_iface_init)
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                g_network_monitor_portal_initable_iface_init)
                         _g_io_modules_ensure_extension_points_registered ();
                         g_io_extension_point_implement (G_NETWORK_MONITOR_EXTENSION_POINT_NAME,
                                                         g_define_type_id,
                                                         "portal",
                                                         30))

static void
g_network_monitor_portal_init (GNetworkMonitorPortal *nm)
{
  nm->priv = g_network_monitor_portal_get_instance_private (nm);
}

static void
g_network_monitor_portal_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  GNetworkMonitorPortal *nm = G_NETWORK_MONITOR_PORTAL (object);

  switch (prop_id)
    {
    case PROP_NETWORK_AVAILABLE:
      g_value_set_boolean (value,
                           nm->priv->network_available &&
                           gxdp_network_monitor_get_available (nm->priv->proxy));
      break;

    case PROP_NETWORK_METERED:
      g_value_set_boolean (value,
                           nm->priv->network_available &&
                           gxdp_network_monitor_get_metered (nm->priv->proxy));
      break;

    case PROP_CONNECTIVITY:
      g_value_set_enum (value,
                        nm->priv->network_available
                        ? gxdp_network_monitor_get_connectivity (nm->priv->proxy)
                        : G_NETWORK_CONNECTIVITY_LOCAL);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
proxy_changed (GXdpNetworkMonitor    *proxy,
               gboolean               available,
               GNetworkMonitorPortal *nm)
{
  if (nm->priv->network_available)
    g_signal_emit_by_name (nm, "network-changed", available);
}

static gboolean
g_network_monitor_portal_initable_init (GInitable     *initable,
                                        GCancellable  *cancellable,
                                        GError       **error)
{
  GNetworkMonitorPortal *nm = G_NETWORK_MONITOR_PORTAL (initable);
  GXdpNetworkMonitor *proxy;
  gchar *name_owner = NULL;

  if (!glib_should_use_portal ())
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, "Not using portals");
      return FALSE;
    }

  proxy = gxdp_network_monitor_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                                       G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START
                                                       | G_DBUS_PROXY_FLAGS_GET_INVALIDATED_PROPERTIES,
                                                       "org.freedesktop.portal.Desktop",
                                                       "/org/freedesktop/portal/desktop",
                                                       cancellable,
                                                       error);
  if (!proxy)
    return FALSE;

  name_owner = g_dbus_proxy_get_name_owner (G_DBUS_PROXY (proxy));

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

  g_signal_connect (proxy, "changed", G_CALLBACK (proxy_changed), nm);
  nm->priv->proxy = proxy;
  nm->priv->network_available = glib_network_available_in_sandbox ();

  return TRUE;
}

static void
g_network_monitor_portal_finalize (GObject *object)
{
  GNetworkMonitorPortal *nm = G_NETWORK_MONITOR_PORTAL (object);

  g_clear_object (&nm->priv->proxy);

  G_OBJECT_CLASS (g_network_monitor_portal_parent_class)->finalize (object);
}

static void
g_network_monitor_portal_class_init (GNetworkMonitorPortalClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  gobject_class->finalize  = g_network_monitor_portal_finalize;
  gobject_class->get_property = g_network_monitor_portal_get_property;

  g_object_class_override_property (gobject_class, PROP_NETWORK_AVAILABLE, "network-available");
  g_object_class_override_property (gobject_class, PROP_NETWORK_METERED, "network-metered");
  g_object_class_override_property (gobject_class, PROP_CONNECTIVITY, "connectivity");
}

static void
g_network_monitor_portal_iface_init (GNetworkMonitorInterface *monitor_iface)
{
}

static void
g_network_monitor_portal_initable_iface_init (GInitableIface *iface)
{
  iface->init = g_network_monitor_portal_initable_init;
}
