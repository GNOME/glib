/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright © 2008 Christian Kellner, Samuel Cormier-Iijima
 *           © 2008 codethink
 * Copyright © 2009 Red Hat, Inc
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
 * Authors: Christian Kellner <gicmo@gnome.org>
 *          Samuel Cormier-Iijima <sciyoshi@gmail.com>
 *          Ryan Lortie <desrt@desrt.ca>
 *          Alexander Larsson <alexl@redhat.com>
 */

#include "config.h"

#include "gsocketconnection.h"

#include "gsocketoutputstream.h"
#include "gsocketinputstream.h"
#include <gio/giostream.h>
#include <gio/gsimpleasyncresult.h>
#include "gunixconnection.h"
#include "gtcpconnection.h"
#include "glibintl.h"


/**
 * SECTION:gsocketconnection
 * @short_description: A socket connection
 * @include: gio/gio.h
 * @see_also: #GIOStream, #GSocketClient, #GSocketListener
 *
 * #GSocketConnection is a #GIOStream for a connected socket. They
 * can be created either by #GSocketClient when connecting to a host,
 * or by #GSocketListener when accepting a new client.
 *
 * The type of the #GSocketConnection object returned from these calls
 * depends on the type of the underlying socket that is in use. For
 * instance, for a TCP/IP connection it will be a #GTcpConnection.
 *
 * Choosing what type of object to construct is done with the socket
 * connection factory, and it is possible for 3rd parties to register
 * custom socket connection types for specific combination of socket
 * family/type/protocol using g_socket_connection_factory_register_type().
 *
 * Since: 2.22
 */

G_DEFINE_TYPE (GSocketConnection, g_socket_connection, G_TYPE_IO_STREAM);

enum
{
  PROP_NONE,
  PROP_SOCKET,
};

struct _GSocketConnectionPrivate
{
  GSocket       *socket;
  GInputStream  *input_stream;
  GOutputStream *output_stream;

  gboolean       in_dispose;
};

static gboolean g_socket_connection_close         (GIOStream            *stream,
						   GCancellable         *cancellable,
						   GError              **error);
static void     g_socket_connection_close_async   (GIOStream            *stream,
						   int                   io_priority,
						   GCancellable         *cancellable,
						   GAsyncReadyCallback   callback,
						   gpointer              user_data);
static gboolean g_socket_connection_close_finish  (GIOStream            *stream,
						   GAsyncResult         *result,
						   GError              **error);

static GInputStream *
g_socket_connection_get_input_stream (GIOStream *io_stream)
{
  GSocketConnection *connection = G_SOCKET_CONNECTION (io_stream);

  if (connection->priv->input_stream == NULL)
    connection->priv->input_stream = (GInputStream *)
      _g_socket_input_stream_new (connection->priv->socket);

  return connection->priv->input_stream;
}

static GOutputStream *
g_socket_connection_get_output_stream (GIOStream *io_stream)
{
  GSocketConnection *connection = G_SOCKET_CONNECTION (io_stream);

  if (connection->priv->output_stream == NULL)
    connection->priv->output_stream = (GOutputStream *)
      _g_socket_output_stream_new (connection->priv->socket);

  return connection->priv->output_stream;
}

/**
 * g_socket_connection_is_connected:
 * @connection: a #GSocketConnection
 *
 * Checks if @connection is connected. This is equivalent to calling
 * g_socket_is_connected() on @connection's underlying #GSocket.
 *
 * Returns: whether @connection is connected
 *
 * Since: 2.32
 */
gboolean
g_socket_connection_is_connected (GSocketConnection  *connection)
{
  return g_socket_is_connected (connection->priv->socket);
}

/**
 * g_socket_connection_connect:
 * @connection: a #GSocketConnection
 * @address: a #GSocketAddress specifying the remote address.
 * @cancellable: (allow-none): a %GCancellable or %NULL
 * @error: #GError for error reporting, or %NULL to ignore.
 *
 * Connect @connection to the specified remote address.
 *
 * Returns: %TRUE if the connection succeeded, %FALSE on error
 *
 * Since: 2.32
 */
gboolean
g_socket_connection_connect (GSocketConnection  *connection,
			     GSocketAddress     *address,
			     GCancellable       *cancellable,
			     GError            **error)
{
  g_return_val_if_fail (G_IS_SOCKET_CONNECTION (connection), FALSE);
  g_return_val_if_fail (G_IS_SOCKET_ADDRESS (address), FALSE);

  return g_socket_connect (connection->priv->socket, address,
			   cancellable, error);
}

static gboolean g_socket_connection_connect_callback (GSocket      *socket,
						      GIOCondition  condition,
						      gpointer      user_data);

/**
 * g_socket_connection_connect_async:
 * @connection: a #GSocketConnection
 * @address: a #GSocketAddress specifying the remote address.
 * @cancellable: (allow-none): a %GCancellable or %NULL
 * @callback: (scope async): a #GAsyncReadyCallback
 * @user_data: (closure): user data for the callback
 *
 * Asynchronously connect @connection to the specified remote address.
 *
 * This clears the #GSocket:blocking flag on @connection's underlying
 * socket if it is currently set.
 *
 * Use g_socket_connection_connect_finish() to retrieve the result.
 *
 * Since: 2.32
 */
void
g_socket_connection_connect_async (GSocketConnection   *connection,
				   GSocketAddress      *address,
				   GCancellable        *cancellable,
				   GAsyncReadyCallback  callback,
				   gpointer             user_data)
{
  GSimpleAsyncResult *simple;
  GError *tmp_error = NULL;

  g_return_if_fail (G_IS_SOCKET_CONNECTION (connection));
  g_return_if_fail (G_IS_SOCKET_ADDRESS (address));

  simple = g_simple_async_result_new (G_OBJECT (connection),
				      callback, user_data,
				      g_socket_connection_connect_async);

  g_socket_set_blocking (connection->priv->socket, FALSE);

  if (g_socket_connect (connection->priv->socket, address,
			cancellable, &tmp_error))
    {
      g_simple_async_result_set_op_res_gboolean (simple, TRUE);
      g_simple_async_result_complete_in_idle (simple);
    }
  else if (g_error_matches (tmp_error, G_IO_ERROR, G_IO_ERROR_PENDING))
    {
      GSource *source;

      g_error_free (tmp_error);
      source = g_socket_create_source (connection->priv->socket,
				       G_IO_OUT, cancellable);
      g_source_set_callback (source,
			     (GSourceFunc) g_socket_connection_connect_callback,
			     simple, NULL);
      g_source_attach (source, g_main_context_get_thread_default ());
      g_source_unref (source);
    }
  else
    {
      g_simple_async_result_take_error (simple, tmp_error);
      g_simple_async_result_complete_in_idle (simple);
    }
}

static gboolean
g_socket_connection_connect_callback (GSocket      *socket,
				      GIOCondition  condition,
				      gpointer      user_data)
{
  GSimpleAsyncResult *simple = user_data;
  GSocketConnection *connection;
  GError *error = NULL;

  connection = G_SOCKET_CONNECTION (g_async_result_get_source_object (G_ASYNC_RESULT (simple)));
  g_object_unref (connection);

  if (g_socket_check_connect_result (connection->priv->socket, &error))
    g_simple_async_result_set_op_res_gboolean (simple, TRUE);
  else
    g_simple_async_result_take_error (simple, error);

  g_simple_async_result_complete (simple);
  g_object_unref (simple);
  return FALSE;
}

/**
 * g_socket_connection_connect_finish:
 * @connection: a #GSocketConnection
 * @result: the #GAsyncResult
 * @error: #GError for error reporting, or %NULL to ignore.
 *
 * Gets the result of a g_socket_connection_connect_async() call.
 *
 * Returns: %TRUE if the connection succeeded, %FALSE on error
 *
 * Since: 2.32
 */
gboolean
g_socket_connection_connect_finish (GSocketConnection  *connection,
				    GAsyncResult       *result,
				    GError            **error)
{
  GSimpleAsyncResult *simple;

  g_return_val_if_fail (G_IS_SOCKET_CONNECTION (connection), FALSE);
  g_return_val_if_fail (g_simple_async_result_is_valid (result, G_OBJECT (connection), g_socket_connection_connect_async), FALSE);

  simple = G_SIMPLE_ASYNC_RESULT (result);
  if (g_simple_async_result_propagate_error (simple, error))
    return FALSE;
  return TRUE;
}

/**
 * g_socket_connection_get_socket:
 * @connection: a #GSocketConnection
 *
 * Gets the underlying #GSocket object of the connection.
 * This can be useful if you want to do something unusual on it
 * not supported by the #GSocketConnection APIs.
 *
 * Returns: (transfer none): a #GSocketAddress or %NULL on error.
 *
 * Since: 2.22
 */
GSocket *
g_socket_connection_get_socket (GSocketConnection *connection)
{
  g_return_val_if_fail (G_IS_SOCKET_CONNECTION (connection), NULL);

  return connection->priv->socket;
}

/**
 * g_socket_connection_get_local_address:
 * @connection: a #GSocketConnection
 * @error: #GError for error reporting, or %NULL to ignore.
 *
 * Try to get the local address of a socket connection.
 *
 * Returns: (transfer full): a #GSocketAddress or %NULL on error.
 *     Free the returned object with g_object_unref().
 *
 * Since: 2.22
 */
GSocketAddress *
g_socket_connection_get_local_address (GSocketConnection  *connection,
				       GError            **error)
{
  return g_socket_get_local_address (connection->priv->socket, error);
}

/**
 * g_socket_connection_get_remote_address:
 * @connection: a #GSocketConnection
 * @error: #GError for error reporting, or %NULL to ignore.
 *
 * Try to get the remote address of a socket connection.
 *
 * Returns: (transfer full): a #GSocketAddress or %NULL on error.
 *     Free the returned object with g_object_unref().
 *
 * Since: 2.22
 */
GSocketAddress *
g_socket_connection_get_remote_address (GSocketConnection  *connection,
					GError            **error)
{
  return g_socket_get_remote_address (connection->priv->socket, error);
}

static void
g_socket_connection_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  GSocketConnection *connection = G_SOCKET_CONNECTION (object);

  switch (prop_id)
    {
     case PROP_SOCKET:
      g_value_set_object (value, connection->priv->socket);
      break;

     default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
g_socket_connection_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GSocketConnection *connection = G_SOCKET_CONNECTION (object);

  switch (prop_id)
    {
     case PROP_SOCKET:
      connection->priv->socket = G_SOCKET (g_value_dup_object (value));
      break;

     default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
g_socket_connection_constructed (GObject *object)
{
  GSocketConnection *connection = G_SOCKET_CONNECTION (object);

  g_assert (connection->priv->socket != NULL);
}

static void
g_socket_connection_dispose (GObject *object)
{
  GSocketConnection *connection = G_SOCKET_CONNECTION (object);

  connection->priv->in_dispose = TRUE;

  G_OBJECT_CLASS (g_socket_connection_parent_class)
    ->dispose (object);

  connection->priv->in_dispose = FALSE;
}

static void
g_socket_connection_finalize (GObject *object)
{
  GSocketConnection *connection = G_SOCKET_CONNECTION (object);

  if (connection->priv->input_stream)
    g_object_unref (connection->priv->input_stream);

  if (connection->priv->output_stream)
    g_object_unref (connection->priv->output_stream);

  g_object_unref (connection->priv->socket);

  G_OBJECT_CLASS (g_socket_connection_parent_class)
    ->finalize (object);
}

static void
g_socket_connection_class_init (GSocketConnectionClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GIOStreamClass *stream_class = G_IO_STREAM_CLASS (klass);

  g_type_class_add_private (klass, sizeof (GSocketConnectionPrivate));

  gobject_class->set_property = g_socket_connection_set_property;
  gobject_class->get_property = g_socket_connection_get_property;
  gobject_class->constructed = g_socket_connection_constructed;
  gobject_class->finalize = g_socket_connection_finalize;
  gobject_class->dispose = g_socket_connection_dispose;

  stream_class->get_input_stream = g_socket_connection_get_input_stream;
  stream_class->get_output_stream = g_socket_connection_get_output_stream;
  stream_class->close_fn = g_socket_connection_close;
  stream_class->close_async = g_socket_connection_close_async;
  stream_class->close_finish = g_socket_connection_close_finish;

  g_object_class_install_property (gobject_class,
                                   PROP_SOCKET,
                                   g_param_spec_object ("socket",
			                                P_("Socket"),
			                                P_("The underlying GSocket"),
                                                        G_TYPE_SOCKET,
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_STATIC_STRINGS));
}

static void
g_socket_connection_init (GSocketConnection *connection)
{
  connection->priv = G_TYPE_INSTANCE_GET_PRIVATE (connection,
                                                  G_TYPE_SOCKET_CONNECTION,
                                                  GSocketConnectionPrivate);
}

static gboolean
g_socket_connection_close (GIOStream     *stream,
			   GCancellable  *cancellable,
			   GError       **error)
{
  GSocketConnection *connection = G_SOCKET_CONNECTION (stream);

  if (connection->priv->output_stream)
    g_output_stream_close (connection->priv->output_stream,
			   cancellable, NULL);
  if (connection->priv->input_stream)
    g_input_stream_close (connection->priv->input_stream,
			  cancellable, NULL);

  /* Don't close the underlying socket if this is being called
   * as part of dispose(); when destroying the GSocketConnection,
   * we only want to close the socket if we're holding the last
   * reference on it, and in that case it will close itself when
   * we unref it in finalize().
   */
  if (connection->priv->in_dispose)
    return TRUE;

  return g_socket_close (connection->priv->socket, error);
}


static void
g_socket_connection_close_async (GIOStream           *stream,
				 int                  io_priority,
				 GCancellable        *cancellable,
				 GAsyncReadyCallback  callback,
				 gpointer             user_data)
{
  GSimpleAsyncResult *res;
  GIOStreamClass *class;
  GError *error;

  class = G_IO_STREAM_GET_CLASS (stream);

  /* socket close is not blocked, just do it! */
  error = NULL;
  if (class->close_fn &&
      !class->close_fn (stream, cancellable, &error))
    {
      g_simple_async_report_take_gerror_in_idle (G_OBJECT (stream),
					    callback, user_data,
					    error);
      return;
    }

  res = g_simple_async_result_new (G_OBJECT (stream),
				   callback,
				   user_data,
				   g_socket_connection_close_async);
  g_simple_async_result_complete_in_idle (res);
  g_object_unref (res);
}

static gboolean
g_socket_connection_close_finish (GIOStream     *stream,
				  GAsyncResult  *result,
				  GError       **error)
{
  return TRUE;
}

typedef struct {
  GSocketFamily socket_family;
  GSocketType socket_type;
  int protocol;
  GType implementation;
} ConnectionFactory;

static guint
connection_factory_hash (gconstpointer key)
{
  const ConnectionFactory *factory = key;
  guint h;

  h = factory->socket_family ^ (factory->socket_type << 4) ^ (factory->protocol << 8);
  /* This is likely to be small, so spread over whole
     hash space to get some distribution */
  h = h ^ (h << 8) ^ (h << 16) ^ (h << 24);

  return h;
}

static gboolean
connection_factory_equal (gconstpointer _a,
			  gconstpointer _b)
{
  const ConnectionFactory *a = _a;
  const ConnectionFactory *b = _b;

  if (a->socket_family != b->socket_family)
    return FALSE;

  if (a->socket_type != b->socket_type)
    return FALSE;

  if (a->protocol != b->protocol)
    return FALSE;

  return TRUE;
}

static GHashTable *connection_factories = NULL;
G_LOCK_DEFINE_STATIC(connection_factories);

/**
 * g_socket_connection_factory_register_type:
 * @g_type: a #GType, inheriting from %G_TYPE_SOCKET_CONNECTION
 * @family: a #GSocketFamily
 * @type: a #GSocketType
 * @protocol: a protocol id
 *
 * Looks up the #GType to be used when creating socket connections on
 * sockets with the specified @family, @type and @protocol.
 *
 * If no type is registered, the #GSocketConnection base type is returned.
 *
 * Since: 2.22
 */
void
g_socket_connection_factory_register_type (GType         g_type,
					   GSocketFamily family,
					   GSocketType   type,
					   gint          protocol)
{
  ConnectionFactory *factory;

  g_return_if_fail (g_type_is_a (g_type, G_TYPE_SOCKET_CONNECTION));

  G_LOCK (connection_factories);

  if (connection_factories == NULL)
    connection_factories = g_hash_table_new_full (connection_factory_hash,
						  connection_factory_equal,
						  (GDestroyNotify)g_free,
						  NULL);

  factory = g_new0 (ConnectionFactory, 1);
  factory->socket_family = family;
  factory->socket_type = type;
  factory->protocol = protocol;
  factory->implementation = g_type;

  g_hash_table_insert (connection_factories,
		       factory, factory);

  G_UNLOCK (connection_factories);
}

static void
init_builtin_types (void)
{
  volatile GType a_type;
#ifndef G_OS_WIN32
  a_type = g_unix_connection_get_type ();
#endif
  a_type = g_tcp_connection_get_type ();
  (a_type); /* To avoid -Wunused-but-set-variable */
}

/**
 * g_socket_connection_factory_lookup_type:
 * @family: a #GSocketFamily
 * @type: a #GSocketType
 * @protocol_id: a protocol id
 *
 * Looks up the #GType to be used when creating socket connections on
 * sockets with the specified @family, @type and @protocol_id.
 *
 * If no type is registered, the #GSocketConnection base type is returned.
 *
 * Returns: a #GType
 *
 * Since: 2.22
 */
GType
g_socket_connection_factory_lookup_type (GSocketFamily family,
					 GSocketType   type,
					 gint          protocol_id)
{
  ConnectionFactory *factory, key;
  GType g_type;

  init_builtin_types ();

  G_LOCK (connection_factories);

  g_type = G_TYPE_SOCKET_CONNECTION;

  if (connection_factories)
    {
      key.socket_family = family;
      key.socket_type = type;
      key.protocol = protocol_id;

      factory = g_hash_table_lookup (connection_factories, &key);
      if (factory)
	g_type = factory->implementation;
    }

  G_UNLOCK (connection_factories);

  return g_type;
}

/**
 * g_socket_connection_factory_create_connection:
 * @socket: a #GSocket
 *
 * Creates a #GSocketConnection subclass of the right type for
 * @socket.
 *
 * Returns: (transfer full): a #GSocketConnection
 *
 * Since: 2.22
 */
GSocketConnection *
g_socket_connection_factory_create_connection (GSocket *socket)
{
  GType type;

  type = g_socket_connection_factory_lookup_type (g_socket_get_family (socket),
						  g_socket_get_socket_type (socket),
						  g_socket_get_protocol (socket));
  return g_object_new (type, "socket", socket, NULL);
}
