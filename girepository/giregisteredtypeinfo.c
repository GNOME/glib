/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 * GObject introspection: Registered Type implementation
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

#include "config.h"

#include <string.h>

#include <glib.h>

#include <girepository/girepository.h>
#include "gibaseinfo-private.h"
#include "girepository-private.h"
#include "gitypelib-internal.h"
#include "giregisteredtypeinfo.h"

/**
 * GIRegisteredTypeInfo:
 *
 * `GIRegisteredTypeInfo` represents an entity with a [type@GObject.Type]
 * associated.
 *
 * Could be either a [class@GIRepository.EnumInfo],
 * [class@GIRepository.InterfaceInfo], [class@GIRepository.ObjectInfo],
 * [class@GIRepository.StructInfo] or a [class@GIRepository.UnionInfo].
 *
 * A registered type info struct has a name and a type function.
 *
 * To get the name call [method@GIRepository.RegisteredTypeInfo.get_type_name].
 * Most users want to call [method@GIRepository.RegisteredTypeInfo.get_g_type]
 * and don’t worry about the rest of the details.
 *
 * Since: 2.80
 */

/**
 * gi_registered_type_info_get_type_name:
 * @info: a #GIRegisteredTypeInfo
 *
 * Obtain the type name of the struct within the GObject type system.
 *
 * This type can be passed to [func@GObject.type_name] to get a
 * [type@GObject.Type].
 *
 * Returns: (nullable): the type name, or `NULL` if unknown
 * Since: 2.80
 */
const gchar *
gi_registered_type_info_get_type_name (GIRegisteredTypeInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  RegisteredTypeBlob *blob;

  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (GI_IS_REGISTERED_TYPE_INFO (info), NULL);

  blob = (RegisteredTypeBlob *)&rinfo->typelib->data[rinfo->offset];

  if (blob->gtype_name)
    return gi_typelib_get_string (rinfo->typelib, blob->gtype_name);

  return NULL;
}

/**
 * gi_registered_type_info_get_type_init_function_name:
 * @info: a #GIRegisteredTypeInfo
 *
 * Obtain the type init function for @info.
 *
 * The type init function is the function which will register the
 * [type@GObject.Type] within the GObject type system. Usually this is not
 * called by language bindings or applications — use
 * [method@GIRepository.RegisteredTypeInfo.get_g_type] directly instead.
 *
 * Returns: (nullable): the symbol name of the type init function, suitable for
 *   passing into [method@GModule.Module.symbol], or `NULL` if unknown
 * Since: 2.80
 */
const gchar *
gi_registered_type_info_get_type_init_function_name (GIRegisteredTypeInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  RegisteredTypeBlob *blob;

  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (GI_IS_REGISTERED_TYPE_INFO (info), NULL);

  blob = (RegisteredTypeBlob *)&rinfo->typelib->data[rinfo->offset];

  if (blob->gtype_init)
    return gi_typelib_get_string (rinfo->typelib, blob->gtype_init);

  return NULL;
}

/**
 * gi_registered_type_info_get_g_type:
 * @info: a #GIRegisteredTypeInfo
 *
 * Obtain the [type@GObject.Type] for this registered type.
 *
 * If there is no type information associated with @info, or the shared library
 * which provides the `type_init` function for @info cannot be called, then
 * `G_TYPE_NONE` is returned.
 *
 * Returns: the [type@GObject.Type], or `G_TYPE_NONE` if unknown
 * Since: 2.80
 */
GType
gi_registered_type_info_get_g_type (GIRegisteredTypeInfo *info)
{
  const char *type_init;
  GType (* get_type_func) (void);
  GIRealInfo *rinfo = (GIRealInfo*)info;

  g_return_val_if_fail (info != NULL, G_TYPE_INVALID);
  g_return_val_if_fail (GI_IS_REGISTERED_TYPE_INFO (info), G_TYPE_INVALID);

  type_init = gi_registered_type_info_get_type_init_function_name (info);

  if (type_init == NULL)
    return G_TYPE_NONE;
  else if (!strcmp (type_init, "intern"))
    /* The special string "intern" is used for some types exposed by libgobject
       (that therefore should be always available) */
    return g_type_from_name (gi_registered_type_info_get_type_name (info));

  get_type_func = NULL;
  if (!gi_typelib_symbol (rinfo->typelib,
                          type_init,
                          (void**) &get_type_func))
    return G_TYPE_NONE;

  return (* get_type_func) ();
}

void
gi_registered_type_info_class_init (gpointer g_class,
                                    gpointer class_data)
{
  GIBaseInfoClass *info_class = g_class;

  info_class->info_type = GI_INFO_TYPE_REGISTERED_TYPE;
}
