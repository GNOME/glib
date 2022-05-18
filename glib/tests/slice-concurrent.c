/* test for gslice cross thread allocation/free
 * Copyright (C) 2006 Stefan Westerfeld
 * Copyright (C) 2007 Tim Janik
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
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
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include <glib.h>

#include <stdlib.h>

#define N_THREADS       8
#define N_ALLOCS        50000
#define MAX_BLOCK_SIZE  64

struct ThreadData
{
  int      thread_id;
  GThread* gthread;

  GMutex   to_free_mutex;
  void*    to_free [N_THREADS * N_ALLOCS];
  int      bytes_to_free [N_THREADS * N_ALLOCS];
  int      n_to_free;
  int      n_freed;
} tdata[N_THREADS];

static void *
thread_func (void *arg)
{
  int i;
  struct ThreadData *td = arg;

  for (i = 0; i < N_ALLOCS; i++)
    {
      int bytes, f, t;
      char *mem;

      if (rand() % (N_ALLOCS / 20) == 0)
        g_test_message ("%c", 'a' - 1 + td->thread_id);

      /* allocate block of random size and randomly fill */
      bytes = rand() % MAX_BLOCK_SIZE + 1;
      mem = g_slice_alloc (bytes);

      for (f = 0; f < bytes; f++)
        mem[f] = rand();

      /* associate block with random thread */
      t = rand() % N_THREADS;
      g_mutex_lock (&tdata[t].to_free_mutex);
      tdata[t].to_free[tdata[t].n_to_free] = mem;
      tdata[t].bytes_to_free[tdata[t].n_to_free] = bytes;
      tdata[t].n_to_free++;
      g_mutex_unlock (&tdata[t].to_free_mutex);

      /* shuffle thread execution order every once in a while */
      if (rand() % 97 == 0)
        {
          if (rand() % 2)
            g_thread_yield();   /* concurrent shuffling for single core */
          else
            g_usleep (1000);    /* concurrent shuffling for multi core */
        }

      /* free a block associated with this thread */
      g_mutex_lock (&td->to_free_mutex);
      if (td->n_to_free > 0)
        {
          td->n_to_free--;
          g_slice_free1 (td->bytes_to_free[td->n_to_free],
                         td->to_free[td->n_to_free]);
          td->n_freed++;
        }
      g_mutex_unlock (&td->to_free_mutex);
    }

  return NULL;
}

static void
test_concurrent_slice (void)
{
  int t;

  for (t = 0; t < N_THREADS; t++)
    {
      tdata[t].thread_id = t + 1;
      tdata[t].n_to_free = 0;
      tdata[t].n_freed = 0;
    }

  for (t = 0; t < N_THREADS; t++)
    {
      tdata[t].gthread = g_thread_new (NULL, thread_func, &tdata[t]);
      g_assert_nonnull (tdata[t].gthread);
    }

  for (t = 0; t < N_THREADS; t++)
    {
      g_thread_join (tdata[t].gthread);
    }

  for (t = 0; t < N_THREADS; t++)
    {
      g_test_message ("Thread %d: %d blocks freed, %d blocks not freed",
                      tdata[t].thread_id, tdata[t].n_freed, tdata[t].n_to_free);
    }
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/slice/concurrent", test_concurrent_slice);

  return g_test_run ();
}
