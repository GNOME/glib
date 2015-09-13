/* arraylist.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"

#include <glib.h>

static int test_basic_counter;

static void
test_basic_destroy (gpointer data)
{
  test_basic_counter++;
}

static void
test_basic (GArrayList *al)
{
  const GList *iter, *list;
  gsize i;
  gsize counter = 0;

  g_assert (al != NULL);
  g_assert_cmpint (al->len, ==, 0);

  g_assert_cmpint (GPOINTER_TO_SIZE (g_array_list_first(al)), ==, 0);

  for (i = 1; i <= 1000; i++)
    {
      g_array_list_add (al, GSIZE_TO_POINTER (i));
      g_assert_cmpint (al->len, ==, i);
    }

  g_assert_cmpint (GPOINTER_TO_SIZE (g_array_list_first(al)), ==, 1);
  g_assert_cmpint (GPOINTER_TO_SIZE (g_array_list_last(al)), ==, 1000);

  list = g_array_list_peek (al);

  for (iter = list; iter; iter = iter->next)
    {
      counter++;
      g_assert_cmpint (counter, ==, GPOINTER_TO_SIZE (iter->data));
    }

  g_assert_cmpint (counter, ==, 1000);

  for (i = 1; i <= 500; i++)
    {
      gpointer item = GSIZE_TO_POINTER (i);
      gpointer val = g_array_list_index (al, 0);
      g_assert_cmpint (GPOINTER_TO_SIZE (val), ==, i);
      g_array_list_remove (al, item);
    }

  g_assert_cmpint (al->len, ==, 500);
  g_assert_cmpint (test_basic_counter, ==, 500);
  g_array_list_destroy (al);
  g_assert_cmpint (test_basic_counter, ==, 1000);

  test_basic_counter = 0;
}

static void
test_basic_alloc (void)
{
  test_basic (g_array_list_new (test_basic_destroy));
}

static void
test_basic_stack (void)
{
  GArrayList al;

  g_array_list_init (&al, test_basic_destroy);
  test_basic (&al);
}

int
main (int argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  g_test_bug_base ("http://bugzilla.gnome.org/");

  g_test_add_func ("/GArrayList/heap", test_basic_alloc);
  g_test_add_func ("/GArrayList/stack", test_basic_stack);

  return g_test_run ();
}
