/*
 * Copyright © 2014 Lars Uebernickel
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
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Lars Uebernickel <lars@uebernic.de>
 */

#ifndef __G_LIST_STORE_H__
#define __G_LIST_STORE_H__

#if !defined (__GIO_GIO_H_INSIDE__) && !defined (GIO_COMPILATION)
#error "Only <gio/gio.h> can be included directly."
#endif

#include <gio/giotypes.h>

G_BEGIN_DECLS

#define G_TYPE_LIST_STORE            (g_list_store_get_type ())
#define G_LIST_STORE(o)              (G_TYPE_CHECK_INSTANCE_CAST ((o), G_TYPE_LIST_STORE, GListStore))
#define G_IS_LIST_STORE(o)           (G_TYPE_CHECK_INSTANCE_TYPE ((o), G_TYPE_LIST_STORE))

GLIB_AVAILABLE_IN_2_42
GType                   g_list_store_get_type                           (void) G_GNUC_CONST;

GLIB_AVAILABLE_IN_2_42
GListStore *            g_list_store_new                                (GType item_type);

GLIB_AVAILABLE_IN_2_42
void                    g_list_store_insert                             (GListStore *store,
                                                                         guint       position,
                                                                         gpointer    item);

GLIB_AVAILABLE_IN_2_42
void                    g_list_store_append                             (GListStore *store,
                                                                         gpointer    item);

GLIB_AVAILABLE_IN_2_42
void                    g_list_store_remove                             (GListStore *store,
                                                                         guint       position);

GLIB_AVAILABLE_IN_2_42
void                    g_list_store_remove_all                         (GListStore *store);

GLIB_AVAILABLE_IN_2_42
void                    g_list_store_splice                             (GListStore *store,
                                                                         guint       position,
                                                                         guint       removed,
                                                                         guint       added,
                                                                         gpointer   *items);

G_END_DECLS

#endif
