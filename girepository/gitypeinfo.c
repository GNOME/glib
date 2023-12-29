/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 * GObject introspection: Type implementation
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

#include <glib.h>

#include <girepository/girepository.h>
#include "gibaseinfo-private.h"
#include "girepository-private.h"
#include <gitypelib/gitypelib-private.h>
#include "gitypeinfo.h"

/**
 * GITypeInfo:
 *
 * `GITypeInfo` represents a type, including information about direction and
 * transfer.
 *
 * You can retrieve a type info from an argument (see
 * [class@GIRepository.ArgInfo]), a function’s return value (see
 * [class@GIRepository.FunctionInfo]), a field (see
 * [class@GIRepository.FieldInfo]), a property (see
 * [class@GIRepository.PropertyInfo]), a constant (see
 * [class@GIRepository.ConstantInfo]) or for a union discriminator (see
 * [class@GIRepository.UnionInfo]).
 *
 * A type can either be a of a basic type which is a standard C primitive
 * type or an interface type. For interface types you need to call
 * [method@GIRepository.TypeInfo.get_interface] to get a reference to the base
 * info for that interface.
 *
 * Since: 2.80
 */

/**
 * gi_type_info_is_pointer:
 * @info: a #GITypeInfo
 *
 * Obtain if the type is passed as a reference.
 *
 * Note that the types of `GI_DIRECTION_OUT` and `GI_DIRECTION_INOUT` parameters
 * will only be pointers if the underlying type being transferred is a pointer
 * (i.e. only if the type of the C function’s formal parameter is a pointer to a
 * pointer).
 *
 * Returns: `TRUE` if it is a pointer
 * Since: 2.80
 */
gboolean
gi_type_info_is_pointer (GITypeInfo *info)
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
 * gi_type_info_get_tag:
 * @info: a #GITypeInfo
 *
 * Obtain the type tag for the type.
 *
 * See [type@GIRepository.TypeTag] for a list of type tags.
 *
 * Returns: the type tag
 * Since: 2.80
 */
GITypeTag
gi_type_info_get_tag (GITypeInfo *info)
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
 * gi_type_info_get_param_type:
 * @info: a #GITypeInfo
 * @n: index of the parameter
 *
 * Obtain the parameter type @n, or `NULL` if the type is not an array.
 *
 * Returns: (transfer full) (nullable): the param type info, or `NULL` if the
 *   type is not an array
 * Since: 2.80
 */
GITypeInfo *
gi_type_info_get_param_type (GITypeInfo *info,
                             guint       n)
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
            return gi_type_info_new ((GIBaseInfo*)info, rinfo->typelib,
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
 * gi_type_info_get_interface:
 * @info: a #GITypeInfo
 *
 * For types which have `GI_TYPE_TAG_INTERFACE` such as [class@GObject.Object]s
 * and boxed values, this function returns full information about the referenced
 * type.
 *
 * You can then inspect the type of the returned [class@GIRepository.BaseInfo]
 * to further query whether it is a concrete [class@GObject.Object], an
 * interface, a structure, etc., using
 * [method@GIRepository.BaseInfo.get_info_type].
 *
 * Returns: (transfer full) (nullable): The [class@GIRepository.BaseInfo], or
 *   `NULL`. Free it with gi_base_info_unref() when done.
 * Since: 2.80
 */
GIBaseInfo *
gi_type_info_get_interface (GITypeInfo *info)
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
      return (GIBaseInfo *) gi_info_new (info_type, (GIBaseInfo*)info, rinfo->typelib,
                                         rinfo->offset);
    }
  else
    {
      SimpleTypeBlob *type = (SimpleTypeBlob *)&rinfo->typelib->data[rinfo->offset];
      if (!(type->flags.reserved == 0 && type->flags.reserved2 == 0))
        {
          InterfaceTypeBlob *blob = (InterfaceTypeBlob *)&rinfo->typelib->data[rinfo->offset];

          if (blob->tag == GI_TYPE_TAG_INTERFACE)
            return gi_info_from_entry (rinfo->repository, rinfo->typelib, blob->interface);
        }
    }

  return NULL;
}

/**
 * gi_type_info_get_array_length_index:
 * @info: a #GITypeInfo
 *
 * Obtain the position of the argument which gives the array length of the type.
 *
 * The type tag must be a `GI_TYPE_TAG_ARRAY` or `-1` will be returned.
 *
 * Returns: the array length argument index, or `-1` if the type is not an array
 *   or it has no length argument
 * Since: 2.80
 */
gint
gi_type_info_get_array_length_index (GITypeInfo *info)
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
 * gi_type_info_get_array_fixed_size:
 * @info: a #GITypeInfo
 *
 * Obtain the fixed array size of the type, in number of elements (not bytes).
 *
 * The type tag must be a `GI_TYPE_TAG_ARRAY` or `-1` will be returned.
 *
 * Returns: the size or `-1` if the type is not an array or it has no fixed size
 * Since: 2.80
 */
gssize
gi_type_info_get_array_fixed_size (GITypeInfo *info)
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
	  if (blob->has_size)
	    return blob->dimensions.size;
	}
    }

  return -1;
}

/**
 * gi_type_info_is_zero_terminated:
 * @info: a #GITypeInfo
 *
 * Obtain if the last element of the array is `NULL`.
 *
 * The type tag must be a `GI_TYPE_TAG_ARRAY` or `FALSE` will be returned.
 *
 * Returns: `TRUE` if zero terminated
 * Since: 2.80
 */
gboolean
gi_type_info_is_zero_terminated (GITypeInfo *info)
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
 * gi_type_info_get_array_type:
 * @info: a #GITypeInfo
 *
 * Obtain the array type for this type.
 *
 * See [enum@GIRepository.ArrayType] for a list of possible values. If the type
 * tag of this type is not array, `-1` will be returned.
 *
 * Returns: the array type or `-1`
 * Since: 2.80
 */
GIArrayType
gi_type_info_get_array_type (GITypeInfo *info)
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

/**
 * gi_type_info_get_storage_type:
 * @info: a #GITypeInfo
 *
 * Obtain the type tag corresponding to the underlying storage type in C for
 * the type.
 *
 * See [type@GIRepository.TypeTag] for a list of type tags.
 *
 * Returns: the type tag
 * Since: 2.80
 */
GITypeTag
gi_type_info_get_storage_type (GITypeInfo *info)
{
  GITypeTag type_tag = gi_type_info_get_tag (info);

  if (type_tag == GI_TYPE_TAG_INTERFACE)
    {
      GIBaseInfo *interface = gi_type_info_get_interface (info);
      GIInfoType info_type = gi_base_info_get_info_type (interface);
      if (info_type == GI_INFO_TYPE_ENUM || info_type == GI_INFO_TYPE_FLAGS)
        type_tag = gi_enum_info_get_storage_type ((GIEnumInfo *) interface);
      gi_base_info_unref (interface);
    }

  return type_tag;
}

/**
 * gi_type_tag_argument_from_hash_pointer:
 * @storage_type: a [type@GIRepository.TypeTag] obtained from
 *   [method@GIRepository.TypeInfo.get_storage_type]
 * @hash_pointer: a pointer, such as a [struct@GLib.HashTable] data pointer
 * @arg: (out caller-allocates) (not nullable): a [type@GIRepository.Argument]
 *   to fill in
 *
 * Convert a data pointer from a GLib data structure to a
 * [type@GIRepository.Argument].
 *
 * GLib data structures, such as [type@GLib.List], [type@GLib.SList], and
 * [type@GLib.HashTable], all store data pointers.
 *
 * In the case where the list or hash table is storing single types rather than
 * structs, these data pointers may have values stuffed into them via macros
 * such as `GPOINTER_TO_INT`.
 *
 * Use this function to ensure that all values are correctly extracted from
 * stuffed pointers, regardless of the machine’s architecture or endianness.
 *
 * This function fills in the appropriate field of @arg with the value extracted
 * from @hash_pointer, depending on @storage_type.
 *
 * Since: 2.80
 */
void
gi_type_tag_argument_from_hash_pointer (GITypeTag   storage_type,
                                        gpointer    hash_pointer,
                                        GIArgument *arg)
{
  switch (storage_type)
    {
      case GI_TYPE_TAG_BOOLEAN:
        arg->v_boolean = !!GPOINTER_TO_INT (hash_pointer);
        break;
      case GI_TYPE_TAG_INT8:
        arg->v_int8 = (gint8)GPOINTER_TO_INT (hash_pointer);
        break;
      case GI_TYPE_TAG_UINT8:
        arg->v_uint8 = (guint8)GPOINTER_TO_UINT (hash_pointer);
        break;
      case GI_TYPE_TAG_INT16:
        arg->v_int16 = (gint16)GPOINTER_TO_INT (hash_pointer);
        break;
      case GI_TYPE_TAG_UINT16:
        arg->v_uint16 = (guint16)GPOINTER_TO_UINT (hash_pointer);
        break;
      case GI_TYPE_TAG_INT32:
        arg->v_int32 = (gint32)GPOINTER_TO_INT (hash_pointer);
        break;
      case GI_TYPE_TAG_UINT32:
      case GI_TYPE_TAG_UNICHAR:
        arg->v_uint32 = (guint32)GPOINTER_TO_UINT (hash_pointer);
        break;
      case GI_TYPE_TAG_GTYPE:
        arg->v_size = GPOINTER_TO_SIZE (hash_pointer);
        break;
      case GI_TYPE_TAG_UTF8:
      case GI_TYPE_TAG_FILENAME:
      case GI_TYPE_TAG_INTERFACE:
      case GI_TYPE_TAG_ARRAY:
      case GI_TYPE_TAG_GLIST:
      case GI_TYPE_TAG_GSLIST:
      case GI_TYPE_TAG_GHASH:
      case GI_TYPE_TAG_ERROR:
        arg->v_pointer = hash_pointer;
        break;
      case GI_TYPE_TAG_INT64:
      case GI_TYPE_TAG_UINT64:
      case GI_TYPE_TAG_FLOAT:
      case GI_TYPE_TAG_DOUBLE:
      default:
        g_critical ("Unsupported storage type for pointer-stuffing: %s",
                    gi_type_tag_to_string (storage_type));
        arg->v_pointer = hash_pointer;
    }
}

/**
 * gi_type_info_argument_from_hash_pointer:
 * @info: a #GITypeInfo
 * @hash_pointer: a pointer, such as a [struct@GLib.HashTable] data pointer
 * @arg: (out caller-allocates): a [type@GIRepository.Argument] to fill in
 *
 * Convert a data pointer from a GLib data structure to a
 * [type@GIRepository.Argument].
 *
 * GLib data structures, such as [type@GLib.List], [type@GLib.SList], and
 * [type@GLib.HashTable], all store data pointers.
 *
 * In the case where the list or hash table is storing single types rather than
 * structs, these data pointers may have values stuffed into them via macros
 * such as `GPOINTER_TO_INT`.
 *
 * Use this function to ensure that all values are correctly extracted from
 * stuffed pointers, regardless of the machine’s architecture or endianness.
 *
 * This function fills in the appropriate field of @arg with the value extracted
 * from @hash_pointer, depending on the storage type of @info.
 *
 * Since: 2.80
 */
void
gi_type_info_argument_from_hash_pointer (GITypeInfo *info,
                                         gpointer    hash_pointer,
                                         GIArgument *arg)
{
    GITypeTag storage_type = gi_type_info_get_storage_type (info);
    gi_type_tag_argument_from_hash_pointer (storage_type, hash_pointer,
                                            arg);
}

/**
 * gi_type_tag_hash_pointer_from_argument:
 * @storage_type: a [type@GIRepository.TypeTag] obtained from
 *   [method@GIRepository.TypeInfo.get_storage_type]
 * @arg: a [type@GIRepository.Argument] with the value to stuff into a pointer
 *
 * Convert a [type@GIRepository.Argument] to data pointer for use in a GLib
 * data structure.
 *
 * GLib data structures, such as [type@GLib.List], [type@GLib.SList], and
 * [type@GLib.HashTable], all store data pointers.
 *
 * In the case where the list or hash table is storing single types rather than
 * structs, these data pointers may have values stuffed into them via macros
 * such as `GPOINTER_TO_INT`.
 *
 * Use this function to ensure that all values are correctly stuffed into
 * pointers, regardless of the machine’s architecture or endianness.
 *
 * This function returns a pointer stuffed with the appropriate field of @arg,
 * depending on @storage_type.
 *
 * Returns: A stuffed pointer, that can be stored in a [struct@GLib.HashTable],
 *   for example
 * Since: 2.80
 */
gpointer
gi_type_tag_hash_pointer_from_argument (GITypeTag   storage_type,
                                        GIArgument *arg)
{
  switch (storage_type)
    {
      case GI_TYPE_TAG_BOOLEAN:
        return GINT_TO_POINTER (arg->v_boolean);
      case GI_TYPE_TAG_INT8:
        return GINT_TO_POINTER (arg->v_int8);
      case GI_TYPE_TAG_UINT8:
        return GUINT_TO_POINTER (arg->v_uint8);
      case GI_TYPE_TAG_INT16:
        return GINT_TO_POINTER (arg->v_int16);
      case GI_TYPE_TAG_UINT16:
        return GUINT_TO_POINTER (arg->v_uint16);
      case GI_TYPE_TAG_INT32:
        return GINT_TO_POINTER (arg->v_int32);
      case GI_TYPE_TAG_UINT32:
      case GI_TYPE_TAG_UNICHAR:
        return GUINT_TO_POINTER (arg->v_uint32);
      case GI_TYPE_TAG_GTYPE:
        return GSIZE_TO_POINTER (arg->v_size);
      case GI_TYPE_TAG_UTF8:
      case GI_TYPE_TAG_FILENAME:
      case GI_TYPE_TAG_INTERFACE:
      case GI_TYPE_TAG_ARRAY:
      case GI_TYPE_TAG_GLIST:
      case GI_TYPE_TAG_GSLIST:
      case GI_TYPE_TAG_GHASH:
      case GI_TYPE_TAG_ERROR:
        return arg->v_pointer;
      case GI_TYPE_TAG_INT64:
      case GI_TYPE_TAG_UINT64:
      case GI_TYPE_TAG_FLOAT:
      case GI_TYPE_TAG_DOUBLE:
      default:
        g_critical ("Unsupported storage type for pointer-stuffing: %s",
                    gi_type_tag_to_string (storage_type));
        return arg->v_pointer;
    }
}

/**
 * gi_type_info_hash_pointer_from_argument:
 * @info: a #GITypeInfo
 * @arg: a [struct@GIRepository.Argument] with the value to stuff into a pointer
 *
 * Convert a [type@GIRepository.Argument] to data pointer for use in a GLib
 * data structure.
 *
 * GLib data structures, such as [type@GLib.List], [type@GLib.SList], and
 * [type@GLib.HashTable], all store data pointers.
 *
 * In the case where the list or hash table is storing single types rather than
 * structs, these data pointers may have values stuffed into them via macros
 * such as `GPOINTER_TO_INT`.
 *
 * Use this function to ensure that all values are correctly stuffed into
 * pointers, regardless of the machine’s architecture or endianness.
 *
 * This function returns a pointer stuffed with the appropriate field of @arg,
 * depending on the storage type of @info.
 *
 * Returns: A stuffed pointer, that can be stored in a [struct@GLib.HashTable],
 *   for example
 * Since: 2.80
 */
gpointer
gi_type_info_hash_pointer_from_argument (GITypeInfo *info,
                                         GIArgument *arg)
{
  GITypeTag storage_type = gi_type_info_get_storage_type (info);
  return gi_type_tag_hash_pointer_from_argument (storage_type, arg);
}

void
gi_type_info_class_init (gpointer g_class,
                         gpointer class_data)
{
  GIBaseInfoClass *info_class = g_class;

  info_class->info_type = GI_INFO_TYPE_TYPE;
}
