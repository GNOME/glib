#include "config.h"
#include "mock-resolver.h"

#include <gio/gio.h>

static void
on_connected (GObject *client, GAsyncResult *res, gpointer user_data)
{
  GError *error = NULL;
  GSocketConnection *connection = g_socket_client_connect_finish (G_SOCKET_CLIENT (client), res, &error);
  g_main_loop_quit (user_data);

  /* We want to make sure we made it to the point of conencting to a socket */
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_CONNECTION_REFUSED);
  g_assert_null (connection);
}

static void
test_no_ipv6_addresses (void)
{
  GMainLoop *loop = g_main_loop_new (NULL, FALSE);
  GSocketClient *client = g_socket_client_new ();
  GSocketConnectable *connectable = g_network_address_new ("gnome.fake", 1234);
  GResolver *original_resolver = g_resolver_get_default ();
  MockResolver *mock_resolver = mock_resolver_new ();
  GList *ipv4_results = NULL;

  /* This tests that the client sucessfully attempts an ipv4 address when no ipv6 address is returned */

  g_resolver_set_default (G_RESOLVER (mock_resolver));
  ipv4_results = g_list_append (ipv4_results, g_inet_address_new_loopback (G_SOCKET_FAMILY_IPV4));
  mock_resolver_set_ipv4_results (mock_resolver, ipv4_results);
  mock_resolver_set_ipv4_delay_ms (mock_resolver, 100);

  g_socket_client_set_timeout (client, 5);
  g_socket_client_connect_async (client, connectable, NULL, on_connected, loop);
  g_main_loop_run (loop);

  g_resolver_set_default (original_resolver);
  g_list_free_full (ipv4_results, (GDestroyNotify) g_object_unref);
  g_object_unref (original_resolver);
  g_object_unref (mock_resolver);
  g_object_unref (client);
  g_object_unref (connectable);
  g_main_loop_unref (loop);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  g_test_bug_base ("https://gitlab.gnome.org/GNOME/glib/");

  g_test_add_func ("/socket-client/no-ipv6-addresses", test_no_ipv6_addresses);
  return g_test_run ();
}
