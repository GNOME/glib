/*
   Copyright (C) 2005 John McCutchan
   Copyright © 2015 Canonical Limited

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   see <http://www.gnu.org/licenses/>.

   Authors:
     Ryan Lortie <desrt@desrt.ca>
     John McCutchan <john@johnmccutchan.com>
*/

#include "config.h"

#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <glib.h>
#include "inotify-kernel.h"
#include <sys/inotify.h>
#include <glib/glib-unix.h>

#include "glib-private.h"

/* Thresholds for the boredom algorithm */
#define BOREDOM_MIN          (1 * G_TIME_SPAN_MILLISECOND)
#define BOREDOM_MAX          (1 * G_TIME_SPAN_SECOND)
#define BOREDOM_THRESHOLD    (16 * G_TIME_SPAN_MILLISECOND)
#define BOREDOM_FACTOR       (2)

/* Define limits on the maximum amount of time and maximum amount of
 * interceding events between FROM/TO that can be merged.
 */
#define MOVE_PAIR_DELAY      (10 * G_TIME_SPAN_MILLISECOND)
#define MOVE_PAIR_DISTANCE   (100)

/* We use the lock from inotify-helper.c
 *
 * We only have to take it on our read callback.
 *
 * The rest of locking is taken care of in inotify-helper.c
 */
G_LOCK_EXTERN (inotify_lock);

static ik_event_t *
ik_event_new (struct inotify_event *kevent,
              gint64                now)
{
  ik_event_t *event = g_new0 (ik_event_t, 1);

  event->wd = kevent->wd;
  event->mask = kevent->mask;
  event->cookie = kevent->cookie;
  event->len = kevent->len;
  event->timestamp = now;
  if (event->len)
    event->name = g_strdup (kevent->name);
  else
    event->name = g_strdup ("");

  return event;
}

void
_ik_event_free (ik_event_t *event)
{
  if (event->pair)
    {
      event->pair->pair = NULL;
      _ik_event_free (event->pair);
    }

  g_free (event->name);
  g_free (event);
}

typedef struct
{
  GSource     source;

  GQueue      queue;
  gpointer    fd_tag;
  gint        fd;

  GHashTable *unmatched_moves;
  GTimeSpan   boredom;
  gboolean    ignored;
} InotifyKernelSource;

static InotifyKernelSource *inotify_source;

static gint64
ik_source_get_dispatch_time (InotifyKernelSource *iks)
{
  ik_event_t *head;

  head = g_queue_peek_head (&iks->queue);

  /* nothing in the queue: not ready */
  if (!head)
    return -1;

  /* if it's not an unpaired move, it is ready now */
  if (~head->mask & IN_MOVED_FROM || head->pair)
    return 0;

  /* if the queue is too long then it's ready now */
  if (iks->queue.length > MOVE_PAIR_DISTANCE)
    return 0;

  /* otherwise, it's ready after the delay */
  return head->timestamp + MOVE_PAIR_DELAY;
}

static gboolean
ik_source_can_dispatch_now (InotifyKernelSource *iks,
                            gint64               now)
{
  gint64 dispatch_time;

  dispatch_time = ik_source_get_dispatch_time (iks);

  return 0 <= dispatch_time && dispatch_time <= now;
}

static gboolean
ik_source_is_bored (InotifyKernelSource *iks)
{
  return iks->boredom > BOREDOM_THRESHOLD;
}

static gboolean
ik_source_dispatch (GSource     *source,
                    GSourceFunc  func,
                    gpointer     user_data)
{
  InotifyKernelSource *iks = (InotifyKernelSource *) source;
  gboolean (*user_callback) (ik_event_t *event) = (void *) func;
  gboolean interesting = FALSE;
  gboolean is_ready;
  gint64 now;

  now = g_source_get_time (source);

  /* If we woke up after a timeout caused by boredom, check to see if we
   * actually have anything to read.  If not, go back to waiting for the
   * file descriptor to become ready.
   */
  if (ik_source_is_bored (iks) && g_source_get_ready_time (source))
    {
      GPollFD pollfd;
      gint n_ready;

      pollfd.fd = iks->fd;
      pollfd.events = G_IO_IN;

      do
        n_ready = g_poll (&pollfd, 1, 0);
      while (n_ready == -1 && errno == EINTR);

      if (n_ready == -1)
        g_error ("Unexpected error on poll() of inotify: %s\n", g_strerror (errno));

      if (!n_ready)
        {
          /* Timeout fired but there is nothing to read.  Switch back to
           * waiting for the fd to become ready, but do not reset
           * boredom.  We don't want to cancel our back-off and start
           * all over again just because the delay got longer than the
           * frequency of the change notifications.
           */
          g_source_modify_unix_fd (source, iks->fd_tag, G_IO_IN);
          g_source_set_ready_time (source, 0);

          return TRUE;
        }

      is_ready = TRUE;
    }
  else
    is_ready = g_source_query_unix_fd (source, iks->fd_tag);

  if (is_ready && !ik_source_can_dispatch_now (iks, now))
    {
      gchar buffer[256 * 1024];
      gssize result;
      gssize offset;

      result = read (iks->fd, buffer, sizeof buffer);

      if (result < 0)
        g_error ("inotify error: %s\n", g_strerror (errno));
      else if (result == 0)
        g_error ("inotify unexpectedly hit eof");

      offset = 0;

      while (offset < result)
        {
          struct inotify_event *kevent = (struct inotify_event *) (buffer + offset);
          ik_event_t *event;

          event = ik_event_new (kevent, now);

          offset += sizeof (struct inotify_event) + event->len;

          if (event->mask & IN_MOVED_TO)
            {
              ik_event_t *pair;

              pair = g_hash_table_lookup (iks->unmatched_moves, GUINT_TO_POINTER (event->cookie));
              if (pair != NULL)
                {
                  g_assert (!pair->pair);

                  g_hash_table_remove (iks->unmatched_moves, GUINT_TO_POINTER (event->cookie));
                  event->is_second_in_pair = TRUE;
                  event->pair = pair;
                  pair->pair = event;
                  continue;
                }
            }

          else if (event->mask & IN_MOVED_FROM)
            {
              gboolean new;

              new = g_hash_table_insert (iks->unmatched_moves, GUINT_TO_POINTER (event->cookie), event);
              if G_UNLIKELY (!new)
                g_warning ("inotify: got IN_MOVED_FROM event with already-pending cookie %#x", event->cookie);
            }

          g_queue_push_tail (&iks->queue, event);
        }
    }

  while (ik_source_can_dispatch_now (iks, now))
    {
      ik_event_t *event;

      /* callback will free the event */
      event = g_queue_pop_head (&iks->queue);

      if (event->mask & IN_MOVED_TO && !event->pair)
        g_hash_table_remove (iks->unmatched_moves, GUINT_TO_POINTER (event->cookie));

      G_LOCK (inotify_lock);
      interesting |= (* user_callback) (event);
      G_UNLOCK (inotify_lock);
    }

  /* The queue gets blocked iff we have unmatched moves */
  g_assert ((iks->queue.length > 0) == (g_hash_table_size (iks->unmatched_moves) > 0));

  /* Unpaired moves are interesting since they will be reported to the
   * user, one way or another.  We also want to resolve them as soon as
   * possible.
   */
  interesting |= iks->queue.length > 0;

  if (!interesting)
    {
      iks->boredom = MAX(BOREDOM_MIN, MIN(iks->boredom * BOREDOM_FACTOR, BOREDOM_MAX));

      if (ik_source_is_bored (iks))
        {
          g_source_set_ready_time (source, now + iks->boredom);
          g_source_modify_unix_fd (source, iks->fd_tag, 0);
        }
    }
  else
    {
      g_source_set_ready_time (source, ik_source_get_dispatch_time (iks));
      g_source_modify_unix_fd (source, iks->fd_tag, G_IO_IN);
      iks->boredom = 0;
    }

  return TRUE;
}

static InotifyKernelSource *
ik_source_new (gboolean (* callback) (ik_event_t *event))
{
  static GSourceFuncs source_funcs = {
    NULL, NULL,
    ik_source_dispatch
    /* should have a finalize, but it will never happen */
  };
  InotifyKernelSource *iks;
  GSource *source;

  source = g_source_new (&source_funcs, sizeof (InotifyKernelSource));
  iks = (InotifyKernelSource *) source;

  g_source_set_name (source, "inotify kernel source");

  iks->unmatched_moves = g_hash_table_new (NULL, NULL);
  iks->fd = inotify_init1 (IN_CLOEXEC);

  if (iks->fd < 0)
    iks->fd = inotify_init ();

  if (iks->fd >= 0)
    {
      GError *error = NULL;

      g_unix_set_fd_nonblocking (iks->fd, TRUE, &error);
      g_assert_no_error (error);

      iks->fd_tag = g_source_add_unix_fd (source, iks->fd, G_IO_IN);
    }

  g_source_set_callback (source, (GSourceFunc) callback, NULL, NULL);

  g_source_attach (source, GLIB_PRIVATE_CALL (g_get_worker_context) ());

  return iks;
}

gboolean
_ik_startup (gboolean (*cb)(ik_event_t *event))
{
  if (g_once_init_enter (&inotify_source))
    g_once_init_leave (&inotify_source, ik_source_new (cb));

  return inotify_source->fd >= 0;
}

gint32
_ik_watch (const char *path,
           guint32     mask,
           int        *err)
{
  gint32 wd = -1;
  
  g_assert (path != NULL);
  g_assert (inotify_source && inotify_source->fd >= 0);
  
  wd = inotify_add_watch (inotify_source->fd, path, mask);
  
  if (wd < 0)
    {
      int e = errno;
      /* FIXME: debug msg failed to add watch */
      if (err)
	*err = e;
      return wd;
    }
  
  g_assert (wd >= 0);
  return wd;
}

int
_ik_ignore (const char *path, 
            gint32      wd)
{
  g_assert (wd >= 0);
  g_assert (inotify_source && inotify_source->fd >= 0);
  
  if (inotify_rm_watch (inotify_source->fd, wd) < 0)
    {
      /* int e = errno; */
      /* failed to rm watch */
      return -1;
    }
  
  return 0;
}
