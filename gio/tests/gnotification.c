/*
 * Copyright © 2013 Lars Uebernickel
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2 of the licence or (at
 * your option) any later version.
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
 * Authors: Lars Uebernickel <lars@uebernic.de>
 */

#include <glib.h>

#include "gnotification-server.h"
#include "gdbus-sessionbus.h"

static void
activate_app (GApplication *application,
              gpointer      user_data)
{
  GNotification *notification;

  notification = g_notification_new ("Test");

  g_application_send_notification (application, "test1", notification);
  g_application_send_notification (application, "test2", notification);
  g_application_withdraw_notification (application, "test1");
  g_application_send_notification (application, "test3", notification);

  g_dbus_connection_flush_sync (g_application_get_dbus_connection (application), NULL, NULL);

  g_object_unref (notification);
}

static void
notification_received (GNotificationServer *server,
                       const gchar         *app_id,
                       const gchar         *notification_id,
                       GVariant            *notification,
                       gpointer             user_data)
{
  gint *count = user_data;
  const gchar *title;

  g_assert_cmpstr (app_id, ==, "org.gtk.TestApplication");

  switch (*count)
    {
    case 0:
      g_assert_cmpstr (notification_id, ==, "test1");
      g_assert (g_variant_lookup (notification, "title", "&s", &title));
      g_assert_cmpstr (title, ==, "Test");
      break;

    case 1:
      g_assert_cmpstr (notification_id, ==, "test2");
      break;

    case 2:
      g_assert_cmpstr (notification_id, ==, "test3");

      g_notification_server_stop (server);
      break;
    }

  (*count)++;
}

static void
notification_removed (GNotificationServer *server,
                      const gchar         *app_id,
                      const gchar         *notification_id,
                      gpointer             user_data)
{
  gint *count = user_data;

  g_assert_cmpstr (app_id, ==, "org.gtk.TestApplication");
  g_assert_cmpstr (notification_id, ==, "test1");

  (*count)++;
}

static void
server_notify_is_running (GObject    *object,
                          GParamSpec *pspec,
                          gpointer    user_data)
{
  GMainLoop *loop = user_data;
  GNotificationServer *server = G_NOTIFICATION_SERVER (object);

  if (g_notification_server_get_is_running (server))
    {
      GApplication *app;

      app = g_application_new ("org.gtk.TestApplication", G_APPLICATION_FLAGS_NONE);
      g_signal_connect (app, "activate", G_CALLBACK (activate_app), NULL);

      g_application_run (app, 0, NULL);

      g_object_unref (app);
    }
  else
    {
      g_main_loop_quit (loop);
    }
}

static gboolean
timeout (gpointer user_data)
{
  GNotificationServer *server = user_data;

  g_notification_server_stop (server);

  return G_SOURCE_REMOVE;
}

static void
basic (void)
{
  GNotificationServer *server;
  GMainLoop *loop;
  gint received_count = 0;
  gint removed_count = 0;

  session_bus_up ();

  loop = g_main_loop_new (NULL, FALSE);

  server = g_notification_server_new ();
  g_signal_connect (server, "notification-received", G_CALLBACK (notification_received), &received_count);
  g_signal_connect (server, "notification-removed", G_CALLBACK (notification_removed), &removed_count);
  g_signal_connect (server, "notify::is-running", G_CALLBACK (server_notify_is_running), loop);
  g_timeout_add_seconds (1, timeout, server);

  g_main_loop_run (loop);

  g_assert_cmpint (received_count, ==, 3);
  g_assert_cmpint (removed_count, ==, 1);

  g_object_unref (server);
  g_main_loop_unref (loop);
  session_bus_stop ();
}

int main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/gnotification/basic", basic);

  return g_test_run ();
}
