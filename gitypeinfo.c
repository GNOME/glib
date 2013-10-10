/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 * GObject introspection: Type implementation
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

/**
 * SECTION:gitypeinfo
 * @title: GITypeInfo
 * @short_description: Struct representing a type
 *
 * GITypeInfo represents a type. You can retrieve a type info from
 * an argument (see #GIArgInfo), a functions return value (see #GIFunctionInfo),
 * a field (see #GIFieldInfo), a property (see #GIPropertyInfo), a constant
 * (see #GIConstantInfo) or for a union discriminator (see #GIUnionInfo).
 *
 * A type can either be a of a basic type which is a standard C primitive
 * type or an interface type. For interface types you need to call
 * g_type_info_get_interface() to get a reference to the base info for that
 * interface.
 *
 * <refsect1 id="gi-gitypeinfo.struct-hierarchy" role="struct_hierarchy">
 * <title role="struct_hierarchy.title">Struct hierarchy</title>
 * <synopsis>
 *   <link linkend="gi-GIBaseInfo">GIBaseInfo</link>
 *    +----GITypeInfo
 * </synopsis>
 * </refsect1>
 */

/**
 * g_type_info_is_pointer:
 * @info: a #GITypeInfo
 *
 * Obtain if the type is passed as a reference.
 *
 * Returns: %TRUE if it is a pointer
 */
gboolean
g_type_info_is_pointer (GITypeInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  SimpleTypeBlob *type;

  g_return_val_if_fail (info != NULL, FALSE);
  g_return_val_if_fail (GI_IS_TYPE_INFO (info), FALSE);

  type = (SimpleTypeBlob *)&rinfo->typelib->data[rinfo->offset];

  if (type->flags.reserved == 0 && type->flags.reserved2 == 0)
    return type->flags.pointer;
  else
    {
      InterfaceTypeBlob *iface = (InterfaceTypeBlob *)&rinfo->typelib->data[rinfo->offset];

      return iface->pointer;
    }
}

/**
 * g_type_info_get_tag:
 * @info: a #GITypeInfo
 *
 * Obtain the type tag for the type. See #GITypeTag for a list
 * of type tags.
 *
 * Returns: the type tag
 */
GITypeTag
g_type_info_get_tag (GITypeInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  SimpleTypeBlob *type;

  g_return_val_if_fail (info != NULL, GI_TYPE_TAG_BOOLEAN);
  g_return_val_if_fail (GI_IS_TYPE_INFO (info), GI_TYPE_TAG_BOOLEAN);

  type = (SimpleTypeBlob *)&rinfo->typelib->data[rinfo->offset];

  if (rinfo->type_is_embedded)
    return GI_TYPE_TAG_INTERFACE;
  else if (type->flags.reserved == 0 && type->flags.reserved2 == 0)
    return type->flags.tag;
  else
    {
      InterfaceTypeBlob *iface = (InterfaceTypeBlob *)&rinfo->typelib->data[rinfo->offset];

      return iface->tag;
    }
}

/**
 * g_type_info_get_param_type:
 * @info: a #GITypeInfo
 * @n: index of the parameter
 *
 * Obtain the parameter type @n.
 *
 * Returns: (transfer full): the param type info
 */
GITypeInfo *
g_type_info_get_param_type (GITypeInfo *info,
                            gint        n)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  SimpleTypeBlob *type;

  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (GI_IS_TYPE_INFO (info), NULL);

  type = (SimpleTypeBlob *)&rinfo->typelib->data[rinfo->offset];

  if (!(type->flags.reserved == 0 && type->flags.reserved2 == 0))
    {
      ParamTypeBlob *param = (ParamTypeBlob *)&rinfo->typelib->data[rinfo->offset];

      switch (param->tag)
        {
          case GI_TYPE_TAG_ARRAY:
          case GI_TYPE_TAG_GLIST:
          case GI_TYPE_TAG_GSLIST:
          case GI_TYPE_TAG_GHASH:
            return _g_type_info_new ((GIBaseInfo*)info, rinfo->typelib,
                                    rinfo->offset + sizeof (ParamTypeBlob)
                                    + sizeof (SimpleTypeBlob) * n);
            break;
          default:
            break;
        }
    }

  return NULL;
}

/**
 * g_type_info_get_interface:
 * @info: a #GITypeInfo
 *
 * For types which have #GI_TYPE_TAG_INTERFACE such as GObjects and boxed values,
 * this function returns full information about the referenced type.  You can then
 * inspect the type of the returned #GIBaseInfo to further query whether it is
 * a concrete GObject, a GInterface, a structure, etc. using g_base_info_get_type().
 *
 * Returns: (transfer full): the #GIBaseInfo, or %NULL. Free it with
 * g_base_info_unref() when done.
 */
GIBaseInfo *
g_type_info_get_interface (GITypeInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;

  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (GI_IS_TYPE_INFO (info), NULL);

  /* For embedded types, the given offset is a pointer to the actual blob,
   * after the end of the field.  In that case we know it's a "subclass" of
   * CommonBlob, so use that to determine the info type.
   */
  if (rinfo->type_is_embedded)
    {
      CommonBlob *common = (CommonBlob *)&rinfo->typelib->data[rinfo->offset];
      GIInfoType info_type;

      switch (common->blob_type)
        {
          case BLOB_TYPE_CALLBACK:
            info_type = GI_INFO_TYPE_CALLBACK;
            break;
          default:
            g_assert_not_reached ();
            return NULL;
        }
      return (GIBaseInfo *) g_info_new (info_type, (GIBaseInfo*)info, rinfo->typelib,
                                        rinfo->offset);
    }
  else
    {
      SimpleTypeBlob *type = (SimpleTypeBlob *)&rinfo->typelib->data[rinfo->offset];
      if (!(type->flags.reserved == 0 && type->flags.reserved2 == 0))
        {
          InterfaceTypeBlob *blob = (InterfaceTypeBlob *)&rinfo->typelib->data[rinfo->offset];

          if (blob->tag == GI_TYPE_TAG_INTERFACE)
            return _g_info_from_entry (rinfo->repository, rinfo->typelib, blob->interface);
        }
    }

  return NULL;
}

/**
 * g_type_info_get_array_length:
 * @info: a #GITypeInfo
 *
 * Obtain the array length of the type. The type tag must be a
 * #GI_TYPE_TAG_ARRAY or -1 will returned.
 *
 * Returns: the array length, or -1 if the type is not an array
 */
gint
g_type_info_get_array_length (GITypeInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  SimpleTypeBlob *type;

  g_return_val_if_fail (info != NULL, -1);
  g_return_val_if_fail (GI_IS_TYPE_INFO (info), -1);

  type = (SimpleTypeBlob *)&rinfo->typelib->data[rinfo->offset];

  if (!(type->flags.reserved == 0 && type->flags.reserved2 == 0))
    {
      ArrayTypeBlob *blob = (ArrayTypeBlob *)&rinfo->typelib->data[rinfo->offset];

      if (blob->tag == GI_TYPE_TAG_ARRAY)
	{
	  if (blob->has_length)
	    return blob->dimensions.length;
	}
    }

  return -1;
}

/**
 * g_type_info_get_array_fixed_size:
 * @info: a #GITypeInfo
 *
 * Obtain the fixed array size of the type. The type tag must be a
 * #GI_TYPE_TAG_ARRAY or -1 will returned.
 *
 * Returns: the size or -1 if it's not an array
 */
gint
g_type_info_get_array_fixed_size (GITypeInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  SimpleTypeBlob *type;

  g_return_val_if_fail (info != NULL, 0);
  g_return_val_if_fail (GI_IS_TYPE_INFO (info), 0);

  type = (SimpleTypeBlob *)&rinfo->typelib->data[rinfo->offset];

  if (!(type->flags.reserved == 0 && type->flags.reserved2 == 0))
    {
      ArrayTypeBlob *blob = (ArrayTypeBlob *)&rinfo->typelib->data[rinfo->offset];

      if (blob->tag == GI_TYPE_TAG_ARRAY)
	{
	  if (blob->has_size)
	    return blob->dimensions.size;
	}
    }

  return -1;
}

/**
 * g_type_info_is_zero_terminated:
 * @info: a #GITypeInfo
 *
 * Obtain if the last element of the array is %NULL. The type tag must be a
 * #GI_TYPE_TAG_ARRAY or %FALSE will returned.
 *
 * Returns: %TRUE if zero terminated
 */
gboolean
g_type_info_is_zero_terminated (GITypeInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  SimpleTypeBlob *type;

  g_return_val_if_fail (info != NULL, FALSE);
  g_return_val_if_fail (GI_IS_TYPE_INFO (info), FALSE);

  type = (SimpleTypeBlob *)&rinfo->typelib->data[rinfo->offset];

  if (!(type->flags.reserved == 0 && type->flags.reserved2 == 0))
    {
      ArrayTypeBlob *blob = (ArrayTypeBlob *)&rinfo->typelib->data[rinfo->offset];

      if (blob->tag == GI_TYPE_TAG_ARRAY)
	return blob->zero_terminated;
    }

  return FALSE;
}

/**
 * g_type_info_get_array_type:
 * @info: a #GITypeInfo
 *
 * Obtain the array type for this type. See #GIArrayType for a list of
 * possible values. If the type tag of this type is not array, -1 will be
 * returned.
 *
 * Returns: the array type or -1
 */
GIArrayType
g_type_info_get_array_type (GITypeInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  SimpleTypeBlob *type;

  g_return_val_if_fail (info != NULL, -1);
  g_return_val_if_fail (GI_IS_TYPE_INFO (info), -1);

  type = (SimpleTypeBlob *)&rinfo->typelib->data[rinfo->offset];

  if (!(type->flags.reserved == 0 && type->flags.reserved2 == 0))
    {
      ArrayTypeBlob *blob = (ArrayTypeBlob *)&rinfo->typelib->data[rinfo->offset];
      g_return_val_if_fail (blob->tag == GI_TYPE_TAG_ARRAY, -1);

      return blob->array_type;
    }

  return -1;
}
