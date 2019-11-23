#include "config.h"
#include "mock-resolver.h"

#include <gio/gio.h>

static void
on_connected (GObject *client, GAsyncResult *res, gpointer user_data)
{
  GError *error = NULL;
  GSocketConnection *connection = g_socket_client_connect_finish (G_SOCKET_CLIENT (client), res, &error);
  g_main_loop_quit (user_data);
  g_assert_no_error (error);
  g_assert_nonnull (connection);
}

static void
test_out_of_addresses (void)
{
  GMainLoop *loop = g_main_loop_new (NULL, FALSE);
  GSocketClient *client = g_socket_client_new ();
  GSocketConnectable *connectable = g_network_address_new ("gnome.fake", 1234);
  GResolver *original_resolver = g_resolver_get_default ();
  MockResolver *mock_resolver = mock_resolver_new ();
  GList *ipv4_results = NULL;

  g_resolver_set_default (G_RESOLVER (mock_resolver));
  ipv4_results = g_list_append (ipv4_results, g_inet_address_new_from_string ("123.123.123.123"));
  mock_resolver_set_ipv4_results (mock_resolver, ipv4_results);
  mock_resolver_set_ipv4_delay_ms (mock_resolver, 100);

  g_socket_client_set_timeout (client, 5);
  g_socket_client_connect_async (client, connectable, NULL, on_connected, loop);
  g_main_loop_run (loop);

  // FIXME: cleanup
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  g_test_bug_base ("https://gitlab.gnome.org/GNOME/glib/");

  g_test_add_func ("/socket-client/basic", test_out_of_addresses);
  return g_test_run ();
}
