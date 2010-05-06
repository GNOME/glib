/* GDBus - GLib D-Bus Library
 *
 * Copyright (C) 2008-2009 Red Hat, Inc.
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

#include <stdlib.h>

#include <glib/gi18n.h>

#include "gdbusutils.h"
#include "gdbusnamewatching.h"
#include "gdbusproxywatching.h"
#include "gdbuserror.h"
#include "gdbusprivate.h"
#include "gdbusproxy.h"
#include "gdbusnamewatching.h"
#include "gcancellable.h"

/**
 * SECTION:gdbusproxywatching
 * @title: Watching Proxies
 * @short_description: Simple API for watching proxies
 * @include: gio/gio.h
 *
 * Convenience API for watching bus proxies.
 *
 * <example id="gdbus-watching-proxy"><title>Simple application watching a proxy</title><programlisting><xi:include xmlns:xi="http://www.w3.org/2001/XInclude" parse="text" href="../../../../gio/tests/gdbus-example-watch-proxy.c"><xi:fallback>FIXME: MISSING XINCLUDE CONTENT</xi:fallback></xi:include></programlisting></example>
 */

/* ---------------------------------------------------------------------------------------------------- */

G_LOCK_DEFINE_STATIC (lock);

static guint next_global_id = 1;
static GHashTable *map_id_to_client = NULL;

/* ---------------------------------------------------------------------------------------------------- */

typedef struct
{
  guint                      id;
  GBusProxyAppearedCallback  proxy_appeared_handler;
  GBusProxyVanishedCallback  proxy_vanished_handler;
  gpointer                   user_data;
  GDestroyNotify             user_data_free_func;
  GMainContext              *main_context;

  gchar                     *name;
  gchar                     *name_owner;
  GDBusConnection           *connection;
  guint                      name_watcher_id;

  GCancellable              *cancellable;

  gchar                     *object_path;
  gchar                     *interface_name;
  GType                      interface_type;
  GDBusProxyFlags            proxy_flags;
  GDBusProxy                *proxy;

  gboolean initial_construction;
} Client;

static void
client_unref (Client *client)
{
  /* ensure we're only called from g_bus_unwatch_proxy */
  g_assert (client->name_watcher_id == 0);

  g_free (client->name_owner);
  if (client->connection != NULL)
    g_object_unref (client->connection);
  if (client->proxy != NULL)
    g_object_unref (client->proxy);

  g_free (client->name);
  g_free (client->object_path);
  g_free (client->interface_name);

  if (client->main_context != NULL)
    g_main_context_unref (client->main_context);

  if (client->user_data_free_func != NULL)
    client->user_data_free_func (client->user_data);
  g_free (client);
}

/* ---------------------------------------------------------------------------------------------------- */

static void
proxy_constructed_cb (GObject       *source_object,
                      GAsyncResult  *res,
                      gpointer       user_data)
{
  Client *client = user_data;
  GDBusProxy *proxy;
  GError *error;

  error = NULL;
  proxy = g_dbus_proxy_new_finish (res, &error);
  if (proxy == NULL)
    {
      /* g_warning ("error while constructing proxy: %s", error->message); */
      g_error_free (error);

      /* handle initial construction, send out vanished if the name
       * is there but we constructing a proxy fails
       */
      if (client->initial_construction)
        {
          if (client->proxy_vanished_handler != NULL)
            {
              client->proxy_vanished_handler (client->connection,
                                              client->name,
                                              client->user_data);
            }
          client->initial_construction = FALSE;
        }
    }
  else
    {
      g_assert (client->proxy == NULL);
      g_assert (client->cancellable != NULL);
      client->proxy = G_DBUS_PROXY (proxy);

      g_object_unref (client->cancellable);
      client->cancellable = NULL;

      /* perform callback */
      if (client->proxy_appeared_handler != NULL)
        {
          client->proxy_appeared_handler (client->connection,
                                          client->name,
                                          client->name_owner,
                                          client->proxy,
                                          client->user_data);
        }
      client->initial_construction = FALSE;
    }
}

static void
on_name_appeared (GDBusConnection *connection,
                  const gchar *name,
                  const gchar *name_owner,
                  gpointer user_data)
{
  Client *client = user_data;

  //g_debug ("\n\nname appeared (owner `%s')", name_owner);

  /* invariants */
  g_assert (client->name_owner == NULL);
  g_assert (client->connection == NULL);
  g_assert (client->cancellable == NULL);

  client->name_owner = g_strdup (name_owner);
  client->connection = g_object_ref (connection);
  client->cancellable = g_cancellable_new ();

  g_dbus_proxy_new (client->connection,
                    client->interface_type,
                    client->proxy_flags,
                    NULL, /* GDBusInterfaceInfo */
                    client->name_owner,
                    client->object_path,
                    client->interface_name,
                    client->cancellable,
                    proxy_constructed_cb,
                    client);
}

static void
on_name_vanished (GDBusConnection *connection,
                  const gchar *name,
                  gpointer user_data)
{
  Client *client = user_data;

  /*g_debug ("\n\nname vanished");*/

  g_free (client->name_owner);
  if (client->connection != NULL)
    g_object_unref (client->connection);
  client->name_owner = NULL;
  client->connection = NULL;

  /* free the proxy if we have it */
  if (client->proxy != NULL)
    {
      g_assert (client->cancellable == NULL);

      g_object_unref (client->proxy);
      client->proxy = NULL;

      /* if we have the proxy, it means we last sent out a 'appeared'
       * callback - so send out a 'vanished' callback
       */
      if (client->proxy_vanished_handler != NULL)
        {
          client->proxy_vanished_handler (client->connection,
                                          client->name,
                                          client->user_data);
        }
      client->initial_construction = FALSE;
    }
  else
    {
      /* otherwise cancel construction of the proxy if applicable */
      if (client->cancellable != NULL)
        {
          g_cancellable_cancel (client->cancellable);
          g_object_unref (client->cancellable);
          client->cancellable = NULL;
        }
      else
        {
          /* handle initial construction, send out vanished if
           * the name isn't there
           */
          if (client->initial_construction)
            {
              if (client->proxy_vanished_handler != NULL)
                {
                  client->proxy_vanished_handler (client->connection,
                                                  client->name,
                                                  client->user_data);
                }
              client->initial_construction = FALSE;
            }
        }
    }
}

/**
 * g_bus_watch_proxy:
 * @bus_type: The type of bus to watch a name on (can't be #G_BUS_TYPE_NONE).
 * @name: The name (well-known or unique) to watch.
 * @flags: Flags from the #GBusNameWatcherFlags enumeration.
 * @object_path: The object path of the remote object to watch.
 * @interface_name: The D-Bus interface name for the proxy.
 * @interface_type: The #GType for the kind of proxy to create. This must be a #GDBusProxy derived type.
 * @proxy_flags: Flags from #GDBusProxyFlags to use when constructing the proxy.
 * @proxy_appeared_handler: Handler to invoke when @name is known to exist and the
 * requested proxy is available.
 * @proxy_vanished_handler: Handler to invoke when @name is known to not exist
 * and the previously created proxy is no longer available.
 * @user_data: User data to pass to handlers.
 * @user_data_free_func: Function for freeing @user_data or %NULL.
 *
 * Starts watching a remote object at @object_path owned by @name on
 * the bus specified by @bus_type. When the object is available, a
 * #GDBusProxy (or derived class cf. @interface_type) instance is
 * constructed for the @interface_name D-Bus interface and then
 * @proxy_appeared_handler will be called when the proxy is ready and
 * all properties have been loaded. When @name vanishes,
 * @proxy_vanished_handler is called.
 *
 * This function makes it very simple to write applications that wants
 * to watch a well-known remote object on a well-known name, see <xref
 * linkend="gdbus-watching-proxy"/>. Basically, the application simply
 * starts using the proxy when @proxy_appeared_handler is called and
 * stops using it when @proxy_vanished_handler is called. Callbacks
 * will be invoked in the <link
 * linkend="g-main-context-push-thread-default">thread-default main
 * loop</link> of the thread you are calling this function from.
 *
 * Applications typically use this function to watch the
 * <quote>manager</quote> object of a well-known name. Upon acquiring
 * a proxy for the manager object, applications typically construct
 * additional proxies in response to the result of enumeration methods
 * on the manager object.
 *
 * Many of the comment that applies to g_bus_watch_name() also applies
 * here. For example, you are guaranteed that one of the handlers will
 * be invoked (on the main thread) after calling this function and
 * also that the two handlers alternate. When you are done watching the
 * proxy, just call g_bus_unwatch_proxy().
 *
 * Returns: An identifier (never 0) that can be used with
 * g_bus_unwatch_proxy() to stop watching the remote object.
 *
 * Since: 2.26
 */
guint
g_bus_watch_proxy (GBusType                   bus_type,
                   const gchar               *name,
                   GBusNameWatcherFlags       flags,
                   const gchar               *object_path,
                   const gchar               *interface_name,
                   GType                      interface_type,
                   GDBusProxyFlags            proxy_flags,
                   GBusProxyAppearedCallback  proxy_appeared_handler,
                   GBusProxyVanishedCallback  proxy_vanished_handler,
                   gpointer                   user_data,
                   GDestroyNotify             user_data_free_func)
{
  Client *client;

  g_return_val_if_fail (bus_type != G_BUS_TYPE_NONE, 0);
  g_return_val_if_fail (g_dbus_is_name (name), 0);
  g_return_val_if_fail (g_variant_is_object_path (object_path), 0);
  g_return_val_if_fail (g_dbus_is_interface_name (interface_name), 0);
  g_return_val_if_fail (g_type_is_a (interface_type, G_TYPE_DBUS_PROXY), 0);

  G_LOCK (lock);

  client = g_new0 (Client, 1);
  client->id = next_global_id++; /* TODO: uh oh, handle overflow */
  client->name = g_strdup (name);
  client->proxy_appeared_handler = proxy_appeared_handler;
  client->proxy_vanished_handler = proxy_vanished_handler;
  client->user_data = user_data;
  client->user_data_free_func = user_data_free_func;
  client->main_context = g_main_context_get_thread_default ();
  if (client->main_context != NULL)
    g_main_context_ref (client->main_context);
  client->name_watcher_id = g_bus_watch_name (bus_type,
                                              name,
                                              flags,
                                              on_name_appeared,
                                              on_name_vanished,
                                              client,
                                              NULL);

  client->object_path = g_strdup (object_path);
  client->interface_name = g_strdup (interface_name);
  client->interface_type = interface_type;
  client->proxy_flags = proxy_flags;
  client->initial_construction = TRUE;

  if (map_id_to_client == NULL)
    {
      map_id_to_client = g_hash_table_new (g_direct_hash, g_direct_equal);
    }
  g_hash_table_insert (map_id_to_client,
                       GUINT_TO_POINTER (client->id),
                       client);

  G_UNLOCK (lock);

  return client->id;
}

/**
 * g_bus_unwatch_proxy:
 * @watcher_id: An identifier obtained from g_bus_watch_proxy()
 *
 * Stops watching proxy.
 *
 * Since: 2.26
 */
void
g_bus_unwatch_proxy (guint watcher_id)
{
  Client *client;

  g_return_if_fail (watcher_id > 0);

  client = NULL;

  G_LOCK (lock);
  if (watcher_id == 0 ||
      map_id_to_client == NULL ||
      (client = g_hash_table_lookup (map_id_to_client, GUINT_TO_POINTER (watcher_id))) == NULL)
    {
      g_warning ("Invalid id %d passed to g_bus_unwatch_proxy()", watcher_id);
      goto out;
    }

  g_warn_if_fail (g_hash_table_remove (map_id_to_client, GUINT_TO_POINTER (watcher_id)));

 out:
  G_UNLOCK (lock);

  if (client != NULL)
    {
      g_bus_unwatch_name (client->name_watcher_id);
      client->name_watcher_id = 0;
      client_unref (client);
    }
}
