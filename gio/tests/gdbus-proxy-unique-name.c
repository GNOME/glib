/* GLib testing framework examples and tests
 *
 * Copyright (C) 2008-2010 Red Hat, Inc.
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
 *
 * Author: Cosimo Cecchi <cosimoc@gnome.org>
 */

#include <gio/gio.h>
#include <unistd.h>
#include <string.h>

#include "gdbus-tests.h"

/* all tests rely on a shared mainloop */
static GMainLoop *loop = NULL;

/* ---------------------------------------------------------------------------------------------------- */

static void
proxy_new_cb (GObject       *source_object,
              GAsyncResult  *res,
              gpointer       user_data)
{
  GDBusProxy **ret = user_data;
  GError *error;

  error = NULL;
  *ret = g_dbus_proxy_new_finish (res, &error);
  g_assert_no_error (error);
  g_assert_nonnull (ret);

  g_main_loop_quit (loop);
}

static void
test_proxy_unique_name (void)
{
  GDBusProxy *wp;
  GDBusProxy *p;
  GDBusProxy *ap;
  GDBusConnection *c;
  GError *error;
  gchar *name_owner;
  gchar **property_names;
  GVariant *variant;
  GVariant *result;
  char *unique_name;

  session_bus_up ();

  error = NULL;
  c = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
  g_assert_no_error (error);
  g_assert_nonnull (c);

  /* use a proxy to the well-known name to set things up */
  wp = g_dbus_proxy_new_sync (c,
                              G_DBUS_PROXY_FLAGS_NONE,
                              NULL,                      /* GDBusInterfaceInfo* */
                              "com.example.TestService", /* name */
                              "/com/example/TestObject", /* object path */
                              "com.example.Frob",        /* interface name */
                              NULL,                      /* GCancellable */
                              &error);
  g_assert_no_error (error);

  /* this is safe; testserver will exit once the bus goes away */
  g_assert_true (g_spawn_command_line_async (g_test_get_filename (G_TEST_BUILT, "gdbus-testserver", NULL), NULL));

  /* check that we get the notify::g-name-owner signal */
  _g_assert_property_notify (wp, "g-name-owner");

  /* now get the unique name of testserver's connection */
  unique_name = g_dbus_proxy_get_name_owner (wp);

  /* if we create another a proxy with the service being available, check that
   * it has a name owner and properties
   */
  error = NULL;
  p = g_dbus_proxy_new_sync (c,
                              G_DBUS_PROXY_FLAGS_NONE,
                              NULL,                      /* GDBusInterfaceInfo* */
                              unique_name,               /* name */
                              "/com/example/TestObject", /* object path */
                              "com.example.Frob",        /* interface name */
                              NULL,                      /* GCancellable */
                              &error);
  g_assert_no_error (error);
  name_owner = g_dbus_proxy_get_name_owner (p);
  property_names = g_dbus_proxy_get_cached_property_names (p);
  g_assert_true (g_dbus_is_unique_name (name_owner));
  g_assert_nonnull (property_names);
  g_assert_cmpint (g_strv_length (property_names), >, 0);
  g_free (name_owner);
  g_strfreev (property_names);

  /* also for async: we should have a name owner and cached properties */
  g_dbus_proxy_new (c,
                    G_DBUS_PROXY_FLAGS_NONE,
                    NULL,                      /* GDBusInterfaceInfo* */
                    unique_name,               /* name */
                    "/com/example/TestObject", /* object path */
                    "com.example.Frob",        /* interface name */
                    NULL,                      /* GCancellable */
                    (GAsyncReadyCallback) proxy_new_cb,
                    &ap);
  g_main_loop_run (loop);
  name_owner = g_dbus_proxy_get_name_owner (ap);
  property_names = g_dbus_proxy_get_cached_property_names (ap);
  g_assert_true (g_dbus_is_unique_name (name_owner));
  g_assert_nonnull (property_names);
  g_assert_cmpint (g_strv_length (property_names), >, 0);
  g_free (name_owner);
  g_strfreev (property_names);

  /* Check property value is the initial value */
  variant = g_dbus_proxy_get_cached_property (p, "y");
  g_assert_nonnull (variant);
  g_assert_cmpint (g_variant_get_byte (variant), ==, 1);
  g_variant_unref (variant);
  variant = g_dbus_proxy_get_cached_property (ap, "y");
  g_assert_nonnull (variant);
  g_assert_cmpint (g_variant_get_byte (variant), ==, 1);
  g_variant_unref (variant);

  /* Check that properties are updated on p */
  result = g_dbus_proxy_call_sync (p,
                                   "FrobSetProperty",
                                   g_variant_new ("(sv)",
                                                  "y",
                                                  g_variant_new_byte (42)),
                                   G_DBUS_CALL_FLAGS_NONE,
                                   -1,
                                   NULL,
                                   &error);
  g_assert_no_error (error);
  g_assert_nonnull (result);
  g_assert_cmpstr (g_variant_get_type_string (result), ==, "()");
  g_variant_unref (result);
  _g_assert_signal_received (p, "g-properties-changed");
  variant = g_dbus_proxy_get_cached_property (p, "y");
  g_assert_nonnull (variant);
  g_assert_cmpint (g_variant_get_byte (variant), ==, 42);
  g_variant_unref (variant);
  variant = g_dbus_proxy_get_cached_property (ap, "y");
  g_assert_nonnull (variant);
  g_assert_cmpint (g_variant_get_byte (variant), ==, 42);
  g_variant_unref (variant);

  /* Nuke the service and check that we get the signal and then don't
   * have a name owner nor any cached properties
   */
  result = g_dbus_proxy_call_sync (p,
                                   "Quit",
                                   NULL,
                                   G_DBUS_CALL_FLAGS_NONE,
                                   -1,
                                   NULL,
                                   &error);
  g_assert_no_error (error);
  g_assert_nonnull (result);
  g_assert_cmpstr (g_variant_get_type_string (result), ==, "()");
  g_variant_unref (result);
  /* and wait... */
  _g_assert_property_notify (p, "g-name-owner");
  /* now we shouldn't have a name owner nor any cached properties */
  g_assert_cmpstr (g_dbus_proxy_get_name_owner (p), ==, NULL);
  g_assert_null (g_dbus_proxy_get_cached_property_names (p));
  g_assert_null (g_dbus_proxy_get_cached_property (p, "y"));

  g_object_unref (p);
  g_object_unref (ap);

  g_object_unref (wp);
  g_free (unique_name);

  g_object_unref (c);

  /* tear down bus */
  session_bus_down ();
}

/* ---------------------------------------------------------------------------------------------------- */

int
main (int   argc,
      char *argv[])
{
  gint ret;

  g_test_init (&argc, &argv, NULL);

  /* all the tests rely on a shared main loop */
  loop = g_main_loop_new (NULL, FALSE);

  g_test_dbus_unset ();

  g_test_add_func ("/gdbus/proxy-unique-name", test_proxy_unique_name);

  ret = g_test_run();
  return ret;
}
