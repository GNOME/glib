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

#include "fake-document-portal.h"
#include "fake-document-portal-generated.h"

struct _GFakeDocumentPortalThread
{
  GObject parent_instance;

  char *address;  /* (not nullable) */
  GCancellable *cancellable;  /* (not nullable) (owned) */
  GThread *thread;  /* (not nullable) (owned) */
  GCond cond;  /* (mutex mutex) */
  GMutex mutex;
  gboolean ready;  /* (mutex mutex) */
};

G_DEFINE_FINAL_TYPE (GFakeDocumentPortalThread, g_fake_document_portal_thread, G_TYPE_OBJECT)

static void g_fake_document_portal_thread_finalize (GObject *object);

static void
g_fake_document_portal_thread_class_init (GFakeDocumentPortalThreadClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = g_fake_document_portal_thread_finalize;
}

static void
g_fake_document_portal_thread_init (GFakeDocumentPortalThread *self)
{
  self->cancellable = g_cancellable_new ();
  g_cond_init (&self->cond);
  g_mutex_init (&self->mutex);
}

static void
g_fake_document_portal_thread_finalize (GObject *object)
{
  GFakeDocumentPortalThread *self = G_FAKE_DOCUMENT_PORTAL_THREAD (object);

  g_assert (self->thread == NULL);  /* should already have been joined */

  g_mutex_clear (&self->mutex);
  g_cond_clear (&self->cond);
  g_clear_object (&self->cancellable);
  g_clear_pointer (&self->address, g_free);

  G_OBJECT_CLASS (g_fake_document_portal_thread_parent_class)->finalize (object);
}

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
on_name_acquired (GDBusConnection *connection,
                  const gchar     *name,
                  gpointer         user_data)
{
  GFakeDocumentPortalThread *self = G_FAKE_DOCUMENT_PORTAL_THREAD (user_data);

  g_test_message ("Acquired the name %s", name);

  g_mutex_lock (&self->mutex);
  self->ready = TRUE;
  g_cond_signal (&self->cond);
  g_mutex_unlock (&self->mutex);
}

static void
on_name_lost (GDBusConnection *connection,
              const gchar     *name,
              gpointer         user_data)
{
  g_test_message ("Lost the name %s", name);
}

static gboolean
cancelled_cb (GCancellable *cancellable,
              gpointer      user_data)
{
  g_test_message ("fake-document-portal cancelled");
  return G_SOURCE_CONTINUE;
}

static gpointer
fake_document_portal_thread_cb (gpointer user_data)
{
  GFakeDocumentPortalThread *self = G_FAKE_DOCUMENT_PORTAL_THREAD (user_data);
  GMainContext *context = NULL;
  GDBusConnection *connection = NULL;
  GSource *source = NULL;
  guint id;
  FakeDocuments *interface = NULL;
  GError *local_error = NULL;

  context = g_main_context_new ();
  g_main_context_push_thread_default (context);

  connection = g_dbus_connection_new_for_address_sync (self->address,
                                                       G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT |
                                                       G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION,
                                                       NULL,
                                                       self->cancellable,
                                                       &local_error);
  g_assert_no_error (local_error);

  /* Listen for cancellation. The source will wake up the context iteration
   * which can then re-check its exit condition below. */
  source = g_cancellable_source_new (self->cancellable);
  g_source_set_callback (source, G_SOURCE_FUNC (cancelled_cb), NULL, NULL);
  g_source_attach (source, context);
  g_source_unref (source);

  /* Set up the interface */
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
                                    &local_error);
  g_assert_no_error (local_error);

  /* Own the portal name */
  id = g_bus_own_name_on_connection (connection,
                                     "org.freedesktop.portal.Documents",
                                     G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT |
                                     G_BUS_NAME_OWNER_FLAGS_REPLACE,
                                     on_name_acquired,
                                     on_name_lost,
                                     self,
                                     NULL);

  while (!g_cancellable_is_cancelled (self->cancellable))
    g_main_context_iteration (context, TRUE);

  g_bus_unown_name (id);
  g_dbus_interface_skeleton_unexport (G_DBUS_INTERFACE_SKELETON (interface));
  g_clear_object (&interface);
  g_clear_object (&connection);
  g_main_context_pop_thread_default (context);
  g_clear_pointer (&context, g_main_context_unref);

  return NULL;
}

/*
 * Create a new #GFakeDocumentPortalThread. The thread isn’t started until
 * g_fake_document_portal_thread_run() is called on it.
 *
 * Returns: (transfer full): the new fake document portal wrapper
 */
GFakeDocumentPortalThread *
g_fake_document_portal_thread_new (const char *address)
{
  GFakeDocumentPortalThread *self = g_object_new (G_TYPE_FAKE_DOCUMENT_PORTAL_THREAD, NULL);
  self->address = g_strdup (address);
  return g_steal_pointer (&self);
}

/*
 * Start a worker thread which will run a fake
 * `org.freedesktop.portal.Documents` portal on the bus at @address. This is
 * intended to be used with #GTestDBus to mock up a portal from within a unit
 * test process, rather than relying on D-Bus activation of a mock portal
 * subprocess.
 *
 * It blocks until the thread has owned its D-Bus name and is ready to handle
 * requests.
 */
void
g_fake_document_portal_thread_run (GFakeDocumentPortalThread *self)
{
  g_return_if_fail (G_IS_FAKE_DOCUMENT_PORTAL_THREAD (self));
  g_return_if_fail (self->thread == NULL);

  self->thread = g_thread_new ("fake-document-portal", fake_document_portal_thread_cb, self);

  /* Block until the thread is ready. */
  g_mutex_lock (&self->mutex);
  while (!self->ready)
    g_cond_wait (&self->cond, &self->mutex);
  g_mutex_unlock (&self->mutex);
}

/* Stop and join a worker thread started with fake_document_portal_thread_run().
 * Blocks until the thread has stopped and joined.
 *
 * Once this has been called, it’s safe to drop the final reference on @self. */
void
g_fake_document_portal_thread_stop (GFakeDocumentPortalThread *self)
{
  g_return_if_fail (G_IS_FAKE_DOCUMENT_PORTAL_THREAD (self));
  g_return_if_fail (self->thread != NULL);

  g_cancellable_cancel (self->cancellable);
  g_thread_join (g_steal_pointer (&self->thread));
}
