/*
 * Copyright © 2010 Codethink Limited
 * Copyright © 2011 Canonical Limited
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2 of the licence or (at
 * your option) any later version.
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
 * Authors: Ryan Lortie <desrt@desrt.ca>
 */

#include "config.h"

#include "gsimpleasyncresult.h"
#include "gdbusactiongroup.h"
#include "gdbusconnection.h"
#include "gasyncinitable.h"
#include "gactiongroup.h"


/**
 * SECTION:gdbusactiongroup
 * @title: GDBusActionGroup
 * @short_description: A D-Bus GActionGroup implementation
 * @see_also: <link linkend="gio-GActionGroup-exporter">GActionGroup exporter</link>
 *
 * #GDBusActionGroup is an implementation of the #GActionGroup
 * interface that can be used as a proxy for an action group
 * that is exported over D-Bus with g_dbus_connection_export_action_group().
 */

struct _GDBusActionGroup
{
  GObject parent_instance;

  GDBusConnection *connection;
  gchar           *bus_name;
  gchar           *object_path;
  guint            subscription_id;
  GHashTable      *actions;

  /* The 'strict' flag indicates that the non-existence of at least one
   * action has potentially been observed through the API.  This means
   * that we should always emit 'action-added' signals for all new
   * actions.
   *
   * The user can observe the non-existence of an action by listing the
   * actions or by performing a query (such as parameter type) on a
   * non-existent action.
   *
   * If the user has no way of knowing that a given action didn't
   * already exist then we can skip emitting 'action-added' signals
   * since they have no way of knowing that it wasn't there from the
   * start.
   */
  gboolean         strict;
};

typedef GObjectClass GDBusActionGroupClass;

typedef struct
{
  gchar        *name;
  GVariantType *parameter_type;
  gboolean      enabled;
  GVariant     *state;
} ActionInfo;

static void
action_info_free (gpointer user_data)
{
  ActionInfo *info = user_data;

  g_free (info->name);

  if (info->state)
    g_variant_unref (info->state);

  if (info->parameter_type)
    g_variant_type_free (info->parameter_type);

  g_slice_free (ActionInfo, info);
}

ActionInfo *
action_info_new_from_iter (GVariantIter *iter)
{
  const gchar *param_str;
  ActionInfo *info;
  gboolean enabled;
  GVariant *state;
  gchar *name;

  if (!g_variant_iter_next (iter, "{s(bg@av)}", &name,
                            &enabled, &param_str, &state))
    return NULL;

  info = g_slice_new (ActionInfo);
  info->name = name;
  info->enabled = enabled;

  if (g_variant_n_children (state))
    g_variant_get_child (state, 0, "v", &info->state);
  else
    info->state = NULL;
  g_variant_unref (state);

  if (param_str[0])
    info->parameter_type = g_variant_type_copy ((GVariantType *) param_str);
  else
    info->parameter_type = NULL;

  return info;
}

static void g_dbus_action_group_iface_init (GActionGroupInterface *);
G_DEFINE_TYPE_WITH_CODE (GDBusActionGroup, g_dbus_action_group, G_TYPE_OBJECT,
  G_IMPLEMENT_INTERFACE (G_TYPE_ACTION_GROUP, g_dbus_action_group_iface_init))

static gchar **
g_dbus_action_group_list_actions (GActionGroup *g_group)
{
  GDBusActionGroup *group = G_DBUS_ACTION_GROUP (g_group);
  GHashTableIter iter;
  gint n, i = 0;
  gchar **keys;
  gpointer key;

  n = g_hash_table_size (group->actions);
  keys = g_new (gchar *, n + 1);

  g_hash_table_iter_init (&iter, group->actions);
  while (g_hash_table_iter_next (&iter, &key, NULL))
    keys[i++] = g_strdup (key);
  g_assert_cmpint (i, ==, n);
  keys[n] = NULL;

  group->strict = TRUE;

  return keys;
}

static gboolean
g_dbus_action_group_query_action (GActionGroup        *g_group,
                                  const gchar         *action_name,
                                  gboolean            *enabled,
                                  const GVariantType **parameter_type,
                                  const GVariantType **state_type,
                                  GVariant           **state_hint,
                                  GVariant           **state)
{
  GDBusActionGroup *group = G_DBUS_ACTION_GROUP (g_group);
  ActionInfo *info;

  info = g_hash_table_lookup (group->actions, action_name);

  if (info == NULL)
    {
      group->strict = TRUE;
      return FALSE;
    }

  if (enabled)
    *enabled = info->enabled;

  if (parameter_type)
    *parameter_type = info->parameter_type;

  if (state_type)
    *state_type = info->state ? g_variant_get_type (info->state) : NULL;

  if (state_hint)
    *state_hint = NULL;

  if (state)
    *state = info->state ? g_variant_ref (info->state) : NULL;

  return TRUE;
}

static void
g_dbus_action_group_change_state (GActionGroup *g_group,
                                  const gchar  *action_name,
                                  GVariant     *value)
{
  GDBusActionGroup *group = G_DBUS_ACTION_GROUP (g_group);

  /* Don't bother with the checks.  The other side will do it again. */
  g_dbus_connection_call (group->connection, group->bus_name, group->object_path, "org.gtk.Actions", "SetState",
                          g_variant_new ("(sva{sv})", action_name, value, NULL),
                          NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);
}

static void
g_dbus_action_group_activate (GActionGroup *g_group,
                              const gchar  *action_name,
                              GVariant     *parameter)
{
  GDBusActionGroup *group = G_DBUS_ACTION_GROUP (g_group);
  GVariantBuilder builder;

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("av"));

  if (parameter)
    g_variant_builder_add (&builder, "v", parameter);

  g_dbus_connection_call (group->connection, group->bus_name, group->object_path, "org.gtk.Actions", "Activate",
                          g_variant_new ("(sava{sv})", action_name, &builder, NULL),
                          NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);
}

static void
g_dbus_action_group_finalize (GObject *object)
{
  GDBusActionGroup *group = G_DBUS_ACTION_GROUP (object);

  g_dbus_connection_signal_unsubscribe (group->connection, group->subscription_id);
  g_hash_table_unref (group->actions);
  g_object_unref (group->connection);
  g_free (group->object_path);
  g_free (group->bus_name);

  G_OBJECT_CLASS (g_dbus_action_group_parent_class)
    ->finalize (object);
}

static void
g_dbus_action_group_init (GDBusActionGroup *group)
{
  group->actions = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, action_info_free);
}

static void
g_dbus_action_group_class_init (GDBusActionGroupClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = g_dbus_action_group_finalize;
}

static void
g_dbus_action_group_iface_init (GActionGroupInterface *iface)
{
  iface->list_actions = g_dbus_action_group_list_actions;
  iface->query_action = g_dbus_action_group_query_action;
  iface->change_action_state = g_dbus_action_group_change_state;
  iface->activate_action = g_dbus_action_group_activate;
}

static void
g_dbus_action_group_changed (GDBusConnection *connection,
                             const gchar     *sender,
                             const gchar     *object_path,
                             const gchar     *interface_name,
                             const gchar     *signal_name,
                             GVariant        *parameters,
                             gpointer         user_data)
{
  GDBusActionGroup *group = user_data;
  GActionGroup *g_group = user_data;

  if (g_str_equal (signal_name, "Changed") &&
      g_variant_is_of_type (parameters, G_VARIANT_TYPE ("(asa{sb}a{sv}a{s(bgav)})")))
    {
      /* Removes */
      {
        GVariantIter *iter;
        const gchar *name;

        g_variant_get_child (parameters, 0, "as", &iter);
        while (g_variant_iter_next (iter, "&s", &name))
          {
            if (g_hash_table_lookup (group->actions, name))
              {
                g_hash_table_remove (group->actions, name);
                g_action_group_action_removed (g_group, name);
              }
          }
        g_variant_iter_free (iter);
      }

      /* Enable changes */
      {
        GVariantIter *iter;
        const gchar *name;
        gboolean enabled;

        g_variant_get_child (parameters, 1, "a{sb}", &iter);
        while (g_variant_iter_next (iter, "{&sb}", &name, &enabled))
          {
            ActionInfo *info;

            info = g_hash_table_lookup (group->actions, name);

            if (info && info->enabled != enabled)
              {
                info->enabled = enabled;
                g_action_group_action_enabled_changed (g_group, name, enabled);
              }
          }
        g_variant_iter_free (iter);
      }

      /* State changes */
      {
        GVariantIter *iter;
        const gchar *name;
        GVariant *state;

        g_variant_get_child (parameters, 2, "a{sv}", &iter);
        while (g_variant_iter_next (iter, "{&sv}", &name, &state))
          {
            ActionInfo *info;

            info = g_hash_table_lookup (group->actions, name);

            if (info && info->state && !g_variant_equal (state, info->state) &&
                g_variant_is_of_type (state, g_variant_get_type (info->state)))
              {
                g_variant_unref (info->state);
                info->state = g_variant_ref (state);

                g_action_group_action_state_changed (g_group, name, state);
              }

            g_variant_unref (state);
          }
        g_variant_iter_free (iter);
      }

      /* Additions */
      {
        GVariantIter *iter;
        ActionInfo *info;

        g_variant_get_child (parameters, 3, "a{s(bgav)}", &iter);
        while ((info = action_info_new_from_iter (iter)))
          {
            if (!g_hash_table_lookup (group->actions, info->name))
              {
                g_hash_table_insert (group->actions, info->name, info);

                if (group->strict)
                  g_action_group_action_added (g_group, info->name);
              }
            else
              action_info_free (info);
          }
      }
    }
}

static void
g_dbus_action_group_describe_all_done (GObject      *source,
                                       GAsyncResult *result,
                                       gpointer      user_data)
{
  GSimpleAsyncResult *my_result = user_data;
  GDBusActionGroup *group;
  GError *error = NULL;
  GVariant *reply;

  group = g_simple_async_result_get_op_res_gpointer (my_result);
  g_assert (G_IS_DBUS_ACTION_GROUP (group));
  g_assert (group->connection == (gpointer) source);
  reply = g_dbus_connection_call_finish (group->connection, result, &error);

  if (reply != NULL)
    {
      GVariantIter *iter;
      ActionInfo *action;

      g_variant_get (reply, "(a{s(bgav)})", &iter);
      while ((action = action_info_new_from_iter (iter)))
        g_hash_table_insert (group->actions, action->name, action);
      g_variant_iter_free (iter);
      g_variant_unref (reply);
    }
  else
    {
      g_simple_async_result_set_from_error (my_result, error);
      g_error_free (error);
    }

  g_simple_async_result_complete (my_result);
  g_object_unref (my_result);
}

/**
 * g_dbus_action_group_new:
 * @connection: A #GDBusConnection
 * @bus_name: the bus name which exports the action group
 * @object_path: the object path at which the action group is exported
 * @cancellable: A #GCancellable or %NULL
 * @callback: Callback function to invoke when the object is ready
 * @user_data: User data to pass to @callback
 *
 * Creates a new, empty, #GDBusActionGroup.
 *
 * This is a failable asynchronous constructor - when the object
 * is ready, @callback will be invoked and you can use
 * g_dbus_action_group_new_finish() to get the result.
 *
 * See g_dbus_action_group_new_sync() and for a synchronous version
 * of this constructor.
 */
void
g_dbus_action_group_new (GDBusConnection       *connection,
                         const gchar           *bus_name,
                         const gchar           *object_path,
                         GCancellable          *cancellable,
                         GAsyncReadyCallback    callback,
                         gpointer               user_data)
{
  GSimpleAsyncResult *result;
  GDBusActionGroup *group;

  group = g_object_new (G_TYPE_DBUS_ACTION_GROUP, NULL);
  group->connection = g_object_ref (connection);
  group->bus_name = g_strdup (bus_name);
  group->object_path = g_strdup (object_path);

  /* It's probably excessive to worry about watching the name ownership.
   * The person using this class will know for themselves when the name
   * disappears.
   */

  result = g_simple_async_result_new (G_OBJECT (group), callback, user_data, g_dbus_action_group_new);
  g_simple_async_result_set_op_res_gpointer (result, group, g_object_unref);

  group->subscription_id =
    g_dbus_connection_signal_subscribe (connection, bus_name, "org.gtk.Actions", "Changed", object_path, NULL,
                                        G_DBUS_SIGNAL_FLAGS_NONE, g_dbus_action_group_changed, group, NULL);

  g_dbus_connection_call (connection, bus_name, object_path, "org.gtk.Actions", "DescribeAll", NULL,
                          G_VARIANT_TYPE ("(a{s(bgav)})"), G_DBUS_CALL_FLAGS_NONE, -1, cancellable,
                          g_dbus_action_group_describe_all_done, result);
}

/**
 * g_dbus_action_group_new_finish:
 * @res: A #GAsyncResult obtained from the #GAsyncReadyCallback
 *     function passed to g_dbus_action_group_new()
 * @error: Return location for error or %NULL
 *
 * Finishes creating a #GDBusActionGroup.
 *
 * Returns: A #GDBusProxy or %NULL if @error is set. Free with g_object_unref().
 */
GDBusActionGroup *
g_dbus_action_group_new_finish (GAsyncResult  *result,
                                GError       **error)
{
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (result);

  g_return_val_if_fail (g_simple_async_result_is_valid (result,
                                                        g_simple_async_result_get_op_res_gpointer (simple),
                                                        g_dbus_action_group_new), NULL);

  if (g_simple_async_result_propagate_error (simple, error))
    return NULL;

  return g_object_ref (g_simple_async_result_get_op_res_gpointer (simple));
}

/**
 * g_dbus_action_group_new_sync:
 * @connection: A #GDBusConnection
 * @bus_name: the bus name which exports the action group
 * @object_path: the object path at which the action group is exported
 * @cancellable: A #GCancellable or %NULL
 * @error: Return location for error or %NULL
 *
 * This is a synchronous failable constructor. See g_dbus_action_group_new()
 * and g_dbus_action_group_new_finish() for the asynchronous version.
 *
 * Returns: A #GDBusProxy or %NULL if @error is set. Free with g_object_unref().
 */
GDBusActionGroup *
g_dbus_action_group_new_sync (GDBusConnection        *connection,
                              const gchar            *bus_name,
                              const gchar            *object_path,
                              GCancellable           *cancellable,
                              GError                **error)
{
  GDBusActionGroup *group;
  GVariant *reply;

  group = g_object_new (G_TYPE_DBUS_ACTION_GROUP, NULL);
  group->connection = g_object_ref (connection);
  group->bus_name = g_strdup (bus_name);
  group->object_path = g_strdup (object_path);

  group->subscription_id =
    g_dbus_connection_signal_subscribe (connection, bus_name, "org.gtk.Actions", "Changed", object_path, NULL,
                                          G_DBUS_SIGNAL_FLAGS_NONE, g_dbus_action_group_changed, group, NULL);

  reply = g_dbus_connection_call_sync (connection, bus_name, object_path, "org.gtk.Actions",
                                       "DescribeAll", NULL, G_VARIANT_TYPE ("(a{s(bgav)})"),
                                       G_DBUS_CALL_FLAGS_NONE, -1, cancellable, error);

  if (reply != NULL)
    {
      GVariantIter *iter;
      ActionInfo *action;

      g_variant_get (reply, "(a{s(bgav)})", &iter);
      while ((action = action_info_new_from_iter (iter)))
        g_hash_table_insert (group->actions, action->name, action);
      g_variant_iter_free (iter);
      g_variant_unref (reply);
    }
  else
    {
      g_object_unref (group);
      return NULL;
    }

  return group;
}
