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

#include <stdlib.h>
#include <string.h>

#include <gobject/gvaluecollector.h>

#include "gdbusutils.h"
#include "gdbusproxy.h"
#include "gioenumtypes.h"
#include "gdbusconnection.h"
#include "gdbuserror.h"
#include "gdbusprivate.h"
#include "gio-marshal.h"
#include "ginitable.h"
#include "gasyncinitable.h"
#include "gioerror.h"
#include "gasyncresult.h"
#include "gsimpleasyncresult.h"

#include "glibintl.h"
#include "gioalias.h"

/**
 * SECTION:gdbusproxy
 * @short_description: Base class for proxies
 * @include: gio/gio.h
 *
 * #GDBusProxy is a base class used for proxies to access a D-Bus
 * interface on a remote object. A #GDBusProxy can only be constructed
 * for unique name bus and does not track whether the name
 * vanishes. Use g_bus_watch_proxy() to construct #GDBusProxy proxies
 * for owners of a well-known names.
 *
 * By default, #GDBusProxy will cache all properties (and listen for
 * their changes) of the remote object, and proxy all signals that gets
 * emitted. This behaviour can be changed by passing suitable
 * #GDBusProxyFlags when the proxy is created.
 *
 * The generic #GDBusProxy::g-properties-changed and #GDBusProxy::g-signal
 * signals are not very convenient to work with. Therefore, the recommended
 * way of working with proxies is to subclass #GDBusProxy, and have
 * more natural properties and signals in your derived class. The
 * @interface_type argument of g_bus_watch_proxy() lets you obtain
 * instances of your derived class when using the high-level API.
 *
 * See <xref linkend="gdbus-example-proxy-subclass"/> for an example.
 */

struct _GDBusProxyPrivate
{
  GDBusConnection *connection;
  GDBusProxyFlags flags;
  gchar *unique_bus_name;
  gchar *object_path;
  gchar *interface_name;
  gint timeout_msec;

  /* gchar* -> GVariant* */
  GHashTable *properties;

  GDBusInterfaceInfo *expected_interface;

  guint properties_changed_subscriber_id;
  guint signals_subscriber_id;

  gboolean initialized;
};

enum
{
  PROP_0,
  PROP_G_CONNECTION,
  PROP_G_UNIQUE_BUS_NAME,
  PROP_G_FLAGS,
  PROP_G_OBJECT_PATH,
  PROP_G_INTERFACE_NAME,
  PROP_G_DEFAULT_TIMEOUT,
  PROP_G_INTERFACE_INFO
};

enum
{
  PROPERTIES_CHANGED_SIGNAL,
  SIGNAL_SIGNAL,
  LAST_SIGNAL,
};

static void g_dbus_proxy_constructed (GObject *object);

guint signals[LAST_SIGNAL] = {0};

static void initable_iface_init       (GInitableIface *initable_iface);
static void async_initable_iface_init (GAsyncInitableIface *async_initable_iface);

G_DEFINE_TYPE_WITH_CODE (GDBusProxy, g_dbus_proxy, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, initable_iface_init)
                         G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE, async_initable_iface_init)
                         );

static void
g_dbus_proxy_finalize (GObject *object)
{
  GDBusProxy *proxy = G_DBUS_PROXY (object);

  if (proxy->priv->properties_changed_subscriber_id > 0)
    g_dbus_connection_signal_unsubscribe (proxy->priv->connection,
                                          proxy->priv->properties_changed_subscriber_id);

  if (proxy->priv->signals_subscriber_id > 0)
    g_dbus_connection_signal_unsubscribe (proxy->priv->connection,
                                          proxy->priv->signals_subscriber_id);

  g_object_unref (proxy->priv->connection);
  g_free (proxy->priv->unique_bus_name);
  g_free (proxy->priv->object_path);
  g_free (proxy->priv->interface_name);
  if (proxy->priv->properties != NULL)
    g_hash_table_unref (proxy->priv->properties);

  if (proxy->priv->expected_interface != NULL)
    g_dbus_interface_info_unref (proxy->priv->expected_interface);

  G_OBJECT_CLASS (g_dbus_proxy_parent_class)->finalize (object);
}

static void
g_dbus_proxy_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  GDBusProxy *proxy = G_DBUS_PROXY (object);

  switch (prop_id)
    {
    case PROP_G_CONNECTION:
      g_value_set_object (value, proxy->priv->connection);
      break;

    case PROP_G_FLAGS:
      g_value_set_flags (value, proxy->priv->flags);
      break;

    case PROP_G_UNIQUE_BUS_NAME:
      g_value_set_string (value, proxy->priv->unique_bus_name);
      break;

    case PROP_G_OBJECT_PATH:
      g_value_set_string (value, proxy->priv->object_path);
      break;

    case PROP_G_INTERFACE_NAME:
      g_value_set_string (value, proxy->priv->interface_name);
      break;

    case PROP_G_DEFAULT_TIMEOUT:
      g_value_set_int (value, proxy->priv->timeout_msec);
      break;

    case PROP_G_INTERFACE_INFO:
      g_value_set_boxed (value, g_dbus_proxy_get_interface_info (proxy));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
g_dbus_proxy_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  GDBusProxy *proxy = G_DBUS_PROXY (object);

  switch (prop_id)
    {
    case PROP_G_CONNECTION:
      proxy->priv->connection = g_value_dup_object (value);
      break;

    case PROP_G_FLAGS:
      proxy->priv->flags = g_value_get_flags (value);
      break;

    case PROP_G_UNIQUE_BUS_NAME:
      proxy->priv->unique_bus_name = g_value_dup_string (value);
      break;

    case PROP_G_OBJECT_PATH:
      proxy->priv->object_path = g_value_dup_string (value);
      break;

    case PROP_G_INTERFACE_NAME:
      proxy->priv->interface_name = g_value_dup_string (value);
      break;

    case PROP_G_DEFAULT_TIMEOUT:
      g_dbus_proxy_set_default_timeout (proxy, g_value_get_int (value));
      break;

    case PROP_G_INTERFACE_INFO:
      g_dbus_proxy_set_interface_info (proxy, g_value_get_boxed (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
g_dbus_proxy_class_init (GDBusProxyClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize     = g_dbus_proxy_finalize;
  gobject_class->set_property = g_dbus_proxy_set_property;
  gobject_class->get_property = g_dbus_proxy_get_property;
  gobject_class->constructed  = g_dbus_proxy_constructed;

  /* Note that all property names are prefixed to avoid collisions with D-Bus property names
   * in derived classes */

  /**
   * GDBusProxy:g-interface-info:
   *
   * Ensure that interactions with this proxy conform to the given
   * interface.  For example, when completing a method call, if the
   * type signature of the message isn't what's expected, the given
   * #GError is set.  Signals that have a type signature mismatch are
   * simply dropped.
   *
   * Since: 2.26
   */
  g_object_class_install_property (gobject_class,
                                   PROP_G_INTERFACE_INFO,
                                   g_param_spec_boxed ("g-interface-info",
                                                       P_("Interface Information"),
                                                       P_("Interface Information"),
                                                       G_TYPE_DBUS_INTERFACE_INFO,
                                                       G_PARAM_READABLE |
                                                       G_PARAM_WRITABLE |
                                                       G_PARAM_STATIC_NAME |
                                                       G_PARAM_STATIC_BLURB |
                                                       G_PARAM_STATIC_NICK));

  /**
   * GDBusProxy:g-connection:
   *
   * The #GDBusConnection the proxy is for.
   *
   * Since: 2.26
   */
  g_object_class_install_property (gobject_class,
                                   PROP_G_CONNECTION,
                                   g_param_spec_object ("g-connection",
                                                        P_("g-connection"),
                                                        P_("The connection the proxy is for"),
                                                        G_TYPE_DBUS_CONNECTION,
                                                        G_PARAM_READABLE |
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_BLURB |
                                                        G_PARAM_STATIC_NICK));

  /**
   * GDBusProxy:g-flags:
   *
   * Flags from the #GDBusProxyFlags enumeration.
   *
   * Since: 2.26
   */
  g_object_class_install_property (gobject_class,
                                   PROP_G_FLAGS,
                                   g_param_spec_flags ("g-flags",
                                                       P_("g-flags"),
                                                       P_("Flags for the proxy"),
                                                       G_TYPE_DBUS_PROXY_FLAGS,
                                                       G_DBUS_PROXY_FLAGS_NONE,
                                                       G_PARAM_READABLE |
                                                       G_PARAM_WRITABLE |
                                                       G_PARAM_CONSTRUCT_ONLY |
                                                       G_PARAM_STATIC_NAME |
                                                       G_PARAM_STATIC_BLURB |
                                                       G_PARAM_STATIC_NICK));

  /**
   * GDBusProxy:g-unique-bus-name:
   *
   * The unique bus name the proxy is for.
   *
   * Since: 2.26
   */
  g_object_class_install_property (gobject_class,
                                   PROP_G_UNIQUE_BUS_NAME,
                                   g_param_spec_string ("g-unique-bus-name",
                                                        P_("g-unique-bus-name"),
                                                        P_("The unique bus name the proxy is for"),
                                                        NULL,
                                                        G_PARAM_READABLE |
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_BLURB |
                                                        G_PARAM_STATIC_NICK));

  /**
   * GDBusProxy:g-object-path:
   *
   * The object path the proxy is for.
   *
   * Since: 2.26
   */
  g_object_class_install_property (gobject_class,
                                   PROP_G_OBJECT_PATH,
                                   g_param_spec_string ("g-object-path",
                                                        P_("g-object-path"),
                                                        P_("The object path the proxy is for"),
                                                        NULL,
                                                        G_PARAM_READABLE |
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_BLURB |
                                                        G_PARAM_STATIC_NICK));

  /**
   * GDBusProxy:g-interface-name:
   *
   * The D-Bus interface name the proxy is for.
   *
   * Since: 2.26
   */
  g_object_class_install_property (gobject_class,
                                   PROP_G_INTERFACE_NAME,
                                   g_param_spec_string ("g-interface-name",
                                                        P_("g-interface-name"),
                                                        P_("The D-Bus interface name the proxy is for"),
                                                        NULL,
                                                        G_PARAM_READABLE |
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_BLURB |
                                                        G_PARAM_STATIC_NICK));

  /**
   * GDBusProxy:g-default-timeout:
   *
   * The timeout to use if -1 (specifying default timeout) is passed
   * as @timeout_msec in the g_dbus_proxy_call() and
   * g_dbus_proxy_call_sync() functions.
   *
   * This allows applications to set a proxy-wide timeout for all
   * remote method invocations on the proxy. If this property is -1,
   * the default timeout (typically 25 seconds) is used. If set to
   * %G_MAXINT, then no timeout is used.
   *
   * Since: 2.26
   */
  g_object_class_install_property (gobject_class,
                                   PROP_G_DEFAULT_TIMEOUT,
                                   g_param_spec_int ("g-default-timeout",
                                                     P_("Default Timeout"),
                                                     P_("Timeout for remote method invocation"),
                                                     -1,
                                                     G_MAXINT,
                                                     -1,
                                                     G_PARAM_READABLE |
                                                     G_PARAM_WRITABLE |
                                                     G_PARAM_CONSTRUCT |
                                                     G_PARAM_STATIC_NAME |
                                                     G_PARAM_STATIC_BLURB |
                                                     G_PARAM_STATIC_NICK));

  /**
   * GDBusProxy::g-properties-changed:
   * @proxy: The #GDBusProxy emitting the signal.
   * @changed_properties: A #GVariant containing the properties that changed
   * @invalidated_properties: A %NULL terminated array of properties that was invalidated
   *
   * Emitted when one or more D-Bus properties on @proxy changes. The
   * local cache has already been updated when this signal fires. Note
   * that both @changed_properties and @invalidated_properties are
   * guaranteed to never be %NULL (either may be empty though).
   *
   * This signal corresponds to the
   * <literal>PropertiesChanged</literal> D-Bus signal on the
   * <literal>org.freedesktop.DBus.Properties</literal> interface.
   *
   * Since: 2.26
   */
  signals[PROPERTIES_CHANGED_SIGNAL] = g_signal_new ("g-properties-changed",
                                                     G_TYPE_DBUS_PROXY,
                                                     G_SIGNAL_RUN_LAST,
                                                     G_STRUCT_OFFSET (GDBusProxyClass, g_properties_changed),
                                                     NULL,
                                                     NULL,
                                                     _gio_marshal_VOID__BOXED_BOXED,
                                                     G_TYPE_NONE,
                                                     2,
                                                     G_TYPE_VARIANT,
                                                     G_TYPE_STRV | G_SIGNAL_TYPE_STATIC_SCOPE);

  /**
   * GDBusProxy::g-signal:
   * @proxy: The #GDBusProxy emitting the signal.
   * @sender_name: The sender of the signal or %NULL if the connection is not a bus connection.
   * @signal_name: The name of the signal.
   * @parameters: A #GVariant tuple with parameters for the signal.
   *
   * Emitted when a signal from the remote object and interface that @proxy is for, has been received.
   *
   * Since: 2.26
   */
  signals[SIGNAL_SIGNAL] = g_signal_new ("g-signal",
                                         G_TYPE_DBUS_PROXY,
                                         G_SIGNAL_RUN_LAST,
                                         G_STRUCT_OFFSET (GDBusProxyClass, g_signal),
                                         NULL,
                                         NULL,
                                         _gio_marshal_VOID__STRING_STRING_BOXED,
                                         G_TYPE_NONE,
                                         3,
                                         G_TYPE_STRING,
                                         G_TYPE_STRING,
                                         G_TYPE_VARIANT);


  g_type_class_add_private (klass, sizeof (GDBusProxyPrivate));
}

static void
g_dbus_proxy_init (GDBusProxy *proxy)
{
  proxy->priv = G_TYPE_INSTANCE_GET_PRIVATE (proxy, G_TYPE_DBUS_PROXY, GDBusProxyPrivate);
  proxy->priv->properties = g_hash_table_new_full (g_str_hash,
                                                   g_str_equal,
                                                   g_free,
                                                   (GDestroyNotify) g_variant_unref);
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_dbus_proxy_get_cached_property_names:
 * @proxy: A #GDBusProxy.
 *
 * Gets the names of all cached properties on @proxy.
 *
 * Returns: A %NULL-terminated array of strings or %NULL if @proxy has
 * no cached properties. Free the returned array with g_strfreev().
 *
 * Since: 2.26
 */
gchar **
g_dbus_proxy_get_cached_property_names (GDBusProxy  *proxy)
{
  gchar **names;
  GPtrArray *p;
  GHashTableIter iter;
  const gchar *key;

  g_return_val_if_fail (G_IS_DBUS_PROXY (proxy), NULL);

  names = NULL;
  if (g_hash_table_size (proxy->priv->properties) == 0)
    goto out;

  p = g_ptr_array_new ();

  g_hash_table_iter_init (&iter, proxy->priv->properties);
  while (g_hash_table_iter_next (&iter, (gpointer) &key, NULL))
    g_ptr_array_add (p, g_strdup (key));
  g_ptr_array_sort (p, (GCompareFunc) g_strcmp0);
  g_ptr_array_add (p, NULL);

  names = (gchar **) g_ptr_array_free (p, FALSE);

 out:
  return names;
}

static const GDBusPropertyInfo *
lookup_property_info_or_warn (GDBusProxy  *proxy,
                              const gchar *property_name)
{
  const GDBusPropertyInfo *info;

  if (proxy->priv->expected_interface == NULL)
    return NULL;

  info = g_dbus_interface_info_lookup_property (proxy->priv->expected_interface, property_name);
  if (info == NULL)
    {
      g_warning ("Trying to lookup property %s which isn't in expected interface %s",
                 property_name,
                 proxy->priv->expected_interface->name);
    }

  return info;
}

/**
 * g_dbus_proxy_get_cached_property:
 * @proxy: A #GDBusProxy.
 * @property_name: Property name.
 *
 * Looks up the value for a property from the cache. This call does no
 * blocking IO.
 *
 * If @proxy has an expected interface (see
 * #GDBusProxy:g-interface-info), then @property_name (for existence)
 * is checked against it.
 *
 * Returns: A reference to the #GVariant instance that holds the value
 * for @property_name or %NULL if the value is not in the cache. The
 * returned reference must be freed with g_variant_unref().
 *
 * Since: 2.26
 */
GVariant *
g_dbus_proxy_get_cached_property (GDBusProxy   *proxy,
                                  const gchar  *property_name)
{
  GVariant *value;

  g_return_val_if_fail (G_IS_DBUS_PROXY (proxy), NULL);
  g_return_val_if_fail (property_name != NULL, NULL);

  value = g_hash_table_lookup (proxy->priv->properties, property_name);
  if (value == NULL)
    {
      const GDBusPropertyInfo *info;
      info = lookup_property_info_or_warn (proxy, property_name);
      /* no difference */
      goto out;
    }

  g_variant_ref (value);

 out:
  return value;
}

/**
 * g_dbus_proxy_set_cached_property:
 * @proxy: A #GDBusProxy
 * @property_name: Property name.
 * @value: Value for the property or %NULL to remove it from the cache.
 *
 * If @value is not %NULL, sets the cached value for the property with
 * name @property_name to the value in @value.
 *
 * If @value is %NULL, then the cached value is removed from the
 * property cache.
 *
 * If @proxy has an expected interface (see
 * #GDBusProxy:g-interface-info), then @property_name (for existence)
 * and @value (for the type) is checked against it.
 *
 * If the @value #GVariant is floating, it is consumed. This allows
 * convenient 'inline' use of g_variant_new(), e.g.
 * |[
 *  g_dbus_proxy_set_cached_property (proxy,
 *                                    "SomeProperty",
 *                                    g_variant_new ("(si)",
 *                                                  "A String",
 *                                                  42));
 * ]|
 *
 * Normally you will not need to use this method since @proxy is
 * tracking changes using the
 * <literal>org.freedesktop.DBus.Properties.PropertiesChanged</literal>
 * D-Bus signal. However, for performance reasons an object may decide
 * to not use this signal for some properties and instead use a
 * proprietary out-of-band mechanism to transmit changes.
 *
 * As a concrete example, consider an object with a property
 * <literal>ChatroomParticipants</literal> which is an array of
 * strings. Instead of transmitting the same (long) array every time
 * the property changes, it is more efficient to only transmit the
 * delta using e.g. signals <literal>ChatroomParticipantJoined(String
 * name)</literal> and <literal>ChatroomParticipantParted(String
 * name)</literal>.
 *
 * Since: 2.26
 */
void
g_dbus_proxy_set_cached_property (GDBusProxy   *proxy,
                                  const gchar  *property_name,
                                  GVariant     *value)
{
  const GDBusPropertyInfo *info;

  g_return_if_fail (G_IS_DBUS_PROXY (proxy));
  g_return_if_fail (property_name != NULL);

  if (value != NULL)
    {
      info = lookup_property_info_or_warn (proxy, property_name);
      if (info != NULL)
        {
          if (g_strcmp0 (info->signature, g_variant_get_type_string (value)) != 0)
            {
              g_warning (_("Trying to set property %s of type %s but according to the expected "
                           "interface the type is %s"),
                         property_name,
                         g_variant_get_type_string (value),
                         info->signature);
              goto out;
            }
        }
      g_hash_table_insert (proxy->priv->properties,
                           g_strdup (property_name),
                           g_variant_ref_sink (value));
    }
  else
    {
      g_hash_table_remove (proxy->priv->properties, property_name);
    }

 out:
  ;
}

/* ---------------------------------------------------------------------------------------------------- */

static void
on_signal_received (GDBusConnection *connection,
                    const gchar     *sender_name,
                    const gchar     *object_path,
                    const gchar     *interface_name,
                    const gchar     *signal_name,
                    GVariant        *parameters,
                    gpointer         user_data)
{
  GDBusProxy *proxy = G_DBUS_PROXY (user_data);

  if (!proxy->priv->initialized)
    goto out;

  g_signal_emit (proxy,
                 signals[SIGNAL_SIGNAL],
                 0,
                 sender_name,
                 signal_name,
                 parameters);
 out:
  ;
}

/* ---------------------------------------------------------------------------------------------------- */

static void
on_properties_changed (GDBusConnection *connection,
                       const gchar     *sender_name,
                       const gchar     *object_path,
                       const gchar     *interface_name,
                       const gchar     *signal_name,
                       GVariant        *parameters,
                       gpointer         user_data)
{
  GDBusProxy *proxy = G_DBUS_PROXY (user_data);
  GError *error;
  const gchar *interface_name_for_signal;
  GVariant *changed_properties;
  gchar **invalidated_properties;
  GVariantIter iter;
  gchar *key;
  GVariant *value;
  guint n;

  error = NULL;
  changed_properties = NULL;
  invalidated_properties = NULL;

  if (!proxy->priv->initialized)
    goto out;

  if (!g_variant_is_of_type (parameters, G_VARIANT_TYPE ("(sa{sv}as)")))
    {
      g_warning ("Value for PropertiesChanged signal with type `%s' does not match `(sa{sv}as)'",
                 g_variant_get_type_string (parameters));
      goto out;
    }

  g_variant_get (parameters,
                 "(&s@a{sv}^a&s)",
                 &interface_name_for_signal,
                 &changed_properties,
                 &invalidated_properties);

  if (g_strcmp0 (interface_name_for_signal, proxy->priv->interface_name) != 0)
    goto out;

  g_variant_iter_init (&iter, changed_properties);
  while (g_variant_iter_next (&iter, "{sv}", &key, &value))
    {
      g_hash_table_insert (proxy->priv->properties,
                           key, /* adopts string */
                           value); /* adopts value */
    }

  for (n = 0; invalidated_properties[n] != NULL; n++)
    {
      g_hash_table_remove (proxy->priv->properties, invalidated_properties[n]);
    }

  /* emit signal */
  g_signal_emit (proxy, signals[PROPERTIES_CHANGED_SIGNAL],
                 0,
                 changed_properties,
                 invalidated_properties);

 out:
  if (changed_properties != NULL)
    g_variant_unref (changed_properties);
  g_free (invalidated_properties);
}

/* ---------------------------------------------------------------------------------------------------- */

static void
g_dbus_proxy_constructed (GObject *object)
{
  if (G_OBJECT_CLASS (g_dbus_proxy_parent_class)->constructed != NULL)
    G_OBJECT_CLASS (g_dbus_proxy_parent_class)->constructed (object);
}

/* ---------------------------------------------------------------------------------------------------- */

static void
subscribe_to_signals (GDBusProxy *proxy)
{
  if (!(proxy->priv->flags & G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES))
    {
      /* subscribe to PropertiesChanged() */
      proxy->priv->properties_changed_subscriber_id =
        g_dbus_connection_signal_subscribe (proxy->priv->connection,
                                            proxy->priv->unique_bus_name,
                                            "org.freedesktop.DBus.Properties",
                                            "PropertiesChanged",
                                            proxy->priv->object_path,
                                            proxy->priv->interface_name,
                                            on_properties_changed,
                                            proxy,
                                            NULL);
    }

  if (!(proxy->priv->flags & G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS))
    {
      /* subscribe to all signals for the object */
      proxy->priv->signals_subscriber_id =
        g_dbus_connection_signal_subscribe (proxy->priv->connection,
                                            proxy->priv->unique_bus_name,
                                            proxy->priv->interface_name,
                                            NULL,                        /* member */
                                            proxy->priv->object_path,
                                            NULL,                        /* arg0 */
                                            on_signal_received,
                                            proxy,
                                            NULL);
    }
}

/* ---------------------------------------------------------------------------------------------------- */

static void
process_get_all_reply (GDBusProxy *proxy,
                       GVariant   *result)
{
  GVariantIter *iter;
  gchar *key;
  GVariant *value;

  if (strcmp (g_variant_get_type_string (result), "(a{sv})") != 0)
    {
      g_warning ("Value for GetAll reply with type `%s' does not match `(a{sv})'",
                 g_variant_get_type_string (result));
      goto out;
    }

  g_variant_get (result, "(a{sv})", &iter);
  while (g_variant_iter_next (iter, "{sv}", &key, &value))
    {
      //g_print ("got %s -> %s\n", key, g_variant_markup_print (value, FALSE, 0, 0));

      g_hash_table_insert (proxy->priv->properties,
                           key, /* adopts string */
                           value); /* adopts value */
    }
  g_variant_iter_free (iter);

 out:
  ;
}

static gboolean
initable_init (GInitable     *initable,
               GCancellable  *cancellable,
               GError       **error)
{
  GDBusProxy *proxy = G_DBUS_PROXY (initable);
  GVariant *result;
  gboolean ret;

  ret = FALSE;

  subscribe_to_signals (proxy);

  if (!(proxy->priv->flags & G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES))
    {
      /* load all properties synchronously */
      result = g_dbus_connection_call_sync (proxy->priv->connection,
                                            proxy->priv->unique_bus_name,
                                            proxy->priv->object_path,
                                            "org.freedesktop.DBus.Properties",
                                            "GetAll",
                                            g_variant_new ("(s)", proxy->priv->interface_name),
                                            G_VARIANT_TYPE ("(a{sv})"),
                                            G_DBUS_CALL_FLAGS_NONE,
                                            -1,           /* timeout */
                                            cancellable,
                                            error);
      if (result == NULL)
        goto out;

      process_get_all_reply (proxy, result);

      g_variant_unref (result);
    }

  ret = TRUE;

 out:
  proxy->priv->initialized = TRUE;
  return ret;
}

static void
initable_iface_init (GInitableIface *initable_iface)
{
  initable_iface->init = initable_init;
}

/* ---------------------------------------------------------------------------------------------------- */

static void
get_all_cb (GDBusConnection *connection,
            GAsyncResult    *res,
            gpointer         user_data)
{
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (user_data);
  GVariant *result;
  GError *error;

  error = NULL;
  result = g_dbus_connection_call_finish (connection,
                                          res,
                                          &error);
  if (result == NULL)
    {
      g_simple_async_result_set_from_error (simple, error);
      g_error_free (error);
    }
  else
    {
      g_simple_async_result_set_op_res_gpointer (simple,
                                                 result,
                                                 (GDestroyNotify) g_variant_unref);
    }

  g_simple_async_result_complete_in_idle (simple);
  g_object_unref (simple);
}

static void
async_initable_init_async (GAsyncInitable      *initable,
                           gint                 io_priority,
                           GCancellable        *cancellable,
                           GAsyncReadyCallback  callback,
                           gpointer             user_data)
{
  GDBusProxy *proxy = G_DBUS_PROXY (initable);
  GSimpleAsyncResult *simple;

  simple = g_simple_async_result_new (G_OBJECT (proxy),
                                      callback,
                                      user_data,
                                      NULL);

  subscribe_to_signals (proxy);

  if (!(proxy->priv->flags & G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES))
    {
      /* load all properties asynchronously */
      g_dbus_connection_call (proxy->priv->connection,
                              proxy->priv->unique_bus_name,
                              proxy->priv->object_path,
                              "org.freedesktop.DBus.Properties",
                              "GetAll",
                              g_variant_new ("(s)", proxy->priv->interface_name),
                              G_VARIANT_TYPE ("(a{sv})"),
                              G_DBUS_CALL_FLAGS_NONE,
                              -1,           /* timeout */
                              cancellable,
                              (GAsyncReadyCallback) get_all_cb,
                              simple);
    }
  else
    {
      g_simple_async_result_complete_in_idle (simple);
      g_object_unref (simple);
    }
}

static gboolean
async_initable_init_finish (GAsyncInitable  *initable,
                            GAsyncResult    *res,
                            GError         **error)
{
  GDBusProxy *proxy = G_DBUS_PROXY (initable);
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (res);
  GVariant *result;
  gboolean ret;

  ret = FALSE;

  result = g_simple_async_result_get_op_res_gpointer (simple);
  if (result == NULL)
    {
      if (!(proxy->priv->flags & G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES))
        {
          g_simple_async_result_propagate_error (simple, error);
          goto out;
        }
    }
  else
    {
      process_get_all_reply (proxy, result);
    }

  ret = TRUE;

 out:
  proxy->priv->initialized = TRUE;
  return ret;
}

static void
async_initable_iface_init (GAsyncInitableIface *async_initable_iface)
{
  async_initable_iface->init_async = async_initable_init_async;
  async_initable_iface->init_finish = async_initable_init_finish;
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_dbus_proxy_new:
 * @connection: A #GDBusConnection.
 * @object_type: Either #G_TYPE_DBUS_PROXY or the #GType for the #GDBusProxy<!-- -->-derived type of proxy to create.
 * @flags: Flags used when constructing the proxy.
 * @info: A #GDBusInterfaceInfo specifying the minimal interface that @proxy conforms to or %NULL.
 * @unique_bus_name: A unique bus name or %NULL if @connection is not a message bus connection.
 * @object_path: An object path.
 * @interface_name: A D-Bus interface name.
 * @cancellable: A #GCancellable or %NULL.
 * @callback: Callback function to invoke when the proxy is ready.
 * @user_data: User data to pass to @callback.
 *
 * Creates a proxy for accessing @interface_name on the remote object at @object_path
 * owned by @unique_bus_name at @connection and asynchronously loads D-Bus properties unless the
 * #G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES flag is used. Connect to the
 * #GDBusProxy::g-properties-changed signal to get notified about property changes.
 *
 * If the #G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS flag is not set, also sets up
 * match rules for signals. Connect to the #GDBusProxy::g-signal signal
 * to handle signals from the remote object.
 *
 * This is a failable asynchronous constructor - when the proxy is
 * ready, @callback will be invoked and you can use
 * g_dbus_proxy_new_finish() to get the result.
 *
 * See g_dbus_proxy_new_sync() and for a synchronous version of this constructor.
 *
 * Since: 2.26
 */
void
g_dbus_proxy_new (GDBusConnection     *connection,
                  GType                object_type,
                  GDBusProxyFlags      flags,
                  GDBusInterfaceInfo  *info,
                  const gchar         *unique_bus_name,
                  const gchar         *object_path,
                  const gchar         *interface_name,
                  GCancellable        *cancellable,
                  GAsyncReadyCallback  callback,
                  gpointer             user_data)
{
  g_return_if_fail (G_IS_DBUS_CONNECTION (connection));
  g_return_if_fail (g_type_is_a (object_type, G_TYPE_DBUS_PROXY));
  g_return_if_fail ((unique_bus_name == NULL && g_dbus_connection_get_unique_name (connection) == NULL) ||
                    g_dbus_is_unique_name (unique_bus_name));
  g_return_if_fail (g_variant_is_object_path (object_path));
  g_return_if_fail (g_dbus_is_interface_name (interface_name));

  g_async_initable_new_async (object_type,
                              G_PRIORITY_DEFAULT,
                              cancellable,
                              callback,
                              user_data,
                              "g-flags", flags,
                              "g-interface-info", info,
                              "g-unique-bus-name", unique_bus_name,
                              "g-connection", connection,
                              "g-object-path", object_path,
                              "g-interface-name", interface_name,
                              NULL);
}

/**
 * g_dbus_proxy_new_finish:
 * @res: A #GAsyncResult obtained from the #GAsyncReadyCallback function passed to g_dbus_proxy_new().
 * @error: Return location for error or %NULL.
 *
 * Finishes creating a #GDBusProxy.
 *
 * Returns: A #GDBusProxy or %NULL if @error is set. Free with g_object_unref().
 *
 * Since: 2.26
 */
GDBusProxy *
g_dbus_proxy_new_finish (GAsyncResult  *res,
                         GError       **error)
{
  GObject *object;
  GObject *source_object;

  source_object = g_async_result_get_source_object (res);
  g_assert (source_object != NULL);

  object = g_async_initable_new_finish (G_ASYNC_INITABLE (source_object),
                                        res,
                                        error);
  g_object_unref (source_object);

  if (object != NULL)
    return G_DBUS_PROXY (object);
  else
    return NULL;
}


/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_dbus_proxy_new_sync:
 * @connection: A #GDBusConnection.
 * @object_type: Either #G_TYPE_DBUS_PROXY or the #GType for the #GDBusProxy<!-- -->-derived type of proxy to create.
 * @flags: Flags used when constructing the proxy.
 * @info: A #GDBusInterfaceInfo specifying the minimal interface that @proxy conforms to or %NULL.
 * @unique_bus_name: A unique bus name or %NULL if @connection is not a message bus connection.
 * @object_path: An object path.
 * @interface_name: A D-Bus interface name.
 * @cancellable: A #GCancellable or %NULL.
 * @error: Return location for error or %NULL.
 *
 * Creates a proxy for accessing @interface_name on the remote object at @object_path
 * owned by @unique_bus_name at @connection and synchronously loads D-Bus properties unless the
 * #G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES flag is used.
 *
 * If the #G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS flag is not set, also sets up
 * match rules for signals. Connect to the #GDBusProxy::g-signal signal
 * to handle signals from the remote object.
 *
 * This is a synchronous failable constructor. See g_dbus_proxy_new()
 * and g_dbus_proxy_new_finish() for the asynchronous version.
 *
 * Returns: A #GDBusProxy or %NULL if error is set. Free with g_object_unref().
 *
 * Since: 2.26
 */
GDBusProxy *
g_dbus_proxy_new_sync (GDBusConnection     *connection,
                       GType                object_type,
                       GDBusProxyFlags      flags,
                       GDBusInterfaceInfo  *info,
                       const gchar         *unique_bus_name,
                       const gchar         *object_path,
                       const gchar         *interface_name,
                       GCancellable        *cancellable,
                       GError             **error)
{
  GInitable *initable;

  g_return_val_if_fail (G_IS_DBUS_CONNECTION (connection), NULL);
  g_return_val_if_fail (g_type_is_a (object_type, G_TYPE_DBUS_PROXY), NULL);
  g_return_val_if_fail ((unique_bus_name == NULL && g_dbus_connection_get_unique_name (connection) == NULL) ||
                        g_dbus_is_unique_name (unique_bus_name), NULL);
  g_return_val_if_fail (g_variant_is_object_path (object_path), NULL);
  g_return_val_if_fail (g_dbus_is_interface_name (interface_name), NULL);

  initable = g_initable_new (object_type,
                             cancellable,
                             error,
                             "g-flags", flags,
                             "g-interface-info", info,
                             "g-unique-bus-name", unique_bus_name,
                             "g-connection", connection,
                             "g-object-path", object_path,
                             "g-interface-name", interface_name,
                             NULL);
  if (initable != NULL)
    return G_DBUS_PROXY (initable);
  else
    return NULL;
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_dbus_proxy_get_connection:
 * @proxy: A #GDBusProxy.
 *
 * Gets the connection @proxy is for.
 *
 * Returns: A #GDBusConnection owned by @proxy. Do not free.
 *
 * Since: 2.26
 */
GDBusConnection *
g_dbus_proxy_get_connection (GDBusProxy *proxy)
{
  g_return_val_if_fail (G_IS_DBUS_PROXY (proxy), NULL);
  return proxy->priv->connection;
}

/**
 * g_dbus_proxy_get_flags:
 * @proxy: A #GDBusProxy.
 *
 * Gets the flags that @proxy was constructed with.
 *
 * Returns: Flags from the #GDBusProxyFlags enumeration.
 *
 * Since: 2.26
 */
GDBusProxyFlags
g_dbus_proxy_get_flags (GDBusProxy *proxy)
{
  g_return_val_if_fail (G_IS_DBUS_PROXY (proxy), 0);
  return proxy->priv->flags;
}

/**
 * g_dbus_proxy_get_unique_bus_name:
 * @proxy: A #GDBusProxy.
 *
 * Gets the unique bus name @proxy is for.
 *
 * Returns: A string owned by @proxy. Do not free.
 *
 * Since: 2.26
 */
const gchar *
g_dbus_proxy_get_unique_bus_name (GDBusProxy *proxy)
{
  g_return_val_if_fail (G_IS_DBUS_PROXY (proxy), NULL);
  return proxy->priv->unique_bus_name;
}

/**
 * g_dbus_proxy_get_object_path:
 * @proxy: A #GDBusProxy.
 *
 * Gets the object path @proxy is for.
 *
 * Returns: A string owned by @proxy. Do not free.
 *
 * Since: 2.26
 */
const gchar *
g_dbus_proxy_get_object_path (GDBusProxy *proxy)
{
  g_return_val_if_fail (G_IS_DBUS_PROXY (proxy), NULL);
  return proxy->priv->object_path;
}

/**
 * g_dbus_proxy_get_interface_name:
 * @proxy: A #GDBusProxy.
 *
 * Gets the D-Bus interface name @proxy is for.
 *
 * Returns: A string owned by @proxy. Do not free.
 *
 * Since: 2.26
 */
const gchar *
g_dbus_proxy_get_interface_name (GDBusProxy *proxy)
{
  g_return_val_if_fail (G_IS_DBUS_PROXY (proxy), NULL);
  return proxy->priv->interface_name;
}

/**
 * g_dbus_proxy_get_default_timeout:
 * @proxy: A #GDBusProxy.
 *
 * Gets the timeout to use if -1 (specifying default timeout) is
 * passed as @timeout_msec in the g_dbus_proxy_call() and
 * g_dbus_proxy_call_sync() functions.
 *
 * See the #GDBusProxy:g-default-timeout property for more details.
 *
 * Returns: Timeout to use for @proxy.
 *
 * Since: 2.26
 */
gint
g_dbus_proxy_get_default_timeout (GDBusProxy *proxy)
{
  g_return_val_if_fail (G_IS_DBUS_PROXY (proxy), -1);
  return proxy->priv->timeout_msec;
}

/**
 * g_dbus_proxy_set_default_timeout:
 * @proxy: A #GDBusProxy.
 * @timeout_msec: Timeout in milliseconds.
 *
 * Sets the timeout to use if -1 (specifying default timeout) is
 * passed as @timeout_msec in the g_dbus_proxy_call() and
 * g_dbus_proxy_call_sync() functions.
 *
 * See the #GDBusProxy:g-default-timeout property for more details.
 *
 * Since: 2.26
 */
void
g_dbus_proxy_set_default_timeout (GDBusProxy *proxy,
                                  gint        timeout_msec)
{
  g_return_if_fail (G_IS_DBUS_PROXY (proxy));
  g_return_if_fail (timeout_msec == -1 || timeout_msec >= 0);

  /* TODO: locking? */
  if (proxy->priv->timeout_msec != timeout_msec)
    {
      proxy->priv->timeout_msec = timeout_msec;
      g_object_notify (G_OBJECT (proxy), "g-default-timeout");
    }
}

/**
 * g_dbus_proxy_get_interface_info:
 * @proxy: A #GDBusProxy
 *
 * Returns the #GDBusInterfaceInfo, if any, specifying the minimal
 * interface that @proxy conforms to.
 *
 * See the #GDBusProxy:g-interface-info property for more details.
 *
 * Returns: A #GDBusInterfaceInfo or %NULL. Do not unref the returned
 * object, it is owned by @proxy.
 *
 * Since: 2.26
 */
GDBusInterfaceInfo *
g_dbus_proxy_get_interface_info (GDBusProxy *proxy)
{
  g_return_val_if_fail (G_IS_DBUS_PROXY (proxy), NULL);
  return proxy->priv->expected_interface;
}

/**
 * g_dbus_proxy_set_interface_info:
 * @proxy: A #GDBusProxy
 * @info: Minimum interface this proxy conforms to or %NULL to unset.
 *
 * Ensure that interactions with @proxy conform to the given
 * interface.  For example, when completing a method call, if the type
 * signature of the message isn't what's expected, the given #GError
 * is set.  Signals that have a type signature mismatch are simply
 * dropped.
 *
 * See the #GDBusProxy:g-interface-info property for more details.
 *
 * Since: 2.26
 */
void
g_dbus_proxy_set_interface_info (GDBusProxy         *proxy,
                                 GDBusInterfaceInfo *info)
{
  g_return_if_fail (G_IS_DBUS_PROXY (proxy));
  if (proxy->priv->expected_interface != NULL)
    g_dbus_interface_info_unref (proxy->priv->expected_interface);
  proxy->priv->expected_interface = info != NULL ? g_dbus_interface_info_ref (info) : NULL;
}

/* ---------------------------------------------------------------------------------------------------- */

static gboolean
maybe_split_method_name (const gchar  *method_name,
                         gchar       **out_interface_name,
                         const gchar **out_method_name)
{
  gboolean was_split;

  was_split = FALSE;
  g_assert (out_interface_name != NULL);
  g_assert (out_method_name != NULL);
  *out_interface_name = NULL;
  *out_method_name = NULL;

  if (strchr (method_name, '.') != NULL)
    {
      gchar *p;
      gchar *last_dot;

      p = g_strdup (method_name);
      last_dot = strrchr (p, '.');
      *last_dot = '\0';

      *out_interface_name = p;
      *out_method_name = last_dot + 1;

      was_split = TRUE;
    }

  return was_split;
}


static void
reply_cb (GDBusConnection *connection,
          GAsyncResult    *res,
          gpointer         user_data)
{
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (user_data);
  GVariant *value;
  GError *error;

  error = NULL;
  value = g_dbus_connection_call_finish (connection,
                                         res,
                                         &error);
  if (error != NULL)
    {
      g_simple_async_result_set_from_error (simple,
                                            error);
      g_error_free (error);
    }
  else
    {
      g_simple_async_result_set_op_res_gpointer (simple,
                                                 value,
                                                 (GDestroyNotify) g_variant_unref);
    }

  /* no need to complete in idle since the method GDBusConnection already does */
  g_simple_async_result_complete (simple);
}

static const GDBusMethodInfo *
lookup_method_info_or_warn (GDBusProxy  *proxy,
                            const gchar *method_name)
{
  const GDBusMethodInfo *info;

  if (proxy->priv->expected_interface == NULL)
    return NULL;

  info = g_dbus_interface_info_lookup_method (proxy->priv->expected_interface, method_name);
  if (info == NULL)
    {
      g_warning ("Trying to invoke method %s which isn't in expected interface %s",
                 method_name, proxy->priv->expected_interface->name);
    }

  return info;
}

/**
 * g_dbus_proxy_call:
 * @proxy: A #GDBusProxy.
 * @method_name: Name of method to invoke.
 * @parameters: A #GVariant tuple with parameters for the signal or %NULL if not passing parameters.
 * @flags: Flags from the #GDBusCallFlags enumeration.
 * @timeout_msec: The timeout in milliseconds or -1 to use the proxy default timeout.
 * @cancellable: A #GCancellable or %NULL.
 * @callback: A #GAsyncReadyCallback to call when the request is satisfied or %NULL if you don't
 * care about the result of the method invocation.
 * @user_data: The data to pass to @callback.
 *
 * Asynchronously invokes the @method_name method on @proxy.
 *
 * If @method_name contains any dots, then @name is split into interface and
 * method name parts. This allows using @proxy for invoking methods on
 * other interfaces.
 *
 * If the #GDBusConnection associated with @proxy is closed then
 * the operation will fail with %G_IO_ERROR_CLOSED. If
 * @cancellable is canceled, the operation will fail with
 * %G_IO_ERROR_CANCELLED. If @parameters contains a value not
 * compatible with the D-Bus protocol, the operation fails with
 * %G_IO_ERROR_INVALID_ARGUMENT.
 *
 * If the @parameters #GVariant is floating, it is consumed. This allows
 * convenient 'inline' use of g_variant_new(), e.g.:
 * |[
 *  g_dbus_proxy_call (proxy,
 *                     "TwoStrings",
 *                     g_variant_new ("(ss)",
 *                                    "Thing One",
 *                                    "Thing Two"),
 *                     G_DBUS_CALL_FLAGS_NONE,
 *                     -1,
 *                     NULL,
 *                     (GAsyncReadyCallback) two_strings_done,
 *                     &amp;data);
 * ]|
 *
 * This is an asynchronous method. When the operation is finished,
 * @callback will be invoked in the
 * <link linkend="g-main-context-push-thread-default">thread-default
 * main loop</link> of the thread you are calling this method from.
 * You can then call g_dbus_proxy_call_finish() to get the result of
 * the operation. See g_dbus_proxy_call_sync() for the synchronous
 * version of this method.
 *
 * Since: 2.26
 */
void
g_dbus_proxy_call (GDBusProxy          *proxy,
                   const gchar         *method_name,
                   GVariant            *parameters,
                   GDBusCallFlags       flags,
                   gint                 timeout_msec,
                   GCancellable        *cancellable,
                   GAsyncReadyCallback  callback,
                   gpointer             user_data)
{
  GSimpleAsyncResult *simple;
  gboolean was_split;
  gchar *split_interface_name;
  const gchar *split_method_name;
  const GDBusMethodInfo *expected_method_info;
  const gchar *target_method_name;
  const gchar *target_interface_name;
  GVariantType *reply_type;

  g_return_if_fail (G_IS_DBUS_PROXY (proxy));
  g_return_if_fail (g_dbus_is_member_name (method_name) || g_dbus_is_interface_name (method_name));
  g_return_if_fail (parameters == NULL || g_variant_is_of_type (parameters, G_VARIANT_TYPE_TUPLE));
  g_return_if_fail (timeout_msec == -1 || timeout_msec >= 0);

  simple = g_simple_async_result_new (G_OBJECT (proxy),
                                      callback,
                                      user_data,
                                      g_dbus_proxy_call);

  was_split = maybe_split_method_name (method_name, &split_interface_name, &split_method_name);
  target_method_name = was_split ? split_method_name : method_name;
  target_interface_name = was_split ? split_interface_name : proxy->priv->interface_name;

  g_object_set_data_full (G_OBJECT (simple), "-gdbus-proxy-method-name", g_strdup (target_method_name), g_free);

  /* Just warn here */
  expected_method_info = lookup_method_info_or_warn (proxy, target_method_name);

  if (expected_method_info)
    reply_type = _g_dbus_compute_complete_signature (expected_method_info->out_args);
  else
    reply_type = NULL;

  g_dbus_connection_call (proxy->priv->connection,
                          proxy->priv->unique_bus_name,
                          proxy->priv->object_path,
                          target_interface_name,
                          target_method_name,
                          parameters,
                          reply_type,
                          flags,
                          timeout_msec == -1 ? proxy->priv->timeout_msec : timeout_msec,
                          cancellable,
                          (GAsyncReadyCallback) reply_cb,
                          simple);

  if (reply_type != NULL)
    g_variant_type_free (reply_type);

  g_free (split_interface_name);
}

/**
 * g_dbus_proxy_call_finish:
 * @proxy: A #GDBusProxy.
 * @res: A #GAsyncResult obtained from the #GAsyncReadyCallback passed to g_dbus_proxy_call().
 * @error: Return location for error or %NULL.
 *
 * Finishes an operation started with g_dbus_proxy_call().
 *
 * Returns: %NULL if @error is set. Otherwise a #GVariant tuple with
 * return values. Free with g_variant_unref().
 *
 * Since: 2.26
 */
GVariant *
g_dbus_proxy_call_finish (GDBusProxy    *proxy,
                          GAsyncResult  *res,
                          GError       **error)
{
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (res);
  GVariant *value;
  const char *method_name;

  g_return_val_if_fail (G_IS_DBUS_PROXY (proxy), NULL);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (res), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  g_warn_if_fail (g_simple_async_result_get_source_tag (simple) == g_dbus_proxy_call);

  value = NULL;

  if (g_simple_async_result_propagate_error (simple, error))
    goto out;

  value = g_simple_async_result_get_op_res_gpointer (simple);
  method_name = g_object_get_data (G_OBJECT (simple), "-gdbus-proxy-method-name");

 out:
  return value;
}

/**
 * g_dbus_proxy_call_sync:
 * @proxy: A #GDBusProxy.
 * @method_name: Name of method to invoke.
 * @parameters: A #GVariant tuple with parameters for the signal or %NULL if not passing parameters.
 * @flags: Flags from the #GDBusCallFlags enumeration.
 * @timeout_msec: The timeout in milliseconds or -1 to use the proxy default timeout.
 * @cancellable: A #GCancellable or %NULL.
 * @error: Return location for error or %NULL.
 *
 * Synchronously invokes the @method_name method on @proxy.
 *
 * If @method_name contains any dots, then @name is split into interface and
 * method name parts. This allows using @proxy for invoking methods on
 * other interfaces.
 *
 * If the #GDBusConnection associated with @proxy is disconnected then
 * the operation will fail with %G_IO_ERROR_CLOSED. If
 * @cancellable is canceled, the operation will fail with
 * %G_IO_ERROR_CANCELLED. If @parameters contains a value not
 * compatible with the D-Bus protocol, the operation fails with
 * %G_IO_ERROR_INVALID_ARGUMENT.
 *
 * If the @parameters #GVariant is floating, it is consumed. This allows
 * convenient 'inline' use of g_variant_new(), e.g.:
 * |[
 *  g_dbus_proxy_call_sync (proxy,
 *                          "TwoStrings",
 *                          g_variant_new ("(ss)",
 *                                         "Thing One",
 *                                         "Thing Two"),
 *                          G_DBUS_CALL_FLAGS_NONE,
 *                          -1,
 *                          NULL,
 *                          &amp;error);
 * ]|
 *
 * The calling thread is blocked until a reply is received. See
 * g_dbus_proxy_call() for the asynchronous version of this
 * method.
 *
 * Returns: %NULL if @error is set. Otherwise a #GVariant tuple with
 * return values. Free with g_variant_unref().
 *
 * Since: 2.26
 */
GVariant *
g_dbus_proxy_call_sync (GDBusProxy      *proxy,
                        const gchar     *method_name,
                        GVariant        *parameters,
                        GDBusCallFlags   flags,
                        gint             timeout_msec,
                        GCancellable    *cancellable,
                        GError         **error)
{
  GVariant *ret;
  gboolean was_split;
  gchar *split_interface_name;
  const gchar *split_method_name;
  const GDBusMethodInfo *expected_method_info;
  const gchar *target_method_name;
  const gchar *target_interface_name;
  GVariantType *reply_type;

  g_return_val_if_fail (G_IS_DBUS_PROXY (proxy), NULL);
  g_return_val_if_fail (g_dbus_is_member_name (method_name) || g_dbus_is_interface_name (method_name), NULL);
  g_return_val_if_fail (parameters == NULL || g_variant_is_of_type (parameters, G_VARIANT_TYPE_TUPLE), NULL);
  g_return_val_if_fail (timeout_msec == -1 || timeout_msec >= 0, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  was_split = maybe_split_method_name (method_name, &split_interface_name, &split_method_name);
  target_method_name = was_split ? split_method_name : method_name;
  target_interface_name = was_split ? split_interface_name : proxy->priv->interface_name;

  if (proxy->priv->expected_interface)
    {
      expected_method_info = g_dbus_interface_info_lookup_method (proxy->priv->expected_interface, target_method_name);
      if (expected_method_info == NULL)
        {
          g_warning ("Trying to invoke method `%s' which isn't in expected interface `%s'",
                     target_method_name,
                     target_interface_name);
        }
    }
  else
    {
      expected_method_info = NULL;
    }

  if (expected_method_info)
    reply_type = _g_dbus_compute_complete_signature (expected_method_info->out_args);
  else
    reply_type = NULL;

  ret = g_dbus_connection_call_sync (proxy->priv->connection,
                                     proxy->priv->unique_bus_name,
                                     proxy->priv->object_path,
                                     target_interface_name,
                                     target_method_name,
                                     parameters,
                                     reply_type,
                                     flags,
                                     timeout_msec == -1 ? proxy->priv->timeout_msec : timeout_msec,
                                     cancellable,
                                     error);

  if (reply_type != NULL)
    g_variant_type_free (reply_type);

  g_free (split_interface_name);

  return ret;
}

/* ---------------------------------------------------------------------------------------------------- */

#define __G_DBUS_PROXY_C__
#include "gioaliasdef.c"
