#include <gio/gio.h>
#include <gio/gunixsocketaddress.h>
#include <glib.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

GMainLoop *loop;

gboolean verbose = FALSE;
gboolean non_blocking = FALSE;
gboolean use_udp = FALSE;
int cancel_timeout = 0;
int read_timeout = 0;
gboolean unix_socket = FALSE;
gboolean tls = FALSE;

static GOptionEntry cmd_entries[] = {
  {"cancel", 'c', 0, G_OPTION_ARG_INT, &cancel_timeout,
   "Cancel any op after the specified amount of seconds", NULL},
  {"udp", 'u', 0, G_OPTION_ARG_NONE, &use_udp,
   "Use udp instead of tcp", NULL},
  {"verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose,
   "Be verbose", NULL},
  {"non-blocking", 'n', 0, G_OPTION_ARG_NONE, &non_blocking,
   "Enable non-blocking i/o", NULL},
#ifdef G_OS_UNIX
  {"unix", 'U', 0, G_OPTION_ARG_NONE, &unix_socket,
   "Use a unix socket instead of IP", NULL},
#endif
  {"timeout", 't', 0, G_OPTION_ARG_INT, &read_timeout,
   "Time out reads after the specified number of seconds", NULL},
  {"tls", 'T', 0, G_OPTION_ARG_NONE, &tls,
   "Use TLS (SSL)", NULL},
  {NULL}
};

#include "socket-common.c"

static gboolean
accept_certificate (GTlsClientConnection *conn, GTlsCertificate *cert,
		    GTlsCertificateFlags errors, gpointer user_data)
{
  g_print ("Certificate would have been rejected ( ");
  if (errors & G_TLS_CERTIFICATE_UNKNOWN_CA)
    g_print ("unknown-ca ");
  if (errors & G_TLS_CERTIFICATE_BAD_IDENTITY)
    g_print ("bad-identity ");
  if (errors & G_TLS_CERTIFICATE_NOT_ACTIVATED)
    g_print ("not-activated ");
  if (errors & G_TLS_CERTIFICATE_EXPIRED)
    g_print ("expired ");
  if (errors & G_TLS_CERTIFICATE_REVOKED)
    g_print ("revoked ");
  if (errors & G_TLS_CERTIFICATE_INSECURE)
    g_print ("insecure ");
  g_print (") but accepting anyway.\n");

  return TRUE;
}

int
main (int argc,
      char *argv[])
{
  GSocket *socket;
  GSocketAddress *src_address;
  GSocketAddress *address;
  GSocketType socket_type;
  GSocketFamily socket_family;
  GError *error = NULL;
  GOptionContext *context;
  GCancellable *cancellable;
  GSocketAddressEnumerator *enumerator;
  GSocketConnectable *connectable;
  GIOStream *connection;
  GInputStream *istream;
  GOutputStream *ostream;

  g_thread_init (NULL);

  g_type_init ();

  context = g_option_context_new (" <hostname>[:port] - Test GSocket client stuff");
  g_option_context_add_main_entries (context, cmd_entries, NULL);
  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_printerr ("%s: %s\n", argv[0], error->message);
      return 1;
    }

  if (argc != 2)
    {
      g_printerr ("%s: %s\n", argv[0], "Need to specify hostname / unix socket name");
      return 1;
    }

  if (use_udp && tls)
    {
      g_printerr ("DTLS (TLS over UDP) is not supported");
      return 1;
    }

  if (cancel_timeout)
    {
      cancellable = g_cancellable_new ();
      g_thread_create (cancel_thread, cancellable, FALSE, NULL);
    }
  else
    {
      cancellable = NULL;
    }

  loop = g_main_loop_new (NULL, FALSE);

  if (use_udp)
    socket_type = G_SOCKET_TYPE_DATAGRAM;
  else
    socket_type = G_SOCKET_TYPE_STREAM;

  if (unix_socket)
    socket_family = G_SOCKET_FAMILY_UNIX;
  else
    socket_family = G_SOCKET_FAMILY_IPV4;

  socket = g_socket_new (socket_family, socket_type, 0, &error);
  if (socket == NULL)
    {
      g_printerr ("%s: %s\n", argv[0], error->message);
      return 1;
    }

  if (read_timeout)
    g_socket_set_timeout (socket, read_timeout);

  if (unix_socket)
    {
      GSocketAddress *addr;

      addr = socket_address_from_string (argv[1]);
      if (addr == NULL)
	{
	  g_printerr ("%s: Could not parse '%s' as unix socket name\n", argv[0], argv[1]);
	  return 1;
	}
      connectable = G_SOCKET_CONNECTABLE (addr);
    }
  else
    {
      connectable = g_network_address_parse (argv[1], 7777, &error);
      if (connectable == NULL)
	{
	  g_printerr ("%s: %s\n", argv[0], error->message);
	  return 1;
	}
    }

  enumerator = g_socket_connectable_enumerate (connectable);
  while (TRUE)
    {
      address = g_socket_address_enumerator_next (enumerator, cancellable, &error);
      if (address == NULL)
	{
	  if (error == NULL)
	    g_printerr ("%s: No more addresses to try\n", argv[0]);
	  else
	    g_printerr ("%s: %s\n", argv[0], error->message);
	  return 1;
	}

      if (g_socket_connect (socket, address, cancellable, &error))
	break;
      g_printerr ("%s: Connection to %s failed: %s, trying next\n", argv[0], socket_address_to_string (address), error->message);
      g_error_free (error);
      error = NULL;

      g_object_unref (address);
    }
  g_object_unref (enumerator);

  g_print ("Connected to %s\n",
	   socket_address_to_string (address));

  src_address = g_socket_get_local_address (socket, &error);
  if (!src_address)
    {
      g_printerr ("Error getting local address: %s\n",
		  error->message);
      return 1;
    }
  g_print ("local address: %s\n",
	   socket_address_to_string (src_address));
  g_object_unref (src_address);

  if (use_udp)
    {
      connection = NULL;
      istream = NULL;
      ostream = NULL;
    }
  else
    connection = G_IO_STREAM (g_socket_connection_factory_create_connection (socket));

  if (tls)
    {
      GIOStream *tls_conn;

      tls_conn = g_tls_client_connection_new (connection, connectable, &error);
      if (!tls_conn)
	{
	  g_printerr ("Could not create TLS connection: %s\n",
		      error->message);
	  return 1;
	}

      g_signal_connect (tls_conn, "accept-certificate",
			G_CALLBACK (accept_certificate), NULL);

      if (!g_tls_connection_handshake (G_TLS_CONNECTION (tls_conn),
				       cancellable, &error))
	{
	  g_printerr ("Error during TLS handshake: %s\n",
		      error->message);
	  return 1;
	}

      g_object_unref (connection);
      connection = G_IO_STREAM (tls_conn);
    }
  g_object_unref (connectable);

  if (connection)
    {
      istream = g_io_stream_get_input_stream (connection);
      ostream = g_io_stream_get_output_stream (connection);
    }

  /* TODO: Test non-blocking connect/handshake */
  if (non_blocking)
    g_socket_set_blocking (socket, FALSE);

  while (TRUE)
    {
      gchar buffer[4096];
      gssize size;
      gsize to_send;

      if (fgets (buffer, sizeof buffer, stdin) == NULL)
	break;

      to_send = strlen (buffer);
      while (to_send > 0)
	{
	  if (use_udp)
	    {
	      ensure_socket_condition (socket, G_IO_OUT, cancellable);
	      size = g_socket_send_to (socket, address,
				       buffer, to_send,
				       cancellable, &error);
	    }
	  else
	    {
	      ensure_connection_condition (connection, G_IO_OUT, cancellable);
	      size = g_output_stream_write (ostream,
					    buffer, to_send,
					    cancellable, &error);
	    }

	  if (size < 0)
	    {
	      if (g_error_matches (error,
				   G_IO_ERROR,
				   G_IO_ERROR_WOULD_BLOCK))
		{
		  g_print ("socket send would block, handling\n");
		  g_error_free (error);
		  error = NULL;
		  continue;
		}
	      else
		{
		  g_printerr ("Error sending to socket: %s\n",
			      error->message);
		  return 1;
		}
	    }

	  g_print ("sent %" G_GSSIZE_FORMAT " bytes of data\n", size);

	  if (size == 0)
	    {
	      g_printerr ("Unexpected short write\n");
	      return 1;
	    }

	  to_send -= size;
	}

      if (use_udp)
	{
	  ensure_socket_condition (socket, G_IO_IN, cancellable);
	  size = g_socket_receive_from (socket, &src_address,
					buffer, sizeof buffer,
					cancellable, &error);
	}
      else
	{
	  ensure_connection_condition (connection, G_IO_IN, cancellable);
	  size = g_input_stream_read (istream,
				      buffer, sizeof buffer,
				      cancellable, &error);
	}

      if (size < 0)
	{
	  g_printerr ("Error receiving from socket: %s\n",
		      error->message);
	  return 1;
	}

      if (size == 0)
	break;

      g_print ("received %" G_GSSIZE_FORMAT " bytes of data", size);
      if (use_udp)
	g_print (" from %s", socket_address_to_string (src_address));
      g_print ("\n");

      if (verbose)
	g_print ("-------------------------\n"
		 "%.*s"
		 "-------------------------\n",
		 (int)size, buffer);

    }

  g_print ("closing socket\n");

  if (connection)
    {
      if (!g_io_stream_close (connection, cancellable, &error))
	{
	  g_printerr ("Error closing connection: %s\n",
		      error->message);
	  return 1;
	}
      g_object_unref (connection);
    }
  else
    {
      if (!g_socket_close (socket, &error))
	{
	  g_printerr ("Error closing master socket: %s\n",
		      error->message);
	  return 1;
	}
    }

  g_object_unref (socket);
  g_object_unref (address);

  return 0;
}
