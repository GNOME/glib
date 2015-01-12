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
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Alexander Larsson <alexl@redhat.com>
 */

#include "config.h"

#include "gioenumtypes.h"
#include "glocalfilemonitor.h"
#include "giomodule-priv.h"
#include "gioerror.h"
#include "glibintl.h"
#include "glocalfile.h"
#include "glib-private.h"

#include <string.h>

#define DEFAULT_RATE_LIMIT                           800 * G_TIME_SPAN_MILLISECOND
#define VIRTUAL_CHANGES_DONE_DELAY                     2 * G_TIME_SPAN_SECOND

/* GFileMonitorSource is a GSource responsible for emitting the changed
 * signals in the owner-context of the GFileMonitor.
 *
 * It contains functionality for cross-thread queuing of events.  It
 * also handles merging of CHANGED events and emission of CHANGES_DONE
 * events.
 *
 * We use the "priv" pointer in the external struct to store it.
 */
struct _GFileMonitorSource {
  GSource       source;

  GMutex        lock;
  gpointer      instance;
  GFileMonitorFlags flags;
  gchar        *dirname;
  gchar        *basename;
  gchar        *filename;
  GSequence    *pending_changes; /* sorted by ready time */
  GHashTable   *pending_changes_table;
  GQueue        event_queue;
  gint64        rate_limit;
};

/* PendingChange is a struct to keep track of a file that needs to have
 * (at least) a CHANGES_DONE_HINT event sent for it in the near future.
 *
 * If 'dirty' is TRUE then a CHANGED event also needs to be sent.
 *
 * last_emission is the last time a CHANGED event was emitted.  It is
 * used to calculate the time to send the next event.
 */
typedef struct {
  gchar    *child;
  guint64   last_emission : 63;
  guint64   dirty         :  1;
} PendingChange;

/* QueuedEvent is a signal that will be sent immediately, as soon as the
 * source gets a chance to dispatch.  The existence of any queued event
 * implies that the source is ready now.
 */
typedef struct
{
  GFileMonitorEvent event_type;
  GFile *child;
  GFile *other;
} QueuedEvent;

static gint64
pending_change_get_ready_time (const PendingChange *change,
                               GFileMonitorSource  *fms)
{
  if (change->dirty)
    return change->last_emission + fms->rate_limit;
  else
    return change->last_emission + VIRTUAL_CHANGES_DONE_DELAY;
}

static int
pending_change_compare_ready_time (gconstpointer a_p,
                                   gconstpointer b_p,
                                   gpointer      user_data)
{
  GFileMonitorSource *fms = user_data;
  const PendingChange *a = a_p;
  const PendingChange *b = b_p;
  gint64 ready_time_a;
  gint64 ready_time_b;

  ready_time_a = pending_change_get_ready_time (a, fms);
  ready_time_b = pending_change_get_ready_time (b, fms);

  if (ready_time_a < ready_time_b)
    return -1;
  else
    return ready_time_a > ready_time_b;
}

static void
pending_change_free (gpointer data)
{
  PendingChange *change = data;

  g_free (change->child);

  g_slice_free (PendingChange, change);
}

static void
queued_event_free (QueuedEvent *event)
{
  g_object_unref (event->child);
  if (event->other)
    g_object_unref (event->other);

  g_slice_free (QueuedEvent, event);
}

static gint64
g_file_monitor_source_get_ready_time (GFileMonitorSource *fms)
{
  GSequenceIter *iter;

  if (fms->event_queue.length)
    return 0;

  iter = g_sequence_get_begin_iter (fms->pending_changes);
  if (g_sequence_iter_is_end (iter))
    return -1;

  return pending_change_get_ready_time (g_sequence_get (iter), fms);
}

static void
g_file_monitor_source_update_ready_time (GFileMonitorSource *fms)
{
  g_source_set_ready_time ((GSource *) fms, g_file_monitor_source_get_ready_time (fms));
}

static GSequenceIter *
g_file_monitor_source_find_pending_change (GFileMonitorSource *fms,
                                           const gchar        *child)
{
  return g_hash_table_lookup (fms->pending_changes_table, child);
}

static void
g_file_monitor_source_add_pending_change (GFileMonitorSource *fms,
                                          const gchar        *child,
                                          gint64              now)
{
  PendingChange *change;
  GSequenceIter *iter;

  change = g_slice_new (PendingChange);
  change->child = g_strdup (child);
  change->last_emission = now;
  change->dirty = FALSE;

  iter = g_sequence_insert_sorted (fms->pending_changes, change, pending_change_compare_ready_time, fms);
  g_hash_table_insert (fms->pending_changes_table, change->child, iter);
}

static void
g_file_monitor_source_update_pending_change (GFileMonitorSource *fms,
                                             GSequenceIter      *iter,
                                             gboolean            dirty)
{
  PendingChange *change;

  change = g_sequence_get (iter);

  if (change->dirty != dirty)
    {
      change->dirty = dirty;

      g_sequence_sort_changed (iter, pending_change_compare_ready_time, fms);
    }
}

static void
g_file_monitor_source_remove_pending_change (GFileMonitorSource *fms,
                                             GSequenceIter      *iter,
                                             const gchar        *child)
{
  /* must remove the hash entry first -- its key is owned by the data
   * which will be freed when removing the sequence iter
   */
  g_hash_table_remove (fms->pending_changes_table, child);
  g_sequence_remove (iter);
}

static void
g_file_monitor_source_queue_event (GFileMonitorSource *fms,
                                   GFileMonitorEvent   event_type,
                                   const gchar        *child,
                                   GFile              *other)
{
  QueuedEvent *event;

  event = g_slice_new (QueuedEvent);
  event->event_type = event_type;
  event->child = g_local_file_new2 (fms->dirname, child);
  event->other = other;
  if (other)
    g_object_ref (other);

  g_queue_push_tail (&fms->event_queue, event);
}

static void
g_file_monitor_source_file_changed (GFileMonitorSource *fms,
                                    const gchar        *child,
                                    gint64              now)
{
  GSequenceIter *pending;

  pending = g_file_monitor_source_find_pending_change (fms, child);

  /* If there is no pending change, emit one and create a record,
   * else: just mark the existing record as dirty.
   */
  if (!pending)
    {
      g_file_monitor_source_queue_event (fms, G_FILE_MONITOR_EVENT_CHANGED, child, NULL);
      g_file_monitor_source_add_pending_change (fms, child, now);
    }
  else
    g_file_monitor_source_update_pending_change (fms, pending, TRUE);

  g_file_monitor_source_update_ready_time (fms);
}

static void
g_file_monitor_source_file_changes_done (GFileMonitorSource *fms,
                                         const gchar        *child)
{
  GSequenceIter *pending;

  pending = g_file_monitor_source_find_pending_change (fms, child);
  if (pending)
    {
      g_file_monitor_source_queue_event (fms, G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT, child, NULL);
      g_file_monitor_source_remove_pending_change (fms, pending, child);
    }
}

static void
g_file_monitor_source_file_created (GFileMonitorSource *fms,
                                    const gchar        *child,
                                    gint64              event_time)
{
  /* Unlikely, but if we have pending changes for this filename, make
   * sure we flush those out first, before creating the new ones.
   */
  g_file_monitor_source_file_changes_done (fms, child);

  /* Emit CREATE and add a pending changes record */
  g_file_monitor_source_queue_event (fms, G_FILE_MONITOR_EVENT_CREATED, child, NULL);
  g_file_monitor_source_add_pending_change (fms, child, event_time);
}

static void
g_file_monitor_source_send_event (GFileMonitorSource *fms,
                                  GFileMonitorEvent   event_type,
                                  const gchar        *child,
                                  GFile              *other)
{
  /* always flush any pending changes before we queue a new event */
  g_file_monitor_source_file_changes_done (fms, child);
  g_file_monitor_source_queue_event (fms, event_type, child, other);
}

static gboolean
is_basename (const gchar *name)
{
  if (name[0] == '.' && ((name[1] == '.' && name[2] == '\0') || name[1] == '\0'))
    return FALSE;

  return !strchr (name, '/');
}

void
g_file_monitor_source_handle_event (GFileMonitorSource *fms,
                                    GFileMonitorEvent   event_type,
                                    const gchar        *child,
                                    const gchar        *rename_to,
                                    GFile              *other,
                                    gint64              event_time)
{
  g_assert (is_basename (child));
  g_assert (!rename_to || is_basename (rename_to));

  g_mutex_lock (&fms->lock);

  /* monitor is already gone -- don't bother */
  if (!fms->instance)
    {
      g_mutex_unlock (&fms->lock);
      return;
    }

  switch (event_type)
    {
    case G_FILE_MONITOR_EVENT_CREATED:
      g_assert (!other && !rename_to);
      g_file_monitor_source_file_created (fms, child, event_time);
      break;

    case G_FILE_MONITOR_EVENT_CHANGED:
      g_assert (!other && !rename_to);
      g_file_monitor_source_file_changed (fms, child, event_time);
      break;

    case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT:
      g_assert (!other && !rename_to);
      g_file_monitor_source_file_changes_done (fms, child);
      break;

    case G_FILE_MONITOR_EVENT_MOVED_IN:
      g_assert (!rename_to);
      if (fms->flags & G_FILE_MONITOR_WATCH_MOVES)
        g_file_monitor_source_send_event (fms, G_FILE_MONITOR_EVENT_MOVED_IN, child, other);
      else
        g_file_monitor_source_file_created (fms, child, event_time);
      break;

    case G_FILE_MONITOR_EVENT_MOVED_OUT:
      g_assert (!rename_to);
      if (fms->flags & G_FILE_MONITOR_WATCH_MOVES)
        g_file_monitor_source_send_event (fms, G_FILE_MONITOR_EVENT_MOVED_OUT, child, other);
      else if (fms->flags & G_FILE_MONITOR_SEND_MOVED)
        g_file_monitor_source_send_event (fms, G_FILE_MONITOR_EVENT_MOVED, child, other);
      else
        g_file_monitor_source_send_event (fms, G_FILE_MONITOR_EVENT_DELETED, child, NULL);
      break;

    case G_FILE_MONITOR_EVENT_RENAMED:
      g_assert (!other && rename_to);
      if (fms->flags & G_FILE_MONITOR_WATCH_MOVES)
        {
          GFile *other;

          other = g_local_file_new2 (fms->dirname, rename_to);
          g_file_monitor_source_file_changes_done (fms, rename_to);
          g_file_monitor_source_send_event (fms, G_FILE_MONITOR_EVENT_RENAMED, child, other);
          g_object_unref (other);
        }
      else if (fms->flags & G_FILE_MONITOR_SEND_MOVED)
        {
          GFile *other;

          other = g_local_file_new2 (fms->dirname, rename_to);
          g_file_monitor_source_file_changes_done (fms, rename_to);
          g_file_monitor_source_send_event (fms, G_FILE_MONITOR_EVENT_MOVED, child, other);
          g_object_unref (other);
        }
      else
        {
          g_file_monitor_source_send_event (fms, G_FILE_MONITOR_EVENT_DELETED, child, NULL);
          g_file_monitor_source_file_created (fms, child, event_time);
        }
      break;

    case G_FILE_MONITOR_EVENT_DELETED:
    case G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED:
    case G_FILE_MONITOR_EVENT_PRE_UNMOUNT:
    case G_FILE_MONITOR_EVENT_UNMOUNTED:
      g_assert (!other && !rename_to);
      g_file_monitor_source_send_event (fms, event_type, child, NULL);
      break;

    case G_FILE_MONITOR_EVENT_MOVED:
      /* was never available in this API */
    default:
      g_assert_not_reached ();
    }

  g_file_monitor_source_update_ready_time (fms);

  g_mutex_unlock (&fms->lock);
}

static gint64
g_file_monitor_source_get_rate_limit (GFileMonitorSource *fms)
{
  gint64 rate_limit;

  g_mutex_lock (&fms->lock);
  rate_limit = fms->rate_limit;
  g_mutex_unlock (&fms->lock);

  return rate_limit;
}

static gboolean
g_file_monitor_source_set_rate_limit (GFileMonitorSource *fms,
                                      gint64              rate_limit)
{
  gboolean changed;

  g_mutex_lock (&fms->lock);

  if (rate_limit != fms->rate_limit)
    {
      fms->rate_limit = rate_limit;

      g_sequence_sort (fms->pending_changes, pending_change_compare_ready_time, fms);
      g_file_monitor_source_update_ready_time (fms);
    }
  else
    changed = FALSE;

  g_mutex_unlock (&fms->lock);

  return changed;
}

static gboolean
g_file_monitor_source_dispatch (GSource     *source,
                                GSourceFunc  callback,
                                gpointer     user_data)
{
  GFileMonitorSource *fms = (GFileMonitorSource *) source;
  QueuedEvent *event;
  GQueue event_queue;
  gint64 now;

  /* make sure the monitor still exists */
  if (!fms->instance)
    return FALSE;

  now = g_source_get_time (source);

  /* Acquire the lock once and grab all events in one go, handling the
   * queued events first.  This avoids strange possibilities in cases of
   * long delays, such as CHANGED events coming before CREATED events.
   *
   * We do this by converting the applicable pending changes into queued
   * events (after the ones already queued) and then stealing the entire
   * event queue in one go.
   */
  g_mutex_lock (&fms->lock);

  /* Create events for any pending changes that are due to fire */
  while (g_sequence_get_length (fms->pending_changes))
    {
      GSequenceIter *iter = g_sequence_get_begin_iter (fms->pending_changes);
      PendingChange *pending = g_sequence_get (iter);

      /* We've gotten to a pending change that's not ready.  Stop. */
      if (pending_change_get_ready_time (pending, fms) > now)
        break;

      if (pending->dirty)
        {
          /* It's time to send another CHANGED and update the record */
          g_file_monitor_source_queue_event (fms, G_FILE_MONITOR_EVENT_CHANGED, pending->child, NULL);
          pending->last_emission = now;
          pending->dirty = FALSE;

          g_sequence_sort_changed (iter, pending_change_compare_ready_time, fms);
        }
      else
        {
          /* It's time to send CHANGES_DONE and remove the pending record */
          g_file_monitor_source_queue_event (fms, G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT, pending->child, NULL);
          g_file_monitor_source_remove_pending_change (fms, iter, pending->child);
        }
    }

  /* Steal the queue */
  memcpy (&event_queue, &fms->event_queue, sizeof event_queue);
  memset (&fms->event_queue, 0, sizeof fms->event_queue);

  g_file_monitor_source_update_ready_time (fms);

  g_mutex_unlock (&fms->lock);

  /* We now have our list of events to deliver */
  while ((event = g_queue_pop_head (&event_queue)))
    {
      g_file_monitor_emit_event (fms->instance, event->child, event->other, event->event_type);
      queued_event_free (event);
    }

  return TRUE;
}

static void
g_file_monitor_source_dispose (GFileMonitorSource *fms)
{
  g_mutex_lock (&fms->lock);

  if (fms->instance)
    {
      GHashTableIter iter;
      gpointer seqiter;
      QueuedEvent *event;

      g_hash_table_iter_init (&iter, fms->pending_changes_table);
      while (g_hash_table_iter_next (&iter, NULL, &seqiter))
        {
          g_hash_table_iter_remove (&iter);
          g_sequence_remove (seqiter);
        }

      while ((event = g_queue_pop_head (&fms->event_queue)))
        queued_event_free (event);

      g_assert (g_sequence_get_length (fms->pending_changes) == 0);
      g_assert (g_hash_table_size (fms->pending_changes_table) == 0);
      g_assert (fms->event_queue.length == 0);
      fms->instance = NULL;

      g_file_monitor_source_update_ready_time (fms);
    }

  g_mutex_unlock (&fms->lock);

  g_source_destroy ((GSource *) fms);
}

static void
g_file_monitor_source_finalize (GSource *source)
{
  GFileMonitorSource *fms = (GFileMonitorSource *) source;

  /* should already have been cleared in dispose of the monitor */
  g_assert (fms->instance == NULL);
  g_assert (g_sequence_get_length (fms->pending_changes) == 0);
  g_assert (g_hash_table_size (fms->pending_changes_table) == 0);
  g_assert (fms->event_queue.length == 0);

  g_hash_table_unref (fms->pending_changes_table);
  g_sequence_free (fms->pending_changes);

  g_free (fms->dirname);
  g_free (fms->basename);
  g_free (fms->filename);

  g_mutex_clear (&fms->lock);
}

static GFileMonitorSource *
g_file_monitor_source_new (gpointer           instance,
                           const gchar       *filename,
                           gboolean           is_directory,
                           GFileMonitorFlags  flags)
{
  static GSourceFuncs source_funcs = {
    NULL, NULL,
    g_file_monitor_source_dispatch,
    g_file_monitor_source_finalize
  };
  GFileMonitorSource *fms;
  GSource *source;

  source = g_source_new (&source_funcs, sizeof (GFileMonitorSource));
  fms = (GFileMonitorSource *) source;

  g_mutex_init (&fms->lock);
  fms->instance = instance;
  fms->pending_changes = g_sequence_new (pending_change_free);
  fms->pending_changes_table = g_hash_table_new (g_str_hash, g_str_equal);
  fms->rate_limit = DEFAULT_RATE_LIMIT;
  fms->flags = flags;

  if (is_directory)
    {
      fms->dirname = g_strdup (filename);
      fms->basename = NULL;
      fms->filename = NULL;
    }
  else if (flags & G_FILE_MONITOR_WATCH_HARD_LINKS)
    {
      fms->dirname = NULL;
      fms->basename = NULL;
      fms->filename = g_strdup (filename);
    }
  else
    {
      fms->dirname = g_path_get_dirname (filename);
      fms->basename = g_path_get_basename (filename);
      fms->filename = NULL;
    }

  return fms;
}

G_DEFINE_ABSTRACT_TYPE (GLocalFileMonitor, g_local_file_monitor, G_TYPE_FILE_MONITOR)

enum {
  PROP_0,
  PROP_RATE_LIMIT,
};

static void
g_local_file_monitor_get_property (GObject *object, guint prop_id,
                                   GValue *value, GParamSpec *pspec)
{
  GLocalFileMonitor *monitor = G_LOCAL_FILE_MONITOR (object);
  gint64 rate_limit;

  g_assert (prop_id == PROP_RATE_LIMIT);

  rate_limit = g_file_monitor_source_get_rate_limit (monitor->source);
  rate_limit /= G_TIME_SPAN_MILLISECOND;

  g_value_set_int (value, rate_limit);
}

static void
g_local_file_monitor_set_property (GObject *object, guint prop_id,
                                   const GValue *value, GParamSpec *pspec)
{
  GLocalFileMonitor *monitor = G_LOCAL_FILE_MONITOR (object);
  gint64 rate_limit;

  g_assert (prop_id == PROP_RATE_LIMIT);

  rate_limit = g_value_get_int (value);
  rate_limit *= G_TIME_SPAN_MILLISECOND;

  if (g_file_monitor_source_set_rate_limit (monitor->source, rate_limit))
    g_object_notify (object, "rate-limit");
}

static void
g_local_file_monitor_start (GLocalFileMonitor *local_monitor,
                            const gchar       *filename,
                            gboolean           is_directory,
                            GFileMonitorFlags  flags,
                            GMainContext      *context)
{
  GFileMonitorSource *source;

  g_return_if_fail (G_IS_LOCAL_FILE_MONITOR (local_monitor));

  g_assert (!local_monitor->source);

  source = g_file_monitor_source_new (local_monitor, filename, is_directory, flags);
  local_monitor->source = source; /* owns the ref */

  G_LOCAL_FILE_MONITOR_GET_CLASS (local_monitor)->start (local_monitor,
                                                         source->dirname, source->basename, source->filename,
                                                         source);

  g_source_attach ((GSource *) source, context);
}

static void
g_local_file_monitor_dispose (GObject *object)
{
  GLocalFileMonitor *local_monitor = G_LOCAL_FILE_MONITOR (object);

  g_file_monitor_source_dispose (local_monitor->source);

  G_OBJECT_CLASS (g_local_file_monitor_parent_class)->dispose (object);
}

static void
g_local_file_monitor_finalize (GObject *object)
{
  GLocalFileMonitor *local_monitor = G_LOCAL_FILE_MONITOR (object);

  g_source_unref ((GSource *) local_monitor->source);

  G_OBJECT_CLASS (g_local_file_monitor_parent_class)->finalize (object);
}

static void
g_local_file_monitor_init (GLocalFileMonitor* local_monitor)
{
}

static void g_local_file_monitor_class_init (GLocalFileMonitorClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  gobject_class->get_property = g_local_file_monitor_get_property;
  gobject_class->set_property = g_local_file_monitor_set_property;
  gobject_class->dispose = g_local_file_monitor_dispose;
  gobject_class->finalize = g_local_file_monitor_finalize;

  g_object_class_override_property (gobject_class, PROP_RATE_LIMIT, "rate-limit");
}

static GLocalFileMonitor *
g_local_file_monitor_new (gboolean   is_remote_fs,
                          GError   **error)
{
  GType type = G_TYPE_INVALID;

  if (is_remote_fs)
    type = _g_io_module_get_default_type (G_NFS_FILE_MONITOR_EXTENSION_POINT_NAME,
                                          "GIO_USE_FILE_MONITOR",
                                          G_STRUCT_OFFSET (GLocalFileMonitorClass, is_supported));

  if (type == G_TYPE_INVALID)
    type = _g_io_module_get_default_type (G_LOCAL_FILE_MONITOR_EXTENSION_POINT_NAME,
                                          "GIO_USE_FILE_MONITOR",
                                          G_STRUCT_OFFSET (GLocalFileMonitorClass, is_supported));

  if (type == G_TYPE_INVALID)
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                           _("Unable to find default local file monitor type"));
      return NULL;
    }

  return g_object_new (type, NULL);
}

GFileMonitor *
g_local_file_monitor_new_for_path (const gchar        *pathname,
                                   gboolean            is_directory,
                                   GFileMonitorFlags   flags,
                                   GError            **error)
{
  GLocalFileMonitor *monitor;
  gboolean is_remote_fs;

  is_remote_fs = g_local_file_is_remote (pathname);

  monitor = g_local_file_monitor_new (is_remote_fs, error);

  if (monitor)
    g_local_file_monitor_start (monitor, pathname, is_directory, flags, g_main_context_get_thread_default ());

  return G_FILE_MONITOR (monitor);
}

GFileMonitor *
g_local_file_monitor_new_in_worker (const gchar           *pathname,
                                    gboolean               is_directory,
                                    GFileMonitorFlags      flags,
                                    GFileMonitorCallback   callback,
                                    gpointer               user_data,
                                    GError               **error)
{
  GLocalFileMonitor *monitor;
  gboolean is_remote_fs;

  is_remote_fs = g_local_file_is_remote (pathname);

  monitor = g_local_file_monitor_new (is_remote_fs, error);

  if (monitor)
    {
      if (callback)
        g_signal_connect (monitor, "changed", G_CALLBACK (callback), user_data);

      g_local_file_monitor_start (monitor, pathname, is_directory, flags, GLIB_PRIVATE_CALL(g_get_worker_context) ());
    }

  return G_FILE_MONITOR (monitor);
}
