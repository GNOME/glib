/* GLib testing framework examples and tests
 *
 * Copyright 2014 Red Hat, Inc.
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
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see
 * <http://www.gnu.org/licenses/>.
 */

#include <gio/gio.h>

static void
active_notify_cb (GSocketService *service,
                  GParamSpec     *pspec,
                  gpointer        data)
{
  gboolean *success = (gboolean *)data;

  if (g_socket_service_is_active (service))
    *success = TRUE;
}

static void
connected_cb (GObject      *client,
              GAsyncResult *result,
              gpointer      user_data)
{
  GSocketService *service = G_SOCKET_SERVICE (user_data);
  GSocketConnection *conn;
  GError *error = NULL;

  g_assert_true (g_socket_service_is_active (service));

  conn = g_socket_client_connect_finish (G_SOCKET_CLIENT (client), result, &error);
  g_assert_no_error (error);
  g_object_unref (conn);

  g_socket_service_stop (service);
  g_assert_false (g_socket_service_is_active (service));
}

static void
test_start_stop (void)
{
  gboolean success = FALSE;
  GInetAddress *iaddr;
  GSocketAddress *saddr, *listening_addr;
  GSocketService *service;
  GError *error = NULL;
  GSocketClient *client;

  iaddr = g_inet_address_new_loopback (G_SOCKET_FAMILY_IPV4);
  saddr = g_inet_socket_address_new (iaddr, 0);
  g_object_unref (iaddr);

  /* instanciate with g_object_new so we can pass active = false */
  service = g_object_new (G_TYPE_SOCKET_SERVICE, "active", FALSE, NULL);
  g_assert_false (g_socket_service_is_active (service));

  g_signal_connect (service, "notify::active", G_CALLBACK (active_notify_cb), &success);

  g_socket_listener_add_address (G_SOCKET_LISTENER (service),
                                 saddr,
                                 G_SOCKET_TYPE_STREAM,
                                 G_SOCKET_PROTOCOL_TCP,
                                 NULL,
                                 &listening_addr,
                                 &error);
  g_assert_no_error (error);
  g_object_unref (saddr);

  client = g_socket_client_new ();
  g_socket_client_connect_async (client,
                                 G_SOCKET_CONNECTABLE (listening_addr),
                                 NULL,
                                 connected_cb, service);
  g_object_unref (client);
  g_object_unref (listening_addr);

  g_socket_service_start (service);
  g_assert_true (g_socket_service_is_active (service));

  do
    g_main_context_iteration (NULL, TRUE);
  while (!success);

  g_object_unref (service);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_bug_base ("http://bugzilla.gnome.org/");

  g_test_add_func ("/socket-service/start-stop", test_start_stop);

  return g_test_run();
}
