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
  GSource  source;

  gpointer fd_tag;
  GQueue   queue;
  gint     fd;
} InotifyKernelSource;

static InotifyKernelSource *inotify_source;

static gint64
ik_source_get_ready_time (InotifyKernelSource *iks)
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
  gint64 ready_time;

  ready_time = ik_source_get_ready_time (iks);

  return 0 <= ready_time && ready_time <= now;
}

static void
ik_source_try_to_pair_head (InotifyKernelSource *iks)
{
  ik_event_t *head;
  GList *node;

  node = g_queue_peek_head_link (&iks->queue);

  if (!node)
    return;

  head = node->data;

  /* we should only be here if... */
  g_assert (head->mask & IN_MOVED_FROM && !head->pair);

  while ((node = node->next))
    {
      ik_event_t *candidate = node->data;

      if (candidate->cookie == head->cookie && candidate->mask & IN_MOVED_TO)
        {
          g_queue_remove (&iks->queue, candidate);
          candidate->is_second_in_pair = TRUE;
          head->pair = candidate;
          candidate->pair = head;
          return;
        }
    }
}

static gboolean
ik_source_dispatch (GSource     *source,
                    GSourceFunc  func,
                    gpointer     user_data)
{
  InotifyKernelSource *iks = (InotifyKernelSource *) source;
  gboolean (*user_callback) (ik_event_t *event) = (void *) func;
  gint64 now = g_source_get_time (source);

  now = g_source_get_time (source);

  /* Only try to fill the queue if we don't already have work to do. */
  if (!ik_source_can_dispatch_now (iks, now) &&
      g_source_query_unix_fd (source, iks->fd_tag))
    {
      gchar buffer[sizeof(struct inotify_event) + NAME_MAX + 1];
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
          struct inotify_event *event = (struct inotify_event *) (buffer + offset);

          g_queue_push_tail (&iks->queue, ik_event_new (event, now));

          offset += sizeof (struct inotify_event) + event->len;
        }
    }

  if (!ik_source_can_dispatch_now (iks, now))
    ik_source_try_to_pair_head (iks);

  if (ik_source_can_dispatch_now (iks, now))
    {
      ik_event_t *event;

      /* callback will free the event */
      event = g_queue_pop_head (&iks->queue);

      G_LOCK (inotify_lock);
      (* user_callback) (event);
      G_UNLOCK (inotify_lock);
    }

  g_source_set_ready_time (source, ik_source_get_ready_time (iks));

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

  iks->fd = inotify_init1 (IN_CLOEXEC);

  if (iks->fd < 0)
    iks->fd = inotify_init ();

  if (iks->fd >= 0)
    iks->fd_tag = g_source_add_unix_fd (source, iks->fd, G_IO_IN);

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
