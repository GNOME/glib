/* garraylist.h
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __G_ARRAY_LIST_H__
#define __G_ARRAY_LIST_H__

#if !defined (__GLIB_H_INSIDE__) && !defined (GLIB_COMPILATION)
#error "Only <glib.h> can be included directly."
#endif

#include <glib/gtypes.h>
#include <glib/glist.h>

G_BEGIN_DECLS

typedef struct
{
  guint len;

  guint padding;
  gpointer padding1;
  gpointer padding2;
  gpointer padding3;
  gpointer padding4;
  gpointer padding5;
  gpointer padding6;
  gpointer padding7;
} GArrayList;

GLIB_AVAILABLE_IN_2_46
GArrayList  *g_array_list_new          (GDestroyNotify  notify);

GLIB_AVAILABLE_IN_2_46
void         g_array_list_init         (GArrayList     *list,
                                        GDestroyNotify  notify);

GLIB_AVAILABLE_IN_2_46
const GList *g_array_list_peek         (GArrayList     *list);

GLIB_AVAILABLE_IN_2_46
gpointer     g_array_list_index        (GArrayList     *list,
                                        guint           index);

GLIB_AVAILABLE_IN_2_46
void         g_array_list_add          (GArrayList     *list,
                                        gpointer        data);

GLIB_AVAILABLE_IN_2_46
void         g_array_list_remove       (GArrayList     *list,
                                        gpointer        data);

GLIB_AVAILABLE_IN_2_46
void         g_array_list_remove_index (GArrayList     *list,
                                        guint           index);

GLIB_AVAILABLE_IN_2_46
void         g_array_list_destroy      (GArrayList     *list);

GLIB_AVAILABLE_IN_2_46
const GList *g_array_list_last_link    (GArrayList     *list);

#define g_array_list_first(list) (((list)->len == 0) ? NULL : g_array_list_index((list),0))
#define g_array_list_last(list) (((list)->len == 0) ? NULL : g_array_list_index((list),(list)->len-1))

G_END_DECLS

#endif /* __G_ARRAY_LIST_H__ */
