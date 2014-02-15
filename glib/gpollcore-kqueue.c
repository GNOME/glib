/*
 * Copyright © 2014 Canonical Limited
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the licence, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Ryan Lortie <desrt@desrt.ca>
 */

#include "config.h"

#include "gpollcore.h"

#include <sys/poll.h>
#include <unistd.h>
#include <errno.h>

static gboolean
g_poll_core_create_ready_time_update (struct kevent *events,
                                      gint           n_events,
                                      gint          *n_changes,
                                      gint64         ready_time)
{
  if (*n_changes == n_events)
    return FALSE;

  if (ready_time < 0)
    {
      EV_SET (&events[*n_changes], 0, EVFILT_TIMER, EV_DELETE, 0, 0, NULL);
    }
  else
    {
#if defined(NOTE_USECONDS) && defined(NOTE_ABSOLUTE)
      /* MacOS has a more-capable kevent() than the BSDs.
       *
       * It allows us to set the timer as an absolute monotonic time and
       * also allows for microsecond accuracy.
       */
      EV_SET (&events[*n_changes], 0, EVFILT_TIMER, EV_ADD | EV_ONESHOT,
              NOTE_ABSOLUTE | NOTE_USECONDS, ready_time, NULL);
#else
      /* Need to do calculations to get milliseconds of relative time */
      gint timeout;

      if (ready_time > 0)
        {
          gint64 now = g_get_monotonic_time ();

          if (now < ready_time)
            timeout = (ready_time - now + 999) / 1000;
          else
            timeout = 0;
        }
      else /* ready_time == 0 */
        timeout = 0;

      EV_SET (&events[*n_changes], 0, EVFILT_TIMER, EV_ADD | EV_ONESHOT, 0, timeout, NULL);
#endif
    }

  (*n_changes)++;

  return TRUE;
}

static gboolean
g_poll_core_create_fd_update (struct kevent *events,
                              gint           n_events,
                              gint          *n_changes,
                              gint           fd,
                              guint          old_events,
                              guint          new_events,
                              gpointer       user_data)
{
  if ((old_events ^ new_events) & G_IO_IN)
    {
      if (*n_changes == n_events)
        return FALSE;

      if (new_events & G_IO_IN)
        EV_SET (&events[*n_changes], fd, EVFILT_READ, EV_ADD, 0, 0, user_data);
      else
        EV_SET (&events[*n_changes], fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);

      (*n_changes)++;
    }

  if ((old_events ^ new_events) & G_IO_OUT)
    {
      if (*n_changes == n_events)
        return FALSE;

      if (new_events & G_IO_OUT)
        EV_SET (&events[*n_changes], fd, EVFILT_WRITE, EV_ADD, 0, 0, user_data);
      else
        EV_SET (&events[*n_changes], fd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);

      (*n_changes)++;
    }

  return TRUE;
}

void
g_poll_core_update (GPollCore *core,
                    gint       fd,
                    guint      old_events,
                    guint      new_events,
                    gpointer   user_data)
{
  struct kevent kev[2];
  gint n_changes = 0;

  g_poll_core_create_fd_update (kev, G_N_ELEMENTS (kev), &n_changes, fd, old_events, new_events, user_data);
  kevent (core->kqueue_fd, kev, n_changes, NULL, 0, NULL);
}

void
g_poll_core_set_ready_time (GPollCore *core,
                            gint64     ready_time)
{
  struct kevent kev[1];
  gint n_changes = 0;

  g_poll_core_create_ready_time_update (kev, G_N_ELEMENTS (kev), &n_changes, ready_time);
  kevent (core->kqueue_timer, kev, n_changes, NULL, 0, NULL);
}

void
g_poll_core_wait (GPollCore *core,
                  GMutex    *mutex)
{
  struct pollfd pfd;

  pfd.fd = core->kqueue_fd;
  pfd.events = POLLIN;

  g_mutex_unlock (mutex);
  poll (&pfd, 1, -1);
  g_mutex_lock (mutex);
}

gint
g_poll_core_update_and_collect (GPollCore  *core,
                                GHashTable *updates,
                                gint64     *ready_time_update,
                                GPollEvent *events,
                                gint        max_events)
{
  struct timespec zero = { 0, 0 };
  gint n_changes = 0;

  if (updates)
    {
      GHashTableIter iter;
      gpointer key, value;

      g_hash_table_iter_init (&iter, updates);
      while (g_hash_table_iter_next (&iter, &key, &value))
        {
          GPollUpdate *update = value;

          if (!g_poll_core_create_fd_update (events, max_events, &n_changes, GPOINTER_TO_INT (key),
                                             update->old_events, update->new_events, update->user_data))
            return n_changes;
        }
    }

  /* We convert absolute to relative time here, so try to do it as close
   * as possible to the kevent() call.
   */
  if (ready_time_update)
    g_poll_core_set_ready_time (core, *ready_time_update);
    //if (!g_poll_core_create_ready_time_update (events, max_events, &n_changes, *ready_time_update))
      //return n_changes;

  return kevent (core->kqueue_fd, events, n_changes, events, max_events, &zero);
}

gint
g_poll_core_get_unix_fd (GPollCore *core)
{
  return core->kqueue_fd;
}

void
g_poll_core_init (GPollCore *core)
{
  struct kevent ev;
  gint ret;

  core->kqueue_fd = kqueue ();
  if (core->kqueue_fd < 0)
    g_error ("gpollcore: kqueue() fail: %s", g_strerror (errno));

  core->kqueue_timer = kqueue ();
  if (core->kqueue_timer < 0)
    g_error ("gpollcore: kqueue() fail [timer]: %s", g_strerror (errno));

  EV_SET (&ev, core->kqueue_timer, EVFILT_READ, EV_ADD, 0, 0, NULL);
  ret = kevent (core->kqueue_fd, &ev, 1, NULL, 0, NULL);
  if (ret < 0)
    g_error ("gpollcore: kevent() fail [init]: %s", g_strerror (errno));
}

void
g_poll_core_clear (GPollCore *core)
{
  close (core->kqueue_fd);
}
