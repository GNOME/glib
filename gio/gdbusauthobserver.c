/* GDBus - GLib D-Bus Library
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

#include "config.h"

#include "gdbusauthobserver.h"
#include "gio-marshal.h"
#include "gcredentials.h"
#include "gioenumtypes.h"
#include "giostream.h"

#include "glibintl.h"
#include "gioalias.h"

/**
 * SECTION:gdbusauthobserver
 * @short_description: Object used for authenticating connections
 * @include: gio/gio.h
 *
 * The #GDBusAuthObserver type provides a mechanism for participating
 * in how a #GDBusServer (or a #GDBusConnection) authenticates remote
 * peers. Simply instantiate a #GDBusAuthObserver and connect to the
 * signals you are interested in. Note that new signals may be added
 * in the future
 *
 * For example, if you only want to allow D-Bus connections from
 * processes owned by the same uid as the server, you would do this:
 * <example id="auth-observer"><title>Controlling Authentication</title><programlisting>
 * static gboolean
 * on_deny_authenticated_peer (GDBusAuthObserver *observer,
 *                             GIOStream         *stream,
 *                             GCredentials      *credentials,
 *                             gpointer           user_data)
 * {
 *   gboolean deny;
 *   deny = TRUE;
 *   if (credentials != NULL &&
 *       g_credentials_has_unix_user (credentials) &&
 *       g_credentials_get_unix_user (credentials) == getuid ())
 *     deny = FALSE;
 *   return deny;
 * }
 *
 * static gboolean
 * on_new_connection (GDBusServer     *server,
 *                    GDBusConnection *connection,
 *                    gpointer         user_data)
 * {
 *   /<!-- -->* Guaranteed here that @connection is from a process owned by the same user *<!-- -->/
 * }
 *
 * [...]
 *
 * GDBusAuthObserver *observer;
 * GDBusServer *server;
 * GError *error;
 *
 * error = NULL;
 * observer = g_dbus_auth_observer_new ();
 * server = g_dbus_server_new_sync ("unix:tmpdir=/tmp/my-app-name",
 *                                  G_DBUS_SERVER_FLAGS_NONE,
 *                                  observer,
 *                                  NULL, /<!-- -->* GCancellable *<!-- -->/
 *                                  &error);
 * g_signal_connect (observer,
 *                   "deny-authenticated-peer",
 *                   G_CALLBACK (on_deny_authenticated_peer),
 *                   NULL);
 * g_signal_connect (server,
 *                   "new-connection",
 *                   G_CALLBACK (on_new_connection),
 *                   NULL);
 * g_object_unref (observer);
 * g_dbus_server_start (server);
 * </programlisting></example>
 */

struct _GDBusAuthObserverPrivate
{
  gint foo;
};

enum
{
  DENY_AUTHENTICATED_PEER_SIGNAL,
  LAST_SIGNAL,
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GDBusAuthObserver, g_dbus_auth_observer, G_TYPE_OBJECT);

/* ---------------------------------------------------------------------------------------------------- */

static void
g_dbus_auth_observer_finalize (GObject *object)
{
  //GDBusAuthObserver *observer = G_DBUS_AUTH_OBSERVER (object);

  if (G_OBJECT_CLASS (g_dbus_auth_observer_parent_class)->finalize != NULL)
    G_OBJECT_CLASS (g_dbus_auth_observer_parent_class)->finalize (object);
}

static gboolean
g_dbus_auth_observer_deny_authenticated_peer_real (GDBusAuthObserver  *observer,
                                                   GIOStream          *stream,
                                                   GCredentials       *credentials)
{
  return FALSE;
}

static void
g_dbus_auth_observer_class_init (GDBusAuthObserverClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize     = g_dbus_auth_observer_finalize;

  klass->deny_authenticated_peer = g_dbus_auth_observer_deny_authenticated_peer_real;

  /**
   * GDBusAuthObserver::deny-authenticated-peer:
   * @observer: The #GDBusAuthObserver emitting the signal.
   * @stream: A #GIOStream for the #GDBusConnection.
   * @credentials: Credentials received from the peer or %NULL.
   *
   * Emitted to check if a peer that is successfully authenticated
   * should be denied.
   *
   * Returns: %TRUE if the peer should be denied, %FALSE otherwise.
   *
   * Since: 2.26
   */
  signals[DENY_AUTHENTICATED_PEER_SIGNAL] =
    g_signal_new ("deny-authenticated-peer",
                  G_TYPE_DBUS_AUTH_OBSERVER,
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GDBusAuthObserverClass, deny_authenticated_peer),
                  g_signal_accumulator_true_handled,
                  NULL, /* accu_data */
                  _gio_marshal_BOOLEAN__OBJECT_OBJECT,
                  G_TYPE_BOOLEAN,
                  2,
                  G_TYPE_IO_STREAM,
                  G_TYPE_CREDENTIALS);


  g_type_class_add_private (klass, sizeof (GDBusAuthObserverPrivate));
}

static void
g_dbus_auth_observer_init (GDBusAuthObserver *observer)
{
  /* not used for now */
  observer->priv = G_TYPE_INSTANCE_GET_PRIVATE (observer,
                                                G_TYPE_DBUS_AUTH_OBSERVER,
                                                GDBusAuthObserverPrivate);;
}

/**
 * g_dbus_auth_observer_new:
 *
 * Creates a new #GDBusAuthObserver object.
 *
 * Returns: A #GDBusAuthObserver. Free with g_object_unref().
 *
 * Since: 2.26
 */
GDBusAuthObserver *
g_dbus_auth_observer_new (void)
{
  return g_object_new (G_TYPE_DBUS_AUTH_OBSERVER, NULL);
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_dbus_auth_observer_deny_authenticated_peer:
 * @observer: A #GDBusAuthObserver.
 * @stream: A #GIOStream for the #GDBusConnection.
 * @credentials: Credentials received from the peer or %NULL.
 *
 * Emits the #GDBusAuthObserver::deny-authenticated-peer signal on @observer.
 *
 * Returns: %TRUE if the peer should be denied, %FALSE otherwise.
 *
 * Since: 2.26
 */
gboolean
g_dbus_auth_observer_deny_authenticated_peer (GDBusAuthObserver  *observer,
                                              GIOStream          *stream,
                                              GCredentials       *credentials)
{
  gboolean denied;

  denied = FALSE;
  g_signal_emit (observer,
                 signals[DENY_AUTHENTICATED_PEER_SIGNAL],
                 0,
                 stream,
                 credentials,
                 &denied);
  return denied;
}



#define __G_DBUS_AUTH_OBSERVER_C__
#include "gioaliasdef.c"
