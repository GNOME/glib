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

#include "gsimpleaction.h"
#include "gaction.h"

/**
 * SECTION:gsimpleactiongroup
 * @title: GSimpleActionGroup
 * @short_description: A simple GActionGroup implementation
 *
 * #GSimpleActionGroup is a hash table filled with #GAction objects,
 * implementing the #GActionGroup interface.
 **/

struct _GSimpleActionGroupPrivate
{
  GHashTable *table;  /* string -> GAction */
};

static void g_simple_action_group_iface_init (GActionGroupInterface *);
G_DEFINE_TYPE_WITH_CODE (GSimpleActionGroup,
  g_simple_action_group, G_TYPE_OBJECT,
  G_IMPLEMENT_INTERFACE (G_TYPE_ACTION_GROUP,
    g_simple_action_group_iface_init))

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
g_simple_action_group_change_state (GActionGroup *group,
                                    const gchar  *action_name,
                                    GVariant     *value)
{
  GSimpleActionGroup *simple = G_SIMPLE_ACTION_GROUP (group);
  GAction *action;

  action = g_hash_table_lookup (simple->priv->table, action_name);

  if (action == NULL)
    return;

  g_action_change_state (action, value);
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

  g_action_activate (action, parameter);
}

static void
action_enabled_notify (GAction     *action,
                       GParamSpec  *pspec,
                       gpointer     user_data)
{
  g_action_group_action_enabled_changed (user_data,
                                         g_action_get_name (action),
                                         g_action_get_enabled (action));
}

static void
action_state_notify (GAction    *action,
                     GParamSpec *pspec,
                     gpointer    user_data)
{
  GVariant *value;

  value = g_action_get_state (action);
  g_action_group_action_state_changed (user_data,
                                       g_action_get_name (action),
                                       value);
  g_variant_unref (value);
}

static void
g_simple_action_group_disconnect (gpointer key,
                                  gpointer value,
                                  gpointer user_data)
{
  g_signal_handlers_disconnect_by_func (value, action_enabled_notify,
                                        user_data);
  g_signal_handlers_disconnect_by_func (value, action_state_notify,
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
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = g_simple_action_group_finalize;

  g_type_class_add_private (class, sizeof (GSimpleActionGroupPrivate));
}

static void
g_simple_action_group_iface_init (GActionGroupInterface *iface)
{
  iface->list_actions = g_simple_action_group_list_actions;
  iface->has_action = g_simple_action_group_has_action;
  iface->get_action_parameter_type = g_simple_action_group_get_parameter_type;
  iface->get_action_state_type = g_simple_action_group_get_state_type;
  iface->get_action_state_hint = g_simple_action_group_get_state_hint;
  iface->get_action_enabled = g_simple_action_group_get_enabled;
  iface->get_action_state = g_simple_action_group_get_state;
  iface->change_action_state = g_simple_action_group_change_state;
  iface->activate_action = g_simple_action_group_activate;
}

/**
 * g_simple_action_group_new:
 *
 * Creates a new, empty, #GSimpleActionGroup.
 *
 * Returns: a new #GSimpleActionGroup
 *
 * Since: 2.28
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
 * Since: 2.28
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
 * Since: 2.28
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
                        G_CALLBACK (action_enabled_notify), simple);

      if (g_action_get_state_type (action) != NULL)
        g_signal_connect (action, "notify::state",
                          G_CALLBACK (action_state_notify), simple);

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
 * Since: 2.28
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
 * GActionEntry:
 * @name: the name of the action
 * @activate: the callback to connect to the "activate" signal of the
 *            action
 * @parameter_type: the type of the parameter that must be passed to the
 *                  activate function for this action, given as a single
 *                  GVariant type string (or %NULL for no parameter)
 * @state: the initial state for this action, given in GVariant text
 *         format.  The state is parsed with no extra type information,
 *         so type tags must be added to the string if they are
 *         necessary.
 * @change_state: the callback to connect to the "change-state" signal
 *                of the action
 *
 * This struct defines a single action.  It is for use with
 * g_simple_action_group_add_entries().
 *
 * The order of the items in the structure are intended to reflect
 * frequency of use.  It is permissible to use an incomplete initialiser
 * in order to leave some of the later values as %NULL.  All values
 * after @name are optional.  Additional optional fields may be added in
 * the future.
 *
 * See g_simple_action_group_add_entries() for an example.
 **/

/**
 * g_simple_action_group_add_entries:
 * @simple: a #GSimpleActionGroup
 * @entries: a pointer to the first item in an array of #GActionEntry
 *           structs
 * @n_entries: the length of @entries, or -1
 * @user_data: the user data for signal connections
 *
 * A convenience function for creating multiple #GSimpleAction instances
 * and adding them to the action group.
 *
 * Each action is constructed as per one #GActionEntry.
 *
 * <example>
 * <title>Using g_simple_action_group_add_entries()</title>
 * <programlisting>
 * static void
 * activate_quit (GSimpleAction *simple,
 *                GVariant      *parameter,
 *                gpointer       user_data)
 * {
 *   exit (0);
 * }
 *
 * static void
 * activate_print_string (GSimpleAction *simple,
 *                        GVariant      *parameter,
 *                        gpointer       user_data)
 * {
 *   g_print ("%s\n", g_variant_get_string (parameter, NULL));
 * }
 *
 * static GActionGroup *
 * create_action_group (void)
 * {
 *   const GActionEntry entries[] = {
 *     { "quit",         activate_quit              },
 *     { "print-string", activate_print_string, "s" }
 *   };
 *   GSimpleActionGroup *group;
 *
 *   group = g_simple_action_group_new ();
 *   g_simple_action_group_add_entries (group, entries, G_N_ELEMENTS (entries), NULL);
 *
 *   return G_ACTION_GROUP (group);
 * }
 * </programlisting>
 * </example>
 *
 * Since: 2.30
 **/
void
g_simple_action_group_add_entries (GSimpleActionGroup *simple,
                                   const GActionEntry *entries,
                                   gint                n_entries,
                                   gpointer            user_data)
{
  gint i;

  g_return_if_fail (G_IS_SIMPLE_ACTION_GROUP (simple));
  g_return_if_fail (entries != NULL || n_entries == 0);

  for (i = 0; n_entries == -1 ? entries[i].name != NULL : i < n_entries; i++)
    {
      const GActionEntry *entry = &entries[i];
      const GVariantType *parameter_type;
      GSimpleAction *action;

      if (entry->parameter_type)
        {
          if (!g_variant_type_string_is_valid (entry->parameter_type))
            {
              g_critical ("g_simple_action_group_add_entries: the type "
                          "string '%s' given as the parameter type for "
                          "action '%s' is not a valid GVariant type "
                          "string.  This action will not be added.",
                          entry->parameter_type, entry->name);
              return;
            }

          parameter_type = G_VARIANT_TYPE (entry->parameter_type);
        }
      else
        parameter_type = NULL;

      if (entry->state)
        {
          GError *error = NULL;
          GVariant *state;

          state = g_variant_parse (NULL, entry->state, NULL, NULL, &error);
          if (state == NULL)
            {
              g_critical ("g_simple_action_group_add_entries: GVariant could "
                          "not parse the state value given for action '%s' "
                          "('%s'): %s.  This action will not be added.",
                          entry->name, entry->state, error->message);
              g_error_free (error);
              continue;
            }

          action = g_simple_action_new_stateful (entry->name,
                                                 parameter_type,
                                                 state);

          g_variant_unref (state);
        }
      else
        {
          action = g_simple_action_new (entry->name,
                                        parameter_type);
        }

      if (entry->activate != NULL)
        g_signal_connect (action, "activate",
                          G_CALLBACK (entry->activate), user_data);

      if (entry->change_state != NULL)
        g_signal_connect (action, "change-state",
                          G_CALLBACK (entry->change_state), user_data);

      g_simple_action_group_insert (simple, G_ACTION (action));
      g_object_unref (action);
    }
}
