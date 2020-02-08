/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright © 2010 Red Hat, Inc
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
#include "glib.h"

#include "gtlsconnection.h"
#include "gcancellable.h"
#include "gioenumtypes.h"
#include "gsocket.h"
#include "gtlsbackend.h"
#include "gtlscertificate.h"
#include "gtlsclientconnection.h"
#include "gtlsdatabase.h"
#include "gtlsinteraction.h"
#include "glibintl.h"
#include "gmarshal-internal.h"

/**
 * SECTION:gtlsconnection
 * @short_description: TLS connection type
 * @include: gio/gio.h
 *
 * #GTlsConnection is the base TLS connection class type, which wraps
 * a #GIOStream and provides TLS encryption on top of it. Its
 * subclasses, #GTlsClientConnection and #GTlsServerConnection,
 * implement client-side and server-side TLS, respectively.
 *
 * For DTLS (Datagram TLS) support, see #GDtlsConnection.
 *
 * Since: 2.28
 */

/**
 * GTlsConnection:
 *
 * Abstract base class for the backend-specific #GTlsClientConnection
 * and #GTlsServerConnection types.
 *
 * Since: 2.28
 */

struct _GTlsConnectionPrivate
{
  gchar *negotiated_protocol;
};

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (GTlsConnection, g_tls_connection, G_TYPE_IO_STREAM)

static void g_tls_connection_get_property (GObject    *object,
					   guint       prop_id,
					   GValue     *value,
					   GParamSpec *pspec);
static void g_tls_connection_set_property (GObject      *object,
					   guint         prop_id,
					   const GValue *value,
					   GParamSpec   *pspec);
static void g_tls_connection_finalize (GObject *object);

enum {
  ACCEPT_CERTIFICATE,

  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

enum {
  PROP_0,
  PROP_BASE_IO_STREAM,
  PROP_REQUIRE_CLOSE_NOTIFY,
  PROP_REHANDSHAKE_MODE,
  PROP_USE_SYSTEM_CERTDB,
  PROP_DATABASE,
  PROP_INTERACTION,
  PROP_CERTIFICATE,
  PROP_PEER_CERTIFICATE,
  PROP_PEER_CERTIFICATE_ERRORS,
  PROP_ADVERTISED_PROTOCOLS,
  PROP_NEGOTIATED_PROTOCOL,
};

static void
g_tls_connection_class_init (GTlsConnectionClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->get_property = g_tls_connection_get_property;
  gobject_class->set_property = g_tls_connection_set_property;
  gobject_class->finalize = g_tls_connection_finalize;

  /**
   * GTlsConnection:base-io-stream:
   *
   * The #GIOStream that the connection wraps. The connection holds a reference
   * to this stream, and may run operations on the stream from other threads
   * throughout its lifetime. Consequently, after the #GIOStream has been
   * constructed, application code may only run its own operations on this
   * stream when no #GIOStream operations are running.
   *
   * Since: 2.28
   */
  g_object_class_install_property (gobject_class, PROP_BASE_IO_STREAM,
				   g_param_spec_object ("base-io-stream",
							P_("Base IOStream"),
							P_("The GIOStream that the connection wraps"),
							G_TYPE_IO_STREAM,
							G_PARAM_READWRITE |
							G_PARAM_CONSTRUCT_ONLY |
							G_PARAM_STATIC_STRINGS));
  /**
   * GTlsConnection:use-system-certdb:
   *
   * Whether or not the system certificate database will be used to
   * verify peer certificates. See
   * g_tls_connection_set_use_system_certdb().
   *
   * Deprecated: 2.30: Use GTlsConnection:database instead
   */
  g_object_class_install_property (gobject_class, PROP_USE_SYSTEM_CERTDB,
				   g_param_spec_boolean ("use-system-certdb",
							 P_("Use system certificate database"),
							 P_("Whether to verify peer certificates against the system certificate database"),
							 TRUE,
							 G_PARAM_READWRITE |
							 G_PARAM_CONSTRUCT |
							 G_PARAM_STATIC_STRINGS));
  /**
   * GTlsConnection:database:
   *
   * The certificate database to use when verifying this TLS connection.
   * If no certificate database is set, then the default database will be
   * used. See g_tls_backend_get_default_database().
   *
   * Since: 2.30
   */
  g_object_class_install_property (gobject_class, PROP_DATABASE,
				   g_param_spec_object ("database",
							 P_("Database"),
							 P_("Certificate database to use for looking up or verifying certificates"),
							 G_TYPE_TLS_DATABASE,
							 G_PARAM_READWRITE |
							 G_PARAM_STATIC_STRINGS));
  /**
   * GTlsConnection:interaction:
   *
   * A #GTlsInteraction object to be used when the connection or certificate
   * database need to interact with the user. This will be used to prompt the
   * user for passwords where necessary.
   *
   * Since: 2.30
   */
  g_object_class_install_property (gobject_class, PROP_INTERACTION,
                                   g_param_spec_object ("interaction",
                                                        P_("Interaction"),
                                                        P_("Optional object for user interaction"),
                                                        G_TYPE_TLS_INTERACTION,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));
  /**
   * GTlsConnection:require-close-notify:
   *
   * Whether or not proper TLS close notification is required.
   * See g_tls_connection_set_require_close_notify().
   *
   * Since: 2.28
   */
  g_object_class_install_property (gobject_class, PROP_REQUIRE_CLOSE_NOTIFY,
				   g_param_spec_boolean ("require-close-notify",
							 P_("Require close notify"),
							 P_("Whether to require proper TLS close notification"),
							 TRUE,
							 G_PARAM_READWRITE |
							 G_PARAM_CONSTRUCT |
							 G_PARAM_STATIC_STRINGS));
  /**
   * GTlsConnection:rehandshake-mode:
   *
   * The rehandshaking mode. See
   * g_tls_connection_set_rehandshake_mode().
   *
   * Since: 2.28
   */
  g_object_class_install_property (gobject_class, PROP_REHANDSHAKE_MODE,
				   g_param_spec_enum ("rehandshake-mode",
						      P_("Rehandshake mode"),
						      P_("When to allow rehandshaking"),
						      G_TYPE_TLS_REHANDSHAKE_MODE,
						      G_TLS_REHANDSHAKE_SAFELY,
						      G_PARAM_READWRITE |
						      G_PARAM_CONSTRUCT |
						      G_PARAM_STATIC_STRINGS |
						      G_PARAM_DEPRECATED));
  /**
   * GTlsConnection:certificate:
   *
   * The connection's certificate; see
   * g_tls_connection_set_certificate().
   *
   * Since: 2.28
   */
  g_object_class_install_property (gobject_class, PROP_CERTIFICATE,
				   g_param_spec_object ("certificate",
							P_("Certificate"),
							P_("The connection’s certificate"),
							G_TYPE_TLS_CERTIFICATE,
							G_PARAM_READWRITE |
							G_PARAM_STATIC_STRINGS));
  /**
   * GTlsConnection:peer-certificate:
   *
   * The connection's peer's certificate, after the TLS handshake has
   * completed and the certificate has been accepted. Note in
   * particular that this is not yet set during the emission of
   * #GTlsConnection::accept-certificate.
   *
   * (You can watch for a #GObject::notify signal on this property to
   * detect when a handshake has occurred.)
   *
   * Since: 2.28
   */
  g_object_class_install_property (gobject_class, PROP_PEER_CERTIFICATE,
				   g_param_spec_object ("peer-certificate",
							P_("Peer Certificate"),
							P_("The connection’s peer’s certificate"),
							G_TYPE_TLS_CERTIFICATE,
							G_PARAM_READABLE |
							G_PARAM_STATIC_STRINGS));
  /**
   * GTlsConnection:peer-certificate-errors:
   *
   * The errors noticed-and-ignored while verifying
   * #GTlsConnection:peer-certificate. Normally this should be 0, but
   * it may not be if #GTlsClientConnection:validation-flags is not
   * %G_TLS_CERTIFICATE_VALIDATE_ALL, or if
   * #GTlsConnection::accept-certificate overrode the default
   * behavior.
   *
   * Since: 2.28
   */
  g_object_class_install_property (gobject_class, PROP_PEER_CERTIFICATE_ERRORS,
				   g_param_spec_flags ("peer-certificate-errors",
						       P_("Peer Certificate Errors"),
						       P_("Errors found with the peer’s certificate"),
						       G_TYPE_TLS_CERTIFICATE_FLAGS,
						       0,
						       G_PARAM_READABLE |
						       G_PARAM_STATIC_STRINGS));
  /**
   * GTlsConnection:advertised-protocols:
   *
   * The list of application-layer protocols that the connection
   * advertises that it is willing to speak. See
   * g_tls_connection_set_advertised_protocols().
   *
   * Since: 2.60
   */
  g_object_class_install_property (gobject_class, PROP_ADVERTISED_PROTOCOLS,
                                   g_param_spec_boxed ("advertised-protocols",
                                                       P_("Advertised Protocols"),
                                                       P_("Application-layer protocols available on this connection"),
                                                       G_TYPE_STRV,
                                                       G_PARAM_READWRITE |
                                                       G_PARAM_STATIC_STRINGS));
  /**
   * GTlsConnection:negotiated-protocol:
   *
   * The application-layer protocol negotiated during the TLS
   * handshake. See g_tls_connection_get_negotiated_protocol().
   *
   * Since: 2.60
   */
  g_object_class_install_property (gobject_class, PROP_NEGOTIATED_PROTOCOL,
                                   g_param_spec_string ("negotiated-protocol",
                                                        P_("Negotiated Protocol"),
                                                        P_("Application-layer protocol negotiated for this connection"),
                                                        NULL,
                                                        G_PARAM_READABLE |
                                                        G_PARAM_STATIC_STRINGS));

  /**
   * GTlsConnection::accept-certificate:
   * @conn: a #GTlsConnection
   * @peer_cert: the peer's #GTlsCertificate
   * @errors: the problems with @peer_cert.
   *
   * Emitted during the TLS handshake after the peer certificate has
   * been received. You can examine @peer_cert's certification path by
   * calling g_tls_certificate_get_issuer() on it.
   *
   * For a client-side connection, @peer_cert is the server's
   * certificate, and the signal will only be emitted if the
   * certificate was not acceptable according to @conn's
   * #GTlsClientConnection:validation_flags. If you would like the
   * certificate to be accepted despite @errors, return %TRUE from the
   * signal handler. Otherwise, if no handler accepts the certificate,
   * the handshake will fail with %G_TLS_ERROR_BAD_CERTIFICATE.
   *
   * For a server-side connection, @peer_cert is the certificate
   * presented by the client, if this was requested via the server's
   * #GTlsServerConnection:authentication_mode. On the server side,
   * the signal is always emitted when the client presents a
   * certificate, and the certificate will only be accepted if a
   * handler returns %TRUE.
   *
   * Note that if this signal is emitted as part of asynchronous I/O
   * in the main thread, then you should not attempt to interact with
   * the user before returning from the signal handler. If you want to
   * let the user decide whether or not to accept the certificate, you
   * would have to return %FALSE from the signal handler on the first
   * attempt, and then after the connection attempt returns a
   * %G_TLS_ERROR_BAD_CERTIFICATE, you can interact with the user, and
   * if the user decides to accept the certificate, remember that fact,
   * create a new connection, and return %TRUE from the signal handler
   * the next time.
   *
   * If you are doing I/O in another thread, you do not
   * need to worry about this, and can simply block in the signal
   * handler until the UI thread returns an answer.
   *
   * Returns: %TRUE to accept @peer_cert (which will also
   * immediately end the signal emission). %FALSE to allow the signal
   * emission to continue, which will cause the handshake to fail if
   * no one else overrides it.
   *
   * Since: 2.28
   */
  signals[ACCEPT_CERTIFICATE] =
    g_signal_new (I_("accept-certificate"),
		  G_TYPE_TLS_CONNECTION,
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GTlsConnectionClass, accept_certificate),
		  g_signal_accumulator_true_handled, NULL,
		  _g_cclosure_marshal_BOOLEAN__OBJECT_FLAGS,
		  G_TYPE_BOOLEAN, 2,
		  G_TYPE_TLS_CERTIFICATE,
		  G_TYPE_TLS_CERTIFICATE_FLAGS);
  g_signal_set_va_marshaller (signals[ACCEPT_CERTIFICATE],
                              G_TYPE_FROM_CLASS (klass),
                              _g_cclosure_marshal_BOOLEAN__OBJECT_FLAGSv);
}

static void
g_tls_connection_init (GTlsConnection *conn)
{
}

static void
g_tls_connection_get_property (GObject    *object,
			       guint       prop_id,
			       GValue     *value,
			       GParamSpec *pspec)
{
  G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
}

static void
g_tls_connection_set_property (GObject      *object,
			       guint         prop_id,
			       const GValue *value,
			       GParamSpec   *pspec)
{
  G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
}

static void
g_tls_connection_finalize (GObject *object)
{
  GTlsConnection *conn = G_TLS_CONNECTION(object);
  GTlsConnectionPrivate *priv = g_tls_connection_get_instance_private (conn);

  g_clear_pointer (&priv->negotiated_protocol, g_free);

  G_OBJECT_CLASS (g_tls_connection_parent_class)->finalize (object);
}

/**
 * g_tls_connection_set_use_system_certdb:
 * @conn: a #GTlsConnection
 * @use_system_certdb: whether to use the system certificate database
 *
 * Sets whether @conn uses the system certificate database to verify
 * peer certificates. This is %TRUE by default. If set to %FALSE, then
 * peer certificate validation will always set the
 * %G_TLS_CERTIFICATE_UNKNOWN_CA error (meaning
 * #GTlsConnection::accept-certificate will always be emitted on
 * client-side connections, unless that bit is not set in
 * #GTlsClientConnection:validation-flags).
 *
 * Deprecated: 2.30: Use g_tls_connection_set_database() instead
 */
void
g_tls_connection_set_use_system_certdb (GTlsConnection *conn,
					gboolean        use_system_certdb)
{
  g_return_if_fail (G_IS_TLS_CONNECTION (conn));

  g_object_set (G_OBJECT (conn),
		"use-system-certdb", use_system_certdb,
		NULL);
}

/**
 * g_tls_connection_get_use_system_certdb:
 * @conn: a #GTlsConnection
 *
 * Gets whether @conn uses the system certificate database to verify
 * peer certificates. See g_tls_connection_set_use_system_certdb().
 *
 * Returns: whether @conn uses the system certificate database
 *
 * Deprecated: 2.30: Use g_tls_connection_get_database() instead
 */
gboolean
g_tls_connection_get_use_system_certdb (GTlsConnection *conn)
{
  gboolean use_system_certdb;

  g_return_val_if_fail (G_IS_TLS_CONNECTION (conn), TRUE);

  g_object_get (G_OBJECT (conn),
		"use-system-certdb", &use_system_certdb,
		NULL);
  return use_system_certdb;
}

/**
 * g_tls_connection_set_database:
 * @conn: a #GTlsConnection
 * @database: a #GTlsDatabase
 *
 * Sets the certificate database that is used to verify peer certificates.
 * This is set to the default database by default. See
 * g_tls_backend_get_default_database(). If set to %NULL, then
 * peer certificate validation will always set the
 * %G_TLS_CERTIFICATE_UNKNOWN_CA error (meaning
 * #GTlsConnection::accept-certificate will always be emitted on
 * client-side connections, unless that bit is not set in
 * #GTlsClientConnection:validation-flags).
 *
 * Since: 2.30
 */
void
g_tls_connection_set_database (GTlsConnection *conn,
                               GTlsDatabase   *database)
{
  g_return_if_fail (G_IS_TLS_CONNECTION (conn));
  g_return_if_fail (database == NULL || G_IS_TLS_DATABASE (database));

  g_object_set (G_OBJECT (conn),
		"database", database,
		NULL);
}

/**
 * g_tls_connection_get_database:
 * @conn: a #GTlsConnection
 *
 * Gets the certificate database that @conn uses to verify
 * peer certificates. See g_tls_connection_set_database().
 *
 * Returns: (transfer none): the certificate database that @conn uses or %NULL
 *
 * Since: 2.30
 */
GTlsDatabase*
g_tls_connection_get_database (GTlsConnection *conn)
{
  GTlsDatabase *database = NULL;

  g_return_val_if_fail (G_IS_TLS_CONNECTION (conn), NULL);

  g_object_get (G_OBJECT (conn),
		"database", &database,
		NULL);
  if (database)
    g_object_unref (database);
  return database;
}

/**
 * g_tls_connection_set_certificate:
 * @conn: a #GTlsConnection
 * @certificate: the certificate to use for @conn
 *
 * This sets the certificate that @conn will present to its peer
 * during the TLS handshake. For a #GTlsServerConnection, it is
 * mandatory to set this, and that will normally be done at construct
 * time.
 *
 * For a #GTlsClientConnection, this is optional. If a handshake fails
 * with %G_TLS_ERROR_CERTIFICATE_REQUIRED, that means that the server
 * requires a certificate, and if you try connecting again, you should
 * call this method first. You can call
 * g_tls_client_connection_get_accepted_cas() on the failed connection
 * to get a list of Certificate Authorities that the server will
 * accept certificates from.
 *
 * (It is also possible that a server will allow the connection with
 * or without a certificate; in that case, if you don't provide a
 * certificate, you can tell that the server requested one by the fact
 * that g_tls_client_connection_get_accepted_cas() will return
 * non-%NULL.)
 *
 * Since: 2.28
 */
void
g_tls_connection_set_certificate (GTlsConnection  *conn,
				  GTlsCertificate *certificate)
{
  g_return_if_fail (G_IS_TLS_CONNECTION (conn));
  g_return_if_fail (G_IS_TLS_CERTIFICATE (certificate));

  g_object_set (G_OBJECT (conn), "certificate", certificate, NULL);
}

/**
 * g_tls_connection_get_certificate:
 * @conn: a #GTlsConnection
 *
 * Gets @conn's certificate, as set by
 * g_tls_connection_set_certificate().
 *
 * Returns: (transfer none): @conn's certificate, or %NULL
 *
 * Since: 2.28
 */
GTlsCertificate *
g_tls_connection_get_certificate (GTlsConnection *conn)
{
  GTlsCertificate *certificate;

  g_return_val_if_fail (G_IS_TLS_CONNECTION (conn), NULL);

  g_object_get (G_OBJECT (conn), "certificate", &certificate, NULL);
  if (certificate)
    g_object_unref (certificate);

  return certificate;
}

/**
 * g_tls_connection_set_interaction:
 * @conn: a connection
 * @interaction: (nullable): an interaction object, or %NULL
 *
 * Set the object that will be used to interact with the user. It will be used
 * for things like prompting the user for passwords.
 *
 * The @interaction argument will normally be a derived subclass of
 * #GTlsInteraction. %NULL can also be provided if no user interaction
 * should occur for this connection.
 *
 * Since: 2.30
 */
void
g_tls_connection_set_interaction (GTlsConnection       *conn,
                                  GTlsInteraction      *interaction)
{
  g_return_if_fail (G_IS_TLS_CONNECTION (conn));
  g_return_if_fail (interaction == NULL || G_IS_TLS_INTERACTION (interaction));

  g_object_set (G_OBJECT (conn), "interaction", interaction, NULL);
}

/**
 * g_tls_connection_get_interaction:
 * @conn: a connection
 *
 * Get the object that will be used to interact with the user. It will be used
 * for things like prompting the user for passwords. If %NULL is returned, then
 * no user interaction will occur for this connection.
 *
 * Returns: (transfer none): The interaction object.
 *
 * Since: 2.30
 */
GTlsInteraction *
g_tls_connection_get_interaction (GTlsConnection       *conn)
{
  GTlsInteraction *interaction = NULL;

  g_return_val_if_fail (G_IS_TLS_CONNECTION (conn), NULL);

  g_object_get (G_OBJECT (conn), "interaction", &interaction, NULL);
  if (interaction)
    g_object_unref (interaction);

  return interaction;
}

/**
 * g_tls_connection_get_peer_certificate:
 * @conn: a #GTlsConnection
 *
 * Gets @conn's peer's certificate after the handshake has completed.
 * (It is not set during the emission of
 * #GTlsConnection::accept-certificate.)
 *
 * Returns: (transfer none): @conn's peer's certificate, or %NULL
 *
 * Since: 2.28
 */
GTlsCertificate *
g_tls_connection_get_peer_certificate (GTlsConnection *conn)
{
  GTlsCertificate *peer_certificate;

  g_return_val_if_fail (G_IS_TLS_CONNECTION (conn), NULL);

  g_object_get (G_OBJECT (conn), "peer-certificate", &peer_certificate, NULL);
  if (peer_certificate)
    g_object_unref (peer_certificate);

  return peer_certificate;
}

/**
 * g_tls_connection_get_peer_certificate_errors:
 * @conn: a #GTlsConnection
 *
 * Gets the errors associated with validating @conn's peer's
 * certificate, after the handshake has completed. (It is not set
 * during the emission of #GTlsConnection::accept-certificate.)
 *
 * Returns: @conn's peer's certificate errors
 *
 * Since: 2.28
 */
GTlsCertificateFlags
g_tls_connection_get_peer_certificate_errors (GTlsConnection *conn)
{
  GTlsCertificateFlags errors;

  g_return_val_if_fail (G_IS_TLS_CONNECTION (conn), 0);

  g_object_get (G_OBJECT (conn), "peer-certificate-errors", &errors, NULL);
  return errors;
}

/**
 * g_tls_connection_set_require_close_notify:
 * @conn: a #GTlsConnection
 * @require_close_notify: whether or not to require close notification
 *
 * Sets whether or not @conn expects a proper TLS close notification
 * before the connection is closed. If this is %TRUE (the default),
 * then @conn will expect to receive a TLS close notification from its
 * peer before the connection is closed, and will return a
 * %G_TLS_ERROR_EOF error if the connection is closed without proper
 * notification (since this may indicate a network error, or
 * man-in-the-middle attack).
 *
 * In some protocols, the application will know whether or not the
 * connection was closed cleanly based on application-level data
 * (because the application-level data includes a length field, or is
 * somehow self-delimiting); in this case, the close notify is
 * redundant and sometimes omitted. (TLS 1.1 explicitly allows this;
 * in TLS 1.0 it is technically an error, but often done anyway.) You
 * can use g_tls_connection_set_require_close_notify() to tell @conn
 * to allow an "unannounced" connection close, in which case the close
 * will show up as a 0-length read, as in a non-TLS
 * #GSocketConnection, and it is up to the application to check that
 * the data has been fully received.
 *
 * Note that this only affects the behavior when the peer closes the
 * connection; when the application calls g_io_stream_close() itself
 * on @conn, this will send a close notification regardless of the
 * setting of this property. If you explicitly want to do an unclean
 * close, you can close @conn's #GTlsConnection:base-io-stream rather
 * than closing @conn itself, but note that this may only be done when no other
 * operations are pending on @conn or the base I/O stream.
 *
 * Since: 2.28
 */
void
g_tls_connection_set_require_close_notify (GTlsConnection *conn,
					   gboolean        require_close_notify)
{
  g_return_if_fail (G_IS_TLS_CONNECTION (conn));

  g_object_set (G_OBJECT (conn),
		"require-close-notify", require_close_notify,
		NULL);
}

/**
 * g_tls_connection_get_require_close_notify:
 * @conn: a #GTlsConnection
 *
 * Tests whether or not @conn expects a proper TLS close notification
 * when the connection is closed. See
 * g_tls_connection_set_require_close_notify() for details.
 *
 * Returns: %TRUE if @conn requires a proper TLS close
 * notification.
 *
 * Since: 2.28
 */
gboolean
g_tls_connection_get_require_close_notify (GTlsConnection *conn)
{
  gboolean require_close_notify;

  g_return_val_if_fail (G_IS_TLS_CONNECTION (conn), TRUE);

  g_object_get (G_OBJECT (conn),
		"require-close-notify", &require_close_notify,
		NULL);
  return require_close_notify;
}

/**
 * g_tls_connection_set_rehandshake_mode:
 * @conn: a #GTlsConnection
 * @mode: the rehandshaking mode
 *
 * Sets how @conn behaves with respect to rehandshaking requests, when
 * TLS 1.2 or older is in use.
 *
 * %G_TLS_REHANDSHAKE_NEVER means that it will never agree to
 * rehandshake after the initial handshake is complete. (For a client,
 * this means it will refuse rehandshake requests from the server, and
 * for a server, this means it will close the connection with an error
 * if the client attempts to rehandshake.)
 *
 * %G_TLS_REHANDSHAKE_SAFELY means that the connection will allow a
 * rehandshake only if the other end of the connection supports the
 * TLS `renegotiation_info` extension. This is the default behavior,
 * but means that rehandshaking will not work against older
 * implementations that do not support that extension.
 *
 * %G_TLS_REHANDSHAKE_UNSAFELY means that the connection will allow
 * rehandshaking even without the `renegotiation_info` extension. On
 * the server side in particular, this is not recommended, since it
 * leaves the server open to certain attacks. However, this mode is
 * necessary if you need to allow renegotiation with older client
 * software.
 *
 * Since: 2.28
 *
 * Deprecated: 2.60. Changing the rehandshake mode is no longer
 *   required for compatibility. Also, rehandshaking has been removed
 *   from the TLS protocol in TLS 1.3.
 */
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
void
g_tls_connection_set_rehandshake_mode (GTlsConnection       *conn,
				       GTlsRehandshakeMode   mode)
{
  g_return_if_fail (G_IS_TLS_CONNECTION (conn));

  g_object_set (G_OBJECT (conn),
		"rehandshake-mode", mode,
		NULL);
}
G_GNUC_END_IGNORE_DEPRECATIONS

/**
 * g_tls_connection_get_rehandshake_mode:
 * @conn: a #GTlsConnection
 *
 * Gets @conn rehandshaking mode. See
 * g_tls_connection_set_rehandshake_mode() for details.
 *
 * Returns: @conn's rehandshaking mode
 *
 * Since: 2.28
 *
 * Deprecated: 2.60. Changing the rehandshake mode is no longer
 *   required for compatibility. Also, rehandshaking has been removed
 *   from the TLS protocol in TLS 1.3.
 */
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
GTlsRehandshakeMode
g_tls_connection_get_rehandshake_mode (GTlsConnection       *conn)
{
  GTlsRehandshakeMode mode;

  g_return_val_if_fail (G_IS_TLS_CONNECTION (conn), G_TLS_REHANDSHAKE_NEVER);

  g_object_get (G_OBJECT (conn),
		"rehandshake-mode", &mode,
		NULL);
  return mode;
}
G_GNUC_END_IGNORE_DEPRECATIONS

/**
 * g_tls_connection_set_advertised_protocols:
 * @conn: a #GTlsConnection
 * @protocols: (array zero-terminated=1) (nullable): a %NULL-terminated
 *   array of ALPN protocol names (eg, "http/1.1", "h2"), or %NULL
 *
 * Sets the list of application-layer protocols to advertise that the
 * caller is willing to speak on this connection. The
 * Application-Layer Protocol Negotiation (ALPN) extension will be
 * used to negotiate a compatible protocol with the peer; use
 * g_tls_connection_get_negotiated_protocol() to find the negotiated
 * protocol after the handshake.  Specifying %NULL for the the value
 * of @protocols will disable ALPN negotiation.
 *
 * See [IANA TLS ALPN Protocol IDs](https://www.iana.org/assignments/tls-extensiontype-values/tls-extensiontype-values.xhtml#alpn-protocol-ids)
 * for a list of registered protocol IDs.
 *
 * Since: 2.60
 */
void
g_tls_connection_set_advertised_protocols (GTlsConnection      *conn,
                                           const gchar * const *protocols)
{
  g_return_if_fail (G_IS_TLS_CONNECTION (conn));

  g_object_set (G_OBJECT (conn),
                "advertised-protocols", protocols,
                NULL);
}

/**
 * g_tls_connection_get_negotiated_protocol:
 * @conn: a #GTlsConnection
 *
 * Gets the name of the application-layer protocol negotiated during
 * the handshake.
 *
 * If the peer did not use the ALPN extension, or did not advertise a
 * protocol that matched one of @conn's protocols, or the TLS backend
 * does not support ALPN, then this will be %NULL. See
 * g_tls_connection_set_advertised_protocols().
 *
 * Returns: (nullable): the negotiated protocol, or %NULL
 *
 * Since: 2.60
 */
const gchar *
g_tls_connection_get_negotiated_protocol (GTlsConnection *conn)
{
  GTlsConnectionPrivate *priv;
  gchar *protocol;

  g_return_val_if_fail (G_IS_TLS_CONNECTION (conn), NULL);

  g_object_get (G_OBJECT (conn),
                "negotiated-protocol", &protocol,
                NULL);

  /*
   * Cache the property internally so we can return a `const` pointer
   * to the caller.
   */
  priv = g_tls_connection_get_instance_private (conn);
  if (g_strcmp0 (priv->negotiated_protocol, protocol) != 0)
    {
      g_free (priv->negotiated_protocol);
      priv->negotiated_protocol = protocol;
    }
  else
    {
      g_free (protocol);
    }

  return priv->negotiated_protocol;
}

/**
 * g_tls_connection_handshake:
 * @conn: a #GTlsConnection
 * @cancellable: (nullable): a #GCancellable, or %NULL
 * @error: a #GError, or %NULL
 *
 * Attempts a TLS handshake on @conn.
 *
 * On the client side, it is never necessary to call this method;
 * although the connection needs to perform a handshake after
 * connecting (or after sending a "STARTTLS"-type command) and may
 * need to rehandshake later if the server requests it,
 * #GTlsConnection will handle this for you automatically when you try
 * to send or receive data on the connection. However, you can call
 * g_tls_connection_handshake() manually if you want to know for sure
 * whether the initial handshake succeeded or failed (as opposed to
 * just immediately trying to write to @conn's output stream, in which
 * case if it fails, it may not be possible to tell if it failed
 * before or after completing the handshake).
 *
 * Likewise, on the server side, although a handshake is necessary at
 * the beginning of the communication, you do not need to call this
 * function explicitly unless you want clearer error reporting.
 *
 * If TLS 1.2 or older is in use, you may call
 * g_tls_connection_handshake() after the initial handshake to
 * rehandshake; however, this usage is deprecated because rehandshaking
 * is no longer part of the TLS protocol in TLS 1.3. Accordingly, the
 * behavior of calling this function after the initial handshake is now
 * undefined, except it is guaranteed to be reasonable and
 * nondestructive so as to preserve compatibility with code written for
 * older versions of GLib.
 *
 * #GTlsConnection::accept_certificate may be emitted during the
 * handshake.
 *
 * Returns: success or failure
 *
 * Since: 2.28
 */
gboolean
g_tls_connection_handshake (GTlsConnection   *conn,
			    GCancellable     *cancellable,
			    GError          **error)
{
  g_return_val_if_fail (G_IS_TLS_CONNECTION (conn), FALSE);

  return G_TLS_CONNECTION_GET_CLASS (conn)->handshake (conn, cancellable, error);
}

/**
 * g_tls_connection_handshake_async:
 * @conn: a #GTlsConnection
 * @io_priority: the [I/O priority][io-priority] of the request
 * @cancellable: (nullable): a #GCancellable, or %NULL
 * @callback: callback to call when the handshake is complete
 * @user_data: the data to pass to the callback function
 *
 * Asynchronously performs a TLS handshake on @conn. See
 * g_tls_connection_handshake() for more information.
 *
 * Since: 2.28
 */
void
g_tls_connection_handshake_async (GTlsConnection       *conn,
				  int                   io_priority,
				  GCancellable         *cancellable,
				  GAsyncReadyCallback   callback,
				  gpointer              user_data)
{
  g_return_if_fail (G_IS_TLS_CONNECTION (conn));

  G_TLS_CONNECTION_GET_CLASS (conn)->handshake_async (conn, io_priority,
						      cancellable,
						      callback, user_data);
}

/**
 * g_tls_connection_handshake_finish:
 * @conn: a #GTlsConnection
 * @result: a #GAsyncResult.
 * @error: a #GError pointer, or %NULL
 *
 * Finish an asynchronous TLS handshake operation. See
 * g_tls_connection_handshake() for more information.
 *
 * Returns: %TRUE on success, %FALSE on failure, in which
 * case @error will be set.
 *
 * Since: 2.28
 */
gboolean
g_tls_connection_handshake_finish (GTlsConnection  *conn,
				   GAsyncResult    *result,
				   GError         **error)
{
  g_return_val_if_fail (G_IS_TLS_CONNECTION (conn), FALSE);

  return G_TLS_CONNECTION_GET_CLASS (conn)->handshake_finish (conn, result, error);
}

/**
 * g_tls_error_quark:
 *
 * Gets the TLS error quark.
 *
 * Returns: a #GQuark.
 *
 * Since: 2.28
 */
G_DEFINE_QUARK (g-tls-error-quark, g_tls_error)

/**
 * g_tls_connection_emit_accept_certificate:
 * @conn: a #GTlsConnection
 * @peer_cert: the peer's #GTlsCertificate
 * @errors: the problems with @peer_cert
 *
 * Used by #GTlsConnection implementations to emit the
 * #GTlsConnection::accept-certificate signal.
 *
 * Returns: %TRUE if one of the signal handlers has returned
 *     %TRUE to accept @peer_cert
 *
 * Since: 2.28
 */
gboolean
g_tls_connection_emit_accept_certificate (GTlsConnection       *conn,
					  GTlsCertificate      *peer_cert,
					  GTlsCertificateFlags  errors)
{
  gboolean accept = FALSE;

  g_signal_emit (conn, signals[ACCEPT_CERTIFICATE], 0,
		 peer_cert, errors, &accept);
  return accept;
}
