/*
 * Copyright 2026 Christian Hergert
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
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __G_LIST_MODEL_VIEW_H__
#define __G_LIST_MODEL_VIEW_H__

#if !defined (__GIO_GIO_H_INSIDE__) && !defined (GIO_COMPILATION)
#error "Only <gio/gio.h> can be included directly."
#endif

#include <gio/glistmodel.h>

G_BEGIN_DECLS

#define G_TYPE_LIST_MODEL_VIEW (g_list_model_view_get_type ())

GIO_AVAILABLE_IN_2_88
G_DECLARE_FINAL_TYPE (GListModelView, g_list_model_view, G, LIST_MODEL_VIEW, GObject)

GIO_AVAILABLE_IN_2_88
GListModelView *g_list_model_view_new       (GListModel     *model);
GIO_AVAILABLE_IN_2_88
GListModel     *g_list_model_view_get_model (GListModelView *self);
GIO_AVAILABLE_IN_2_88
void            g_list_model_view_set_model (GListModelView *self,
                                             GListModel     *model);

G_END_DECLS

#endif /* __G_LIST_MODEL_VIEW_H__ */
