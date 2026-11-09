/* GLib testing framework examples and tests
 *
 * Copyright © 2026 Dimitriy Ryazantcev
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
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

/* Tests for what GDBusDaemon does when it is destroyed. */

#include <gio/gio.h>

#include "gdbusdaemon.h"

static void
async_result_cb (GObject      *source_object,
                 GAsyncResult *res,
                 gpointer      user_data)
{
  GAsyncResult **result = user_data;

  g_assert_null (*result);
  *result = g_object_ref (res);
  g_main_context_wakeup (g_main_context_get_thread_default ());
}

/* Connects to the daemon like a client of a message bus does. */
static GDBusConnection *
connect_to_daemon (GDBusDaemon *daemon)
{
  GError *error = NULL;
  GAsyncResult *result = NULL;
  GDBusConnection *client;

  g_dbus_connection_new_for_address (_g_dbus_daemon_get_address (daemon),
                                     G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT |
                                     G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION,
                                     NULL, NULL, async_result_cb, &result);
  while (result == NULL)
    g_main_context_iteration (NULL, TRUE);

  client = g_dbus_connection_new_for_address_finish (result, &error);
  g_assert_no_error (error);
  g_assert_nonnull (client);
  g_clear_object (&result);

  return client;
}

/* Returns the connection of the daemon to its only client. The daemon
 * exports its interface on the connection of each client, and it has not
 * seen the client when the client has connected, so run the main context
 * until it has.
 */
static GDBusConnection *
get_daemon_side_connection (GDBusDaemon *daemon)
{
  GDBusInterfaceSkeleton *skeleton = G_DBUS_INTERFACE_SKELETON (daemon);
  GList *connections;
  GDBusConnection *connection;

  while ((connections = g_dbus_interface_skeleton_get_connections (skeleton)) == NULL)
    g_main_context_iteration (NULL, TRUE);

  g_assert_cmpuint (g_list_length (connections), ==, 1);
  connection = g_object_ref (connections->data);
  g_list_free_full (connections, g_object_unref);

  return connection;
}

static void
closed_cb (GDBusConnection *connection,
           gboolean         remote_peer_vanished,
           GError          *error,
           gpointer         user_data)
{
  gboolean *closed = user_data;

  g_assert_false (*closed);
  *closed = TRUE;
  g_main_context_wakeup (g_main_context_get_thread_default ());
}

/* The connection of a client can outlive the daemon: it is kept alive until
 * its "closed" signal has been emitted.  The handler of the signal must not
 * use the client of the daemon after it has been freed with the daemon.
 */
static void
test_client_closed_after_daemon (void)
{
  GError *error = NULL;
  GDBusDaemon *daemon;
  GDBusConnection *client;
  GDBusConnection *daemon_side;
  gboolean closed = FALSE;
  gulong closed_handler_id;

  daemon = _g_dbus_daemon_new (NULL, NULL, &error);
  g_assert_no_error (error);
  g_assert_nonnull (daemon);

  client = connect_to_daemon (daemon);
  daemon_side = get_daemon_side_connection (daemon);

  /* This handler runs after the one of the daemon, and is only there to know
   * when the signal has been emitted.
   */
  closed_handler_id = g_signal_connect (daemon_side, "closed",
                                        G_CALLBACK (closed_cb), &closed);

  /* The daemon notices that the client has gone, and schedules the emission
   * of the signal in the main context.  Wait for that by polling, without
   * running the main context: the signal must not be emitted until the daemon
   * is destroyed, and waiting for it would emit it while the daemon is alive.
   * If the connection never closes, the test is stopped by its timeout.
   */
  g_dbus_connection_close_sync (client, NULL, &error);
  g_assert_no_error (error);
  g_object_unref (client);
  while (!g_dbus_connection_is_closed (daemon_side))
    g_usleep (1000);
  g_assert_false (closed);

  g_object_unref (daemon);

  /* Emit the signal now: it must not reach the freed client. */
  while (!closed)
    g_main_context_iteration (NULL, TRUE);

  g_signal_handler_disconnect (daemon_side, closed_handler_id);
  g_object_unref (daemon_side);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, G_TEST_OPTION_ISOLATE_DIRS, NULL);

  g_test_add_func ("/gdbus/daemon/teardown/client-closed-after-daemon",
                   test_client_closed_after_daemon);

  return g_test_run ();
}
