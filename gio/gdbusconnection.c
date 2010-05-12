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

/*
 * TODO for GDBus:
 *
 * - Validate all data (e.g. UTF-8) and check all the required D-Bus headers
 *   are present and forbidden ones aren't
 *   - When writing: g_dbus_message_to_blob()
 *   - When reading: g_dbus_message_new_from_blob()
 *
 * - would be nice to expose GDBusAuthMechanism and an extension point
 *
 * - Need to rewrite GDBusAuth and rework GDBusAuthMechanism. In particular
 *   the mechanism VFuncs need to be able to set an error.
 *
 * - Need to document other mechanisms/sources for determining the D-Bus
 *   address of a well-known bus.
 *
 *   - e.g. on Win32 we need code like from here
 *
 *     http://cgit.freedesktop.org/~david/gdbus-standalone/tree/gdbus/gdbusaddress.c#n900
 *
 *     that was never copied over here because it originally was copy-paste
 *     from the GPLv2 / AFL 2.1 libdbus sources.
 *
 *   - on OS X we need to look in launchd for the address
 *
 *     https://bugs.freedesktop.org/show_bug.cgi?id=14259
 *
 *   - on X11 we need to look in a X11 property on the X server
 *     - (we can also just use dbus-launch(1) from the D-Bus
 *        distribution)
 *
 *   - (ideally) this requires D-Bus spec work because none of
 *     this has never really been specced out properly (excect
 *     the X11 bits)
 *
 * - Related to the above, we also need to be able to launch a message bus
 *   instance.... Since we don't want to write our own bus daemon we should
 *   launch dbus-daemon(1) (thus: Win32 and OS X need to bundle it)
 *
 * - probably want a G_DBUS_NONCE_TCP_TMPDIR environment variable
 *   to specify where the nonce is stored. This will allow people to use
 *   G_DBUS_NONCE_TCP_TMPDIR=/mnt/secure.company.server/dbus-nonce-dir
 *   to easily acheive secure RPC via nonce-tcp.
 *
 * - need to expose an extension point for resolving D-Bus address and
 *   turning them into GIOStream objects. This will allow us to implement
 *   e.g. X11 D-Bus transports without dlopen()'ing or linking against
 *   libX11 from libgio.
 *   - see g_dbus_address_connect() in gdbusaddress.c
 *
 * - would be cute to use kernel-specific APIs to resolve fds for
 *   debug output when using G_DBUS_DEBUG=messages, e.g. in addition to
 *
 *     fd 21: dev=8:1,mode=0100644,ino=1171231,uid=0,gid=0,rdev=0:0,size=234,atime=1273070640,mtime=1267126160,ctime=1267126160
 *
 *   maybe we can show more information about what fd 21 really is.
 *   Ryan suggests looking in /proc/self/fd for clues / symlinks!
 *   Initial experiments on Linux 2.6 suggests that the symlink looks
 *   like this:
 *
 *    3 -> /proc/18068/fd
 *
 *   e.g. not of much use.
 *
 *  - GDBus High-Level docs
 *    - Proxy: properties, signals...
 *    - Connection: IOStream based, ::close, connection setup steps
 *                  mainloop integration, threading
 *    - Differences from libdbus (extend "Migrating from")
 *      - the message handling thread
 *      - Using GVariant instead of GValue
 *    - Explain why the high-level API is a good thing and what
 *      kind of pitfalls it avoids
 *      - Export objects before claiming names
 *    - Talk about auto-starting services (cf. GBusNameWatcherFlags)
 *
 *  - Mention in all API that the GVariant is sunk. Also mention
 *    when the returned GVariant is floating.
 *
 *  - Small example snippet for each method where useful (won't
 *    have to compile) where it makes sense (e.g. connetion_call()
 *    and using a floating GVariant).
 *
 *  - Consistent timeout handling (25s vs 30s?)
 *
 *  - GDBusProxy subclass example
 *
 *  - Update GDBusAuthObserver (is_same_user(), s/deny/authorize/)
 *
 *  - Remove properties on GDBusMethodInvocation
 *
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>

#include "gdbusauth.h"
#include "gdbusutils.h"
#include "gdbusaddress.h"
#include "gdbusmessage.h"
#include "gdbusconnection.h"
#include "gdbuserror.h"
#include "gioenumtypes.h"
#include "gdbusintrospection.h"
#include "gdbusmethodinvocation.h"
#include "gdbusprivate.h"
#include "gdbusauthobserver.h"
#include "gio-marshal.h"
#include "ginitable.h"
#include "gasyncinitable.h"
#include "giostream.h"
#include "gasyncresult.h"
#include "gsimpleasyncresult.h"

#ifdef G_OS_UNIX
#include <gio/gunixconnection.h>
#include <gio/gunixfdmessage.h>
#include <unistd.h>
#include <sys/types.h>
#endif

#include "glibintl.h"
#include "gioalias.h"

/**
 * SECTION:gdbusconnection
 * @short_description: D-Bus Connections
 * @include: gio/gio.h
 *
 * The #GDBusConnection type is used for D-Bus connections to remote
 * peers such as a message buses. It is a low-level API that offers a
 * lot of flexibility. For instance, it lets you establish a connection
 * over any transport that can by represented as an #GIOStream.
 *
 * This class is rarely used directly in D-Bus clients. If you are writing
 * an D-Bus client, it is often easier to use the g_bus_own_name(),
 * g_bus_watch_name() or g_bus_watch_proxy() APIs.
 *
 * <example id="gdbus-server"><title>D-Bus server example</title><programlisting><xi:include xmlns:xi="http://www.w3.org/2001/XInclude" parse="text" href="../../../../gio/tests/gdbus-example-server.c"><xi:fallback>FIXME: MISSING XINCLUDE CONTENT</xi:fallback></xi:include></programlisting></example>
 *
 * <example id="gdbus-subtree-server"><title>D-Bus subtree example</title><programlisting><xi:include xmlns:xi="http://www.w3.org/2001/XInclude" parse="text" href="../../../../gio/tests/gdbus-example-subtree.c"><xi:fallback>FIXME: MISSING XINCLUDE CONTENT</xi:fallback></xi:include></programlisting></example>
 *
 * <example id="gdbus-unix-fd-client"><title>D-Bus UNIX File Descriptor example</title><programlisting><xi:include xmlns:xi="http://www.w3.org/2001/XInclude" parse="text" href="../../../../gio/tests/gdbus-example-unix-fd-client.c"><xi:fallback>FIXME: MISSING XINCLUDE CONTENT</xi:fallback></xi:include></programlisting></example>
 */

/* ---------------------------------------------------------------------------------------------------- */

G_LOCK_DEFINE_STATIC (message_bus_lock);

static GDBusConnection *the_session_bus = NULL;
static GDBusConnection *the_system_bus = NULL;

/* ---------------------------------------------------------------------------------------------------- */

static gboolean
_g_strv_has_string (const gchar* const *haystack,
                    const gchar        *needle)
{
  guint n;

  for (n = 0; haystack != NULL && haystack[n] != NULL; n++)
    {
      if (g_strcmp0 (haystack[n], needle) == 0)
        return TRUE;
    }
  return FALSE;
}

/* ---------------------------------------------------------------------------------------------------- */

#ifdef G_OS_WIN32
#define CONNECTION_ENSURE_LOCK(obj) do { ; } while (FALSE)
#else
// TODO: for some reason this doesn't work on Windows
#define CONNECTION_ENSURE_LOCK(obj) do {                                \
    if (G_UNLIKELY (g_mutex_trylock((obj)->priv->lock)))                \
      {                                                                 \
        g_assertion_message (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
                             "CONNECTION_ENSURE_LOCK: GDBusConnection object lock is not locked"); \
      }                                                                 \
  } while (FALSE)
#endif

#define CONNECTION_LOCK(obj) do {                                       \
    g_mutex_lock ((obj)->priv->lock);                                   \
  } while (FALSE)

#define CONNECTION_UNLOCK(obj) do {                                     \
    g_mutex_unlock ((obj)->priv->lock);                                 \
  } while (FALSE)

struct _GDBusConnectionPrivate
{
  /* ------------------------------------------------------------------------ */
  /* -- General object state ------------------------------------------------ */
  /* ------------------------------------------------------------------------ */

  /* object-wide lock */
  GMutex *lock;

  /* A lock used in the init() method of the GInitable interface - see comments
   * in initable_init() for why a separate lock is needed
   */
  GMutex *init_lock;

  /* Set (by loading the contents of /var/lib/dbus/machine-id) the first time
   * someone calls org.freedesktop.DBus.GetMachineId()
   */
  gchar *machine_id;

  /* The underlying stream used for communication */
  GIOStream *stream;

  /* The object used for authentication (if any) */
  GDBusAuth *auth;

  /* Set to TRUE if the connection has been closed */
  gboolean closed;

  /* Last serial used */
  guint32 last_serial;

  /* The object used to send/receive message */
  GDBusWorker *worker;

  /* If connected to a message bus, this contains the unique name assigned to
   * us by the bus (e.g. ":1.42")
   */
  gchar *bus_unique_name;

  /* The GUID returned by the other side if we authenticed as a client or
   * the GUID to use if authenticating as a server
   */
  gchar *guid;

  /* set to TRUE exactly when initable_init() has finished running */
  gboolean is_initialized;

  /* If the connection could not be established during initable_init(), this GError will set */
  GError *initialization_error;

  /* The result of g_main_context_get_thread_default() when the object
   * was created (the GObject _init() function) - this is used for delivery
   * of the :closed GObject signal.
   */
  GMainContext *main_context_at_construction;

  /* construct properties */
  gchar *address;
  GDBusConnectionFlags flags;

  /* Map used for managing method replies */
  GHashTable *map_method_serial_to_send_message_data;  /* guint32 -> SendMessageData* */

  /* Maps used for managing signal subscription */
  GHashTable *map_rule_to_signal_data;          /* gchar* -> SignalData */
  GHashTable *map_id_to_signal_data;            /* guint  -> SignalData */
  GHashTable *map_sender_to_signal_data_array;  /* gchar* -> GPtrArray* of SignalData */

  /* Maps used for managing exported objects and subtrees */
  GHashTable *map_object_path_to_eo;  /* gchar* -> ExportedObject* */
  GHashTable *map_id_to_ei;           /* guint  -> ExportedInterface* */
  GHashTable *map_object_path_to_es;  /* gchar* -> ExportedSubtree* */
  GHashTable *map_id_to_es;           /* guint  -> ExportedSubtree* */

  /* Structure used for message filters */
  GPtrArray *filters;

  /* Whether to exit on close */
  gboolean exit_on_close;

  /* Capabilities negotiated during authentication */
  GDBusCapabilityFlags capabilities;

  GDBusAuthObserver *authentication_observer;
  GCredentials *crendentials;
};

typedef struct ExportedObject ExportedObject;
static void exported_object_free (ExportedObject *eo);

typedef struct ExportedSubtree ExportedSubtree;
static void exported_subtree_free (ExportedSubtree *es);

enum
{
  CLOSED_SIGNAL,
  LAST_SIGNAL,
};

enum
{
  PROP_0,
  PROP_STREAM,
  PROP_ADDRESS,
  PROP_FLAGS,
  PROP_GUID,
  PROP_UNIQUE_NAME,
  PROP_CLOSED,
  PROP_EXIT_ON_CLOSE,
  PROP_CAPABILITY_FLAGS,
  PROP_AUTHENTICATION_OBSERVER,
};

static void distribute_signals (GDBusConnection  *connection,
                                GDBusMessage     *message);

static void distribute_method_call (GDBusConnection  *connection,
                                    GDBusMessage     *message);

static gboolean handle_generic_unlocked (GDBusConnection *connection,
                                         GDBusMessage    *message);


static void purge_all_signal_subscriptions (GDBusConnection *connection);
static void purge_all_filters (GDBusConnection *connection);

#define _G_ENSURE_LOCK(name) do {                                       \
    if (G_UNLIKELY (G_TRYLOCK(name)))                                   \
      {                                                                 \
        g_assertion_message (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
                             "_G_ENSURE_LOCK: Lock `" #name "' is not locked"); \
      }                                                                 \
  } while (FALSE)                                                       \

static guint signals[LAST_SIGNAL] = { 0 };

static void initable_iface_init       (GInitableIface      *initable_iface);
static void async_initable_iface_init (GAsyncInitableIface *async_initable_iface);

G_DEFINE_TYPE_WITH_CODE (GDBusConnection, g_dbus_connection, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, initable_iface_init)
                         G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_INITABLE, async_initable_iface_init)
                         );

static void
g_dbus_connection_dispose (GObject *object)
{
  GDBusConnection *connection = G_DBUS_CONNECTION (object);

  G_LOCK (message_bus_lock);
  //g_debug ("disposing %p", connection);
  if (connection == the_session_bus)
    {
      the_session_bus = NULL;
    }
  else if (connection == the_system_bus)
    {
      the_system_bus = NULL;
    }
  if (connection->priv->worker != NULL)
    {
      _g_dbus_worker_stop (connection->priv->worker);
      connection->priv->worker = NULL;
    }
  G_UNLOCK (message_bus_lock);

  if (G_OBJECT_CLASS (g_dbus_connection_parent_class)->dispose != NULL)
    G_OBJECT_CLASS (g_dbus_connection_parent_class)->dispose (object);
}

static void
g_dbus_connection_finalize (GObject *object)
{
  GDBusConnection *connection = G_DBUS_CONNECTION (object);

  if (connection->priv->authentication_observer != NULL)
    g_object_unref (connection->priv->authentication_observer);

  if (connection->priv->auth != NULL)
    g_object_unref (connection->priv->auth);

  //g_debug ("finalizing %p", connection);
  if (connection->priv->stream != NULL)
    {
      /* We don't really care if closing the stream succeeds or not */
      g_io_stream_close_async (connection->priv->stream,
                               G_PRIORITY_DEFAULT,
                               NULL,  /* GCancellable */
                               NULL,  /* GAsyncReadyCallback */
                               NULL); /* userdata */
      g_object_unref (connection->priv->stream);
      connection->priv->stream = NULL;
    }

  g_free (connection->priv->address);

  g_free (connection->priv->guid);
  g_free (connection->priv->bus_unique_name);

  if (connection->priv->initialization_error != NULL)
    g_error_free (connection->priv->initialization_error);

  g_hash_table_unref (connection->priv->map_method_serial_to_send_message_data);

  purge_all_signal_subscriptions (connection);
  g_hash_table_unref (connection->priv->map_rule_to_signal_data);
  g_hash_table_unref (connection->priv->map_id_to_signal_data);
  g_hash_table_unref (connection->priv->map_sender_to_signal_data_array);

  g_hash_table_unref (connection->priv->map_id_to_ei);
  g_hash_table_unref (connection->priv->map_object_path_to_eo);
  g_hash_table_unref (connection->priv->map_id_to_es);
  g_hash_table_unref (connection->priv->map_object_path_to_es);

  purge_all_filters (connection);
  g_ptr_array_unref (connection->priv->filters);

  if (connection->priv->main_context_at_construction != NULL)
    g_main_context_unref (connection->priv->main_context_at_construction);

  g_free (connection->priv->machine_id);

  g_mutex_free (connection->priv->init_lock);
  g_mutex_free (connection->priv->lock);

  G_OBJECT_CLASS (g_dbus_connection_parent_class)->finalize (object);
}

static void
g_dbus_connection_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  GDBusConnection *connection = G_DBUS_CONNECTION (object);

  switch (prop_id)
    {
    case PROP_STREAM:
      g_value_set_object (value, g_dbus_connection_get_stream (connection));
      break;

    case PROP_GUID:
      g_value_set_string (value, g_dbus_connection_get_guid (connection));
      break;

    case PROP_UNIQUE_NAME:
      g_value_set_string (value, g_dbus_connection_get_unique_name (connection));
      break;

    case PROP_CLOSED:
      g_value_set_boolean (value, g_dbus_connection_is_closed (connection));
      break;

    case PROP_EXIT_ON_CLOSE:
      g_value_set_boolean (value, g_dbus_connection_get_exit_on_close (connection));
      break;

    case PROP_CAPABILITY_FLAGS:
      g_value_set_flags (value, g_dbus_connection_get_capabilities (connection));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
g_dbus_connection_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GDBusConnection *connection = G_DBUS_CONNECTION (object);

  switch (prop_id)
    {
    case PROP_STREAM:
      connection->priv->stream = g_value_dup_object (value);
      break;

    case PROP_GUID:
      connection->priv->guid = g_value_dup_string (value);
      break;

    case PROP_ADDRESS:
      connection->priv->address = g_value_dup_string (value);
      break;

    case PROP_FLAGS:
      connection->priv->flags = g_value_get_flags (value);
      break;

    case PROP_EXIT_ON_CLOSE:
      g_dbus_connection_set_exit_on_close (connection, g_value_get_boolean (value));
      break;

    case PROP_AUTHENTICATION_OBSERVER:
      connection->priv->authentication_observer = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
g_dbus_connection_real_closed (GDBusConnection *connection,
                               gboolean         remote_peer_vanished,
                               GError          *error)
{
  if (remote_peer_vanished && connection->priv->exit_on_close)
    {
      g_print ("%s: Remote peer vanished. Exiting.\n", G_STRFUNC);
      raise (SIGTERM);
    }
}

static void
g_dbus_connection_class_init (GDBusConnectionClass *klass)
{
  GObjectClass *gobject_class;

  g_type_class_add_private (klass, sizeof (GDBusConnectionPrivate));

  gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize     = g_dbus_connection_finalize;
  gobject_class->dispose      = g_dbus_connection_dispose;
  gobject_class->set_property = g_dbus_connection_set_property;
  gobject_class->get_property = g_dbus_connection_get_property;

  klass->closed = g_dbus_connection_real_closed;

  /**
   * GDBusConnection:stream:
   *
   * The underlying #GIOStream used for I/O.
   *
   * Since: 2.26
   */
  g_object_class_install_property (gobject_class,
                                   PROP_STREAM,
                                   g_param_spec_object ("stream",
                                                        P_("IO Stream"),
                                                        P_("The underlying streams used for I/O"),
                                                        G_TYPE_IO_STREAM,
                                                        G_PARAM_READABLE |
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_BLURB |
                                                        G_PARAM_STATIC_NICK));

  /**
   * GDBusConnection:address:
   *
   * A D-Bus address specifying potential endpoints that can be used
   * when establishing the connection.
   *
   * Since: 2.26
   */
  g_object_class_install_property (gobject_class,
                                   PROP_ADDRESS,
                                   g_param_spec_string ("address",
                                                        P_("Address"),
                                                        P_("D-Bus address specifying potential socket endpoints"),
                                                        NULL,
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_BLURB |
                                                        G_PARAM_STATIC_NICK));

  /**
   * GDBusConnection:flags:
   *
   * Flags from the #GDBusConnectionFlags enumeration.
   *
   * Since: 2.26
   */
  g_object_class_install_property (gobject_class,
                                   PROP_FLAGS,
                                   g_param_spec_flags ("flags",
                                                       P_("Flags"),
                                                       P_("Flags"),
                                                       G_TYPE_DBUS_CONNECTION_FLAGS,
                                                       G_DBUS_CONNECTION_FLAGS_NONE,
                                                       G_PARAM_WRITABLE |
                                                       G_PARAM_CONSTRUCT_ONLY |
                                                       G_PARAM_STATIC_NAME |
                                                       G_PARAM_STATIC_BLURB |
                                                       G_PARAM_STATIC_NICK));

  /**
   * GDBusConnection:guid:
   *
   * The GUID of the peer performing the role of server when
   * authenticating.
   *
   * If you are constructing a #GDBusConnection and pass
   * %G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_SERVER in the
   * #GDBusConnection:flags property then you MUST also set this
   * property to a valid guid.
   *
   * If you are constructing a #GDBusConnection and pass
   * %G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT in the
   * #GDBusConnection:flags property you will be able to read the GUID
   * of the other peer here after the connection has been successfully
   * initialized.
   *
   * Since: 2.26
   */
  g_object_class_install_property (gobject_class,
                                   PROP_GUID,
                                   g_param_spec_string ("guid",
                                                        P_("GUID"),
                                                        P_("GUID of the server peer"),
                                                        NULL,
                                                        G_PARAM_READABLE |
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_BLURB |
                                                        G_PARAM_STATIC_NICK));

  /**
   * GDBusConnection:unique-name:
   *
   * The unique name as assigned by the message bus or %NULL if the
   * connection is not open or not a message bus connection.
   *
   * Since: 2.26
   */
  g_object_class_install_property (gobject_class,
                                   PROP_UNIQUE_NAME,
                                   g_param_spec_string ("unique-name",
                                                        P_("unique-name"),
                                                        P_("Unique name of bus connection"),
                                                        NULL,
                                                        G_PARAM_READABLE |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_BLURB |
                                                        G_PARAM_STATIC_NICK));

  /**
   * GDBusConnection:closed:
   *
   * A boolean specifying whether the connection has been closed.
   *
   * Since: 2.26
   */
  g_object_class_install_property (gobject_class,
                                   PROP_CLOSED,
                                   g_param_spec_boolean ("closed",
                                                         P_("Closed"),
                                                         P_("Whether the connection is closed"),
                                                         FALSE,
                                                         G_PARAM_READABLE |
                                                         G_PARAM_STATIC_NAME |
                                                         G_PARAM_STATIC_BLURB |
                                                         G_PARAM_STATIC_NICK));

  /**
   * GDBusConnection:exit-on-close:
   *
   * A boolean specifying whether the process will be terminated (by
   * calling <literal>raise(SIGTERM)</literal>) if the connection
   * is closed by the remote peer.
   *
   * Since: 2.26
   */
  g_object_class_install_property (gobject_class,
                                   PROP_EXIT_ON_CLOSE,
                                   g_param_spec_boolean ("exit-on-close",
                                                         P_("Exit on close"),
                                                         P_("Whether the process is terminated when the connection is closed"),
                                                         FALSE,
                                                         G_PARAM_READABLE |
                                                         G_PARAM_WRITABLE |
                                                         G_PARAM_STATIC_NAME |
                                                         G_PARAM_STATIC_BLURB |
                                                         G_PARAM_STATIC_NICK));

  /**
   * GDBusConnection:capabilities:
   *
   * Flags from the #GDBusCapabilityFlags enumeration
   * representing connection features negotiated with the other peer.
   *
   * Since: 2.26
   */
  g_object_class_install_property (gobject_class,
                                   PROP_CAPABILITY_FLAGS,
                                   g_param_spec_flags ("capabilities",
                                                       P_("Capabilities"),
                                                       P_("Capabilities"),
                                                       G_TYPE_DBUS_CAPABILITY_FLAGS,
                                                       G_DBUS_CAPABILITY_FLAGS_NONE,
                                                       G_PARAM_READABLE |
                                                       G_PARAM_STATIC_NAME |
                                                       G_PARAM_STATIC_BLURB |
                                                       G_PARAM_STATIC_NICK));

  /**
   * GDBusConnection:authentication-observer:
   *
   * A #GDBusAuthObserver object to assist in the authentication process or %NULL.
   *
   * Since: 2.26
   */
  g_object_class_install_property (gobject_class,
                                   PROP_AUTHENTICATION_OBSERVER,
                                   g_param_spec_object ("authentication-observer",
                                                        P_("Authentication Observer"),
                                                        P_("Object used to assist in the authentication process"),
                                                        G_TYPE_DBUS_AUTH_OBSERVER,
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_BLURB |
                                                        G_PARAM_STATIC_NICK));

  /**
   * GDBusConnection::closed:
   * @connection: The #GDBusConnection emitting the signal.
   * @remote_peer_vanished: %TRUE if @connection is closed because the
   * remote peer closed its end of the connection.
   * @error: A #GError with more details about the event or %NULL.
   *
   * Emitted when the connection is closed.
   *
   * The cause of this event can be
   * <itemizedlist>
   * <listitem><para>
   *    If g_dbus_connection_close() is called. In this case
   *    @remote_peer_vanished is set to %FALSE and @error is %NULL.
   * </para></listitem>
   * <listitem><para>
   *    If the remote peer closes the connection. In this case
   *    @remote_peer_vanished is set to %TRUE and @error is set.
   * </para></listitem>
   * <listitem><para>
   *    If the remote peer sends invalid or malformed data. In this
   *    case @remote_peer_vanished is set to %FALSE and @error
   *    is set.
   * </para></listitem>
   * </itemizedlist>
   *
   * Upon receiving this signal, you should give up your reference to
   * @connection. You are guaranteed that this signal is emitted only
   * once.
   *
   * Since: 2.26
   */
  signals[CLOSED_SIGNAL] = g_signal_new ("closed",
                                         G_TYPE_DBUS_CONNECTION,
                                         G_SIGNAL_RUN_LAST,
                                         G_STRUCT_OFFSET (GDBusConnectionClass, closed),
                                         NULL,
                                         NULL,
                                         _gio_marshal_VOID__BOOLEAN_BOXED,
                                         G_TYPE_NONE,
                                         2,
                                         G_TYPE_BOOLEAN,
                                         G_TYPE_ERROR);
}

static void
g_dbus_connection_init (GDBusConnection *connection)
{
  connection->priv = G_TYPE_INSTANCE_GET_PRIVATE (connection, G_TYPE_DBUS_CONNECTION, GDBusConnectionPrivate);

  connection->priv->lock = g_mutex_new ();
  connection->priv->init_lock = g_mutex_new ();

  connection->priv->map_method_serial_to_send_message_data = g_hash_table_new (g_direct_hash, g_direct_equal);

  connection->priv->map_rule_to_signal_data = g_hash_table_new (g_str_hash,
                                                                g_str_equal);
  connection->priv->map_id_to_signal_data = g_hash_table_new (g_direct_hash,
                                                              g_direct_equal);
  connection->priv->map_sender_to_signal_data_array = g_hash_table_new_full (g_str_hash,
                                                                             g_str_equal,
                                                                             g_free,
                                                                             NULL);

  connection->priv->map_object_path_to_eo = g_hash_table_new_full (g_str_hash,
                                                                   g_str_equal,
                                                                   NULL,
                                                                   (GDestroyNotify) exported_object_free);

  connection->priv->map_id_to_ei = g_hash_table_new (g_direct_hash,
                                                     g_direct_equal);

  connection->priv->map_object_path_to_es = g_hash_table_new_full (g_str_hash,
                                                                   g_str_equal,
                                                                   NULL,
                                                                   (GDestroyNotify) exported_subtree_free);

  connection->priv->map_id_to_es = g_hash_table_new (g_direct_hash,
                                                     g_direct_equal);

  connection->priv->main_context_at_construction = g_main_context_get_thread_default ();
  if (connection->priv->main_context_at_construction != NULL)
    g_main_context_ref (connection->priv->main_context_at_construction);

  connection->priv->filters = g_ptr_array_new ();
}

/**
 * g_dbus_connection_get_stream:
 * @connection: a #GDBusConnection
 *
 * Gets the underlying stream used for IO.
 *
 * Returns: the stream used for IO
 *
 * Since: 2.26
 */
GIOStream *
g_dbus_connection_get_stream (GDBusConnection *connection)
{
  g_return_val_if_fail (G_IS_DBUS_CONNECTION (connection), NULL);
  return connection->priv->stream;
}


/**
 * g_dbus_connection_is_closed:
 * @connection: A #GDBusConnection.
 *
 * Gets whether @connection is closed.
 *
 * Returns: %TRUE if the connection is closed, %FALSE otherwise.
 *
 * Since: 2.26
 */
gboolean
g_dbus_connection_is_closed (GDBusConnection *connection)
{
  g_return_val_if_fail (G_IS_DBUS_CONNECTION (connection), FALSE);
  return connection->priv->closed;
}

/**
 * g_dbus_connection_get_capabilities:
 * @connection: A #GDBusConnection.
 *
 * Gets the capabilities negotiated with the remote peer
 *
 * Returns: Zero or more flags from the #GDBusCapabilityFlags enumeration.
 *
 * Since: 2.26
 */
GDBusCapabilityFlags
g_dbus_connection_get_capabilities (GDBusConnection *connection)
{
  g_return_val_if_fail (G_IS_DBUS_CONNECTION (connection), G_DBUS_CAPABILITY_FLAGS_NONE);
  return connection->priv->capabilities;
}


/* ---------------------------------------------------------------------------------------------------- */

typedef struct
{
  GDBusConnection *connection;
  GError *error;
  gboolean remote_peer_vanished;
} EmitClosedData;

static void
emit_closed_data_free (EmitClosedData *data)
{
  g_object_unref (data->connection);
  if (data->error != NULL)
    g_error_free (data->error);
  g_free (data);
}

static gboolean
emit_closed_in_idle (gpointer user_data)
{
  EmitClosedData *data = user_data;
  gboolean result;

  g_object_notify (G_OBJECT (data->connection), "closed");
  g_signal_emit (data->connection,
                 signals[CLOSED_SIGNAL],
                 0,
                 data->remote_peer_vanished,
                 data->error,
                 &result);
  return FALSE;
}

/* Can be called from any thread, must hold lock */
static void
set_closed_unlocked (GDBusConnection *connection,
                     gboolean         remote_peer_vanished,
                     GError          *error)
{
  GSource *idle_source;
  EmitClosedData *data;

  CONNECTION_ENSURE_LOCK (connection);

  g_assert (!connection->priv->closed);

  connection->priv->closed = TRUE;

  data = g_new0 (EmitClosedData, 1);
  data->connection = g_object_ref (connection);
  data->remote_peer_vanished = remote_peer_vanished;
  data->error = error != NULL ? g_error_copy (error) : NULL;

  idle_source = g_idle_source_new ();
  g_source_set_priority (idle_source, G_PRIORITY_DEFAULT);
  g_source_set_callback (idle_source,
                         emit_closed_in_idle,
                         data,
                         (GDestroyNotify) emit_closed_data_free);
  g_source_attach (idle_source, connection->priv->main_context_at_construction);
  g_source_unref (idle_source);
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_dbus_connection_close:
 * @connection: A #GDBusConnection.
 *
 * Closes @connection. Note that this never causes the process to
 * exit (this might only happen if the other end of a shared message
 * bus connection disconnects).
 *
 * If @connection is already closed, this method does nothing.
 *
 * Since: 2.26
 */
void
g_dbus_connection_close (GDBusConnection *connection)
{
  g_return_if_fail (G_IS_DBUS_CONNECTION (connection));

  CONNECTION_LOCK (connection);
  if (!connection->priv->closed)
    {
      GError *error = NULL;

      /* TODO: do this async */
      //g_debug ("closing connection %p's stream %p", connection, connection->priv->stream);
      if (!g_io_stream_close (connection->priv->stream, NULL, &error))
        {
          g_warning ("Error closing stream: %s", error->message);
          g_error_free (error);
        }

      set_closed_unlocked (connection, FALSE, NULL);
    }
  CONNECTION_UNLOCK (connection);
}

/* ---------------------------------------------------------------------------------------------------- */

static gboolean
g_dbus_connection_send_message_unlocked (GDBusConnection   *connection,
                                         GDBusMessage      *message,
                                         volatile guint32  *out_serial,
                                         GError           **error)
{
  guchar *blob;
  gsize blob_size;
  guint32 serial_to_use;
  gboolean ret;

  CONNECTION_ENSURE_LOCK (connection);

  g_return_val_if_fail (G_IS_DBUS_CONNECTION (connection), FALSE);
  g_return_val_if_fail (G_IS_DBUS_MESSAGE (message), FALSE);

  /* TODO: check all necessary headers are present */

  ret = FALSE;
  blob = NULL;

  if (out_serial != NULL)
    *out_serial = 0;

  if (connection->priv->closed)
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_CLOSED,
                           _("The connection is closed"));
      goto out;
    }

  blob = g_dbus_message_to_blob (message,
                                 &blob_size,
                                 error);
  if (blob == NULL)
    goto out;

  serial_to_use = ++connection->priv->last_serial; /* TODO: handle overflow */

  switch (blob[0])
    {
    case 'l':
      ((guint32 *) blob)[2] = GUINT32_TO_LE (serial_to_use);
      break;
    case 'B':
      ((guint32 *) blob)[2] = GUINT32_TO_BE (serial_to_use);
      break;
    default:
      g_assert_not_reached ();
      break;
    }

#if 0
  g_printerr ("Writing message of %" G_GSIZE_FORMAT " bytes (serial %d) on %p:\n",
              blob_size, serial_to_use, connection);
  g_printerr ("----\n");
  hexdump (blob, blob_size);
  g_printerr ("----\n");
#endif

  /* TODO: use connection->priv->auth to encode the blob */

  if (out_serial != NULL)
    *out_serial = serial_to_use;

  g_dbus_message_set_serial (message, serial_to_use);

  _g_dbus_worker_send_message (connection->priv->worker,
                               message,
                               (gchar*) blob,
                               blob_size);
  blob = NULL; /* since _g_dbus_worker_send_message() steals the blob */

  ret = TRUE;

 out:
  g_free (blob);

  return ret;
}

/**
 * g_dbus_connection_send_message:
 * @connection: A #GDBusConnection.
 * @message: A #GDBusMessage
 * @out_serial: Return location for serial number assigned to @message when sending it or %NULL.
 * @error: Return location for error or %NULL.
 *
 * Asynchronously sends @message to the peer represented by @connection.
 *
 * If @out_serial is not %NULL, then the serial number assigned to
 * @message by @connection will be written to this location prior to
 * submitting the message to the underlying transport.
 *
 * If @connection is closed then the operation will fail with
 * %G_IO_ERROR_CLOSED. If @cancellable is canceled, the operation will
 * fail with %G_IO_ERROR_CANCELLED. If @message is not well-formed,
 * the operation fails with %G_IO_ERROR_INVALID_ARGUMENT.
 *
 * See <xref linkend="gdbus-server"/> and <xref
 * linkend="gdbus-unix-fd-client"/> for an example of how to use this
 * low-level API to send and receive UNIX file descriptors.
 *
 * Returns: %TRUE if the message was well-formed and queued for
 * transmission, %FALSE if @error is set.
 *
 * Since: 2.26
 */
gboolean
g_dbus_connection_send_message (GDBusConnection   *connection,
                                GDBusMessage      *message,
                                volatile guint32  *out_serial,
                                GError           **error)
{
  gboolean ret;

  g_return_val_if_fail (G_IS_DBUS_CONNECTION (connection), FALSE);
  g_return_val_if_fail (G_IS_DBUS_MESSAGE (message), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  CONNECTION_LOCK (connection);
  ret = g_dbus_connection_send_message_unlocked (connection, message, out_serial, error);
  CONNECTION_UNLOCK (connection);
  return ret;
}

/* ---------------------------------------------------------------------------------------------------- */

typedef struct
{
  volatile gint ref_count;
  GDBusConnection *connection;
  guint32 serial;
  GSimpleAsyncResult *simple;

  GMainContext *main_context;

  GCancellable *cancellable;

  gulong cancellable_handler_id;

  GSource *timeout_source;

  gboolean delivered;
} SendMessageData;

static SendMessageData *
send_message_data_ref (SendMessageData *data)
{
  g_atomic_int_inc (&data->ref_count);
  return data;
}

static void
send_message_data_unref (SendMessageData *data)
{
  if (g_atomic_int_dec_and_test (&data->ref_count))
    {
      g_assert (data->timeout_source == NULL);
      g_assert (data->simple == NULL);
      g_assert (data->cancellable_handler_id == 0);
      g_object_unref (data->connection);
      if (data->cancellable != NULL)
        g_object_unref (data->cancellable);
      if (data->main_context != NULL)
        g_main_context_unref (data->main_context);
      g_free (data);
    }
}

/* ---------------------------------------------------------------------------------------------------- */

/* can be called from any thread with lock held - caller must have prepared GSimpleAsyncResult already */
static void
send_message_with_reply_deliver (SendMessageData *data)
{
  CONNECTION_ENSURE_LOCK (data->connection);

  g_assert (!data->delivered);

  data->delivered = TRUE;

  g_simple_async_result_complete_in_idle (data->simple);
  g_object_unref (data->simple);
  data->simple = NULL;

  if (data->timeout_source != NULL)
    {
      g_source_destroy (data->timeout_source);
      data->timeout_source = NULL;
    }
  if (data->cancellable_handler_id > 0)
    {
      g_cancellable_disconnect (data->cancellable, data->cancellable_handler_id);
      data->cancellable_handler_id = 0;
    }

  g_warn_if_fail (g_hash_table_remove (data->connection->priv->map_method_serial_to_send_message_data,
                                       GUINT_TO_POINTER (data->serial)));

  send_message_data_unref (data);
}

/* ---------------------------------------------------------------------------------------------------- */

/* must hold lock */
static void
send_message_data_deliver_reply_unlocked (SendMessageData *data,
                                          GDBusMessage    *reply)
{
  if (data->delivered)
    goto out;

  g_simple_async_result_set_op_res_gpointer (data->simple,
                                             g_object_ref (reply),
                                             g_object_unref);

  send_message_with_reply_deliver (data);

 out:
  ;
}

/* ---------------------------------------------------------------------------------------------------- */

static gboolean
send_message_with_reply_cancelled_idle_cb (gpointer user_data)
{
  SendMessageData *data = user_data;

  CONNECTION_LOCK (data->connection);
  if (data->delivered)
    goto out;

  g_simple_async_result_set_error (data->simple,
                                   G_IO_ERROR,
                                   G_IO_ERROR_CANCELLED,
                                   _("Operation was cancelled"));

  send_message_with_reply_deliver (data);

 out:
  CONNECTION_UNLOCK (data->connection);
  return FALSE;
}

/* Can be called from any thread with or without lock held */
static void
send_message_with_reply_cancelled_cb (GCancellable *cancellable,
                                      gpointer      user_data)
{
  SendMessageData *data = user_data;
  GSource *idle_source;

  /* postpone cancellation to idle handler since we may be called directly
   * via g_cancellable_connect() (e.g. holding lock)
   */
  idle_source = g_idle_source_new ();
  g_source_set_priority (idle_source, G_PRIORITY_DEFAULT);
  g_source_set_callback (idle_source,
                         send_message_with_reply_cancelled_idle_cb,
                         send_message_data_ref (data),
                         (GDestroyNotify) send_message_data_unref);
  g_source_attach (idle_source, data->main_context);
  g_source_unref (idle_source);
}

/* ---------------------------------------------------------------------------------------------------- */

static gboolean
send_message_with_reply_timeout_cb (gpointer user_data)
{
  SendMessageData *data = user_data;

  CONNECTION_LOCK (data->connection);
  if (data->delivered)
    goto out;

  g_simple_async_result_set_error (data->simple,
                                   G_IO_ERROR,
                                   G_IO_ERROR_TIMED_OUT,
                                   _("Timeout was reached"));

  send_message_with_reply_deliver (data);

 out:
  CONNECTION_UNLOCK (data->connection);

  return FALSE;
}

/* ---------------------------------------------------------------------------------------------------- */

static void
g_dbus_connection_send_message_with_reply_unlocked (GDBusConnection     *connection,
                                                    GDBusMessage        *message,
                                                    gint                 timeout_msec,
                                                    volatile guint32    *out_serial,
                                                    GCancellable        *cancellable,
                                                    GAsyncReadyCallback  callback,
                                                    gpointer             user_data)
{
  GSimpleAsyncResult *simple;
  SendMessageData *data;
  GError *error;
  volatile guint32 serial;

  data = NULL;

  if (out_serial == NULL)
    out_serial = &serial;

  if (timeout_msec == -1)
    timeout_msec = 30 * 1000; /* TODO: check 30 secs is the default timeout */

  simple = g_simple_async_result_new (G_OBJECT (connection),
                                      callback,
                                      user_data,
                                      g_dbus_connection_send_message_with_reply);

  if (g_cancellable_is_cancelled (cancellable))
    {
      g_simple_async_result_set_error (simple,
                                       G_IO_ERROR,
                                       G_IO_ERROR_CANCELLED,
                                       _("Operation was cancelled"));
      g_simple_async_result_complete_in_idle (simple);
      g_object_unref (simple);
      goto out;
    }

  if (connection->priv->closed)
    {
      g_simple_async_result_set_error (simple,
                                       G_IO_ERROR,
                                       G_IO_ERROR_CLOSED,
                                       _("The connection is closed"));
      g_simple_async_result_complete_in_idle (simple);
      g_object_unref (simple);
      goto out;
    }

  error = NULL;
  if (!g_dbus_connection_send_message_unlocked (connection, message, out_serial, &error))
    {
      g_simple_async_result_set_from_error (simple, error);
      g_simple_async_result_complete_in_idle (simple);
      g_object_unref (simple);
      goto out;
    }

  data = g_new0 (SendMessageData, 1);
  data->ref_count = 1;
  data->connection = g_object_ref (connection);
  data->simple = simple;
  data->serial = *out_serial;
  data->main_context = g_main_context_get_thread_default ();
  if (data->main_context != NULL)
    g_main_context_ref (data->main_context);

  if (cancellable != NULL)
    {
      data->cancellable = g_object_ref (cancellable);
      data->cancellable_handler_id = g_cancellable_connect (cancellable,
                                                            G_CALLBACK (send_message_with_reply_cancelled_cb),
                                                            send_message_data_ref (data),
                                                            (GDestroyNotify) send_message_data_unref);
      g_object_set_data_full (G_OBJECT (simple),
                              "cancellable",
                              g_object_ref (cancellable),
                              (GDestroyNotify) g_object_unref);
    }

  data->timeout_source = g_timeout_source_new (timeout_msec);
  g_source_set_priority (data->timeout_source, G_PRIORITY_DEFAULT);
  g_source_set_callback (data->timeout_source,
                         send_message_with_reply_timeout_cb,
                         send_message_data_ref (data),
                         (GDestroyNotify) send_message_data_unref);
  g_source_attach (data->timeout_source, data->main_context);
  g_source_unref (data->timeout_source);

  g_hash_table_insert (connection->priv->map_method_serial_to_send_message_data,
                       GUINT_TO_POINTER (*out_serial),
                       data);

 out:
  ;
}

/**
 * g_dbus_connection_send_message_with_reply:
 * @connection: A #GDBusConnection.
 * @message: A #GDBusMessage.
 * @timeout_msec: The timeout in milliseconds or -1 to use the default timeout.
 * @out_serial: Return location for serial number assigned to @message when sending it or %NULL.
 * @cancellable: A #GCancellable or %NULL.
 * @callback: A #GAsyncReadyCallback to call when the request is satisfied or %NULL if you don't
 * care about the result.
 * @user_data: The data to pass to @callback.
 *
 * Asynchronously sends @message to the peer represented by @connection.
 *
 * If @out_serial is not %NULL, then the serial number assigned to
 * @message by @connection will be written to this location prior to
 * submitting the message to the underlying transport.
 *
 * If @connection is closed then the operation will fail with
 * %G_IO_ERROR_CLOSED. If @cancellable is canceled, the operation will
 * fail with %G_IO_ERROR_CANCELLED. If @message is not well-formed,
 * the operation fails with %G_IO_ERROR_INVALID_ARGUMENT.
 *
 * This is an asynchronous method. When the operation is finished, @callback will be invoked
 * in the <link linkend="g-main-context-push-thread-default">thread-default main loop</link>
 * of the thread you are calling this method from. You can then call
 * g_dbus_connection_send_message_with_reply_finish() to get the result of the operation.
 * See g_dbus_connection_send_message_with_reply_sync() for the synchronous version.
 *
 * See <xref linkend="gdbus-server"/> and <xref
 * linkend="gdbus-unix-fd-client"/> for an example of how to use this
 * low-level API to send and receive UNIX file descriptors.
 *
 * Since: 2.26
 */
void
g_dbus_connection_send_message_with_reply (GDBusConnection     *connection,
                                           GDBusMessage        *message,
                                           gint                 timeout_msec,
                                           volatile guint32    *out_serial,
                                           GCancellable        *cancellable,
                                           GAsyncReadyCallback  callback,
                                           gpointer             user_data)
{
  g_return_if_fail (G_IS_DBUS_CONNECTION (connection));
  g_return_if_fail (G_IS_DBUS_MESSAGE (message));
  g_return_if_fail (timeout_msec >= 0 || timeout_msec == -1);

  CONNECTION_LOCK (connection);
  g_dbus_connection_send_message_with_reply_unlocked (connection,
                                                      message,
                                                      timeout_msec,
                                                      out_serial,
                                                      cancellable,
                                                      callback,
                                                      user_data);
  CONNECTION_UNLOCK (connection);
}

/**
 * g_dbus_connection_send_message_with_reply_finish:
 * @connection: a #GDBusConnection
 * @res: A #GAsyncResult obtained from the #GAsyncReadyCallback passed to g_dbus_connection_send_message_with_reply().
 * @error: Return location for error or %NULL.
 *
 * Finishes an operation started with g_dbus_connection_send_message_with_reply().
 *
 * Note that @error is only set if a local in-process error
 * occured. That is to say that the returned #GDBusMessage object may
 * be of type %G_DBUS_MESSAGE_TYPE_ERROR. Use
 * g_dbus_message_to_gerror() to transcode this to a #GError.
 *
 * See <xref linkend="gdbus-server"/> and <xref
 * linkend="gdbus-unix-fd-client"/> for an example of how to use this
 * low-level API to send and receive UNIX file descriptors.
 *
 * Returns: A #GDBusMessage or %NULL if @error is set.
 *
 * Since: 2.26
 */
GDBusMessage *
g_dbus_connection_send_message_with_reply_finish (GDBusConnection  *connection,
                                                  GAsyncResult     *res,
                                                  GError          **error)
{
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (res);
  GDBusMessage *reply;
  GCancellable *cancellable;

  g_return_val_if_fail (G_IS_DBUS_CONNECTION (connection), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  reply = NULL;

  g_warn_if_fail (g_simple_async_result_get_source_tag (simple) == g_dbus_connection_send_message_with_reply);

  if (g_simple_async_result_propagate_error (simple, error))
    goto out;

  reply = g_object_ref (g_simple_async_result_get_op_res_gpointer (simple));
  cancellable = g_object_get_data (G_OBJECT (simple), "cancellable");
  if (cancellable != NULL && g_cancellable_is_cancelled (cancellable))
    {
      g_object_unref (reply);
      reply = NULL;
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_CANCELLED,
                           _("Operation was cancelled"));
    }
 out:
  return reply;
}

/* ---------------------------------------------------------------------------------------------------- */

typedef struct
{
  GAsyncResult *res;
  GMainContext *context;
  GMainLoop *loop;
} SendMessageSyncData;

static void
send_message_with_reply_sync_cb (GDBusConnection *connection,
                                 GAsyncResult    *res,
                                 gpointer         user_data)
{
  SendMessageSyncData *data = user_data;
  data->res = g_object_ref (res);
  g_main_loop_quit (data->loop);
}

/**
 * g_dbus_connection_send_message_with_reply_sync:
 * @connection: A #GDBusConnection.
 * @message: A #GDBusMessage.
 * @timeout_msec: The timeout in milliseconds or -1 to use the default timeout.
 * @out_serial: Return location for serial number assigned to @message when sending it or %NULL.
 * @cancellable: A #GCancellable or %NULL.
 * @error: Return location for error or %NULL.
 *
 * Synchronously sends @message to the peer represented by @connection
 * and blocks the calling thread until a reply is received or the
 * timeout is reached. See g_dbus_connection_send_message_with_reply()
 * for the asynchronous version of this method.
 *
 * If @out_serial is not %NULL, then the serial number assigned to
 * @message by @connection will be written to this location prior to
 * submitting the message to the underlying transport.
 *
 * If @connection is closed then the operation will fail with
 * %G_IO_ERROR_CLOSED. If @cancellable is canceled, the operation will
 * fail with %G_IO_ERROR_CANCELLED. If @message is not well-formed,
 * the operation fails with %G_IO_ERROR_INVALID_ARGUMENT.
 *
 * Note that @error is only set if a local in-process error
 * occured. That is to say that the returned #GDBusMessage object may
 * be of type %G_DBUS_MESSAGE_TYPE_ERROR. Use
 * g_dbus_message_to_gerror() to transcode this to a #GError.
 *
 * See <xref linkend="gdbus-server"/> and <xref
 * linkend="gdbus-unix-fd-client"/> for an example of how to use this
 * low-level API to send and receive UNIX file descriptors.
 *
 * Returns: A #GDBusMessage that is the reply to @message or %NULL if @error is set.
 *
 * Since: 2.26
 */
GDBusMessage *
g_dbus_connection_send_message_with_reply_sync (GDBusConnection   *connection,
                                                GDBusMessage      *message,
                                                gint               timeout_msec,
                                                volatile guint32  *out_serial,
                                                GCancellable      *cancellable,
                                                GError           **error)
{
  SendMessageSyncData *data;
  GDBusMessage *reply;

  g_return_val_if_fail (G_IS_DBUS_CONNECTION (connection), NULL);
  g_return_val_if_fail (G_IS_DBUS_MESSAGE (message), NULL);
  g_return_val_if_fail (timeout_msec >= 0 || timeout_msec == -1, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  data = g_new0 (SendMessageSyncData, 1);
  data->context = g_main_context_new ();
  data->loop = g_main_loop_new (data->context, FALSE);

  g_main_context_push_thread_default (data->context);

  g_dbus_connection_send_message_with_reply (connection,
                                             message,
                                             timeout_msec,
                                             out_serial,
                                             cancellable,
                                             (GAsyncReadyCallback) send_message_with_reply_sync_cb,
                                             data);
  g_main_loop_run (data->loop);
  reply = g_dbus_connection_send_message_with_reply_finish (connection,
                                                            data->res,
                                                            error);

  g_main_context_pop_thread_default (data->context);

  g_main_context_unref (data->context);
  g_main_loop_unref (data->loop);
  g_object_unref (data->res);
  g_free (data);

  return reply;
}

/* ---------------------------------------------------------------------------------------------------- */

typedef struct
{
  GDBusMessageFilterFunction func;
  gpointer user_data;
} FilterCallback;

typedef struct
{
  guint                       id;
  GDBusMessageFilterFunction  filter_function;
  gpointer                    user_data;
  GDestroyNotify              user_data_free_func;
} FilterData;

/* Called in worker's thread - we must not block */
static void
on_worker_message_received (GDBusWorker  *worker,
                            GDBusMessage *message,
                            gpointer      user_data)
{
  GDBusConnection *connection = G_DBUS_CONNECTION (user_data);
  FilterCallback *filters;
  gboolean consumed_by_filter;
  guint num_filters;
  guint n;

  //g_debug ("in on_worker_message_received");

  g_object_ref (connection);

  /* First collect the set of callback functions */
  CONNECTION_LOCK (connection);
  num_filters = connection->priv->filters->len;
  filters = g_new0 (FilterCallback, num_filters);
  for (n = 0; n < num_filters; n++)
    {
      FilterData *data = connection->priv->filters->pdata[n];
      filters[n].func = data->filter_function;
      filters[n].user_data = data->user_data;
    }
  CONNECTION_UNLOCK (connection);

  /* the call the filters in order (without holding the lock) */
  consumed_by_filter = FALSE;
  for (n = 0; n < num_filters; n++)
    {
      consumed_by_filter = filters[n].func (connection,
                                            message,
                                            filters[n].user_data);
      if (consumed_by_filter)
        break;
    }

  /* Standard dispatch unless the filter ate the message */
  if (!consumed_by_filter)
    {
      GDBusMessageType message_type;

      message_type = g_dbus_message_get_message_type (message);
      if (message_type == G_DBUS_MESSAGE_TYPE_METHOD_RETURN || message_type == G_DBUS_MESSAGE_TYPE_ERROR)
        {
          guint32 reply_serial;
          SendMessageData *send_message_data;

          reply_serial = g_dbus_message_get_reply_serial (message);
          CONNECTION_LOCK (connection);
          send_message_data = g_hash_table_lookup (connection->priv->map_method_serial_to_send_message_data,
                                                   GUINT_TO_POINTER (reply_serial));
          if (send_message_data != NULL)
            {
              //g_debug ("delivering reply/error for serial %d for %p", reply_serial, connection);
              send_message_data_deliver_reply_unlocked (send_message_data, message);
            }
          else
            {
              //g_debug ("message reply/error for serial %d but no SendMessageData found for %p", reply_serial, connection);
            }
          CONNECTION_UNLOCK (connection);
        }
      else if (message_type == G_DBUS_MESSAGE_TYPE_SIGNAL)
        {
          CONNECTION_LOCK (connection);
          distribute_signals (connection, message);
          CONNECTION_UNLOCK (connection);
        }
      else if (message_type == G_DBUS_MESSAGE_TYPE_METHOD_CALL)
        {
          CONNECTION_LOCK (connection);
          distribute_method_call (connection, message);
          CONNECTION_UNLOCK (connection);
        }
    }

  g_object_unref (connection);
  g_free (filters);
}

/* Called in worker's thread - we must not block */
static void
on_worker_closed (GDBusWorker *worker,
                  gboolean     remote_peer_vanished,
                  GError      *error,
                  gpointer     user_data)
{
  GDBusConnection *connection = G_DBUS_CONNECTION (user_data);

  //g_debug ("in on_worker_closed: %s", error->message);

  CONNECTION_LOCK (connection);
  if (!connection->priv->closed)
    set_closed_unlocked (connection, remote_peer_vanished, error);
  CONNECTION_UNLOCK (connection);
}

/* ---------------------------------------------------------------------------------------------------- */

/* Determines the biggest set of capabilities we can support on this connection */
static GDBusCapabilityFlags
get_offered_capabilities_max (GDBusConnection *connection)
{
      GDBusCapabilityFlags ret;
      ret = G_DBUS_CAPABILITY_FLAGS_NONE;
#ifdef G_OS_UNIX
      if (G_IS_UNIX_CONNECTION (connection->priv->stream))
        ret |= G_DBUS_CAPABILITY_FLAGS_UNIX_FD_PASSING;
#endif
      return ret;
}

static gboolean
initable_init (GInitable     *initable,
               GCancellable  *cancellable,
               GError       **error)
{
  GDBusConnection *connection = G_DBUS_CONNECTION (initable);
  gboolean ret;

  /* This method needs to be idempotent to work with the singleton
   * pattern. See the docs for g_initable_init(). We implement this by
   * locking.
   *
   * Unfortunately we can't use the main lock since the on_worker_*()
   * callbacks above needs the lock during initialization (for message
   * bus connections we do a synchronous Hello() call on the bus).
   */
  g_mutex_lock (connection->priv->init_lock);

  ret = FALSE;

  if (connection->priv->is_initialized)
    {
      if (connection->priv->stream != NULL)
        ret = TRUE;
      else
        g_assert (connection->priv->initialization_error != NULL);
      goto out;
    }
  g_assert (connection->priv->initialization_error == NULL);

  /* The user can pass multiple (but mutally exclusive) construct
   * properties:
   *
   *  - stream (of type GIOStream)
   *  - address (of type gchar*)
   *
   * At the end of the day we end up with a non-NULL GIOStream
   * object in connection->priv->stream.
   */
  if (connection->priv->address != NULL)
    {
      g_assert (connection->priv->stream == NULL);

      if ((connection->priv->flags & G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_SERVER) ||
          (connection->priv->flags & G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_ALLOW_ANONYMOUS))
        {
          g_set_error_literal (error,
                               G_IO_ERROR,
                               G_IO_ERROR_INVALID_ARGUMENT,
                               _("Unsupported flags encountered when constructing a client-side connection"));
          goto out;
        }

      connection->priv->stream = g_dbus_address_get_stream_sync (connection->priv->address,
                                                                 NULL, /* TODO: out_guid */
                                                                 cancellable,
                                                                 &connection->priv->initialization_error);
      if (connection->priv->stream == NULL)
        goto out;
    }
  else if (connection->priv->stream != NULL)
    {
      /* nothing to do */
    }
  else
    {
      g_assert_not_reached ();
    }

  /* Authenticate the connection */
  if (connection->priv->flags & G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_SERVER)
    {
      g_assert (!(connection->priv->flags & G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT));
      g_assert (connection->priv->guid != NULL);
      connection->priv->auth = _g_dbus_auth_new (connection->priv->stream);
      if (!_g_dbus_auth_run_server (connection->priv->auth,
                                    connection->priv->authentication_observer,
                                    connection->priv->guid,
                                    (connection->priv->flags & G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_ALLOW_ANONYMOUS),
                                    get_offered_capabilities_max (connection),
                                    &connection->priv->capabilities,
                                    &connection->priv->crendentials,
                                    cancellable,
                                    &connection->priv->initialization_error))
        goto out;
    }
  else if (connection->priv->flags & G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT)
    {
      g_assert (!(connection->priv->flags & G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_SERVER));
      g_assert (connection->priv->guid == NULL);
      connection->priv->auth = _g_dbus_auth_new (connection->priv->stream);
      connection->priv->guid = _g_dbus_auth_run_client (connection->priv->auth,
                                                        get_offered_capabilities_max (connection),
                                                        &connection->priv->capabilities,
                                                        cancellable,
                                                        &connection->priv->initialization_error);
      if (connection->priv->guid == NULL)
        goto out;
    }

  if (connection->priv->authentication_observer != NULL)
    {
      g_object_unref (connection->priv->authentication_observer);
      connection->priv->authentication_observer = NULL;
    }

  //g_output_stream_flush (G_SOCKET_CONNECTION (connection->priv->stream)

  //g_debug ("haz unix fd passing powers: %d", connection->priv->capabilities & G_DBUS_CAPABILITY_FLAGS_UNIX_FD_PASSING);

  /* Hack used until
   *
   *  https://bugzilla.gnome.org/show_bug.cgi?id=616458
   *
   * has been resolved
   */
  if (G_IS_SOCKET_CONNECTION (connection->priv->stream))
    {
      g_socket_set_blocking (g_socket_connection_get_socket (G_SOCKET_CONNECTION (connection->priv->stream)), FALSE);
    }

  connection->priv->worker = _g_dbus_worker_new (connection->priv->stream,
                                                 connection->priv->capabilities,
                                                 on_worker_message_received,
                                                 on_worker_closed,
                                                 connection);

  /* if a bus connection, invoke org.freedesktop.DBus.Hello - this is how we're getting a name */
  if (connection->priv->flags & G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION)
    {
      GVariant *hello_result;
      const gchar *s;

      hello_result = g_dbus_connection_call_sync (connection,
                                                  "org.freedesktop.DBus", /* name */
                                                  "/org/freedesktop/DBus", /* path */
                                                  "org.freedesktop.DBus", /* interface */
                                                  "Hello",
                                                  NULL, /* parameters */
                                                  G_DBUS_CALL_FLAGS_NONE,
                                                  -1,
                                                  NULL, /* TODO: cancellable */
                                                  &connection->priv->initialization_error);
      if (hello_result == NULL)
        goto out;

      g_variant_get (hello_result, "(s)", &s);
      connection->priv->bus_unique_name = g_strdup (s);
      g_variant_unref (hello_result);
      //g_debug ("unique name is `%s'", connection->priv->bus_unique_name);
    }

  connection->priv->is_initialized = TRUE;

  ret = TRUE;
 out:
  if (!ret)
    {
      g_assert (connection->priv->initialization_error != NULL);
      g_propagate_error (error, g_error_copy (connection->priv->initialization_error));
    }

  g_mutex_unlock (connection->priv->init_lock);

  return ret;
}

static void
initable_iface_init (GInitableIface *initable_iface)
{
  initable_iface->init = initable_init;
}

/* ---------------------------------------------------------------------------------------------------- */

static void
async_init_thread (GSimpleAsyncResult *res,
                   GObject            *object,
                   GCancellable       *cancellable)
{
  GError *error = NULL;

  if (!g_initable_init (G_INITABLE (object), cancellable, &error))
    {
      g_simple_async_result_set_from_error (res, error);
      g_error_free (error);
    }
}

static void
async_initable_init_async (GAsyncInitable      *initable,
                           gint                 io_priority,
                           GCancellable        *cancellable,
                           GAsyncReadyCallback  callback,
                           gpointer             user_data)
{
  GSimpleAsyncResult *res;

  g_return_if_fail (G_IS_INITABLE (initable));

  res = g_simple_async_result_new (G_OBJECT (initable), callback, user_data,
                                   async_initable_init_async);
  g_simple_async_result_run_in_thread (res, async_init_thread,
                                       io_priority, cancellable);
  g_object_unref (res);
}

static gboolean
async_initable_init_finish (GAsyncInitable  *initable,
                            GAsyncResult    *res,
                            GError         **error)
{
  return TRUE; /* Errors handled by base impl */
}

static void
async_initable_iface_init (GAsyncInitableIface *async_initable_iface)
{
  /* We basically just want to use GIO's default implementation - though that one is
   * unfortunately broken, see #615111. So we copy-paste a fixed-up version.
   */
  async_initable_iface->init_async = async_initable_init_async;
  async_initable_iface->init_finish = async_initable_init_finish;
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_dbus_connection_new:
 * @stream: A #GIOStream.
 * @guid: The GUID to use if a authenticating as a server or %NULL.
 * @flags: Flags describing how to make the connection.
 * @observer: A #GDBusAuthObserver or %NULL.
 * @cancellable: A #GCancellable or %NULL.
 * @callback: A #GAsyncReadyCallback to call when the request is satisfied.
 * @user_data: The data to pass to @callback.
 *
 * Asynchronously sets up a D-Bus connection for exchanging D-Bus messages
 * with the end represented by @stream.
 *
 * If %G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_SERVER is set in @flags,
 * @observer (if not %NULL) is used to assist in the client
 * authentication process.
 *
 * When the operation is finished, @callback will be invoked. You can
 * then call g_dbus_connection_new_finish() to get the result of the
 * operation.
 *
 * This is a asynchronous failable constructor. See
 * g_dbus_connection_new_sync() for the synchronous
 * version.
 *
 * Since: 2.26
 */
void
g_dbus_connection_new (GIOStream            *stream,
                       const gchar          *guid,
                       GDBusConnectionFlags  flags,
                       GDBusAuthObserver    *observer,
                       GCancellable         *cancellable,
                       GAsyncReadyCallback   callback,
                       gpointer              user_data)
{
  g_return_if_fail (G_IS_IO_STREAM (stream));
  g_async_initable_new_async (G_TYPE_DBUS_CONNECTION,
                              G_PRIORITY_DEFAULT,
                              cancellable,
                              callback,
                              user_data,
                              "stream", stream,
                              "guid", guid,
                              "flags", flags,
                              "authentication-observer", observer,
                              NULL);
}

/**
 * g_dbus_connection_new_finish:
 * @res: A #GAsyncResult obtained from the #GAsyncReadyCallback passed to g_dbus_connection_new().
 * @error: Return location for error or %NULL.
 *
 * Finishes an operation started with g_dbus_connection_new().
 *
 * Returns: A #GDBusConnection or %NULL if @error is set. Free with g_object_unref().
 *
 * Since: 2.26
 */
GDBusConnection *
g_dbus_connection_new_finish (GAsyncResult  *res,
                              GError       **error)
{
  GObject *object;
  GObject *source_object;

  g_return_val_if_fail (G_IS_ASYNC_RESULT (res), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  source_object = g_async_result_get_source_object (res);
  g_assert (source_object != NULL);
  object = g_async_initable_new_finish (G_ASYNC_INITABLE (source_object),
                                        res,
                                        error);
  g_object_unref (source_object);
  if (object != NULL)
    return G_DBUS_CONNECTION (object);
  else
    return NULL;
}

/**
 * g_dbus_connection_new_sync:
 * @stream: A #GIOStream.
 * @guid: The GUID to use if a authenticating as a server or %NULL.
 * @flags: Flags describing how to make the connection.
 * @observer: A #GDBusAuthObserver or %NULL.
 * @cancellable: A #GCancellable or %NULL.
 * @error: Return location for error or %NULL.
 *
 * Synchronously sets up a D-Bus connection for exchanging D-Bus messages
 * with the end represented by @stream.
 *
 * If %G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_SERVER is set in @flags,
 * @observer (if not %NULL) is used to assist in the client
 * authentication process.
 *
 * This is a synchronous failable constructor. See
 * g_dbus_connection_new() for the asynchronous version.
 *
 * Returns: A #GDBusConnection or %NULL if @error is set. Free with g_object_unref().
 *
 * Since: 2.26
 */
GDBusConnection *
g_dbus_connection_new_sync (GIOStream             *stream,
                            const gchar           *guid,
                            GDBusConnectionFlags   flags,
                            GDBusAuthObserver     *observer,
                            GCancellable          *cancellable,
                            GError               **error)
{
  g_return_val_if_fail (G_IS_IO_STREAM (stream), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);
  return g_initable_new (G_TYPE_DBUS_CONNECTION,
                         cancellable,
                         error,
                         "stream", stream,
                         "guid", guid,
                         "flags", flags,
                         "authentication-observer", observer,
                         NULL);
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_dbus_connection_new_for_address:
 * @address: A D-Bus address.
 * @flags: Flags describing how to make the connection.
 * @cancellable: A #GCancellable or %NULL.
 * @callback: A #GAsyncReadyCallback to call when the request is satisfied.
 * @user_data: The data to pass to @callback.
 *
 * Asynchronously connects and sets up a D-Bus client connection for
 * exchanging D-Bus messages with an endpoint specified by @address
 * which must be in the D-Bus address format.
 *
 * This constructor can only be used to initiate client-side
 * connections - use g_dbus_connection_new() if you need to act as the
 * server. In particular, @flags cannot contain the
 * %G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_SERVER or
 * %G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_ALLOW_ANONYMOUS flags.
 *
 * When the operation is finished, @callback will be invoked. You can
 * then call g_dbus_connection_new_finish() to get the result of the
 * operation.
 *
 * This is a asynchronous failable constructor. See
 * g_dbus_connection_new_for_address_sync() for the synchronous
 * version.
 *
 * Since: 2.26
 */
void
g_dbus_connection_new_for_address (const gchar          *address,
                                   GDBusConnectionFlags  flags,
                                   GCancellable         *cancellable,
                                   GAsyncReadyCallback   callback,
                                   gpointer              user_data)
{
  g_return_if_fail (address != NULL);
  g_async_initable_new_async (G_TYPE_DBUS_CONNECTION,
                              G_PRIORITY_DEFAULT,
                              cancellable,
                              callback,
                              user_data,
                              "address", address,
                              "flags", flags,
                              NULL);
}

/**
 * g_dbus_connection_new_for_address_finish:
 * @res: A #GAsyncResult obtained from the #GAsyncReadyCallback passed to g_dbus_connection_new().
 * @error: Return location for error or %NULL.
 *
 * Finishes an operation started with g_dbus_connection_new_for_address().
 *
 * Returns: A #GDBusConnection or %NULL if @error is set. Free with g_object_unref().
 *
 * Since: 2.26
 */
GDBusConnection *
g_dbus_connection_new_for_address_finish (GAsyncResult  *res,
                                          GError       **error)
{
  GObject *object;
  GObject *source_object;

  g_return_val_if_fail (G_IS_ASYNC_RESULT (res), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  source_object = g_async_result_get_source_object (res);
  g_assert (source_object != NULL);
  object = g_async_initable_new_finish (G_ASYNC_INITABLE (source_object),
                                        res,
                                        error);
  g_object_unref (source_object);
  if (object != NULL)
    return G_DBUS_CONNECTION (object);
  else
    return NULL;
}

/**
 * g_dbus_connection_new_for_address_sync:
 * @address: A D-Bus address.
 * @flags: Flags describing how to make the connection.
 * @cancellable: A #GCancellable or %NULL.
 * @error: Return location for error or %NULL.
 *
 * Synchronously connects and sets up a D-Bus client connection for
 * exchanging D-Bus messages with an endpoint specified by @address
 * which must be in the D-Bus address format.
 *
 * This constructor can only be used to initiate client-side
 * connections - use g_dbus_connection_new_sync() if you need to act
 * as the server. In particular, @flags cannot contain the
 * %G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_SERVER or
 * %G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_ALLOW_ANONYMOUS flags.
 *
 * This is a synchronous failable constructor. See
 * g_dbus_connection_new_for_address() for the asynchronous version.
 *
 * Returns: A #GDBusConnection or %NULL if @error is set. Free with g_object_unref().
 *
 * Since: 2.26
 */
GDBusConnection *
g_dbus_connection_new_for_address_sync (const gchar           *address,
                                        GDBusConnectionFlags   flags,
                                        GCancellable          *cancellable,
                                        GError               **error)
{
  g_return_val_if_fail (address != NULL, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);
  return g_initable_new (G_TYPE_DBUS_CONNECTION,
                         cancellable,
                         error,
                         "address", address,
                         "flags", flags,
                         NULL);
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_dbus_connection_set_exit_on_close:
 * @connection: A #GDBusConnection.
 * @exit_on_close: Whether the process should be terminated
 * when @connection is closed by the remote peer.
 *
 * Sets whether the process should be terminated when @connection is
 * closed by the remote peer. See #GDBusConnection:exit-on-close for
 * more details.
 *
 * Since: 2.26
 */
void
g_dbus_connection_set_exit_on_close (GDBusConnection *connection,
                                     gboolean         exit_on_close)
{
  g_return_if_fail (G_IS_DBUS_CONNECTION (connection));
  connection->priv->exit_on_close = exit_on_close;
}

/**
 * g_dbus_connection_get_exit_on_close:
 * @connection: A #GDBusConnection.
 *
 * Gets whether the process is terminated when @connection is
 * closed by the remote peer. See
 * #GDBusConnection:exit-on-close for more details.
 *
 * Returns: Whether the process is terminated when @connection is
 * closed by the remote peer.
 *
 * Since: 2.26
 */
gboolean
g_dbus_connection_get_exit_on_close (GDBusConnection *connection)
{
  g_return_val_if_fail (G_IS_DBUS_CONNECTION (connection), FALSE);
  return connection->priv->exit_on_close;
}

/**
 * g_dbus_connection_get_guid:
 * @connection: A #GDBusConnection.
 *
 * The GUID of the peer performing the role of server when
 * authenticating. See #GDBusConnection:guid for more details.
 *
 * Returns: The GUID. Do not free this string, it is owned by
 * @connection.
 *
 * Since: 2.26
 */
const gchar *
g_dbus_connection_get_guid (GDBusConnection *connection)
{
  g_return_val_if_fail (G_IS_DBUS_CONNECTION (connection), NULL);
  return connection->priv->guid;
}

/**
 * g_dbus_connection_get_unique_name:
 * @connection: A #GDBusConnection.
 *
 * Gets the unique name of @connection as assigned by the message
 * bus. This can also be used to figure out if @connection is a
 * message bus connection.
 *
 * Returns: The unique name or %NULL if @connection is not a message
 * bus connection. Do not free this string, it is owned by
 * @connection.
 *
 * Since: 2.26
 */
const gchar *
g_dbus_connection_get_unique_name (GDBusConnection *connection)
{
  g_return_val_if_fail (G_IS_DBUS_CONNECTION (connection), NULL);
  return connection->priv->bus_unique_name;
}

/**
 * g_dbus_connection_get_peer_credentials:
 * @connection: A #GDBusConnection.
 *
 * Gets the credentials of the authenticated peer. This will always
 * return %NULL unless @connection acted as a server
 * (e.g. %G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_SERVER was passed)
 * when set up and the client passed credentials as part of the
 * authentication process.
 *
 * In a message bus setup, the message bus is always the server and
 * each application is a client. So this method will always return
 * %NULL for message bus clients.
 *
 * Returns: A #GCredentials or %NULL if not available. Do not free
 * this object, it is owned by @connection.
 *
 * Since: 2.26
 */
GCredentials *
g_dbus_connection_get_peer_credentials (GDBusConnection *connection)
{
  g_return_val_if_fail (G_IS_DBUS_CONNECTION (connection), NULL);
  return connection->priv->crendentials;
}

/* ---------------------------------------------------------------------------------------------------- */

static guint _global_filter_id = 1;

/**
 * g_dbus_connection_add_filter:
 * @connection: A #GDBusConnection.
 * @filter_function: A filter function.
 * @user_data: User data to pass to @filter_function.
 * @user_data_free_func: Function to free @user_data with when filter
 * is removed or %NULL.
 *
 * Adds a message filter. Filters are handlers that are run on all
 * incoming messages, prior to standard dispatch. Filters are run in
 * the order that they were added.  The same handler can be added as a
 * filter more than once, in which case it will be run more than once.
 * Filters added during a filter callback won't be run on the message
 * being processed.
 *
 * Note that filters are run in a dedicated message handling thread so
 * they can't block and, generally, can't do anything but signal a
 * worker thread. Also note that filters are rarely needed - use API
 * such as g_dbus_connection_send_message_with_reply(),
 * g_dbus_connection_signal_subscribe() or
 * g_dbus_connection_call() instead.
 *
 * Returns: A filter identifier that can be used with
 * g_dbus_connection_remove_filter().
 *
 * Since: 2.26
 */
guint
g_dbus_connection_add_filter (GDBusConnection            *connection,
                              GDBusMessageFilterFunction  filter_function,
                              gpointer                    user_data,
                              GDestroyNotify              user_data_free_func)
{
  FilterData *data;

  g_return_val_if_fail (G_IS_DBUS_CONNECTION (connection), 0);
  g_return_val_if_fail (filter_function != NULL, 0);

  CONNECTION_LOCK (connection);
  data = g_new0 (FilterData, 1);
  data->id = _global_filter_id++; /* TODO: overflow etc. */
  data->filter_function = filter_function;
  data->user_data = user_data;
  data->user_data_free_func = user_data_free_func;
  g_ptr_array_add (connection->priv->filters, data);
  CONNECTION_UNLOCK (connection);

  return data->id;
}

/* only called from finalize(), removes all filters */
static void
purge_all_filters (GDBusConnection *connection)
{
  guint n;
  for (n = 0; n < connection->priv->filters->len; n++)
    {
      FilterData *data = connection->priv->filters->pdata[n];
      if (data->user_data_free_func != NULL)
        data->user_data_free_func (data->user_data);
      g_free (data);
    }
}

/**
 * g_dbus_connection_remove_filter:
 * @connection: a #GDBusConnection
 * @filer_id: an identifier obtained from g_dbus_connection_add_filter()
 *
 * Removes a filter.
 *
 * Since: 2.26
 */
void
g_dbus_connection_remove_filter (GDBusConnection *connection,
                                 guint            filter_id)
{
  guint n;
  FilterData *to_destroy;

  g_return_if_fail (G_IS_DBUS_CONNECTION (connection));

  CONNECTION_LOCK (connection);
  to_destroy = NULL;
  for (n = 0; n < connection->priv->filters->len; n++)
    {
      FilterData *data = connection->priv->filters->pdata[n];
      if (data->id == filter_id)
        {
          g_ptr_array_remove_index (connection->priv->filters, n);
          to_destroy = data;
          break;
        }
    }
  CONNECTION_UNLOCK (connection);

  /* do free without holding lock */
  if (to_destroy != NULL)
    {
      if (to_destroy->user_data_free_func != NULL)
        to_destroy->user_data_free_func (to_destroy->user_data);
      g_free (to_destroy);
    }
  else
    {
      g_warning ("g_dbus_connection_remove_filter: No filter found for filter_id %d", filter_id);
    }
}

/* ---------------------------------------------------------------------------------------------------- */

typedef struct
{
  gchar *rule;
  gchar *sender;
  gchar *interface_name;
  gchar *member;
  gchar *object_path;
  gchar *arg0;
  GArray *subscribers;
} SignalData;

typedef struct
{
  GDBusSignalCallback callback;
  gpointer user_data;
  GDestroyNotify user_data_free_func;
  guint id;
  GMainContext *context;
} SignalSubscriber;

static void
signal_data_free (SignalData *data)
{
  g_free (data->rule);
  g_free (data->sender);
  g_free (data->interface_name);
  g_free (data->member);
  g_free (data->object_path);
  g_free (data->arg0);
  g_array_free (data->subscribers, TRUE);
  g_free (data);
}

static gchar *
args_to_rule (const gchar *sender,
              const gchar *interface_name,
              const gchar *member,
              const gchar *object_path,
              const gchar *arg0)
{
  GString *rule;

  rule = g_string_new ("type='signal'");
  if (sender != NULL)
    g_string_append_printf (rule, ",sender='%s'", sender);
  if (interface_name != NULL)
    g_string_append_printf (rule, ",interface='%s'", interface_name);
  if (member != NULL)
    g_string_append_printf (rule, ",member='%s'", member);
  if (object_path != NULL)
    g_string_append_printf (rule, ",path='%s'", object_path);
  if (arg0 != NULL)
    g_string_append_printf (rule, ",arg0='%s'", arg0);

  return g_string_free (rule, FALSE);
}

static guint _global_subscriber_id = 1;
static guint _global_registration_id = 1;
static guint _global_subtree_registration_id = 1;

/* ---------------------------------------------------------------------------------------------------- */

/* must hold lock when calling */
static void
add_match_rule (GDBusConnection *connection,
                const gchar     *match_rule)
{
  GError *error;
  GDBusMessage *message;

  message = g_dbus_message_new_method_call ("org.freedesktop.DBus", /* name */
                                            "/org/freedesktop/DBus", /* path */
                                            "org.freedesktop.DBus", /* interface */
                                            "AddMatch");
  g_dbus_message_set_body (message, g_variant_new ("(s)", match_rule));

  error = NULL;
  if (!g_dbus_connection_send_message_unlocked (connection,
                                                message,
                                                NULL,
                                                &error))
    {
      g_critical ("Error while sending AddMatch() message: %s", error->message);
      g_error_free (error);
    }
  g_object_unref (message);
}

/* ---------------------------------------------------------------------------------------------------- */

/* must hold lock when calling */
static void
remove_match_rule (GDBusConnection *connection,
                   const gchar     *match_rule)
{
  GError *error;
  GDBusMessage *message;

  message = g_dbus_message_new_method_call ("org.freedesktop.DBus", /* name */
                                            "/org/freedesktop/DBus", /* path */
                                            "org.freedesktop.DBus", /* interface */
                                            "RemoveMatch");
  g_dbus_message_set_body (message, g_variant_new ("(s)", match_rule));

  error = NULL;
  if (!g_dbus_connection_send_message_unlocked (connection,
                                                message,
                                                NULL,
                                                &error))
    {
      g_critical ("Error while sending RemoveMatch() message: %s", error->message);
      g_error_free (error);
    }
  g_object_unref (message);
}

/* ---------------------------------------------------------------------------------------------------- */

static gboolean
is_signal_data_for_name_lost_or_acquired (SignalData *signal_data)
{
  return g_strcmp0 (signal_data->sender, "org.freedesktop.DBus") == 0 &&
         g_strcmp0 (signal_data->interface_name, "org.freedesktop.DBus") == 0 &&
         g_strcmp0 (signal_data->object_path, "/org/freedesktop/DBus") == 0 &&
         (g_strcmp0 (signal_data->member, "NameLost") == 0 ||
          g_strcmp0 (signal_data->member, "NameAcquired") == 0);
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_dbus_connection_signal_subscribe:
 * @connection: A #GDBusConnection.
 * @sender: Sender name to match on. Must be either <literal>org.freedesktop.DBus</literal> (for listening to signals from the message bus daemon) or a unique name or %NULL to listen from all senders.
 * @interface_name: D-Bus interface name to match on or %NULL to match on all interfaces.
 * @member: D-Bus signal name to match on or %NULL to match on all signals.
 * @object_path: Object path to match on or %NULL to match on all object paths.
 * @arg0: Contents of first string argument to match on or %NULL to match on all kinds of arguments.
 * @callback: Callback to invoke when there is a signal matching the requested data.
 * @user_data: User data to pass to @callback.
 * @user_data_free_func: Function to free @user_data with when subscription is removed or %NULL.
 *
 * Subscribes to signals on @connection and invokes @callback with a
 * whenever the signal is received. Note that @callback
 * will be invoked in the <link
 * linkend="g-main-context-push-thread-default">thread-default main
 * loop</link> of the thread you are calling this method from.
 *
 * It is considered a programming error to use this function if @connection is closed.
 *
 * Note that if @sender is not <literal>org.freedesktop.DBus</literal> (for listening to signals from the
 * message bus daemon), then it needs to be a unique bus name or %NULL (for listening to signals from any
 * name) - you cannot pass a name like <literal>com.example.MyApp</literal>.
 * Use e.g. g_bus_watch_name() to find the unique name for the owner of the name you are interested in. Also note
 * that this function does not remove a subscription if @sender vanishes from the bus. You have to manually
 * call g_dbus_connection_signal_unsubscribe() to remove a subscription.
 *
 * Returns: A subscription identifier that can be used with g_dbus_connection_signal_unsubscribe().
 *
 * Since: 2.26
 */
guint
g_dbus_connection_signal_subscribe (GDBusConnection     *connection,
                                    const gchar         *sender,
                                    const gchar         *interface_name,
                                    const gchar         *member,
                                    const gchar         *object_path,
                                    const gchar         *arg0,
                                    GDBusSignalCallback  callback,
                                    gpointer             user_data,
                                    GDestroyNotify       user_data_free_func)
{
  gchar *rule;
  SignalData *signal_data;
  SignalSubscriber subscriber;
  GPtrArray *signal_data_array;

  /* Right now we abort if AddMatch() fails since it can only fail with the bus being in
   * an OOM condition. We might want to change that but that would involve making
   * g_dbus_connection_signal_subscribe() asynchronous and having the call sites
   * handle that. And there's really no sensible way of handling this short of retrying
   * to add the match rule... and then there's the little thing that, hey, maybe there's
   * a reason the bus in an OOM condition.
   *
   * Doable, but not really sure it's worth it...
   */

  g_return_val_if_fail (G_IS_DBUS_CONNECTION (connection), 0);
  g_return_val_if_fail (!g_dbus_connection_is_closed (connection), 0);
  g_return_val_if_fail (sender == NULL || ((strcmp (sender, "org.freedesktop.DBus") == 0 || sender[0] == ':') &&
                                           (connection->priv->flags & G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION)), 0);
  g_return_val_if_fail (interface_name == NULL || g_dbus_is_interface_name (interface_name), 0);
  g_return_val_if_fail (member == NULL || g_dbus_is_member_name (member), 0);
  g_return_val_if_fail (object_path == NULL || g_variant_is_object_path (object_path), 0);
  g_return_val_if_fail (callback != NULL, 0);

  CONNECTION_LOCK (connection);

  rule = args_to_rule (sender, interface_name, member, object_path, arg0);

  if (sender == NULL)
    sender = "";

  subscriber.callback = callback;
  subscriber.user_data = user_data;
  subscriber.user_data_free_func = user_data_free_func;
  subscriber.id = _global_subscriber_id++; /* TODO: overflow etc. */
  subscriber.context = g_main_context_get_thread_default ();
  if (subscriber.context != NULL)
    g_main_context_ref (subscriber.context);

  /* see if we've already have this rule */
  signal_data = g_hash_table_lookup (connection->priv->map_rule_to_signal_data, rule);
  if (signal_data != NULL)
    {
      g_array_append_val (signal_data->subscribers, subscriber);
      g_free (rule);
      goto out;
    }

  signal_data = g_new0 (SignalData, 1);
  signal_data->rule           = rule;
  signal_data->sender         = g_strdup (sender);
  signal_data->interface_name = g_strdup (interface_name);
  signal_data->member         = g_strdup (member);
  signal_data->object_path    = g_strdup (object_path);
  signal_data->arg0           = g_strdup (arg0);
  signal_data->subscribers    = g_array_new (FALSE, FALSE, sizeof (SignalSubscriber));
  g_array_append_val (signal_data->subscribers, subscriber);

  g_hash_table_insert (connection->priv->map_rule_to_signal_data,
                       signal_data->rule,
                       signal_data);

  /* Add the match rule to the bus...
   *
   * Avoid adding match rules for NameLost and NameAcquired messages - the bus will
   * always send such messages to us.
   */
  if (connection->priv->flags & G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION)
    {
      if (!is_signal_data_for_name_lost_or_acquired (signal_data))
        add_match_rule (connection, signal_data->rule);
    }

 out:
  g_hash_table_insert (connection->priv->map_id_to_signal_data,
                       GUINT_TO_POINTER (subscriber.id),
                       signal_data);

  signal_data_array = g_hash_table_lookup (connection->priv->map_sender_to_signal_data_array,
                                           signal_data->sender);
  if (signal_data_array == NULL)
    {
      signal_data_array = g_ptr_array_new ();
      g_hash_table_insert (connection->priv->map_sender_to_signal_data_array,
                           g_strdup (signal_data->sender),
                           signal_data_array);
    }
  g_ptr_array_add (signal_data_array, signal_data);

  CONNECTION_UNLOCK (connection);

  return subscriber.id;
}

/* ---------------------------------------------------------------------------------------------------- */

/* must hold lock when calling this */
static void
unsubscribe_id_internal (GDBusConnection *connection,
                         guint            subscription_id,
                         GArray          *out_removed_subscribers)
{
  SignalData *signal_data;
  GPtrArray *signal_data_array;
  guint n;

  signal_data = g_hash_table_lookup (connection->priv->map_id_to_signal_data,
                                     GUINT_TO_POINTER (subscription_id));
  if (signal_data == NULL)
    {
      /* Don't warn here, we may have thrown all subscriptions out when the connection was closed */
      goto out;
    }

  for (n = 0; n < signal_data->subscribers->len; n++)
    {
      SignalSubscriber *subscriber;

      subscriber = &(g_array_index (signal_data->subscribers, SignalSubscriber, n));
      if (subscriber->id != subscription_id)
        continue;

      g_warn_if_fail (g_hash_table_remove (connection->priv->map_id_to_signal_data,
                                           GUINT_TO_POINTER (subscription_id)));
      g_array_append_val (out_removed_subscribers, *subscriber);
      g_array_remove_index (signal_data->subscribers, n);

      if (signal_data->subscribers->len == 0)
        g_warn_if_fail (g_hash_table_remove (connection->priv->map_rule_to_signal_data, signal_data->rule));

      signal_data_array = g_hash_table_lookup (connection->priv->map_sender_to_signal_data_array,
                                               signal_data->sender);
      g_warn_if_fail (signal_data_array != NULL);
      g_warn_if_fail (g_ptr_array_remove (signal_data_array, signal_data));

      if (signal_data_array->len == 0)
        {
          g_warn_if_fail (g_hash_table_remove (connection->priv->map_sender_to_signal_data_array, signal_data->sender));

          /* remove the match rule from the bus unless NameLost or NameAcquired (see subscribe()) */
          if (connection->priv->flags & G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION)
            {
              if (!is_signal_data_for_name_lost_or_acquired (signal_data))
                remove_match_rule (connection, signal_data->rule);
            }

          signal_data_free (signal_data);
        }

      goto out;
    }

  g_assert_not_reached ();

 out:
  ;
}

/**
 * g_dbus_connection_signal_unsubscribe:
 * @connection: A #GDBusConnection.
 * @subscription_id: A subscription id obtained from g_dbus_connection_signal_subscribe().
 *
 * Unsubscribes from signals.
 *
 * Since: 2.26
 */
void
g_dbus_connection_signal_unsubscribe (GDBusConnection *connection,
                                      guint            subscription_id)
{
  GArray *subscribers;
  guint n;

  g_return_if_fail (G_IS_DBUS_CONNECTION (connection));

  subscribers = g_array_new (FALSE, FALSE, sizeof (SignalSubscriber));

  CONNECTION_LOCK (connection);
  unsubscribe_id_internal (connection,
                           subscription_id,
                           subscribers);
  CONNECTION_UNLOCK (connection);

  /* invariant */
  g_assert (subscribers->len == 0 || subscribers->len == 1);

  /* call GDestroyNotify without lock held */
  for (n = 0; n < subscribers->len; n++)
    {
      SignalSubscriber *subscriber;
      subscriber = &(g_array_index (subscribers, SignalSubscriber, n));
      if (subscriber->user_data_free_func != NULL)
        subscriber->user_data_free_func (subscriber->user_data);
      if (subscriber->context != NULL)
        g_main_context_unref (subscriber->context);
    }

  g_array_free (subscribers, TRUE);
}

/* ---------------------------------------------------------------------------------------------------- */

typedef struct
{
  guint                subscription_id;
  GDBusSignalCallback  callback;
  gpointer             user_data;
  GDBusMessage        *message;
  GDBusConnection     *connection;
  const gchar         *sender;
  const gchar         *path;
  const gchar         *interface;
  const gchar         *member;
} SignalInstance;

/* called on delivery thread (e.g. where g_dbus_connection_signal_subscribe() was called) with
 * no locks held
 */
static gboolean
emit_signal_instance_in_idle_cb (gpointer data)
{
  SignalInstance *signal_instance = data;
  GVariant *parameters;
  gboolean has_subscription;

  parameters = g_dbus_message_get_body (signal_instance->message);
  if (parameters == NULL)
    {
      parameters = g_variant_new ("()");
      g_variant_ref_sink (parameters);
    }
  else
    {
      g_variant_ref_sink (parameters);
    }

#if 0
  g_debug ("in emit_signal_instance_in_idle_cb (sender=%s path=%s interface=%s member=%s params=%s)",
           signal_instance->sender,
           signal_instance->path,
           signal_instance->interface,
           signal_instance->member,
           g_variant_print (parameters, TRUE));
#endif

  /* Careful here, don't do the callback if we no longer has the subscription */
  CONNECTION_LOCK (signal_instance->connection);
  has_subscription = FALSE;
  if (g_hash_table_lookup (signal_instance->connection->priv->map_id_to_signal_data,
                           GUINT_TO_POINTER (signal_instance->subscription_id)) != NULL)
    has_subscription = TRUE;
  CONNECTION_UNLOCK (signal_instance->connection);

  if (has_subscription)
    signal_instance->callback (signal_instance->connection,
                               signal_instance->sender,
                               signal_instance->path,
                               signal_instance->interface,
                               signal_instance->member,
                               parameters,
                               signal_instance->user_data);

  if (parameters != NULL)
    g_variant_unref (parameters);

  return FALSE;
}

static void
signal_instance_free (SignalInstance *signal_instance)
{
  g_object_unref (signal_instance->message);
  g_object_unref (signal_instance->connection);
  g_free (signal_instance);
}

/* called in message handler thread WITH lock held */
static void
schedule_callbacks (GDBusConnection *connection,
                    GPtrArray       *signal_data_array,
                    GDBusMessage    *message,
                    const gchar     *sender)
{
  guint n, m;
  const gchar *interface;
  const gchar *member;
  const gchar *path;
  const gchar *arg0;

  interface = NULL;
  member = NULL;
  path = NULL;
  arg0 = NULL;

  interface = g_dbus_message_get_interface (message);
  member = g_dbus_message_get_member (message);
  path = g_dbus_message_get_path (message);
  arg0 = g_dbus_message_get_arg0 (message);

#if 0
  g_debug ("sender    = `%s'", sender);
  g_debug ("interface = `%s'", interface);
  g_debug ("member    = `%s'", member);
  g_debug ("path      = `%s'", path);
  g_debug ("arg0      = `%s'", arg0);
#endif

  /* TODO: if this is slow, then we can change signal_data_array into
   *       map_object_path_to_signal_data_array or something.
   */
  for (n = 0; n < signal_data_array->len; n++)
    {
      SignalData *signal_data = signal_data_array->pdata[n];

      if (signal_data->interface_name != NULL && g_strcmp0 (signal_data->interface_name, interface) != 0)
        continue;

      if (signal_data->member != NULL && g_strcmp0 (signal_data->member, member) != 0)
        continue;

      if (signal_data->object_path != NULL && g_strcmp0 (signal_data->object_path, path) != 0)
        continue;

      if (signal_data->arg0 != NULL && g_strcmp0 (signal_data->arg0, arg0) != 0)
        continue;

      for (m = 0; m < signal_data->subscribers->len; m++)
        {
          SignalSubscriber *subscriber;
          GSource *idle_source;
          SignalInstance *signal_instance;

          subscriber = &(g_array_index (signal_data->subscribers, SignalSubscriber, m));

          signal_instance = g_new0 (SignalInstance, 1);
          signal_instance->subscription_id = subscriber->id;
          signal_instance->callback = subscriber->callback;
          signal_instance->user_data = subscriber->user_data;
          signal_instance->message = g_object_ref (message);
          signal_instance->connection = g_object_ref (connection);
          signal_instance->sender = sender;
          signal_instance->path = path;
          signal_instance->interface = interface;
          signal_instance->member = member;

          idle_source = g_idle_source_new ();
          g_source_set_priority (idle_source, G_PRIORITY_DEFAULT);
          g_source_set_callback (idle_source,
                                 emit_signal_instance_in_idle_cb,
                                 signal_instance,
                                 (GDestroyNotify) signal_instance_free);
          g_source_attach (idle_source, subscriber->context);
          g_source_unref (idle_source);
        }
    }
}

/* called in message handler thread with lock held */
static void
distribute_signals (GDBusConnection *connection,
                    GDBusMessage    *message)
{
  GPtrArray *signal_data_array;
  const gchar *sender;

  sender = g_dbus_message_get_sender (message);

  /* collect subscribers that match on sender */
  if (sender != NULL)
    {
      signal_data_array = g_hash_table_lookup (connection->priv->map_sender_to_signal_data_array, sender);
      if (signal_data_array != NULL)
        schedule_callbacks (connection, signal_data_array, message, sender);
    }

  /* collect subscribers not matching on sender */
  signal_data_array = g_hash_table_lookup (connection->priv->map_sender_to_signal_data_array, "");
  if (signal_data_array != NULL)
    schedule_callbacks (connection, signal_data_array, message, sender);
}

/* ---------------------------------------------------------------------------------------------------- */

/* only called from finalize(), removes all subscriptions */
static void
purge_all_signal_subscriptions (GDBusConnection *connection)
{
  GHashTableIter iter;
  gpointer key;
  GArray *ids;
  GArray *subscribers;
  guint n;

  ids = g_array_new (FALSE, FALSE, sizeof (guint));
  g_hash_table_iter_init (&iter, connection->priv->map_id_to_signal_data);
  while (g_hash_table_iter_next (&iter, &key, NULL))
    {
      guint subscription_id = GPOINTER_TO_UINT (key);
      g_array_append_val (ids, subscription_id);
    }

  subscribers = g_array_new (FALSE, FALSE, sizeof (SignalSubscriber));
  for (n = 0; n < ids->len; n++)
    {
      guint subscription_id = g_array_index (ids, guint, n);
      unsubscribe_id_internal (connection,
                               subscription_id,
                               subscribers);
    }
  g_array_free (ids, TRUE);

  /* call GDestroyNotify without lock held */
  for (n = 0; n < subscribers->len; n++)
    {
      SignalSubscriber *subscriber;
      subscriber = &(g_array_index (subscribers, SignalSubscriber, n));
      if (subscriber->user_data_free_func != NULL)
        subscriber->user_data_free_func (subscriber->user_data);
      if (subscriber->context != NULL)
        g_main_context_unref (subscriber->context);
    }

  g_array_free (subscribers, TRUE);
}

/* ---------------------------------------------------------------------------------------------------- */

struct ExportedObject
{
  gchar *object_path;
  GDBusConnection *connection;

  /* maps gchar* -> ExportedInterface* */
  GHashTable *map_if_name_to_ei;
};

/* only called with lock held */
static void
exported_object_free (ExportedObject *eo)
{
  g_free (eo->object_path);
  g_hash_table_unref (eo->map_if_name_to_ei);
  g_free (eo);
}

typedef struct
{
  ExportedObject *eo;

  guint                       id;
  gchar                      *interface_name;
  const GDBusInterfaceVTable *vtable;
  const GDBusInterfaceInfo   *introspection_data;

  GMainContext               *context;
  gpointer                    user_data;
  GDestroyNotify              user_data_free_func;
} ExportedInterface;

/* called with lock held */
static void
exported_interface_free (ExportedInterface *ei)
{
  if (ei->user_data_free_func != NULL)
    /* TODO: push to thread-default mainloop */
    ei->user_data_free_func (ei->user_data);

  if (ei->context != NULL)
    g_main_context_unref (ei->context);

  g_free (ei->interface_name);
  g_free (ei);
}

/* ---------------------------------------------------------------------------------------------------- */

typedef struct
{
  GDBusConnection *connection;
  GDBusMessage *message;
  gpointer user_data;
  const char *property_name;
  const GDBusInterfaceVTable *vtable;
  const GDBusInterfaceInfo *interface_info;
  const GDBusPropertyInfo *property_info;
} PropertyData;

static void
property_data_free (PropertyData *data)
{
  g_object_unref (data->connection);
  g_object_unref (data->message);
  g_free (data);
}

/* called in thread where object was registered - no locks held */
static gboolean
invoke_get_property_in_idle_cb (gpointer _data)
{
  PropertyData *data = _data;
  GVariant *value;
  GError *error;
  GDBusMessage *reply;

  error = NULL;
  value = data->vtable->get_property (data->connection,
                                      g_dbus_message_get_sender (data->message),
                                      g_dbus_message_get_path (data->message),
                                      data->interface_info->name,
                                      data->property_name,
                                      &error,
                                      data->user_data);


  if (value != NULL)
    {
      g_assert_no_error (error);

      g_variant_ref_sink (value);
      reply = g_dbus_message_new_method_reply (data->message);
      g_dbus_message_set_body (reply, g_variant_new ("(v)", value));
      g_dbus_connection_send_message (data->connection, reply, NULL, NULL);
      g_variant_unref (value);
      g_object_unref (reply);
    }
  else
    {
      gchar *dbus_error_name;

      g_assert (error != NULL);

      dbus_error_name = g_dbus_error_encode_gerror (error);
      reply = g_dbus_message_new_method_error_literal (data->message,
                                                       dbus_error_name,
                                                       error->message);
      g_dbus_connection_send_message (data->connection, reply, NULL, NULL);
      g_free (dbus_error_name);
      g_error_free (error);
      g_object_unref (reply);
    }

  return FALSE;
}

/* called in thread where object was registered - no locks held */
static gboolean
invoke_set_property_in_idle_cb (gpointer _data)
{
  PropertyData *data = _data;
  GError *error;
  GDBusMessage *reply;
  GVariant *value;

  error = NULL;
  value = NULL;

  g_variant_get (g_dbus_message_get_body (data->message),
                 "(ssv)",
                 NULL,
                 NULL,
                 &value);

  /* Fail with org.freedesktop.DBus.Error.InvalidArgs if the type
   * of the given value is wrong
   */
  if (g_strcmp0 (g_variant_get_type_string (value), data->property_info->signature) != 0)
    {
      reply = g_dbus_message_new_method_error (data->message,
                                               "org.freedesktop.DBus.Error.InvalidArgs",
                                               _("Error setting property `%s': Expected type `%s' but got `%s'"),
                                               data->property_info->name,
                                               data->property_info->signature,
                                               g_variant_get_type_string (value));
      goto out;
    }

  if (!data->vtable->set_property (data->connection,
                                   g_dbus_message_get_sender (data->message),
                                   g_dbus_message_get_path (data->message),
                                   data->interface_info->name,
                                   data->property_name,
                                   value,
                                   &error,
                                   data->user_data))
    {
      gchar *dbus_error_name;
      g_assert (error != NULL);
      dbus_error_name = g_dbus_error_encode_gerror (error);
      reply = g_dbus_message_new_method_error_literal (data->message,
                                                       dbus_error_name,
                                                       error->message);
      g_free (dbus_error_name);
      g_error_free (error);
    }
  else
    {
      reply = g_dbus_message_new_method_reply (data->message);
    }

 out:
  g_assert (reply != NULL);
  g_dbus_connection_send_message (data->connection, reply, NULL, NULL);
  g_object_unref (reply);

  return FALSE;
}

/* called with lock held */
static gboolean
validate_and_maybe_schedule_property_getset (GDBusConnection            *connection,
                                             GDBusMessage               *message,
                                             gboolean                    is_get,
                                             const GDBusInterfaceInfo   *introspection_data,
                                             const GDBusInterfaceVTable *vtable,
                                             GMainContext               *main_context,
                                             gpointer                    user_data)
{
  gboolean handled;
  const char *interface_name;
  const char *property_name;
  const GDBusPropertyInfo *property_info;
  GSource *idle_source;
  PropertyData *property_data;
  GDBusMessage *reply;

  handled = FALSE;

  if (is_get)
    g_variant_get (g_dbus_message_get_body (message),
                   "(ss)",
                   &interface_name,
                   &property_name);
  else
    g_variant_get (g_dbus_message_get_body (message),
                   "(ssv)",
                   &interface_name,
                   &property_name,
                   NULL);


  if (is_get)
    {
      if (vtable == NULL || vtable->get_property == NULL)
        goto out;
    }
  else
    {
      if (vtable == NULL || vtable->set_property == NULL)
        goto out;
    }

  /* Check that the property exists - if not fail with org.freedesktop.DBus.Error.InvalidArgs
   */
  property_info = NULL;

  /* TODO: the cost of this is O(n) - it might be worth caching the result */
  property_info = g_dbus_interface_info_lookup_property (introspection_data, property_name);
  if (property_info == NULL)
    {
      reply = g_dbus_message_new_method_error (message,
                                               "org.freedesktop.DBus.Error.InvalidArgs",
                                               _("No such property `%s'"),
                                               property_name);
      g_dbus_connection_send_message_unlocked (connection, reply, NULL, NULL);
      g_object_unref (reply);
      handled = TRUE;
      goto out;
    }

  if (is_get && !(property_info->flags & G_DBUS_PROPERTY_INFO_FLAGS_READABLE))
    {
      reply = g_dbus_message_new_method_error (message,
                                               "org.freedesktop.DBus.Error.InvalidArgs",
                                               _("Property `%s' is not readable"),
                                               property_name);
      g_dbus_connection_send_message_unlocked (connection, reply, NULL, NULL);
      g_object_unref (reply);
      handled = TRUE;
      goto out;
    }
  else if (!is_get && !(property_info->flags & G_DBUS_PROPERTY_INFO_FLAGS_WRITABLE))
    {
      reply = g_dbus_message_new_method_error (message,
                                               "org.freedesktop.DBus.Error.InvalidArgs",
                                               _("Property `%s' is not writable"),
                                               property_name);
      g_dbus_connection_send_message_unlocked (connection, reply, NULL, NULL);
      g_object_unref (reply);
      handled = TRUE;
      goto out;
    }

  /* ok, got the property info - call user code in an idle handler */
  property_data = g_new0 (PropertyData, 1);
  property_data->connection = g_object_ref (connection);
  property_data->message = g_object_ref (message);
  property_data->user_data = user_data;
  property_data->property_name = property_name;
  property_data->vtable = vtable;
  property_data->interface_info = introspection_data;
  property_data->property_info = property_info;

  idle_source = g_idle_source_new ();
  g_source_set_priority (idle_source, G_PRIORITY_DEFAULT);
  g_source_set_callback (idle_source,
                         is_get ? invoke_get_property_in_idle_cb : invoke_set_property_in_idle_cb,
                         property_data,
                         (GDestroyNotify) property_data_free);
  g_source_attach (idle_source, main_context);
  g_source_unref (idle_source);

  handled = TRUE;

 out:
  return handled;
}

/* called with lock held */
static gboolean
handle_getset_property (GDBusConnection *connection,
                        ExportedObject  *eo,
                        GDBusMessage    *message,
                        gboolean         is_get)
{
  ExportedInterface *ei;
  gboolean handled;
  const char *interface_name;
  const char *property_name;

  handled = FALSE;

  if (is_get)
    g_variant_get (g_dbus_message_get_body (message),
                   "(ss)",
                   &interface_name,
                   &property_name);
  else
    g_variant_get (g_dbus_message_get_body (message),
                   "(ssv)",
                   &interface_name,
                   &property_name,
                   NULL);

  /* Fail with org.freedesktop.DBus.Error.InvalidArgs if there is
   * no such interface registered
   */
  ei = g_hash_table_lookup (eo->map_if_name_to_ei, interface_name);
  if (ei == NULL)
    {
      GDBusMessage *reply;
      reply = g_dbus_message_new_method_error (message,
                                               "org.freedesktop.DBus.Error.InvalidArgs",
                                               _("No such interface `%s'"),
                                               interface_name);
      g_dbus_connection_send_message_unlocked (eo->connection, reply, NULL, NULL);
      g_object_unref (reply);
      handled = TRUE;
      goto out;
    }

  handled = validate_and_maybe_schedule_property_getset (eo->connection,
                                                         message,
                                                         is_get,
                                                         ei->introspection_data,
                                                         ei->vtable,
                                                         ei->context,
                                                         ei->user_data);
 out:
  return handled;
}

/* ---------------------------------------------------------------------------------------------------- */

typedef struct
{
  GDBusConnection *connection;
  GDBusMessage *message;
  gpointer user_data;
  const GDBusInterfaceVTable *vtable;
  const GDBusInterfaceInfo *interface_info;
} PropertyGetAllData;

static void
property_get_all_data_free (PropertyData *data)
{
  g_object_unref (data->connection);
  g_object_unref (data->message);
  g_free (data);
}

/* called in thread where object was registered - no locks held */
static gboolean
invoke_get_all_properties_in_idle_cb (gpointer _data)
{
  PropertyGetAllData *data = _data;
  GVariantBuilder *builder;
  GVariant *packed;
  GVariant *result;
  GError *error;
  GDBusMessage *reply;
  guint n;

  error = NULL;

  /* TODO: Right now we never fail this call - we just omit values if
   *       a get_property() call is failing.
   *
   *       We could fail the whole call if just a single get_property() call
   *       returns an error. We need clarification in the D-Bus spec about this.
   */
  builder = g_variant_builder_new (G_VARIANT_TYPE_ARRAY);
  for (n = 0; data->interface_info->properties != NULL && data->interface_info->properties[n] != NULL; n++)
    {
      const GDBusPropertyInfo *property_info = data->interface_info->properties[n];
      GVariant *value;

      if (!(property_info->flags & G_DBUS_PROPERTY_INFO_FLAGS_READABLE))
        continue;

      value = data->vtable->get_property (data->connection,
                                          g_dbus_message_get_sender (data->message),
                                          g_dbus_message_get_path (data->message),
                                          data->interface_info->name,
                                          property_info->name,
                                          NULL,
                                          data->user_data);

      if (value == NULL)
        continue;

      g_variant_ref_sink (value);
      g_variant_builder_add (builder,
                             "{sv}",
                             property_info->name,
                             value);
      g_variant_unref (value);
    }
  result = g_variant_builder_end (builder);

  builder = g_variant_builder_new (G_VARIANT_TYPE_TUPLE);
  g_variant_builder_add_value (builder, result); /* steals result since result is floating */
  packed = g_variant_builder_end (builder);

  reply = g_dbus_message_new_method_reply (data->message);
  g_dbus_message_set_body (reply, packed);
  g_dbus_connection_send_message (data->connection, reply, NULL, NULL);
  g_object_unref (reply);

  return FALSE;
}

/* called with lock held */
static gboolean
validate_and_maybe_schedule_property_get_all (GDBusConnection            *connection,
                                              GDBusMessage               *message,
                                              const GDBusInterfaceInfo   *introspection_data,
                                              const GDBusInterfaceVTable *vtable,
                                              GMainContext               *main_context,
                                              gpointer                    user_data)
{
  gboolean handled;
  const char *interface_name;
  GSource *idle_source;
  PropertyGetAllData *property_get_all_data;

  handled = FALSE;

  g_variant_get (g_dbus_message_get_body (message),
                 "(s)",
                 &interface_name);

  if (vtable == NULL || vtable->get_property == NULL)
    goto out;

  /* ok, got the property info - call user in an idle handler */
  property_get_all_data = g_new0 (PropertyGetAllData, 1);
  property_get_all_data->connection = g_object_ref (connection);
  property_get_all_data->message = g_object_ref (message);
  property_get_all_data->user_data = user_data;
  property_get_all_data->vtable = vtable;
  property_get_all_data->interface_info = introspection_data;

  idle_source = g_idle_source_new ();
  g_source_set_priority (idle_source, G_PRIORITY_DEFAULT);
  g_source_set_callback (idle_source,
                         invoke_get_all_properties_in_idle_cb,
                         property_get_all_data,
                         (GDestroyNotify) property_get_all_data_free);
  g_source_attach (idle_source, main_context);
  g_source_unref (idle_source);

  handled = TRUE;

 out:
  return handled;
}

/* called with lock held */
static gboolean
handle_get_all_properties (GDBusConnection *connection,
                           ExportedObject  *eo,
                           GDBusMessage    *message)
{
  ExportedInterface *ei;
  gboolean handled;
  const char *interface_name;

  handled = FALSE;

  g_variant_get (g_dbus_message_get_body (message),
                 "(s)",
                 &interface_name);

  /* Fail with org.freedesktop.DBus.Error.InvalidArgs if there is
   * no such interface registered
   */
  ei = g_hash_table_lookup (eo->map_if_name_to_ei, interface_name);
  if (ei == NULL)
    {
      GDBusMessage *reply;
      reply = g_dbus_message_new_method_error (message,
                                               "org.freedesktop.DBus.Error.InvalidArgs",
                                               _("No such interface"),
                                               interface_name);
      g_dbus_connection_send_message_unlocked (eo->connection, reply, NULL, NULL);
      g_object_unref (reply);
      handled = TRUE;
      goto out;
    }

  handled = validate_and_maybe_schedule_property_get_all (eo->connection,
                                                          message,
                                                          ei->introspection_data,
                                                          ei->vtable,
                                                          ei->context,
                                                          ei->user_data);
 out:
  return handled;
}

/* ---------------------------------------------------------------------------------------------------- */

static const gchar introspect_header[] =
  "<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\"\n"
  "                      \"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">\n"
  "<!-- GDBus " PACKAGE_VERSION " -->\n"
  "<node>\n";

static const gchar introspect_tail[] =
  "</node>\n";

static const gchar introspect_standard_interfaces[] =
  "  <interface name=\"org.freedesktop.DBus.Properties\">\n"
  "    <method name=\"Get\">\n"
  "      <arg type=\"s\" name=\"interface_name\" direction=\"in\"/>\n"
  "      <arg type=\"s\" name=\"property_name\" direction=\"in\"/>\n"
  "      <arg type=\"v\" name=\"value\" direction=\"out\"/>\n"
  "    </method>\n"
  "    <method name=\"GetAll\">\n"
  "      <arg type=\"s\" name=\"interface_name\" direction=\"in\"/>\n"
  "      <arg type=\"a{sv}\" name=\"properties\" direction=\"out\"/>\n"
  "    </method>\n"
  "    <method name=\"Set\">\n"
  "      <arg type=\"s\" name=\"interface_name\" direction=\"in\"/>\n"
  "      <arg type=\"s\" name=\"property_name\" direction=\"in\"/>\n"
  "      <arg type=\"v\" name=\"value\" direction=\"in\"/>\n"
  "    </method>\n"
  "    <signal name=\"PropertiesChanged\">\n"
  "      <arg type=\"s\" name=\"interface_name\"/>\n"
  "      <arg type=\"a{sv}\" name=\"changed_properties\"/>\n"
  "    </signal>\n"
  "  </interface>\n"
  "  <interface name=\"org.freedesktop.DBus.Introspectable\">\n"
  "    <method name=\"Introspect\">\n"
  "      <arg type=\"s\" name=\"xml_data\" direction=\"out\"/>\n"
  "    </method>\n"
  "  </interface>\n"
  "  <interface name=\"org.freedesktop.DBus.Peer\">\n"
  "    <method name=\"Ping\"/>\n"
  "    <method name=\"GetMachineId\">\n"
  "      <arg type=\"s\" name=\"machine_uuid\" direction=\"out\"/>\n"
  "    </method>\n"
  "  </interface>\n";

static void
introspect_append_header (GString *s)
{
  g_string_append (s, introspect_header);
}

static void
introspect_append_standard_interfaces (GString *s)
{
  g_string_append (s, introspect_standard_interfaces);
}

static void
maybe_add_path (const gchar *path, gsize path_len, const gchar *object_path, GHashTable *set)
{
  if (g_str_has_prefix (object_path, path) && strlen (object_path) >= path_len)
    {
      const gchar *begin;
      const gchar *end;
      gchar *s;

      begin = object_path + path_len;
      end = strchr (begin, '/');

      if (end != NULL)
        s = g_strndup (begin, end - begin);
      else
        s = g_strdup (begin);

      if (g_hash_table_lookup (set, s) == NULL)
        g_hash_table_insert (set, s, GUINT_TO_POINTER (1));
      else
        g_free (s);
    }
}

/* TODO: we want a nicer public interface for this */
static gchar **
g_dbus_connection_list_registered_unlocked (GDBusConnection *connection,
                                            const gchar     *path)
{
  GPtrArray *p;
  gchar **ret;
  GHashTableIter hash_iter;
  const gchar *object_path;
  gsize path_len;
  GHashTable *set;
  GList *keys;
  GList *l;

  CONNECTION_ENSURE_LOCK (connection);

  path_len = strlen (path);
  if (path_len > 1)
    path_len++;

  set = g_hash_table_new (g_str_hash, g_str_equal);

  g_hash_table_iter_init (&hash_iter, connection->priv->map_object_path_to_eo);
  while (g_hash_table_iter_next (&hash_iter, (gpointer) &object_path, NULL))
    maybe_add_path (path, path_len, object_path, set);

  g_hash_table_iter_init (&hash_iter, connection->priv->map_object_path_to_es);
  while (g_hash_table_iter_next (&hash_iter, (gpointer) &object_path, NULL))
    maybe_add_path (path, path_len, object_path, set);

  p = g_ptr_array_new ();
  keys = g_hash_table_get_keys (set);
  for (l = keys; l != NULL; l = l->next)
    g_ptr_array_add (p, l->data);
  g_hash_table_unref (set);
  g_list_free (keys);

  g_ptr_array_add (p, NULL);
  ret = (gchar **) g_ptr_array_free (p, FALSE);
  return ret;
}

static gchar **
g_dbus_connection_list_registered (GDBusConnection *connection,
                                   const gchar     *path)
{
  gchar **ret;
  CONNECTION_LOCK (connection);
  ret = g_dbus_connection_list_registered_unlocked (connection, path);
  CONNECTION_UNLOCK (connection);
  return ret;
}

/* called in message handler thread with lock held */
static gboolean
handle_introspect (GDBusConnection *connection,
                   ExportedObject  *eo,
                   GDBusMessage    *message)
{
  guint n;
  GString *s;
  GDBusMessage *reply;
  GHashTableIter hash_iter;
  ExportedInterface *ei;
  gchar **registered;

  /* first the header with the standard interfaces */
  s = g_string_sized_new (sizeof (introspect_header) +
                          sizeof (introspect_standard_interfaces) +
                          sizeof (introspect_tail));
  introspect_append_header (s);
  introspect_append_standard_interfaces (s);

  /* then include the registered interfaces */
  g_hash_table_iter_init (&hash_iter, eo->map_if_name_to_ei);
  while (g_hash_table_iter_next (&hash_iter, NULL, (gpointer) &ei))
    g_dbus_interface_info_generate_xml (ei->introspection_data, 2, s);

  /* finally include nodes registered below us */
  registered = g_dbus_connection_list_registered_unlocked (connection, eo->object_path);
  for (n = 0; registered != NULL && registered[n] != NULL; n++)
    g_string_append_printf (s, "  <node name=\"%s\"/>\n", registered[n]);
  g_strfreev (registered);
  g_string_append (s, introspect_tail);

  reply = g_dbus_message_new_method_reply (message);
  g_dbus_message_set_body (reply, g_variant_new ("(s)", s->str));
  g_dbus_connection_send_message_unlocked (connection, reply, NULL, NULL);
  g_object_unref (reply);
  g_string_free (s, TRUE);

  return TRUE;
}

/* called in thread where object was registered - no locks held */
static gboolean
call_in_idle_cb (gpointer user_data)
{
  GDBusMethodInvocation *invocation = G_DBUS_METHOD_INVOCATION (user_data);
  GDBusInterfaceVTable *vtable;

  vtable = g_object_get_data (G_OBJECT (invocation), "g-dbus-interface-vtable");
  g_assert (vtable != NULL && vtable->method_call != NULL);

  vtable->method_call (g_dbus_method_invocation_get_connection (invocation),
                       g_dbus_method_invocation_get_sender (invocation),
                       g_dbus_method_invocation_get_object_path (invocation),
                       g_dbus_method_invocation_get_interface_name (invocation),
                       g_dbus_method_invocation_get_method_name (invocation),
                       g_dbus_method_invocation_get_parameters (invocation),
                       g_object_ref (invocation),
                       g_dbus_method_invocation_get_user_data (invocation));

  return FALSE;
}

/* called in message handler thread with lock held */
static gboolean
validate_and_maybe_schedule_method_call (GDBusConnection            *connection,
                                         GDBusMessage               *message,
                                         const GDBusInterfaceInfo   *introspection_data,
                                         const GDBusInterfaceVTable *vtable,
                                         GMainContext               *main_context,
                                         gpointer                    user_data)
{
  GDBusMethodInvocation *invocation;
  const GDBusMethodInfo *method_info;
  GDBusMessage *reply;
  GVariant *parameters;
  GSource *idle_source;
  gboolean handled;
  gchar *in_signature;

  handled = FALSE;

  /* TODO: the cost of this is O(n) - it might be worth caching the result */
  method_info = g_dbus_interface_info_lookup_method (introspection_data, g_dbus_message_get_member (message));

  /* if the method doesn't exist, return the org.freedesktop.DBus.Error.UnknownMethod
   * error to the caller
   */
  if (method_info == NULL)
    {
      reply = g_dbus_message_new_method_error (message,
                                               "org.freedesktop.DBus.Error.UnknownMethod",
                                               _("No such method `%s'"),
                                               g_dbus_message_get_member (message));
      g_dbus_connection_send_message_unlocked (connection, reply, NULL, NULL);
      g_object_unref (reply);
      handled = TRUE;
      goto out;
    }

  /* Check that the incoming args are of the right type - if they are not, return
   * the org.freedesktop.DBus.Error.InvalidArgs error to the caller
   *
   * TODO: might also be worth caching the combined signature.
   */
  in_signature = _g_dbus_compute_complete_signature (method_info->in_args, FALSE);
  if (g_strcmp0 (g_dbus_message_get_signature (message), in_signature) != 0)
    {
      reply = g_dbus_message_new_method_error (message,
                                               "org.freedesktop.DBus.Error.InvalidArgs",
                                               _("Signature of message, `%s', does not match expected signature `%s'"),
                                               g_dbus_message_get_signature (message),
                                               in_signature);
      g_dbus_connection_send_message_unlocked (connection, reply, NULL, NULL);
      g_object_unref (reply);
      g_free (in_signature);
      handled = TRUE;
      goto out;
    }
  g_free (in_signature);

  parameters = g_dbus_message_get_body (message);
  if (parameters == NULL)
    {
      parameters = g_variant_new ("()");
      g_variant_ref_sink (parameters);
    }
  else
    {
      g_variant_ref (parameters);
    }

  /* schedule the call in idle */
  invocation = g_dbus_method_invocation_new (g_dbus_message_get_sender (message),
                                             g_dbus_message_get_path (message),
                                             g_dbus_message_get_interface (message),
                                             g_dbus_message_get_member (message),
                                             method_info,
                                             connection,
                                             message,
                                             parameters,
                                             user_data);
  g_variant_unref (parameters);
  g_object_set_data (G_OBJECT (invocation),
                     "g-dbus-interface-vtable",
                     (gpointer) vtable);

  idle_source = g_idle_source_new ();
  g_source_set_priority (idle_source, G_PRIORITY_DEFAULT);
  g_source_set_callback (idle_source,
                         call_in_idle_cb,
                         invocation,
                         g_object_unref);
  g_source_attach (idle_source, main_context);
  g_source_unref (idle_source);

  handled = TRUE;

 out:
  return handled;
}

/* ---------------------------------------------------------------------------------------------------- */

/* called in message handler thread with lock held */
static gboolean
obj_message_func (GDBusConnection *connection,
                  ExportedObject  *eo,
                  GDBusMessage    *message)
{
  const gchar *interface_name;
  const gchar *member;
  const gchar *signature;
  gboolean handled;

  handled = FALSE;

  interface_name = g_dbus_message_get_interface (message);
  member = g_dbus_message_get_member (message);
  signature = g_dbus_message_get_signature (message);

  /* see if we have an interface for handling this call */
  if (interface_name != NULL)
    {
      ExportedInterface *ei;
      ei = g_hash_table_lookup (eo->map_if_name_to_ei, interface_name);
      if (ei != NULL)
        {
          /* we do - invoke the handler in idle in the right thread */

          /* handle no vtable or handler being present */
          if (ei->vtable == NULL || ei->vtable->method_call == NULL)
            goto out;

          handled = validate_and_maybe_schedule_method_call (connection,
                                                             message,
                                                             ei->introspection_data,
                                                             ei->vtable,
                                                             ei->context,
                                                             ei->user_data);
          goto out;
        }
    }

  if (g_strcmp0 (interface_name, "org.freedesktop.DBus.Introspectable") == 0 &&
      g_strcmp0 (member, "Introspect") == 0 &&
      g_strcmp0 (signature, "") == 0)
    {
      handled = handle_introspect (connection, eo, message);
      goto out;
    }
  else if (g_strcmp0 (interface_name, "org.freedesktop.DBus.Properties") == 0 &&
           g_strcmp0 (member, "Get") == 0 &&
           g_strcmp0 (signature, "ss") == 0)
    {
      handled = handle_getset_property (connection, eo, message, TRUE);
      goto out;
    }
  else if (g_strcmp0 (interface_name, "org.freedesktop.DBus.Properties") == 0 &&
           g_strcmp0 (member, "Set") == 0 &&
           g_strcmp0 (signature, "ssv") == 0)
    {
      handled = handle_getset_property (connection, eo, message, FALSE);
      goto out;
    }
  else if (g_strcmp0 (interface_name, "org.freedesktop.DBus.Properties") == 0 &&
           g_strcmp0 (member, "GetAll") == 0 &&
           g_strcmp0 (signature, "s") == 0)
    {
      handled = handle_get_all_properties (connection, eo, message);
      goto out;
    }

 out:
  return handled;
}

/**
 * g_dbus_connection_register_object:
 * @connection: A #GDBusConnection.
 * @object_path: The object path to register at.
 * @interface_name: The D-Bus interface to register.
 * @introspection_data: Introspection data for the interface.
 * @vtable: A #GDBusInterfaceVTable to call into or %NULL.
 * @user_data: Data to pass to functions in @vtable.
 * @user_data_free_func: Function to call when the object path is unregistered.
 * @error: Return location for error or %NULL.
 *
 * Registers callbacks for exported objects at @object_path with the
 * D-Bus interface @interface_name.
 *
 * Calls to functions in @vtable (and @user_data_free_func) will
 * happen in the <link linkend="g-main-context-push-thread-default">thread-default main
 * loop</link> of the thread you are calling this method from.
 *
 * Note that all #GVariant values passed to functions in @vtable will match
 * the signature given in @introspection_data - if a remote caller passes
 * incorrect values, the <literal>org.freedesktop.DBus.Error.InvalidArgs</literal>
 * is returned to the remote caller.
 *
 * Additionally, if the remote caller attempts to invoke methods or
 * access properties not mentioned in @introspection_data the
 * <literal>org.freedesktop.DBus.Error.UnknownMethod</literal> resp.
 * <literal>org.freedesktop.DBus.Error.InvalidArgs</literal> errors
 * are returned to the caller.
 *
 * It is considered a programming error if the
 * #GDBusInterfaceGetPropertyFunc function in @vtable returns a
 * #GVariant of incorrect type.
 *
 * If an existing callback is already registered at @object_path and
 * @interface_name, then @error is set to #G_IO_ERROR_EXISTS.
 *
 * See <xref linkend="gdbus-server"/> for an example of how to use this method.
 *
 * Returns: 0 if @error is set, otherwise a registration id (never 0)
 * that can be used with g_dbus_connection_unregister_object() .
 *
 * Since: 2.26
 */
guint
g_dbus_connection_register_object (GDBusConnection            *connection,
                                   const gchar                *object_path,
                                   const gchar                *interface_name,
                                   const GDBusInterfaceInfo   *introspection_data,
                                   const GDBusInterfaceVTable *vtable,
                                   gpointer                    user_data,
                                   GDestroyNotify              user_data_free_func,
                                   GError                    **error)
{
  ExportedObject *eo;
  ExportedInterface *ei;
  guint ret;

  g_return_val_if_fail (G_IS_DBUS_CONNECTION (connection), 0);
  g_return_val_if_fail (!g_dbus_connection_is_closed (connection), 0);
  g_return_val_if_fail (object_path != NULL && g_variant_is_object_path (object_path), 0);
  g_return_val_if_fail (interface_name == NULL || g_dbus_is_interface_name (interface_name), 0);
  g_return_val_if_fail (introspection_data != NULL, 0);
  g_return_val_if_fail (error == NULL || *error == NULL, 0);

  ret = 0;

  CONNECTION_LOCK (connection);

  eo = g_hash_table_lookup (connection->priv->map_object_path_to_eo, object_path);
  if (eo == NULL)
    {
      eo = g_new0 (ExportedObject, 1);
      eo->object_path = g_strdup (object_path);
      eo->connection = connection;
      eo->map_if_name_to_ei = g_hash_table_new_full (g_str_hash,
                                                     g_str_equal,
                                                     NULL,
                                                     (GDestroyNotify) exported_interface_free);
      g_hash_table_insert (connection->priv->map_object_path_to_eo, eo->object_path, eo);
    }

  ei = g_hash_table_lookup (eo->map_if_name_to_ei, interface_name);
  if (ei != NULL)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_EXISTS,
                   _("An object is already exported for the interface %s at %s"),
                   interface_name,
                   object_path);
      goto out;
    }

  ei = g_new0 (ExportedInterface, 1);
  ei->id = _global_registration_id++; /* TODO: overflow etc. */
  ei->eo = eo;
  ei->user_data = user_data;
  ei->user_data_free_func = user_data_free_func;
  ei->vtable = vtable;
  ei->introspection_data = introspection_data;
  ei->interface_name = g_strdup (interface_name);
  ei->context = g_main_context_get_thread_default ();
  if (ei->context != NULL)
    g_main_context_ref (ei->context);

  g_hash_table_insert (eo->map_if_name_to_ei,
                       (gpointer) ei->interface_name,
                       ei);
  g_hash_table_insert (connection->priv->map_id_to_ei,
                       GUINT_TO_POINTER (ei->id),
                       ei);

  ret = ei->id;

 out:
  CONNECTION_UNLOCK (connection);

  return ret;
}

/**
 * g_dbus_connection_unregister_object:
 * @connection: A #GDBusConnection.
 * @registration_id: A registration id obtained from g_dbus_connection_register_object().
 *
 * Unregisters an object.
 *
 * Returns: %TRUE if the object was unregistered, %FALSE otherwise.
 *
 * Since: 2.26
 */
gboolean
g_dbus_connection_unregister_object (GDBusConnection *connection,
                                     guint            registration_id)
{
  ExportedInterface *ei;
  ExportedObject *eo;
  gboolean ret;

  g_return_val_if_fail (G_IS_DBUS_CONNECTION (connection), FALSE);

  ret = FALSE;

  CONNECTION_LOCK (connection);

  ei = g_hash_table_lookup (connection->priv->map_id_to_ei,
                            GUINT_TO_POINTER (registration_id));
  if (ei == NULL)
    goto out;

  eo = ei->eo;

  g_warn_if_fail (g_hash_table_remove (connection->priv->map_id_to_ei, GUINT_TO_POINTER (ei->id)));
  g_warn_if_fail (g_hash_table_remove (eo->map_if_name_to_ei, ei->interface_name));
  /* unregister object path if we have no more exported interfaces */
  if (g_hash_table_size (eo->map_if_name_to_ei) == 0)
    g_warn_if_fail (g_hash_table_remove (connection->priv->map_object_path_to_eo,
                                         eo->object_path));

  ret = TRUE;

 out:
  CONNECTION_UNLOCK (connection);

  return ret;
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_dbus_connection_emit_signal:
 * @connection: A #GDBusConnection.
 * @destination_bus_name: The unique bus name for the destination for the signal or %NULL to emit to all listeners.
 * @object_path: Path of remote object.
 * @interface_name: D-Bus interface to emit a signal on.
 * @signal_name: The name of the signal to emit.
 * @parameters: A #GVariant tuple with parameters for the signal or %NULL if not passing parameters.
 * @error: Return location for error or %NULL.
 *
 * Emits a signal.
 *
 * This can only fail if @parameters is not compatible with the D-Bus protocol.
 *
 * Returns: %TRUE unless @error is set.
 *
 * Since: 2.26
 */
gboolean
g_dbus_connection_emit_signal (GDBusConnection  *connection,
                               const gchar      *destination_bus_name,
                               const gchar      *object_path,
                               const gchar      *interface_name,
                               const gchar      *signal_name,
                               GVariant         *parameters,
                               GError          **error)
{
  GDBusMessage *message;
  gboolean ret;

  message = NULL;
  ret = FALSE;

  g_return_val_if_fail (G_IS_DBUS_CONNECTION (connection), FALSE);
  g_return_val_if_fail (destination_bus_name == NULL || g_dbus_is_name (destination_bus_name), FALSE);
  g_return_val_if_fail (object_path != NULL && g_variant_is_object_path (object_path), FALSE);
  g_return_val_if_fail (interface_name != NULL && g_dbus_is_interface_name (interface_name), FALSE);
  g_return_val_if_fail (signal_name != NULL && g_dbus_is_member_name (signal_name), FALSE);
  g_return_val_if_fail (parameters == NULL || g_variant_is_of_type (parameters, G_VARIANT_TYPE_TUPLE), FALSE);

  message = g_dbus_message_new_signal (object_path,
                                       interface_name,
                                       signal_name);

  if (destination_bus_name != NULL)
    g_dbus_message_set_header (message,
                               G_DBUS_MESSAGE_HEADER_FIELD_DESTINATION,
                               g_variant_new_string (destination_bus_name));

  if (parameters != NULL)
    g_dbus_message_set_body (message, parameters);

  ret = g_dbus_connection_send_message (connection, message, NULL, error);
  g_object_unref (message);

  return ret;
}

static void
add_call_flags (GDBusMessage           *message,
                         GDBusCallFlags  flags)
{
  if (flags & G_DBUS_CALL_FLAGS_NO_AUTO_START)
    g_dbus_message_set_flags (message, G_DBUS_MESSAGE_FLAGS_NO_AUTO_START);
}

/**
 * g_dbus_connection_call:
 * @connection: A #GDBusConnection.
 * @bus_name: A unique or well-known bus name or %NULL if @connection is not a message bus connection.
 * @object_path: Path of remote object.
 * @interface_name: D-Bus interface to invoke method on.
 * @method_name: The name of the method to invoke.
 * @parameters: A #GVariant tuple with parameters for the method or %NULL if not passing parameters.
 * @flags: Flags from the #GDBusCallFlags enumeration.
 * @timeout_msec: The timeout in milliseconds or -1 to use the default timeout.
 * @cancellable: A #GCancellable or %NULL.
 * @callback: A #GAsyncReadyCallback to call when the request is satisfied or %NULL if you don't
 * care about the result of the method invocation.
 * @user_data: The data to pass to @callback.
 *
 * Asynchronously invokes the @method_name method on the
 * @interface_name D-Bus interface on the remote object at
 * @object_path owned by @bus_name.
 *
 * If @connection is closed then the operation will fail with
 * %G_IO_ERROR_CLOSED. If @cancellable is canceled, the operation will
 * fail with %G_IO_ERROR_CANCELLED. If @parameters contains a value
 * not compatible with the D-Bus protocol, the operation fails with
 * %G_IO_ERROR_INVALID_ARGUMENT.
 *
 * This is an asynchronous method. When the operation is finished, @callback will be invoked
 * in the <link linkend="g-main-context-push-thread-default">thread-default main loop</link>
 * of the thread you are calling this method from. You can then call
 * g_dbus_connection_call_finish() to get the result of the operation.
 * See g_dbus_connection_call_sync() for the synchronous version of this
 * function.
 *
 * Since: 2.26
 */
void
g_dbus_connection_call (GDBusConnection        *connection,
                        const gchar            *bus_name,
                        const gchar            *object_path,
                        const gchar            *interface_name,
                        const gchar            *method_name,
                        GVariant               *parameters,
                        GDBusCallFlags          flags,
                        gint                    timeout_msec,
                        GCancellable           *cancellable,
                        GAsyncReadyCallback     callback,
                        gpointer                user_data)
{
  GDBusMessage *message;

  g_return_if_fail (G_IS_DBUS_CONNECTION (connection));
  g_return_if_fail (bus_name == NULL || g_dbus_is_name (bus_name));
  g_return_if_fail (object_path != NULL && g_variant_is_object_path (object_path));
  g_return_if_fail (interface_name != NULL && g_dbus_is_interface_name (interface_name));
  g_return_if_fail (method_name != NULL && g_dbus_is_member_name (method_name));
  g_return_if_fail (timeout_msec >= 0 || timeout_msec == -1);
  g_return_if_fail ((parameters == NULL) || g_variant_is_of_type (parameters, G_VARIANT_TYPE_TUPLE));

  message = g_dbus_message_new_method_call (bus_name,
                                            object_path,
                                            interface_name,
                                            method_name);
  add_call_flags (message, flags);
  if (parameters != NULL)
    g_dbus_message_set_body (message, parameters);

  g_dbus_connection_send_message_with_reply (connection,
                                             message,
                                             timeout_msec,
                                             NULL, /* volatile guint32 *out_serial */
                                             cancellable,
                                             callback,
                                             user_data);

  if (message != NULL)
    g_object_unref (message);
}

static GVariant *
decode_method_reply (GDBusMessage  *reply,
                     GError       **error)
{
  GVariant *result;

  result = NULL;
  switch (g_dbus_message_get_message_type (reply))
    {
    case G_DBUS_MESSAGE_TYPE_METHOD_RETURN:
      result = g_dbus_message_get_body (reply);
      if (result == NULL)
        {
          result = g_variant_new ("()");
          g_variant_ref_sink (result);
        }
      else
        {
          g_variant_ref (result);
        }
      break;

    case G_DBUS_MESSAGE_TYPE_ERROR:
      g_dbus_message_to_gerror (reply, error);
      break;

    default:
      g_assert_not_reached ();
      break;
    }

  return result;
}

/**
 * g_dbus_connection_call_finish:
 * @connection: A #GDBusConnection.
 * @res: A #GAsyncResult obtained from the #GAsyncReadyCallback passed to g_dbus_connection_call().
 * @error: Return location for error or %NULL.
 *
 * Finishes an operation started with g_dbus_connection_call().
 *
 * Returns: %NULL if @error is set. Otherwise a #GVariant tuple with
 * return values. Free with g_variant_unref().
 *
 * Since: 2.26
 */
GVariant *
g_dbus_connection_call_finish (GDBusConnection  *connection,
                               GAsyncResult     *res,
                               GError          **error)
{
  GDBusMessage *reply;
  GVariant *result;

  g_return_val_if_fail (G_IS_DBUS_CONNECTION (connection), NULL);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (res), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  result = NULL;

  reply = g_dbus_connection_send_message_with_reply_finish (connection, res, error);
  if (reply == NULL)
    goto out;

  result = decode_method_reply (reply, error);

  g_object_unref (reply);

 out:
  return result;
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_dbus_connection_call_sync:
 * @connection: A #GDBusConnection.
 * @bus_name: A unique or well-known bus name.
 * @object_path: Path of remote object.
 * @interface_name: D-Bus interface to invoke method on.
 * @method_name: The name of the method to invoke.
 * @parameters: A #GVariant tuple with parameters for the method or %NULL if not passing parameters.
 * @flags: Flags from the #GDBusCallFlags enumeration.
 * @timeout_msec: The timeout in milliseconds or -1 to use the default timeout.
 * @cancellable: A #GCancellable or %NULL.
 * @error: Return location for error or %NULL.
 *
 * Synchronously invokes the @method_name method on the
 * @interface_name D-Bus interface on the remote object at
 * @object_path owned by @bus_name.
 *
 * If @connection is closed then the operation will fail with
 * %G_IO_ERROR_CLOSED. If @cancellable is canceled, the
 * operation will fail with %G_IO_ERROR_CANCELLED. If @parameters
 * contains a value not compatible with the D-Bus protocol, the operation
 * fails with %G_IO_ERROR_INVALID_ARGUMENT.
 *
 * The calling thread is blocked until a reply is received. See
 * g_dbus_connection_call() for the asynchronous version of
 * this method.
 *
 * Returns: %NULL if @error is set. Otherwise a #GVariant tuple with
 * return values. Free with g_variant_unref().
 *
 * Since: 2.26
 */
GVariant *
g_dbus_connection_call_sync (GDBusConnection         *connection,
                             const gchar             *bus_name,
                             const gchar             *object_path,
                             const gchar             *interface_name,
                             const gchar             *method_name,
                             GVariant                *parameters,
                             GDBusCallFlags           flags,
                             gint                     timeout_msec,
                             GCancellable            *cancellable,
                             GError                 **error)
{
  GDBusMessage *message;
  GDBusMessage *reply;
  GVariant *result;

  message = NULL;
  reply = NULL;
  result = NULL;

  g_return_val_if_fail (G_IS_DBUS_CONNECTION (connection), NULL);
  g_return_val_if_fail (bus_name == NULL || g_dbus_is_name (bus_name), NULL);
  g_return_val_if_fail (object_path != NULL && g_variant_is_object_path (object_path), NULL);
  g_return_val_if_fail (interface_name != NULL && g_dbus_is_interface_name (interface_name), NULL);
  g_return_val_if_fail (method_name != NULL && g_dbus_is_member_name (method_name), NULL);
  g_return_val_if_fail (timeout_msec >= 0 || timeout_msec == -1, NULL);
  g_return_val_if_fail ((parameters == NULL) || g_variant_is_of_type (parameters, G_VARIANT_TYPE_TUPLE), NULL);

  message = g_dbus_message_new_method_call (bus_name,
                                            object_path,
                                            interface_name,
                                            method_name);
  add_call_flags (message, flags);
  if (parameters != NULL)
    g_dbus_message_set_body (message, parameters);

  reply = g_dbus_connection_send_message_with_reply_sync (connection,
                                                          message,
                                                          timeout_msec,
                                                          NULL, /* volatile guint32 *out_serial */
                                                          cancellable,
                                                          error);

  if (reply == NULL)
    goto out;

  result = decode_method_reply (reply, error);

 out:
  if (message != NULL)
    g_object_unref (message);
  if (reply != NULL)
    g_object_unref (reply);

  return result;
}

/* ---------------------------------------------------------------------------------------------------- */

struct ExportedSubtree
{
  guint                     id;
  gchar                    *object_path;
  GDBusConnection          *connection;
  const GDBusSubtreeVTable *vtable;
  GDBusSubtreeFlags         flags;

  GMainContext             *context;
  gpointer                  user_data;
  GDestroyNotify            user_data_free_func;
};

static void
exported_subtree_free (ExportedSubtree *es)
{
  if (es->user_data_free_func != NULL)
    /* TODO: push to thread-default mainloop */
    es->user_data_free_func (es->user_data);

  if (es->context != NULL)
    g_main_context_unref (es->context);

  g_free (es->object_path);
  g_free (es);
}

/* called without lock held */
static gboolean
handle_subtree_introspect (GDBusConnection *connection,
                           ExportedSubtree *es,
                           GDBusMessage    *message)
{
  GString *s;
  gboolean handled;
  GDBusMessage *reply;
  gchar **children;
  gboolean is_root;
  const gchar *sender;
  const gchar *requested_object_path;
  const gchar *requested_node;
  GPtrArray *interfaces;
  guint n;
  gchar **subnode_paths;

  handled = FALSE;

  requested_object_path = g_dbus_message_get_path (message);
  sender = g_dbus_message_get_sender (message);
  is_root = (g_strcmp0 (requested_object_path, es->object_path) == 0);

  s = g_string_new (NULL);
  introspect_append_header (s);

  /* Strictly we don't need the children in dynamic mode, but we avoid the
   * conditionals to preserve code clarity
   */
  children = es->vtable->enumerate (es->connection,
                                    sender,
                                    es->object_path,
                                    es->user_data);

  if (!is_root)
    {
      requested_node = strrchr (requested_object_path, '/') + 1;

      /* Assert existence of object if we are not dynamic */
      if (!(es->flags & G_DBUS_SUBTREE_FLAGS_DISPATCH_TO_UNENUMERATED_NODES) &&
          !_g_strv_has_string ((const gchar * const *) children, requested_node))
        goto out;
    }
  else
    {
      requested_node = "/";
    }

  interfaces = es->vtable->introspect (es->connection,
                                       sender,
                                       es->object_path,
                                       requested_node,
                                       es->user_data);
  if (interfaces != NULL)
    {
      if (interfaces->len > 0)
        {
          /* we're in business */
          introspect_append_standard_interfaces (s);

          for (n = 0; n < interfaces->len; n++)
            {
              const GDBusInterfaceInfo *interface_info = interfaces->pdata[n];
              g_dbus_interface_info_generate_xml (interface_info, 2, s);
            }
        }
      g_ptr_array_unref (interfaces);
    }

  /* then include <node> entries from the Subtree for the root */
  if (is_root)
    {
      for (n = 0; children != NULL && children[n] != NULL; n++)
        g_string_append_printf (s, "  <node name=\"%s\"/>\n", children[n]);
    }

  /* finally include nodes registered below us */
  subnode_paths = g_dbus_connection_list_registered (es->connection, requested_object_path);
  for (n = 0; subnode_paths != NULL && subnode_paths[n] != NULL; n++)
    g_string_append_printf (s, "  <node name=\"%s\"/>\n", subnode_paths[n]);
  g_strfreev (subnode_paths);

  g_string_append (s, "</node>\n");

  reply = g_dbus_message_new_method_reply (message);
  g_dbus_message_set_body (reply, g_variant_new ("(s)", s->str));
  g_dbus_connection_send_message (connection, reply, NULL, NULL);
  g_object_unref (reply);

  handled = TRUE;

 out:
  g_string_free (s, TRUE);
  g_strfreev (children);
  return handled;
}

/* called without lock held */
static gboolean
handle_subtree_method_invocation (GDBusConnection *connection,
                                  ExportedSubtree *es,
                                  GDBusMessage    *message)
{
  gboolean handled;;
  const gchar *sender;
  const gchar *interface_name;
  const gchar *member;
  const gchar *signature;
  const gchar *requested_object_path;
  const gchar *requested_node;
  gboolean is_root;
  gchar **children;
  const GDBusInterfaceInfo *introspection_data;
  const GDBusInterfaceVTable *interface_vtable;
  gpointer interface_user_data;
  guint n;
  GPtrArray *interfaces;
  gboolean is_property_get;
  gboolean is_property_set;
  gboolean is_property_get_all;

  handled = FALSE;
  interfaces = NULL;

  requested_object_path = g_dbus_message_get_path (message);
  sender = g_dbus_message_get_sender (message);
  interface_name = g_dbus_message_get_interface (message);
  member = g_dbus_message_get_member (message);
  signature = g_dbus_message_get_signature (message);
  is_root = (g_strcmp0 (requested_object_path, es->object_path) == 0);

  is_property_get = FALSE;
  is_property_set = FALSE;
  is_property_get_all = FALSE;
  if (g_strcmp0 (interface_name, "org.freedesktop.DBus.Properties") == 0)
    {
      if (g_strcmp0 (member, "Get") == 0 && g_strcmp0 (signature, "ss") == 0)
        is_property_get = TRUE;
      else if (g_strcmp0 (member, "Set") == 0 && g_strcmp0 (signature, "ssv") == 0)
        is_property_set = TRUE;
      else if (g_strcmp0 (member, "GetAll") == 0 && g_strcmp0 (signature, "s") == 0)
        is_property_get_all = TRUE;
    }

  children = es->vtable->enumerate (es->connection,
                                    sender,
                                    es->object_path,
                                    es->user_data);

  if (!is_root)
    {
      requested_node = strrchr (requested_object_path, '/') + 1;

      /* If not dynamic, skip if requested node is not part of children */
      if (!(es->flags & G_DBUS_SUBTREE_FLAGS_DISPATCH_TO_UNENUMERATED_NODES) &&
          !_g_strv_has_string ((const gchar * const *) children, requested_node))
        goto out;
    }
  else
    {
      requested_node = "/";
    }

  /* get introspection data for the node */
  interfaces = es->vtable->introspect (es->connection,
                                       sender,
                                       requested_object_path,
                                       requested_node,
                                       es->user_data);
  g_assert (interfaces != NULL);
  introspection_data = NULL;
  for (n = 0; n < interfaces->len; n++)
    {
      const GDBusInterfaceInfo *id_n = (const GDBusInterfaceInfo *) interfaces->pdata[n];
      if (g_strcmp0 (id_n->name, interface_name) == 0)
        introspection_data = id_n;
    }

  /* dispatch the call if the user wants to handle it */
  if (introspection_data != NULL)
    {
      /* figure out where to dispatch the method call */
      interface_user_data = NULL;
      interface_vtable = es->vtable->dispatch (es->connection,
                                               sender,
                                               es->object_path,
                                               interface_name,
                                               requested_node,
                                               &interface_user_data,
                                               es->user_data);
      if (interface_vtable == NULL)
        goto out;

      CONNECTION_LOCK (connection);
      handled = validate_and_maybe_schedule_method_call (es->connection,
                                                         message,
                                                         introspection_data,
                                                         interface_vtable,
                                                         es->context,
                                                         interface_user_data);
      CONNECTION_UNLOCK (connection);
    }
  /* handle org.freedesktop.DBus.Properties interface if not explicitly handled */
  else if (is_property_get || is_property_set || is_property_get_all)
    {
      if (is_property_get)
        g_variant_get (g_dbus_message_get_body (message), "(ss)", &interface_name, NULL);
      else if (is_property_set)
        g_variant_get (g_dbus_message_get_body (message), "(ssv)", &interface_name, NULL, NULL);
      else if (is_property_get_all)
        g_variant_get (g_dbus_message_get_body (message), "(s)", &interface_name, NULL, NULL);
      else
        g_assert_not_reached ();

      /* see if the object supports this interface at all */
      for (n = 0; n < interfaces->len; n++)
        {
          const GDBusInterfaceInfo *id_n = (const GDBusInterfaceInfo *) interfaces->pdata[n];
          if (g_strcmp0 (id_n->name, interface_name) == 0)
            introspection_data = id_n;
        }

      /* Fail with org.freedesktop.DBus.Error.InvalidArgs if the user-code
       * claims it won't support the interface
       */
      if (introspection_data == NULL)
        {
          GDBusMessage *reply;
          reply = g_dbus_message_new_method_error (message,
                                                   "org.freedesktop.DBus.Error.InvalidArgs",
                                                   _("No such interface `%s'"),
                                                   interface_name);
          g_dbus_connection_send_message (es->connection, reply, NULL, NULL);
          g_object_unref (reply);
          handled = TRUE;
          goto out;
        }

      /* figure out where to dispatch the property get/set/getall calls */
      interface_user_data = NULL;
      interface_vtable = es->vtable->dispatch (es->connection,
                                               sender,
                                               es->object_path,
                                               interface_name,
                                               requested_node,
                                               &interface_user_data,
                                               es->user_data);
      if (interface_vtable == NULL)
        goto out;

      if (is_property_get || is_property_set)
        {
          CONNECTION_LOCK (connection);
          handled = validate_and_maybe_schedule_property_getset (es->connection,
                                                                 message,
                                                                 is_property_get,
                                                                 introspection_data,
                                                                 interface_vtable,
                                                                 es->context,
                                                                 interface_user_data);
          CONNECTION_UNLOCK (connection);
        }
      else if (is_property_get_all)
        {
          CONNECTION_LOCK (connection);
          handled = validate_and_maybe_schedule_property_get_all (es->connection,
                                                                  message,
                                                                  introspection_data,
                                                                  interface_vtable,
                                                                  es->context,
                                                                  interface_user_data);
          CONNECTION_UNLOCK (connection);
        }
    }

 out:
  if (interfaces != NULL)
    g_ptr_array_unref (interfaces);
  g_strfreev (children);
  return handled;
}

typedef struct
{
  GDBusMessage *message;
  ExportedSubtree *es;
} SubtreeDeferredData;

static void
subtree_deferred_data_free (SubtreeDeferredData *data)
{
  g_object_unref (data->message);
  g_free (data);
}

/* called without lock held in the thread where the caller registered the subtree */
static gboolean
process_subtree_vtable_message_in_idle_cb (gpointer _data)
{
  SubtreeDeferredData *data = _data;
  gboolean handled;

  handled = FALSE;

  if (g_strcmp0 (g_dbus_message_get_interface (data->message), "org.freedesktop.DBus.Introspectable") == 0 &&
      g_strcmp0 (g_dbus_message_get_member (data->message), "Introspect") == 0 &&
      g_strcmp0 (g_dbus_message_get_signature (data->message), "") == 0)
    handled = handle_subtree_introspect (data->es->connection,
                                         data->es,
                                         data->message);
  else
    handled = handle_subtree_method_invocation (data->es->connection,
                                                data->es,
                                                data->message);

  if (!handled)
    {
      CONNECTION_LOCK (data->es->connection);
      handled = handle_generic_unlocked (data->es->connection, data->message);
      CONNECTION_UNLOCK (data->es->connection);
    }

  /* if we couldn't handle the request, just bail with the UnknownMethod error */
  if (!handled)
    {
      GDBusMessage *reply;
      reply = g_dbus_message_new_method_error (data->message,
                                               "org.freedesktop.DBus.Error.UnknownMethod",
                                               _("Method `%s' on interface `%s' with signature `%s' does not exist"),
                                               g_dbus_message_get_member (data->message),
                                               g_dbus_message_get_interface (data->message),
                                               g_dbus_message_get_signature (data->message));
      g_dbus_connection_send_message (data->es->connection, reply, NULL, NULL);
      g_object_unref (reply);
    }

  return FALSE;
}

/* called in message handler thread with lock held */
static gboolean
subtree_message_func (GDBusConnection *connection,
                      ExportedSubtree *es,
                      GDBusMessage    *message)
{
  GSource *idle_source;
  SubtreeDeferredData *data;

  data = g_new0 (SubtreeDeferredData, 1);
  data->message = g_object_ref (message);
  data->es = es;

  /* defer this call to an idle handler in the right thread */
  idle_source = g_idle_source_new ();
  g_source_set_priority (idle_source, G_PRIORITY_HIGH);
  g_source_set_callback (idle_source,
                         process_subtree_vtable_message_in_idle_cb,
                         data,
                         (GDestroyNotify) subtree_deferred_data_free);
  g_source_attach (idle_source, es->context);
  g_source_unref (idle_source);

  /* since we own the entire subtree, handlers for objects not in the subtree have been
   * tried already by libdbus-1 - so we just need to ensure that we're always going
   * to reply to the message
   */
  return TRUE;
}

/**
 * g_dbus_connection_register_subtree:
 * @connection: A #GDBusConnection.
 * @object_path: The object path to register the subtree at.
 * @vtable: A #GDBusSubtreeVTable to enumerate, introspect and dispatch nodes in the subtree.
 * @flags: Flags used to fine tune the behavior of the subtree.
 * @user_data: Data to pass to functions in @vtable.
 * @user_data_free_func: Function to call when the subtree is unregistered.
 * @error: Return location for error or %NULL.
 *
 * Registers a whole subtree of <quote>dynamic</quote> objects.
 *
 * The @enumerate and @introspection functions in @vtable are used to
 * convey, to remote callers, what nodes exist in the subtree rooted
 * by @object_path.
 *
 * When handling remote calls into any node in the subtree, first the
 * @enumerate function is used to check if the node exists. If the node exists
 * or the #G_DBUS_SUBTREE_FLAGS_DISPATCH_TO_UNENUMERATED_NODES flag is set
 * the @introspection function is used to check if the node supports the
 * requested method. If so, the @dispatch function is used to determine
 * where to dispatch the call. The collected #GDBusInterfaceVTable and
 * #gpointer will be used to call into the interface vtable for processing
 * the request.
 *
 * All calls into user-provided code will be invoked in the <link
 * linkend="g-main-context-push-thread-default">thread-default main
 * loop</link> of the thread you are calling this method from.
 *
 * If an existing subtree is already registered at @object_path or
 * then @error is set to #G_IO_ERROR_EXISTS.
 *
 * Note that it is valid to register regular objects (using
 * g_dbus_connection_register_object()) in a subtree registered with
 * g_dbus_connection_register_subtree() - if so, the subtree handler
 * is tried as the last resort. One way to think about a subtree
 * handler is to consider it a <quote>fallback handler</quote>
 * for object paths not registered via g_dbus_connection_register_object()
 * or other bindings.
 *
 * See <xref linkend="gdbus-subtree-server"/> for an example of how to use this method.
 *
 * Returns: 0 if @error is set, otherwise a subtree registration id (never 0)
 * that can be used with g_dbus_connection_unregister_subtree() .
 *
 * Since: 2.26
 */
guint
g_dbus_connection_register_subtree (GDBusConnection           *connection,
                                    const gchar               *object_path,
                                    const GDBusSubtreeVTable  *vtable,
                                    GDBusSubtreeFlags          flags,
                                    gpointer                   user_data,
                                    GDestroyNotify             user_data_free_func,
                                    GError                   **error)
{
  guint ret;
  ExportedSubtree *es;

  g_return_val_if_fail (G_IS_DBUS_CONNECTION (connection), 0);
  g_return_val_if_fail (object_path != NULL && g_variant_is_object_path (object_path), 0);
  g_return_val_if_fail (vtable != NULL, 0);
  g_return_val_if_fail (error == NULL || *error == NULL, 0);

  ret = 0;

  CONNECTION_LOCK (connection);

  es = g_hash_table_lookup (connection->priv->map_object_path_to_es, object_path);
  if (es != NULL)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_EXISTS,
                   _("A subtree is already exported for %s"),
                   object_path);
      goto out;
    }

  es = g_new0 (ExportedSubtree, 1);
  es->object_path = g_strdup (object_path);
  es->connection = connection;

  es->vtable = vtable;
  es->flags = flags;
  es->id = _global_subtree_registration_id++; /* TODO: overflow etc. */
  es->user_data = user_data;
  es->user_data_free_func = user_data_free_func;
  es->context = g_main_context_get_thread_default ();
  if (es->context != NULL)
    g_main_context_ref (es->context);

  g_hash_table_insert (connection->priv->map_object_path_to_es, es->object_path, es);
  g_hash_table_insert (connection->priv->map_id_to_es,
                       GUINT_TO_POINTER (es->id),
                       es);

  ret = es->id;

 out:
  CONNECTION_UNLOCK (connection);

  return ret;
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_dbus_connection_unregister_subtree:
 * @connection: A #GDBusConnection.
 * @registration_id: A subtree registration id obtained from g_dbus_connection_register_subtree().
 *
 * Unregisters a subtree.
 *
 * Returns: %TRUE if the subtree was unregistered, %FALSE otherwise.
 *
 * Since: 2.26
 */
gboolean
g_dbus_connection_unregister_subtree (GDBusConnection *connection,
                                      guint            registration_id)
{
  ExportedSubtree *es;
  gboolean ret;

  g_return_val_if_fail (G_IS_DBUS_CONNECTION (connection), FALSE);

  ret = FALSE;

  CONNECTION_LOCK (connection);

  es = g_hash_table_lookup (connection->priv->map_id_to_es,
                            GUINT_TO_POINTER (registration_id));
  if (es == NULL)
    goto out;

  g_warn_if_fail (g_hash_table_remove (connection->priv->map_id_to_es, GUINT_TO_POINTER (es->id)));
  g_warn_if_fail (g_hash_table_remove (connection->priv->map_object_path_to_es, es->object_path));

  ret = TRUE;

 out:
  CONNECTION_UNLOCK (connection);

  return ret;
}

/* ---------------------------------------------------------------------------------------------------- */

/* must be called with lock held */
static void
handle_generic_ping_unlocked (GDBusConnection *connection,
                              const gchar     *object_path,
                              GDBusMessage    *message)
{
  GDBusMessage *reply;
  reply = g_dbus_message_new_method_reply (message);
  g_dbus_connection_send_message_unlocked (connection, reply, NULL, NULL);
  g_object_unref (reply);
}

/* must be called with lock held */
static void
handle_generic_get_machine_id_unlocked (GDBusConnection *connection,
                                        const gchar     *object_path,
                                        GDBusMessage    *message)
{
  GDBusMessage *reply;

  reply = NULL;
  if (connection->priv->machine_id == NULL)
    {
      GError *error;
      error = NULL;
      /* TODO: use PACKAGE_LOCALSTATEDIR ? */
      if (!g_file_get_contents ("/var/lib/dbus/machine-id",
                                &connection->priv->machine_id,
                                NULL,
                                &error))
        {
          reply = g_dbus_message_new_method_error (message,
                                                   "org.freedesktop.DBus.Error.Failed",
                                                   _("Unable to load /var/lib/dbus/machine-id: %s"),
                                                   error->message);
          g_error_free (error);
        }
      else
        {
          g_strstrip (connection->priv->machine_id);
          /* TODO: validate value */
        }
    }

  if (reply == NULL)
    {
      reply = g_dbus_message_new_method_reply (message);
      g_dbus_message_set_body (reply, g_variant_new ("(s)", connection->priv->machine_id));
    }
  g_dbus_connection_send_message_unlocked (connection, reply, NULL, NULL);
  g_object_unref (reply);
}

/* must be called with lock held */
static void
handle_generic_introspect_unlocked (GDBusConnection *connection,
                                    const gchar     *object_path,
                                    GDBusMessage    *message)
{
  guint n;
  GString *s;
  gchar **registered;
  GDBusMessage *reply;

  /* first the header */
  s = g_string_new (NULL);
  introspect_append_header (s);

  registered = g_dbus_connection_list_registered_unlocked (connection, object_path);
  for (n = 0; registered != NULL && registered[n] != NULL; n++)
      g_string_append_printf (s, "  <node name=\"%s\"/>\n", registered[n]);
  g_strfreev (registered);
  g_string_append (s, "</node>\n");

  reply = g_dbus_message_new_method_reply (message);
  g_dbus_message_set_body (reply, g_variant_new ("(s)", s->str));
  g_dbus_connection_send_message_unlocked (connection, reply, NULL, NULL);
  g_object_unref (reply);
  g_string_free (s, TRUE);
}

/* must be called with lock held */
static gboolean
handle_generic_unlocked (GDBusConnection *connection,
                         GDBusMessage    *message)
{
  gboolean handled;
  const gchar *interface_name;
  const gchar *member;
  const gchar *signature;
  const gchar *path;

  CONNECTION_ENSURE_LOCK (connection);

  handled = FALSE;

  interface_name = g_dbus_message_get_interface (message);
  member = g_dbus_message_get_member (message);
  signature = g_dbus_message_get_signature (message);
  path = g_dbus_message_get_path (message);

  if (g_strcmp0 (interface_name, "org.freedesktop.DBus.Introspectable") == 0 &&
      g_strcmp0 (member, "Introspect") == 0 &&
      g_strcmp0 (signature, "") == 0)
    {
      handle_generic_introspect_unlocked (connection, path, message);
      handled = TRUE;
    }
  else if (g_strcmp0 (interface_name, "org.freedesktop.DBus.Peer") == 0 &&
           g_strcmp0 (member, "Ping") == 0 &&
           g_strcmp0 (signature, "") == 0)
    {
      handle_generic_ping_unlocked (connection, path, message);
      handled = TRUE;
    }
  else if (g_strcmp0 (interface_name, "org.freedesktop.DBus.Peer") == 0 &&
           g_strcmp0 (member, "GetMachineId") == 0 &&
           g_strcmp0 (signature, "") == 0)
    {
      handle_generic_get_machine_id_unlocked (connection, path, message);
      handled = TRUE;
    }

  return handled;
}

/* ---------------------------------------------------------------------------------------------------- */

/* called in message handler thread with lock held */
static void
distribute_method_call (GDBusConnection *connection,
                        GDBusMessage    *message)
{
  ExportedObject *eo;
  ExportedSubtree *es;
  const gchar *object_path;
  const gchar *interface_name;
  const gchar *member;
  const gchar *signature;
  const gchar *path;
  gchar *subtree_path;
  gchar *needle;

  g_assert (g_dbus_message_get_message_type (message) == G_DBUS_MESSAGE_TYPE_METHOD_CALL);

  interface_name = g_dbus_message_get_interface (message);
  member = g_dbus_message_get_member (message);
  signature = g_dbus_message_get_signature (message);
  path = g_dbus_message_get_path (message);
  subtree_path = g_strdup (path);
  needle = strrchr (subtree_path, '/');
  if (needle != NULL && needle != subtree_path)
    {
      *needle = '\0';
    }
  else
    {
      g_free (subtree_path);
      subtree_path = NULL;
    }

#if 0
  g_debug ("interface    = `%s'", interface_name);
  g_debug ("member       = `%s'", member);
  g_debug ("signature    = `%s'", signature);
  g_debug ("path         = `%s'", path);
  g_debug ("subtree_path = `%s'", subtree_path != NULL ? subtree_path : "N/A");
#endif

  object_path = g_dbus_message_get_path (message);
  g_assert (object_path != NULL);

  eo = g_hash_table_lookup (connection->priv->map_object_path_to_eo, object_path);
  if (eo != NULL)
    {
      if (obj_message_func (connection, eo, message))
        goto out;
    }

  es = g_hash_table_lookup (connection->priv->map_object_path_to_es, object_path);
  if (es != NULL)
    {
      if (subtree_message_func (connection, es, message))
        goto out;
    }

  if (subtree_path != NULL)
    {
      es = g_hash_table_lookup (connection->priv->map_object_path_to_es, subtree_path);
      if (es != NULL)
        {
          if (subtree_message_func (connection, es, message))
            goto out;
        }
    }

  if (handle_generic_unlocked (connection, message))
    goto out;

  /* if we end up here, the message has not been not handled */

 out:
  g_free (subtree_path);
}

/* ---------------------------------------------------------------------------------------------------- */

static GDBusConnection **
message_bus_get_singleton (GBusType   bus_type,
                           GError   **error)
{
  GDBusConnection **ret;
  const gchar *starter_bus;

  ret = NULL;

  switch (bus_type)
    {
    case G_BUS_TYPE_SESSION:
      ret = &the_session_bus;
      break;

    case G_BUS_TYPE_SYSTEM:
      ret = &the_system_bus;
      break;

    case G_BUS_TYPE_STARTER:
      starter_bus = g_getenv ("DBUS_STARTER_BUS_TYPE");
      if (g_strcmp0 (starter_bus, "session") == 0)
        {
          ret = message_bus_get_singleton (G_BUS_TYPE_SESSION, error);
          goto out;
        }
      else if (g_strcmp0 (starter_bus, "system") == 0)
        {
          ret = message_bus_get_singleton (G_BUS_TYPE_SYSTEM, error);
          goto out;
        }
      else
        {
          if (starter_bus != NULL)
            {
              g_set_error (error,
                           G_IO_ERROR,
                           G_IO_ERROR_INVALID_ARGUMENT,
                           _("Cannot determine bus address from DBUS_STARTER_BUS_TYPE environment variable"
                             " - unknown value `%s'"),
                           starter_bus);
            }
          else
            {
              g_set_error_literal (error,
                                   G_IO_ERROR,
                                   G_IO_ERROR_INVALID_ARGUMENT,
                                   _("Cannot determine bus address because the DBUS_STARTER_BUS_TYPE environment "
                                     "variable is not set"));
            }
        }
      break;

    default:
      g_assert_not_reached ();
      break;
    }

 out:
  return ret;
}

static GDBusConnection *
get_uninitialized_connection (GBusType       bus_type,
                              GCancellable  *cancellable,
                              GError       **error)
{
  GDBusConnection **singleton;
  GDBusConnection *ret;

  ret = NULL;

  G_LOCK (message_bus_lock);
  singleton = message_bus_get_singleton (bus_type, error);
  if (singleton == NULL)
    goto out;

  if (*singleton == NULL)
    {
      gchar *address;
      address = g_dbus_address_get_for_bus_sync (bus_type, cancellable, error);
      if (address == NULL)
        goto out;
      ret = *singleton = g_object_new (G_TYPE_DBUS_CONNECTION,
                                       "address", address,
                                       "flags", G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT |
                                                G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION,
                                       "exit-on-close", TRUE,
                                       NULL);
    }
  else
    {
      ret = g_object_ref (*singleton);
    }

  g_assert (ret != NULL);

 out:
  G_UNLOCK (message_bus_lock);
  return ret;
}

/**
 * g_bus_get_sync:
 * @bus_type: A #GBusType.
 * @cancellable: A #GCancellable or %NULL.
 * @error: Return location for error or %NULL.
 *
 * Synchronously connects to the message bus specified by @bus_type.
 * Note that the returned object may shared with other callers,
 * e.g. if two separate parts of a process calls this function with
 * the same @bus_type, they will share the same object.
 *
 * This is a synchronous failable function. See g_bus_get() and
 * g_bus_get_finish() for the asynchronous version.
 *
 * The returned object is a singleton, that is, shared with other
 * callers of g_bus_get() and g_bus_get_sync() for @bus_type. In the
 * event that you need a private message bus connection, use
 * g_dbus_address_get_for_bus_sync() and
 * g_dbus_connection_new_for_address().
 *
 * Note that the returned #GDBusConnection object will (usually) have
 * the #GDBusConnection:exit-on-close property set to %TRUE.
 *
 * Returns: A #GDBusConnection or %NULL if @error is set. Free with g_object_unref().
 *
 * Since: 2.26
 */
GDBusConnection *
g_bus_get_sync (GBusType       bus_type,
                GCancellable  *cancellable,
                GError       **error)
{
  GDBusConnection *connection;

  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  connection = get_uninitialized_connection (bus_type, cancellable, error);
  if (connection == NULL)
    goto out;

  if (!g_initable_init (G_INITABLE (connection), cancellable, error))
    {
      g_object_unref (connection);
      connection = NULL;
    }

 out:
  return connection;
}

static void
bus_get_async_initable_cb (GObject      *source_object,
                           GAsyncResult *res,
                           gpointer      user_data)
{
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (user_data);
  GError *error;

  error = NULL;
  if (!g_async_initable_init_finish (G_ASYNC_INITABLE (source_object),
                                     res,
                                     &error))
    {
      g_assert (error != NULL);
      g_simple_async_result_set_from_error (simple, error);
      g_error_free (error);
      g_object_unref (source_object);
    }
  else
    {
      g_simple_async_result_set_op_res_gpointer (simple,
                                                 source_object,
                                                 g_object_unref);
    }
  g_simple_async_result_complete_in_idle (simple);
  g_object_unref (simple);
}

/**
 * g_bus_get:
 * @bus_type: A #GBusType.
 * @cancellable: A #GCancellable or %NULL.
 * @callback: A #GAsyncReadyCallback to call when the request is satisfied.
 * @user_data: The data to pass to @callback.
 *
 * Asynchronously connects to the message bus specified by @bus_type.
 *
 * When the operation is finished, @callback will be invoked. You can
 * then call g_bus_get_finish() to get the result of the operation.
 *
 * This is a asynchronous failable function. See g_bus_get_sync() for
 * the synchronous version.
 *
 * Since: 2.26
 */
void
g_bus_get (GBusType             bus_type,
           GCancellable        *cancellable,
           GAsyncReadyCallback  callback,
           gpointer             user_data)
{
  GDBusConnection *connection;
  GSimpleAsyncResult *simple;
  GError *error;

  simple = g_simple_async_result_new (NULL,
                                      callback,
                                      user_data,
                                      g_bus_get);

  error = NULL;
  connection = get_uninitialized_connection (bus_type, cancellable, &error);
  if (connection == NULL)
    {
      g_assert (error != NULL);
      g_simple_async_result_set_from_error (simple, error);
      g_error_free (error);
      g_simple_async_result_complete_in_idle (simple);
      g_object_unref (simple);
    }
  else
    {
      g_async_initable_init_async (G_ASYNC_INITABLE (connection),
                                   G_PRIORITY_DEFAULT,
                                   cancellable,
                                   bus_get_async_initable_cb,
                                   simple);
    }
}

/**
 * g_bus_get_finish:
 * @res: A #GAsyncResult obtained from the #GAsyncReadyCallback passed to g_bus_get().
 * @error: Return location for error or %NULL.
 *
 * Finishes an operation started with g_bus_get().
 *
 * The returned object is a singleton, that is, shared with other
 * callers of g_bus_get() and g_bus_get_sync() for @bus_type. In the
 * event that you need a private message bus connection, use
 * g_dbus_address_get_for_bus() and
 * g_dbus_connection_new_for_address().
 *
 * Note that the returned #GDBusConnection object will (usually) have
 * the #GDBusConnection:exit-on-close property set to %TRUE.
 *
 * Returns: A #GDBusConnection or %NULL if @error is set. Free with g_object_unref().
 *
 * Since: 2.26
 */
GDBusConnection *
g_bus_get_finish (GAsyncResult  *res,
                  GError       **error)
{
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (res);
  GObject *object;
  GDBusConnection *ret;

  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  g_warn_if_fail (g_simple_async_result_get_source_tag (simple) == g_bus_get);

  ret = NULL;

  if (g_simple_async_result_propagate_error (simple, error))
    goto out;

  object = g_simple_async_result_get_op_res_gpointer (simple);
  g_assert (object != NULL);
  ret = g_object_ref (G_DBUS_CONNECTION (object));

 out:
  return ret;
}

/* ---------------------------------------------------------------------------------------------------- */

#define __G_DBUS_CONNECTION_C__
#include "gioaliasdef.c"
