/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 * GObject introspection: GIBaseInfo
 *
 * Copyright (C) 2005 Matthias Clasen
 * Copyright (C) 2008,2009 Red Hat, Inc.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
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

#pragma once

#if !defined (__GIREPOSITORY_H_INSIDE__) && !defined (GI_COMPILATION)
#error "Only <girepository.h> can be included directly."
#endif

#include <glib-object.h>
#include <gitypelib/gitypelib-private.h>
#include <girepository/gitypes.h>
#include <gitypelib/gitypelib.h>

G_BEGIN_DECLS

/**
 * GIAttributeIter:
 *
 * An opaque structure used to iterate over attributes
 * in a [class@GIRepository.BaseInfo] struct.
 *
 * Since: 2.80
 */
typedef struct {
  /*< private >*/
  gpointer data;
  gpointer data2;
  gpointer data3;
  gpointer data4;
} GIAttributeIter;

#define GI_TYPE_BASE_INFO	(gi_base_info_get_type ())


GI_AVAILABLE_IN_ALL
GType                  gi_base_info_get_type         (void) G_GNUC_CONST;

GI_AVAILABLE_IN_ALL
GIBaseInfo *           gi_base_info_ref              (GIBaseInfo   *info);

GI_AVAILABLE_IN_ALL
void                   gi_base_info_unref            (GIBaseInfo   *info);

GI_AVAILABLE_IN_ALL
GIInfoType             gi_base_info_get_info_type    (GIBaseInfo   *info);

GI_AVAILABLE_IN_ALL
const gchar *          gi_base_info_get_name         (GIBaseInfo   *info);

GI_AVAILABLE_IN_ALL
const gchar *          gi_base_info_get_namespace    (GIBaseInfo   *info);

GI_AVAILABLE_IN_ALL
gboolean               gi_base_info_is_deprecated    (GIBaseInfo   *info);

GI_AVAILABLE_IN_ALL
const gchar *          gi_base_info_get_attribute    (GIBaseInfo  *info,
                                                      const gchar *name);

GI_AVAILABLE_IN_ALL
gboolean               gi_base_info_iterate_attributes (GIBaseInfo       *info,
                                                        GIAttributeIter  *iterator,
                                                        const char      **name,
                                                        const char      **value);

GI_AVAILABLE_IN_ALL
GIBaseInfo *           gi_base_info_get_container    (GIBaseInfo   *info);

GI_AVAILABLE_IN_ALL
GITypelib *            gi_base_info_get_typelib      (GIBaseInfo   *info);

GI_AVAILABLE_IN_ALL
gboolean               gi_base_info_equal            (GIBaseInfo *info1,
                                                      GIBaseInfo *info2);

GI_AVAILABLE_IN_ALL
GIBaseInfo *           gi_info_new                   (GIInfoType  type,
                                                      GIBaseInfo *container,
                                                      GITypelib  *typelib,
                                                      guint32     offset);

G_END_DECLS
