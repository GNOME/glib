/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright (C) 2008 Red Hat, Inc.
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
 */

#include "config.h"
#include <glib.h>
#include "glibintl.h"

#include <stdlib.h>
#include "gnetworkaddress.h"
#include "gasyncresult.h"
#include "ginetaddress.h"
#include "ginetsocketaddress.h"
#include "gnetworkingprivate.h"
#include "gresolver.h"
#include "gsimpleasyncresult.h"
#include "gsocketaddressenumerator.h"
#include "gioerror.h"
#include "gsocketconnectable.h"

#include <string.h>

#include "gioalias.h"

/**
 * SECTION:gnetworkaddress
 * @short_description: A GSocketConnectable for resolving hostnames
 * @include: gio/gio.h
 *
 * #GNetworkAddress provides an easy way to resolve a hostname and
 * then attempt to connect to that host, handling the possibility of
 * multiple IP addresses and multiple address families.
 *
 * See #GSocketConnectable for and example of using the connectable
 * interface.
 */

/**
 * GNetworkAddress:
 *
 * A #GSocketConnectable for resolving a hostname and connecting to
 * that host.
 */

struct _GNetworkAddressPrivate {
  gchar *hostname;
  guint16 port;
  GList *sockaddrs;
};

enum {
  PROP_0,
  PROP_HOSTNAME,
  PROP_PORT,
};

static void g_network_address_set_property (GObject      *object,
                                            guint         prop_id,
                                            const GValue *value,
                                            GParamSpec   *pspec);
static void g_network_address_get_property (GObject      *object,
                                            guint         prop_id,
                                            GValue       *value,
                                            GParamSpec   *pspec);

static void                      g_network_address_connectable_iface_init (GSocketConnectableIface *iface);
static GSocketAddressEnumerator *g_network_address_connectable_enumerate  (GSocketConnectable      *connectable);

G_DEFINE_TYPE_WITH_CODE (GNetworkAddress, g_network_address, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_SOCKET_CONNECTABLE,
                                                g_network_address_connectable_iface_init))

static void
g_network_address_finalize (GObject *object)
{
  GNetworkAddress *addr = G_NETWORK_ADDRESS (object);

  g_free (addr->priv->hostname);

  if (addr->priv->sockaddrs)
    {
      GList *a;

      for (a = addr->priv->sockaddrs; a; a = a->next)
        g_object_unref (a->data);
      g_list_free (addr->priv->sockaddrs);
    }

  G_OBJECT_CLASS (g_network_address_parent_class)->finalize (object);
}

static void
g_network_address_class_init (GNetworkAddressClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (GNetworkAddressPrivate));

  gobject_class->set_property = g_network_address_set_property;
  gobject_class->get_property = g_network_address_get_property;
  gobject_class->finalize = g_network_address_finalize;

  g_object_class_install_property (gobject_class, PROP_HOSTNAME,
                                   g_param_spec_string ("hostname",
                                                        P_("Hostname"),
                                                        P_("Hostname to resolve"),
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_PORT,
                                   g_param_spec_uint ("port",
                                                      P_("Port"),
                                                      P_("Network port"),
                                                      0, 65535, 0,
                                                      G_PARAM_READWRITE |
                                                      G_PARAM_CONSTRUCT_ONLY |
                                                      G_PARAM_STATIC_STRINGS));
}

static void
g_network_address_connectable_iface_init (GSocketConnectableIface *connectable_iface)
{
  connectable_iface->enumerate  = g_network_address_connectable_enumerate;
}

static void
g_network_address_init (GNetworkAddress *addr)
{
  addr->priv = G_TYPE_INSTANCE_GET_PRIVATE (addr, G_TYPE_NETWORK_ADDRESS,
                                            GNetworkAddressPrivate);
}

static void
g_network_address_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GNetworkAddress *addr = G_NETWORK_ADDRESS (object);

  switch (prop_id)
    {
    case PROP_HOSTNAME:
      if (addr->priv->hostname)
        g_free (addr->priv->hostname);
      addr->priv->hostname = g_value_dup_string (value);
      break;

    case PROP_PORT:
      addr->priv->port = g_value_get_uint (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }

}

static void
g_network_address_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  GNetworkAddress *addr = G_NETWORK_ADDRESS (object);

  switch (prop_id)
    {
    case PROP_HOSTNAME:
      g_value_set_string (value, addr->priv->hostname);
      break;

    case PROP_PORT:
      g_value_set_uint (value, addr->priv->port);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }

}

static void
g_network_address_set_addresses (GNetworkAddress *addr,
                                 GList           *addresses)
{
  GList *a;
  GSocketAddress *sockaddr;

  g_return_if_fail (addresses != NULL && addr->priv->sockaddrs == NULL);

  for (a = addresses; a; a = a->next)
    {
      sockaddr = g_inet_socket_address_new (a->data, addr->priv->port);
      addr->priv->sockaddrs = g_list_prepend (addr->priv->sockaddrs, sockaddr);
      g_object_unref (a->data);
    }
  g_list_free (addresses);
  addr->priv->sockaddrs = g_list_reverse (addr->priv->sockaddrs);
}

/**
 * g_network_address_new:
 * @hostname: the hostname
 * @port: the port
 *
 * Creates a new #GSocketConnectable for connecting to the given
 * @hostname and @port.
 *
 * Return value: the new #GNetworkAddress
 *
 * Since: 2.22
 */
GSocketConnectable *
g_network_address_new (const gchar *hostname,
                       guint16      port)
{
  return g_object_new (G_TYPE_NETWORK_ADDRESS,
                       "hostname", hostname,
                       "port", port,
                       NULL);
}

/**
 * g_network_address_parse:
 * @host_and_port: the hostname and optionally a port
 * @default_port: the default port if not in @host_and_port
 * @error: a pointer to a #GError, or %NULL
 *
 * Creates a new #GSocketConnectable for connecting to the given
 * @hostname and @port. May fail and return %NULL in case
 * parsing @host_and_port fails.
 *
 * @host_and_port may be in any of a number of recognised formats: an IPv6
 * address, an IPv4 address, or a domain name (in which case a DNS
 * lookup is performed). Quoting with [] is supported for all address
 * types. A port override may be specified in the usual way with a
 * colon. Ports may be given as decimal numbers or symbolic names (in
 * which case an /etc/services lookup is performed).
 *
 * If no port is specified in @host_and_port then @default_port will be
 * used as the port number to connect to.
 *
 * In general, @host_and_port is expected to be provided by the user
 * (allowing them to give the hostname, and a port overide if necessary)
 * and @default_port is expected to be provided by the application.
 *
 * Return value: the new #GNetworkAddress, or %NULL on error
 *
 * Since: 2.22
 */
GSocketConnectable *
g_network_address_parse (const gchar  *host_and_port,
                         guint16       default_port,
                         GError      **error)
{
  GSocketConnectable *connectable;
  const gchar *port;
  guint16 portnum;
  gchar *name;

  g_return_val_if_fail (host_and_port != NULL, NULL);

  port = NULL;
  if (host_and_port[0] == '[')
    /* escaped host part (to allow, eg. "[2001:db8::1]:888") */
    {
      const gchar *end;

      end = strchr (host_and_port, ']');
      if (end == NULL)
        {
          g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                       _("Hostname '%s' contains '[' but not ']'"), host_and_port);
          return NULL;
        }

      if (end[1] == '\0')
        port = NULL;
      else if (end[1] == ':')
        port = &end[2];
      else
        {
          g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                       "The ']' character (in hostname '%s') must come at the"
                       " end or be immediately followed by ':' and a port",
                       host_and_port);
          return NULL;
        }

      name = g_strndup (host_and_port + 1, end - host_and_port - 1);
    }

  else if ((port = strchr (host_and_port, ':')))
    /* string has a ':' in it */
    {
      /* skip ':' */
      port++;

      if (strchr (port, ':'))
        /* more than one ':' in string */
        {
          /* this is actually an unescaped IPv6 address */
          name = g_strdup (host_and_port);
          port = NULL;
        }
      else
        name = g_strndup (host_and_port, port - host_and_port - 1);
    }

  else
    /* plain hostname, no port */
    name = g_strdup (host_and_port);

  if (port != NULL)
    {
      if (port[0] == '\0')
        {
          g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                       "If a ':' character is given, it must be followed by a "
                       "port (in hostname '%s').", host_and_port);
          g_free (name);
          return NULL;
        }

      else if ('0' <= port[0] && port[0] <= '9')
        {
          char *end;
          long value;

          value = strtol (port, &end, 10);
          if (*end != '\0' || value < 0 || value > G_MAXUINT16)
            {
              g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                           "Invalid numeric port '%s' specified in hostname '%s'",
                           port, host_and_port);
              g_free (name);
              return NULL;
            }

          portnum = value;
        }

      else
        {
          struct servent *entry;

          entry = getservbyname (port, "tcp");
          if (entry == NULL)
            {
              g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                           "Unknown service '%s' specified in hostname '%s'",
                           port, host_and_port);
#ifdef HAVE_ENDSERVENT
              endservent ();
#endif
              g_free (name);
              return NULL;
            }

          portnum = g_ntohs (entry->s_port);

#ifdef HAVE_ENDSERVENT
          endservent ();
#endif
        }
    }
  else
    {
      /* No port in host_and_port */
      portnum = default_port;
    }

  connectable = g_network_address_new (name, portnum);
  g_free (name);

  return connectable;
}

/**
 * g_network_address_get_hostname:
 * @addr: a #GNetworkAddress
 *
 * Gets @addr's hostname. This might be either UTF-8 or ASCII-encoded,
 * depending on what @addr was created with.
 *
 * Return value: @addr's hostname
 *
 * Since: 2.22
 */
const gchar *
g_network_address_get_hostname (GNetworkAddress *addr)
{
  g_return_val_if_fail (G_IS_NETWORK_ADDRESS (addr), NULL);

  return addr->priv->hostname;
}

/**
 * g_network_address_get_port:
 * @addr: a #GNetworkAddress
 *
 * Gets @addr's port number
 *
 * Return value: @addr's port (which may be 0)
 *
 * Since: 2.22
 */
guint16
g_network_address_get_port (GNetworkAddress *addr)
{
  g_return_val_if_fail (G_IS_NETWORK_ADDRESS (addr), 0);

  return addr->priv->port;
}

#define G_TYPE_NETWORK_ADDRESS_ADDRESS_ENUMERATOR (_g_network_address_address_enumerator_get_type ())
#define G_NETWORK_ADDRESS_ADDRESS_ENUMERATOR(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), G_TYPE_NETWORK_ADDRESS_ADDRESS_ENUMERATOR, GNetworkAddressAddressEnumerator))

typedef struct {
  GSocketAddressEnumerator parent_instance;

  GNetworkAddress *addr;
  GList *a;
} GNetworkAddressAddressEnumerator;

typedef struct {
  GSocketAddressEnumeratorClass parent_class;

} GNetworkAddressAddressEnumeratorClass;

G_DEFINE_TYPE (GNetworkAddressAddressEnumerator, _g_network_address_address_enumerator, G_TYPE_SOCKET_ADDRESS_ENUMERATOR)

static void
g_network_address_address_enumerator_finalize (GObject *object)
{
  GNetworkAddressAddressEnumerator *addr_enum =
    G_NETWORK_ADDRESS_ADDRESS_ENUMERATOR (object);

  g_object_unref (addr_enum->addr);

  G_OBJECT_CLASS (_g_network_address_address_enumerator_parent_class)->finalize (object);
}

static GSocketAddress *
g_network_address_address_enumerator_next (GSocketAddressEnumerator  *enumerator,
                                           GCancellable              *cancellable,
                                           GError                   **error)
{
  GNetworkAddressAddressEnumerator *addr_enum =
    G_NETWORK_ADDRESS_ADDRESS_ENUMERATOR (enumerator);
  GSocketAddress *sockaddr;

  if (!addr_enum->addr->priv->sockaddrs)
    {
      GResolver *resolver = g_resolver_get_default ();
      GList *addresses;

      addresses = g_resolver_lookup_by_name (resolver,
                                             addr_enum->addr->priv->hostname,
                                             cancellable, error);
      g_object_unref (resolver);

      if (!addresses)
        return NULL;

      g_network_address_set_addresses (addr_enum->addr, addresses);
      addr_enum->a = addr_enum->addr->priv->sockaddrs;
    }

  if (!addr_enum->a)
    return NULL;
  else
    {
      sockaddr = addr_enum->a->data;
      addr_enum->a = addr_enum->a->next;
      return g_object_ref (sockaddr);
    }
}

static void
got_addresses (GObject      *source_object,
               GAsyncResult *result,
               gpointer      user_data)
{
  GSimpleAsyncResult *simple = user_data;
  GNetworkAddressAddressEnumerator *addr_enum =
    g_simple_async_result_get_op_res_gpointer (simple);
  GResolver *resolver = G_RESOLVER (source_object);
  GList *addresses;
  GError *error = NULL;

  addresses = g_resolver_lookup_by_name_finish (resolver, result, &error);
  if (!addr_enum->addr->priv->sockaddrs)
    {
      if (error)
        {
          g_simple_async_result_set_from_error (simple, error);
          g_error_free (error);
        }
      else
        {
          g_network_address_set_addresses (addr_enum->addr, addresses);
          addr_enum->a = addr_enum->addr->priv->sockaddrs;
        }
    }
  else if (error)
    g_error_free (error);

  g_object_unref (resolver);

  g_simple_async_result_complete (simple);
  g_object_unref (simple);
}

static void
g_network_address_address_enumerator_next_async (GSocketAddressEnumerator  *enumerator,
                                                 GCancellable              *cancellable,
                                                 GAsyncReadyCallback        callback,
                                                 gpointer                   user_data)
{
  GNetworkAddressAddressEnumerator *addr_enum =
    G_NETWORK_ADDRESS_ADDRESS_ENUMERATOR (enumerator);
  GSimpleAsyncResult *simple;

  simple = g_simple_async_result_new (G_OBJECT (enumerator),
                                      callback, user_data,
                                      g_network_address_address_enumerator_next_async);

  if (!addr_enum->addr->priv->sockaddrs)
    {
      GResolver *resolver = g_resolver_get_default ();

      g_simple_async_result_set_op_res_gpointer (simple, g_object_ref (addr_enum), g_object_unref);
      g_resolver_lookup_by_name_async (resolver,
                                       addr_enum->addr->priv->hostname,
                                       cancellable,
                                       got_addresses, simple);
    }
  else
    {
      g_simple_async_result_complete_in_idle (simple);
      g_object_unref (simple);
    }
}

static GSocketAddress *
g_network_address_address_enumerator_next_finish (GSocketAddressEnumerator  *enumerator,
                                                  GAsyncResult              *result,
                                                  GError                   **error)
{
  GNetworkAddressAddressEnumerator *addr_enum =
    G_NETWORK_ADDRESS_ADDRESS_ENUMERATOR (enumerator);
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (result);
  GSocketAddress *sockaddr;

  if (g_simple_async_result_propagate_error (simple, error))
    return NULL;
  else if (!addr_enum->a)
    return NULL;
  else
    {
      sockaddr = addr_enum->a->data;
      addr_enum->a = addr_enum->a->next;
      return g_object_ref (sockaddr);
    }
}

static void
_g_network_address_address_enumerator_init (GNetworkAddressAddressEnumerator *enumerator)
{
}

static void
_g_network_address_address_enumerator_class_init (GNetworkAddressAddressEnumeratorClass *addrenum_class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (addrenum_class);
  GSocketAddressEnumeratorClass *enumerator_class =
    G_SOCKET_ADDRESS_ENUMERATOR_CLASS (addrenum_class);

  enumerator_class->next = g_network_address_address_enumerator_next;
  enumerator_class->next_async = g_network_address_address_enumerator_next_async;
  enumerator_class->next_finish = g_network_address_address_enumerator_next_finish;
  object_class->finalize = g_network_address_address_enumerator_finalize;
}

static GSocketAddressEnumerator *
g_network_address_connectable_enumerate (GSocketConnectable *connectable)
{
  GNetworkAddressAddressEnumerator *addr_enum;

  addr_enum = g_object_new (G_TYPE_NETWORK_ADDRESS_ADDRESS_ENUMERATOR, NULL);
  addr_enum->addr = g_object_ref (connectable);

  return (GSocketAddressEnumerator *)addr_enum;
}

#define __G_NETWORK_ADDRESS_C__
#include "gioaliasdef.c"
