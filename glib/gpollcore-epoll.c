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

#include <sys/timerfd.h>
#include <sys/epoll.h>
#include <sys/poll.h>
#include <unistd.h>
#include <errno.h>

void
g_poll_core_update (GPollCore *core,
                    gint       fd,
                    guint      old_events,
                    guint      new_events,
                    gpointer   user_data)
{
  struct epoll_event event;
  gint ret;
  gint op;

  event.events = new_events;
  event.data.ptr = user_data;

  if (old_events == 0)
    op = EPOLL_CTL_ADD;
  else if (new_events == 0)
    op = EPOLL_CTL_DEL;
  else
    op = EPOLL_CTL_MOD;

  ret = epoll_ctl (core->epollfd, op, fd, &event);

  if (ret != 0)
    g_error ("gpollcore: epoll_ctl() fail: %s\n", g_strerror (errno));
}

void
g_poll_core_set_ready_time (GPollCore *core,
                            gint64     ready_time)
{
  struct itimerspec its;
  gint ret;

  if (ready_time >= 0)
    {
      /* Arm */
      its.it_value.tv_sec = (ready_time / G_TIME_SPAN_SECOND);
      its.it_value.tv_nsec = (ready_time % G_TIME_SPAN_SECOND) * 1000;

      /* Make sure we don't disarm the timer for a ready_time of 0 */
      if (!its.it_value.tv_sec && !its.it_value.tv_nsec)
        its.it_value.tv_nsec = 1;
    }
  else
    /* All-zeros = disarm */
    its.it_value.tv_sec = its.it_value.tv_nsec = 0;

  its.it_interval.tv_sec = its.it_interval.tv_nsec = 0;

  ret = timerfd_settime (core->timerfd, TFD_TIMER_ABSTIME, &its, NULL);

  if (ret != 0)
    g_error ("gpollcore: timerfd_settime() fail: %s\n", g_strerror (errno));
}

void
g_poll_core_wait (GPollCore *core)
{
  struct pollfd pfd;

  pfd.fd = core->epollfd;
  pfd.events = POLLIN;

  poll (&pfd, 1, -1);
}

gint
g_poll_core_update_and_collect (GPollCore  *core,
                                GHashTable *updates,
                                gint64     *ready_time_update,
                                GPollEvent *events,
                                gint        max_events)
{
  if (ready_time_update)
    g_poll_core_set_ready_time (core, *ready_time_update);

  if (updates)
    {
      GHashTableIter iter;
      gpointer key, value;

      g_hash_table_iter_init (&iter, updates);
      while (g_hash_table_iter_next (&iter, &key, &value))
        {
          GPollUpdate *update = value;

          g_poll_core_update (core, GPOINTER_TO_INT (key), update->old_events, update->new_events, update->user_data);
        }
    }

  return epoll_wait (core->epollfd, events, max_events, 0);
}

gint
g_poll_core_get_unix_fd (GPollCore *core)
{
  return core->epollfd;
}

void
g_poll_core_init (GPollCore *core)
{
  struct epoll_event ev;
  gint ret;

  core->epollfd = epoll_create1 (EPOLL_CLOEXEC);
  if (core->epollfd < 0)
    g_error ("gpollcore: epoll_create1() fail: %s\n", g_strerror (errno));

  core->timerfd = timerfd_create (CLOCK_MONOTONIC, TFD_CLOEXEC);
  if (core->timerfd < 0)
    g_error ("gpollcore: timerfd_create() fail: %s\n", g_strerror (errno));

  ev.events = EPOLLIN;
  ev.data.ptr = NULL;

  ret = epoll_ctl (core->epollfd, EPOLL_CTL_ADD, core->timerfd, &ev);

  if (ret < 0)
    g_error ("gpollcore: epoll_ctl() fail [init]: %s\n", g_strerror (errno));
}

void
g_poll_core_clear (GPollCore *core)
{
  close (core->epollfd);
  close (core->timerfd);
}
