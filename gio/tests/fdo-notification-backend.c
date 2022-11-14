/*
 * Copyright © 2022 Endless OS Foundation, LLC
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
 *
 * Author: Philip Withnall <pwithnall@endlessos.org>
 */

#include <gio/gio.h>
#include <locale.h>

#include <gio/giomodule-priv.h>
#include "gio/gnotificationbackend.h"


static GNotificationBackend *
construct_backend (GApplication **app_out)
{
  GApplication *app = NULL;
  GType fdo_type = G_TYPE_INVALID;
  GNotificationBackend *backend = NULL;
  GError *local_error = NULL;

  /* Construct the app first and withdraw a notification, to ensure that IO modules are loaded. */
  app = g_application_new ("org.gtk.TestApplication", G_APPLICATION_DEFAULT_FLAGS);
  g_application_register (app, NULL, &local_error);
  g_assert_no_error (local_error);
  g_application_withdraw_notification (app, "org.gtk.TestApplication.NonexistentNotification");

  fdo_type = g_type_from_name ("GFdoNotificationBackend");
  g_assert_cmpuint (fdo_type, !=, G_TYPE_INVALID);

  /* Replicate the behaviour from g_notification_backend_new_default(), which is
   * not exported publicly so can‘t be easily used in the test. */
  backend = g_object_new (fdo_type, NULL);
  backend->application = app;
  backend->dbus_connection = g_application_get_dbus_connection (app);
  if (backend->dbus_connection)
    g_object_ref (backend->dbus_connection);

  if (app_out != NULL)
    *app_out = g_object_ref (app);

  g_clear_object (&app);

  return g_steal_pointer (&backend);
}

static void
test_construction (void)
{
  GNotificationBackend *backend = NULL;
  GApplication *app = NULL;
  GTestDBus *bus = NULL;

  g_test_message ("Test constructing a GFdoNotificationBackend");

  /* Set up a test session bus and connection. */
  bus = g_test_dbus_new (G_TEST_DBUS_NONE);
  g_test_dbus_up (bus);

  backend = construct_backend (&app);
  g_assert_nonnull (backend);

  g_application_quit (app);
  g_clear_object (&app);
  g_clear_object (&backend);

  g_test_dbus_down (bus);
  g_clear_object (&bus);
}

int
main (int   argc,
      char *argv[])
{
  setlocale (LC_ALL, "");

  /* Force use of the FDO backend */
  g_setenv ("GNOTIFICATION_BACKEND", "freedesktop", TRUE);

  g_test_init (&argc, &argv, NULL);

  /* Make sure we don’t send notifications to the actual D-Bus session. */
  g_test_dbus_unset ();

  g_test_add_func ("/fdo-notification-backend/construction", test_construction);

  return g_test_run ();
}
