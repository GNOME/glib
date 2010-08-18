/*
 * Copyright © 2010 Codethink Limited
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

#include "gsimpleactiongroup.h"
#include "gaction.h"

/**
 * SECTION:gsimpleactiongroup
 * @title: GSimpleActionGroup
 * @short_description: a simple #GActionGroup implementation
 *
 * #GSimpleActionGroup is a hash table filled with #GAction objects,
 * implementing the #GActionGroup interface.
 **/

struct _GSimpleActionGroupPrivate
{
  GHashTable *table;  /* string -> GAction */
};

G_DEFINE_TYPE (GSimpleActionGroup, g_simple_action_group, G_TYPE_ACTION_GROUP)

static gchar **
g_simple_action_group_list_actions (GActionGroup *group)
{
  GSimpleActionGroup *simple = G_SIMPLE_ACTION_GROUP (group);
  GHashTableIter iter;
  gint n, i = 0;
  gchar **keys;
  gpointer key;

  n = g_hash_table_size (simple->priv->table);
  keys = g_new (gchar *, n + 1);

  g_hash_table_iter_init (&iter, simple->priv->table);
  while (g_hash_table_iter_next (&iter, &key, NULL))
    keys[i++] = g_strdup (key);
  g_assert_cmpint (i, ==, n);
  keys[n] = NULL;

  return keys;
}

static gboolean
g_simple_action_group_has_action (GActionGroup *group,
                                  const gchar  *action_name)
{
  GSimpleActionGroup *simple = G_SIMPLE_ACTION_GROUP (group);

  return g_hash_table_lookup (simple->priv->table, action_name) != NULL;
}

static const GVariantType *
g_simple_action_group_get_parameter_type (GActionGroup *group,
                                          const gchar  *action_name)
{
  GSimpleActionGroup *simple = G_SIMPLE_ACTION_GROUP (group);
  GAction *action;

  action = g_hash_table_lookup (simple->priv->table, action_name);

  if (action == NULL)
    return NULL;

  return g_action_get_parameter_type (action);
}

static const GVariantType *
g_simple_action_group_get_state_type (GActionGroup *group,
                                      const gchar  *action_name)
{
  GSimpleActionGroup *simple = G_SIMPLE_ACTION_GROUP (group);
  GAction *action;

  action = g_hash_table_lookup (simple->priv->table, action_name);

  if (action == NULL)
    return NULL;

  return g_action_get_state_type (action);
}

static GVariant *
g_simple_action_group_get_state_hint (GActionGroup *group,
                                       const gchar  *action_name)
{
  GSimpleActionGroup *simple = G_SIMPLE_ACTION_GROUP (group);
  GAction *action;

  action = g_hash_table_lookup (simple->priv->table, action_name);

  if (action == NULL)
    return NULL;

  return g_action_get_state_hint (action);
}

static gboolean
g_simple_action_group_get_enabled (GActionGroup *group,
                                   const gchar  *action_name)
{
  GSimpleActionGroup *simple = G_SIMPLE_ACTION_GROUP (group);
  GAction *action;

  action = g_hash_table_lookup (simple->priv->table, action_name);

  if (action == NULL)
    return FALSE;

  return g_action_get_enabled (action);
}

static GVariant *
g_simple_action_group_get_state (GActionGroup *group,
                                 const gchar  *action_name)
{
  GSimpleActionGroup *simple = G_SIMPLE_ACTION_GROUP (group);
  GAction *action;

  action = g_hash_table_lookup (simple->priv->table, action_name);

  if (action == NULL)
    return NULL;

  return g_action_get_state (action);
}

static void
g_simple_action_group_set_state (GActionGroup *group,
                                 const gchar  *action_name,
                                 GVariant     *value)
{
  GSimpleActionGroup *simple = G_SIMPLE_ACTION_GROUP (group);
  GAction *action;

  action = g_hash_table_lookup (simple->priv->table, action_name);

  if (action == NULL)
    return;

  return g_action_set_state (action, value);
}

static void
g_simple_action_group_activate (GActionGroup *group,
                                const gchar  *action_name,
                                GVariant     *parameter)
{
  GSimpleActionGroup *simple = G_SIMPLE_ACTION_GROUP (group);
  GAction *action;

  action = g_hash_table_lookup (simple->priv->table, action_name);

  if (action == NULL)
    return;

  return g_action_activate (action, parameter);
}

static void
action_enabled_changed (GAction  *action,
                        gboolean  enabled,
                        gpointer  user_data)
{
  g_action_group_action_enabled_changed (user_data,
                                         g_action_get_name (action),
                                         enabled);
}

static void
action_state_changed (GAction  *action,
                      GVariant *value,
                      gpointer  user_data)
{
  g_action_group_action_state_changed (user_data,
                                       g_action_get_name (action),
                                       value);
}

static void
g_simple_action_group_disconnect (gpointer key,
                                  gpointer value,
                                  gpointer user_data)
{
  g_signal_handlers_disconnect_by_func (value, action_enabled_changed,
                                        user_data);
  g_signal_handlers_disconnect_by_func (value, action_state_changed,
                                        user_data);
}

static void
g_simple_action_group_finalize (GObject *object)
{
  GSimpleActionGroup *simple = G_SIMPLE_ACTION_GROUP (object);

  g_hash_table_foreach (simple->priv->table,
                        g_simple_action_group_disconnect,
                        simple);
  g_hash_table_unref (simple->priv->table);

  G_OBJECT_CLASS (g_simple_action_group_parent_class)
    ->finalize (object);
}

static void
g_simple_action_group_init (GSimpleActionGroup *simple)
{
  simple->priv = G_TYPE_INSTANCE_GET_PRIVATE (simple,
                                              G_TYPE_SIMPLE_ACTION_GROUP,
                                              GSimpleActionGroupPrivate);
  simple->priv->table = g_hash_table_new_full (g_str_hash, g_str_equal,
                                               g_free, g_object_unref);
}

static void
g_simple_action_group_class_init (GSimpleActionGroupClass *class)
{
  GActionGroupClass *group_class = G_ACTION_GROUP_CLASS (class);
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = g_simple_action_group_finalize;

  group_class->list_actions = g_simple_action_group_list_actions;
  group_class->has_action = g_simple_action_group_has_action;
  group_class->get_parameter_type = g_simple_action_group_get_parameter_type;
  group_class->get_state_type = g_simple_action_group_get_state_type;
  group_class->get_state_hint = g_simple_action_group_get_state_hint;
  group_class->get_enabled = g_simple_action_group_get_enabled;
  group_class->get_state = g_simple_action_group_get_state;
  group_class->set_state = g_simple_action_group_set_state;
  group_class->activate = g_simple_action_group_activate;

  g_type_class_add_private (class, sizeof (GSimpleActionGroupPrivate));
}

/**
 * g_simple_action_group_new:
 *
 * Creates a new, empty, #GSimpleActionGroup.
 *
 * Returns: a new #GSimpleActionGroup
 *
 * Since: 2.26
 **/
GSimpleActionGroup *
g_simple_action_group_new (void)
{
  return g_object_new (G_TYPE_SIMPLE_ACTION_GROUP, NULL);
}

/**
 * g_simple_action_group_lookup:
 * @simple: a #GSimpleActionGroup
 * @action_name: the name of an action
 *
 * Looks up the action with the name @action_name in the group.
 *
 * If no such action exists, returns %NULL.
 *
 * Returns: (transfer none): a #GAction, or %NULL
 *
 * Since: 2.26
 **/
GAction *
g_simple_action_group_lookup (GSimpleActionGroup *simple,
                              const gchar        *action_name)
{
  g_return_val_if_fail (G_IS_SIMPLE_ACTION_GROUP (simple), NULL);

  return g_hash_table_lookup (simple->priv->table, action_name);
}

/**
 * g_simple_action_group_insert:
 * @simple: a #GSimpleActionGroup
 * @action: a #GAction
 *
 * Adds an action to the action group.
 *
 * If the action group already contains an action with the same name as
 * @action then the old action is dropped from the group.
 *
 * The action group takes its own reference on @action.
 *
 * Since: 2.26
 **/
void
g_simple_action_group_insert (GSimpleActionGroup *simple,
                              GAction            *action)
{
  const gchar *action_name;
  GAction *old_action;

  g_return_if_fail (G_IS_SIMPLE_ACTION_GROUP (simple));
  g_return_if_fail (G_IS_ACTION (action));

  action_name = g_action_get_name (action);
  old_action = g_hash_table_lookup (simple->priv->table, action_name);

  if (old_action != action)
    {
      if (old_action != NULL)
        {
          g_action_group_action_removed (G_ACTION_GROUP (simple),
                                         action_name);
          g_simple_action_group_disconnect (NULL, old_action, simple);
        }

      g_signal_connect (action, "notify::enabled",
                        G_CALLBACK (action_enabled_changed), simple);

      if (g_action_get_state_type (action) != NULL)
        g_signal_connect (action, "notify::state",
                          G_CALLBACK (action_state_changed), simple);

      g_hash_table_insert (simple->priv->table,
                           g_strdup (action_name),
                           g_object_ref (action));

      g_action_group_action_added (G_ACTION_GROUP (simple), action_name);
    }
}

/**
 * g_simple_action_group_remove:
 * @simple: a #GSimpleActionGroup
 * @action_name: the name of the action
 *
 * Removes the named action from the action group.
 *
 * If no action of this name is in the group then nothing happens.
 *
 * Since: 2.26
 **/
void
g_simple_action_group_remove (GSimpleActionGroup *simple,
                              const gchar        *action_name)
{
  GAction *action;

  g_return_if_fail (G_IS_SIMPLE_ACTION_GROUP (simple));

  action = g_hash_table_lookup (simple->priv->table, action_name);

  if (action != NULL)
    {
      g_action_group_action_removed (G_ACTION_GROUP (simple), action_name);
      g_simple_action_group_disconnect (NULL, action, simple);
      g_hash_table_remove (simple->priv->table, action_name);
    }
}

/**
 * g_simple_action_group_set_enabled:
 * @simple: a #GSimpleActionGroup
 * @action_name: the name of an action
 * @enabled: if the action should be enabled
 *
 * Sets an action in the group as being enabled or not.
 *
 * This is a convenience function, equivalent to calling
 * g_simple_action_group_lookup() and using g_action_set_enabled() on
 * the result.
 *
 * If no action named @action_name exists then this function does
 * nothing.
 *
 * Since: 2.26
 **/
void
g_simple_action_group_set_enabled (GSimpleActionGroup *simple,
                                   const gchar        *action_name,
                                   gboolean            enabled)
{
  GAction *action;

  g_return_if_fail (G_IS_SIMPLE_ACTION_GROUP (simple));

  action = g_hash_table_lookup (simple->priv->table, action_name);

  if (action != NULL)
    g_action_set_enabled (action, enabled);
}
