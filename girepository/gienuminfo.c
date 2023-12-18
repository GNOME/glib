/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 * GObject introspection: Enum implementation
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
#include "gitypelib-internal.h"
#include "gienuminfo.h"

/**
 * SECTION:gienuminfo
 * @title: GIEnumInfo
 * @short_description: Structs representing an enumeration and its values
 *
 * A GIEnumInfo represents an enumeration, and a GIValueInfo represents
 * a value in the enumeration.
 *
 * The GIEnumInfo contains a set of values and a type.
 *
 * The GIValueInfo is fetched by calling gi_enum_info_get_value() on
 * a GIEnumInfo.
 */

/**
 * gi_enum_info_get_n_values:
 * @info: a #GIEnumInfo
 *
 * Obtain the number of values this enumeration contains.
 *
 * Returns: the number of enumeration values
 */
guint
gi_enum_info_get_n_values (GIEnumInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  EnumBlob *blob;

  g_return_val_if_fail (info != NULL, 0);
  g_return_val_if_fail (GI_IS_ENUM_INFO (info), 0);

  blob = (EnumBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->n_values;
}

/**
 * gi_enum_info_get_error_domain:
 * @info: a #GIEnumInfo
 *
 * Obtain the string form of the quark for the error domain associated with
 * this enum, if any.
 *
 * Returns: (transfer none): the string form of the error domain associated
 * with this enum, or %NULL.
 * Since: 2.80
 */
const gchar *
gi_enum_info_get_error_domain (GIEnumInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  EnumBlob *blob;

  g_return_val_if_fail (info != NULL, 0);
  g_return_val_if_fail (GI_IS_ENUM_INFO (info), 0);

  blob = (EnumBlob *)&rinfo->typelib->data[rinfo->offset];

  if (blob->error_domain)
    return gi_typelib_get_string (rinfo->typelib, blob->error_domain);
  else
    return NULL;
}

/**
 * gi_enum_info_get_value:
 * @info: a #GIEnumInfo
 * @n: index of value to fetch
 *
 * Obtain a value for this enumeration.
 *
 * Returns: (transfer full): the enumeration value or %NULL if type tag is wrong,
 *   free the struct with gi_base_info_unref() when done.
 */
GIValueInfo *
gi_enum_info_get_value (GIEnumInfo *info,
                        guint        n)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  Header *header;
  gint offset;

  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (GI_IS_ENUM_INFO (info), NULL);

  header = (Header *)rinfo->typelib->data;
  offset = rinfo->offset + header->enum_blob_size
    + n * header->value_blob_size;

  return (GIValueInfo *) gi_info_new (GI_INFO_TYPE_VALUE, (GIBaseInfo*)info, rinfo->typelib, offset);
}

/**
 * gi_enum_info_get_n_methods:
 * @info: a #GIEnumInfo
 *
 * Obtain the number of methods that this enum type has.
 *
 * Returns: number of methods
 * Since: 2.80
 */
guint
gi_enum_info_get_n_methods (GIEnumInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  EnumBlob *blob;

  g_return_val_if_fail (info != NULL, 0);
  g_return_val_if_fail (GI_IS_ENUM_INFO (info), 0);

  blob = (EnumBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->n_methods;
}

/**
 * gi_enum_info_get_method:
 * @info: a #GIEnumInfo
 * @n: index of method to get
 *
 * Obtain an enum type method at index @n.
 *
 * Returns: (transfer full): the #GIFunctionInfo. Free the struct by calling
 *   gi_base_info_unref() when done.
 * Since: 2.80
 */
GIFunctionInfo *
gi_enum_info_get_method (GIEnumInfo *info,
                         guint       n)
{
  gint offset;
  GIRealInfo *rinfo = (GIRealInfo *)info;
  Header *header;
  EnumBlob *blob;

  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (GI_IS_ENUM_INFO (info), NULL);

  header = (Header *)rinfo->typelib->data;
  blob = (EnumBlob *)&rinfo->typelib->data[rinfo->offset];

  offset = rinfo->offset + header->enum_blob_size
    + blob->n_values * header->value_blob_size
    + n * header->function_blob_size;

  return (GIFunctionInfo *) gi_info_new (GI_INFO_TYPE_FUNCTION, (GIBaseInfo*)info,
                                         rinfo->typelib, offset);
}

/**
 * gi_enum_info_get_storage_type:
 * @info: a #GIEnumInfo
 *
 * Obtain the tag of the type used for the enum in the C ABI. This will
 * will be a signed or unsigned integral type.
 *
 * Note that in the current implementation the width of the type is
 * computed correctly, but the signed or unsigned nature of the type
 * may not match the sign of the type used by the C compiler.
 *
 * Returns: the storage type for the enumeration
 */
GITypeTag
gi_enum_info_get_storage_type (GIEnumInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  EnumBlob *blob;

  g_return_val_if_fail (info != NULL, GI_TYPE_TAG_BOOLEAN);
  g_return_val_if_fail (GI_IS_ENUM_INFO (info), GI_TYPE_TAG_BOOLEAN);

  blob = (EnumBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->storage_type;
}

void
gi_enum_info_class_init (gpointer g_class,
                         gpointer class_data)
{
  GIBaseInfoClass *info_class = g_class;

  info_class->info_type = GI_INFO_TYPE_ENUM;
}

/**
 * gi_value_info_get_value:
 * @info: a #GIValueInfo
 *
 * Obtain the enumeration value of the #GIValueInfo.
 *
 * Returns: the enumeration value. This will always be representable
 *   as a 32-bit signed or unsigned value. The use of gint64 as the
 *   return type is to allow both.
 */
gint64
gi_value_info_get_value (GIValueInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  ValueBlob *blob;

  g_return_val_if_fail (info != NULL, -1);
  g_return_val_if_fail (GI_IS_VALUE_INFO (info), -1);

  blob = (ValueBlob *)&rinfo->typelib->data[rinfo->offset];

  if (blob->unsigned_value)
    return (gint64)(guint32)blob->value;
  else
    return (gint64)blob->value;
}

void
gi_value_info_class_init (gpointer g_class,
                          gpointer class_data)
{
  GIBaseInfoClass *info_class = g_class;

  info_class->info_type = GI_INFO_TYPE_VALUE;
}
