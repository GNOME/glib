/*
 * Copyright © 2010 Codethink Limited
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

#include "config.h"

#include "gperiodic.h"

#include "gio-marshal.h"

/**
 * SECTION:gperiodic
 * @title: GPeriodic
 * @short_description: a periodic event clock
 *
 * #GPeriodic is a periodic event clock that fires a configurable number
 * of times per second and is capable of being put into synch with an
 * external time source.
 *
 * A number of #GPeriodicTickFunc<!-- -->s are registered with
 * g_periodic_add() and are called each time the clock "ticks".
 *
 * The tick functions can report "damage" (ie: updates that need to be
 * performed) that are handled in a "repair" phase that follows all the
 * tick functions having been run.  It is also possible to report damage
 * while the clock is not running, in which case the rate of repairs
 * will be rate limited as if the clock were running.
 *
 * #GPeriodic is intended to be used as a paint clock for managing
 * geometry updates and painting of windows.
 *
 * Since: 2.28
 **/

/**
 * GPeriodic:
 *
 * #GPeriodic is an opaque structure type.
 *
 * Since: 2.28
 **/

typedef GObjectClass GPeriodicClass;

typedef struct
{
  GPeriodicTickFunc callback;
  gpointer          user_data;
  GDestroyNotify    notify;
  guint             id;
} GPeriodicTick;

typedef struct
{
  GPeriodicRepairFunc callback;
  gpointer            user_data;
  GDestroyNotify      notify;
} GPeriodicRepair;

typedef struct
{
  GSource    source;
  GPeriodic *periodic;
} GPeriodicSource;

struct _GPeriodic
{
  GObject  parent_instance;
  GSource *source;            /* GPeriodicSource */
  guint64  last_run;
  guint    blocked;
  guint    hz;

  GSList  *ticks;             /* List<GPeriodicTick> */
  GSList  *repairs;           /* List<GPeriodicRepair> */

  /* debug */
  guint    in_tick   : 1;
  guint    in_repair : 1;
};

G_DEFINE_TYPE (GPeriodic, g_periodic, G_TYPE_OBJECT)

#define PERIODIC_FROM_SOURCE(src) (((GPeriodicSource *) (src))->periodic)

enum
{
  PROP_NONE,
  PROP_HZ,
  PROP_PRIORITY
};

static guint g_periodic_tick;
static guint g_periodic_repair;

static guint64
g_periodic_get_microticks (GPeriodic *periodic)
{
  guint64 microticks;
  GTimeSpec timespec;

  g_source_get_time (periodic->source, &timespec);

  microticks = timespec.tv_sec;
  microticks *= 1000000;
  microticks += timespec.tv_nsec / 1000;
  microticks *= periodic->hz;

  return microticks;
}

static void
g_periodic_run (GPeriodic *periodic)
{
  g_assert (periodic->blocked == 0);

  if (periodic->ticks)
    {
      guint64 microseconds;
      GSList *iter;

      microseconds = periodic->last_run / periodic->hz;

      periodic->in_tick = TRUE;
      for (iter = periodic->ticks; iter; iter = iter->next)
        {
          GPeriodicTick *tick = iter->data;

          tick->callback (periodic, microseconds, tick->user_data);
        }
      g_signal_emit (periodic, g_periodic_tick, 0, microseconds);
      periodic->in_tick = FALSE;
    }

  g_assert (periodic->blocked == 0);

  if (periodic->repairs)
    {
      periodic->in_repair = TRUE;
      while (periodic->repairs)
        {
          GPeriodicRepair *repair = periodic->repairs->data;

          repair->callback (periodic, repair->user_data);

          periodic->repairs = g_slist_remove (periodic->repairs, repair);

          if (repair->notify)
            repair->notify (repair->user_data);

          g_slice_free (GPeriodicRepair, repair);
        }
      g_signal_emit (periodic, g_periodic_repair, 0);
      periodic->in_repair = FALSE;
    }
}

static gboolean
g_periodic_prepare (GSource *source,
                    gint    *timeout)
{
  GPeriodic *periodic = PERIODIC_FROM_SOURCE (source);

  if ((periodic->ticks || periodic->repairs) && !periodic->blocked)
    /* We need to run. */
    {
      gint64 remaining;
     
      remaining = periodic->last_run + 1000000 -
                  g_periodic_get_microticks (periodic);

      if (remaining > 0)
        /* It's too soon.  Wait some more before running. */
        {
          /* Round-up the timeout.
           *
           * If we round down, then we will quite often wake to discover
           * that not enough time has passed and want to sleep again, so
           * save ourselves the future bother.
           */
          *timeout = (remaining + periodic->hz - 1) / periodic->hz / 1000;

          return FALSE;
        }

      else
        /* Enough time has passed.  Run now. */
        {
          *timeout = 0;
          return TRUE;
        }
    }
  else
    /* We shouldn't be running now at all. */
    {
      *timeout = -1;
      return FALSE;
    }
}

static gboolean
g_periodic_check (GSource *source)
{
  GPeriodic *periodic = PERIODIC_FROM_SOURCE (source);

  if ((periodic->ticks || periodic->repairs) && !periodic->blocked)
    /* We need to run. */
    {
      guint64 now = g_periodic_get_microticks (periodic);

      /* Run if it's not too soon. */
      return !(now < periodic->last_run + 1000000);
    }

  else
    /* We shouldn't be running now at all. */
    return FALSE;
}

static gboolean
g_periodic_dispatch (GSource     *source,
                     GSourceFunc  callback,
                     gpointer     user_data)
{
  GPeriodic *periodic = PERIODIC_FROM_SOURCE (source);
  guint64 elapsed_ticks;
  guint64 now;

  g_assert (periodic->blocked == 0);

  /* Update the last_run.
   *
   * In the normal case we want to add exactly 1 tick to it.  This
   * ensures that the clock runs at the proper rate in the normal case
   * (ie: the dispatch overhead time is not lost).
   *
   * If more than one tick has elapsed, we set it equal to the current
   * time.  This has two purposes:
   *
   *   - sets last_run to something reasonable if the clock is running
   *     for the first time or after a long period of inactivity
   *
   *   - resets our stride if we missed a frame
   */
  now = g_periodic_get_microticks (periodic);
  elapsed_ticks = (now - periodic->last_run) / 1000000;
  g_assert (elapsed_ticks > 0);

  if G_LIKELY (elapsed_ticks == 1)
    periodic->last_run += 1000000;
  else
    periodic->last_run = now;

  g_periodic_run (periodic);

  return TRUE;
}

static void
g_periodic_get_property (GObject *object, guint prop_id,
                         GValue *value, GParamSpec *pspec)
{
  GPeriodic *periodic = G_PERIODIC (object);

  switch (prop_id)
    {
    case PROP_PRIORITY:
      g_value_set_int (value, g_source_get_priority (periodic->source));
      break;

    case PROP_HZ:
      g_value_set_uint (value, periodic->hz);
      break;

    default:
      g_assert_not_reached ();
    }
}

static void
g_periodic_set_property (GObject *object, guint prop_id,
                         const GValue *value, GParamSpec *pspec)
{
  GPeriodic *periodic = G_PERIODIC (object);

  switch (prop_id)
    {
    case PROP_PRIORITY:
      g_source_set_priority (periodic->source, g_value_get_int (value));
      break;

    case PROP_HZ:
      periodic->hz = g_value_get_uint (value);
      break;

    default:
      g_assert_not_reached ();
    }
}

static void
g_periodic_finalize (GObject *object)
{
  GPeriodic *periodic = G_PERIODIC (object);

  g_source_destroy (periodic->source);
  g_source_unref (periodic->source);

  G_OBJECT_CLASS (g_periodic_parent_class)
    ->finalize (object);
}

static void
g_periodic_init (GPeriodic *periodic)
{
  static GSourceFuncs source_funcs = {
    g_periodic_prepare, g_periodic_check, g_periodic_dispatch
  };

  periodic->source = g_source_new (&source_funcs, sizeof (GPeriodicSource));
  ((GPeriodicSource *) periodic->source)->periodic = periodic;
  g_source_attach (periodic->source, g_main_context_get_thread_default ());
}

static void
g_periodic_class_init (GObjectClass *class)
{
  class->get_property = g_periodic_get_property;
  class->set_property = g_periodic_set_property;
  class->finalize = g_periodic_finalize;

  g_periodic_tick = g_signal_new ("tick", G_TYPE_PERIODIC,
                                  G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                                  _gio_marshal_VOID__UINT64,
                                  G_TYPE_NONE, 1, G_TYPE_UINT64);
  g_periodic_repair = g_signal_new ("repair", G_TYPE_PERIODIC,
                                    G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                                    g_cclosure_marshal_VOID__VOID,
                                    G_TYPE_NONE, 0);

  g_object_class_install_property (class, PROP_HZ,
    g_param_spec_uint ("hz", "ticks per second",
                       "rate (in Hz) at which the 'tick' signal is emitted",
                       1, 120, 1, G_PARAM_READWRITE |
                       G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (class, PROP_PRIORITY,
    g_param_spec_int ("priority", "priority level",
                      "the GSource priority level to run at",
                      G_MININT, G_MAXINT, 0, G_PARAM_READWRITE |
                      G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
}

/**
 * g_periodic_block:
 * @periodic: a #GPeriodic clock
 *
 * Temporarily blocks @periodic from running in order to bring it in
 * synch with an external time source.
 *
 * This function must be called from a handler of the "repair" signal.
 *
 * If this function is called, emission of the tick signal will be
 * suspended until g_periodic_unblock() is called an equal number of
 * times.  Once that happens, the "tick" signal will run immediately and
 * future "tick" signals will be emitted relative to the time at which
 * the last call to g_periodic_unblock() occured.
 *
 * Since: 2.28
 **/
void
g_periodic_block (GPeriodic *periodic)
{
  g_return_if_fail (G_IS_PERIODIC (periodic));
  g_return_if_fail (periodic->in_repair);

  periodic->blocked++;
}

/**
 * g_periodic_unblock:
 * @periodic: a #GPeriodic clock
 *
 * Reverses the effect of a previous call to g_periodic_block().
 *
 * If this call removes the last block, the tick signal is immediately
 * run.  The repair signal may also be run if the clock is marked as
 * damaged.
 *
 * This function may not be called from handlers of any signal emitted
 * by @periodic.
 *
 * Since: 2.28
 **/
void
g_periodic_unblock (GPeriodic *periodic)
{
  g_return_if_fail (G_IS_PERIODIC (periodic));
  g_return_if_fail (!periodic->in_repair);
  g_return_if_fail (!periodic->in_tick);
  g_return_if_fail (periodic->blocked);

  if (--periodic->blocked)
    {
      periodic->last_run = g_periodic_get_microticks (periodic);
      g_periodic_run (periodic);
    }
}

/**
 * g_periodic_add:
 * @periodic: a #GPeriodic clock
 * @callback: a #GPeriodicTickFunc function
 * @user_data: data for @callback
 * @notify: for freeing @user_data when it is no longer needed
 *
 * Request periodic calls to @callback to start.  The periodicity of the
 * calls is determined by the 'hz' property.
 *
 * This function may not be called from a handler of the repair signal,
 * but it is perfectly reasonable to call it from a handler of the tick
 * signal.
 *
 * The callback may be cancelled later by using g_periodic_remove() on
 * the return value of this function.
 *
 * Returns: a non-zero tag identifying this callback
 *
 * Since: 2.28
 **/
/**
 * GPeriodicTickFunc:
 * @periodic: the #GPeriodic clock that is ticking
 * @timestamp: the timestamp at the time of the tick
 * @user_data: the user data given to g_periodic_add()
 *
 * The signature of the callback function that is called when the
 * #GPeriodic clock ticks.
 *
 * The @timestamp parameter is equal for all callbacks called during a
 * particular tick on a given clock.
 *
 * Since: 2.28
 **/
guint
g_periodic_add (GPeriodic         *periodic,
                GPeriodicTickFunc  callback,
                gpointer           user_data,
                GDestroyNotify     notify)
{
  GPeriodicTick *tick;
  static guint id;

  g_return_val_if_fail (G_IS_PERIODIC (periodic), 0);
  g_return_val_if_fail (!periodic->in_repair, 0);

  tick = g_slice_new (GPeriodicTick);
  tick->callback = callback;
  tick->user_data = user_data;
  tick->notify = notify;
  tick->id = ++id;

  periodic->ticks = g_slist_prepend (periodic->ticks, tick);

  return tick->id;
}

/**
 * g_periodic_remove:
 * @periodic: a #GPeriodic clock
 * @tag: the ID of the callback to remove
 *
 * Reverse the effect of a previous call to g_periodic_start().
 *
 * @tag is the ID returned by that function.
 *
 * This function may not be called from a handler of the repair signal,
 * but it is perfectly reasonable to call it from a handler of the tick
 * signal.
 *
 * Since: 2.28
 **/
void
g_periodic_remove (GPeriodic *periodic,
                   guint      tag)
{
  GSList **iter;

  g_return_if_fail (G_IS_PERIODIC (periodic));
  g_return_if_fail (!periodic->in_repair);

  for (iter = &periodic->ticks; *iter; iter = &(*iter)->next)
    {
      GPeriodicTick *tick = (*iter)->data;

      if (tick->id == tag)
        {
          /* do this first incase the destroy notify re-enters */
          *iter = g_slist_remove (*iter, tick);

          if (tick->notify)
            tick->notify (tick->user_data);

          g_slice_free (GPeriodicTick, tick);
          return;
        }
    }

  g_critical ("GPeriodic: tag %u not registered", tag);
}

/**
 * g_periodic_damaged:
 * @periodic: a #GPeriodic clock
 * @callback: a #GPeriodicRepairFunc
 * @user_data: data for @callback
 * @notify: for freeing @user_data when it is no longer needed
 *
 * Report damage and schedule @callback to be called during the next
 * repair phase.  Multiple registrations result in multiple invocations,
 * so you should track for yourself if you are already registered.
 *
 * You may not call this function during the repair phase.
 *
 * Since: 2.28
 **/
/**
 * GPeriodicRepairFunc:
 * @periodic: the #GPeriodic clock that the damage was reported to
 * @user_data: the user data given to g_periodic_damaged()
 *
 * The signature of the callback function that is called during the
 * repair phase when damaged was reported using g_periodic_damaged().
 *
 * Since: 2.28
 **/
void
g_periodic_damaged (GPeriodic           *periodic,
                    GPeriodicRepairFunc  callback,
                    gpointer             user_data,
                    GDestroyNotify       notify)
{
  GPeriodicRepair *repair;

  g_return_if_fail (G_IS_PERIODIC (periodic));
  g_return_if_fail (!periodic->in_repair);

  repair = g_slice_new (GPeriodicRepair);
  repair->callback = callback;
  repair->user_data = user_data;
  repair->notify = notify;

  periodic->repairs = g_slist_prepend (periodic->repairs, repair);
}

/**
 * g_periodic_get_hz:
 * @periodic: a #GPeriodic clock
 *
 * Gets the frequency of the clock.
 *
 * Returns: the frquency of the clock, in Hz
 *
 * Since: 2.28
 **/
guint
g_periodic_get_hz (GPeriodic *periodic)
{
  return periodic->hz;
}

/**
 * g_periodic_get_priority:
 * @periodic: a #GPeriodic clock
 *
 * Gets the #GSource priority of the clock.
 *
 * Returns: the priority level
 *
 * Since: 2.28
 **/
gint
g_periodic_get_priority (GPeriodic *periodic)
{
  return g_source_get_priority (periodic->source);
}

/**
 * g_periodic_new:
 * @hz: the frequency of the new clock in Hz (between 1 and 120)
 * @priority: the #GSource priority to run at
 *
 * Creates a new #GPeriodic clock.
 *
 * The created clock is attached to the thread-default main context in
 * effect at the time of the call to this function.  See
 * g_main_context_push_thread_default() for more information.
 *
 * Due to the fact that #GMainContext is only accurate to the nearest
 * millisecond, the frequency can not meaningfully get too close to
 * 1000.  For this reason, it is arbitrarily bounded at 120.
 *
 * Returns: a new #GPeriodic
 *
 * Since: 2.28
 **/
GPeriodic *
g_periodic_new (guint hz,
                gint  priority)
{
  g_return_val_if_fail (1 <= hz && hz <= 120, NULL);

  return g_object_new (G_TYPE_PERIODIC,
                       "hz", hz,
                       "priority", priority,
                       NULL);
}
