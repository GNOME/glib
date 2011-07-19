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

#include "config.h"
#include "gactiongroup.h"
#include "gaction.h"
#include "glibintl.h"

/**
 * SECTION:gactiongroup
 * @title: GActionGroup
 * @short_description: A group of actions
 *
 * #GActionGroup represents a group of actions.
 *
 * Each action in the group has a unique name (which is a string).  All
 * method calls, except g_action_group_list_actions() take the name of
 * an action as an argument.
 *
 * The #GActionGroup API is meant to be the 'public' API to the action
 * group.  The calls here are exactly the interaction that 'external
 * forces' (eg: UI, incoming D-Bus messages, etc.) are supposed to have
 * with actions.  'Internal' APIs (ie: ones meant only to be accessed by
 * the action group implementation) are found on subclasses.  This is
 * why you will find -- for example -- g_action_group_get_action_enabled()
 * but not an equivalent <function>set()</function> call.
 *
 * Signals are emitted on the action group in response to state changes
 * on individual actions.
 **/

G_DEFINE_INTERFACE (GActionGroup, g_action_group, G_TYPE_OBJECT)

enum
{
  SIGNAL_ACTION_ADDED,
  SIGNAL_ACTION_REMOVED,
  SIGNAL_ACTION_ENABLED_CHANGED,
  SIGNAL_ACTION_STATE_CHANGED,
  NR_SIGNALS
};

static guint g_action_group_signals[NR_SIGNALS];

static void
g_action_group_default_init (GActionGroupInterface *class)
{
  /**
   * GActionGroup::action-added:
   * @action_group: the #GActionGroup that changed
   * @action_name: the name of the action in @action_group
   *
   * Signals that a new action was just added to the group.
   * This signal is emitted after the action has been added
   * and is now visible.
   *
   * Since: 2.28
   **/
  g_action_group_signals[SIGNAL_ACTION_ADDED] =
    g_signal_new (I_("action-added"),
                  G_TYPE_ACTION_GROUP,
                  G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                  G_STRUCT_OFFSET (GActionGroupInterface, action_added),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__STRING,
                  G_TYPE_NONE, 1,
                  G_TYPE_STRING);

  /**
   * GActionGroup::action-removed:
   * @action_group: the #GActionGroup that changed
   * @action_name: the name of the action in @action_group
   *
   * Signals that an action is just about to be removed from the group.
   * This signal is emitted before the action is removed, so the action
   * is still visible and can be queried from the signal handler.
   *
   * Since: 2.28
   **/
  g_action_group_signals[SIGNAL_ACTION_REMOVED] =
    g_signal_new (I_("action-removed"),
                  G_TYPE_ACTION_GROUP,
                  G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                  G_STRUCT_OFFSET (GActionGroupInterface, action_removed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__STRING,
                  G_TYPE_NONE, 1,
                  G_TYPE_STRING);


  /**
   * GActionGroup::action-enabled-changed:
   * @action_group: the #GActionGroup that changed
   * @action_name: the name of the action in @action_group
   * @enabled: whether the action is enabled or not
   *
   * Signals that the enabled status of the named action has changed.
   *
   * Since: 2.28
   **/
  g_action_group_signals[SIGNAL_ACTION_ENABLED_CHANGED] =
    g_signal_new (I_("action-enabled-changed"),
                  G_TYPE_ACTION_GROUP,
                  G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                  G_STRUCT_OFFSET (GActionGroupInterface,
                                   action_enabled_changed),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 2,
                  G_TYPE_STRING,
                  G_TYPE_BOOLEAN);

  /**
   * GActionGroup::action-state-changed:
   * @action_group: the #GActionGroup that changed
   * @action_name: the name of the action in @action_group
   * @value: the new value of the state
   *
   * Signals that the state of the named action has changed.
   *
   * Since: 2.28
   **/
  g_action_group_signals[SIGNAL_ACTION_STATE_CHANGED] =
    g_signal_new (I_("action-state-changed"),
                  G_TYPE_ACTION_GROUP,
                  G_SIGNAL_RUN_LAST |
                  G_SIGNAL_DETAILED |
                  G_SIGNAL_MUST_COLLECT,
                  G_STRUCT_OFFSET (GActionGroupInterface,
                                   action_state_changed),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 2,
                  G_TYPE_STRING,
                  G_TYPE_VARIANT);
}

/**
 * g_action_group_list_actions:
 * @action_group: a #GActionGroup
 *
 * Lists the actions contained within @action_group.
 *
 * The caller is responsible for freeing the list with g_strfreev() when
 * it is no longer required.
 *
 * Returns: (transfer full): a %NULL-terminated array of the names of the
 * actions in the groupb
 *
 * Since: 2.28
 **/
gchar **
g_action_group_list_actions (GActionGroup *action_group)
{
  g_return_val_if_fail (G_IS_ACTION_GROUP (action_group), NULL);

  return G_ACTION_GROUP_GET_IFACE (action_group)
    ->list_actions (action_group);
}

/**
 * g_action_group_has_action:
 * @action_group: a #GActionGroup
 * @action_name: the name of the action to check for
 *
 * Checks if the named action exists within @action_group.
 *
 * Returns: whether the named action exists
 *
 * Since: 2.28
 **/
gboolean
g_action_group_has_action (GActionGroup *action_group,
                           const gchar  *action_name)
{
  g_return_val_if_fail (G_IS_ACTION_GROUP (action_group), FALSE);

  return G_ACTION_GROUP_GET_IFACE (action_group)
    ->has_action (action_group, action_name);
}

/**
 * g_action_group_get_action_parameter_type:
 * @action_group: a #GActionGroup
 * @action_name: the name of the action to query
 *
 * Queries the type of the parameter that must be given when activating
 * the named action within @action_group.
 *
 * When activating the action using g_action_group_activate_action(),
 * the #GVariant given to that function must be of the type returned
 * by this function.
 *
 * In the case that this function returns %NULL, you must not give any
 * #GVariant, but %NULL instead.
 *
 * The parameter type of a particular action will never change but it is
 * possible for an action to be removed and for a new action to be added
 * with the same name but a different parameter type.
 *
 * Return value: the parameter type
 *
 * Since: 2.28
 **/
const GVariantType *
g_action_group_get_action_parameter_type (GActionGroup *action_group,
                                          const gchar  *action_name)
{
  g_return_val_if_fail (G_IS_ACTION_GROUP (action_group), NULL);

  return G_ACTION_GROUP_GET_IFACE (action_group)
    ->get_action_parameter_type (action_group, action_name);
}

/**
 * g_action_group_get_action_state_type:
 * @action_group: a #GActionGroup
 * @action_name: the name of the action to query
 *
 * Queries the type of the state of the named action within
 * @action_group.
 *
 * If the action is stateful then this function returns the
 * #GVariantType of the state.  All calls to
 * g_action_group_change_action_state() must give a #GVariant of this
 * type and g_action_group_get_action_state() will return a #GVariant
 * of the same type.
 *
 * If the action is not stateful then this function will return %NULL.
 * In that case, g_action_group_get_action_state() will return %NULL
 * and you must not call g_action_group_change_action_state().
 *
 * The state type of a particular action will never change but it is
 * possible for an action to be removed and for a new action to be added
 * with the same name but a different state type.
 *
 * Returns: (transfer full): the state type, if the action is stateful
 *
 * Since: 2.28
 **/
const GVariantType *
g_action_group_get_action_state_type (GActionGroup *action_group,
                                      const gchar  *action_name)
{
  g_return_val_if_fail (G_IS_ACTION_GROUP (action_group), NULL);

  return G_ACTION_GROUP_GET_IFACE (action_group)
    ->get_action_state_type (action_group, action_name);
}

/**
 * g_action_group_get_action_state_hint:
 * @action_group: a #GActionGroup
 * @action_name: the name of the action to query
 *
 * Requests a hint about the valid range of values for the state of the
 * named action within @action_group.
 *
 * If %NULL is returned it either means that the action is not stateful
 * or that there is no hint about the valid range of values for the
 * state of the action.
 *
 * If a #GVariant array is returned then each item in the array is a
 * possible value for the state.  If a #GVariant pair (ie: two-tuple) is
 * returned then the tuple specifies the inclusive lower and upper bound
 * of valid values for the state.
 *
 * In any case, the information is merely a hint.  It may be possible to
 * have a state value outside of the hinted range and setting a value
 * within the range may fail.
 *
 * The return value (if non-%NULL) should be freed with
 * g_variant_unref() when it is no longer required.
 *
 * Return value: (transfer full): the state range hint
 *
 * Since: 2.28
 **/
GVariant *
g_action_group_get_action_state_hint (GActionGroup *action_group,
                                      const gchar  *action_name)
{
  g_return_val_if_fail (G_IS_ACTION_GROUP (action_group), NULL);

  return G_ACTION_GROUP_GET_IFACE (action_group)
    ->get_action_state_hint (action_group, action_name);
}

/**
 * g_action_group_get_action_enabled:
 * @action_group: a #GActionGroup
 * @action_name: the name of the action to query
 *
 * Checks if the named action within @action_group is currently enabled.
 *
 * An action must be enabled in order to be activated or in order to
 * have its state changed from outside callers.
 *
 * Return value: whether or not the action is currently enabled
 *
 * Since: 2.28
 **/
gboolean
g_action_group_get_action_enabled (GActionGroup *action_group,
                                   const gchar  *action_name)
{
  g_return_val_if_fail (G_IS_ACTION_GROUP (action_group), FALSE);

  return G_ACTION_GROUP_GET_IFACE (action_group)
    ->get_action_enabled (action_group, action_name);
}

/**
 * g_action_group_get_action_state:
 * @action_group: a #GActionGroup
 * @action_name: the name of the action to query
 *
 * Queries the current state of the named action within @action_group.
 *
 * If the action is not stateful then %NULL will be returned.  If the
 * action is stateful then the type of the return value is the type
 * given by g_action_group_get_action_state_type().
 *
 * The return value (if non-%NULL) should be freed with
 * g_variant_unref() when it is no longer required.
 *
 * Return value: (allow-none): the current state of the action
 *
 * Since: 2.28
 **/
GVariant *
g_action_group_get_action_state (GActionGroup *action_group,
                                 const gchar  *action_name)
{
  g_return_val_if_fail (G_IS_ACTION_GROUP (action_group), NULL);

  return G_ACTION_GROUP_GET_IFACE (action_group)
    ->get_action_state (action_group, action_name);
}

/**
 * g_action_group_change_action_state:
 * @action_group: a #GActionGroup
 * @action_name: the name of the action to request the change on
 * @value: the new state
 *
 * Request for the state of the named action within @action_group to be
 * changed to @value.
 *
 * The action must be stateful and @value must be of the correct type.
 * See g_action_group_get_action_state_type().
 *
 * This call merely requests a change.  The action may refuse to change
 * its state or may change its state to something other than @value.
 * See g_action_group_get_action_state_hint().
 *
 * If the @value GVariant is floating, it is consumed.
 *
 * Since: 2.28
 **/
void
g_action_group_change_action_state (GActionGroup *action_group,
                                    const gchar  *action_name,
                                    GVariant     *value)
{
  g_return_if_fail (G_IS_ACTION_GROUP (action_group));
  g_return_if_fail (action_name != NULL);
  g_return_if_fail (value != NULL);

  G_ACTION_GROUP_GET_IFACE (action_group)
    ->change_action_state (action_group, action_name, value);
}

/**
 * g_action_group_activate_action:
 * @action_group: a #GActionGroup
 * @action_name: the name of the action to activate
 * @parameter: (allow-none): parameters to the activation
 *
 * Activate the named action within @action_group.
 *
 * If the action is expecting a parameter, then the correct type of
 * parameter must be given as @parameter.  If the action is expecting no
 * parameters then @parameter must be %NULL.  See
 * g_action_group_get_action_parameter_type().
 *
 * Since: 2.28
 **/
void
g_action_group_activate_action (GActionGroup *action_group,
                                const gchar  *action_name,
                                GVariant     *parameter)
{
  g_return_if_fail (G_IS_ACTION_GROUP (action_group));
  g_return_if_fail (action_name != NULL);

  G_ACTION_GROUP_GET_IFACE (action_group)
    ->activate_action (action_group, action_name, parameter);
}

/**
 * g_action_group_action_added:
 * @action_group: a #GActionGroup
 * @action_name: the name of an action in the group
 *
 * Emits the #GActionGroup::action-added signal on @action_group.
 *
 * This function should only be called by #GActionGroup implementations.
 *
 * Since: 2.28
 **/
void
g_action_group_action_added (GActionGroup *action_group,
                             const gchar  *action_name)
{
  g_return_if_fail (G_IS_ACTION_GROUP (action_group));
  g_return_if_fail (action_name != NULL);

  g_signal_emit (action_group,
                 g_action_group_signals[SIGNAL_ACTION_ADDED],
                 g_quark_try_string (action_name),
                 action_name);
}

/**
 * g_action_group_action_removed:
 * @action_group: a #GActionGroup
 * @action_name: the name of an action in the group
 *
 * Emits the #GActionGroup::action-removed signal on @action_group.
 *
 * This function should only be called by #GActionGroup implementations.
 *
 * Since: 2.28
 **/
void
g_action_group_action_removed (GActionGroup *action_group,
                               const gchar  *action_name)
{
  g_return_if_fail (G_IS_ACTION_GROUP (action_group));
  g_return_if_fail (action_name != NULL);

  g_signal_emit (action_group,
                 g_action_group_signals[SIGNAL_ACTION_REMOVED],
                 g_quark_try_string (action_name),
                 action_name);
}

/**
 * g_action_group_action_enabled_changed:
 * @action_group: a #GActionGroup
 * @action_name: the name of an action in the group
 * @enabled: whether or not the action is now enabled
 *
 * Emits the #GActionGroup::action-enabled-changed signal on @action_group.
 *
 * This function should only be called by #GActionGroup implementations.
 *
 * Since: 2.28
 **/
void
g_action_group_action_enabled_changed (GActionGroup *action_group,
                                       const gchar  *action_name,
                                       gboolean      enabled)
{
  g_return_if_fail (G_IS_ACTION_GROUP (action_group));
  g_return_if_fail (action_name != NULL);

  enabled = !!enabled;

  g_signal_emit (action_group,
                 g_action_group_signals[SIGNAL_ACTION_ENABLED_CHANGED],
                 g_quark_try_string (action_name),
                 action_name,
                 enabled);
}

/**
 * g_action_group_action_state_changed:
 * @action_group: a #GActionGroup
 * @action_name: the name of an action in the group
 * @state: the new state of the named action
 *
 * Emits the #GActionGroup::action-state-changed signal on @action_group.
 *
 * This function should only be called by #GActionGroup implementations.
 *
 * Since: 2.28
 **/
void
g_action_group_action_state_changed (GActionGroup *action_group,
                                     const gchar  *action_name,
                                     GVariant     *state)
{
  g_return_if_fail (G_IS_ACTION_GROUP (action_group));
  g_return_if_fail (action_name != NULL);

  g_signal_emit (action_group,
                 g_action_group_signals[SIGNAL_ACTION_STATE_CHANGED],
                 g_quark_try_string (action_name),
                 action_name,
                 state);
}
