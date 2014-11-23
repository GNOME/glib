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

GMutex mutex_712570;
GCond cond_712570;
volatile gboolean finalized;

GType test_threaded_socket_service_get_type (void);
typedef GThreadedSocketService TestThreadedSocketService;
typedef GThreadedSocketServiceClass TestThreadedSocketServiceClass;

G_DEFINE_TYPE (TestThreadedSocketService, test_threaded_socket_service, G_TYPE_THREADED_SOCKET_SERVICE);

static void
test_threaded_socket_service_init (TestThreadedSocketService *service)
{
}

static void
test_threaded_socket_service_finalize (GObject *object)
{
  G_OBJECT_CLASS (test_threaded_socket_service_parent_class)->finalize (object);

  /* Signal the main thread that finalization completed successfully
   * rather than hanging.
   */
  finalized = TRUE;
  g_cond_signal (&cond_712570);
  g_mutex_unlock (&mutex_712570);
}

static void
test_threaded_socket_service_class_init (TestThreadedSocketServiceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = test_threaded_socket_service_finalize;
}

static gboolean
connection_cb (GThreadedSocketService *service,
               GSocketConnection      *connection,
               GObject                *source_object,
               gpointer                user_data)
{
  /* Block until the main thread has dropped its ref to @service, so that we
   * will drop the final ref from this thread.
   */
  g_mutex_lock (&mutex_712570);

  /* The service should now have 1 ref owned by the current "run"
   * signal emission, and another added by GThreadedSocketService for
   * this thread. Both will be dropped after we return.
   */
  g_assert_cmpint (G_OBJECT (service)->ref_count, ==, 2);

  return FALSE;
}

static void
client_connected_cb (GObject      *client,
                     GAsyncResult *result,
                     gpointer      user_data)
{
  GMainLoop *loop = user_data;
  GSocketConnection *conn;
  GError *error = NULL;

  conn = g_socket_client_connect_finish (G_SOCKET_CLIENT (client), result, &error);
  g_assert_no_error (error);

  g_object_unref (conn);
  g_main_loop_quit (loop);
}

static void
test_threaded_712570 (void)
{
  GSocketService *service;
  GSocketAddress *addr, *listening_addr;
  GMainLoop *loop;
  GSocketClient *client;
  GError *error = NULL;
  int ref_count;

  g_test_bug ("712570");

  g_mutex_lock (&mutex_712570);

  service = g_object_new (test_threaded_socket_service_get_type (), NULL);

  addr = g_inet_socket_address_new_from_string ("127.0.0.1", 0);
  g_socket_listener_add_address (G_SOCKET_LISTENER (service),
                                 addr,
                                 G_SOCKET_TYPE_STREAM,
                                 G_SOCKET_PROTOCOL_TCP,
                                 NULL,
                                 &listening_addr,
                                 &error);
  g_assert_no_error (error);
  g_object_unref (addr);

  g_signal_connect (service, "run", G_CALLBACK (connection_cb), NULL);

  loop = g_main_loop_new (NULL, FALSE);

  client = g_socket_client_new ();
  g_socket_client_connect_async (client,
                                 G_SOCKET_CONNECTABLE (listening_addr),
                                 NULL,
                                 client_connected_cb, loop);
  g_object_unref (client);
  g_object_unref (listening_addr);

  g_main_loop_run (loop);
  g_main_loop_unref (loop);

  /* Stop the service and then wait for it to asynchronously cancel
   * its outstanding accept() call (and drop the associated ref).
   */
  ref_count = G_OBJECT (service)->ref_count;
  g_socket_service_stop (G_SOCKET_SERVICE (service));
  while (G_OBJECT (service)->ref_count == ref_count)
    g_main_context_iteration (NULL, TRUE);

  /* Drop our ref, then unlock the mutex and wait for the service to be
   * finalized. (Without the fix for 712570 it would hang forever here.)
   */
  g_object_unref (service);

  while (!finalized)
    g_cond_wait (&cond_712570, &mutex_712570);
  g_mutex_unlock (&mutex_712570);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_bug_base ("http://bugzilla.gnome.org/");

  g_test_add_func ("/socket-listener/threaded/712570", test_threaded_712570);

  return g_test_run();
}

