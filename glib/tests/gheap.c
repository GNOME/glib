/* gheap.c
 *
 * Copyright (C) 2014 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <glib.h>

typedef struct
{
   gint64 size;
   gpointer pointer;
} Tuple;

static int
cmpint_rev (gconstpointer a,
            gconstpointer b)
{
   return *(const gint *)b - *(const gint *)a;
}

static int
cmpptr_rev (gconstpointer a,
            gconstpointer b)
{
   return GPOINTER_TO_SIZE(*(gpointer *)b) - GPOINTER_TO_SIZE (*(gpointer *)a);
}

static int
cmptuple_rev (gconstpointer a,
              gconstpointer b)
{
   Tuple *at = (Tuple *)a;
   Tuple *bt = (Tuple *)b;

   return bt->size - at->size;
}

static void
test_GHeap_insert_val_int (void)
{
   GHeap *heap;
   gint i;

   heap = g_heap_new (sizeof (gint), cmpint_rev);

   for (i = 0; i < 100000; i++) {
      g_heap_insert_val (heap, i);
      g_assert_cmpint (heap->len, ==, i + 1);
   }

   for (i = 0; i < 100000; i++) {
      g_assert_cmpint (heap->len, ==, 100000 - i);
      g_assert_cmpint (g_heap_peek (heap, gint), ==, i);
      g_heap_extract (heap, NULL);
   }

   g_heap_unref (heap);
}

static void
test_GHeap_insert_val_ptr (void)
{
   gconstpointer ptr;
   GHeap *heap;
   gint i;

   heap = g_heap_new (sizeof (gpointer), cmpptr_rev);

   for (i = 0; i < 100000; i++) {
      ptr = GINT_TO_POINTER (i);
      g_heap_insert_val (heap, ptr);
      g_assert_cmpint (heap->len, ==, i + 1);
   }

   for (i = 0; i < 100000; i++) {
      g_assert_cmpint (heap->len, ==, 100000 - i);
      g_assert (g_heap_peek (heap, gpointer) == GINT_TO_POINTER (i));
      g_heap_extract (heap, NULL);
   }

   g_heap_unref (heap);
}

static void
test_GHeap_insert_val_tuple (void)
{
   Tuple t;
   GHeap *heap;
   gint i;

   heap = g_heap_new (sizeof (Tuple), cmptuple_rev);

   for (i = 0; i < 100000; i++) {
      t.pointer = GINT_TO_POINTER (i);
      t.size = i;
      g_heap_insert_val (heap, t);
      g_assert_cmpint (heap->len, ==, i + 1);
   }

   for (i = 0; i < 100000; i++) {
      g_assert_cmpint (heap->len, ==, 100000 - i);
      g_assert (g_heap_peek (heap, Tuple).size == i);
      g_assert (g_heap_peek (heap, Tuple).pointer == GINT_TO_POINTER (i));
      g_heap_extract (heap, NULL);
   }

   g_heap_unref (heap);
}

static void
test_GHeap_extract_int (void)
{
   GHeap *heap;
   gint removed[5];
   gint i;
   gint v;

   heap = g_heap_new (sizeof (gint), cmpint_rev);

   for (i = 0; i < 100000; i++) {
      g_heap_insert_val (heap, i);
   }

   removed [0] = g_heap_index (heap, gint, 1578); g_heap_extract_index (heap, 1578, NULL);
   removed [1] = g_heap_index (heap, gint, 2289); g_heap_extract_index (heap, 2289, NULL);
   removed [2] = g_heap_index (heap, gint, 3312); g_heap_extract_index (heap, 3312, NULL);
   removed [3] = g_heap_index (heap, gint, 78901); g_heap_extract_index (heap, 78901, NULL);
   removed [4] = g_heap_index (heap, gint, 99000); g_heap_extract_index (heap, 99000, NULL);

   for (i = 0; i < 100000; i++) {
      if (g_heap_peek (heap, gint) != i) {
         g_assert ((i == removed[0]) ||
                   (i == removed[1]) ||
                   (i == removed[2]) ||
                   (i == removed[3]) ||
                   (i == removed[4]));
      } else {
         g_heap_extract (heap, &v);
         g_assert_cmpint (v, ==, i);
      }
   }

   g_assert_cmpint (heap->len, ==, 0);

   g_heap_unref (heap);
}

int
main (gint   argc,
      gchar *argv[])
{
   g_test_init (&argc, &argv, NULL);
   g_test_bug_base ("http://bugzilla.gnome.org/");

   g_test_add_func ("/GHeap/insert_and_extract<gint>", test_GHeap_insert_val_int);
   g_test_add_func ("/GHeap/insert_and_extract<gpointer>", test_GHeap_insert_val_ptr);
   g_test_add_func ("/GHeap/insert_and_extract<Tuple>", test_GHeap_insert_val_tuple);
   g_test_add_func ("/GHeap/extract_index<int>", test_GHeap_extract_int);

   return g_test_run ();
}
