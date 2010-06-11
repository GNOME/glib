/* GObject introspection: Object implementation
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
 * SECTION:giobjectinfo
 * @Short_description: Struct representing a GObject
 * @Title: GIObjectInfo
 *
 * GIPropertyInfo represents a #GObject. This doesn't represent a specific
 * instance of a GObject, instead this represent the object type (eg class).
 *
 * A GObject has methods, fields, properties, signals, interfaces, constants
 * and virtual functions.
 *
 * <refsect1 id="gi-giobjectinfo.struct-hierarchy" role="struct_hierarchy">
 * <title role="struct_hierarchy.title">Struct hierarchy</title>
 * <synopsis>
 *   <link linkend="gi-GIBaseInfo">GIBaseInfo</link>
 *    +----<link linkend="gi-GIRegisteredTypeInfo">GIRegisteredTypeInfo</link>
 *          +----GIObjectInfo
 * </synopsis>
 * </refsect1>
 */

/**
 * g_object_info_get_parent:
 * @info: a #GIObjectInfo
 *
 * Obtain the parent of the object type.
 *
 * Returns: (transfer full): the #GIObjectInfo. Free the struct by calling
 * g_base_info_unref() when done.
 */
GIObjectInfo *
g_object_info_get_parent (GIObjectInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  ObjectBlob *blob;

  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (GI_IS_OBJECT_INFO (info), NULL);

  blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];

  if (blob->parent)
    return (GIObjectInfo *) _g_info_from_entry (rinfo->repository,
                                                rinfo->typelib, blob->parent);
  else
    return NULL;
}

/**
 * g_object_info_get_abstract:
 * @info: a #GIObjectInfo
 *
 * Obtain if the object type is an abstract type, eg if it cannot be
 * instantiated
 *
 * Returns: %TRUE if the object type is abstract
 */
gboolean
g_object_info_get_abstract (GIObjectInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  ObjectBlob *blob;

  g_return_val_if_fail (info != NULL, FALSE);
  g_return_val_if_fail (GI_IS_OBJECT_INFO (info), FALSE);

  blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->abstract != 0;
}

/**
 * g_object_info_get_type_name:
 * @info: a #GIObjectInfo
 *
 * Obtain the name of the objects class/type.
 *
 * Returns: name of the objects type
 */
const gchar *
g_object_info_get_type_name (GIObjectInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  ObjectBlob *blob;

  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (GI_IS_OBJECT_INFO (info), NULL);

  blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];

  return g_typelib_get_string (rinfo->typelib, blob->gtype_name);
}

/**
 * g_object_info_get_type_init:
 * @info: a #GIObjectInfo
 *
 * Obtain the function which when called will return the GType
 * function for which this object type is registered.
 *
 * Returns: the type init function
 */
const gchar *
g_object_info_get_type_init (GIObjectInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  ObjectBlob *blob;

  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (GI_IS_OBJECT_INFO (info), NULL);

  blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];

  return g_typelib_get_string (rinfo->typelib, blob->gtype_init);
}

/**
 * g_object_info_get_n_interfaces:
 * @info: a #GIObjectInfo
 *
 * Obtain the number of interfaces that this object type has.
 *
 * Returns: number of interfaces
 */
gint
g_object_info_get_n_interfaces (GIObjectInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  ObjectBlob *blob;

  g_return_val_if_fail (info != NULL, 0);
  g_return_val_if_fail (GI_IS_OBJECT_INFO (info), 0);

  blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->n_interfaces;
}

/**
 * g_object_info_get_interface:
 * @info: a #GIObjectInfo
 * @n: index of interface to get
 *
 * Obtain an object type interface at index @n.
 *
 * Returns: (transfer full): the #GIInterfaceInfo. Free the struct by calling
 * g_base_info_unref() when done.
 */
GIInterfaceInfo *
g_object_info_get_interface (GIObjectInfo *info,
			     gint          n)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  ObjectBlob *blob;

  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (GI_IS_OBJECT_INFO (info), NULL);

  blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];

  return (GIInterfaceInfo *) _g_info_from_entry (rinfo->repository,
						 rinfo->typelib, blob->interfaces[n]);
}

/**
 * g_object_info_get_n_fields:
 * @info: a #GIObjectInfo
 *
 * Obtain the number of fields that this object type has.
 *
 * Returns: number of fields
 */
gint
g_object_info_get_n_fields (GIObjectInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  ObjectBlob *blob;

  g_return_val_if_fail (info != NULL, 0);
  g_return_val_if_fail (GI_IS_OBJECT_INFO (info), 0);

  blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->n_fields;
}

/**
 * g_object_info_get_field:
 * @info: a #GIObjectInfo
 * @n: index of field to get
 *
 * Obtain an object type field at index @n.
 *
 * Returns: (transfer full): the #GIFieldInfo. Free the struct by calling
 * g_base_info_unref() when done.
 */
GIFieldInfo *
g_object_info_get_field (GIObjectInfo *info,
			 gint          n)
{
  gint offset;
  GIRealInfo *rinfo = (GIRealInfo *)info;
  Header *header;
  ObjectBlob *blob;

  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (GI_IS_OBJECT_INFO (info), NULL);

  header = (Header *)rinfo->typelib->data;
  blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];

  offset = rinfo->offset + header->object_blob_size
    + (blob->n_interfaces + blob->n_interfaces % 2) * 2
    + n * header->field_blob_size;

  return (GIFieldInfo *) g_info_new (GI_INFO_TYPE_FIELD, (GIBaseInfo*)info, rinfo->typelib, offset);
}

/**
 * g_object_info_get_n_properties:
 * @info: a #GIObjectInfo
 *
 * Obtain the number of properties that this object type has.
 *
 * Returns: number of properties
 */
gint
g_object_info_get_n_properties (GIObjectInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  ObjectBlob *blob;

  g_return_val_if_fail (info != NULL, 0);
  g_return_val_if_fail (GI_IS_OBJECT_INFO (info), 0);

  blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];
  return blob->n_properties;
}

/**
 * g_object_info_get_property:
 * @info: a #GIObjectInfo
 * @n: index of property to get
 *
 * Obtain an object type property at index @n.
 *
 * Returns: (transfer full): the #GIPropertyInfo. Free the struct by calling
 * g_base_info_unref() when done.
 */
GIPropertyInfo *
g_object_info_get_property (GIObjectInfo *info,
			    gint          n)
{
  gint offset;
  GIRealInfo *rinfo = (GIRealInfo *)info;
  Header *header;
  ObjectBlob *blob;

  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (GI_IS_OBJECT_INFO (info), NULL);

  header = (Header *)rinfo->typelib->data;
  blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];

  offset = rinfo->offset + header->object_blob_size
    + (blob->n_interfaces + blob->n_interfaces % 2) * 2
    + blob->n_fields * header->field_blob_size
    + n * header->property_blob_size;

  return (GIPropertyInfo *) g_info_new (GI_INFO_TYPE_PROPERTY, (GIBaseInfo*)info,
					rinfo->typelib, offset);
}

/**
 * g_object_info_get_n_methods:
 * @info: a #GIObjectInfo
 *
 * Obtain the number of methods that this object type has.
 *
 * Returns: number of methods
 */
gint
g_object_info_get_n_methods (GIObjectInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  ObjectBlob *blob;

  g_return_val_if_fail (info != NULL, 0);
  g_return_val_if_fail (GI_IS_OBJECT_INFO (info), 0);

  blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->n_methods;
}

/**
 * g_object_info_get_method:
 * @info: a #GIObjectInfo
 * @n: index of method to get
 *
 * Obtain an object type method at index @n.
 *
 * Returns: (transfer full): the #GIFunctionInfo. Free the struct by calling
 * g_base_info_unref() when done.
 */
GIFunctionInfo *
g_object_info_get_method (GIObjectInfo *info,
			  gint          n)
{
  gint offset;
  GIRealInfo *rinfo = (GIRealInfo *)info;
  Header *header;
  ObjectBlob *blob;

  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (GI_IS_OBJECT_INFO (info), NULL);

  header = (Header *)rinfo->typelib->data;
  blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];


  offset = rinfo->offset + header->object_blob_size
    + (blob->n_interfaces + blob->n_interfaces % 2) * 2
    + blob->n_fields * header->field_blob_size
    + blob->n_properties * header->property_blob_size
    + n * header->function_blob_size;

    return (GIFunctionInfo *) g_info_new (GI_INFO_TYPE_FUNCTION, (GIBaseInfo*)info,
					  rinfo->typelib, offset);
}

/**
 * g_object_info_find_method:
 * @info: a #GIObjectInfo
 *
 * Returns: (transfer full): the #GIFunctionInfo. Free the struct by calling
 * g_base_info_unref() when done.
 */
GIFunctionInfo *
g_object_info_find_method (GIObjectInfo *info,
			   const gchar  *name)
{
  gint offset;
  GIRealInfo *rinfo = (GIRealInfo *)info;
  Header *header;
  ObjectBlob *blob;

  g_return_val_if_fail (info != NULL, 0);
  g_return_val_if_fail (GI_IS_OBJECT_INFO (info), 0);

  header = (Header *)rinfo->typelib->data;
  blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];

  offset = rinfo->offset + header->object_blob_size
    + (blob->n_interfaces + blob->n_interfaces % 2) * 2
    + blob->n_fields * header->field_blob_size +
    + blob->n_properties * header->property_blob_size;

  return _g_base_info_find_method ((GIBaseInfo*)info, offset, blob->n_methods, name);
}

/**
 * g_object_info_get_n_signals:
 * @info: a #GIObjectInfo
 *
 * Obtain the number of signals that this object type has.
 *
 * Returns: number of signals
 */
gint
g_object_info_get_n_signals (GIObjectInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  ObjectBlob *blob;

  g_return_val_if_fail (info != NULL, 0);
  g_return_val_if_fail (GI_IS_OBJECT_INFO (info), 0);

  blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->n_signals;
}

/**
 * g_object_info_get_signal:
 * @info: a #GIObjectInfo
 * @n: index of signal to get
 *
 * Obtain an object type signal at index @n.
 *
 * Returns: (transfer full): the #GISignalInfo. Free the struct by calling
 * g_base_info_unref() when done.
 */
GISignalInfo *
g_object_info_get_signal (GIObjectInfo *info,
			  gint          n)
{
  gint offset;
  GIRealInfo *rinfo = (GIRealInfo *)info;
  Header *header;
  ObjectBlob *blob;

  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (GI_IS_OBJECT_INFO (info), NULL);

  header = (Header *)rinfo->typelib->data;
  blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];

  offset = rinfo->offset + header->object_blob_size
    + (blob->n_interfaces + blob->n_interfaces % 2) * 2
    + blob->n_fields * header->field_blob_size
    + blob->n_properties * header->property_blob_size
    + blob->n_methods * header->function_blob_size
    + n * header->signal_blob_size;

  return (GISignalInfo *) g_info_new (GI_INFO_TYPE_SIGNAL, (GIBaseInfo*)info,
				      rinfo->typelib, offset);
}

/**
 * g_object_info_get_n_vfuncs:
 * @info: a #GIObjectInfo
 *
 * Obtain the number of virtual functions that this object type has.
 *
 * Returns: number of virtual functions
 */
gint
g_object_info_get_n_vfuncs (GIObjectInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  ObjectBlob *blob;

  g_return_val_if_fail (info != NULL, 0);
  g_return_val_if_fail (GI_IS_OBJECT_INFO (info), 0);

  blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->n_vfuncs;
}

/**
 * g_object_info_get_vfunc:
 * @info: a #GIObjectInfo
 * @n: index of virtual function to get
 *
 * Obtain an object type virtual function at index @n.
 *
 * Returns: (transfer full): the #GIVFuncInfo. Free the struct by calling
 * g_base_info_unref() when done.
 */
GIVFuncInfo *
g_object_info_get_vfunc (GIObjectInfo *info,
			 gint          n)
{
  gint offset;
  GIRealInfo *rinfo = (GIRealInfo *)info;
  Header *header;
  ObjectBlob *blob;

  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (GI_IS_OBJECT_INFO (info), NULL);

  header = (Header *)rinfo->typelib->data;
  blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];

  offset = rinfo->offset + header->object_blob_size
    + (blob->n_interfaces + blob->n_interfaces % 2) * 2
    + blob->n_fields * header->field_blob_size
    + blob->n_properties * header->property_blob_size
    + blob->n_methods * header->function_blob_size
    + blob->n_signals * header->signal_blob_size
    + n * header->vfunc_blob_size;

  return (GIVFuncInfo *) g_info_new (GI_INFO_TYPE_VFUNC, (GIBaseInfo*)info,
				     rinfo->typelib, offset);
}

/**
 * g_object_info_find_vfunc:
 * @info: a #GIObjectInfo
 * @name: The name of a virtual function to find.
 *
 * Locate a virtual function slot with name @name. Note that the namespace
 * for virtuals is distinct from that of methods; there may or may not be
 * a concrete method associated for a virtual. If there is one, it may
 * be retrieved using g_vfunc_info_get_invoker(), otherwise %NULL will be
 * returned.
 * See the documentation for g_vfunc_info_get_invoker() for more
 * information on invoking virtuals.
 *
 * Returns: (transfer full): the #GIVFuncInfo, or %NULL. Free it with
 * g_base_info_unref() when done.
 */
GIVFuncInfo *
g_object_info_find_vfunc (GIObjectInfo *info,
                          const gchar  *name)
{
  gint offset;
  GIRealInfo *rinfo = (GIRealInfo *)info;
  Header *header;
  ObjectBlob *blob;

  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (GI_IS_OBJECT_INFO (info), NULL);

  header = (Header *)rinfo->typelib->data;
  blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];

  offset = rinfo->offset + header->object_blob_size
    + (blob->n_interfaces + blob->n_interfaces % 2) * 2
    + blob->n_fields * header->field_blob_size
    + blob->n_properties * header->property_blob_size
    + blob->n_methods * header->function_blob_size
    + blob->n_signals * header->signal_blob_size;

  return _g_base_info_find_vfunc (rinfo, offset, blob->n_vfuncs, name);
}

/**
 * g_object_info_get_n_constants:
 * @info: a #GIObjectInfo
 *
 * Obtain the number of constants that this object type has.
 *
 * Returns: number of constants
 */
gint
g_object_info_get_n_constants (GIObjectInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  ObjectBlob *blob;

  g_return_val_if_fail (info != NULL, 0);
  g_return_val_if_fail (GI_IS_OBJECT_INFO (info), 0);

  blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->n_constants;
}

/**
 * g_object_info_get_constant:
 * @info: a #GIObjectInfo
 * @n: index of constant to get
 *
 * Obtain an object type constant at index @n.
 *
 * Returns: (transfer full): the #GIConstantInfo. Free the struct by calling
 * g_base_info_unref() when done.
 */
GIConstantInfo *
g_object_info_get_constant (GIObjectInfo *info,
			    gint          n)
{
  gint offset;
  GIRealInfo *rinfo = (GIRealInfo *)info;
  Header *header;
  ObjectBlob *blob;

  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (GI_IS_OBJECT_INFO (info), NULL);

  header = (Header *)rinfo->typelib->data;
  blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];

  offset = rinfo->offset + header->object_blob_size
    + (blob->n_interfaces + blob->n_interfaces % 2) * 2
    + blob->n_fields * header->field_blob_size
    + blob->n_properties * header->property_blob_size
    + blob->n_methods * header->function_blob_size
    + blob->n_signals * header->signal_blob_size
    + blob->n_vfuncs * header->vfunc_blob_size
    + n * header->constant_blob_size;

  return (GIConstantInfo *) g_info_new (GI_INFO_TYPE_CONSTANT, (GIBaseInfo*)info,
					rinfo->typelib, offset);
}

/**
 * g_object_info_get_class_struct:
 * @info: a #GIObjectInfo
 *
 * Every #GObject has two structures; an instance structure and a class
 * structure.  This function returns the metadata for the class structure.
 *
 * Returns: (transfer full): the #GIStructInfo or %NULL. Free with
 * g_base_info_unref() when done.
 */
GIStructInfo *
g_object_info_get_class_struct (GIObjectInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  ObjectBlob *blob;

  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (GI_IS_OBJECT_INFO (info), NULL);

  blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];

  if (blob->gtype_struct)
    return (GIStructInfo *) _g_info_from_entry (rinfo->repository,
                                                rinfo->typelib, blob->gtype_struct);
  else
    return NULL;
}

