/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 * GObject introspection: Virtual Functions
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
 * GI_IS_VFUNC_INFO
 * @info: an info structure
 *
 * Checks if @info is a #GIVfuncInfo.
 */
#define GI_IS_VFUNC_INFO(info) \
    (gi_base_info_get_type((GIBaseInfo*)info) ==  GI_INFO_TYPE_VFUNC)

GI_AVAILABLE_IN_ALL
GIVFuncInfoFlags  gi_vfunc_info_get_flags   (GIVFuncInfo *info);

GI_AVAILABLE_IN_ALL
gint              gi_vfunc_info_get_offset  (GIVFuncInfo *info);

GI_AVAILABLE_IN_ALL
GISignalInfo *    gi_vfunc_info_get_signal  (GIVFuncInfo *info);

GI_AVAILABLE_IN_ALL
GIFunctionInfo *  gi_vfunc_info_get_invoker (GIVFuncInfo *info);

GI_AVAILABLE_IN_ALL
gpointer          gi_vfunc_info_get_address (GIVFuncInfo *info,
                                             GType        implementor_gtype,
                                             GError     **error);

GI_AVAILABLE_IN_ALL
gboolean          gi_vfunc_info_invoke      (GIVFuncInfo      *info,
                                             GType             implementor,
                                             const GIArgument *in_args,
                                             int               n_in_args,
                                             const GIArgument *out_args,
                                             int               n_out_args,
                                             GIArgument       *return_value,
                                             GError          **error);

G_END_DECLS
