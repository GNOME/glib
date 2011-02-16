/*
 * Copyright (C) 2011 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <glib.h>

static int
int_compare_data (gconstpointer p1, gconstpointer p2, gpointer data)
{
  const gint *i1 = p1;
  const gint *i2 = p2;

  return *i1 - *i2;
}

static void
test_sort_basic (void)
{
  gint *data;
  gint i;

  data = g_malloc (10000 * sizeof (int));
  for (i = 0; i < 10000; i++)
    {
      data[i] = g_random_int_range (0, 10000);
    }

  g_qsort_with_data (data, 10000, sizeof (int), int_compare_data, NULL);

  for (i = 1; i < 10000; i++)
    g_assert_cmpint (data[i -1], <=, data[i]);

  g_free (data);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/sort/basic", test_sort_basic);

  return g_test_run ();
}

