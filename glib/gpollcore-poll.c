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
#include <string.h>
#include <errno.h>

static void
g_poll_core_update_locked (GPollCore *core,
                           gint       fd,
                           guint      old_events,
                           guint      new_events,
                           gpointer   user_data)
{
  gint position;

  if (old_events)
    {
      for (position = 0; position < core->n_pfds; position++)
        if (core->pfds[position].fd == fd)
          break;
      g_assert (position < core->n_pfds);
    }
  else
    position = core->n_pfds;

  if (new_events)
    {
      if (position == core->n_allocated_pfds)
        {
          core->n_allocated_pfds *= 2;
          core->pfds = g_renew (struct pollfd, core->pfds, core->n_allocated_pfds);
          core->user_data = g_renew (gpointer, core->user_data, core->n_allocated_pfds);
        }

      core->pfds[position].fd = fd;
      core->pfds[position].events = new_events;
      core->user_data[position] = user_data;

      if (position == core->n_pfds)
        core->n_pfds++;
    }
}

void
g_poll_core_update (GPollCore *core,
                    gint       fd,
                    guint      old_events,
                    guint      new_events,
                    gpointer   user_data)
{
  g_mutex_lock (&core->mutex);

  g_poll_core_update_locked (core, fd, old_events, new_events, user_data);

  if (core->waiting)
    {
      gint ret;

      do
        ret = write (core->pipes[1], "x", 1);
      while (ret == -1 && errno == EINTR);
    }

  g_mutex_unlock (&core->mutex);
}

void
g_poll_core_set_ready_time (GPollCore *core,
                            gint64     ready_time)
{
  g_mutex_lock (&core->mutex);

  /* We want to wake the owner thread if it is sleeping and if the
   * current timeout is greater than the new one.
   */
  if (core->waiting && ready_time >= 0 && ready_time < core->ready_time)
    {
      gint ret;

      do
        ret = write (core->pipes[1], "x", 1);
      while (ret == -1 && errno == EINTR);
    }

  core->ready_time = ready_time;

  g_mutex_unlock (&core->mutex);
}

void
g_poll_core_wait (GPollCore *core)
{
  struct pollfd *pfds;
  gint n_pfds;
  gint timeout;
  gint result;

  g_mutex_lock (&core->mutex);

again:
  pfds = g_new (struct pollfd, core->n_pfds + 1);
  pfds[0].fd = core->pipes[0];
  pfds[1].events = POLLIN;
  memcpy (pfds + 1, core->pfds, sizeof (struct pollfd) * core->n_pfds);
  n_pfds = core->n_pfds + 1;

  if (core->ready_time > 0)
    {
      gint64 now = g_get_monotonic_time ();

      if (now < core->ready_time)
        timeout = (core->ready_time - now + 999) / 1000;
      else
        timeout = 0;
    }
  else if (core->ready_time == 0)
    timeout = 0;
  else
    timeout = -1;

  core->waiting = TRUE;

  g_mutex_unlock (&core->mutex);

  do
    result = poll (pfds, n_pfds, timeout);
  while (result < 0 && errno == EINTR);

  if (result < 0)
    g_error ("gpollcore: poll() fail [wait]: %s", g_strerror (errno));

  g_mutex_lock (&core->mutex);

  core->waiting = FALSE;

  if (pfds[0].revents & POLLIN)
    {
      char buffer[20];

      while (read (core->pipes[0], buffer, sizeof buffer) > 0)
        ;
      g_free (pfds);
      goto again;
    }

  g_free (pfds);

  g_mutex_unlock (&core->mutex);
}

gint
g_poll_core_update_and_collect (GPollCore  *core,
                                GHashTable *updates,
                                gint64     *ready_time_update,
                                GPollEvent *events,
                                gint        max_events)
{
  gint n_collected = 0;
  gint n_ready;
  gint i;

  /* We are protected by the GMainContext lock here, so no need to use
   * our own...
   */

  /* Make sure there is room for timeout */
  g_assert (max_events >= 1);

  if (ready_time_update)
    core->ready_time = *ready_time_update;

  if (updates)
    {
      GHashTableIter iter;
      gpointer key, value;

      g_hash_table_iter_init (&iter, updates);
      while (g_hash_table_iter_next (&iter, &key, &value))
        {
          GPollUpdate *update = value;

          g_poll_core_update_locked (core, GPOINTER_TO_INT (key),
                                     update->old_events, update->new_events, update->user_data);
        }
    }

  /* Check for timeout */
  if (core->ready_time < g_get_monotonic_time ())
    events[n_collected++].user_data = NULL;

  /* Check the file descriptors */
  do
    n_ready = poll (core->pfds, core->n_pfds, 0);
  while (n_ready < 0 && errno == EINTR);

  if (n_ready < 0)
    g_error ("gpollcore: poll() fail [collect]: %s", g_strerror (errno));

  for (i = 0; n_ready && i < core->n_pfds; i++)
    if (core->pfds[i].revents)
      {
        events[n_collected].revents = core->pfds[i].revents;
        events[n_collected].user_data = core->user_data[i];
        n_collected++;
        n_ready--;
      }

  return n_collected;
}
