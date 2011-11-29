/*
 * Copyright © 2011 Canonical Ltd.
 *
 *  This library is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  licence, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
 *  USA.
 *
 * Author: Ryan Lortie <desrt@desrt.ca>
 */

#include "gmenuexporter.h"

#include "gdbusmethodinvocation.h"
#include "gdbusintrospection.h"
#include "gdbusnamewatching.h"
#include "gdbuserror.h"

/**
 * SECTION:gmenuexporter
 * @title: GMenuModel exporter
 * @short_description: Export GMenuModels on D-Bus
 * @see_also: #GMenuModel, #GMenuProxy
 *
 * These functions support exporting a #GMenuModel on D-Bus.
 * The D-Bus interface that is used is a private implementation
 * detail.
 *
 * To access an exported #GMenuModel remotely, use
 * g_menu_proxy_get() to obtain a #GMenuProxy.
 */

/* {{{1 D-Bus Interface description */

/* The org.gtk.Menus interface
 * ===========================
 *
 * The interface is primarily concerned with three things:
 *
 * - communicating menus to the client
 * - establishing links between menus and other menus
 * - notifying clients of changes
 *
 * As a basic principle, it is recognised that the menu structure
 * of an application is often large. It is also recognised that some
 * menus are liable to frequently change without the user ever having
 * opened the menu. For both of these reasons, the individual menus are
 * arranged into subscription groups. Each subscription group is specified
 * by an unsigned integer. The assignment of integers need not be consecutive.
 *
 * Within a subscription group there are multiple menus. Each menu is
 * identified with an unsigned integer, unique to its subscription group.
 *
 * By convention, the primary menu is numbered 0 without subscription group 0.
 *
 * Actionable menu items (ie: those that produce some effect in the
 * application when they are activated) have a related action, specified by
 * a string. This string specifies the name of the action, according to the
 * org.gtk.Actions interface, at the same object path as the menu.
 *
 * Methods
 * -------
 *
 * Start :: (au) → (a(uuaa{sv}))
 *
 *   The Start method is used to indicate that a client is interested in
 *   tracking and displaying the content of the menus of a particular list
 *   of subscription groups.
 *
 *   Most typically, the client will request subscription group 0 to start.
 *
 *   The call has two effects. First, it replies with all menus defined
 *   within the requested subscription groups. The format of the reply is
 *   an array of tuples, where the items in each tuple are:
 *   - the subscription group of the menu
 *   - the number of the menu within that group
 *   - an array of menu items
 *
 *   Each menu item is a dictionary of attributes (a{sv}).
 *
 *   Secondly, this call has a side effect: it atomically requests that
 *   the Changed signal start to be emitted for the requested subscription
 *   group. Each group has a subscription count and only signals changes
 *   on itself when this count is greater than zero.
 *
 *   If a group is specified multiple times then the result is that the
 *   contents of that group is only returned once, but the subscription
 *   count is increased multiple times.
 *
 *   If a client disconnects from the bus while holding subscriptions then
 *   its subscriptions will be cancelled. This prevents "leaking" subscriptions
 *   in the case of crashes and is also useful for applications that want
 *   to exit without manually cleaning up.
 *
 * End :: (au)
 *
 *   The End method reverses the previous effects of a call to Start.
 *
 *   When clients are no longer interested in the contents of a subscription
 *   group, they should call the End method.
 *
 *   The parameter lists the subscription groups. A subscription group
 *   needs to be cancelled the same number of times as it was requested.
 *   For this reason, it might make sense to specify the same subscription
 *   group multiple times (if multiple Start calls were made for this group).
 *
 * Signals
 * -------
 *
 * Changed :: (a(uuuuaa{sv}))
 *
 *   The changed signal indicates changes to a particular menu.
 *
 *   The changes come as an array of tuples where the items in each tuple are:
 *   - the subscription group of the menu
 *   - the number of the menu within that group
 *   - the position in the menu at which to make the change
 *   - the number of items to delete from that position
 *   - a list of new items to insert at that position
 *
 *   Each new menu item is a dictionary of attributes (a{sv}).
 *
 * Attributes
 * ----------
 *
 * label (string): the label to display
 * action (string): the name of the action
 * target (variant): the parameter to pass when activating the action
 * :section ((uu)): the menu to use to populate that section, specified
 *     as a pair of subscription group and menu within that group
 * :submenu ((uu)): the menu to use as a submenu, specified
 *     as a pair of subscription group and menu within that group
 */

static GDBusInterfaceInfo *
org_gtk_Menus_get_interface (void)
{
  static GDBusInterfaceInfo *interface_info;

  if (interface_info == NULL)
    {
      GError *error = NULL;
      GDBusNodeInfo *info;

      info = g_dbus_node_info_new_for_xml ("<node>"
                                           "  <interface name='org.gtk.Menus'>"
                                           "    <method name='Start'>"
                                           "      <arg type='au' name='groups' direction='in'/>"
                                           "      <arg type='a(uuaa{sv})' name='content' direction='out'/>"
                                           "    </method>"
                                           "    <method name='End'>"
                                           "      <arg type='au' name='groups' direction='in'/>"
                                           "    </method>"
                                           "    <signal name='Changed'>"
                                           "      arg type='a(uuuuaa{sv})' name='changes'/>"
                                           "    </signal>"
                                           "  </interface>"
                                           "</node>", &error);
      if (info == NULL)
        g_error ("%s\n", error->message);
      interface_info = g_dbus_node_info_lookup_interface (info, "org.gtk.Menus");
      g_assert (interface_info != NULL);
      g_dbus_interface_info_ref (interface_info);
      g_dbus_node_info_unref (info);
    }

  return interface_info;
}

/* {{{1 Forward declarations */
typedef struct _GMenuExporterMenu                           GMenuExporterMenu;
typedef struct _GMenuExporterLink                           GMenuExporterLink;
typedef struct _GMenuExporterGroup                          GMenuExporterGroup;
typedef struct _GMenuExporterRemote                         GMenuExporterRemote;
typedef struct _GMenuExporterWatch                          GMenuExporterWatch;
typedef struct _GMenuExporter                               GMenuExporter;

static gboolean                 g_menu_exporter_group_is_subscribed    (GMenuExporterGroup *group);
static guint                    g_menu_exporter_group_get_id           (GMenuExporterGroup *group);
static GMenuExporter *          g_menu_exporter_group_get_exporter     (GMenuExporterGroup *group);
static GMenuExporterMenu *      g_menu_exporter_group_add_menu         (GMenuExporterGroup *group,
                                                                        GMenuModel         *model);
static void                     g_menu_exporter_group_remove_menu      (GMenuExporterGroup *group,
                                                                        guint               id);

static GMenuExporterGroup *     g_menu_exporter_create_group           (GMenuExporter      *exporter);
static GMenuExporterGroup *     g_menu_exporter_lookup_group           (GMenuExporter      *exporter,
                                                                        guint               group_id);
static void                     g_menu_exporter_report                 (GMenuExporter      *exporter,
                                                                        GVariant           *report);
static void                     g_menu_exporter_remove_group           (GMenuExporter      *exporter,
                                                                        guint               id);

/* {{{1 GMenuExporterLink, GMenuExporterMenu */

struct _GMenuExporterMenu
{
  GMenuExporterGroup *group;
  guint               id;

  GMenuModel *model;
  gulong      handler_id;
  GSequence  *item_links;
};

struct _GMenuExporterLink
{
  gchar             *name;
  GMenuExporterMenu *menu;
  GMenuExporterLink *next;
};

static void
g_menu_exporter_menu_free (GMenuExporterMenu *menu)
{
  g_menu_exporter_group_remove_menu (menu->group, menu->id);

  if (menu->handler_id != 0)
    g_signal_handler_disconnect (menu->model, menu->handler_id);

  if (menu->item_links != NULL)
    g_sequence_free (menu->item_links);

  g_object_unref (menu->model);

  g_slice_free (GMenuExporterMenu, menu);
}

static void
g_menu_exporter_link_free (gpointer data)
{
  GMenuExporterLink *link = data;

  while (link != NULL)
    {
      GMenuExporterLink *tmp = link;
      link = tmp->next;

      g_menu_exporter_menu_free (tmp->menu);
      g_free (tmp->name);

      g_slice_free (GMenuExporterLink, tmp);
    }
}

static GMenuExporterLink *
g_menu_exporter_menu_create_links (GMenuExporterMenu *menu,
                                   gint               position)
{
  GMenuExporterLink *list = NULL;
  GMenuLinkIter *iter;
  const char *name;
  GMenuModel *model;

  iter = g_menu_model_iterate_item_links (menu->model, position);

  while (g_menu_link_iter_get_next (iter, &name, &model))
    {
      GMenuExporterGroup *group;
      GMenuExporterLink *tmp;

      /* keep sections in the same group, but create new groups
       * otherwise
       */
      if (!g_str_equal (name, "section"))
        group = g_menu_exporter_create_group (g_menu_exporter_group_get_exporter (menu->group));
      else
        group = menu->group;

      tmp = g_slice_new (GMenuExporterLink);
      tmp->name = g_strconcat (":", name, NULL);
      tmp->menu = g_menu_exporter_group_add_menu (group, model);
      tmp->next = list;
      list = tmp;

      g_object_unref (model);
    }

  g_object_unref (iter);

  return list;
}

static GVariant *
g_menu_exporter_menu_describe_item (GMenuExporterMenu *menu,
                                    gint               position)
{
  GMenuAttributeIter *attr_iter;
  GVariantBuilder builder;
  GSequenceIter *iter;
  GMenuExporterLink *link;
  const char *name;
  GVariant *value;

  g_variant_builder_init (&builder, G_VARIANT_TYPE_VARDICT);

  attr_iter = g_menu_model_iterate_item_attributes (menu->model, position);
  while (g_menu_attribute_iter_get_next (attr_iter, &name, &value))
    {
      g_variant_builder_add (&builder, "{sv}", name, value);
      g_variant_unref (value);
    }
  g_object_unref (attr_iter);

  iter = g_sequence_get_iter_at_pos (menu->item_links, position);
  for (link = g_sequence_get (iter); link; link = link->next)
    g_variant_builder_add (&builder, "{sv}", link->name,
                           g_variant_new ("(uu)", g_menu_exporter_group_get_id (link->menu->group), link->menu->id));

  return g_variant_builder_end (&builder);
}

static GVariant *
g_menu_exporter_menu_list (GMenuExporterMenu *menu)
{
  GVariantBuilder builder;
  gint i, n;

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("aa{sv}"));

  n = g_sequence_get_length (menu->item_links);
  for (i = 0; i < n; i++)
    g_variant_builder_add_value (&builder, g_menu_exporter_menu_describe_item (menu, i));

  return g_variant_builder_end (&builder);
}

static void
g_menu_exporter_menu_items_changed (GMenuModel *model,
                                    gint        position,
                                    gint        removed,
                                    gint        added,
                                    gpointer    user_data)
{
  GMenuExporterMenu *menu = user_data;
  GSequenceIter *point;
  gint i;

  g_assert (menu->model == model);
  g_assert (menu->item_links != NULL);
  g_assert (position + removed <= g_sequence_get_length (menu->item_links));

  point = g_sequence_get_iter_at_pos (menu->item_links, position + removed);
  g_sequence_remove_range (g_sequence_get_iter_at_pos (menu->item_links, position), point);

  for (i = position; i < position + added; i++)
    g_sequence_insert_before (point, g_menu_exporter_menu_create_links (menu, i));

  if (g_menu_exporter_group_is_subscribed (menu->group))
    {
      GVariantBuilder builder;

      g_variant_builder_init (&builder, G_VARIANT_TYPE ("(uuuuaa{sv})"));
      g_variant_builder_add (&builder, "u", g_menu_exporter_group_get_id (menu->group));
      g_variant_builder_add (&builder, "u", menu->id);
      g_variant_builder_add (&builder, "u", position);
      g_variant_builder_add (&builder, "u", removed);

      g_variant_builder_open (&builder, G_VARIANT_TYPE ("aa{sv}"));
      for (i = position; i < position + added; i++)
        g_variant_builder_add_value (&builder, g_menu_exporter_menu_describe_item (menu, i));
      g_variant_builder_close (&builder);

      g_menu_exporter_report (g_menu_exporter_group_get_exporter (menu->group), g_variant_builder_end (&builder));
    }
}

static void
g_menu_exporter_menu_prepare (GMenuExporterMenu *menu)
{
  gint n_items;

  g_assert (menu->item_links == NULL);

  if (g_menu_model_is_mutable (menu->model))
    menu->handler_id = g_signal_connect (menu->model, "items-changed",
                                         G_CALLBACK (g_menu_exporter_menu_items_changed), menu);

  menu->item_links = g_sequence_new (g_menu_exporter_link_free);

  n_items = g_menu_model_get_n_items (menu->model);
  if (n_items)
    g_menu_exporter_menu_items_changed (menu->model, 0, 0, n_items, menu);
}

static GMenuExporterMenu *
g_menu_exporter_menu_new (GMenuExporterGroup *group,
                          guint               id,
                          GMenuModel         *model)
{
  GMenuExporterMenu *menu;

  menu = g_slice_new0 (GMenuExporterMenu);
  menu->group = group;
  menu->id = id;
  menu->model = g_object_ref (model);

  return menu;
}

/* {{{1 GMenuExporterGroup */

struct _GMenuExporterGroup
{
  GMenuExporter *exporter;
  guint          id;

  GHashTable *menus;
  guint       next_menu_id;
  gboolean    prepared;

  gint subscribed;
};

static void
g_menu_exporter_group_check_if_useless (GMenuExporterGroup *group)
{
  if (g_hash_table_size (group->menus) == 0 && group->subscribed == 0)
    {
      g_menu_exporter_remove_group (group->exporter, group->id);

      g_hash_table_unref (group->menus);

      g_slice_free (GMenuExporterGroup, group);
    }
}

static void
g_menu_exporter_group_subscribe (GMenuExporterGroup *group,
                                 GVariantBuilder    *builder)
{
  GHashTableIter iter;
  gpointer key, val;

  if (!group->prepared)
    {
      GMenuExporterMenu *menu;

      /* set this first, so that any menus created during the
       * preparation of the first menu also end up in the prepared
       * state.
       * */
      group->prepared = TRUE;

      menu = g_hash_table_lookup (group->menus, 0);
      g_menu_exporter_menu_prepare (menu);
    }

  group->subscribed++;

  g_hash_table_iter_init (&iter, group->menus);
  while (g_hash_table_iter_next (&iter, &key, &val))
    {
      guint id = GPOINTER_TO_INT (key);
      GMenuExporterMenu *menu = val;

      if (g_sequence_get_length (menu->item_links))
        {
          g_variant_builder_open (builder, G_VARIANT_TYPE ("(uuaa{sv})"));
          g_variant_builder_add (builder, "u", group->id);
          g_variant_builder_add (builder, "u", id);
          g_variant_builder_add_value (builder, g_menu_exporter_menu_list (menu));
          g_variant_builder_close (builder);
        }
    }
}

static void
g_menu_exporter_group_unsubscribe (GMenuExporterGroup *group,
                                   gint                count)
{
  g_assert (group->subscribed >= count);

  group->subscribed -= count;

  g_menu_exporter_group_check_if_useless (group);
}

static GMenuExporter *
g_menu_exporter_group_get_exporter (GMenuExporterGroup *group)
{
  return group->exporter;
}

static gboolean
g_menu_exporter_group_is_subscribed (GMenuExporterGroup *group)
{
  return group->subscribed > 0;
}

static guint
g_menu_exporter_group_get_id (GMenuExporterGroup *group)
{
  return group->id;
}

static void
g_menu_exporter_group_remove_menu (GMenuExporterGroup *group,
                                   guint               id)
{
  g_hash_table_remove (group->menus, GINT_TO_POINTER (id));

  g_menu_exporter_group_check_if_useless (group);
}

static GMenuExporterMenu *
g_menu_exporter_group_add_menu (GMenuExporterGroup *group,
                                GMenuModel         *model)
{
  GMenuExporterMenu *menu;
  guint id;

  id = group->next_menu_id++;
  menu = g_menu_exporter_menu_new (group, id, model);
  g_hash_table_insert (group->menus, GINT_TO_POINTER (id), menu);

  if (group->prepared)
    g_menu_exporter_menu_prepare (menu);

  return menu;
}

static GMenuExporterGroup *
g_menu_exporter_group_new (GMenuExporter *exporter,
                           guint          id)
{
  GMenuExporterGroup *group;

  group = g_slice_new0 (GMenuExporterGroup);
  group->menus = g_hash_table_new (NULL, NULL);
  group->exporter = exporter;
  group->id = id;

  return group;
}

/* {{{1 GMenuExporterRemote */

struct _GMenuExporterRemote
{
  GMenuExporter *exporter;
  GHashTable    *watches;
  guint          watch_id;
};

static void
g_menu_exporter_remote_subscribe (GMenuExporterRemote *remote,
                                  guint                group_id,
                                  GVariantBuilder     *builder)
{
  GMenuExporterGroup *group;
  guint count;

  count = (gsize) g_hash_table_lookup (remote->watches, GINT_TO_POINTER (group_id));
  g_hash_table_insert (remote->watches, GINT_TO_POINTER (group_id), GINT_TO_POINTER (count + 1));

  group = g_menu_exporter_lookup_group (remote->exporter, group_id);
  g_menu_exporter_group_subscribe (group, builder);
}

static void
g_menu_exporter_remote_unsubscribe (GMenuExporterRemote *remote,
                                    guint                group_id)
{
  GMenuExporterGroup *group;
  guint count;

  count = (gsize) g_hash_table_lookup (remote->watches, GINT_TO_POINTER (group_id));

  if (count == 0)
    return;

  if (count != 1)
    g_hash_table_insert (remote->watches, GINT_TO_POINTER (group_id), GINT_TO_POINTER (count - 1));
  else
    g_hash_table_remove (remote->watches, GINT_TO_POINTER (group_id));

  group = g_menu_exporter_lookup_group (remote->exporter, group_id);
  g_menu_exporter_group_unsubscribe (group, 1);
}

static gboolean
g_menu_exporter_remote_has_subscriptions (GMenuExporterRemote *remote)
{
  return g_hash_table_size (remote->watches) != 0;
}

static void
g_menu_exporter_remote_free (gpointer data)
{
  GMenuExporterRemote *remote = data;
  GHashTableIter iter;
  gpointer key, val;

  g_hash_table_iter_init (&iter, remote->watches);
  while (g_hash_table_iter_next (&iter, &key, &val))
    {
      GMenuExporterGroup *group;

      group = g_menu_exporter_lookup_group (remote->exporter, GPOINTER_TO_INT (key));
      g_menu_exporter_group_unsubscribe (group, GPOINTER_TO_INT (val));
    }

  g_bus_unwatch_name (remote->watch_id);
  g_hash_table_unref (remote->watches);

  g_slice_free (GMenuExporterRemote, remote);
}

static GMenuExporterRemote *
g_menu_exporter_remote_new (GMenuExporter *exporter,
                            guint          watch_id)
{
  GMenuExporterRemote *remote;

  remote = g_slice_new0 (GMenuExporterRemote);
  remote->exporter = exporter;
  remote->watches = g_hash_table_new (NULL, NULL);
  remote->watch_id = watch_id;

  return remote;
}

/* {{{1 GMenuExporter */

struct _GMenuExporter
{
  GDBusConnection *connection;
  gchar *object_path;
  guint registration_id;
  GHashTable *groups;
  guint next_group_id;

  GMenuExporterMenu *root;
  GHashTable *remotes;
};

static void
g_menu_exporter_name_vanished (GDBusConnection *connection,
                               const gchar     *name,
                               gpointer         user_data)
{
  GMenuExporter *exporter = user_data;

  g_assert (exporter->connection == connection);

  g_hash_table_remove (exporter->remotes, name);
}

static GVariant *
g_menu_exporter_subscribe (GMenuExporter *exporter,
                           const gchar   *sender,
                           GVariant      *group_ids)
{
  GMenuExporterRemote *remote;
  GVariantBuilder builder;
  GVariantIter iter;
  guint32 id;

  remote = g_hash_table_lookup (exporter->remotes, sender);

  if (remote == NULL)
    {
      guint watch_id;

      watch_id = g_bus_watch_name_on_connection (exporter->connection, sender, G_BUS_NAME_WATCHER_FLAGS_NONE,
                                                 NULL, g_menu_exporter_name_vanished, exporter, NULL);
      remote = g_menu_exporter_remote_new (exporter, watch_id);
      g_hash_table_insert (exporter->remotes, g_strdup (sender), remote);
    }

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("(a(uuaa{sv}))"));

  g_variant_builder_open (&builder, G_VARIANT_TYPE ("a(uuaa{sv})"));

  g_variant_iter_init (&iter, group_ids);
  while (g_variant_iter_next (&iter, "u", &id))
    g_menu_exporter_remote_subscribe (remote, id, &builder);

  g_variant_builder_close (&builder);

  return g_variant_builder_end (&builder);
}

static void
g_menu_exporter_unsubscribe (GMenuExporter *exporter,
                             const gchar   *sender,
                             GVariant      *group_ids)
{
  GMenuExporterRemote *remote;
  GVariantIter iter;
  guint32 id;

  remote = g_hash_table_lookup (exporter->remotes, sender);

  if (remote == NULL)
    return;

  g_variant_iter_init (&iter, group_ids);
  while (g_variant_iter_next (&iter, "u", &id))
    g_menu_exporter_remote_unsubscribe (remote, id);

  if (!g_menu_exporter_remote_has_subscriptions (remote))
    g_hash_table_remove (exporter->remotes, sender);
}

static void
g_menu_exporter_report (GMenuExporter *exporter,
                        GVariant      *report)
{
  GVariantBuilder builder;

  g_variant_builder_init (&builder, G_VARIANT_TYPE_TUPLE);
  g_variant_builder_open (&builder, G_VARIANT_TYPE_ARRAY);
  g_variant_builder_add_value (&builder, report);
  g_variant_builder_close (&builder);

  g_dbus_connection_emit_signal (exporter->connection,
                                 NULL,
                                 exporter->object_path,
                                 "org.gtk.Menus", "Changed",
                                 g_variant_builder_end (&builder),
                                 NULL);
}

static void
g_menu_exporter_remove_group (GMenuExporter *exporter,
                              guint          id)
{
  g_hash_table_remove (exporter->groups, GINT_TO_POINTER (id));
}

static GMenuExporterGroup *
g_menu_exporter_lookup_group (GMenuExporter *exporter,
                              guint          group_id)
{
  GMenuExporterGroup *group;

  group = g_hash_table_lookup (exporter->groups, GINT_TO_POINTER (group_id));

  if (group == NULL)
    {
      group = g_menu_exporter_group_new (exporter, group_id);
      g_hash_table_insert (exporter->groups, GINT_TO_POINTER (group_id), group);
    }

  return group;
}

static GMenuExporterGroup *
g_menu_exporter_create_group (GMenuExporter *exporter)
{
  GMenuExporterGroup *group;
  guint id;

  id = exporter->next_group_id++;
  group = g_menu_exporter_group_new (exporter, id);
  g_hash_table_insert (exporter->groups, GINT_TO_POINTER (id), group);

  return group;
}

static void
g_menu_exporter_free (GMenuExporter *exporter)
{
  g_dbus_connection_unregister_object (exporter->connection, exporter->registration_id);
  g_menu_exporter_menu_free (exporter->root);
  g_hash_table_unref (exporter->remotes);
  g_hash_table_unref (exporter->groups);
  g_object_unref (exporter->connection);
  g_free (exporter->object_path);

  g_slice_free (GMenuExporter, exporter);
}

static void
g_menu_exporter_method_call (GDBusConnection       *connection,
                             const gchar           *sender,
                             const gchar           *object_path,
                             const gchar           *interface_name,
                             const gchar           *method_name,
                             GVariant              *parameters,
                             GDBusMethodInvocation *invocation,
                             gpointer               user_data)
{
  GMenuExporter *exporter = user_data;
  GVariant *group_ids;

  group_ids = g_variant_get_child_value (parameters, 0);

  if (g_str_equal (method_name, "Start"))
    g_dbus_method_invocation_return_value (invocation, g_menu_exporter_subscribe (exporter, sender, group_ids));

  else if (g_str_equal (method_name, "End"))
    {
      g_menu_exporter_unsubscribe (exporter, sender, group_ids);
      g_dbus_method_invocation_return_value (invocation, NULL);
    }

  else
    g_assert_not_reached ();

  g_variant_unref (group_ids);
}

static GDBusConnection *
g_menu_exporter_get_connection (GMenuExporter *exporter)
{
  return exporter->connection;
}

static const gchar *
g_menu_exporter_get_object_path (GMenuExporter *exporter)
{
  return exporter->object_path;
}

static GMenuExporter *
g_menu_exporter_new (GDBusConnection  *connection,
                     const gchar      *object_path,
                     GMenuModel       *model,
                     GError          **error)
{
  const GDBusInterfaceVTable vtable = {
    g_menu_exporter_method_call,
  };
  GMenuExporter *exporter;
  guint id;

  exporter = g_slice_new0 (GMenuExporter);

  id = g_dbus_connection_register_object (connection, object_path, org_gtk_Menus_get_interface (),
                                          &vtable, exporter, NULL, error);

  if (id == 0)
    {
      g_slice_free (GMenuExporter, exporter);
      return NULL;
    }

  exporter->connection = g_object_ref (connection);
  exporter->object_path = g_strdup (object_path);
  exporter->registration_id = id;
  exporter->groups = g_hash_table_new (NULL, NULL);
  exporter->remotes = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_menu_exporter_remote_free);
  exporter->root = g_menu_exporter_group_add_menu (g_menu_exporter_create_group (exporter), model);

  return exporter;
}

/* {{{1 Public API */

static GHashTable *exported_menus;

/**
 * g_menu_model_dbus_export_start:
 * @connection: a #GDBusConnection
 * @object_path: a D-Bus object path
 * @menu: a #GMenuModel
 * @error: return location for an error, or %NULL
 *
 * Exports @menu on @connection at @object_path.
 *
 * The implemented D-Bus API should be considered private.
 * It is subject to change in the future.
 *
 * A given menu model can only be exported on one object path
 * and an object path can only have one action group exported
 * on it. If either constraint is violated, the export will
 * fail and %FALSE will be returned (with @error set accordingly).
 *
 * Use g_menu_model_dbus_export_stop() to stop exporting @menu
 * or g_menu_model_dbus_export_query() to find out if and where
 * a given menu model is exported.
 *
 * Returns: %TRUE if the export is successful, or %FALSE (with
 *     @error set) in the event of a failure.
 */
gboolean
g_menu_model_dbus_export_start (GDBusConnection  *connection,
                                const gchar      *object_path,
                                GMenuModel       *menu,
                                GError          **error)
{
  GMenuExporter *exporter;

  if G_UNLIKELY (exported_menus == NULL)
    exported_menus = g_hash_table_new (NULL, NULL);

  if G_UNLIKELY (g_hash_table_lookup (exported_menus, menu))
    {
      g_set_error (error, G_DBUS_ERROR, G_DBUS_ERROR_FILE_EXISTS,
                   "The given GMenuModel has already been exported");
      return FALSE;
    }

  exporter = g_menu_exporter_new (connection, object_path, menu, error);

  if (exporter == NULL)
    return FALSE;

  g_hash_table_insert (exported_menus, menu, exporter);

  return TRUE;
}

/**
 * g_menu_model_dbus_export_stop:
 * @menu: a #GMenuModel
 *
 * Stops the export of @menu.
 *
 * This reverses the effect of a previous call to
 * g_menu_model_dbus_export_start() for @menu.
 *
 * Returns: %TRUE if an export was stopped or %FALSE
 *     if @menu was not exported in the first place
 */
gboolean
g_menu_model_dbus_export_stop (GMenuModel *menu)
{
  GMenuExporter *exporter;

  if G_UNLIKELY (exported_menus == NULL)
    return FALSE;

  exporter = g_hash_table_lookup (exported_menus, menu);
  if G_UNLIKELY (exporter == NULL)
    return FALSE;

  g_hash_table_remove (exported_menus, menu);
  g_menu_exporter_free (exporter);

  return TRUE;
}

/**
 * g_menu_model_dbus_export_query:
 * @menu: a #GMenuModel
 * @connection: (out): the #GDBusConnection used for exporting
 * @object_path: (out): the object path used for exporting
 *
 * Queries if and where @menu is exported.
 *
 * If @menu is exported, %TRUE is returned. If @connection is
 * non-%NULL then it is set to the #GDBusConnection used for
 * the export. If @object_path is non-%NULL then it is set to
 * the object path.
 *
 * If the @menu is not exported, %FALSE is returned and
 * @connection and @object_path remain unmodified.
 *
 * Returns: %TRUE if @menu was exported, else %FALSE
 */
gboolean
g_menu_model_dbus_export_query (GMenuModel       *menu,
                                GDBusConnection **connection,
                                const gchar     **object_path)
{
  GMenuExporter *exporter;

  if (exported_menus == NULL)
    return FALSE;

  exporter = g_hash_table_lookup (exported_menus, menu);
  if (exporter == NULL)
    return FALSE;

  if (connection)
    *connection = g_menu_exporter_get_connection (exporter);

  if (object_path)
    *object_path = g_menu_exporter_get_object_path (exporter);

  return TRUE;
}

/* {{{1 Epilogue */
/* vim:set foldmethod=marker: */
