/* GDBus - GLib D-Bus Library
 *
 * Copyright (C) 2008-2010 Red Hat, Inc.
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
 * Author: David Zeuthen <davidz@redhat.com>
 */

#include "config.h"

#include "gdbusobject.h"
#include "gdbusobjectproxy.h"
#include "gdbusconnection.h"
#include "gdbusprivate.h"
#include "gdbusutils.h"
#include "gdbusproxy.h"

#include "glibintl.h"

/**
 * SECTION:gdbusobjectproxy
 * @short_description: Client-side D-Bus object
 * @include: gio/gio.h
 *
 * A #GDBusObjectProxy is an object used to represent a remote object
 * with one or more D-Bus interfaces. Normally, you don't instantiate
 * a #GDBusObjectProxy yourself - typically #GDBusObjectManagerClient
 * is used to obtain it.
 *
 * Since: 2.30
 */

struct _GDBusObjectProxyPrivate
{
  GHashTable *map_name_to_iface;
  gchar *object_path;
  GDBusConnection *connection;
};

enum
{
  PROP_0,
  PROP_OBJECT_PATH,
  PROP_CONNECTION
};

static void dbus_object_interface_init (GDBusObjectIface *iface);

G_DEFINE_TYPE_WITH_CODE (GDBusObjectProxy, g_dbus_object_proxy, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_DBUS_OBJECT, dbus_object_interface_init));

static void
g_dbus_object_proxy_finalize (GObject *object)
{
  GDBusObjectProxy *proxy = G_DBUS_OBJECT_PROXY (object);

  g_hash_table_unref (proxy->priv->map_name_to_iface);

  if (G_OBJECT_CLASS (g_dbus_object_proxy_parent_class)->finalize != NULL)
    G_OBJECT_CLASS (g_dbus_object_proxy_parent_class)->finalize (object);
}

static void
g_dbus_object_proxy_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  GDBusObjectProxy *proxy = G_DBUS_OBJECT_PROXY (object);

  switch (prop_id)
    {
    case PROP_OBJECT_PATH:
      g_value_set_string (value, proxy->priv->object_path);
      break;

    case PROP_CONNECTION:
      g_value_set_object (value, g_dbus_object_proxy_get_connection (proxy));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (_object, prop_id, pspec);
      break;
    }
}

static void
g_dbus_object_proxy_set_property (GObject       *object,
                                  guint          prop_id,
                                  const GValue  *value,
                                  GParamSpec    *pspec)
{
  GDBusObjectProxy *proxy = G_DBUS_OBJECT_PROXY (object);

  switch (prop_id)
    {
    case PROP_OBJECT_PATH:
      proxy->priv->object_path = g_value_dup_string (value);
      break;

    case PROP_CONNECTION:
      proxy->priv->connection = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (_object, prop_id, pspec);
      break;
    }
}

static void
g_dbus_object_proxy_class_init (GDBusObjectProxyClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize     = g_dbus_object_proxy_finalize;
  gobject_class->set_property = g_dbus_object_proxy_set_property;
  gobject_class->get_property = g_dbus_object_proxy_get_property;

  /**
   * GDBusObjectProxy:object-path:
   *
   * The object path of the proxy.
   *
   * Since: 2.30
   */
  g_object_class_install_property (gobject_class,
                                   PROP_OBJECT_PATH,
                                   g_param_spec_string ("object-path",
                                                        "Object Path",
                                                        "The object path of the proxy",
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));

  /**
   * GDBusObjectProxy:connection:
   *
   * The connection of the proxy.
   *
   * Since: 2.30
   */
  g_object_class_install_property (gobject_class,
                                   PROP_CONNECTION,
                                   g_param_spec_object ("connection",
                                                        "Connection",
                                                        "The connection of the proxy",
                                                        G_TYPE_DBUS_CONNECTION,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));

  g_type_class_add_private (klass, sizeof (GDBusObjectProxyPrivate));
}

static void
g_dbus_object_proxy_init (GDBusObjectProxy *proxy)
{
  proxy->priv = G_TYPE_INSTANCE_GET_PRIVATE (proxy,
                                             G_TYPE_DBUS_OBJECT_PROXY,
                                             GDBusObjectProxyPrivate);
  proxy->priv->map_name_to_iface = g_hash_table_new_full (g_str_hash,
                                                          g_str_equal,
                                                          g_free,
                                                          (GDestroyNotify) g_object_unref);
}

static const gchar *
g_dbus_object_proxy_get_object_path (GDBusObject *object)
{
  GDBusObjectProxy *proxy = G_DBUS_OBJECT_PROXY (object);
  return proxy->priv->object_path;
}

/**
 * g_dbus_object_proxy_get_connection:
 * @proxy: a #GDBusObjectProxy
 *
 * Gets the connection that @proxy is for.
 *
 * Returns: (transfer none): A #GDBusConnection. Do not free, the
 *   object is owned by @proxy.
 *
 * Since: 2.30
 */
GDBusConnection *
g_dbus_object_proxy_get_connection (GDBusObjectProxy *proxy)
{
  g_return_val_if_fail (G_IS_DBUS_OBJECT_PROXY (proxy), NULL);
  return proxy->priv->connection;
}

static GDBusInterface *
g_dbus_object_proxy_get_interface (GDBusObject *object,
                                   const gchar *interface_name)
{
  GDBusObjectProxy *proxy = G_DBUS_OBJECT_PROXY (object);
  GDBusProxy *ret;

  g_return_val_if_fail (G_IS_DBUS_OBJECT_PROXY (proxy), NULL);
  g_return_val_if_fail (g_dbus_is_interface_name (interface_name), NULL);

  ret = g_hash_table_lookup (proxy->priv->map_name_to_iface, interface_name);
  if (ret != NULL)
    g_object_ref (ret);
  return (GDBusInterface *) ret; /* TODO: proper cast */
}

static GList *
g_dbus_object_proxy_get_interfaces (GDBusObject *object)
{
  GDBusObjectProxy *proxy = G_DBUS_OBJECT_PROXY (object);
  GList *ret;
  GHashTableIter iter;
  GDBusProxy *interface_proxy;

  g_return_val_if_fail (G_IS_DBUS_OBJECT_PROXY (proxy), NULL);

  ret = NULL;

  g_hash_table_iter_init (&iter, proxy->priv->map_name_to_iface);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer) &interface_proxy))
    ret = g_list_prepend (ret, g_object_ref (interface_proxy));

  return ret;
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_dbus_object_proxy_new:
 * @connection: a #GDBusConnection
 * @object_path: the object path
 *
 * Creates a new #GDBusObjectProxy for the given connection and
 * object path.
 *
 * Returns: a new #GDBusObjectProxy
 *
 * Since: 2.30
 */
GDBusObjectProxy *
g_dbus_object_proxy_new (GDBusConnection *connection,
                         const gchar     *object_path)
{
  g_return_val_if_fail (G_IS_DBUS_CONNECTION (connection), NULL);
  g_return_val_if_fail (g_variant_is_object_path (object_path), NULL);
  return G_DBUS_OBJECT_PROXY (g_object_new (G_TYPE_DBUS_OBJECT_PROXY,
                                            "object-path", object_path,
                                            "connection", connection,
                                            NULL));
}

void
_g_dbus_object_proxy_add_interface (GDBusObjectProxy *proxy,
                                    GDBusProxy       *interface_proxy)
{
  const gchar *interface_name;

  g_return_if_fail (G_IS_DBUS_OBJECT_PROXY (proxy));
  g_return_if_fail (G_IS_DBUS_PROXY (interface_proxy));

  interface_name = g_dbus_proxy_get_interface_name (interface_proxy);
  _g_dbus_object_proxy_remove_interface (proxy, interface_name);
  g_hash_table_insert (proxy->priv->map_name_to_iface,
                       g_strdup (interface_name),
                       g_object_ref (interface_proxy));
  g_signal_emit_by_name (proxy, "interface-added", interface_proxy);
}

void
_g_dbus_object_proxy_remove_interface (GDBusObjectProxy *proxy,
                                       const gchar      *interface_name)
{
  GDBusProxy *interface_proxy;

  g_return_if_fail (G_IS_DBUS_OBJECT_PROXY (proxy));
  g_return_if_fail (g_dbus_is_interface_name (interface_name));

  interface_proxy = g_hash_table_lookup (proxy->priv->map_name_to_iface, interface_name);
  if (interface_proxy != NULL)
    {
      g_object_ref (interface_proxy);
      g_warn_if_fail (g_hash_table_remove (proxy->priv->map_name_to_iface, interface_name));
      g_signal_emit_by_name (proxy, "interface-removed", interface_proxy);
      g_object_unref (interface_proxy);
    }
}

static void
dbus_object_interface_init (GDBusObjectIface *iface)
{
  iface->get_object_path       = g_dbus_object_proxy_get_object_path;
  iface->get_interfaces        = g_dbus_object_proxy_get_interfaces;
  iface->get_interface         = g_dbus_object_proxy_get_interface;
}
