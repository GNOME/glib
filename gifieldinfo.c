/* GObject introspection: Field implementation
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
 * SECTION:gifieldinfo
 * @Short_description: Struct representing a struct or union field
 * @Title: GIFieldInfo
 *
 * A GIFieldInfo struct represents a field of a struct (see #GIStructInfo),
 * union (see #GIUnionInfo) or an object (see #GIObjectInfo). The GIFieldInfo
 * is fetched by calling g_struct_info_get_field(), g_union_info_get_field()
 * or g_object_info_get_value().
 * A field has a size, type and a struct offset asssociated and a set of flags,
 * which is currently #GI_FIELD_IS_READABLE or #GI_FIELD_IS_WRITABLE.
 */

GIFieldInfoFlags
g_field_info_get_flags (GIFieldInfo *info)
{
  GIFieldInfoFlags flags;

  GIRealInfo *rinfo = (GIRealInfo *)info;
  FieldBlob *blob = (FieldBlob *)&rinfo->typelib->data[rinfo->offset];

  flags = 0;

  if (blob->readable)
    flags = flags | GI_FIELD_IS_READABLE;

  if (blob->writable)
    flags = flags | GI_FIELD_IS_WRITABLE;

  return flags;
}

gint
g_field_info_get_size (GIFieldInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  FieldBlob *blob = (FieldBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->bits;
}

gint
g_field_info_get_offset (GIFieldInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  FieldBlob *blob = (FieldBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->struct_offset;
}

GITypeInfo *
g_field_info_get_type (GIFieldInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  Header *header = (Header *)rinfo->typelib->data;
  FieldBlob *blob = (FieldBlob *)&rinfo->typelib->data[rinfo->offset];
  GIRealInfo *type_info;

  if (blob->has_embedded_type)
    {
      type_info = (GIRealInfo *) g_info_new (GI_INFO_TYPE_TYPE,
                                                (GIBaseInfo*)info, rinfo->typelib,
                                                rinfo->offset + header->field_blob_size);
      type_info->type_is_embedded = TRUE;
    }
  else
    return _g_type_info_new ((GIBaseInfo*)info, rinfo->typelib, rinfo->offset + G_STRUCT_OFFSET (FieldBlob, type));

  return (GIBaseInfo*)type_info;
}

