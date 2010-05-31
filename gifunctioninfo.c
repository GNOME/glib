/* GObject introspection: Repository implementation
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

#include <glib.h>

#include <girepository.h>
#include "girepository-private.h"
#include "gitypelib-internal.h"

/* GIFunctionInfo functions */

/**
 * SECTION:gifunctioninfo
 * @Short_description: Struct representing a function
 * @Title: GIFunctionInfo
 *
 * GIFunctionInfo represents a function, method or constructor.
 * To find out what kind of entity a #GIFunctionInfo represents, call
 * g_function_info_get_flags().
 *
 * See also #GICallableInfo for information on how to retreive arguments and
 * other metadata.
 */

/**
 * g_function_info_get_symbol:
 * @info: a #GIFunctionInfo
 *
 * Obtain the symbol of the function. The symbol is the name of the
 * exported function, suitable to be used as an argument to
 * g_module_symbol().
 *
 * Returns: the symbol
 */
const gchar *
g_function_info_get_symbol (GIFunctionInfo *info)
{
  GIRealInfo *rinfo;
  FunctionBlob *blob;

  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (GI_IS_FUNCTION_INFO (info), NULL);

  rinfo = (GIRealInfo *)info;
  blob = (FunctionBlob *)&rinfo->typelib->data[rinfo->offset];

  return g_typelib_get_string (rinfo->typelib, blob->symbol);
}

/**
 * g_function_info_get_flags:
 * @info: a #GIFunctionInfo
 *
 * Obtain the #GIFunctionInfoFlags for the @info.
 *
 * Returns: the flags
 */
GIFunctionInfoFlags
g_function_info_get_flags (GIFunctionInfo *info)
{
  GIFunctionInfoFlags flags;
  GIRealInfo *rinfo;
  FunctionBlob *blob;

  g_return_val_if_fail (info != NULL, -1);
  g_return_val_if_fail (GI_IS_FUNCTION_INFO (info), -1);

  rinfo = (GIRealInfo *)info;
  blob = (FunctionBlob *)&rinfo->typelib->data[rinfo->offset];

  flags = 0;

  /* Make sure we don't flag Constructors as methods */
  if (!blob->constructor && !blob->is_static)
    flags = flags | GI_FUNCTION_IS_METHOD;

  if (blob->constructor)
    flags = flags | GI_FUNCTION_IS_CONSTRUCTOR;

  if (blob->getter)
    flags = flags | GI_FUNCTION_IS_GETTER;

  if (blob->setter)
    flags = flags | GI_FUNCTION_IS_SETTER;

  if (blob->wraps_vfunc)
    flags = flags | GI_FUNCTION_WRAPS_VFUNC;

  if (blob->throws)
    flags = flags | GI_FUNCTION_THROWS;

  return flags;
}

/**
 * g_function_info_get_property:
 * @info: a #GIFunctionInfo
 *
 * Obtain the property associated with this #GIFunctionInfo.
 * Only #GIFunctionInfo with the flag %GI_FUNCTION_IS_GETTER or
 * %GI_FUNCTION_IS_SETTER have a property set. For other cases,
 * %NULL will be returned.
 *
 * Returns: (transfer full): the property or %NULL if not set. Free it with
 * g_base_info_unref() when done.
 */
GIPropertyInfo *
g_function_info_get_property (GIFunctionInfo *info)
{
  GIRealInfo *rinfo;
  FunctionBlob *blob;
  GIInterfaceInfo *container;

  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (GI_IS_FUNCTION_INFO (info), NULL);

  rinfo = (GIRealInfo *)info;
  blob = (FunctionBlob *)&rinfo->typelib->data[rinfo->offset];
  container = (GIInterfaceInfo *)rinfo->container;

  return g_interface_info_get_property (container, blob->index);
}

/**
 * g_function_info_get_vfunc:
 * @info: a #GIFunctionInfo
 *
 * Obtain the virtual function associated with this #GIFunctionInfo.
 * Only #GIFunctionInfo with the flag %GI_FUNCTION_WRAPS_VFUNC has
 * a virtual function set. For other cases, %NULL will be returned.
 *
 * Returns: (transfer full): the virtual function or %NULL if not set.
 * Free it by calling g_base_info_unref() when done.
 */
GIVFuncInfo *
g_function_info_get_vfunc (GIFunctionInfo *info)
{
  GIRealInfo *rinfo;
  FunctionBlob *blob;
  GIInterfaceInfo *container;

  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (GI_IS_FUNCTION_INFO (info), NULL);

  rinfo = (GIRealInfo *)info;
  blob = (FunctionBlob *)&rinfo->typelib->data[rinfo->offset];
  container = (GIInterfaceInfo *)rinfo->container;

  return g_interface_info_get_vfunc (container, blob->index);
}

