/* GLIB sliced memory - fast threaded memory chunk allocator
 * Copyright (C) 2005 Tim Janik
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
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#include <glib.h>

#include <stdio.h>
#include <string.h>
#include <sys/time.h> // gettimeofday

#define quick_rand32()  (rand_accu = 1664525 * rand_accu + 1013904223, rand_accu)
static guint prime_size = 1021; // 769; // 509

static gpointer
test_sliced_mem_thread (gpointer data)
{
  guint32 rand_accu = 2147483563;
  /* initialize random numbers */
  if (data)
    rand_accu = *(guint32*) data;
  else
    {
      struct timeval rand_tv;
      gettimeofday (&rand_tv, NULL);
      rand_accu = rand_tv.tv_usec + (rand_tv.tv_sec << 16);
    }

  guint i, m = 10000;   /* number of blocks */
  guint j, n = 10000;   /* number of alloc+free repetitions */
  guint8 **ps = g_new (guint8*, m);
  guint   *ss = g_new (guint, m);
  /* create m random sizes */
  for (i = 0; i < m; i++)
    ss[i] = quick_rand32() % prime_size;
  /* allocate m blocks */
  for (i = 0; i < m; i++)
    ps[i] = g_slice_alloc (ss[i]);
  for (j = 0; j < n; j++)
    {
      /* free m/2 blocks */
      for (i = 0; i < m; i += 2)
        g_slice_free1 (ss[i], ps[i]);
      /* allocate m/2 blocks with new sizes */
      for (i = 0; i < m; i += 2)
        {
          ss[i] = quick_rand32() % prime_size;
          ps[i] = g_slice_alloc (ss[i]);
        }
    }
  /* free m blocks */
  for (i = 0; i < m; i++)
    g_slice_free1 (ss[i], ps[i]);
  /* alloc and free many equally sized chunks in a row */
  for (i = 0; i < n; i++)
    {
      guint sz = quick_rand32() % prime_size;
      guint k = m / 100;
      for (j = 0; j < k; j++)
        ps[j] = g_slice_alloc (sz);
      for (j = 0; j < k; j++)
        g_slice_free1 (sz, ps[j]);
    }

  return NULL;
}

static void
usage (void)
{
  g_print ("Usage: gslicedmemory [n_threads] [G|S|M][f][c] [maxblocksize] [seed]\n");
}

int
main (int   argc,
      char *argv[])
{
  guint seed32, *seedp = NULL;
  gboolean ccounters = FALSE;
  guint n_threads = 1;
  const gchar *mode = "slab allocator + magazine cache", *emode = " ";
  if (argc > 1)
    n_threads = g_ascii_strtoull (argv[1], NULL, 10);
  if (argc > 2)
    {
      guint i, l = strlen (argv[2]);
      for (i = 0; i < l; i++)
        switch (argv[2][i])
          {
          case 'G': /* GLib mode */
            g_slice_set_config (G_SLICE_CONFIG_ALWAYS_MALLOC, FALSE);
            g_slice_set_config (G_SLICE_CONFIG_BYPASS_MAGAZINES, FALSE);
            mode = "slab allocator + magazine cache";
            break;
          case 'S': /* slab mode */
            g_slice_set_config (G_SLICE_CONFIG_ALWAYS_MALLOC, FALSE);
            g_slice_set_config (G_SLICE_CONFIG_BYPASS_MAGAZINES, TRUE);
            mode = "slab allocator";
            break;
          case 'M': /* malloc mode */
            g_slice_set_config (G_SLICE_CONFIG_ALWAYS_MALLOC, TRUE);
            mode = "system malloc";
            break;
          case 'f': /* eager freeing */
            g_slice_set_config (G_SLICE_CONFIG_ALWAYS_FREE, TRUE);
            emode = " with eager freeing";
            break;
          case 'c': /* print contention counters */
            ccounters = TRUE;
            break;
          default:
            usage();
            return 1;
          }
    }
  if (argc > 3)
    prime_size = g_ascii_strtoull (argv[3], NULL, 10);
  if (argc > 4)
    {
      seed32 = g_ascii_strtoull (argv[4], NULL, 10);
      seedp = &seed32;
    }

  g_thread_init (NULL);

  if (argc <= 1)
    usage();

  gchar strseed[64] = "<random>";
  if (seedp)
    g_snprintf (strseed, 64, "%u", *seedp);
  g_print ("Starting %d threads allocating random blocks <= %u bytes with seed=%s using %s%s\n", n_threads, prime_size, strseed, mode, emode);
  
  GThread *threads[n_threads];
  guint i;
  for (i = 0; i < n_threads; i++)
    threads[i] = g_thread_create_full (test_sliced_mem_thread, seedp, 0, TRUE, FALSE, 0, NULL);
  for (i = 0; i < n_threads; i++)
    g_thread_join (threads[i]);
  
  if (ccounters)
    {
      guint n, n_chunks = g_slice_get_config (G_SLICE_CONFIG_CHUNK_SIZES);
      g_print ("    ChunkSize | MagazineSize | Contention\n");
      for (i = 0; i < n_chunks; i++)
        {
          gint64 *vals = g_slice_get_config_state (G_SLICE_CONFIG_CONTENTION_COUNTER, i, &n);
          g_print ("  %9llu   |  %9llu   |  %9llu\n", vals[0], vals[2], vals[1]);
          g_free (vals);
        }
    }
  else
    g_print ("Done.\n");
  return 0;
}
