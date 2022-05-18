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

#include <glib.h>

#define ALIGN(size, base) \
  ((base) * (gsize) (((size) + (base) - 1) / (base)))

static void
fill_memory (guint **mem,
             guint   n,
             guint   val)
{
  guint j;

  for (j = 0; j < n; j++)
    mem[j][0] = val;
}

static guint64
access_memory3 (guint  **mema,
                guint  **memb,
                guint  **memd,
                guint    n,
                guint64  repeats)
{
  guint64 accu = 0, i, j;

  for (i = 0; i < repeats; i++)
    {
      for (j = 1; j < n; j += 2)
        memd[j][0] = mema[j][0] + memb[j][0];
    }

  for (i = 0; i < repeats; i++)
    for (j = 0; j < n; j++)
      accu += memd[j][0];

  return accu;
}

static void
touch_mem (guint64 block_size,
           guint64 n_blocks,
           guint64 repeats)
{
  GTimer *timer;
  guint **mema, **memb, **memc;
  guint64 j, accu, n = n_blocks;

  mema = g_new (guint*, n);
  for (j = 0; j < n; j++)
    mema[j] = g_slice_alloc (block_size);

  memb = g_new (guint*, n);
  for (j = 0; j < n; j++)
    memb[j] = g_slice_alloc (block_size);

  memc = g_new (guint*, n);
  for (j = 0; j < n; j++)
    memc[j] = g_slice_alloc (block_size);

  timer = g_timer_new();

  fill_memory (mema, n, 2);
  fill_memory (memb, n, 3);
  fill_memory (memc, n, 4);

  access_memory3 (mema, memb, memc, n, 3);

  g_timer_start (timer);
  accu = access_memory3 (mema, memb, memc, n, repeats);
  g_timer_stop (timer);

  g_test_message ("Access-time = %fs", g_timer_elapsed (timer, NULL));
  g_assert_cmpuint (accu / repeats, ==, (2 + 3) * n / 2 + 4 * n / 2);

  for (j = 0; j < n; j++)
    {
      g_slice_free1 (block_size, mema[j]);
      g_slice_free1 (block_size, memb[j]);
      g_slice_free1 (block_size, memc[j]);
    }

  g_timer_destroy (timer);
  g_free (mema);
  g_free (memb);
  g_free (memc);
}

static void
test_slice_colors (void)
{
  guint64 block_size = 512;
  guint64 area_size = 1024 * 1024;
  guint64 n_blocks, repeats = 1000000;

  /* figure number of blocks from block and area size.
   * divide area by 3 because touch_mem() allocates 3 areas */
  n_blocks = area_size / 3 / ALIGN (block_size, sizeof (gsize) * 2);

  g_test_message ("Allocate and touch %" G_GUINT64_FORMAT
                  " blocks of %" G_GUINT64_FORMAT " bytes"
                  " (= %" G_GUINT64_FORMAT " bytes) %" G_GUINT64_FORMAT
                  " times with color increment",
                  n_blocks, block_size, n_blocks * block_size, repeats);

  touch_mem (block_size, n_blocks, repeats);
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/slice/colors", test_slice_colors);

  return g_test_run ();
}
