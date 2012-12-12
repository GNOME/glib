/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright © 2010 Red Hat, Inc
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
#include "glib.h"

#include "gtlsserverconnection.h"
#include "ginitable.h"
#include "gioenumtypes.h"
#include "gsocket.h"
#include "gtlsbackend.h"
#include "gtlscertificate.h"
#include "glibintl.h"

/**
 * SECTION:gtlsserverconnection
 * @short_description: TLS server-side connection
 * @include: gio/gio.h
 *
 * #GTlsServerConnection is the server-side subclass of #GTlsConnection,
 * representing a server-side TLS connection.
 *
 * Since: 2.28
 */

G_DEFINE_INTERFACE (GTlsServerConnection, g_tls_server_connection, G_TYPE_TLS_CONNECTION)

static void
g_tls_server_connection_default_init (GTlsServerConnectionInterface *iface)
{
  /**
   * GTlsServerConnection:authentication-mode:
   *
   * The #GTlsAuthenticationMode for the server. This can be changed
   * before calling g_tls_connection_handshake() if you want to
   * rehandshake with a different mode from the initial handshake.
   *
   * Since: 2.28
   */
  g_object_interface_install_property (iface,
				       g_param_spec_enum ("authentication-mode",
							  P_("Authentication Mode"),
							  P_("The client authentication mode"),
							  G_TYPE_TLS_AUTHENTICATION_MODE,
							  G_TLS_AUTHENTICATION_NONE,
							  G_PARAM_READWRITE |
							  G_PARAM_STATIC_STRINGS));

  /**
   * GTlsServerConnection:server-identity:
   *
   * The server identity chosen by the client via the SNI extension.
   * If the client sends that extension in the handshake, this
   * property will be updated when it is parsed.
   *
   * You can connect to #GObject::notify for this property to be
   * notified when this is set, and then call
   * g_tls_connection_set_certificate() to set an appropriate
   * certificate to send in reply. Beware that the notification may be
   * emitted in a different thread from the one that you started the
   * handshake in (but, as long as you are not also getting or setting
   * the certificate from another thread, it is safe to call
   * g_tls_connection_set_certificate() from that thread).
   *
   * Since: 2.46
   */
  g_object_interface_install_property (iface,
				       g_param_spec_string ("server-identity",
                                                            P_("Server Identity"),
                                                            P_("The server identity requested by the client"),
                                                            NULL,
                                                            G_PARAM_READABLE |
                                                            G_PARAM_STATIC_STRINGS));
}

/**
 * g_tls_server_connection_new:
 * @base_io_stream: the #GIOStream to wrap
 * @certificate: (allow-none): the default server certificate, or %NULL
 * @error: #GError for error reporting, or %NULL to ignore.
 *
 * Creates a new #GTlsServerConnection wrapping @base_io_stream (which
 * must have pollable input and output streams).
 *
 * Returns: (transfer full) (type GTlsServerConnection): the new
 * #GTlsServerConnection, or %NULL on error
 *
 * Since: 2.28
 */
GIOStream *
g_tls_server_connection_new (GIOStream        *base_io_stream,
			     GTlsCertificate  *certificate,
			     GError          **error)
{
  GObject *conn;
  GTlsBackend *backend;

  backend = g_tls_backend_get_default ();
  conn = g_initable_new (g_tls_backend_get_server_connection_type (backend),
			 NULL, error,
			 "base-io-stream", base_io_stream,
			 "certificate", certificate,
			 NULL);
  return G_IO_STREAM (conn);
}

/**
 * g_tls_server_connection_get_server_identity:
 * @conn: a #GTlsServerConnection
 *
 * Gets the server identity requested by the client via the SNI
 * extension, after it has been set during the handshake.
 *
 * Return value: the requested server identity, or %NULL if the
 *   client didn't use SNI.
 *
 * Since: 2.46
 */
const gchar *
g_tls_server_connection_get_server_identity (GTlsServerConnection *conn)
{
  if (G_TLS_SERVER_CONNECTION_GET_INTERFACE (conn)->get_server_identity)
    return G_TLS_SERVER_CONNECTION_GET_INTERFACE (conn)->get_server_identity (conn);
  else
    return NULL;
}
