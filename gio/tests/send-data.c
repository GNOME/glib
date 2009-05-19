#include <gio/gio.h>
#include <string.h>
#include <stdio.h>

int cancel_timeout = 0;
static GOptionEntry cmd_entries[] = {
  {"cancel", 'c', 0, G_OPTION_ARG_INT, &cancel_timeout,
   "Cancel any op after the specified amount of seconds", NULL},
  {NULL}
};

static gpointer
cancel_thread (gpointer data)
{
  GCancellable *cancellable = data;

  g_usleep (1000*1000*cancel_timeout);
  g_print ("Cancelling\n");
  g_cancellable_cancel (cancellable);
  return NULL;
}

static char *
socket_address_to_string (GSocketAddress *address)
{
  GInetAddress *inet_address;
  char *str, *res;
  int port;

  inet_address = g_inet_socket_address_get_address (G_INET_SOCKET_ADDRESS (address));
  str = g_inet_address_to_string (inet_address);
  port = g_inet_socket_address_get_port (G_INET_SOCKET_ADDRESS (address));
  res = g_strdup_printf ("%s:%d", str, port);
  g_free (str);
  return res;
}

int
main (int argc, char *argv[])
{
  GOptionContext *context;
  GSocketClient *client;
  GSocketConnection *connection;
  GSocketAddress *address;
  GCancellable *cancellable;
  GOutputStream *out;
  GError *error = NULL;
  char buffer[1000];

  g_type_init ();
  g_thread_init (NULL);

  context = g_option_context_new (" <hostname>[:port] - send data to tcp host");
  g_option_context_add_main_entries (context, cmd_entries, NULL);
  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_printerr ("%s: %s\n", argv[0], error->message);
      return 1;
    }

  if (argc != 2)
    {
      g_printerr ("%s: %s\n", argv[0], "Need to specify hostname");
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

  client = g_socket_client_new ();
  connection = g_socket_client_connect_to_host (client,
						argv[1],
						7777,
						cancellable, &error);
  if (connection == NULL)
    {
      g_printerr ("%s can't connect: %s\n", argv[0], error->message);
      return 1;
    }

  address = g_socket_connection_get_remote_address (connection, &error);
  if (!address)
    {
      g_printerr ("Error getting remote address: %s\n",
		  error->message);
      return 1;
    }
  g_print ("Connected to address: %s\n",
	   socket_address_to_string (address));
  g_object_unref (address);

  out = g_io_stream_get_output_stream (G_IO_STREAM (connection));

  while (fgets(buffer, sizeof (buffer), stdin) != NULL)
    {
      if (!g_output_stream_write_all (out, buffer, strlen (buffer),
				      NULL, cancellable, &error))
	{
	  g_warning ("send error: %s\n",  error->message);
	  g_error_free (error);
	  error = NULL;
	}
    }

  g_print ("closing stream\n");
  if (!g_io_stream_close (G_IO_STREAM (connection), cancellable, &error))
    {
      g_warning ("close error: %s\n",  error->message);
      return 1;
    }

  return 0;
}
