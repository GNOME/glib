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

#include <glib.h>

#include "gpollcore.h"

#include <sys/poll.h>
#include <unistd.h>

static void
update_free (gpointer data)
{
  GPollUpdate *update = data;

  g_slice_free (GPollUpdate, update);
}

static void
add_update (GHashTable *table,
            gint        fd,
            guint       old_events,
            guint       new_events,
            gpointer    user_data)
{
  GPollUpdate *update;

  update = g_hash_table_lookup (table, GINT_TO_POINTER (fd));

  if (!update)
    {
      update = g_slice_new (GPollUpdate);
      update->old_events = old_events;
      update->user_data = user_data;

      g_hash_table_insert (table, GINT_TO_POINTER (fd), update);
    }
  else
    {
      g_assert_cmpint (update->new_events, ==, old_events);
      g_assert (update->user_data == user_data);
    }

  update->new_events = new_events;

  /* XXX: what to do if only user_data changed? */
  if (update->new_events == update->old_events)
    g_hash_table_remove (table, GINT_TO_POINTER (fd));
}

static gboolean
is_ready (gint fd)
{
  struct pollfd pfd;

  pfd.fd = fd;
  pfd.events = POLLIN;

  return poll (&pfd, 1, 0);
}

static gpointer
kick_core (gpointer user_data)
{
  GPollCore *core = user_data;

  g_usleep (G_TIME_SPAN_SECOND * 0.1);
  g_poll_core_set_ready_time (core, 0);
  return NULL;
}

static void
test_pollcore (void)
{
  GPollEvent events[10];
  GHashTable *updates;
  GPollCore core;
  GMutex lock;
  gint pipes[2];
  gint64 time;
  gchar b;
  gint fd;
  gint r;

  g_mutex_init (&lock);
  g_mutex_lock (&lock);

  updates = g_hash_table_new_full (NULL, NULL, NULL, update_free);

  pipe (pipes);

  g_poll_core_init (&core);

  fd = g_poll_core_get_unix_fd (&core);

  r = g_poll_core_update_and_collect (&core, NULL, NULL, events, G_N_ELEMENTS (events));
  g_assert_cmpint (r, ==, 0);
  g_assert (!is_ready (fd));

  add_update (updates, pipes[1], 0, G_IO_IN, test_pollcore);
  add_update (updates, pipes[1], G_IO_IN, G_IO_OUT, test_pollcore);
  r = g_poll_core_update_and_collect (&core, updates, NULL, events, G_N_ELEMENTS (events));
  g_assert_cmpint (r, ==, 1);
  g_hash_table_remove_all (updates);
  g_assert (is_ready (fd));

  add_update (updates, pipes[0], 0, G_IO_IN, test_pollcore);
  r = g_poll_core_update_and_collect (&core, updates, NULL, events, G_N_ELEMENTS (events));
  g_assert_cmpint (r, ==, 1);
  g_hash_table_remove_all (updates);
  g_assert (is_ready (fd));

  write (pipes[1], "x", 1);

  r = g_poll_core_update_and_collect (&core, updates, NULL, events, G_N_ELEMENTS (events));
  g_assert_cmpint (r, ==, 2);
  g_assert (is_ready (fd));

  read (pipes[0], &b, 1);

  time = g_get_monotonic_time () + G_TIME_SPAN_SECOND * 0.1;

  add_update (updates, pipes[1], G_IO_OUT, 0, NULL);
  r = g_poll_core_update_and_collect (&core, updates, &time, events, G_N_ELEMENTS (events));
  g_hash_table_remove_all (updates);
  g_assert_cmpint (r, ==, 0);
  g_assert (!is_ready (fd));

  g_poll_core_wait (&core, &lock);
  g_assert (is_ready (fd));

  r = g_poll_core_update_and_collect (&core, NULL, NULL, events, G_N_ELEMENTS (events));
  g_assert_cmpint (r, ==, 1);
  g_assert (is_ready (fd));

  time = -1;
  r = g_poll_core_update_and_collect (&core, updates, &time, events, G_N_ELEMENTS (events));
  g_assert_cmpint (r, ==, 0);
  g_assert (!is_ready (fd));

  g_thread_unref (g_thread_new ("kicker", kick_core, &core));

  g_poll_core_wait (&core, &lock);
  r = g_poll_core_update_and_collect (&core, NULL, NULL, events, G_N_ELEMENTS (events));
  g_assert_cmpint (r, ==, 1);
  g_assert (is_ready (fd));

  r = g_poll_core_update_and_collect (&core, NULL, &time, events, G_N_ELEMENTS (events));
  g_assert_cmpint (r, ==, 0);
  g_assert (!is_ready (fd));

  g_poll_core_clear (&core);

  g_mutex_unlock (&lock);
  g_mutex_clear (&lock);
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/glib/pollcore", test_pollcore);

  return g_test_run ();
}
