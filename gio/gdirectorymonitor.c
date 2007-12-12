/* GIO - GLib Input, Output and Streaming Library
 * 
 * Copyright (C) 2006-2007 Red Hat, Inc.
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
 * Author: Alexander Larsson <alexl@redhat.com>
 */

#include <config.h>
#include <string.h>

#include "gdirectorymonitor.h"
#include "gio-marshal.h"
#include "gfile.h"
#include "gvfs.h"
#include "glibintl.h"

#include "gioalias.h"

/**
 * SECTION:gdirectorymonitor
 * @short_description: Directory Monitor
 * @see_also: #GFileMonitor
 * 
 * Monitors a directory for changes to files in it.
 *
 **/

enum {
  CHANGED,
  LAST_SIGNAL
};

G_DEFINE_ABSTRACT_TYPE (GDirectoryMonitor, g_directory_monitor, G_TYPE_OBJECT);

typedef struct {
  GFile *file;
  guint32 last_sent_change_time; /* 0 == not sent */
  guint32 send_delayed_change_at; /* 0 == never */
  guint32 send_virtual_changes_done_at; /* 0 == never */
} RateLimiter;

struct _GDirectoryMonitorPrivate {
  gboolean cancelled;
  int rate_limit_msec;

  GHashTable *rate_limiter;
  
  GSource *timeout;
  guint32 timeout_fires_at;
};

enum {
  PROP_0,
  PROP_RATE_LIMIT,
  PROP_CANCELLED
};

#define DEFAULT_RATE_LIMIT_MSECS 800
#define DEFAULT_VIRTUAL_CHANGES_DONE_DELAY_SECS 2

static guint signals[LAST_SIGNAL] = { 0 };

static void
rate_limiter_free (RateLimiter *limiter)
{
  g_object_unref (limiter->file);
  g_free (limiter);
}

static void
g_directory_monitor_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GDirectoryMonitor *monitor;

  monitor = G_DIRECTORY_MONITOR (object);

  switch (prop_id)
    {
    case PROP_RATE_LIMIT:
      g_directory_monitor_set_rate_limit (monitor, g_value_get_int (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
g_directory_monitor_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  GDirectoryMonitor *monitor;
  GDirectoryMonitorPrivate *priv;

  monitor = G_DIRECTORY_MONITOR (object);
  priv = monitor->priv;

  switch (prop_id)
    {
    case PROP_RATE_LIMIT:
      g_value_set_int (value, priv->rate_limit_msec);
      break;

    case PROP_CANCELLED:
      g_value_set_boolean (value, priv->cancelled);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
g_directory_monitor_finalize (GObject *object)
{
  GDirectoryMonitor *monitor;

  monitor = G_DIRECTORY_MONITOR (object);

  if (monitor->priv->timeout)
    {
      g_source_destroy (monitor->priv->timeout);
      g_source_unref (monitor->priv->timeout);
    }

  g_hash_table_destroy (monitor->priv->rate_limiter);
  
  if (G_OBJECT_CLASS (g_directory_monitor_parent_class)->finalize)
    (*G_OBJECT_CLASS (g_directory_monitor_parent_class)->finalize) (object);
}

static void
g_directory_monitor_dispose (GObject *object)
{
  GDirectoryMonitor *monitor;
  
  monitor = G_DIRECTORY_MONITOR (object);

  /* Make sure we cancel on last unref */
  if (!monitor->priv->cancelled)
    g_directory_monitor_cancel (monitor);
  
  if (G_OBJECT_CLASS (g_directory_monitor_parent_class)->dispose)
    (*G_OBJECT_CLASS (g_directory_monitor_parent_class)->dispose) (object);
}

static void
g_directory_monitor_class_init (GDirectoryMonitorClass *klass)
{
  GObjectClass *object_class;
  
  g_type_class_add_private (klass, sizeof (GDirectoryMonitorPrivate));
  
  object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = g_directory_monitor_finalize;
  object_class->dispose = g_directory_monitor_dispose;
  object_class->get_property = g_directory_monitor_get_property;
  object_class->set_property = g_directory_monitor_set_property;

  /**
   * GDirectoryMonitor::changed:
   * @monitor: the #GDirectoryMonitor
   * @child: the #GFile which changed
   * @other_file: the other #GFile which changed
   * @event_type: a #GFileMonitorEvent indicating what the event was
   *
   * Emitted when a child file changes.
   */
  signals[CHANGED] =
    g_signal_new (I_("changed"),
		  G_TYPE_DIRECTORY_MONITOR,
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GDirectoryMonitorClass, changed),
		  NULL, NULL,
		  _gio_marshal_VOID__OBJECT_OBJECT_INT,
		  G_TYPE_NONE, 3,
		  G_TYPE_FILE, G_TYPE_FILE, G_TYPE_INT);

  g_object_class_install_property (object_class,
                                   PROP_RATE_LIMIT,
                                   g_param_spec_int ("rate-limit",
                                                     P_("Rate limit"),
                                                     P_("The limit of the monitor to watch for changes, in milliseconds"),
                                                     0, G_MAXINT,
                                                     DEFAULT_RATE_LIMIT_MSECS,
                                                     G_PARAM_READWRITE|
                                                     G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB));

  g_object_class_install_property (object_class,
                                   PROP_CANCELLED,
                                   g_param_spec_boolean ("cancelled",
                                                         P_("Cancelled"),
                                                         P_("Whether the monitor has been cancelled"),
                                                         FALSE,
                                                         G_PARAM_READABLE|
                                                         G_PARAM_STATIC_NAME|G_PARAM_STATIC_NICK|G_PARAM_STATIC_BLURB));

}

static void
g_directory_monitor_init (GDirectoryMonitor *monitor)
{
  monitor->priv = G_TYPE_INSTANCE_GET_PRIVATE (monitor,
					       G_TYPE_DIRECTORY_MONITOR,
					       GDirectoryMonitorPrivate);

  monitor->priv->rate_limit_msec = DEFAULT_RATE_LIMIT_MSECS;
  monitor->priv->rate_limiter = g_hash_table_new_full (g_file_hash, (GEqualFunc)g_file_equal,
						       NULL, (GDestroyNotify) rate_limiter_free);
}


/**
 * g_directory_monitor_cancel:
 * @monitor: a #GDirectoryMonitor.
 *
 * Cancels the monitoring activity on @monitor. Note that
 * the monitor is automatically cancelled when finalized.
 *
 * It is safe to call this multiple times.
 * 
 * Returns: %TRUE if the monitor was cancelled successfully. %FALSE otherwise.
 **/
gboolean
g_directory_monitor_cancel (GDirectoryMonitor *monitor)
{
  GDirectoryMonitorClass *class;

  g_return_val_if_fail (G_IS_DIRECTORY_MONITOR (monitor), FALSE);
  
  if (monitor->priv->cancelled)
    return TRUE;
  
  monitor->priv->cancelled = TRUE;
  g_object_notify (G_OBJECT (monitor), "cancelled");
  
  class = G_DIRECTORY_MONITOR_GET_CLASS (monitor);
  return (* class->cancel) (monitor);
}

/**
 * g_directory_monitor_set_rate_limit:
 * @monitor: a #GDirectoryMonitor.
 * @limit_msecs: the change rate limit of the directory monitor in milliseconds.
 *
 * Report same consecutive changes of the same type at most once each @limit_msecs milliseconds.
 **/
void
g_directory_monitor_set_rate_limit (GDirectoryMonitor *monitor,
				    int                limit_msecs)
{
  GDirectoryMonitorPrivate *priv;
  g_return_if_fail (G_IS_DIRECTORY_MONITOR (monitor));
  priv = monitor->priv;
  if (priv->rate_limit_msec != limit_msecs)
    {
      monitor->priv->rate_limit_msec = limit_msecs;
      g_object_notify (G_OBJECT (monitor), "rate-limit");
    }
}

/**
 * g_directory_monitor_is_cancelled:
 * @monitor: a #GDirectoryMonitor.
 * 
 * Checks whether @monitor is cancelled.
 *
 * Returns: %TRUE if the monitor on the directory was cancelled. 
 *     %FALSE otherwise.
 **/
gboolean
g_directory_monitor_is_cancelled (GDirectoryMonitor *monitor)
{
  g_return_val_if_fail (G_IS_DIRECTORY_MONITOR (monitor), FALSE);

  return monitor->priv->cancelled;
}

static guint32
get_time_msecs (void)
{
  return g_thread_gettime() / (1000 * 1000);
}

static guint32
time_difference (guint32 from, guint32 to)
{
  if (from > to)
    return 0;
  return to - from;
}

static RateLimiter *
new_limiter (GDirectoryMonitor *monitor,
	     GFile             *file)
{
  RateLimiter *limiter;

  limiter = g_new0 (RateLimiter, 1);
  limiter->file = g_object_ref (file);
  g_hash_table_insert (monitor->priv->rate_limiter, file, limiter);
  
  return limiter;
}

static void
rate_limiter_send_virtual_changes_done_now (GDirectoryMonitor *monitor, 
                                            RateLimiter       *limiter)
{
  if (limiter->send_virtual_changes_done_at != 0)
    {
      g_signal_emit (monitor, signals[CHANGED], 0, limiter->file, NULL, G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT);
      limiter->send_virtual_changes_done_at = 0;
    }
}

static void
rate_limiter_send_delayed_change_now (GDirectoryMonitor *monitor, 
                                      RateLimiter       *limiter, 
                                      guint32            time_now)
{
  if (limiter->send_delayed_change_at != 0)
    {
      g_signal_emit (monitor, signals[CHANGED], 0, limiter->file, NULL, G_FILE_MONITOR_EVENT_CHANGED);
      limiter->send_delayed_change_at = 0;
      limiter->last_sent_change_time = time_now;
    }
}

typedef struct {
  guint32 min_time;
  guint32 time_now;
  GDirectoryMonitor *monitor;
} ForEachData;

static gboolean
calc_min_time (GDirectoryMonitor *monitor, 
               RateLimiter       *limiter, 
               guint32            time_now, 
               guint32           *min_time)
{
  gboolean delete_me;
  guint32 expire_at;

  delete_me = TRUE;

  if (limiter->last_sent_change_time != 0)
    {
      /* Set a timeout at 2*rate limit so that we can clear out the change from the hash eventualy */
      expire_at = limiter->last_sent_change_time + 2 * monitor->priv->rate_limit_msec;

      if (time_difference (time_now, expire_at) > 0)
	{
	  delete_me = FALSE;
	  *min_time = MIN (*min_time,
			   time_difference (time_now, expire_at));
	}
    }

  if (limiter->send_delayed_change_at != 0)
    {
      delete_me = FALSE;
      *min_time = MIN (*min_time,
		       time_difference (time_now, limiter->send_delayed_change_at));
    }

  if (limiter->send_virtual_changes_done_at != 0)
    {
      delete_me = FALSE;
      *min_time = MIN (*min_time,
		       time_difference (time_now, limiter->send_virtual_changes_done_at));
    }

  return delete_me;
}

static gboolean
foreach_rate_limiter_fire (gpointer key,
			   gpointer value,
			   gpointer user_data)
{
  RateLimiter *limiter = value;
  ForEachData *data = user_data;

  if (limiter->send_delayed_change_at != 0 &&
      time_difference (data->time_now, limiter->send_delayed_change_at) == 0)
    rate_limiter_send_delayed_change_now (data->monitor, limiter, data->time_now);

  if (limiter->send_virtual_changes_done_at != 0 &&
      time_difference (data->time_now, limiter->send_virtual_changes_done_at) == 0)
    rate_limiter_send_virtual_changes_done_now (data->monitor, limiter);

  return calc_min_time (data->monitor, limiter, data->time_now, &data->min_time);
}

static gboolean 
rate_limiter_timeout (gpointer timeout_data)
{
  GDirectoryMonitor *monitor = timeout_data;
  ForEachData data;
  GSource *source;
  
  data.min_time = G_MAXUINT32;
  data.monitor = monitor;
  data.time_now = get_time_msecs ();
  g_hash_table_foreach_remove (monitor->priv->rate_limiter,
			       foreach_rate_limiter_fire,
			       &data);

  /* Remove old timeout */
  if (monitor->priv->timeout)
    {
      g_source_destroy (monitor->priv->timeout);
      g_source_unref (monitor->priv->timeout);
      monitor->priv->timeout = NULL;
      monitor->priv->timeout_fires_at = 0;
    }
  
  /* Set up new timeout */
  if (data.min_time != G_MAXUINT32)
    {
      source = g_timeout_source_new (data.min_time + 1); /* + 1 to make sure we've really passed the time */
      g_source_set_callback (source, rate_limiter_timeout, monitor, NULL);
      g_source_attach (source, NULL);
      
      monitor->priv->timeout = source;
      monitor->priv->timeout_fires_at = data.time_now + data.min_time; 
    }
  
  return FALSE;
}

static gboolean
foreach_rate_limiter_update (gpointer key,
			     gpointer value,
			     gpointer user_data)
{
  RateLimiter *limiter = value;
  ForEachData *data = user_data;

  return calc_min_time (data->monitor, limiter, data->time_now, &data->min_time);
}

static void
update_rate_limiter_timeout (GDirectoryMonitor *monitor, 
                             guint              new_time)
{
  ForEachData data;
  GSource *source;
  
  if (monitor->priv->timeout_fires_at != 0 && new_time != 0 &&
      time_difference (new_time, monitor->priv->timeout_fires_at) == 0)
    return; /* Nothing to do, we already fire earlier than that */

  data.min_time = G_MAXUINT32;
  data.monitor = monitor;
  data.time_now = get_time_msecs ();
  g_hash_table_foreach_remove (monitor->priv->rate_limiter,
			       foreach_rate_limiter_update,
			       &data);

  /* Remove old timeout */
  if (monitor->priv->timeout)
    {
      g_source_destroy (monitor->priv->timeout);
      g_source_unref (monitor->priv->timeout);
      monitor->priv->timeout_fires_at = 0;
      monitor->priv->timeout = NULL;
    }

  /* Set up new timeout */
  if (data.min_time != G_MAXUINT32)
    {
      source = g_timeout_source_new (data.min_time + 1);  /* + 1 to make sure we've really passed the time */
      g_source_set_callback (source, rate_limiter_timeout, monitor, NULL);
      g_source_attach (source, NULL);
      
      monitor->priv->timeout = source;
      monitor->priv->timeout_fires_at = data.time_now + data.min_time; 
    }
}

/**
 * g_directory_monitor_emit_event:
 * @monitor: a #GDirectoryMonitor.
 * @child: a #GFile.
 * @other_file: a #GFile.
 * @event_type: a set of #GFileMonitorEvent flags.
 * 
 * Emits the #GDirectoryMonitor::changed signal if a change
 * has taken place. Should be called from directory monitor 
 * implementations only.
 **/
void
g_directory_monitor_emit_event (GDirectoryMonitor *monitor,
				GFile             *child,
				GFile             *other_file,
				GFileMonitorEvent  event_type)
{
  guint32 time_now, since_last;
  gboolean emit_now;
  RateLimiter *limiter;

  g_return_if_fail (G_IS_DIRECTORY_MONITOR (monitor));
  g_return_if_fail (G_IS_FILE (child));

  limiter = g_hash_table_lookup (monitor->priv->rate_limiter, child);

  if (event_type != G_FILE_MONITOR_EVENT_CHANGED)
    {
      if (limiter)
	{
	  rate_limiter_send_delayed_change_now (monitor, limiter, get_time_msecs ());
	  if (event_type == G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT)
	    limiter->send_virtual_changes_done_at = 0;
	  else
	    rate_limiter_send_virtual_changes_done_now (monitor, limiter);
	  update_rate_limiter_timeout (monitor, 0);
	}
      g_signal_emit (monitor, signals[CHANGED], 0, child, other_file, event_type);
    }
  else
    {
      /* Changed event, rate limit */
      time_now = get_time_msecs ();
      emit_now = TRUE;
      
      if (limiter)
	{
	  since_last = time_difference (limiter->last_sent_change_time, time_now);
	  if (since_last < monitor->priv->rate_limit_msec)
	    {
	      /* We ignore this change, but arm a timer so that we can fire it later if we
		 don't get any other events (that kill this timeout) */
	      emit_now = FALSE;
	      if (limiter->send_delayed_change_at == 0)
		{
		  limiter->send_delayed_change_at = time_now + monitor->priv->rate_limit_msec;
		  update_rate_limiter_timeout (monitor, limiter->send_delayed_change_at);
		}
	    }
	}

      if (limiter == NULL)
	limiter = new_limiter (monitor, child);
      
      if (emit_now)
	{
	  g_signal_emit (monitor, signals[CHANGED], 0, child, other_file, event_type);

	  limiter->last_sent_change_time = time_now;
	  limiter->send_delayed_change_at = 0;
	  /* Set a timeout of 2*rate limit so that we can clear out the change from the hash eventualy */
	  update_rate_limiter_timeout (monitor, time_now + 2 * monitor->priv->rate_limit_msec);
	}

      /* Schedule a virtual change done. This is removed if we get a real one, and
	 postponed if we get more change events. */

      limiter->send_virtual_changes_done_at = time_now + DEFAULT_VIRTUAL_CHANGES_DONE_DELAY_SECS * 1000;
      update_rate_limiter_timeout (monitor, limiter->send_virtual_changes_done_at);
    }
}

#define __G_DIRECTORY_MONITOR_C__
#include "gioaliasdef.c"
