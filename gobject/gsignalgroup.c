/* GObject - GLib Type, Object, Parameter and Signal Library
 *
 * Copyright (C) 2015-2022 Christian Hergert <christian@hergert.me>
 * Copyright (C) 2015 Garrett Regier <garrettregier@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"
#include "glib.h"
#include "glibintl.h"

#include "gparamspecs.h"
#include "gsignalgroup.h"
#include "gvaluetypes.h"

/**
 * GSignalGroup:
 *
 * `GSignalGroup` manages a collection of signals on a `GObject`.
 *
 * `GSignalGroup` simplifies the process of connecting  many signals to a `GObject`
 * as a group. As such there is no API to disconnect a signal from the group.
 *
 * In particular, this allows you to:
 *
 *  - Change the target instance, which automatically causes disconnection
 *    of the signals from the old instance and connecting to the new instance.
 *  - Block and unblock signals as a group
 *  - Ensuring that blocked state transfers across target instances.
 *
 * One place you might want to use such a structure is with `GtkTextView` and
 * `GtkTextBuffer`. Often times, you'll need to connect to many signals on
 * `GtkTextBuffer` from a `GtkTextView` subclass. This allows you to create a
 * signal group during instance construction, simply bind the
 * `GtkTextView:buffer` property to `GSignalGroup:target` and connect
 * all the signals you need. When the `GtkTextView:buffer` property changes
 * all of the signals will be transitioned correctly.
 *
 * Since: 2.72
 */

typedef struct
{
  gatomicrefcount ref_count;
  GRecMutex mutex;
  GSignalGroup *self;
} SyncData;

typedef struct
{
  SyncData *sync_data;

  /* Note that we don't own a strong reference to @target. However,
   * we have a g_object_weak_ref() notification that guards this object.
   * This gives us some guarantees about the object still being alive.
   *
   * So, while we hold target->sync_data->mutex:
   * - if another thread destroyes the object, it will invoke the weak
   *   notification and wait to obtain the mutex.
   * - note that target->sync_data->mutex is a GRecMutex. So if you are in a
   *   function that holds the mutex and you check whether we have a target,
   *   then must must be careful to not call something (e.g. user callbacks)
   *   that can trigger an unref of the target. But we anyway must not call
   *   user code while holding the mutex. */
  GObject *target;
} TargetData;

struct _GSignalGroup
{
  GObject     parent_instance;

  SyncData   *sync_data;
  TargetData *target_data;
  GPtrArray  *handlers;
  GType       target_type;
  gssize      block_count;

  guint       has_bound_at_least_once : 1;
};

typedef struct _GSignalGroupClass
{
  GObjectClass parent_class;

  void (*bind) (GSignalGroup *self,
                GObject      *target);
} GSignalGroupClass;

typedef struct
{
  GSignalGroup *group;
  gulong             handler_id;
  GClosure          *closure;
  guint              signal_id;
  GQuark             signal_detail;
  guint              connect_after : 1;
} SignalHandler;

G_DEFINE_TYPE (GSignalGroup, g_signal_group, G_TYPE_OBJECT)

typedef enum
{
  PROP_TARGET = 1,
  PROP_TARGET_TYPE,
  LAST_PROP
} GSignalGroupProperty;

enum
{
  BIND,
  UNBIND,
  LAST_SIGNAL
};

static GParamSpec *properties[LAST_PROP];
static guint signals[LAST_SIGNAL];

static void g_signal_group__target_weak_notify (gpointer data,
                                                GObject *where_object_was);

static SyncData *
sync_data_new (GSignalGroup *self)
{
  SyncData *sync_data;

  sync_data = g_new (SyncData, 1);
  *sync_data = (SyncData) {
    .self = self,
  };
  g_atomic_ref_count_init (&sync_data->ref_count);
  g_rec_mutex_init (&sync_data->mutex);

  return sync_data;
}

static SyncData *
sync_data_ref (SyncData *sync_data)
{
  g_atomic_ref_count_inc (&sync_data->ref_count);
  return sync_data;
}

static void
sync_data_unref (SyncData *sync_data)
{
  if (g_atomic_ref_count_dec (&sync_data->ref_count))
    {
      g_rec_mutex_clear (&sync_data->mutex);
      g_free_sized (sync_data, sizeof (SyncData));
    }
}

static TargetData *
target_data_new (SyncData *sync_data, GObject *target)
{
  TargetData *target_data;

  target_data = g_new (TargetData, 1);
  *target_data = (TargetData) {
    .sync_data = sync_data_ref (sync_data),
    .target = target,
  };

  return target_data;
}

static void
target_data_free (TargetData *target_data)
{
  sync_data_unref (target_data->sync_data);
  g_free_sized (target_data, sizeof (TargetData));
}

static void
target_data_weakunref (TargetData *target_data)
{
  GObject *target = target_data->target;

  target_data->target = NULL;
  g_object_weak_unref_full (target,
                            g_signal_group__target_weak_notify,
                            target_data,
                            TRUE);
}

static void
g_signal_group_set_target_type (GSignalGroup *self,
                                GType         target_type)
{
  g_assert (G_IS_SIGNAL_GROUP (self));
  g_assert (g_type_is_a (target_type, G_TYPE_OBJECT));

  self->target_type = target_type;

  /* The class must be created at least once for the signals
   * to be registered, otherwise g_signal_parse_name() will fail
   */
  if (G_TYPE_IS_INTERFACE (target_type))
    {
      if (g_type_default_interface_peek (target_type) == NULL)
        g_type_default_interface_unref (g_type_default_interface_ref (target_type));
    }
  else
    {
      if (g_type_class_peek (target_type) == NULL)
        g_type_class_unref (g_type_class_ref (target_type));
    }
}

static void
g_signal_group_gc_handlers (GSignalGroup *self)
{
  guint i;

  g_assert (G_IS_SIGNAL_GROUP (self));

  /*
   * Remove any handlers for which the closures have become invalid. We do
   * this cleanup lazily to avoid situations where we could have disposal
   * active on both the signal group and the peer object.
   */

  for (i = self->handlers->len; i > 0; i--)
    {
      const SignalHandler *handler = g_ptr_array_index (self->handlers, i - 1);

      g_assert (handler != NULL);
      g_assert (handler->closure != NULL);

      if (handler->closure->is_invalid)
        g_ptr_array_remove_index (self->handlers, i - 1);
    }
}

static void
g_signal_group__target_weak_notify (gpointer  data,
                                    GObject  *where_object_was)
{
  TargetData *target_data = data;
  GSignalGroup *self;
  guint i;

  g_rec_mutex_lock (&target_data->sync_data->mutex);

  if (!target_data->target)
    {
      g_rec_mutex_unlock (&target_data->sync_data->mutex);
      return;
    }

#ifdef G_ENABLE_DEBUG
  g_assert (target_data->target == where_object_was);
  g_assert (target_data->sync_data->self->target_data == target_data);
#endif

  target_data->target = NULL;

  self = target_data->sync_data->self;

  self->target_data = NULL;

  for (i = 0; i < self->handlers->len; i++)
    {
      SignalHandler *handler = g_ptr_array_index (self->handlers, i);

      handler->handler_id = 0;
    }

  g_signal_emit (self, signals[UNBIND], 0);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TARGET]);

  g_rec_mutex_unlock (&self->sync_data->mutex);
}

static void
g_signal_group_bind_handler (GSignalGroup  *self,
                             SignalHandler *handler,
                             GObject       *target)
{
  gssize i;

  g_assert (self != NULL);
  g_assert (G_IS_OBJECT (target));
  g_assert (handler != NULL);
  g_assert (handler->signal_id != 0);
  g_assert (handler->closure != NULL);
  g_assert (handler->closure->is_invalid == 0);
  g_assert (handler->handler_id == 0);

  handler->handler_id = g_signal_connect_closure_by_id (target,
                                                        handler->signal_id,
                                                        handler->signal_detail,
                                                        handler->closure,
                                                        handler->connect_after);

  g_assert (handler->handler_id != 0);

  for (i = 0; i < self->block_count; i++)
    g_signal_handler_block (target, handler->handler_id);
}

static void
g_signal_group_bind (GSignalGroup *self,
                     GObject      *target)
{
  GObject *hold;
  guint i;

  g_assert (G_IS_SIGNAL_GROUP (self));
  g_assert (!target || G_IS_OBJECT (target));

  if (target == NULL)
    return;

  self->has_bound_at_least_once = TRUE;

  hold = g_object_ref (target);

  g_clear_pointer (&self->target_data, target_data_weakunref);

  self->target_data = target_data_new (self->sync_data, hold);

  g_object_weak_ref_full (hold,
                          g_signal_group__target_weak_notify,
                          self->target_data,
                          (GDestroyNotify) target_data_free);

  g_signal_group_gc_handlers (self);

  for (i = 0; i < self->handlers->len; i++)
    {
      SignalHandler *handler = g_ptr_array_index (self->handlers, i);

      g_signal_group_bind_handler (self, handler, hold);
    }

  g_signal_emit (self, signals [BIND], 0, hold);

  g_object_unref (hold);
}

static void
g_signal_group_unbind (GSignalGroup *self)
{
  GObject *target = NULL;
  guint i;

  g_return_if_fail (G_IS_SIGNAL_GROUP (self));

  /*
   * Target may be NULL by this point, as we got notified of its destruction.
   * However, if we're early enough, we may get a full reference back and can
   * cleanly disconnect our connections.
   */

  if (self->target_data)
    {
      target = self->target_data->target;
      /*
       * Let go of our weak reference now that we have a full reference
       * for the life of this function.
       */
      g_clear_pointer (&self->target_data, target_data_weakunref);
    }

  g_signal_group_gc_handlers (self);

  for (i = 0; i < self->handlers->len; i++)
    {
      SignalHandler *handler;
      gulong handler_id;

      handler = g_ptr_array_index (self->handlers, i);

      g_assert (handler != NULL);
      g_assert (handler->signal_id != 0);
      g_assert (handler->closure != NULL);

      handler_id = handler->handler_id;
      handler->handler_id = 0;

      /*
       * If @target is NULL, we lost a race to cleanup the weak
       * instance and the signal connections have already been
       * finalized and therefore nothing to do.
       *
       * Note that we don't hold a strong reference on @target. But at this point
       * we still can be sure that @target is alive, because:
       * - if another thread were to destroy GObject, then it would need to invoke
       *   the weak notification, which would wait on sync_data->mutex.
       * - as sync_data->mutex is a GRecMutex, the current thread could destroy
       *   @target. But if you review the code between where we get the pointer
       *   to @target and here, you see that we don't call external code or don't do
       *   anything that could cause @target to be destroyed.
       */

      if (target != NULL && handler_id != 0)
        g_signal_handler_disconnect (target, handler_id);
    }

  g_signal_emit (self, signals [UNBIND], 0);
}

static gboolean
g_signal_group_check_target_type (GSignalGroup *self,
                                  gpointer      target)
{
  if ((target != NULL) &&
      !g_type_is_a (G_OBJECT_TYPE (target), self->target_type))
    {
      g_critical ("Failed to set GSignalGroup of target type %s "
                  "using target %p of type %s",
                  g_type_name (self->target_type),
                  target, G_OBJECT_TYPE_NAME (target));
      return FALSE;
    }

  return TRUE;
}

/**
 * g_signal_group_block:
 * @self: the #GSignalGroup
 *
 * Blocks all signal handlers managed by @self so they will not
 * be called during any signal emissions. Must be unblocked exactly
 * the same number of times it has been blocked to become active again.
 *
 * This blocked state will be kept across changes of the target instance.
 *
 * Since: 2.72
 */
void
g_signal_group_block (GSignalGroup *self)
{
  guint i;

  g_return_if_fail (G_IS_SIGNAL_GROUP (self));
  g_return_if_fail (self->block_count >= 0);

  g_rec_mutex_lock (&self->sync_data->mutex);

  self->block_count++;

  if (!self->target_data)
    goto unlock;

  for (i = 0; i < self->handlers->len; i++)
    {
      const SignalHandler *handler = g_ptr_array_index (self->handlers, i);

      g_assert (handler != NULL);
      g_assert (handler->signal_id != 0);
      g_assert (handler->closure != NULL);
      g_assert (handler->handler_id != 0);

      g_signal_handler_block (self->target_data->target,
                              handler->handler_id);
    }

unlock:
  g_rec_mutex_unlock (&self->sync_data->mutex);
}

/**
 * g_signal_group_unblock:
 * @self: the #GSignalGroup
 *
 * Unblocks all signal handlers managed by @self so they will be
 * called again during any signal emissions unless it is blocked
 * again. Must be unblocked exactly the same number of times it
 * has been blocked to become active again.
 *
 * Since: 2.72
 */
void
g_signal_group_unblock (GSignalGroup *self)
{
  guint i;

  g_return_if_fail (G_IS_SIGNAL_GROUP (self));
  g_return_if_fail (self->block_count > 0);

  g_rec_mutex_lock (&self->sync_data->mutex);

  self->block_count--;

  if (!self->target_data)
    goto unlock;

  for (i = 0; i < self->handlers->len; i++)
    {
      const SignalHandler *handler = g_ptr_array_index (self->handlers, i);

      g_assert (handler != NULL);
      g_assert (handler->signal_id != 0);
      g_assert (handler->closure != NULL);
      g_assert (handler->handler_id != 0);

      g_signal_handler_unblock (self->target_data->target,
                                handler->handler_id);
    }

unlock:
  g_rec_mutex_unlock (&self->sync_data->mutex);
}

/**
 * g_signal_group_dup_target:
 * @self: the #GSignalGroup
 *
 * Gets the target instance used when connecting signals.
 *
 * Returns: (nullable) (transfer full) (type GObject): The target instance
 *
 * Since: 2.72
 */
gpointer
g_signal_group_dup_target (GSignalGroup *self)
{
  GObject *target;

  g_return_val_if_fail (G_IS_SIGNAL_GROUP (self), NULL);

  g_rec_mutex_lock (&self->sync_data->mutex);
  target = self->target_data
               ? g_object_ref (self->target_data->target)
               : NULL;
  g_rec_mutex_unlock (&self->sync_data->mutex);

  return target;
}

/**
 * g_signal_group_set_target:
 * @self: the #GSignalGroup.
 * @target: (nullable) (type GObject) (transfer none): The target instance used
 *     when connecting signals.
 *
 * Sets the target instance used when connecting signals. Any signal
 * that has been registered with g_signal_group_connect_object() or
 * similar functions will be connected to this object.
 *
 * If the target instance was previously set, signals will be
 * disconnected from that object prior to connecting to @target.
 *
 * Since: 2.72
 */
void
g_signal_group_set_target (GSignalGroup *self,
                           gpointer      target)
{
  g_return_if_fail (G_IS_SIGNAL_GROUP (self));

  g_rec_mutex_lock (&self->sync_data->mutex);

  if ((self->target_data ? self->target_data->target : NULL) == target)
    goto cleanup;

  if (!g_signal_group_check_target_type (self, target))
    goto cleanup;

  if (self->has_bound_at_least_once)
    g_signal_group_unbind (self);

  g_signal_group_bind (self, target);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TARGET]);

cleanup:
  g_rec_mutex_unlock (&self->sync_data->mutex);
}

static void
signal_handler_free (gpointer data)
{
  SignalHandler *handler = data;

  if (handler->closure != NULL)
    g_closure_invalidate (handler->closure);

  handler->handler_id = 0;
  handler->signal_id = 0;
  handler->signal_detail = 0;
  g_clear_pointer (&handler->closure, g_closure_unref);
  g_slice_free (SignalHandler, handler);
}

static void
g_signal_group_constructed (GObject *object)
{
  GSignalGroup *self = (GSignalGroup *)object;
  GObject *target;

  g_rec_mutex_lock (&self->sync_data->mutex);

  target = self->target_data
               ? g_object_ref (self->target_data->target)
               : NULL;

  if (!g_signal_group_check_target_type (self, target))
    g_signal_group_set_target (self, NULL);

  G_OBJECT_CLASS (g_signal_group_parent_class)->constructed (object);

  g_clear_object (&target);

  g_rec_mutex_unlock (&self->sync_data->mutex);
}

static void
g_signal_group_dispose (GObject *object)
{
  GSignalGroup *self = (GSignalGroup *)object;

  g_rec_mutex_lock (&self->sync_data->mutex);

  g_signal_group_gc_handlers (self);

  if (self->has_bound_at_least_once)
    g_signal_group_unbind (self);

  g_ptr_array_set_size (self->handlers, 0);

  g_rec_mutex_unlock (&self->sync_data->mutex);

  G_OBJECT_CLASS (g_signal_group_parent_class)->dispose (object);
}

static void
g_signal_group_finalize (GObject *object)
{
  GSignalGroup *self = (GSignalGroup *)object;

  sync_data_unref (self->sync_data);
  g_ptr_array_unref (self->handlers);

  G_OBJECT_CLASS (g_signal_group_parent_class)->finalize (object);
}

static void
g_signal_group_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  GSignalGroup *self = G_SIGNAL_GROUP (object);

  switch ((GSignalGroupProperty) prop_id)
    {
    case PROP_TARGET:
      g_value_take_object (value, g_signal_group_dup_target (self));
      break;

    case PROP_TARGET_TYPE:
      g_value_set_gtype (value, self->target_type);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
g_signal_group_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  GSignalGroup *self = G_SIGNAL_GROUP (object);

  switch ((GSignalGroupProperty) prop_id)
    {
    case PROP_TARGET:
      g_signal_group_set_target (self, g_value_get_object (value));
      break;

    case PROP_TARGET_TYPE:
      g_signal_group_set_target_type (self, g_value_get_gtype (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
g_signal_group_class_init (GSignalGroupClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = g_signal_group_constructed;
  object_class->dispose = g_signal_group_dispose;
  object_class->finalize = g_signal_group_finalize;
  object_class->get_property = g_signal_group_get_property;
  object_class->set_property = g_signal_group_set_property;

  /**
   * GSignalGroup:target
   *
   * The target instance used when connecting signals.
   *
   * Since: 2.72
   */
  properties[PROP_TARGET] =
      g_param_spec_object ("target", NULL, NULL,
                           G_TYPE_OBJECT,
                           (G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));

  /**
   * GSignalGroup:target-type
   *
   * The #GType of the target property.
   *
   * Since: 2.72
   */
  properties[PROP_TARGET_TYPE] =
      g_param_spec_gtype ("target-type", NULL, NULL,
                          G_TYPE_OBJECT,
                          (G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, LAST_PROP, properties);

  /**
   * GSignalGroup::bind:
   * @self: the #GSignalGroup
   * @instance: a #GObject containing the new value for #GSignalGroup:target
   *
   * This signal is emitted when #GSignalGroup:target is set to a new value
   * other than %NULL. It is similar to #GObject::notify on `target` except it
   * will not emit when #GSignalGroup:target is %NULL and also allows for
   * receiving the #GObject without a data-race.
   *
   * Since: 2.72
   */
  signals[BIND] =
      g_signal_new ("bind",
                    G_TYPE_FROM_CLASS (klass),
                    G_SIGNAL_RUN_LAST,
                    0,
                    NULL, NULL, NULL,
                    G_TYPE_NONE,
                    1,
                    G_TYPE_OBJECT);

  /**
   * GSignalGroup::unbind:
   * @self: a #GSignalGroup
   *
   * This signal is emitted when the target instance of @self is set to a
   * new #GObject.
   *
   * This signal will only be emitted if the previous target of @self is
   * non-%NULL.
   *
   * Since: 2.72
   */
  signals[UNBIND] =
      g_signal_new ("unbind",
                    G_TYPE_FROM_CLASS (klass),
                    G_SIGNAL_RUN_LAST,
                    0,
                    NULL, NULL, NULL,
                    G_TYPE_NONE,
                    0);
}

static void
g_signal_group_init (GSignalGroup *self)
{
  self->sync_data = sync_data_new (self);
  self->handlers = g_ptr_array_new_with_free_func (signal_handler_free);
  self->target_type = G_TYPE_OBJECT;
}

/**
 * g_signal_group_new:
 * @target_type: the #GType of the target instance.
 *
 * Creates a new #GSignalGroup for target instances of @target_type.
 *
 * Returns: (transfer full): a new #GSignalGroup
 *
 * Since: 2.72
 */
GSignalGroup *
g_signal_group_new (GType target_type)
{
  g_return_val_if_fail (g_type_is_a (target_type, G_TYPE_OBJECT), NULL);

  return g_object_new (G_TYPE_SIGNAL_GROUP,
                       "target-type", target_type,
                       NULL);
}

static gboolean
g_signal_group_connect_closure_ (GSignalGroup   *self,
                                 const gchar    *detailed_signal,
                                 GClosure       *closure,
                                 gboolean        after)
{
  SignalHandler *handler;
  guint signal_id;
  GQuark signal_detail;

  g_return_val_if_fail (G_IS_SIGNAL_GROUP (self), FALSE);
  g_return_val_if_fail (detailed_signal != NULL, FALSE);
  g_return_val_if_fail (closure != NULL, FALSE);

  if (!g_signal_parse_name (detailed_signal, self->target_type,
                            &signal_id, &signal_detail, TRUE))
    {
      g_critical ("Invalid signal name “%s”", detailed_signal);
      return FALSE;
    }

  g_rec_mutex_lock (&self->sync_data->mutex);

  if (self->has_bound_at_least_once)
    {
      g_critical ("Cannot add signals after setting target");
      g_rec_mutex_unlock (&self->sync_data->mutex);
      return FALSE;
    }

  handler = g_slice_new0 (SignalHandler);
  handler->group = self;
  handler->signal_id = signal_id;
  handler->signal_detail = signal_detail;
  handler->closure = g_closure_ref (closure);
  handler->connect_after = (guint) after;

  g_closure_sink (closure);

  g_ptr_array_add (self->handlers, handler);

  if (self->target_data)
    {
      GObject *target;

      target = g_object_ref (self->target_data->target);
      g_signal_group_bind_handler (self, handler, target);
      g_object_unref (target);
    }

  /* Lazily remove any old handlers on connect */
  g_signal_group_gc_handlers (self);

  g_rec_mutex_unlock (&self->sync_data->mutex);
  return TRUE;
}

/**
 * g_signal_group_connect_closure:
 * @self: a #GSignalGroup
 * @detailed_signal: a string of the form `signal-name` with optional `::signal-detail`
 * @closure: (not nullable): the closure to connect.
 * @after: whether the handler should be called before or after the
 *  default handler of the signal.
 *
 * Connects @closure to the signal @detailed_signal on #GSignalGroup:target.
 *
 * You cannot connect a signal handler after #GSignalGroup:target has been set.
 *
 * Since: 2.74
 */
void
g_signal_group_connect_closure (GSignalGroup   *self,
                                const gchar    *detailed_signal,
                                GClosure       *closure,
                                gboolean        after)
{
  g_signal_group_connect_closure_ (self, detailed_signal, closure, after);
}

static void
g_signal_group_connect_full (GSignalGroup   *self,
                             const gchar    *detailed_signal,
                             GCallback       c_handler,
                             gpointer        data,
                             GClosureNotify  notify,
                             GConnectFlags   flags,
                             gboolean        is_object)
{
  GClosure *closure;

  g_return_if_fail (c_handler != NULL);
  g_return_if_fail (!is_object || G_IS_OBJECT (data));

  if ((flags & G_CONNECT_SWAPPED) != 0)
    closure = g_cclosure_new_swap (c_handler, data, notify);
  else
    closure = g_cclosure_new (c_handler, data, notify);

  if (is_object)
    {
      /* Set closure->is_invalid when data is disposed. We only track this to avoid
       * reconnecting in the future. However, we do a round of cleanup when ever we
       * connect a new object or the target changes to GC the old handlers.
       */
      g_object_watch_closure (data, closure);
    }

  if (!g_signal_group_connect_closure_ (self,
                                        detailed_signal,
                                        closure,
                                        (flags & G_CONNECT_AFTER) != 0))
    g_closure_unref (closure);
}

/**
 * g_signal_group_connect_object: (skip)
 * @self: a #GSignalGroup
 * @detailed_signal: a string of the form `signal-name` with optional `::signal-detail`
 * @c_handler: (scope notified): the #GCallback to connect
 * @object: (not nullable) (transfer none): the #GObject to pass as data to @c_handler calls
 * @flags: #GConnectFlags for the signal connection
 *
 * Connects @c_handler to the signal @detailed_signal on #GSignalGroup:target.
 *
 * Ensures that the @object stays alive during the call to @c_handler
 * by temporarily adding a reference count. When the @object is destroyed
 * the signal handler will automatically be removed.
 *
 * You cannot connect a signal handler after #GSignalGroup:target has been set.
 *
 * Since: 2.72
 */
void
g_signal_group_connect_object (GSignalGroup  *self,
                               const gchar   *detailed_signal,
                               GCallback      c_handler,
                               gpointer       object,
                               GConnectFlags  flags)
{
  g_return_if_fail (G_IS_OBJECT (object));

  g_signal_group_connect_full (self, detailed_signal, c_handler, object, NULL,
                               flags, TRUE);
}

/**
 * g_signal_group_connect_data:
 * @self: a #GSignalGroup
 * @detailed_signal: a string of the form "signal-name::detail"
 * @c_handler: (scope notified) (closure data) (destroy notify): the #GCallback to connect
 * @data: the data to pass to @c_handler calls
 * @notify: function to be called when disposing of @self
 * @flags: the flags used to create the signal connection
 *
 * Connects @c_handler to the signal @detailed_signal
 * on the target instance of @self.
 *
 * You cannot connect a signal handler after #GSignalGroup:target has been set.
 *
 * Since: 2.72
 */
void
g_signal_group_connect_data (GSignalGroup   *self,
                             const gchar    *detailed_signal,
                             GCallback       c_handler,
                             gpointer        data,
                             GClosureNotify  notify,
                             GConnectFlags   flags)
{
  g_signal_group_connect_full (self, detailed_signal, c_handler, data, notify,
                               flags, FALSE);
}

/**
 * g_signal_group_connect: (skip)
 * @self: a #GSignalGroup
 * @detailed_signal: a string of the form "signal-name::detail"
 * @c_handler: (scope notified): the #GCallback to connect
 * @data: the data to pass to @c_handler calls
 *
 * Connects @c_handler to the signal @detailed_signal
 * on the target instance of @self.
 *
 * You cannot connect a signal handler after #GSignalGroup:target has been set.
 *
 * Since: 2.72
 */
void
g_signal_group_connect (GSignalGroup *self,
                        const gchar  *detailed_signal,
                        GCallback     c_handler,
                        gpointer      data)
{
  g_signal_group_connect_full (self, detailed_signal, c_handler, data, NULL,
                               0, FALSE);
}

/**
 * g_signal_group_connect_after: (skip)
 * @self: a #GSignalGroup
 * @detailed_signal: a string of the form "signal-name::detail"
 * @c_handler: (scope notified): the #GCallback to connect
 * @data: the data to pass to @c_handler calls
 *
 * Connects @c_handler to the signal @detailed_signal
 * on the target instance of @self.
 *
 * The @c_handler will be called after the default handler of the signal.
 *
 * You cannot connect a signal handler after #GSignalGroup:target has been set.
 *
 * Since: 2.72
 */
void
g_signal_group_connect_after (GSignalGroup *self,
                              const gchar  *detailed_signal,
                              GCallback     c_handler,
                              gpointer      data)
{
  g_signal_group_connect_full (self, detailed_signal, c_handler,
                               data, NULL, G_CONNECT_AFTER, FALSE);
}

/**
 * g_signal_group_connect_swapped:
 * @self: a #GSignalGroup
 * @detailed_signal: a string of the form "signal-name::detail"
 * @c_handler: (scope async): the #GCallback to connect
 * @data: the data to pass to @c_handler calls
 *
 * Connects @c_handler to the signal @detailed_signal
 * on the target instance of @self.
 *
 * The instance on which the signal is emitted and @data
 * will be swapped when calling @c_handler.
 *
 * You cannot connect a signal handler after #GSignalGroup:target has been set.
 *
 * Since: 2.72
 */
void
g_signal_group_connect_swapped (GSignalGroup *self,
                                const gchar  *detailed_signal,
                                GCallback     c_handler,
                                gpointer      data)
{
  g_signal_group_connect_full (self, detailed_signal, c_handler, data, NULL,
                               G_CONNECT_SWAPPED, FALSE);
}
