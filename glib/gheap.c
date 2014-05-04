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

#include <string.h>

#include "gheap.h"

/**
 * SECTION:gheap
 * @title: Heaps
 * @short_description: efficient priority queues using min/max heaps
 *
 * Heaps are similar to a partially sorted tree but implemented as an
 * array. They allow for efficient O(1) lookup of the highest priority
 * item as it will always be the first item of the array.
 *
 * To create a new heap use g_heap_new().
 *
 * To add items to the heap, use g_heap_insert_val() or
 * g_heap_insert_vals() to insert in bulk.
 *
 * To access an item in the heap, use g_heap_index().
 *
 * To remove an arbitrary item from the heap, use g_heap_extract_index().
 *
 * To remove the highest priority item in the heap, use g_heap_extract().
 *
 * To free a heap, use g_heap_unref().
 *
 * Here is an example that stores integers in a #GHeap:
 * |[<!-- language="C" -->
 * static int
 * cmpint (gconstpointer a,
 *         gconstpointer b)
 * {
 *   return *(const gint *)a - *(const gint *)b;
 * }
 *
 * int
 * main (gint   argc,
 *       gchar *argv[])
 * {
 *   GHeap *heap;
 *   gint i;
 *   gint v;
 *
 *   heap = g_heap_new (sizeof (gint), cmpint);
 *
 *   for (i = 0; i < 10000; i++)
 *     g_heap_insert_val (heap, i);
 *   for (i = 0; i < 10000; i++)
 *     g_heap_extract (heap, &v);
 *
 *   g_heap_unref (heap);
 * }
 * ]|
 */

#define MIN_HEAP_SIZE 16

/*
 * Based upon Mastering Algorithms in C by Kyle Loudon.
 * Section 10 - Heaps and Priority Queues.
 */

typedef struct _GHeapReal GHeapReal;

struct _GHeapReal
{
    gchar          *data;
    gsize           len;
    volatile gint   ref_count;
    guint           element_size;
    gsize           allocated_len;
    GCompareFunc    compare;
    gchar           tmp[0];
};

#define heap_parent(npos)   (((npos)-1)/2)
#define heap_left(npos)     (((npos)*2)+1)
#define heap_right(npos)    (((npos)*2)+2)
#define heap_index(h,i)     ((h)->data + (i * (h)->element_size))
#define heap_compare(h,a,b) ((h)->compare(heap_index(h,a), heap_index(h,b)))
#define heap_swap(h,a,b) \
    G_STMT_START { \
        memcpy ((h)->tmp, heap_index (h, a), (h)->element_size); \
        memcpy (heap_index (h, a), heap_index (h, b), (h)->element_size); \
        memcpy (heap_index (h, b), (h)->tmp, (h)->element_size); \
   } G_STMT_END

GHeap *
g_heap_new (guint          element_size,
            GCompareFunc   compare_func)
{
    GHeapReal *real;

    g_return_val_if_fail (element_size, NULL);
    g_return_val_if_fail (compare_func, NULL);

    real = g_malloc_n (1, sizeof (GHeapReal) + element_size);
    real->data = NULL;
    real->len = 0;
    real->ref_count = 1;
    real->element_size = element_size;
    real->allocated_len = 0;
    real->compare = compare_func;

    return (GHeap *)real;
}

GHeap *
g_heap_ref (GHeap *heap)
{
    GHeapReal *real = (GHeapReal *)heap;

    g_return_val_if_fail (heap, NULL);
    g_return_val_if_fail (real->ref_count, NULL);

    g_atomic_int_inc (&real->ref_count);

    return heap;
}

static void
g_heap_real_free (GHeapReal *real)
{
    g_assert (real);
    g_assert_cmpint (real->ref_count, ==, 0);

    g_free (real->data);
    g_free (real);
}

void
g_heap_unref (GHeap *heap)
{
    GHeapReal *real = (GHeapReal *)heap;

    g_return_if_fail (heap);
    g_return_if_fail (real->ref_count);

    if (g_atomic_int_dec_and_test (&real->ref_count)) {
        g_heap_real_free (real);
    }
}

static void
g_heap_real_grow (GHeapReal *real)
{
    g_assert (real);
    g_assert_cmpint (real->allocated_len, <, G_MAXSIZE / 2);

    real->allocated_len = MAX (MIN_HEAP_SIZE, (real->allocated_len * 2));
    real->data = g_realloc_n (real->data,
                              real->allocated_len,
                              real->element_size);
}

static void
g_heap_real_shrink (GHeapReal *real)
{
   g_assert (real);
   g_assert ((real->allocated_len / 2) >= real->len);

   real->allocated_len = MAX (MIN_HEAP_SIZE, real->allocated_len / 2);
   real->data = g_realloc_n (real->data,
                             real->allocated_len,
                             real->element_size);
}

static void
g_heap_real_insert_val (GHeapReal     *real,
                        gconstpointer  data)
{
    gint ipos;
    gint ppos;

    g_assert (real);
    g_assert (data);

    if (G_UNLIKELY (real->len == real->allocated_len)) {
        g_heap_real_grow (real);
    }

    memcpy (real->data + (real->element_size * real->len), data,
            real->element_size);

    ipos = real->len;
    ppos = heap_parent (ipos);

    while ((ipos > 0) && (heap_compare (real, ppos, ipos) < 0)) {
        heap_swap (real, ppos, ipos);
        ipos = ppos;
        ppos = heap_parent (ipos);
    }

    real->len++;
}

void
g_heap_insert_vals (GHeap         *heap,
                    gconstpointer  data,
                    guint          len)
{
    GHeapReal *real = (GHeapReal *)heap;
    const gchar *ptr = data;
    guint i;

    g_return_if_fail (heap);
    g_return_if_fail (data);
    g_return_if_fail (len);

    for (i = 0; i < len; i++, ptr += real->element_size) {
        g_heap_real_insert_val (real, ptr);
    }
}

gboolean
g_heap_extract (GHeap    *heap,
                gpointer  result)
{
    GHeapReal *real = (GHeapReal *)heap;
    gboolean ret;
    gint ipos;
    gint lpos;
    gint rpos;
    gint mpos;

    g_return_val_if_fail (heap, FALSE);

    ret = (real->len > 0);

    if (ret) {
       if (result) {
          memcpy (result, heap_index (real, 0), real->element_size);
       }

       if (real->len && --real->len) {
           memmove (real->data, heap_index (real, real->len), real->element_size);

           ipos = 0;

           for (;;) {
               lpos = heap_left (ipos);
               rpos = heap_right (ipos);

               if ((lpos < real->len) && (heap_compare (real, lpos, ipos) > 0)) {
                   mpos = lpos;
               } else {
                   mpos = ipos;
               }

               if ((rpos < real->len) && (heap_compare (real, rpos, mpos) > 0)) {
                   mpos = rpos;
               }

               if (mpos == ipos) {
                   break;
               }

               heap_swap (real, mpos, ipos);

               ipos = mpos;
           }
       }
    }

    if ((real->len >= MIN_HEAP_SIZE) &&
        (real->allocated_len / 2) >= real->len) {
       g_heap_real_shrink (real);
    }

    return ret;
}

gboolean
g_heap_extract_index (GHeap    *heap,
                      guint     index_,
                      gpointer  result)
{
    GHeapReal *real = (GHeapReal *)heap;
    gboolean ret;
    gint ipos;
    gint lpos;
    gint mpos;
    gint ppos;
    gint rpos;

    g_return_val_if_fail (heap, FALSE);

    ret = (real->len > 0);

    if (real->len) {
       if (result) {
          memcpy (result, heap_index (real, 0), real->element_size);
       }

       real->len--;

       if (real->len && index_ != real->len) {
           memcpy (heap_index (real, index_),
                   heap_index (real, real->len),
                   real->element_size);

           ipos = index_;
           ppos = heap_parent (ipos);

           while (heap_compare (real, ipos, ppos) > 0) {
               heap_swap (real, ipos, ppos);
               ipos = ppos;
               ppos = heap_parent (ppos);
           }

           if (ipos == index_) {
               for (;;) {
                   lpos = heap_left (ipos);
                   rpos = heap_right (ipos);

                   if ((lpos < real->len) && (heap_compare (real, lpos, ipos) > 0)) {
                       mpos = lpos;
                   } else {
                       mpos = ipos;
                   }

                   if ((rpos < real->len) && (heap_compare (real, rpos, mpos) > 0)) {
                       mpos = rpos;
                   }

                   if (mpos == ipos) {
                       break;
                   }

                   heap_swap (real, mpos, ipos);

                   ipos = mpos;
               }
           }
       }
    }

    if ((real->len >= MIN_HEAP_SIZE) &&
        (real->allocated_len / 2) >= real->len) {
       g_heap_real_shrink (real);
    }

    return ret;
}
