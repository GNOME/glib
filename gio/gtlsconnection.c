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
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"
#include "glib.h"

#include "gtlsconnection.h"
#include "gcancellable.h"
#include "gioenumtypes.h"
#include "gio-marshal.h"
#include "gsocket.h"
#include "gtlsbackend.h"
#include "gtlscertificate.h"
#include "gtlsclientconnection.h"
#include "glibintl.h"

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

G_DEFINE_ABSTRACT_TYPE (GTlsConnection, g_tls_connection, G_TYPE_IO_STREAM)

static void g_tls_connection_get_property (GObject    *object,
					   guint       prop_id,
					   GValue     *value,
					   GParamSpec *pspec);
static void g_tls_connection_set_property (GObject      *object,
					   guint         prop_id,
					   const GValue *value,
					   GParamSpec   *pspec);
static void g_tls_connection_finalize     (GObject      *object);

static gboolean g_tls_connection_certificate_accumulator (GSignalInvocationHint *ihint,
							  GValue                *return_accu,
							  const GValue          *handler_return,
							  gpointer               dummy);

enum {
  NEED_CERTIFICATE,
  ACCEPT_CERTIFICATE,

  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

enum {
  PROP_0,
  PROP_BASE_IO_STREAM,
  PROP_REQUIRE_CLOSE_NOTIFY,
  PROP_REHANDSHAKE_MODE,
  PROP_CERTIFICATE,
  PROP_PEER_CERTIFICATE
};

struct _GTlsConnectionPrivate {
  GTlsCertificate *certificate, *peer_certificate;
};

static void
g_tls_connection_class_init (GTlsConnectionClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (GTlsConnectionPrivate));

  gobject_class->get_property = g_tls_connection_get_property;
  gobject_class->set_property = g_tls_connection_set_property;
  gobject_class->finalize = g_tls_connection_finalize;

  /**
   * GTlsConnection:base-io-stream:
   *
   * The #GIOStream that the connection wraps
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
						      G_PARAM_STATIC_STRINGS));
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
							P_("The connection's certificate"),
							G_TYPE_TLS_CERTIFICATE,
							G_PARAM_READWRITE |
							G_PARAM_STATIC_STRINGS));
  /**
   * GTlsConnection:peer-certificate:
   *
   * The connection's peer's certificate, after it has been set during
   * the TLS handshake.
   *
   * Since: 2.28
   */
  g_object_class_install_property (gobject_class, PROP_PEER_CERTIFICATE,
				   g_param_spec_object ("peer-certificate",
							P_("Peer Certificate"),
							P_("The connection's peer's certificate"),
							G_TYPE_TLS_CERTIFICATE,
							G_PARAM_READABLE |
							G_PARAM_STATIC_STRINGS));

  /**
   * GTlsConnection::need-certificate:
   * @conn: a #GTlsConnection
   *
   * Emitted during the TLS handshake if a certificate is needed and
   * one has not been set via g_tls_connection_set_certificate().
   *
   * For server-side connections, a certificate is always needed, and
   * the connection will fail if none is provided.
   *
   * For client-side connections, the signal will be emitted only if
   * the server has requested a certificate; you can call
   * g_tls_client_connection_get_accepted_cas() to get a list of
   * Certificate Authorities that the server will accept certificates
   * from. If you do not return a certificate (and have not provided
   * one via g_tls_connection_set_certificate()) then the server may
   * reject the handshake, in which case the operation will eventually
   * fail with %G_TLS_ERROR_CERTIFICATE_REQUIRED.
   *
   * Note that if this signal is emitted as part of asynchronous I/O
   * in the main thread, then you should not attempt to interact with
   * the user before returning from the signal handler. If you want to
   * let the user choose a certificate to return, you would have to
   * return %NULL from the signal handler on the first attempt, and
   * then after the connection attempt returns a
   * %G_TLS_ERROR_CERTIFICATE_REQUIRED, you can interact with the
   * user, create a new connection, and call
   * g_tls_connection_set_certificate() on it before handshaking (or
   * just connect to the signal again and return the certificate the
   * next time).
   *
   * If you are doing I/O in another thread, you do not
   * need to worry about this, and can simply block in the signal
   * handler until the UI thread returns an answer.
   *
   * Return value: the certificate to send to the peer, or %NULL to
   * send no certificate. If you return a certificate, the signal
   * emission will be stopped and further handlers will not be called.
   *
   * Since: 2.28
   */
  signals[NEED_CERTIFICATE] =
    g_signal_new (I_("need-certificate"),
		  G_TYPE_TLS_CONNECTION,
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GTlsConnectionClass, need_certificate),
		  g_tls_connection_certificate_accumulator, NULL,
		  _gio_marshal_OBJECT__VOID,
		  G_TYPE_TLS_CERTIFICATE, 0);

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
   * As with #GTlsConnection::need_certificate, you should not
   * interact with the user during the signal emission if the signal
   * was emitted as part of an asynchronous operation in the main
   * thread.
   *
   * Return value: %TRUE to accept @peer_cert (which will also
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
		  _gio_marshal_BOOLEAN__OBJECT_FLAGS,
		  G_TYPE_BOOLEAN, 2,
		  G_TYPE_TLS_CERTIFICATE,
		  G_TYPE_TLS_CERTIFICATE_FLAGS);
}

static void
g_tls_connection_init (GTlsConnection *conn)
{
  conn->priv = G_TYPE_INSTANCE_GET_PRIVATE (conn, G_TYPE_TLS_CONNECTION, GTlsConnectionPrivate);
}

static void
g_tls_connection_finalize (GObject *object)
{
  GTlsConnection *conn = G_TLS_CONNECTION (object);

  if (conn->priv->certificate)
    g_object_unref (conn->priv->certificate);
  if (conn->priv->peer_certificate)
    g_object_unref (conn->priv->peer_certificate);

  G_OBJECT_CLASS (g_tls_connection_parent_class)->finalize (object);
}

static void
g_tls_connection_get_property (GObject    *object,
			       guint       prop_id,
			       GValue     *value,
			       GParamSpec *pspec)
{
  GTlsConnection *conn = G_TLS_CONNECTION (object);

  switch (prop_id)
    {
    case PROP_CERTIFICATE:
      g_value_set_object (value, conn->priv->certificate);
      break;

    case PROP_PEER_CERTIFICATE:
      g_value_set_object (value, conn->priv->peer_certificate);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
g_tls_connection_set_property (GObject      *object,
			       guint         prop_id,
			       const GValue *value,
			       GParamSpec   *pspec)
{
  GTlsConnection *conn = G_TLS_CONNECTION (object);

  switch (prop_id)
    {
    case PROP_CERTIFICATE:
      g_tls_connection_set_certificate (conn, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/**
 * g_tls_connection_set_certificate:
 * @conn: a #GTlsConnection
 * @certificate: the certificate to use for @conn
 *
 * This sets the certificate that @conn will present to its peer
 * during the TLS handshake. If this is not set,
 * #GTlsConnection::need-certificate will be emitted during the
 * handshake if needed.
 *
 * Since: 2.28
 */
void
g_tls_connection_set_certificate (GTlsConnection  *conn,
				  GTlsCertificate *certificate)
{
  g_return_if_fail (G_IS_TLS_CONNECTION (conn));
  g_return_if_fail (G_IS_TLS_CERTIFICATE (certificate));

  if (conn->priv->certificate)
    g_object_unref (conn->priv->certificate);
  conn->priv->certificate = certificate ? g_object_ref (certificate) : NULL;
  g_object_notify (G_OBJECT (conn), "certificate");
}

/**
 * g_tls_connection_get_certificate:
 * @conn: a #GTlsConnection
 *
 * Gets @conn's certificate, as set by
 * g_tls_connection_set_certificate() or returned from one of the
 * signals.
 *
 * Return value: @conn's certificate, or %NULL
 *
 * Since: 2.28
 */
GTlsCertificate *
g_tls_connection_get_certificate (GTlsConnection *conn)
{
  g_return_val_if_fail (G_IS_TLS_CONNECTION (conn), NULL);

  return conn->priv->certificate;
}

/**
 * g_tls_connection_get_peer_certificate:
 * @conn: a #GTlsConnection
 *
 * Gets @conn's peer's certificate after it has been set during the
 * handshake.
 *
 * Return value: @conn's peer's certificate, or %NULL
 *
 * Since: 2.28
 */
GTlsCertificate *
g_tls_connection_get_peer_certificate (GTlsConnection *conn)
{
  g_return_val_if_fail (G_IS_TLS_CONNECTION (conn), NULL);

  return conn->priv->peer_certificate;
}

/**
 * g_tls_connection_set_require_close_notify:
 * @conn: a #GTlsConnection
 * @require_close_notify: whether or not to require close notification
 *
 * Sets whether or not @conn requires a proper TLS close notification
 * before closing the connection. If this is %TRUE (the default), then
 * calling g_io_stream_close() on @conn will send a TLS close
 * notification, and likewise it will expect to receive a close
 * notification before the connection is closed when reading, and will
 * return a %G_TLS_ERROR_EOF error if the connection is closed without
 * proper notification (since this may indicate a network error, or
 * man-in-the-middle attack).
 *
 * In some protocols, the application will know whether or not the
 * connection was closed cleanly based on application-level data
 * (because the application-level data includes a length field, or is
 * somehow self-delimiting); in this case, the close notify is
 * redundant and sometimes omitted. (TLS 1.1 explicitly allows this;
 * in TLS 1.0 it is technically an error, but often done anyway.) You
 * can use g_tls_connection_set_require_close_notify() to tell @conn to
 * allow an "unannounced" connection close, in which case it is up to
 * the application to check that the data has been fully received.
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
 * Tests whether or not @conn requires a proper TLS close notification
 * before closing the connection. See
 * g_tls_connection_set_require_close_notify() for details.
 *
 * Return value: %TRUE if @conn requires a proper TLS close
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
 * Sets how @conn behaves with respect to rehandshaking requests.
 *
 * %G_TLS_REHANDSHAKE_NEVER means that it will never agree to
 * rehandshake after the initial handshake is complete. (For a client,
 * this means it will refuse rehandshake requests from the server, and
 * for a server, this means it will close the connection with an error
 * if the client attempts to rehandshake.)
 *
 * %G_TLS_REHANDSHAKE_SAFELY means that the connection will allow a
 * rehandshake only if the other end of the connection supports the
 * TLS <literal>renegotiation_info</literal> extension. This is the
 * default behavior, but means that rehandshaking will not work
 * against older implementations that do not support that extension.
 *
 * %G_TLS_REHANDSHAKE_UNSAFELY means that the connection will allow
 * rehandshaking even without the
 * <literal>renegotiation_info</literal> extension. On the server side
 * in particular, this is not recommended, since it leaves the server
 * open to certain attacks. However, this mode is necessary if you
 * need to allow renegotiation with older client software.
 *
 * Since: 2.28
 */
void
g_tls_connection_set_rehandshake_mode (GTlsConnection       *conn,
				       GTlsRehandshakeMode   mode)
{
  g_return_if_fail (G_IS_TLS_CONNECTION (conn));

  g_object_set (G_OBJECT (conn),
		"rehandshake-mode", mode,
		NULL);
}

/**
 * g_tls_connection_get_rehandshake_mode:
 * @conn: a #GTlsConnection
 *
 * Gets @conn rehandshaking mode. See
 * g_tls_connection_set_rehandshake() for details.
 *
 * Return value: @conn's rehandshaking mode
 *
 * Since: 2.28
 */
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

/**
 * g_tls_connection_handshake:
 * @conn: a #GTlsConnection
 * @cancellable: a #GCancellable, or %NULL
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
 * However, you may call g_tls_connection_handshake() later on to
 * renegotiate parameters (encryption methods, etc) with the client.
 *
 * #GTlsConnection::accept_certificate and
 * #GTlsConnection::need_certificate may be emitted during the
 * handshake.
 *
 * Return value: success or failure
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
 * @io_priority: the <link linkend="io-priority">I/O priority</link>
 * of the request.
 * @cancellable: a #GCancellable, or %NULL
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

  return G_TLS_CONNECTION_GET_CLASS (conn)->handshake_async (conn, io_priority,
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
 * Return value: %TRUE on success, %FALSE on failure, in which
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
 * Return value: a #GQuark.
 *
 * Since: 2.28
 */
GQuark
g_tls_error_quark (void)
{
  return g_quark_from_static_string ("g-tls-error-quark");
}


static gboolean
g_tls_connection_certificate_accumulator (GSignalInvocationHint *ihint,
					  GValue                *return_accu,
					  const GValue          *handler_return,
					  gpointer               dummy)
{
  GTlsCertificate *cert;

  cert = g_value_get_object (handler_return);
  if (cert)
    g_value_set_object (return_accu, cert);

  return cert != NULL;
}

/**
 * g_tls_connection_emit_need_certificate:
 * @conn: a #GTlsConnection
 *
 * Used by #GTlsConnection implementations to emit the
 * #GTlsConnection::need-certificate signal.
 *
 * Returns: a new #GTlsCertificate
 *
 * Since: 2.28
 */
GTlsCertificate *
g_tls_connection_emit_need_certificate (GTlsConnection *conn)
{
  GTlsCertificate *cert = NULL;

  g_signal_emit (conn, signals[NEED_CERTIFICATE], 0,
		 &cert);
  return cert;
}

/**
 * g_tls_connection_emit_accept_certificate:
 * @conn: a #GTlsConnection
 * @peer_cert: the peer's #GTlsCertificate
 * @errors: the problems with @peer_cert
 *
 * Used by #GTlsConnection implementations to emit the
 * #GTlsConnection::accept-certificate signal.
 *
 * Return value: %TRUE if one of the signal handlers has returned
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

/**
 * g_tls_connection_set_peer_certificate:
 * @conn: a #GTlsConnection
 * @certificate: the peer certificate
 *
 * Used by #GTlsConnection implementations to set the connection's
 * peer certificate.
 *
 * Since: 2.28
 */
void
g_tls_connection_set_peer_certificate (GTlsConnection  *conn,
				       GTlsCertificate *certificate)
{
  if (conn->priv->peer_certificate)
    g_object_unref (conn->priv->peer_certificate);
  conn->priv->peer_certificate = certificate ? g_object_ref (certificate) : NULL;
  g_object_notify (G_OBJECT (conn), "peer-certificate");
}
