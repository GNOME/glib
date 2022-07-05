/* GLIB sliced memory - fast threaded memory chunk allocator
 * Copyright (C) 2005 Tim Janik
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

/* We are testing some deprecated APIs here */
#ifndef GLIB_DISABLE_DEPRECATION_WARNINGS
#define GLIB_DISABLE_DEPRECATION_WARNINGS
#endif

#include <glib.h>

#define quick_rand32() \
  (rand_accu = 1664525 * rand_accu + 1013904223, rand_accu)

static guint    prime_size = 1021; /* 769; 509 */
static guint    number_of_blocks = 10000;          /* total number of blocks allocated */
static guint    number_of_repetitions = 10000;     /* number of alloc+free repetitions */
static gboolean want_corruption = FALSE;

/* --- old memchunk prototypes (memchunks.c) --- */
GMemChunk*      old_mem_chunk_new       (const gchar  *name,
                                         gulong        atom_size,
                                         gulong        area_size,
                                         gint          type);
void            old_mem_chunk_destroy   (GMemChunk *mem_chunk);
gpointer        old_mem_chunk_alloc     (GMemChunk *mem_chunk);
gpointer        old_mem_chunk_alloc0    (GMemChunk *mem_chunk);
void            old_mem_chunk_free      (GMemChunk *mem_chunk,
                                         gpointer   mem);
void            old_mem_chunk_clean     (GMemChunk *mem_chunk);
void            old_mem_chunk_reset     (GMemChunk *mem_chunk);
void            old_mem_chunk_print     (GMemChunk *mem_chunk);
void            old_mem_chunk_info      (void);

#ifndef G_ALLOC_AND_FREE
#define G_ALLOC_AND_FREE  2
#endif

/* --- functions --- */
static inline int
corruption (void)
{
  if (G_UNLIKELY (want_corruption))
    {
      /* corruption per call likelyness is about 1:4000000 */
      guint32 r = g_random_int() % 8000009;
      return r == 277 ? +1 : r == 281 ? -1 : 0;
    }
  return 0;
}

static gpointer
test_sliced_mem_thread (gpointer data)
{
  guint32 rand_accu = 2147483563;
  guint i, j;
  guint8 **ps;
  guint   *ss;

  /* initialize random numbers */
  if (data)
    rand_accu = *(guint32*) data;
  else
    {
      GTimeVal rand_tv;
      g_get_current_time (&rand_tv);
      rand_accu = rand_tv.tv_usec + (rand_tv.tv_sec << 16);
    }

  ps = g_new (guint8*, number_of_blocks);
  ss = g_new (guint, number_of_blocks);
  /* create number_of_blocks random sizes */
  for (i = 0; i < number_of_blocks; i++)
    ss[i] = quick_rand32() % prime_size;
  /* allocate number_of_blocks blocks */
  for (i = 0; i < number_of_blocks; i++)
    ps[i] = g_slice_alloc (ss[i] + corruption());
  for (j = 0; j < number_of_repetitions; j++)
    {
      /* free number_of_blocks/2 blocks */
      for (i = 0; i < number_of_blocks; i += 2)
        g_slice_free1 (ss[i] + corruption(), ps[i] + corruption());
      /* allocate number_of_blocks/2 blocks with new sizes */
      for (i = 0; i < number_of_blocks; i += 2)
        {
          ss[i] = quick_rand32() % prime_size;
          ps[i] = g_slice_alloc (ss[i] + corruption());
        }
    }
  /* free number_of_blocks blocks */
  for (i = 0; i < number_of_blocks; i++)
    g_slice_free1 (ss[i] + corruption(), ps[i] + corruption());
  /* alloc and free many equally sized chunks in a row */
  for (i = 0; i < number_of_repetitions; i++)
    {
      guint sz = quick_rand32() % prime_size;
      guint k = number_of_blocks / 100;
      for (j = 0; j < k; j++)
        ps[j] = g_slice_alloc (sz + corruption());
      for (j = 0; j < k; j++)
        g_slice_free1 (sz + corruption(), ps[j] + corruption());
    }
  g_free (ps);
  g_free (ss);

  return NULL;
}

static void
test_slice_glib (void)
{
  GThread **threads;
  guint i, n_threads = 1;

  g_test_message ("Starting %d threads allocating random blocks <= %u bytes",
                  n_threads, prime_size);

  threads = g_alloca (sizeof(GThread*) * n_threads);

  for (i = 0; i < n_threads; i++)
    threads[i] = g_thread_create (test_sliced_mem_thread, NULL, TRUE, NULL);

  for (i = 0; i < n_threads; i++)
    g_thread_join (threads[i]);
}

int
main (int   argc,
      char *argv[])
{
  g_slice_set_config (G_SLICE_CONFIG_ALWAYS_MALLOC, FALSE);
  g_slice_set_config (G_SLICE_CONFIG_BYPASS_MAGAZINES, FALSE);

  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/slice/glib", test_slice_glib);

  return g_test_run ();
}
