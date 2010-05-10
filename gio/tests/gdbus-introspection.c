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
/* Test introspection parser */
/* ---------------------------------------------------------------------------------------------------- */

static void
introspection_on_proxy_appeared (GDBusConnection *connection,
                                 const gchar     *name,
                                 const gchar     *name_owner,
                                 GDBusProxy      *proxy,
                                 gpointer         user_data)
{
  GError *error;
  const gchar *xml_data;
  GDBusNodeInfo *node_info;
  const GDBusInterfaceInfo *interface_info;
  const GDBusMethodInfo *method_info;
  const GDBusSignalInfo *signal_info;
  GVariant *result;

  error = NULL;

  /*
   * Invoke Introspect(), then parse the output.
   */
  result = g_dbus_proxy_call_sync (proxy,
                                   "org.freedesktop.DBus.Introspectable.Introspect",
                                   NULL,
                                   G_DBUS_CALL_FLAGS_NONE,
                                   -1,
                                   NULL,
                                   &error);
  g_assert_no_error (error);
  g_assert (result != NULL);
  g_variant_get (result, "(s)", &xml_data);

  node_info = g_dbus_node_info_new_for_xml (xml_data, &error);
  g_assert_no_error (error);
  g_assert (node_info != NULL);

  /* for now we only check a couple of things. TODO: check more things */

  interface_info = g_dbus_node_info_lookup_interface (node_info, "com.example.NonExistantInterface");
  g_assert (interface_info == NULL);

  interface_info = g_dbus_node_info_lookup_interface (node_info, "org.freedesktop.DBus.Introspectable");
  g_assert (interface_info != NULL);
  method_info = g_dbus_interface_info_lookup_method (interface_info, "NonExistantMethod");
  g_assert (method_info == NULL);
  method_info = g_dbus_interface_info_lookup_method (interface_info, "Introspect");
  g_assert (method_info != NULL);
  g_assert (method_info->in_args == NULL);
  g_assert (method_info->out_args != NULL);
  g_assert (method_info->out_args[0] != NULL);
  g_assert (method_info->out_args[1] == NULL);
  g_assert_cmpstr (method_info->out_args[0]->signature, ==, "s");

  interface_info = g_dbus_node_info_lookup_interface (node_info, "com.example.Frob");
  g_assert (interface_info != NULL);
  signal_info = g_dbus_interface_info_lookup_signal (interface_info, "TestSignal");
  g_assert (signal_info != NULL);
  g_assert (signal_info->args != NULL);
  g_assert (signal_info->args[0] != NULL);
  g_assert_cmpstr (signal_info->args[0]->signature, ==, "s");
  g_assert (signal_info->args[1] != NULL);
  g_assert_cmpstr (signal_info->args[1]->signature, ==, "o");
  g_assert (signal_info->args[2] != NULL);
  g_assert_cmpstr (signal_info->args[2]->signature, ==, "v");
  g_assert (signal_info->args[3] == NULL);

  g_dbus_node_info_unref (node_info);
  g_variant_unref (result);

  g_main_loop_quit (loop);
}

static void
introspection_on_proxy_vanished (GDBusConnection *connection,
                                 const gchar     *name,
                                 gpointer         user_data)
{
}

static void
test_introspection_parser (void)
{
  guint watcher_id;

  session_bus_up ();

  watcher_id = g_bus_watch_proxy (G_BUS_TYPE_SESSION,
                                  "com.example.TestService",
                                  G_BUS_NAME_WATCHER_FLAGS_NONE,
                                  "/com/example/TestObject",
                                  "com.example.Frob",
                                  G_TYPE_DBUS_PROXY,
                                  G_DBUS_PROXY_FLAGS_NONE,
                                  introspection_on_proxy_appeared,
                                  introspection_on_proxy_vanished,
                                  NULL,
                                  NULL);

  /* TODO: wait a bit for the bus to come up.. ideally session_bus_up() won't return
   * until one can connect to the bus but that's not how things work right now
   */
  usleep (500 * 1000);
  /* this is safe; testserver will exit once the bus goes away */
  g_assert (g_spawn_command_line_async ("./gdbus-testserver.py", NULL));

  g_main_loop_run (loop);

  g_bus_unwatch_proxy (watcher_id);

  /* tear down bus */
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

  g_test_add_func ("/gdbus/introspection-parser", test_introspection_parser);
  return g_test_run();
}
