/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 * GObject introspection: Function
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
 * GI_IS_FUNCTION_INFO
 * @info: an info structure
 *
 * Checks if @info is a #GIFunctionInfo.
 */
#define GI_IS_FUNCTION_INFO(info) \
    (gi_base_info_get_type((GIBaseInfo*)info) ==  GI_INFO_TYPE_FUNCTION)


GI_AVAILABLE_IN_ALL
const gchar *           gi_function_info_get_symbol     (GIFunctionInfo *info);

GI_AVAILABLE_IN_ALL
GIFunctionInfoFlags     gi_function_info_get_flags      (GIFunctionInfo *info);

GI_AVAILABLE_IN_ALL
GIPropertyInfo *        gi_function_info_get_property   (GIFunctionInfo *info);

GI_AVAILABLE_IN_ALL
GIVFuncInfo *           gi_function_info_get_vfunc      (GIFunctionInfo *info);

/**
 * GI_INVOKE_ERROR:
 *
 * TODO
 */
#define GI_INVOKE_ERROR (gi_invoke_error_quark ())

GI_AVAILABLE_IN_ALL
GQuark gi_invoke_error_quark (void);

/**
 * GIInvokeError:
 * @GI_INVOKE_ERROR_FAILED: invokation failed, unknown error.
 * @GI_INVOKE_ERROR_SYMBOL_NOT_FOUND: symbol couldn't be found in any of the
 *   libraries associated with the typelib of the function.
 * @GI_INVOKE_ERROR_ARGUMENT_MISMATCH: the arguments provided didn't match
 *   the expected arguments for the functions type signature.
 *
 * An error occuring while invoking a function via
 * gi_function_info_invoke().
 */

typedef enum
{
  GI_INVOKE_ERROR_FAILED,
  GI_INVOKE_ERROR_SYMBOL_NOT_FOUND,
  GI_INVOKE_ERROR_ARGUMENT_MISMATCH
} GIInvokeError;


GI_AVAILABLE_IN_ALL
gboolean              gi_function_info_invoke         (GIFunctionInfo    *info,
                                                       const GIArgument  *in_args,
                                                       int                n_in_args,
                                                       const GIArgument  *out_args,
                                                       int                n_out_args,
                                                       GIArgument        *return_value,
                                                       GError           **error);


G_END_DECLS
