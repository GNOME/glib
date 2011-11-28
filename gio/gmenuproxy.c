/*
 * Copyright © 2011 Canonical Ltd.
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * licence, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
 * USA.
 *
 * Author: Ryan Lortie <desrt@desrt.ca>
 */

#include "gmenuproxy.h"

#include "gmenumodel.h"

/* Prelude {{{1 */

/**
 * SECTION:gmenuproxy
 * @title: GMenuProxy
 * @short_description: A D-Bus GMenuModel implementation
 * @see_also: <link linkend="gio-GMenuModel-exporter">GMenuModel Exporter</link>
 *
 * #GMenuProxy is an implementation of #GMenuModel that can be used
 * as a proxy for a menu model that is exported over D-Bus with
 * g_menu_model_dbus_export_start().
 */

/*
 * There are 3 main (quasi-)classes involved here:
 *
 *   - GMenuProxyPath
 *   - GMenuProxyGroup
 *   - GMenuProxy
 *
 * Each of these classes exists as a parameterised singleton keyed to a
 * particular thing:
 *
 *   - GMenuProxyPath represents a D-Bus object path on a particular
 *     unique bus name on a particular GDBusConnection.
 *
 *   - GMenuProxyGroup represents a particular group on a particular
 *     GMenuProxyPath.
 *
 *   - GMenuProxy represents a particular menu within a particular
 *     GMenuProxyGroup.
 *
 * There are also two (and a half) utility structs:
 *
 *  - PathIdentifier and ConstPathIdentifier
 *  - GMenuProxyItem
 *
 * PathIdentifier is the triplet of (GDBusConnection, unique name,
 * object path) that uniquely identifies a particular GMenuProxyPath.
 * It holds ownership on each of these things, so we have a
 * ConstPathIdentifier variant that does not.
 *
 * We have a 3-level hierarchy of hashtables:
 *
 *   - a global hashtable (g_menu_proxy_paths) maps from PathIdentifier
 *     to GMenuProxyPath
 *
 *   - each GMenuProxyPath has a hashtable mapping from guint (group
 *     number) to GMenuProxyGroup
 *
 *   - each GMenuProxyGroup has a hashtable mapping from guint (menu
 *     number) to GMenuProxy.
 *
 * In this way, each quintuplet of (connection, bus name, object path,
 * group id, menu id) maps to a single GMenuProxy instance that can be
 * located via 3 hashtable lookups.
 *
 * All of the 3 classes are refcounted (GMenuProxyPath and
 * GMenuProxyGroup manually, and GMenuProxy by virtue of being a
 * GObject).  The hashtables do not hold references -- rather, when the
 * last reference is dropped, the object is removed from the hashtable.
 *
 * The hard references go in the other direction: GMenuProxy is created
 * as the user requests it and only exists as long as the user holds a
 * reference on it.  GMenuProxy holds a reference on the GMenuProxyGroup
 * from which it came. GMenuProxyGroup holds a reference on
 * GMenuProxyPath.
 *
 * In addition to refcounts, each object has an 'active' variable (ints
 * for GMenuProxyPath and GMenuProxyGroup, boolean for GMenuProxy).
 *
 *   - GMenuProxy is inactive when created and becomes active only when
 *     first queried for information.  This prevents extra work from
 *     happening just by someone acquiring a GMenuProxy (and not
 *     actually trying to display it yet).
 *
 *   - The active count on GMenuProxyGroup is equal to the number of
 *     GMenuProxy instances in that group that are active.  When the
 *     active count transitions from 0 to 1, the group calls the 'Start'
 *     method on the service to begin monitoring that group.  When it
 *     drops from 1 to 0, the group calls the 'End' method to stop
 *     monitoring.
 *
 *   - The active count on GMenuProxyPath is equal to the number of
 *     GMenuProxyGroup instances on that path with a non-zero active
 *     count.  When the active count transitions from 0 to 1, the path
 *     sets up a signal subscription to monitor any changes.  The signal
 *     subscription is taken down when the active count transitions from
 *     1 to 0.
 *
 * When active, GMenuProxyPath gets incoming signals when changes occur.
 * If the change signal mentions a group for which we currently have an
 * active GMenuProxyGroup, the change signal is passed along to that
 * group.  If the group is inactive, the change signal is ignored.
 *
 * Most of the "work" occurs in GMenuProxyGroup.  In addition to the
 * hashtable of GMenuProxy instances, it keeps a hashtable of the actual
 * menu contents, each encoded as GSequence of GMenuProxyItem.  It
 * initially populates this table with the results of the "Start" method
 * call and then updates it according to incoming change signals.  If
 * the change signal mentions a menu for which we current have an active
 * GMenuProxy, the change signal is passed along to that proxy.  If the
 * proxy is inactive, the change signal is ignored.
 *
 * GMenuProxyItem is just a pair of hashtables, one for the attributes
 * and one for the links of the item (mapping strings to
 * other GMenuProxy instances). XXX reconsider this since it means the
 * submenu proxies stay around forever.....
 *
 * Following the "empty is the same as non-existent" rule, the hashtable
 * of GSequence of GMenuProxyItem holds NULL for empty menus.
 *
 * GMenuProxy contains very little functionality of its own.  It holds a
 * (weak) reference to the GSequence of GMenuProxyItem contained in the
 * GMenuProxyGroup.  It uses this GSequence to implement the GMenuModel
 * interface.  It also emits the "items-changed" signal if it is active
 * and it was told that the contents of the GSequence changed.
 */

typedef struct _GMenuProxyGroup GMenuProxyGroup;
typedef struct _GMenuProxyPath GMenuProxyPath;

static void                     g_menu_proxy_group_changed              (GMenuProxyGroup *group,
                                                                         guint            menu_id,
                                                                         gint             position,
                                                                         gint             removed,
                                                                         GVariant        *added);
static void                     g_menu_proxy_changed                    (GMenuProxy      *proxy,
                                                                         GSequence       *items,
                                                                         gint             position,
                                                                         gint             removed,
                                                                         gint             added);
static GMenuProxyGroup *        g_menu_proxy_group_get_from_path        (GMenuProxyPath  *path,
                                                                         guint            group_id);
static GMenuProxy *             g_menu_proxy_get_from_group             (GMenuProxyGroup *group,
                                                                         guint            menu_id);

/* PathIdentifier {{{1 */
typedef struct
{
  GDBusConnection *connection;
  gchar *bus_name;
  gchar *object_path;
} PathIdentifier;

typedef const struct
{
  GDBusConnection *connection;
  const gchar *bus_name;
  const gchar *object_path;
} ConstPathIdentifier;

static guint
path_identifier_hash (gconstpointer data)
{
  ConstPathIdentifier *id = data;

  return g_str_hash (id->object_path);
}

static gboolean
path_identifier_equal (gconstpointer a,
                       gconstpointer b)
{
  ConstPathIdentifier *id_a = a;
  ConstPathIdentifier *id_b = b;

  return id_a->connection == id_b->connection &&
         g_str_equal (id_a->bus_name, id_b->bus_name) &&
         g_str_equal (id_a->object_path, id_b->object_path);
}

static void
path_identifier_free (PathIdentifier *id)
{
  g_object_unref (id->connection);
  g_free (id->bus_name);
  g_free (id->object_path);

  g_slice_free (PathIdentifier, id);
}

static PathIdentifier *
path_identifier_new (ConstPathIdentifier *cid)
{
  PathIdentifier *id;

  id = g_slice_new (PathIdentifier);
  id->connection = g_object_ref (cid->connection);
  id->bus_name = g_strdup (cid->bus_name);
  id->object_path = g_strdup (cid->object_path);

  return id;
}

/* GMenuProxyPath {{{1 */

struct _GMenuProxyPath
{
  PathIdentifier *id;
  gint ref_count;

  GHashTable *groups;
  gint active;
  guint watch_id;
};

static GHashTable *g_menu_proxy_paths;

static GMenuProxyPath *
g_menu_proxy_path_ref (GMenuProxyPath *path)
{
  path->ref_count++;

  return path;
}

static void
g_menu_proxy_path_unref (GMenuProxyPath *path)
{
  if (--path->ref_count == 0)
    {
      g_hash_table_remove (g_menu_proxy_paths, path->id);
      g_hash_table_unref (path->groups);
      path_identifier_free (path->id);

      g_slice_free (GMenuProxyPath, path);
    }
}

static void
g_menu_proxy_path_signal (GDBusConnection *connection,
                          const gchar     *sender_name,
                          const gchar     *object_path,
                          const gchar     *interface_name,
                          const gchar     *signal_name,
                          GVariant        *parameters,
                          gpointer         user_data)
{
  GMenuProxyPath *path = user_data;
  GVariantIter *iter;
  guint group_id;
  guint menu_id;
  guint position;
  guint removes;
  GVariant *adds;

  if (!g_variant_is_of_type (parameters, G_VARIANT_TYPE ("(a(uuuuaa{sv}))")))
    return;

  g_variant_get (parameters, "(a(uuuuaa{sv}))", &iter);
  while (g_variant_iter_loop (iter, "(uuuu@aa{sv})", &group_id, &menu_id, &position, &removes, &adds))
    {
      GMenuProxyGroup *group;

      group = g_hash_table_lookup (path->groups, GINT_TO_POINTER (group_id));

      if (group != NULL)
        g_menu_proxy_group_changed (group, menu_id, position, removes, adds);
    }
  g_variant_iter_free (iter);
}

static void
g_menu_proxy_path_activate (GMenuProxyPath *path)
{
  if (path->active++ == 0)
    path->watch_id = g_dbus_connection_signal_subscribe (path->id->connection, path->id->bus_name,
                                                         "org.gtk.Menus", "Changed", path->id->object_path,
                                                         NULL, G_DBUS_SIGNAL_FLAGS_NONE,
                                                         g_menu_proxy_path_signal, path, NULL);
}

static void
g_menu_proxy_path_deactivate (GMenuProxyPath *path)
{
  if (--path->active == 0)
    g_dbus_connection_signal_unsubscribe (path->id->connection, path->watch_id);
}

static GMenuProxyPath *
g_menu_proxy_path_get (GDBusConnection *connection,
                       const gchar     *bus_name,
                       const gchar     *object_path)
{
  ConstPathIdentifier cid = { connection, bus_name, object_path };
  GMenuProxyPath *path;

  if (g_menu_proxy_paths == NULL)
    g_menu_proxy_paths = g_hash_table_new (path_identifier_hash, path_identifier_equal);

  path = g_hash_table_lookup (g_menu_proxy_paths, &cid);

  if (path == NULL)
    {
      path = g_slice_new (GMenuProxyPath);
      path->id = path_identifier_new (&cid);
      path->groups = g_hash_table_new (NULL, NULL);
      path->ref_count = 0;
      path->active = 0;

      g_hash_table_insert (g_menu_proxy_paths, path->id, path);
    }

  return g_menu_proxy_path_ref (path);
}

/* GMenuProxyGroup, GMenuProxyItem {{{1 */
typedef enum
{
  GROUP_OFFLINE,
  GROUP_PENDING,
  GROUP_ONLINE
} GroupStatus;

struct _GMenuProxyGroup
{
  GMenuProxyPath *path;
  guint id;

  GHashTable *proxies; /* uint -> unowned GMenuProxy */
  GHashTable *menus;   /* uint -> owned GSequence */
  gint ref_count;
  GroupStatus state;
  gint active;
};

typedef struct
{
  GHashTable *attributes;
  GHashTable *links;
} GMenuProxyItem;

static GMenuProxyGroup *
g_menu_proxy_group_ref (GMenuProxyGroup *group)
{
  group->ref_count++;

  return group;
}

static void
g_menu_proxy_group_unref (GMenuProxyGroup *group)
{
  if (--group->ref_count == 0)
    {
      g_assert (group->state == GROUP_OFFLINE);
      g_assert (group->active == 0);

      g_hash_table_remove (group->path->groups, GINT_TO_POINTER (group->id));
      g_hash_table_unref (group->proxies);

      g_menu_proxy_path_unref (group->path);

      g_slice_free (GMenuProxyGroup, group);
    }
}

static void
g_menu_proxy_item_free (gpointer data)
{
  GMenuProxyItem *item = data;

  g_hash_table_unref (item->attributes);
  g_hash_table_unref (item->links);

  g_slice_free (GMenuProxyItem, item);
}

static GMenuProxyItem *
g_menu_proxy_group_create_item (GMenuProxyGroup *context,
                                GVariant        *description)
{
  GMenuProxyItem *item;
  GVariantIter iter;
  const gchar *key;
  GVariant *value;

  item = g_slice_new (GMenuProxyItem);
  item->attributes = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) g_variant_unref);
  item->links = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) g_object_unref);

  g_variant_iter_init (&iter, description);
  while (g_variant_iter_loop (&iter, "{&sv}", &key, &value))
    if (key[0] == ':')
      {
        if (g_variant_is_of_type (value, G_VARIANT_TYPE ("(uu)")))
          {
            guint group_id, menu_id;
            GMenuProxyGroup *group;
            GMenuProxy *proxy;

            g_variant_get (value, "(uu)", &group_id, &menu_id);

            /* save the hash lookup in a relatively common case */
            if (context->id != group_id)
              group = g_menu_proxy_group_get_from_path (context->path, group_id);
            else
              group = g_menu_proxy_group_ref (context);

            proxy = g_menu_proxy_get_from_group (group, menu_id);

            /* key + 1 to skip the ':' */
            g_hash_table_insert (item->links, g_strdup (key + 1), proxy);

            g_menu_proxy_group_unref (group);
          }
      }
    else
      g_hash_table_insert (item->attributes, g_strdup (key), g_variant_ref (value));

  return item;
}

/*
 * GMenuProxyGroup can be in three states:
 *
 * OFFLINE: not subscribed to this group
 * PENDING: we made the call to subscribe to this group, but the result
 *          has not come back yet
 * ONLINE: we are fully subscribed
 *
 * We can get into some nasty situations where we make a call due to an
 * activation request but receive a deactivation request before the call
 * returns.  If another activation request occurs then we could risk
 * sending a Start request even though one is already in progress.  For
 * this reason, we have to carefully consider what to do in each of the
 * three states for each of the following situations:
 *
 *  - activation requested
 *  - deactivation requested
 *  - Start call finishes
 *
 * To simplify things a bit, we do not have a callback for the Stop
 * call.  We just send it and assume that it takes effect immediately.
 *
 * Activation requested:
 *   OFFLINE: make the Start call and transition to PENDING
 *   PENDING: do nothing -- call is already in progress.
 *   ONLINE: this should not be possible
 *
 * Deactivation requested:
 *   OFFLINE: this should not be possible
 *   PENDING: do nothing -- handle it when the Start call finishes
 *   ONLINE: send the Stop call and move to OFFLINE immediately
 *
 * Start call finishes:
 *   OFFLINE: this should not be possible
 *   PENDING:
 *     If we should be active (ie: active count > 0): move to ONLINE
 *     If not: send Stop call and move to OFFLINE immediately
 *   ONLINE: this should not be possible
 *
 * We have to take care with regards to signal subscriptions (ie:
 * activation of the GMenuProxyPath).  The signal subscription is always
 * established when transitioning from OFFLINE to PENDING and taken down
 * when transitioning to OFFLINE (from either PENDING or ONLINE).
 *
 * Since there are two places where we transition to OFFLINE, we split
 * that code out into a separate function.
 */
static void
g_menu_proxy_group_go_offline (GMenuProxyGroup *group)
{
  g_menu_proxy_path_deactivate (group->path);
  g_dbus_connection_call (group->path->id->connection,
                          group->path->id->bus_name,
                          group->path->id->object_path,
                          "org.gtk.Menus", "End",
                          g_variant_new_parsed ("([ %u ],)", group->id),
                          NULL, G_DBUS_CALL_FLAGS_NONE, -1,
                          NULL, NULL, NULL);
  group->state = GROUP_OFFLINE;
}


static void
g_menu_proxy_group_start_ready (GObject      *source_object,
                                GAsyncResult *result,
                                gpointer      user_data)
{
  GDBusConnection *connection = G_DBUS_CONNECTION (source_object);
  GMenuProxyGroup *group = user_data;
  GVariant *reply;

  g_assert (group->state == GROUP_PENDING);

  reply = g_dbus_connection_call_finish (connection, result, NULL);

  if (group->active)
    {
      group->state = GROUP_ONLINE;

      /* If we receive no reply, just act like we got an empty reply. */
      if (reply)
        {
          GVariantIter *iter;
          GVariant *items;
          guint group_id;
          guint menu_id;

          g_variant_get (reply, "(a(uuaa{sv}))", &iter);
          while (g_variant_iter_loop (iter, "(uu@aa{sv})", &group_id, &menu_id, &items))
            if (group_id == group->id)
              g_menu_proxy_group_changed (group, menu_id, 0, 0, items);
          g_variant_iter_free (iter);
        }
    }
  else
    g_menu_proxy_group_go_offline (group);

  if (reply)
    g_variant_unref (reply);

  g_menu_proxy_group_unref (group);
}

static void
g_menu_proxy_group_activate (GMenuProxyGroup *group)
{
  if (group->active++ == 0)
    {
      g_assert (group->state != GROUP_ONLINE);

      if (group->state == GROUP_OFFLINE)
        {
          g_menu_proxy_path_activate (group->path);

          g_dbus_connection_call (group->path->id->connection,
                                  group->path->id->bus_name,
                                  group->path->id->object_path,
                                  "org.gtk.Menus", "Start",
                                  g_variant_new_parsed ("([ %u ],)", group->id),
                                  G_VARIANT_TYPE ("(a(uuaa{sv}))"),
                                  G_DBUS_CALL_FLAGS_NONE, -1, NULL,
                                  g_menu_proxy_group_start_ready,
                                  g_menu_proxy_group_ref (group));
          group->state = GROUP_PENDING;
        }
    }
}

static void
g_menu_proxy_group_deactivate (GMenuProxyGroup *group)
{
  if (--group->active == 0)
    {
      g_assert (group->state != GROUP_OFFLINE);

      if (group->state == GROUP_ONLINE)
        {
          /* We are here because nobody is watching, so just free
           * everything and don't bother with the notifications.
           */
          g_hash_table_remove_all (group->menus);

          g_menu_proxy_group_go_offline (group);
        }
    }
}

static void
g_menu_proxy_group_changed (GMenuProxyGroup *group,
                            guint            menu_id,
                            gint             position,
                            gint             removed,
                            GVariant        *added)
{
  GSequenceIter *point;
  GVariantIter iter;
  GMenuProxy *proxy;
  GSequence *items;
  GVariant *item;
  gint n_added;

  /* We could have signals coming to us when we're not active (due to
   * some other process having subscribed to this group) or when we're
   * pending.  In both of those cases, we want to ignore the signal
   * since we'll get our own information when we call "Start" for
   * ourselves.
   */
  if (group->state != GROUP_ONLINE)
    return;

  items = g_hash_table_lookup (group->menus, GINT_TO_POINTER (menu_id));

  if (items == NULL)
    {
      items = g_sequence_new (g_menu_proxy_item_free);
      g_hash_table_insert (group->menus, GINT_TO_POINTER (menu_id), items);
    }

  point = g_sequence_get_iter_at_pos (items, position + removed);

  g_return_if_fail (point != NULL);

  if (removed)
    {
      GSequenceIter *start;

      start = g_sequence_get_iter_at_pos (items, position);
      g_sequence_remove_range (start, point);
    }

  n_added = g_variant_iter_init (&iter, added);
  while (g_variant_iter_loop (&iter, "@a{sv}", &item))
    g_sequence_insert_before (point, g_menu_proxy_group_create_item (group, item));

  if (g_sequence_get_length (items) == 0)
    {
      g_hash_table_remove (group->menus, GINT_TO_POINTER (menu_id));
      items = NULL;
    }

  if ((proxy = g_hash_table_lookup (group->proxies, GINT_TO_POINTER (menu_id))))
    g_menu_proxy_changed (proxy, items, position, removed, n_added);
}

static GMenuProxyGroup *
g_menu_proxy_group_get_from_path (GMenuProxyPath *path,
                                  guint           group_id)
{
  GMenuProxyGroup *group;

  group = g_hash_table_lookup (path->groups, GINT_TO_POINTER (group_id));

  if (group == NULL)
    {
      group = g_slice_new (GMenuProxyGroup);
      group->path = g_menu_proxy_path_ref (path);
      group->id = group_id;
      group->proxies = g_hash_table_new (NULL, NULL);
      group->menus = g_hash_table_new_full (NULL, NULL, NULL, (GDestroyNotify) g_sequence_free);
      group->state = GROUP_OFFLINE;
      group->active = 0;
      group->ref_count = 0;

      g_hash_table_insert (path->groups, GINT_TO_POINTER (group->id), group);
    }

  return g_menu_proxy_group_ref (group);
}

static GMenuProxyGroup *
g_menu_proxy_group_get (GDBusConnection *connection,
                        const gchar     *bus_name,
                        const gchar     *object_path,
                        guint            group_id)
{
  GMenuProxyGroup *group;
  GMenuProxyPath *path;

  path = g_menu_proxy_path_get (connection, bus_name, object_path);
  group = g_menu_proxy_group_get_from_path (path, group_id);
  g_menu_proxy_path_unref (path);

  return group;
}

/* GMenuProxy {{{1 */

typedef GMenuModelClass GMenuProxyClass;
struct _GMenuProxy
{
  GMenuModel parent;

  GMenuProxyGroup *group;
  guint id;

  GSequence *items; /* unowned */
  gboolean active;
};

G_DEFINE_TYPE (GMenuProxy, g_menu_proxy, G_TYPE_MENU_MODEL)

static gboolean
g_menu_proxy_is_mutable (GMenuModel *model)
{
  return TRUE;
}

static gint
g_menu_proxy_get_n_items (GMenuModel *model)
{
  GMenuProxy *proxy = G_MENU_PROXY (model);

  if (!proxy->active)
    {
      g_menu_proxy_group_activate (proxy->group);
      proxy->active = TRUE;
    }

  return proxy->items ? g_sequence_get_length (proxy->items) : 0;
}

static void
g_menu_proxy_get_item_attributes (GMenuModel  *model,
                                  gint         item_index,
                                  GHashTable **table)
{
  GMenuProxy *proxy = G_MENU_PROXY (model);
  GMenuProxyItem *item;
  GSequenceIter *iter;

  g_return_if_fail (proxy->active);
  g_return_if_fail (proxy->items);

  iter = g_sequence_get_iter_at_pos (proxy->items, item_index);
  g_return_if_fail (iter);

  item = g_sequence_get (iter);
  g_return_if_fail (item);

  *table = g_hash_table_ref (item->attributes);
}

static void
g_menu_proxy_get_item_links (GMenuModel  *model,
                             gint         item_index,
                             GHashTable **table)
{
  GMenuProxy *proxy = G_MENU_PROXY (model);
  GMenuProxyItem *item;
  GSequenceIter *iter;

  g_return_if_fail (proxy->active);
  g_return_if_fail (proxy->items);

  iter = g_sequence_get_iter_at_pos (proxy->items, item_index);
  g_return_if_fail (iter);

  item = g_sequence_get (iter);
  g_return_if_fail (item);

  *table = g_hash_table_ref (item->links);
}

static void
g_menu_proxy_finalize (GObject *object)
{
  GMenuProxy *proxy = G_MENU_PROXY (object);

  if (proxy->active)
    g_menu_proxy_group_deactivate (proxy->group);

  g_hash_table_remove (proxy->group->proxies, GINT_TO_POINTER (proxy->id));
  g_menu_proxy_group_unref (proxy->group);

  G_OBJECT_CLASS (g_menu_proxy_parent_class)
    ->finalize (object);
}

static void
g_menu_proxy_init (GMenuProxy *proxy)
{
}

static void
g_menu_proxy_class_init (GMenuProxyClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  class->is_mutable = g_menu_proxy_is_mutable;
  class->get_n_items = g_menu_proxy_get_n_items;
  class->get_item_attributes = g_menu_proxy_get_item_attributes;
  class->get_item_links = g_menu_proxy_get_item_links;

  object_class->finalize = g_menu_proxy_finalize;
}

static void
g_menu_proxy_changed (GMenuProxy *proxy,
                      GSequence  *items,
                      gint        position,
                      gint        removed,
                      gint        added)
{
  proxy->items = items;

  if (proxy->active && (removed || added))
    g_menu_model_items_changed (G_MENU_MODEL (proxy), position, removed, added);
}

static GMenuProxy *
g_menu_proxy_get_from_group (GMenuProxyGroup *group,
                             guint            menu_id)
{
  GMenuProxy *proxy;

  proxy = g_hash_table_lookup (group->proxies, GINT_TO_POINTER (menu_id));
  if (proxy)
    g_object_ref (proxy);

  if (proxy == NULL)
    {
      proxy = g_object_new (G_TYPE_MENU_PROXY, NULL);
      proxy->items = g_hash_table_lookup (group->menus, GINT_TO_POINTER (menu_id));
      g_hash_table_insert (group->proxies, GINT_TO_POINTER (menu_id), proxy);
      proxy->group = g_menu_proxy_group_ref (group);
      proxy->id = menu_id;
    }

  return proxy;
}

/**
 * g_menu_proxy_get:
 * @connection: a #GDBusConnection
 * @bus_name: the bus name which exports the menu model
 * @object_path: the object path at which the menu model is exported
 *
 * Obtains a #GMenuProxy for the menu model which is exported
 * at the given @bus_name and @object_path.
 *
 * Returns: (transfer full): a #GMenuProxy object. Free with g_object_unref().
 */
GMenuProxy *
g_menu_proxy_get (GDBusConnection *connection,
                  const gchar     *bus_name,
                  const gchar     *object_path)
{
  GMenuProxyGroup *group;
  GMenuProxy *proxy;

  group = g_menu_proxy_group_get (connection, bus_name, object_path, 0);
  proxy = g_menu_proxy_get_from_group (group, 0);
  g_menu_proxy_group_unref (group);

  return proxy;
}

/* Epilogue {{{1 */
/* vim:set foldmethod=marker: */
