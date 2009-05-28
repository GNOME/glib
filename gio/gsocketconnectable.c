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
#include "gsocketconnectable.h"
#include "glibintl.h"

#include "gioalias.h"

/**
 * SECTION:gsocketconnectable
 * @short_description: Interface for potential socket endpoints
 *
 * Objects that describe one or more potential socket endpoints
 * implement #GSocketConnectable. Callers can then use
 * g_socket_connectable_enumerate() to get a #GSocketAddressEnumerator
 * to try out each socket address in turn until one succeeds, as shown
 * in the sample code below.
 *
 * |[
 * MyConnectionType *
 * connect_to_host (const char    *hostname,
 *                  guint16        port,
 *                  GCancellable  *cancellable,
 *                  GError       **error)
 * {
 *   MyConnection *conn = NULL;
 *   GSocketConnectable *addr;
 *   GSocketAddressEnumerator *enumerator;
 *   GSocketAddress *sockaddr;
 *   GError *conn_error = NULL;
 *
 *   addr = g_network_address_new ("www.gnome.org", 80);
 *   enumerator = g_socket_connectable_enumerate (addr);
 *   g_object_unref (addr);
 *
 *   /<!-- -->* Try each sockaddr until we succeed. Record the first
 *    * connection error, but not any further ones (since they'll probably
 *    * be basically the same as the first).
 *    *<!-- -->/
 *   while (!conn && (sockaddr = g_socket_address_enumerator_next (enumerator, cancellable, error))
 *     {
 *       conn = connect_to_sockaddr (sockaddr, conn_error ? NULL : &conn_error);
 *       g_object_unref (sockaddr);
 *     }
 *   g_object_unref (enumerator);
 *
 *   if (conn)
 *     {
 *       if (conn_error)
 *         {
 *           /<!-- -->* We couldn't connect to the first address, but we succeeded
 *            * in connecting to a later address.
 *            *<!-- -->/
 *           g_error_free (conn_error);
 *         }
 *       return conn;
 *     }
 *   else if (error)
 *     {
 *       /<!-- -->* Either the initial lookup failed, or else the caller
 *        * cancelled us.
 *        *<!-- -->/
 *       if (conn_error)
 *         g_error_free (conn_error);
 *       return NULL;
 *     }
 *   else
 *     {
 *       g_error_propagate (error, conn_error);
 *       return NULL;
 *     }
 * }
 * ]|
 */

GType
g_socket_connectable_get_type (void)
{
  static volatile gsize g_define_type_id__volatile = 0;

  if (g_once_init_enter (&g_define_type_id__volatile))
    {
      const GTypeInfo connectable_info =
      {
        sizeof (GSocketConnectableIface), /* class_size */
	NULL,		/* base_init */
	NULL,		/* base_finalize */
	NULL,
	NULL,		/* class_finalize */
	NULL,		/* class_data */
	0,
	0,              /* n_preallocs */
	NULL
      };
      GType g_define_type_id =
	g_type_register_static (G_TYPE_INTERFACE, I_("GSocketConnectable"),
				&connectable_info, 0);

      g_type_interface_add_prerequisite (g_define_type_id, G_TYPE_OBJECT);

      g_once_init_leave (&g_define_type_id__volatile, g_define_type_id);
    }

  return g_define_type_id__volatile;
}

/**
 * g_socket_connectable_enumerate:
 * @connectable: a #GSocketConnectable
 *
 * Creates a #GSocketAddressEnumerator for @connectable.
 *
 * Return value: a new #GSocketAddressEnumerator.
 *
 * Since: 2.22
 */
GSocketAddressEnumerator *
g_socket_connectable_enumerate (GSocketConnectable *connectable)
{
  GSocketConnectableIface *iface;

  g_return_val_if_fail (G_IS_SOCKET_CONNECTABLE (connectable), NULL);

  iface = G_SOCKET_CONNECTABLE_GET_IFACE (connectable);

  return (* iface->enumerate) (connectable);
}

#define __G_SOCKET_CONNECTABLE_C__
#include "gioaliasdef.c"
