/* gheap.h
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

#ifndef G_HEAP_H
#define G_HEAP_H

#include <glib.h>

G_BEGIN_DECLS

#define g_heap_insert_val(h,v) g_heap_insert_vals(h,&(v),1)
#define g_heap_index(h,t,i)    (((t*)(h)->data)[i])
#define g_heap_peek(h,t)       g_heap_index(h,t,0)

typedef struct _GHeap GHeap;

struct _GHeap
{
   gchar *data;
   guint  len;
};

GLIB_AVAILABLE_IN_2_42
GHeap   *g_heap_new           (guint           element_size,
                               GCompareFunc    compare_func);
GLIB_AVAILABLE_IN_2_42
GHeap   *g_heap_ref           (GHeap          *heap);
GLIB_AVAILABLE_IN_2_42
void     g_heap_unref         (GHeap          *heap);
GLIB_AVAILABLE_IN_2_42
void     g_heap_insert_vals   (GHeap          *heap,
                               gconstpointer   data,
                               guint           len);
GLIB_AVAILABLE_IN_2_42
gboolean g_heap_extract       (GHeap          *heap,
                               gpointer        result);
GLIB_AVAILABLE_IN_2_42
gboolean g_heap_extract_index (GHeap          *heap,
                               guint           index_,
                               gpointer        result);

G_END_DECLS

#endif /* G_HEAP_H */
