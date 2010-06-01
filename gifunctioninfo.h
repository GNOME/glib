/* GObject introspection: Functions
 *
 * Copyright (C) 2005 Matthias Clasen
 * Copyright (C) 2008,2009 Red Hat, Inc.
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

#ifndef __GIFUNCTIONINFO_H__
#define __GIFUNCTIONINFO_H__

#if !defined (__GIREPOSITORY_H_INSIDE__) && !defined (GI_COMPILATION)
#error "Only <girepository.h> can be included directly."
#endif

#include <gitypes.h>

G_BEGIN_DECLS

#define GI_IS_FUNCTION_INFO(info) \
    (g_base_info_get_type((GIBaseInfo*)info) ==  GI_INFO_TYPE_FUNCTION)

/**
 * GIFunctionInfoFlags:
 * @GI_FUNCTION_IS_METHOD: is a method.
 * @GI_FUNCTION_IS_CONSTRUCTOR: is a constructor.
 * @GI_FUNCTION_IS_GETTER: is a getter of a #GIPropertyInfo.
 * @GI_FUNCTION_IS_SETTER: is a setter of a #GIPropertyInfo.
 * @GI_FUNCTION_WRAPS_VFUNC: represents a virtual function.
 * @GI_FUNCTION_THROWS: the function may throw an error.
 *
 * Flags for a #GIFunctionInfo struct.
 */
typedef enum
{
  GI_FUNCTION_IS_METHOD      = 1 << 0,
  GI_FUNCTION_IS_CONSTRUCTOR = 1 << 1,
  GI_FUNCTION_IS_GETTER      = 1 << 2,
  GI_FUNCTION_IS_SETTER      = 1 << 3,
  GI_FUNCTION_WRAPS_VFUNC    = 1 << 4,
  GI_FUNCTION_THROWS         = 1 << 5
} GIFunctionInfoFlags;

const gchar *           g_function_info_get_symbol     (GIFunctionInfo *info);
GIFunctionInfoFlags     g_function_info_get_flags      (GIFunctionInfo *info);
GIPropertyInfo *        g_function_info_get_property   (GIFunctionInfo *info);
GIVFuncInfo *           g_function_info_get_vfunc      (GIFunctionInfo *info);

#define G_INVOKE_ERROR (g_invoke_error_quark ())
GQuark g_invoke_error_quark (void);

/**
 * GInvokeError:
 * @G_INVOKE_ERROR_FAILED: invokation failed, unknown error.
 * @G_INVOKE_ERROR_SYMBOL_NOT_FOUND: symbol couldn't be found in any of the
 * libraries associated with the typelib of the function.
 * @G_INVOKE_ERROR_ARGUMENT_MISMATCH: the arguments provided didn't match
 * the expected arguments for the functions type signature.
 *
 * An error occuring while invoking a function via
 * g_function_info_invoke().
 */

typedef enum
{
  G_INVOKE_ERROR_FAILED,
  G_INVOKE_ERROR_SYMBOL_NOT_FOUND,
  G_INVOKE_ERROR_ARGUMENT_MISMATCH
} GInvokeError;

gboolean              g_function_info_invoke         (GIFunctionInfo *info,
						      const GArgument  *in_args,
						      int               n_in_args,
						      const GArgument  *out_args,
						      int               n_out_args,
						      GArgument        *return_value,
						      GError          **error);


G_END_DECLS


#endif  /* __G_IREPOSITORY_H__ */

