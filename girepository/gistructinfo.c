/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 * GObject introspection: Struct implementation
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
#include "girepository-private.h"
#include "gitypelib-internal.h"
#include "gistructinfo.h"

/**
 * SECTION:gistructinfo
 * @title: GIStructInfo
 * @short_description: Struct representing a C structure
 *
 * GIStructInfo represents a generic C structure type.
 *
 * A structure has methods and fields.
 */

/**
 * gi_struct_info_get_n_fields:
 * @info: a #GIStructInfo
 *
 * Obtain the number of fields this structure has.
 *
 * Returns: number of fields
 */
gint
gi_struct_info_get_n_fields (GIStructInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  StructBlob *blob = (StructBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->n_fields;
}

/**
 * gi_struct_info_get_field_offset:
 * @info: a #GIStructInfo
 * @n: index of queried field
 *
 * Obtain the offset of the specified field.
 *
 * Returns: field offset in bytes
 */
static gint32
gi_struct_get_field_offset (GIStructInfo *info,
                            gint          n)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  Header *header = (Header *)rinfo->typelib->data;
  guint32 offset = rinfo->offset + header->struct_blob_size;
  gint i;
  FieldBlob *field_blob;

  for (i = 0; i < n; i++)
    {
      field_blob = (FieldBlob *)&rinfo->typelib->data[offset];
      offset += header->field_blob_size;
      if (field_blob->has_embedded_type)
        offset += header->callback_blob_size;
    }

  return offset;
}

/**
 * gi_struct_info_get_field:
 * @info: a #GIStructInfo
 * @n: a field index
 *
 * Obtain the type information for field with specified index.
 *
 * Returns: (transfer full): the #GIFieldInfo, free it with gi_base_info_unref()
 * when done.
 */
GIFieldInfo *
gi_struct_info_get_field (GIStructInfo *info,
                          gint          n)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;

  return (GIFieldInfo *) gi_info_new (GI_INFO_TYPE_FIELD, (GIBaseInfo*)info, rinfo->typelib,
                                      gi_struct_get_field_offset (info, n));
}

/**
 * gi_struct_info_find_field:
 * @info: a #GIStructInfo
 * @name: a field name
 *
 * Obtain the type information for field named @name.
 *
 * Since: 1.46
 * Returns: (transfer full): the #GIFieldInfo or %NULL if not found,
 * free it with gi_base_info_unref() when done.
 */
GIFieldInfo *
gi_struct_info_find_field (GIStructInfo *info,
                           const gchar  *name)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  StructBlob *blob = (StructBlob *)&rinfo->typelib->data[rinfo->offset];
  Header *header = (Header *)rinfo->typelib->data;
  guint32 offset = rinfo->offset + header->struct_blob_size;
  gint i;

  for (i = 0; i < blob->n_fields; i++)
    {
      FieldBlob *field_blob = (FieldBlob *)&rinfo->typelib->data[offset];
      const gchar *fname = (const gchar *)&rinfo->typelib->data[field_blob->name];

      if (strcmp (name, fname) == 0)
        {
          return (GIFieldInfo *) gi_info_new (GI_INFO_TYPE_FIELD,
                                              (GIBaseInfo* )info,
                                              rinfo->typelib,
                                              offset);
        }

      offset += header->field_blob_size;
      if (field_blob->has_embedded_type)
        offset += header->callback_blob_size;
    }

  return NULL;
}

/**
 * gi_struct_info_get_n_methods:
 * @info: a #GIStructInfo
 *
 * Obtain the number of methods this structure has.
 *
 * Returns: number of methods
 */
gint
gi_struct_info_get_n_methods (GIStructInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  StructBlob *blob = (StructBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->n_methods;
}

/**
 * gi_struct_info_get_method:
 * @info: a #GIStructInfo
 * @n: a method index
 *
 * Obtain the type information for method with specified index.
 *
 * Returns: (transfer full): the #GIFunctionInfo, free it with gi_base_info_unref()
 * when done.
 */
GIFunctionInfo *
gi_struct_info_get_method (GIStructInfo *info,
                           gint          n)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  StructBlob *blob = (StructBlob *)&rinfo->typelib->data[rinfo->offset];
  Header *header = (Header *)rinfo->typelib->data;
  gint offset;

  offset = gi_struct_get_field_offset (info, blob->n_fields) + n * header->function_blob_size;
  return (GIFunctionInfo *) gi_info_new (GI_INFO_TYPE_FUNCTION, (GIBaseInfo*)info,
                                         rinfo->typelib, offset);
}

/**
 * gi_struct_info_find_method:
 * @info: a #GIStructInfo
 * @name: a method name
 *
 * Obtain the type information for method named @name.
 *
 * Returns: (transfer full): the #GIFunctionInfo, free it with gi_base_info_unref()
 * when done.
 */
GIFunctionInfo *
gi_struct_info_find_method (GIStructInfo *info,
                            const gchar  *name)
{
  gint offset;
  GIRealInfo *rinfo = (GIRealInfo *)info;
  StructBlob *blob = (StructBlob *)&rinfo->typelib->data[rinfo->offset];

  offset = gi_struct_get_field_offset (info, blob->n_fields);
  return gi_base_info_find_method ((GIBaseInfo*)info, offset, blob->n_methods, name);
}

/**
 * gi_struct_info_get_size:
 * @info: a #GIStructInfo
 *
 * Obtain the total size of the structure.
 *
 * Returns: size of the structure in bytes
 */
gsize
gi_struct_info_get_size (GIStructInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  StructBlob *blob = (StructBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->size;
}

/**
 * gi_struct_info_get_alignment:
 * @info: a #GIStructInfo
 *
 * Obtain the required alignment of the structure.
 *
 * Returns: required alignment in bytes
 */
gsize
gi_struct_info_get_alignment (GIStructInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  StructBlob *blob = (StructBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->alignment;
}

/**
 * gi_struct_info_is_foreign:
 * @info: TODO
 *
 * TODO
 *
 * Returns: TODO
 */
gboolean
gi_struct_info_is_foreign (GIStructInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  StructBlob *blob = (StructBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->foreign;
}

/**
 * gi_struct_info_is_gtype_struct:
 * @info: a #GIStructInfo
 *
 * Return true if this structure represents the "class structure" for some
 * #GObject or #GInterface.  This function is mainly useful to hide this kind of structure
 * from generated public APIs.
 *
 * Returns: %TRUE if this is a class struct, %FALSE otherwise
 */
gboolean
gi_struct_info_is_gtype_struct (GIStructInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  StructBlob *blob = (StructBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->is_gtype_struct;
}

/**
 * gi_struct_info_get_copy_function:
 * @info: a struct information blob
 *
 * Retrieves the name of the copy function for @info, if any is set.
 *
 * Returns: (transfer none) (nullable): the name of the copy function
 *
 * Since: 1.76
 */
const char *
gi_struct_info_get_copy_function (GIStructInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  StructBlob *blob;

  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (GI_IS_STRUCT_INFO (info), NULL);

  blob = (StructBlob *)&rinfo->typelib->data[rinfo->offset];

  if (blob->copy_func)
    return gi_typelib_get_string (rinfo->typelib, blob->copy_func);

  return NULL;
}

/**
 * gi_struct_info_get_free_function:
 * @info: a struct information blob
 *
 * Retrieves the name of the free function for @info, if any is set.
 *
 * Returns: (transfer none) (nullable): the name of the free function
 *
 * Since: 1.76
 */
const char *
gi_struct_info_get_free_function (GIStructInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  StructBlob *blob;

  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (GI_IS_STRUCT_INFO (info), NULL);

  blob = (StructBlob *)&rinfo->typelib->data[rinfo->offset];

  if (blob->free_func)
    return gi_typelib_get_string (rinfo->typelib, blob->free_func);

  return NULL;
}
