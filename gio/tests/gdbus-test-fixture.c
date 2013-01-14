/* gdbus-test-fixture.c - Test covering activation of in-tree servers.
 *
 * Copyright (C) 2012 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 * Authors: Tristan Van Berkom <tristanvb@openismus.com>
 */

#include <gio/gio.h>
#include "gdbus-example-objectmanager-generated.h"

typedef struct {
  GTestDBus *dbus;
  GDBusObjectManager *manager;
} TestFixture;

static void
fixture_setup (TestFixture *fixture, gconstpointer unused)
{
  /* Create the global dbus-daemon for this test suite */
  fixture->dbus = g_test_dbus_new (G_TEST_DBUS_NONE);

  /* Add the private directory with our in-tree service files */
  g_test_dbus_add_service_dir (fixture->dbus, TEST_SERVICES);

  /* Start the private D-Bus daemon */
  g_test_dbus_up (fixture->dbus);
}

static void
fixture_teardown (TestFixture *fixture, gconstpointer unused)
{
  if (fixture->manager)
    g_object_unref (fixture->manager);

  /* Stop the private D-Bus daemon */
  g_test_dbus_down (fixture->dbus);

  g_object_unref (fixture->dbus);
}

/* The gdbus-example-objectmanager-server exports 10 objects,
 * to test the server has actually activated, let's ensure
 * that 10 objects exist.
 */
static void
assert_ten_objects (GDBusObjectManager *manager)
{
  GList *objects;

  objects = g_dbus_object_manager_get_objects (manager);

  g_assert_cmpint (g_list_length (objects), ==, 10);
  g_list_free_full (objects, g_object_unref);
}

static void
test_gtest_dbus (TestFixture *fixture, gconstpointer unused)
{

  GError *error = NULL;

  fixture->manager =
    example_object_manager_client_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                                    G_DBUS_OBJECT_MANAGER_CLIENT_FLAGS_NONE,
                                                    "org.gtk.GDBus.Examples.ObjectManager",
                                                    "/example/Animals",
                                                    NULL, /* GCancellable */
                                                    &error);
  if (fixture->manager == NULL)
    g_error ("Error getting object manager client: %s", error->message);

  assert_ten_objects (fixture->manager);
}

int
main (int   argc,
      char *argv[])
{
#if !GLIB_CHECK_VERSION (2, 35, 1)
  g_type_init ();
#endif
  g_test_init (&argc, &argv, NULL);

  /* Ensure that we can bring the GTestDBus up and down a hand full of times
   * in a row, each time successfully activating the in-tree service
   */
  g_test_add ("/GTestDBus/Cycle1", TestFixture, NULL,
  	      fixture_setup, test_gtest_dbus, fixture_teardown);
  g_test_add ("/GTestDBus/Cycle2", TestFixture, NULL,
  	      fixture_setup, test_gtest_dbus, fixture_teardown);
  g_test_add ("/GTestDBus/Cycle3", TestFixture, NULL,
  	      fixture_setup, test_gtest_dbus, fixture_teardown);
  g_test_add ("/GTestDBus/Cycle4", TestFixture, NULL,
  	      fixture_setup, test_gtest_dbus, fixture_teardown);
  g_test_add ("/GTestDBus/Cycle5", TestFixture, NULL,
  	      fixture_setup, test_gtest_dbus, fixture_teardown);
  
  return g_test_run ();
}
