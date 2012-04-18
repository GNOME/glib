/* GLib testing framework examples and tests
 *
 * Copyright (C) 2008-2012 Red Hat, Inc.
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

#include <locale.h>
#include <gio/gio.h>

#include <string.h>
#include <unistd.h>
#include <dbus/dbus.h>

#include "gdbus-tests.h"

#ifdef G_OS_UNIX
#include <gio/gunixconnection.h>
#include <gio/gnetworkingprivate.h>
#include <gio/gunixsocketaddress.h>
#include <gio/gunixfdlist.h>
#endif

/* ---------------------------------------------------------------------------------------------------- */

static gboolean
on_allow_mechanism (GDBusAuthObserver *observer,
                    const gchar       *mechanism,
                    gpointer           user_data)
{
  const gchar *mechanism_to_allow = user_data;
  if (g_strcmp0 (mechanism, mechanism_to_allow) == 0)
    return TRUE;
  else
    return FALSE;
}

static void
auth_client_mechanism (const gchar *mechanism)
{
  gchar *address = NULL;
  GDBusConnection *c = NULL;
  GError *error = NULL;
  GDBusAuthObserver *auth_observer;

  address = g_dbus_address_get_for_bus_sync (G_BUS_TYPE_SESSION, NULL, &error);
  g_assert_no_error (error);
  g_assert (address != NULL);

  auth_observer = g_dbus_auth_observer_new ();

  g_signal_connect (auth_observer,
                    "allow-mechanism",
                    G_CALLBACK (on_allow_mechanism),
                    (gpointer) mechanism);

  c = g_dbus_connection_new_for_address_sync (address,
                                              G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT |
                                              G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION,
                                              auth_observer,
                                              NULL,
                                              &error);
  g_assert_no_error (error);
  g_assert (c != NULL);

  g_free (address);
  g_object_unref (c);
  g_object_unref (auth_observer);
}

static void
auth_client_external (void)
{
  auth_client_mechanism ("EXTERNAL");
}

static void
auth_client_dbus_cookie_sha1 (void)
{
  auth_client_mechanism ("DBUS_COOKIE_SHA1");
}

/* ---------------------------------------------------------------------------------------------------- */

static gboolean
on_new_connection (GDBusServer     *server,
                   GDBusConnection *connection,
                   gpointer         user_data)
{
  GMainLoop *loop = user_data;
  g_main_loop_quit (loop);
  return FALSE;
}

static gboolean
on_timeout (gpointer user_data)
{
  g_error ("Timeout waiting for client");
  g_assert_not_reached ();
  return FALSE;
}

static gpointer
dbus_1_client_thread_func (gpointer user_data)
{
  const gchar *address = user_data;
  gchar *str_stdout;
  gchar *str_stderr;
  gint exit_status;
  gboolean rc;
  gchar *command_line;
  GError *error;

  // For the dbus part, just use dbus-monitor to connect(1)
  command_line = g_strdup_printf ("dbus-monitor --address %s", address);

  error = NULL;
  rc = g_spawn_command_line_sync (command_line,
                                  &str_stdout,
                                  &str_stderr,
                                  &exit_status,
                                  &error);
  g_assert_no_error (error);
  g_assert (rc);
  g_free (str_stdout);
  g_free (str_stderr);
  g_free (command_line);
  return NULL;
}

static void
auth_server_mechanism (const gchar *mechanism)
{
  gchar *addr;
  gchar *guid;
  GDBusServer *server;
  GDBusAuthObserver *auth_observer;
  GMainLoop *loop;
  GError *error;
  GThread *client_thread;
  GDBusServerFlags flags;

  /* Skip DBUS_COOKIE_SHA1 test unless we have a sufficiently new version of dbus-1 */
  if (g_strcmp0 (mechanism, "DBUS_COOKIE_SHA1") == 0)
    {
      int dbus_major, dbus_minor, dbus_micro;
      dbus_get_version (&dbus_major, &dbus_minor, &dbus_micro);
      if (!((dbus_major == 1 && dbus_minor == 4 && dbus_micro >= 21) ||
            (dbus_major == 1 && dbus_minor == 5 && dbus_micro >= 13) ||
            (dbus_major == 1 && dbus_minor > 5) ||
            (dbus_major > 1)))
        {
          g_print ("Your libdbus-1 library is too old (version %d.%d.%d) so skipping DBUS_COOKIE_SHA1 test. See https://bugs.freedesktop.org/show_bug.cgi?id=48580 for more details.\n",
                   dbus_major, dbus_minor, dbus_micro);
          return;
        }
    }

  guid = g_dbus_generate_guid ();

#ifdef G_OS_UNIX
  if (g_unix_socket_address_abstract_names_supported ())
    {
      addr = g_strdup ("unix:tmpdir=/tmp/gdbus-test-");
    }
  else
    {
      gchar *tmpdir;
      tmpdir = g_dir_make_tmp ("gdbus-test-XXXXXX", NULL);
      addr = g_strdup_printf ("unix:tmpdir=%s", tmpdir);
      g_free (tmpdir);
    }
#else
  addr = g_strdup ("nonce-tcp:");
#endif

  loop = g_main_loop_new (NULL, FALSE);

  auth_observer = g_dbus_auth_observer_new ();

  flags = G_DBUS_SERVER_FLAGS_NONE;
  if (g_strcmp0 (mechanism, "ANONYMOUS") == 0)
    flags |= G_DBUS_SERVER_FLAGS_AUTHENTICATION_ALLOW_ANONYMOUS;

  error = NULL;
  server = g_dbus_server_new_sync (addr,
                                   flags,
                                   guid,
                                   auth_observer,
                                   NULL, /* cancellable */
                                   &error);
  g_assert_no_error (error);
  g_assert (server != NULL);

  g_signal_connect (auth_observer,
                    "allow-mechanism",
                    G_CALLBACK (on_allow_mechanism),
                    (gpointer) mechanism);

  g_signal_connect (server,
                    "new-connection",
                    G_CALLBACK (on_new_connection),
                    loop);

  g_dbus_server_start (server);

  g_timeout_add_seconds (5, on_timeout, NULL);

  /* run the libdbus-1 client in a thread */
  client_thread = g_thread_new ("dbus-1-client-thread",
                                dbus_1_client_thread_func,
                                (gpointer) g_dbus_server_get_client_address (server));

  g_main_loop_run (loop);

  g_dbus_server_stop (server);

  g_thread_join (client_thread);

  g_free (addr);
  g_free (guid);
  g_object_unref (auth_observer);
  g_object_unref (server);
}

static void
auth_server_anonymous (void)
{
  auth_server_mechanism ("ANONYMOUS");
}

static void
auth_server_external (void)
{
  auth_server_mechanism ("EXTERNAL");
}

static void
auth_server_dbus_cookie_sha1 (void)
{
  auth_server_mechanism ("DBUS_COOKIE_SHA1");
}

/* ---------------------------------------------------------------------------------------------------- */

int
main (int   argc,
      char *argv[])
{
  gint ret;

  setlocale (LC_ALL, "C");

  g_type_init ();
  g_test_init (&argc, &argv, NULL);

  session_bus_up ();

  g_test_add_func ("/gdbus/auth/client/EXTERNAL",         auth_client_external);
  g_test_add_func ("/gdbus/auth/client/DBUS_COOKIE_SHA1", auth_client_dbus_cookie_sha1);
  g_test_add_func ("/gdbus/auth/server/ANONYMOUS",        auth_server_anonymous);
  g_test_add_func ("/gdbus/auth/server/EXTERNAL",         auth_server_external);
  g_test_add_func ("/gdbus/auth/server/DBUS_COOKIE_SHA1", auth_server_dbus_cookie_sha1);

  ret = g_test_run();

  /* tear down bus */
  session_bus_down ();

  return ret;
}

