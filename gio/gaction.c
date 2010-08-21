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
#include "gaction.h"
#include "glibintl.h"

G_DEFINE_TYPE (GAction, g_action, G_TYPE_OBJECT)

/**
 * SECTION:gaction
 * @title: GAction
 * @short_description: an action
 *
 * #GAction represents a single named action.
 *
 * The main interface to an action is that it can be activated with
 * g_action_activate().  This results in the 'activate' signal being
 * emitted.  An activation has a #GVariant parameter (which may be
 * %NULL).  The correct type for the parameter is determined by a static
 * parameter type (which is given at construction time).
 *
 * An action may optionally have a state, in which case the state may be
 * set with g_action_set_state().  This call takes a #GVariant.  The
 * correct type for the state is determined by a static state type
 * (which is given at construction time).
 *
 * The state may have a hint associated with it, specifying its valid
 * range.
 *
 * #GAction is intended to be used both as a simple action class and as
 * a base class for more complicated action types.  The base class
 * itself supports activation and state.  Not supported are state hints
 * and filtering requests to set the state based on the requested value.
 * You should subclass if you require either of these abilities.
 *
 * In all cases, the base class is responsible for storing the name of
 * the action, the parameter type, the enabled state, the optional state
 * type and the state and emitting the appropriate signals when these
 * change.  The base class is also responsbile for filtering calls to
 * g_action_activate() and g_action_set_state() for type safety and for
 * the state being enabled.
 *
 * Probably the only useful thing to do with a #GAction is to put it
 * inside of a #GSimpleActionGroup.
 **/

struct _GActionPrivate
{
  gchar        *name;
  GVariantType *parameter_type;
  guint         enabled : 1;
  guint         state_set : 1;
  GVariant     *state;
};

enum
{
  PROP_NONE,
  PROP_NAME,
  PROP_PARAMETER_TYPE,
  PROP_ENABLED,
  PROP_STATE_TYPE,
  PROP_STATE
};

enum
{
  SIGNAL_ACTIVATE,
  NR_SIGNALS
};

static guint g_action_signals[NR_SIGNALS];

static void
g_action_real_set_state (GAction  *action,
                         GVariant *value)
{
  if (action->priv->state == value)
    return;

  if (!action->priv->state || !g_variant_equal (action->priv->state, value))
    {
      if (action->priv->state)
        g_variant_unref (action->priv->state);

      action->priv->state = g_variant_ref (value);

      g_object_notify (G_OBJECT (action), "state");
    }
}

static GVariant *
g_action_real_get_state_hint (GAction *action)
{
  return NULL;
}

static void
g_action_set_property (GObject *object, guint prop_id,
                       const GValue *value, GParamSpec *pspec)
{
  GAction *action = G_ACTION (object);

  switch (prop_id)
    {
    case PROP_NAME:
      g_assert (action->priv->name == NULL);
      action->priv->name = g_value_dup_string (value);
      break;

    case PROP_PARAMETER_TYPE:
      g_assert (action->priv->parameter_type == NULL);
      action->priv->parameter_type = g_value_dup_boxed (value);
      break;

    case PROP_ENABLED:
      g_action_set_enabled (action, g_value_get_boolean (value));
      break;

    case PROP_STATE:
      /* PROP_STATE is marked as G_PARAM_CONSTRUCT so we always get a
       * call during object construction, even if it is NULL.  We treat
       * that first call differently, for a number of reasons.
       *
       * First, we don't want the value to be rejected by the
       * possibly-overridden .set_state() function.  Second, we don't
       * want to be tripped by the assertions in g_action_set_state()
       * that would enforce the catch22 that we only provide a value of
       * the same type as the existing value (when there is not yet an
       * existing value).
       */
      if (action->priv->state_set)
        g_action_set_state (action, g_value_get_variant (value));

      else /* this is the special case */
        {
          /* only do it the first time. */
          action->priv->state_set = TRUE;

          /* blindly set it. */
          action->priv->state = g_value_dup_variant (value);
        }
      break;

    default:
      g_assert_not_reached ();
    }
}

static void
g_action_get_property (GObject *object, guint prop_id,
                       GValue *value, GParamSpec *pspec)
{
  GAction *action = G_ACTION (object);

  switch (prop_id)
    {
    case PROP_NAME:
      g_value_set_string (value, g_action_get_name (action));
      break;

    case PROP_PARAMETER_TYPE:
      g_value_set_boxed (value, g_action_get_parameter_type (action));
      break;

    case PROP_ENABLED:
      g_value_set_boolean (value, g_action_get_enabled (action));
      break;

    case PROP_STATE_TYPE:
      g_value_set_boxed (value, g_action_get_state_type (action));
      break;

    case PROP_STATE:
      g_value_set_variant (value, g_action_get_state (action));
      break;

    default:
      g_assert_not_reached ();
    }
}

static void
g_action_finalize (GObject *object)
{
  GAction *action = G_ACTION (object);

  g_free (action->priv->name);
  if (action->priv->parameter_type)
    g_variant_type_free (action->priv->parameter_type);
  if (action->priv->state)
    g_variant_unref (action->priv->state);

  G_OBJECT_CLASS (g_action_parent_class)
    ->finalize (object);
}

void
g_action_init (GAction *action)
{
  action->priv = G_TYPE_INSTANCE_GET_PRIVATE (action,
                                              G_TYPE_ACTION,
                                              GActionPrivate);
}

void
g_action_class_init (GActionClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  class->get_state_hint = g_action_real_get_state_hint;
  class->set_state = g_action_real_set_state;

  object_class->get_property = g_action_get_property;
  object_class->set_property = g_action_set_property;
  object_class->finalize = g_action_finalize;

  /**
   * GAction::activate:
   * @action: the #GAction
   * @parameter: (allow-none): the parameter to the activation
   *
   * Indicates that the action was just activated.
   *
   * @parameter will always be of the expected type.  In the event that
   * an incorrect type was given, no signal will be emitted.
   *
   * Since: 2.26
   */
  g_action_signals[SIGNAL_ACTIVATE] =
    g_signal_new (I_("activate"),
                  G_TYPE_ACTION,
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GActionClass, activate),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VARIANT,
                  G_TYPE_NONE, 1,
                  G_TYPE_VARIANT);

  /**
   * GAction:name:
   *
   * The name of the action.  This is mostly meaningful for identifying
   * the action once it has been added to a #GActionGroup.
   *
   * Since: 2.26
   **/
  g_object_class_install_property (object_class, PROP_NAME,
                                   g_param_spec_string ("name",
                                                        P_("Action Name"),
                                                        P_("The name used to invoke the action"),
                                                        NULL,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));

  /**
   * GAction:parameter-type:
   *
   * The type of the parameter that must be given when activating the
   * action.
   *
   * Since: 2.26
   **/
  g_object_class_install_property (object_class, PROP_PARAMETER_TYPE,
                                   g_param_spec_boxed ("parameter-type",
                                                       P_("Parameter Type"),
                                                       P_("The type of GVariant passed to activate()"),
                                                       G_TYPE_VARIANT_TYPE,
                                                       G_PARAM_READWRITE |
                                                       G_PARAM_CONSTRUCT_ONLY |
                                                       G_PARAM_STATIC_STRINGS));

  /**
   * GAction:enabled:
   *
   * If @action is currently enabled.
   *
   * If the action is disabled then calls to g_action_activate() and
   * g_action_set_state() have no effect.
   *
   * Since: 2.26
   **/
  g_object_class_install_property (object_class, PROP_ENABLED,
                                   g_param_spec_boolean ("enabled",
                                                         P_("Enabled"),
                                                         P_("If the action can be activated"),
                                                         TRUE,
                                                         G_PARAM_CONSTRUCT |
                                                         G_PARAM_READWRITE |
                                                         G_PARAM_STATIC_STRINGS));

  /**
   * GAction:state-type:
   *
   * The #GVariantType of the state that the action has, or %NULL if the
   * action is stateless.
   *
   * Since: 2.26
   **/
  g_object_class_install_property (object_class, PROP_STATE_TYPE,
                                   g_param_spec_boxed ("state-type",
                                                       P_("State Type"),
                                                       P_("The type of the state kept by the action"),
                                                       G_TYPE_VARIANT_TYPE,
                                                       G_PARAM_READABLE |
                                                       G_PARAM_STATIC_STRINGS));

  /**
   * GAction:state:
   *
   * The state of the action, or %NULL if the action is stateless.
   *
   * Since: 2.26
   **/
  g_object_class_install_property (object_class, PROP_STATE,
                                   g_param_spec_variant ("state",
                                                         P_("State"),
                                                         P_("The state the action is in"),
                                                         G_VARIANT_TYPE_ANY,
                                                         NULL,
                                                         G_PARAM_CONSTRUCT |
                                                         G_PARAM_READWRITE |
                                                         G_PARAM_STATIC_STRINGS));

  g_type_class_add_private (class, sizeof (GActionPrivate));
}

/**
 * g_action_set_state:
 * @action: a #GAction
 * @value: the new state
 *
 * Request for the state of @action to be changed to @value.
 *
 * The action must be stateful and @value must be of the correct type.
 * See g_action_get_state_type().
 *
 * This call merely requests a change.  The action may refuse to change
 * its state or may change its state to something other than @value.
 * See g_action_get_state_hint().
 *
 * Since: 2.26
 **/
void
g_action_set_state (GAction  *action,
                    GVariant *value)
{
  const GVariantType *state_type;

  g_return_if_fail (G_IS_ACTION (action));
  g_return_if_fail (value != NULL);
  state_type = g_action_get_state_type (action);
  g_return_if_fail (state_type != NULL);
  g_return_if_fail (g_variant_is_of_type (value, state_type));

  g_variant_ref_sink (value);

  if (action->priv->enabled)
    G_ACTION_GET_CLASS (action)
      ->set_state (action, value);

  g_variant_unref (value);
}

/**
 * g_action_get_state:
 * @action: a #GAction
 *
 * Queries the current state of @action.
 *
 * If the action is not stateful then %NULL will be returned.  If the
 * action is stateful then the type of the return value is the type
 * given by g_action_get_state_type().
 *
 * Returns: (allow-none) (transfer none): the current state of the action
 *
 * Since: 2.26
 **/
GVariant *
g_action_get_state (GAction *action)
{
  g_return_val_if_fail (G_IS_ACTION (action), NULL);

  return action->priv->state;
}

/**
 * g_action_get_name:
 * @action: a #GAction
 *
 * Queries the name of @action.
 *
 * Returns: the name of the action
 *
 * Since: 2.26
 **/
const gchar *
g_action_get_name (GAction *action)
{
  g_return_val_if_fail (G_IS_ACTION (action), NULL);

  return action->priv->name;
}

/**
 * g_action_get_parameter_type:
 * @action: a #GAction
 *
 * Queries the type of the parameter that must be given when activating
 * @action.
 *
 * When activating the action using g_action_activate(), the #GVariant
 * given to that function must be of the type returned by this function.
 *
 * In the case that this function returns %NULL, you must not give any
 * #GVariant, but %NULL instead.
 *
 * Returns: (allow-none): the parameter type
 *
 * Since: 2.26
 **/
const GVariantType *
g_action_get_parameter_type (GAction *action)
{
  g_return_val_if_fail (G_IS_ACTION (action), NULL);

  return action->priv->parameter_type;
}

/**
 * g_action_get_state_type:
 * @action: a #GAction
 *
 * Queries the type of the state of @action.
 *
 * If the action is stateful (ie: was created with
 * g_action_new_stateful()) then this function returns the #GVariantType
 * of the state.  This is the type of the initial value given as the
 * state.  All calls to g_action_set_state() must give a #GVariant of
 * this type and g_action_get_state() will return a #GVariant of the
 * same type.
 *
 * If the action is not stateful (ie: created with g_action_new()) then
 * this function will return %NULL.  In that case, g_action_get_state()
 * will return %NULL and you must not call g_action_set_state().
 *
 * Returns: (allow-none): the state type, if the action is stateful
 *
 * Since: 2.26
 **/
const GVariantType *
g_action_get_state_type (GAction *action)
{
  g_return_val_if_fail (G_IS_ACTION (action), NULL);

  if (action->priv->state != NULL)
    return g_variant_get_type (action->priv->state);
  else
    return NULL;
}

/**
 * g_action_get_state_hint:
 * @action: a #GAction
 *
 * Requests a hint about the valid range of values for the state of
 * @action.
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
 * Returns: (allow-none): the state range hint
 *
 * Since: 2.26
 **/
GVariant *
g_action_get_state_hint (GAction *action)
{
  g_return_val_if_fail (G_IS_ACTION (action), NULL);

  return G_ACTION_GET_CLASS (action)->get_state_hint (action);
}

/**
 * g_action_get_enabled:
 * @action: a #GAction
 *
 * Checks if @action is currently enabled.
 *
 * An action must be enabled in order to be activated or in order to
 * have its state changed from outside callers.
 *
 * Returns: whether the action is enabled
 *
 * Since: 2.26
 **/
gboolean
g_action_get_enabled (GAction *action)
{
  g_return_val_if_fail (G_IS_ACTION (action), FALSE);

  return action->priv->enabled;
}

/**
 * g_action_set_enabled:
 * @action: a #GAction
 * @enabled: whether the action is enabled
 *
 * Sets the action as enabled or not.
 *
 * An action must be enabled in order to be activated or in order to
 * have its state changed from outside callers.
 *
 * Since: 2.26
 **/
void
g_action_set_enabled (GAction  *action,
                      gboolean  enabled)
{
  g_return_if_fail (G_IS_ACTION (action));

  enabled = !!enabled;

  if (action->priv->enabled != enabled)
    {
      action->priv->enabled = enabled;
      g_object_notify (G_OBJECT (action), "enabled");
    }
}

/**
 * g_action_activate:
 * @action: a #GAction
 * @parameter: the parameter to the activation
 *
 * Activates the action.
 *
 * @parameter must be the correct type of parameter for the action (ie:
 * the parameter type given at construction time).  If the parameter
 * type was %NULL then @parameter must also be %NULL.
 *
 * Since: 2.26
 **/
void
g_action_activate (GAction  *action,
                   GVariant *parameter)
{
  g_return_if_fail (G_IS_ACTION (action));

  g_return_if_fail (action->priv->parameter_type == NULL ?
                      parameter == NULL :
                    (parameter != NULL &&
                     g_variant_is_of_type (parameter,
                                           action->priv->parameter_type)));

  if (parameter != NULL)
    g_variant_ref_sink (parameter);

  if (action->priv->enabled)
    g_signal_emit (action, g_action_signals[SIGNAL_ACTIVATE], 0, parameter);

  if (parameter != NULL)
    g_variant_unref (parameter);
}

/**
 * g_action_new:
 * @name: the name of the action
 * @parameter_type: the type of parameter to the activate function
 *
 * Creates a new action.
 *
 * The created action is stateless.  See g_action_new_stateful().
 *
 * Returns: a new #GAction
 *
 * Since: 2.26
 **/
GAction *
g_action_new (const gchar        *name,
              const GVariantType *parameter_type)
{
  return g_object_new (G_TYPE_ACTION,
                       "name", name,
                       "parameter-type", parameter_type,
                       NULL);
}

/**
 * g_action_new_stateful:
 * @name: the name of the action
 * @parameter_type: the type of the parameter to the activate function
 * @state: the initial state of the action
 *
 * Creates a new stateful action.
 *
 * @state is the initial state of the action.  All future state values
 * must have the same #GVariantType as the initial state.
 *
 * Returns: a new #GAction
 *
 * Since: 2.26
 **/
GAction *
g_action_new_stateful (const gchar        *name,
                       const GVariantType *parameter_type,
                       GVariant           *state)
{
  return g_object_new (G_TYPE_ACTION,
                       "name", name,
                       "parameter-type", parameter_type,
                       "state", state,
                       NULL);
}
