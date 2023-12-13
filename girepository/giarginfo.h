/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 * GObject introspection: Argument
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

#include <girepository/gitypes.h>

G_BEGIN_DECLS

/**
 * GI_IS_ARG_INFO:
 * @info: an info structure
 *
 * Checks if @info is a [class@GIRepository.ArgInfo].
 *
 * Since: 2.80
 */
#define GI_IS_ARG_INFO(info) \
    (gi_base_info_get_info_type ((GIBaseInfo*) info) ==  GI_INFO_TYPE_ARG)


GI_AVAILABLE_IN_ALL
GIDirection            gi_arg_info_get_direction          (GIArgInfo *info);

GI_AVAILABLE_IN_ALL
gboolean               gi_arg_info_is_return_value        (GIArgInfo *info);

GI_AVAILABLE_IN_ALL
gboolean               gi_arg_info_is_optional            (GIArgInfo *info);

GI_AVAILABLE_IN_ALL
gboolean               gi_arg_info_is_caller_allocates    (GIArgInfo *info);

GI_AVAILABLE_IN_ALL
gboolean               gi_arg_info_may_be_null            (GIArgInfo *info);

GI_AVAILABLE_IN_ALL
gboolean               gi_arg_info_is_skip                (GIArgInfo *info);

GI_AVAILABLE_IN_ALL
GITransfer             gi_arg_info_get_ownership_transfer (GIArgInfo *info);

GI_AVAILABLE_IN_ALL
GIScopeType            gi_arg_info_get_scope              (GIArgInfo *info);

GI_AVAILABLE_IN_ALL
gint                   gi_arg_info_get_closure_index      (GIArgInfo *info);

GI_AVAILABLE_IN_ALL
gint                   gi_arg_info_get_destroy_index      (GIArgInfo *info);

GI_AVAILABLE_IN_ALL
GITypeInfo *           gi_arg_info_get_type_info          (GIArgInfo *info);

GI_AVAILABLE_IN_ALL
void                   gi_arg_info_load_type              (GIArgInfo *info,
                                                           GITypeInfo *type);
G_END_DECLS
