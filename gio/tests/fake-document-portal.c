/*
 * Copyright (C) 2019 Canonical Limited
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
 * Authors: James Henstridge <james.henstridge@canonical.com>
 */

/* A stub implementation of xdg-document-portal covering enough to
 * support g_document_portal_add_documents */

#include <glib.h>
#include <gio/gio.h>
#include <gio/gunixfdlist.h>

#include "fake-document-portal-generated.h"

static gboolean
on_handle_get_mount_point (FakeDocuments         *object,
                           GDBusMethodInvocation *invocation,
                           gpointer               user_data)
{
  fake_documents_complete_get_mount_point (object,
                                           invocation,
                                           "/document-portal");
  return TRUE;
}

static gboolean
on_handle_add_full (FakeDocuments         *object,
                    GDBusMethodInvocation *invocation,
                    GUnixFDList           *o_path_fds,
                    guint                  flags,
                    const gchar           *app_id,
                    const gchar * const   *permissions,
                    gpointer               user_data)
{
  const gchar **doc_ids = NULL;
  GVariant *extra_out = NULL;
  gsize length, i;

  if (o_path_fds != NULL)
    length = g_unix_fd_list_get_length (o_path_fds);
  else
    length = 0;

  doc_ids = g_new0 (const gchar *, length + 1  /* NULL terminator */);
  for (i = 0; i < length; i++)
    {
      doc_ids[i] = "document-id";
    }
  extra_out = g_variant_new_array (G_VARIANT_TYPE ("{sv}"), NULL, 0);

  fake_documents_complete_add_full (object,
                                    invocation,
                                    NULL,
                                    doc_ids,
                                    extra_out);

  g_free (doc_ids);

  return TRUE;
}

static void
on_bus_acquired (GDBusConnection *connection,
                 const gchar     *name,
                 gpointer         user_data)
{
  FakeDocuments *interface;
  GError *error = NULL;

  g_test_message ("Acquired a message bus connection");

  interface = fake_documents_skeleton_new ();
  g_signal_connect (interface,
                    "handle-get-mount-point",
                    G_CALLBACK (on_handle_get_mount_point),
                    NULL);
  g_signal_connect (interface,
                    "handle-add-full",
                    G_CALLBACK (on_handle_add_full),
                    NULL);

  g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (interface),
                                    connection,
                                    "/org/freedesktop/portal/documents",
                                    &error);
  g_assert_no_error (error);
}

static void
on_name_acquired (GDBusConnection *connection,
                  const gchar     *name,
                  gpointer         user_data)
{
  g_test_message ("Acquired the name %s", name);
}

static void
on_name_lost (GDBusConnection *connection,
              const gchar     *name,
              gpointer         user_data)
{
  g_test_message ("Lost the name %s", name);
}


gint
main (gint argc, gchar *argv[])
{
  GMainLoop *loop;
  guint id;

  g_log_writer_default_set_use_stderr (TRUE);

  loop = g_main_loop_new (NULL, FALSE);

  id = g_bus_own_name (G_BUS_TYPE_SESSION,
                       "org.freedesktop.portal.Documents",
                       G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT |
                       G_BUS_NAME_OWNER_FLAGS_REPLACE,
                       on_bus_acquired,
                       on_name_acquired,
                       on_name_lost,
                       loop,
                       NULL);

  g_main_loop_run (loop);

  g_bus_unown_name (id);
  g_main_loop_unref (loop);

  return 0;
}
