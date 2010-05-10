/* GLib testing framework examples and tests
 *
 * Copyright (C) 2008-2010 Red Hat, Inc.
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
 * Author: David Zeuthen <davidz@redhat.com>
 */

#include <gio/gio.h>
#include <unistd.h>
#include <string.h>

#include "gdbus-tests.h"

/* all tests rely on a shared mainloop */
static GMainLoop *loop = NULL;

/* ---------------------------------------------------------------------------------------------------- */
/* Connection life-cycle testing */
/* ---------------------------------------------------------------------------------------------------- */

static void
test_connection_life_cycle (void)
{
  GDBusConnection *c;
  GDBusConnection *c2;
  GError *error;

  error = NULL;

  /*
   * Check for correct behavior when no bus is present
   *
   */
  c = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
  _g_assert_error_domain (error, G_IO_ERROR);
  g_assert (!g_dbus_error_is_remote_error (error));
  g_assert (c == NULL);
  g_error_free (error);
  error = NULL;

  /*
   *  Check for correct behavior when a bus is present
   */
  session_bus_up ();
  /* case 1 */
  c = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
  g_assert_no_error (error);
  g_assert (c != NULL);
  g_assert (!g_dbus_connection_is_closed (c));

  /*
   * Check that singleton handling work
   */
  c2 = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
  g_assert_no_error (error);
  g_assert (c2 != NULL);
  g_assert (c == c2);
  g_object_unref (c2);

  /*
   * Check that private connections work
   */
  c2 = _g_bus_get_priv (G_BUS_TYPE_SESSION, NULL, &error);
  g_assert_no_error (error);
  g_assert (c2 != NULL);
  g_assert (c != c2);
  g_object_unref (c2);

  c2 = _g_bus_get_priv (G_BUS_TYPE_SESSION, NULL, &error);
  g_assert_no_error (error);
  g_assert (c2 != NULL);
  g_assert (!g_dbus_connection_is_closed (c2));
  g_dbus_connection_close (c2);
  _g_assert_signal_received (c2, "closed");
  g_assert (g_dbus_connection_is_closed (c2));
  g_object_unref (c2);

  /*
   *  Check for correct behavior when the bus goes away
   *
   */
  g_assert (!g_dbus_connection_is_closed (c));
  g_dbus_connection_set_exit_on_close (c, FALSE);
  session_bus_down ();
  if (!g_dbus_connection_is_closed (c))
    _g_assert_signal_received (c, "closed");
  g_assert (g_dbus_connection_is_closed (c));

  _g_object_wait_for_single_ref (c);
  g_object_unref (c);
}

/* ---------------------------------------------------------------------------------------------------- */
/* Test that sending and receiving messages work as expected */
/* ---------------------------------------------------------------------------------------------------- */

static void
msg_cb_expect_error_disconnected (GDBusConnection *connection,
                                  GAsyncResult    *res,
                                  gpointer         user_data)
{
  GError *error;
  GVariant *result;

  error = NULL;
  result = g_dbus_connection_call_finish (connection,
                                          res,
                                          &error);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_CLOSED);
  g_assert (!g_dbus_error_is_remote_error (error));
  g_error_free (error);
  g_assert (result == NULL);

  g_main_loop_quit (loop);
}

static void
msg_cb_expect_error_unknown_method (GDBusConnection *connection,
                                    GAsyncResult    *res,
                                    gpointer         user_data)
{
  GError *error;
  GVariant *result;

  error = NULL;
  result = g_dbus_connection_call_finish (connection,
                                          res,
                                          &error);
  g_assert_error (error, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_METHOD);
  g_assert (g_dbus_error_is_remote_error (error));
  g_assert (result == NULL);

  g_main_loop_quit (loop);
}

static void
msg_cb_expect_success (GDBusConnection *connection,
                       GAsyncResult    *res,
                       gpointer         user_data)
{
  GError *error;
  GVariant *result;

  error = NULL;
  result = g_dbus_connection_call_finish (connection,
                                          res,
                                          &error);
  g_assert_no_error (error);
  g_assert (result != NULL);
  g_variant_unref (result);

  g_main_loop_quit (loop);
}

static void
msg_cb_expect_error_cancelled (GDBusConnection *connection,
                               GAsyncResult    *res,
                               gpointer         user_data)
{
  GError *error;
  GVariant *result;

  error = NULL;
  result = g_dbus_connection_call_finish (connection,
                                          res,
                                          &error);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_CANCELLED);
  g_assert (!g_dbus_error_is_remote_error (error));
  g_error_free (error);
  g_assert (result == NULL);

  g_main_loop_quit (loop);
}

static void
msg_cb_expect_error_cancelled_2 (GDBusConnection *connection,
                                 GAsyncResult    *res,
                                 gpointer         user_data)
{
  GError *error;
  GVariant *result;

  error = NULL;
  result = g_dbus_connection_call_finish (connection,
                                          res,
                                          &error);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_CANCELLED);
  g_assert (!g_dbus_error_is_remote_error (error));
  g_error_free (error);
  g_assert (result == NULL);

  g_main_loop_quit (loop);
}

/* ---------------------------------------------------------------------------------------------------- */

static void
test_connection_send (void)
{
  GDBusConnection *c;
  GCancellable *ca;

  session_bus_up ();

  /* First, get an unopened connection */
  c = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
  g_assert (c != NULL);
  g_assert (!g_dbus_connection_is_closed (c));

  /*
   * Check that we never actually send a message if the GCancellable
   * is already cancelled - i.e.  we should get #G_IO_ERROR_CANCELLED
   * when the actual connection is not up.
   */
  ca = g_cancellable_new ();
  g_cancellable_cancel (ca);
  g_dbus_connection_call (c,
                          "org.freedesktop.DBus",  /* bus_name */
                          "/org/freedesktop/DBus", /* object path */
                          "org.freedesktop.DBus",  /* interface name */
                          "GetId",                 /* method name */
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          ca,
                          (GAsyncReadyCallback) msg_cb_expect_error_cancelled,
                          NULL);
  g_main_loop_run (loop);
  g_object_unref (ca);

  /*
   * Check that we get a reply to the GetId() method call.
   */
  g_dbus_connection_call (c,
                          "org.freedesktop.DBus",  /* bus_name */
                          "/org/freedesktop/DBus", /* object path */
                          "org.freedesktop.DBus",  /* interface name */
                          "GetId",                 /* method name */
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          NULL,
                          (GAsyncReadyCallback) msg_cb_expect_success,
                          NULL);
  g_main_loop_run (loop);

  /*
   * Check that we get an error reply to the NonExistantMethod() method call.
   */
  g_dbus_connection_call (c,
                          "org.freedesktop.DBus",  /* bus_name */
                          "/org/freedesktop/DBus", /* object path */
                          "org.freedesktop.DBus",  /* interface name */
                          "NonExistantMethod",     /* method name */
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          NULL,
                          (GAsyncReadyCallback) msg_cb_expect_error_unknown_method,
                          NULL);
  g_main_loop_run (loop);

  /*
   * Check that cancellation works when the message is already in flight.
   */
  ca = g_cancellable_new ();
  g_dbus_connection_call (c,
                          "org.freedesktop.DBus",  /* bus_name */
                          "/org/freedesktop/DBus", /* object path */
                          "org.freedesktop.DBus",  /* interface name */
                          "GetId",                 /* method name */
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          ca,
                          (GAsyncReadyCallback) msg_cb_expect_error_cancelled_2,
                          NULL);
  g_cancellable_cancel (ca);
  g_main_loop_run (loop);
  g_object_unref (ca);

  /*
   * Check that we get an error when sending to a connection that is disconnected.
   */
  g_dbus_connection_set_exit_on_close (c, FALSE);
  session_bus_down ();
  _g_assert_signal_received (c, "closed");
  g_assert (g_dbus_connection_is_closed (c));

  g_dbus_connection_call (c,
                          "org.freedesktop.DBus",  /* bus_name */
                          "/org/freedesktop/DBus", /* object path */
                          "org.freedesktop.DBus",  /* interface name */
                          "GetId",                 /* method name */
                          NULL,
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          NULL,
                          (GAsyncReadyCallback) msg_cb_expect_error_disconnected,
                          NULL);
  g_main_loop_run (loop);

  _g_object_wait_for_single_ref (c);
  g_object_unref (c);
}

/* ---------------------------------------------------------------------------------------------------- */
/* Connection signal tests */
/* ---------------------------------------------------------------------------------------------------- */

static void
test_connection_signal_handler (GDBusConnection  *connection,
                                const gchar      *sender_name,
                                const gchar      *object_path,
                                const gchar      *interface_name,
                                const gchar      *signal_name,
                                GVariant         *parameters,
                                gpointer         user_data)
{
  gint *counter = user_data;
  *counter += 1;

  /*g_debug ("in test_connection_signal_handler (sender=%s path=%s interface=%s member=%s)",
           sender_name,
           object_path,
           interface_name,
           signal_name);*/

  g_main_loop_quit (loop);
}

static gboolean
test_connection_signal_quit_mainloop (gpointer user_data)
{
  gboolean *quit_mainloop_fired = user_data;
  *quit_mainloop_fired = TRUE;
  g_main_loop_quit (loop);
  return TRUE;
}

static void
test_connection_signals (void)
{
  GDBusConnection *c1;
  GDBusConnection *c2;
  GDBusConnection *c3;
  guint s1;
  guint s2;
  guint s3;
  gint count_s1;
  gint count_s2;
  gint count_name_owner_changed;
  GError *error;
  gboolean ret;
  GVariant *result;

  error = NULL;

  /*
   * Bring up first separate connections
   */
  session_bus_up ();
  /* if running with dbus-monitor, it claims the name :1.0 - so if we don't run with the monitor
   * emulate this
   */
  if (g_getenv ("G_DBUS_MONITOR") == NULL)
    {
      c1 = _g_bus_get_priv (G_BUS_TYPE_SESSION, NULL, NULL);
      g_assert (c1 != NULL);
      g_assert (!g_dbus_connection_is_closed (c1));
      g_object_unref (c1);
    }
  c1 = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
  g_assert (c1 != NULL);
  g_assert (!g_dbus_connection_is_closed (c1));
  g_assert_cmpstr (g_dbus_connection_get_unique_name (c1), ==, ":1.1");

  /*
   * Install two signal handlers for the first connection
   *
   *  - Listen to the signal "Foo" from :1.2 (e.g. c2)
   *  - Listen to the signal "Foo" from anyone (e.g. both c2 and c3)
   *
   * and then count how many times this signal handler was invoked.
   */
  s1 = g_dbus_connection_signal_subscribe (c1,
                                           ":1.2",
                                           "org.gtk.GDBus.ExampleInterface",
                                           "Foo",
                                           "/org/gtk/GDBus/ExampleInterface",
                                           NULL,
                                           test_connection_signal_handler,
                                           &count_s1,
                                           NULL);
  s2 = g_dbus_connection_signal_subscribe (c1,
                                           NULL, /* match any sender */
                                           "org.gtk.GDBus.ExampleInterface",
                                           "Foo",
                                           "/org/gtk/GDBus/ExampleInterface",
                                           NULL,
                                           test_connection_signal_handler,
                                           &count_s2,
                                           NULL);
  s3 = g_dbus_connection_signal_subscribe (c1,
                                           "org.freedesktop.DBus",  /* sender */
                                           "org.freedesktop.DBus",  /* interface */
                                           "NameOwnerChanged",      /* member */
                                           "/org/freedesktop/DBus", /* path */
                                           NULL,
                                           test_connection_signal_handler,
                                           &count_name_owner_changed,
                                           NULL);
  g_assert (s1 != 0);
  g_assert (s2 != 0);
  g_assert (s3 != 0);

  count_s1 = 0;
  count_s2 = 0;
  count_name_owner_changed = 0;

  /*
   * Bring up two other connections
   */
  c2 = _g_bus_get_priv (G_BUS_TYPE_SESSION, NULL, NULL);
  g_assert (c2 != NULL);
  g_assert (!g_dbus_connection_is_closed (c2));
  g_assert_cmpstr (g_dbus_connection_get_unique_name (c2), ==, ":1.2");
  c3 = _g_bus_get_priv (G_BUS_TYPE_SESSION, NULL, NULL);
  g_assert (c3 != NULL);
  g_assert (!g_dbus_connection_is_closed (c3));
  g_assert_cmpstr (g_dbus_connection_get_unique_name (c3), ==, ":1.3");

  /*
   * Make c2 emit "Foo" - we should catch it twice
   *
   * Note that there is no way to be sure that the signal subscriptions
   * on c1 are effective yet - for all we know, the AddMatch() messages
   * could sit waiting in a buffer somewhere between this process and
   * the message bus. And emitting signals on c2 (a completely other
   * socket!) will not necessarily change this.
   *
   * To ensure this is not the case, do a synchronous call on c1.
   */
  result = g_dbus_connection_call_sync (c1,
                                        "org.freedesktop.DBus",  /* bus name */
                                        "/org/freedesktop/DBus", /* object path */
                                        "org.freedesktop.DBus",  /* interface name */
                                        "GetId",                 /* method name */
                                        NULL,                    /* parameters */
                                        G_DBUS_CALL_FLAGS_NONE,
                                        -1,
                                        NULL,
                                        &error);
  g_assert_no_error (error);
  g_assert (result != NULL);
  g_variant_unref (result);
  /* now, emit the signal on c2 */
  ret = g_dbus_connection_emit_signal (c2,
                                       NULL, /* destination bus name */
                                       "/org/gtk/GDBus/ExampleInterface",
                                       "org.gtk.GDBus.ExampleInterface",
                                       "Foo",
                                       NULL,
                                       &error);
  g_assert_no_error (error);
  g_assert (ret);
  while (!(count_s1 == 1 && count_s2 == 1))
    g_main_loop_run (loop);
  g_assert_cmpint (count_s1, ==, 1);
  g_assert_cmpint (count_s2, ==, 1);

  /*
   * Make c3 emit "Foo" - we should catch it only once
   */
  ret = g_dbus_connection_emit_signal (c3,
                                       NULL, /* destination bus name */
                                       "/org/gtk/GDBus/ExampleInterface",
                                       "org.gtk.GDBus.ExampleInterface",
                                       "Foo",
                                       NULL,
                                       &error);
  g_assert_no_error (error);
  g_assert (ret);
  while (!(count_s1 == 1 && count_s2 == 2))
    g_main_loop_run (loop);
  g_assert_cmpint (count_s1, ==, 1);
  g_assert_cmpint (count_s2, ==, 2);

  /*
   * Also to check the total amount of NameOwnerChanged signals - use a 5 second ceiling
   * to avoid spinning forever
   */
  gboolean quit_mainloop_fired;
  guint quit_mainloop_id;
  quit_mainloop_fired = FALSE;
  quit_mainloop_id = g_timeout_add (5000, test_connection_signal_quit_mainloop, &quit_mainloop_fired);
  while (count_name_owner_changed != 2 && !quit_mainloop_fired)
    g_main_loop_run (loop);
  g_source_remove (quit_mainloop_id);
  g_assert_cmpint (count_s1, ==, 1);
  g_assert_cmpint (count_s2, ==, 2);
  g_assert_cmpint (count_name_owner_changed, ==, 2);

  g_dbus_connection_signal_unsubscribe (c1, s1);
  g_dbus_connection_signal_unsubscribe (c1, s2);
  g_dbus_connection_signal_unsubscribe (c1, s3);

  _g_object_wait_for_single_ref (c1);
  _g_object_wait_for_single_ref (c2);
  _g_object_wait_for_single_ref (c3);

  g_object_unref (c1);
  g_object_unref (c2);
  g_object_unref (c3);

  session_bus_down ();
}

/* ---------------------------------------------------------------------------------------------------- */

typedef struct
{
  guint num_handled;
  guint32 serial;
} FilterData;

static gboolean
filter_func (GDBusConnection *connection,
             GDBusMessage    *message,
             gpointer         user_data)
{
  FilterData *data = user_data;
  guint32 reply_serial;

  reply_serial = g_dbus_message_get_reply_serial (message);
  if (reply_serial == data->serial)
    data->num_handled += 1;

  return FALSE;
}

static void
test_connection_filter (void)
{
  GDBusConnection *c;
  FilterData data;
  GDBusMessage *m;
  GDBusMessage *r;
  GError *error;
  guint filter_id;

  memset (&data, '\0', sizeof (FilterData));

  session_bus_up ();

  error = NULL;
  c = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
  g_assert_no_error (error);
  g_assert (c != NULL);

  filter_id = g_dbus_connection_add_filter (c,
                                            filter_func,
                                            &data,
                                            NULL);

  m = g_dbus_message_new_method_call ("org.freedesktop.DBus", /* name */
                                      "/org/freedesktop/DBus", /* path */
                                      "org.freedesktop.DBus", /* interface */
                                      "GetNameOwner");
  g_dbus_message_set_body (m, g_variant_new ("(s)", "org.freedesktop.DBus"));
  error = NULL;
  g_dbus_connection_send_message (c, m, &data.serial, &error);
  g_assert_no_error (error);

  while (data.num_handled == 0)
    g_thread_yield ();

  g_dbus_connection_send_message (c, m, &data.serial, &error);
  g_assert_no_error (error);

  while (data.num_handled == 1)
    g_thread_yield ();

  r = g_dbus_connection_send_message_with_reply_sync (c,
                                                      m,
                                                      -1,
                                                      &data.serial,
                                                      NULL, /* GCancellable */
                                                      &error);
  g_assert_no_error (error);
  g_assert (r != NULL);
  g_object_unref (r);
  g_assert_cmpint (data.num_handled, ==, 3);

  g_dbus_connection_remove_filter (c, filter_id);

  r = g_dbus_connection_send_message_with_reply_sync (c,
                                                      m,
                                                      -1,
                                                      &data.serial,
                                                      NULL, /* GCancellable */
                                                      &error);
  g_assert_no_error (error);
  g_assert (r != NULL);
  g_object_unref (r);
  g_assert_cmpint (data.num_handled, ==, 3);

  _g_object_wait_for_single_ref (c);
  g_object_unref (c);
  g_object_unref (m);

  session_bus_down ();
}

/* ---------------------------------------------------------------------------------------------------- */

int
main (int   argc,
      char *argv[])
{
  g_type_init ();
  g_test_init (&argc, &argv, NULL);

  /* all the tests rely on a shared main loop */
  loop = g_main_loop_new (NULL, FALSE);

  /* all the tests use a session bus with a well-known address that we can bring up and down
   * using session_bus_up() and session_bus_down().
   */
  g_unsetenv ("DISPLAY");
  g_setenv ("DBUS_SESSION_BUS_ADDRESS", session_bus_get_temporary_address (), TRUE);

  g_test_add_func ("/gdbus/connection-life-cycle", test_connection_life_cycle);
  g_test_add_func ("/gdbus/connection-send", test_connection_send);
  g_test_add_func ("/gdbus/connection-signals", test_connection_signals);
  g_test_add_func ("/gdbus/connection-filter", test_connection_filter);
  return g_test_run();
}
