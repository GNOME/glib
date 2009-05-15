/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright © 2008, 2009 Codethink Limited
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2 of the licence or (at
 * your option) any later version.
 *
 * See the included COPYING file for more information.
 */

/**
 * SECTION: gtcpconnection
 * @title: GTcpConnection
 * @short_description: a TCP #GSocketConnection
 * @see_also: #GSocketConnection.
 *
 * This is the subclass of #GSocketConnection that is created
 * for TCP/IP sockets.
 *
 * It is currently empty; it offers no additional functionality
 * over its base class.
 *
 * Eventually, some TCP-specific socket stuff will be added.
 *
 * Since: 2.22
 **/

#include "config.h"
#include "gtcpconnection.h"
#include "glibintl.h"

#include "gioalias.h"

G_DEFINE_TYPE_WITH_CODE (GTcpConnection, g_tcp_connection,
			 G_TYPE_SOCKET_CONNECTION,
  g_socket_connection_factory_register_type (g_define_type_id,
					     G_SOCKET_FAMILY_IPV4,
					     G_SOCKET_TYPE_STREAM,
					     0);
  g_socket_connection_factory_register_type (g_define_type_id,
					     G_SOCKET_FAMILY_IPV6,
					     G_SOCKET_TYPE_STREAM,
					     0);
  g_socket_connection_factory_register_type (g_define_type_id,
					     G_SOCKET_FAMILY_IPV4,
					     G_SOCKET_TYPE_STREAM,
					     g_socket_protocol_id_lookup_by_name ("tcp"));
  g_socket_connection_factory_register_type (g_define_type_id,
					     G_SOCKET_FAMILY_IPV6,
					     G_SOCKET_TYPE_STREAM,
					     g_socket_protocol_id_lookup_by_name ("tcp"));
			 );

static void
g_tcp_connection_init (GTcpConnection *connection)
{
}

static void
g_tcp_connection_class_init (GTcpConnectionClass *class)
{
}

#define __G_TCP_CONNECTION_C__
#include "gioaliasdef.c"
