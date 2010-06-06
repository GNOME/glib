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

#include <stdlib.h>
#include <string.h>

#include <glib.h>
#include <glib-object.h>

#include "gitypelib-internal.h"
#include "girepository-private.h"

/* GIStructInfo functions */
gint
g_struct_info_get_n_fields (GIStructInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  StructBlob *blob = (StructBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->n_fields;
}

static gint32
g_struct_get_field_offset (GIStructInfo *info,
			   gint         n)
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

GIFieldInfo *
g_struct_info_get_field (GIStructInfo *info,
                         gint          n)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;

  return (GIFieldInfo *) g_info_new (GI_INFO_TYPE_FIELD, (GIBaseInfo*)info, rinfo->typelib,
                                     g_struct_get_field_offset (info, n));
}

gint
g_struct_info_get_n_methods (GIStructInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  StructBlob *blob = (StructBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->n_methods;
}

GIFunctionInfo *
g_struct_info_get_method (GIStructInfo *info,
			  gint         n)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  StructBlob *blob = (StructBlob *)&rinfo->typelib->data[rinfo->offset];
  Header *header = (Header *)rinfo->typelib->data;
  gint offset;

  offset = g_struct_get_field_offset (info, blob->n_fields) + n * header->function_blob_size;
  return (GIFunctionInfo *) g_info_new (GI_INFO_TYPE_FUNCTION, (GIBaseInfo*)info,
                                        rinfo->typelib, offset);
}

static GIFunctionInfo *
find_method (GIBaseInfo   *base,
             guint32       offset,
             gint          n_methods,
             const gchar  *name)
{
  /* FIXME hash */
  GIRealInfo *rinfo = (GIRealInfo*)base;
  Header *header = (Header *)rinfo->typelib->data;
  gint i;

  for (i = 0; i < n_methods; i++)
    {
      FunctionBlob *fblob = (FunctionBlob *)&rinfo->typelib->data[offset];
      const gchar *fname = (const gchar *)&rinfo->typelib->data[fblob->name];

      if (strcmp (name, fname) == 0)
        return (GIFunctionInfo *) g_info_new (GI_INFO_TYPE_FUNCTION, base,
			                      rinfo->typelib, offset);

      offset += header->function_blob_size;
    }

  return NULL;
}

GIFunctionInfo *
g_struct_info_find_method (GIStructInfo *info,
			   const gchar  *name)
{
  gint offset;
  GIRealInfo *rinfo = (GIRealInfo *)info;
  Header *header = (Header *)rinfo->typelib->data;
  StructBlob *blob = (StructBlob *)&rinfo->typelib->data[rinfo->offset];

  offset = rinfo->offset + header->struct_blob_size
    + blob->n_fields * header->field_blob_size;

  return find_method ((GIBaseInfo*)info, offset, blob->n_methods, name);
}

gsize
g_struct_info_get_size (GIStructInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  StructBlob *blob = (StructBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->size;
}

gsize
g_struct_info_get_alignment (GIStructInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  StructBlob *blob = (StructBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->alignment;
}

gboolean
g_struct_info_is_foreign (GIStructInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  StructBlob *blob = (StructBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->foreign;
}

/**
 * g_struct_info_is_gtype_struct:
 * @info: a #GIStructInfo
 *
 * Return true if this structure represents the "class structure" for some
 * #GObject or #GInterface.  This function is mainly useful to hide this kind of structure
 * from generated public APIs.
 *
 * Returns: %TRUE if this is a class struct, %FALSE otherwise
 */
gboolean
g_struct_info_is_gtype_struct (GIStructInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  StructBlob *blob = (StructBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->is_gtype_struct;
}

/* GIObjectInfo functions */
GIObjectInfo *
g_object_info_get_parent (GIObjectInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  ObjectBlob *blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];

  if (blob->parent)
    return (GIObjectInfo *) _g_info_from_entry (rinfo->repository,
                                                rinfo->typelib, blob->parent);
  else
    return NULL;
}

gboolean
g_object_info_get_abstract (GIObjectInfo    *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  ObjectBlob *blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];
  return blob->abstract != 0;
}

const gchar *
g_object_info_get_type_name (GIObjectInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  ObjectBlob *blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];

  return g_typelib_get_string (rinfo->typelib, blob->gtype_name);
}

const gchar *
g_object_info_get_type_init (GIObjectInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  ObjectBlob *blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];

  return g_typelib_get_string (rinfo->typelib, blob->gtype_init);
}

gint
g_object_info_get_n_interfaces (GIObjectInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  ObjectBlob *blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->n_interfaces;
}

GIInterfaceInfo *
g_object_info_get_interface (GIObjectInfo *info,
			     gint          n)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  ObjectBlob *blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];

  return (GIInterfaceInfo *) _g_info_from_entry (rinfo->repository,
						 rinfo->typelib, blob->interfaces[n]);
}

gint
g_object_info_get_n_fields (GIObjectInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  ObjectBlob *blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->n_fields;
}

GIFieldInfo *
g_object_info_get_field (GIObjectInfo *info,
			 gint          n)
{
  gint offset;
  GIRealInfo *rinfo = (GIRealInfo *)info;
  Header *header = (Header *)rinfo->typelib->data;
  ObjectBlob *blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];

  offset = rinfo->offset + header->object_blob_size
    + (blob->n_interfaces + blob->n_interfaces % 2) * 2
    + n * header->field_blob_size;

  return (GIFieldInfo *) g_info_new (GI_INFO_TYPE_FIELD, (GIBaseInfo*)info, rinfo->typelib, offset);
}

gint
g_object_info_get_n_properties (GIObjectInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  ObjectBlob *blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->n_properties;
}

GIPropertyInfo *
g_object_info_get_property (GIObjectInfo *info,
			    gint          n)
{
  gint offset;
  GIRealInfo *rinfo = (GIRealInfo *)info;
  Header *header = (Header *)rinfo->typelib->data;
  ObjectBlob *blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];

  offset = rinfo->offset + header->object_blob_size
    + (blob->n_interfaces + blob->n_interfaces % 2) * 2
    + blob->n_fields * header->field_blob_size
    + n * header->property_blob_size;

  return (GIPropertyInfo *) g_info_new (GI_INFO_TYPE_PROPERTY, (GIBaseInfo*)info,
					rinfo->typelib, offset);
}

gint
g_object_info_get_n_methods (GIObjectInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  ObjectBlob *blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->n_methods;
}

GIFunctionInfo *
g_object_info_get_method (GIObjectInfo *info,
			  gint          n)
{
  gint offset;
  GIRealInfo *rinfo = (GIRealInfo *)info;
  Header *header = (Header *)rinfo->typelib->data;
  ObjectBlob *blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];

  offset = rinfo->offset + header->object_blob_size
    + (blob->n_interfaces + blob->n_interfaces % 2) * 2
    + blob->n_fields * header->field_blob_size
    + blob->n_properties * header->property_blob_size
    + n * header->function_blob_size;

    return (GIFunctionInfo *) g_info_new (GI_INFO_TYPE_FUNCTION, (GIBaseInfo*)info,
					  rinfo->typelib, offset);
}

GIFunctionInfo *
g_object_info_find_method (GIObjectInfo *info,
			   const gchar  *name)
{
  gint offset;
  GIRealInfo *rinfo = (GIRealInfo *)info;
  Header *header = (Header *)rinfo->typelib->data;
  ObjectBlob *blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];

  offset = rinfo->offset + header->object_blob_size
    + (blob->n_interfaces + blob->n_interfaces % 2) * 2
    + blob->n_fields * header->field_blob_size +
    + blob->n_properties * header->property_blob_size;

  return find_method ((GIBaseInfo*)info, offset, blob->n_methods, name);
}

gint
g_object_info_get_n_signals (GIObjectInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  ObjectBlob *blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->n_signals;
}

GISignalInfo *
g_object_info_get_signal (GIObjectInfo *info,
			  gint          n)
{
  gint offset;
  GIRealInfo *rinfo = (GIRealInfo *)info;
  Header *header = (Header *)rinfo->typelib->data;
  ObjectBlob *blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];

  offset = rinfo->offset + header->object_blob_size
    + (blob->n_interfaces + blob->n_interfaces % 2) * 2
    + blob->n_fields * header->field_blob_size
    + blob->n_properties * header->property_blob_size
    + blob->n_methods * header->function_blob_size
    + n * header->signal_blob_size;

  return (GISignalInfo *) g_info_new (GI_INFO_TYPE_SIGNAL, (GIBaseInfo*)info,
				      rinfo->typelib, offset);
}

gint
g_object_info_get_n_vfuncs (GIObjectInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  ObjectBlob *blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->n_vfuncs;
}

GIVFuncInfo *
g_object_info_get_vfunc (GIObjectInfo *info,
			 gint          n)
{
  gint offset;
  GIRealInfo *rinfo = (GIRealInfo *)info;
  Header *header = (Header *)rinfo->typelib->data;
  ObjectBlob *blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];

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

static GIVFuncInfo *
find_vfunc (GIRealInfo   *rinfo,
            guint32       offset,
            gint          n_vfuncs,
            const gchar  *name)
{
  /* FIXME hash */
  Header *header = (Header *)rinfo->typelib->data;
  gint i;

  for (i = 0; i < n_vfuncs; i++)
    {
      VFuncBlob *fblob = (VFuncBlob *)&rinfo->typelib->data[offset];
      const gchar *fname = (const gchar *)&rinfo->typelib->data[fblob->name];

      if (strcmp (name, fname) == 0)
        return (GIVFuncInfo *) g_info_new (GI_INFO_TYPE_VFUNC, (GIBaseInfo*) rinfo,
                                           rinfo->typelib, offset);

      offset += header->vfunc_blob_size;
    }

  return NULL;
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
  Header *header = (Header *)rinfo->typelib->data;
  ObjectBlob *blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];

  offset = rinfo->offset + header->object_blob_size
    + (blob->n_interfaces + blob->n_interfaces % 2) * 2
    + blob->n_fields * header->field_blob_size
    + blob->n_properties * header->property_blob_size
    + blob->n_methods * header->function_blob_size
    + blob->n_signals * header->signal_blob_size;

  return find_vfunc (rinfo, offset, blob->n_vfuncs, name);
}

gint
g_object_info_get_n_constants (GIObjectInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  ObjectBlob *blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->n_constants;
}

GIConstantInfo *
g_object_info_get_constant (GIObjectInfo *info,
			    gint          n)
{
  gint offset;
  GIRealInfo *rinfo = (GIRealInfo *)info;
  Header *header = (Header *)rinfo->typelib->data;
  ObjectBlob *blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];

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
  ObjectBlob *blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];

  if (blob->gtype_struct)
    return (GIStructInfo *) _g_info_from_entry (rinfo->repository,
                                                rinfo->typelib, blob->gtype_struct);
  else
    return NULL;
}

/* GIInterfaceInfo functions */
gint
g_interface_info_get_n_prerequisites (GIInterfaceInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  InterfaceBlob *blob = (InterfaceBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->n_prerequisites;
}

GIBaseInfo *
g_interface_info_get_prerequisite (GIInterfaceInfo *info,
				   gint            n)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  InterfaceBlob *blob = (InterfaceBlob *)&rinfo->typelib->data[rinfo->offset];

  return _g_info_from_entry (rinfo->repository,
			     rinfo->typelib, blob->prerequisites[n]);
}


gint
g_interface_info_get_n_properties (GIInterfaceInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  InterfaceBlob *blob = (InterfaceBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->n_properties;
}

GIPropertyInfo *
g_interface_info_get_property (GIInterfaceInfo *info,
			       gint            n)
{
  gint offset;
  GIRealInfo *rinfo = (GIRealInfo *)info;
  Header *header = (Header *)rinfo->typelib->data;
  InterfaceBlob *blob = (InterfaceBlob *)&rinfo->typelib->data[rinfo->offset];

  offset = rinfo->offset + header->interface_blob_size
    + (blob->n_prerequisites + (blob->n_prerequisites % 2)) * 2
    + n * header->property_blob_size;

  return (GIPropertyInfo *) g_info_new (GI_INFO_TYPE_PROPERTY, (GIBaseInfo*)info,
					rinfo->typelib, offset);
}

gint
g_interface_info_get_n_methods (GIInterfaceInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  InterfaceBlob *blob = (InterfaceBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->n_methods;
}

GIFunctionInfo *
g_interface_info_get_method (GIInterfaceInfo *info,
			     gint            n)
{
  gint offset;
  GIRealInfo *rinfo = (GIRealInfo *)info;
  Header *header = (Header *)rinfo->typelib->data;
  InterfaceBlob *blob = (InterfaceBlob *)&rinfo->typelib->data[rinfo->offset];

  offset = rinfo->offset + header->interface_blob_size
    + (blob->n_prerequisites + (blob->n_prerequisites % 2)) * 2
    + blob->n_properties * header->property_blob_size
    + n * header->function_blob_size;

  return (GIFunctionInfo *) g_info_new (GI_INFO_TYPE_FUNCTION, (GIBaseInfo*)info,
					rinfo->typelib, offset);
}

GIFunctionInfo *
g_interface_info_find_method (GIInterfaceInfo *info,
			      const gchar     *name)
{
  gint offset;
  GIRealInfo *rinfo = (GIRealInfo *)info;
  Header *header = (Header *)rinfo->typelib->data;
  InterfaceBlob *blob = (InterfaceBlob *)&rinfo->typelib->data[rinfo->offset];

  offset = rinfo->offset + header->interface_blob_size
    + (blob->n_prerequisites + (blob->n_prerequisites % 2)) * 2
    + blob->n_properties * header->property_blob_size;

  return find_method ((GIBaseInfo*)info, offset, blob->n_methods, name);
}

gint
g_interface_info_get_n_signals (GIInterfaceInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  InterfaceBlob *blob = (InterfaceBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->n_signals;
}

GISignalInfo *
g_interface_info_get_signal (GIInterfaceInfo *info,
			     gint            n)
{
  gint offset;
  GIRealInfo *rinfo = (GIRealInfo *)info;
  Header *header = (Header *)rinfo->typelib->data;
  InterfaceBlob *blob = (InterfaceBlob *)&rinfo->typelib->data[rinfo->offset];

  offset = rinfo->offset + header->interface_blob_size
    + (blob->n_prerequisites + (blob->n_prerequisites % 2)) * 2
    + blob->n_properties * header->property_blob_size
    + blob->n_methods * header->function_blob_size
    + n * header->signal_blob_size;

  return (GISignalInfo *) g_info_new (GI_INFO_TYPE_SIGNAL, (GIBaseInfo*)info,
				      rinfo->typelib, offset);
}

gint
g_interface_info_get_n_vfuncs (GIInterfaceInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  InterfaceBlob *blob = (InterfaceBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->n_vfuncs;
}

GIVFuncInfo *
g_interface_info_get_vfunc (GIInterfaceInfo *info,
			    gint            n)
{
  gint offset;
  GIRealInfo *rinfo = (GIRealInfo *)info;
  Header *header = (Header *)rinfo->typelib->data;
  InterfaceBlob *blob = (InterfaceBlob *)&rinfo->typelib->data[rinfo->offset];

  offset = rinfo->offset + header->interface_blob_size
    + (blob->n_prerequisites + (blob->n_prerequisites % 2)) * 2
    + blob->n_properties * header->property_blob_size
    + blob->n_methods * header->function_blob_size
    + blob->n_signals * header->signal_blob_size
    + n * header->vfunc_blob_size;

  return (GIVFuncInfo *) g_info_new (GI_INFO_TYPE_VFUNC, (GIBaseInfo*)info,
				     rinfo->typelib, offset);
}

/**
 * g_interface_info_find_vfunc:
 * @info: a #GIObjectInfo
 * @name: The name of a virtual function to find.
 *
 * Locate a virtual function slot with name @name. See the documentation
 * for g_object_info_find_vfunc() for more information on virtuals.
 *
 * Returns: (transfer full): the #GIVFuncInfo, or %NULL. Free it with
 * g_base_info_unref() when done.
 */
GIVFuncInfo *
g_interface_info_find_vfunc (GIInterfaceInfo *info,
                             const gchar  *name)
{
  gint offset;
  GIRealInfo *rinfo = (GIRealInfo *)info;
  Header *header = (Header *)rinfo->typelib->data;
  InterfaceBlob *blob = (InterfaceBlob *)&rinfo->typelib->data[rinfo->offset];

  offset = rinfo->offset + header->interface_blob_size
    + (blob->n_prerequisites + blob->n_prerequisites % 2) * 2
    + blob->n_properties * header->property_blob_size
    + blob->n_methods * header->function_blob_size
    + blob->n_signals * header->signal_blob_size;

  return find_vfunc (rinfo, offset, blob->n_vfuncs, name);
}

gint
g_interface_info_get_n_constants (GIInterfaceInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  InterfaceBlob *blob = (InterfaceBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->n_constants;
}

GIConstantInfo *
g_interface_info_get_constant (GIInterfaceInfo *info,
			       gint             n)
{
  gint offset;
  GIRealInfo *rinfo = (GIRealInfo *)info;
  Header *header = (Header *)rinfo->typelib->data;
  InterfaceBlob *blob = (InterfaceBlob *)&rinfo->typelib->data[rinfo->offset];

  offset = rinfo->offset + header->interface_blob_size
    + (blob->n_prerequisites + (blob->n_prerequisites % 2)) * 2
    + blob->n_properties * header->property_blob_size
    + blob->n_methods * header->function_blob_size
    + blob->n_signals * header->signal_blob_size
    + blob->n_vfuncs * header->vfunc_blob_size
    + n * header->constant_blob_size;

  return (GIConstantInfo *) g_info_new (GI_INFO_TYPE_CONSTANT, (GIBaseInfo*)info,
					rinfo->typelib, offset);
}

/**
 * g_interface_info_get_iface_struct:
 * @info: a #GIInterfaceInfo
 *
 * Returns the layout C structure associated with this #GInterface.
 *
 * Returns: (transfer full): the #GIStructInfo or %NULL. Free it with
 * g_base_info_unref() when done.
 */
GIStructInfo *
g_interface_info_get_iface_struct (GIInterfaceInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  InterfaceBlob *blob = (InterfaceBlob *)&rinfo->typelib->data[rinfo->offset];

  if (blob->gtype_struct)
    return (GIStructInfo *) _g_info_from_entry (rinfo->repository,
                                                rinfo->typelib, blob->gtype_struct);
  else
    return NULL;
}

/* GIPropertyInfo functions */
GParamFlags
g_property_info_get_flags (GIPropertyInfo *info)
{
  GParamFlags flags;
  GIRealInfo *rinfo = (GIRealInfo *)info;
  PropertyBlob *blob = (PropertyBlob *)&rinfo->typelib->data[rinfo->offset];

  flags = 0;

  if (blob->readable)
    flags = flags | G_PARAM_READABLE;

  if (blob->writable)
    flags = flags | G_PARAM_WRITABLE;

  if (blob->construct)
    flags = flags | G_PARAM_CONSTRUCT;

  if (blob->construct_only)
    flags = flags | G_PARAM_CONSTRUCT_ONLY;

  return flags;
}

GITypeInfo *
g_property_info_get_type (GIPropertyInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;

  return _g_type_info_new ((GIBaseInfo*)info, rinfo->typelib, rinfo->offset + G_STRUCT_OFFSET (PropertyBlob, type));
}


/* GISignalInfo functions */
GSignalFlags
g_signal_info_get_flags (GISignalInfo *info)
{
  GSignalFlags flags;

  GIRealInfo *rinfo = (GIRealInfo *)info;
  SignalBlob *blob = (SignalBlob *)&rinfo->typelib->data[rinfo->offset];

  flags = 0;

  if (blob->run_first)
    flags = flags | G_SIGNAL_RUN_FIRST;

  if (blob->run_last)
    flags = flags | G_SIGNAL_RUN_LAST;

  if (blob->run_cleanup)
    flags = flags | G_SIGNAL_RUN_CLEANUP;

  if (blob->no_recurse)
    flags = flags | G_SIGNAL_NO_RECURSE;

  if (blob->detailed)
    flags = flags | G_SIGNAL_DETAILED;

  if (blob->action)
    flags = flags | G_SIGNAL_ACTION;

  if (blob->no_hooks)
    flags = flags | G_SIGNAL_NO_HOOKS;

  return flags;
}

GIVFuncInfo *
g_signal_info_get_class_closure (GISignalInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  SignalBlob *blob = (SignalBlob *)&rinfo->typelib->data[rinfo->offset];

  if (blob->has_class_closure)
    return g_interface_info_get_vfunc ((GIInterfaceInfo *)rinfo->container, blob->class_closure);

  return NULL;
}

gboolean
g_signal_info_true_stops_emit (GISignalInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  SignalBlob *blob = (SignalBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->true_stops_emit;
}

/* GIVFuncInfo functions */
GIVFuncInfoFlags
g_vfunc_info_get_flags (GIVFuncInfo *info)
{
  GIVFuncInfoFlags flags;

  GIRealInfo *rinfo = (GIRealInfo *)info;
  VFuncBlob *blob = (VFuncBlob *)&rinfo->typelib->data[rinfo->offset];

  flags = 0;

  if (blob->must_chain_up)
    flags = flags | GI_VFUNC_MUST_CHAIN_UP;

  if (blob->must_be_implemented)
    flags = flags | GI_VFUNC_MUST_OVERRIDE;

  if (blob->must_not_be_implemented)
    flags = flags | GI_VFUNC_MUST_NOT_OVERRIDE;

  return flags;
}

gint
g_vfunc_info_get_offset (GIVFuncInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  VFuncBlob *blob = (VFuncBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->struct_offset;
}

GISignalInfo *
g_vfunc_info_get_signal (GIVFuncInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  VFuncBlob *blob = (VFuncBlob *)&rinfo->typelib->data[rinfo->offset];

  if (blob->class_closure)
    return g_interface_info_get_signal ((GIInterfaceInfo *)rinfo->container, blob->signal);

  return NULL;
}

/**
 * g_vfunc_info_get_invoker:
 * @info: a #GIVFuncInfo
 *
 * If this virtual function has an associated invoker method, this
 * method will return it.  An invoker method is a C entry point.
 *
 * Not all virtuals will have invokers.
 *
 * Returns: (transfer full): the #GIVFuncInfo or %NULL. Free it with
 * g_base_info_unref() when done.
 */
GIFunctionInfo *
g_vfunc_info_get_invoker (GIVFuncInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  VFuncBlob *blob = (VFuncBlob *)&rinfo->typelib->data[rinfo->offset];
  GIBaseInfo *container = rinfo->container;
  GIInfoType parent_type;

  /* 1023 = 0x3ff is the maximum of the 10 bits for invoker index */
  if (blob->invoker == 1023)
    return NULL;

  parent_type = g_base_info_get_type (container);
  if (parent_type == GI_INFO_TYPE_OBJECT)
    return g_object_info_get_method ((GIObjectInfo*)container, blob->invoker);
  else if (parent_type == GI_INFO_TYPE_INTERFACE)
    return g_interface_info_get_method ((GIInterfaceInfo*)container, blob->invoker);
  else
    g_assert_not_reached ();
}

/* GIConstantInfo functions */
GITypeInfo *
g_constant_info_get_type (GIConstantInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;

  return _g_type_info_new ((GIBaseInfo*)info, rinfo->typelib, rinfo->offset + 8);
}

gint
g_constant_info_get_value (GIConstantInfo *info,
			   GArgument      *value)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  ConstantBlob *blob = (ConstantBlob *)&rinfo->typelib->data[rinfo->offset];

  /* FIXME non-basic types ? */
  if (blob->type.flags.reserved == 0 && blob->type.flags.reserved2 == 0)
    {
      if (blob->type.flags.pointer)
	value->v_pointer = g_memdup (&rinfo->typelib->data[blob->offset], blob->size);
      else
	{
	  switch (blob->type.flags.tag)
	    {
	    case GI_TYPE_TAG_BOOLEAN:
	      value->v_boolean = *(gboolean*)&rinfo->typelib->data[blob->offset];
	      break;
	    case GI_TYPE_TAG_INT8:
	      value->v_int8 = *(gint8*)&rinfo->typelib->data[blob->offset];
	      break;
	    case GI_TYPE_TAG_UINT8:
	      value->v_uint8 = *(guint8*)&rinfo->typelib->data[blob->offset];
	      break;
	    case GI_TYPE_TAG_INT16:
	      value->v_int16 = *(gint16*)&rinfo->typelib->data[blob->offset];
	      break;
	    case GI_TYPE_TAG_UINT16:
	      value->v_uint16 = *(guint16*)&rinfo->typelib->data[blob->offset];
	      break;
	    case GI_TYPE_TAG_INT32:
	      value->v_int32 = *(gint32*)&rinfo->typelib->data[blob->offset];
	      break;
	    case GI_TYPE_TAG_UINT32:
	      value->v_uint32 = *(guint32*)&rinfo->typelib->data[blob->offset];
	      break;
	    case GI_TYPE_TAG_INT64:
	      value->v_int64 = *(gint64*)&rinfo->typelib->data[blob->offset];
	      break;
	    case GI_TYPE_TAG_UINT64:
	      value->v_uint64 = *(guint64*)&rinfo->typelib->data[blob->offset];
	      break;
	    case GI_TYPE_TAG_FLOAT:
	      value->v_float = *(gfloat*)&rinfo->typelib->data[blob->offset];
	      break;
	    case GI_TYPE_TAG_DOUBLE:
	      value->v_double = *(gdouble*)&rinfo->typelib->data[blob->offset];
	      break;
	    case GI_TYPE_TAG_TIME_T:
	      value->v_long = *(long*)&rinfo->typelib->data[blob->offset];
	      break;
	    case GI_TYPE_TAG_SHORT:
	      value->v_short = *(gshort*)&rinfo->typelib->data[blob->offset];
	      break;
	    case GI_TYPE_TAG_USHORT:
	      value->v_ushort = *(gushort*)&rinfo->typelib->data[blob->offset];
	      break;
	    case GI_TYPE_TAG_INT:
	      value->v_int = *(gint*)&rinfo->typelib->data[blob->offset];
	      break;
	    case GI_TYPE_TAG_UINT:
	      value->v_uint = *(guint*)&rinfo->typelib->data[blob->offset];
	      break;
	    case GI_TYPE_TAG_LONG:
	      value->v_long = *(glong*)&rinfo->typelib->data[blob->offset];
	      break;
	    case GI_TYPE_TAG_ULONG:
	      value->v_ulong = *(gulong*)&rinfo->typelib->data[blob->offset];
	      break;
	    }
	}
    }

  return blob->size;
}

/* GIUnionInfo functions */
gint
g_union_info_get_n_fields  (GIUnionInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  UnionBlob *blob = (UnionBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->n_fields;
}

GIFieldInfo *
g_union_info_get_field (GIUnionInfo *info,
			gint         n)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  Header *header = (Header *)rinfo->typelib->data;

  return (GIFieldInfo *) g_info_new (GI_INFO_TYPE_FIELD, (GIBaseInfo*)info, rinfo->typelib,
				     rinfo->offset + header->union_blob_size +
				     n * header->field_blob_size);
}

gint
g_union_info_get_n_methods (GIUnionInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  UnionBlob *blob = (UnionBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->n_functions;
}

GIFunctionInfo *
g_union_info_get_method (GIUnionInfo *info,
			 gint         n)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  UnionBlob *blob = (UnionBlob *)&rinfo->typelib->data[rinfo->offset];
  Header *header = (Header *)rinfo->typelib->data;
  gint offset;

  offset = rinfo->offset + header->union_blob_size
    + blob->n_fields * header->field_blob_size
    + n * header->function_blob_size;
  return (GIFunctionInfo *) g_info_new (GI_INFO_TYPE_FUNCTION, (GIBaseInfo*)info,
					rinfo->typelib, offset);
}

gboolean
g_union_info_is_discriminated (GIUnionInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  UnionBlob *blob = (UnionBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->discriminated;
}

gint
g_union_info_get_discriminator_offset (GIUnionInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  UnionBlob *blob = (UnionBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->discriminator_offset;
}

GITypeInfo *
g_union_info_get_discriminator_type (GIUnionInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;

  return _g_type_info_new ((GIBaseInfo*)info, rinfo->typelib, rinfo->offset + 24);
}

GIConstantInfo *
g_union_info_get_discriminator (GIUnionInfo *info,
				gint         n)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  UnionBlob *blob = (UnionBlob *)&rinfo->typelib->data[rinfo->offset];

  if (blob->discriminated)
    {
      Header *header = (Header *)rinfo->typelib->data;
      gint offset;

      offset = rinfo->offset + header->union_blob_size
	+ blob->n_fields * header->field_blob_size
	+ blob->n_functions * header->function_blob_size
	+ n * header->constant_blob_size;

      return (GIConstantInfo *) g_info_new (GI_INFO_TYPE_CONSTANT, (GIBaseInfo*)info,
					    rinfo->typelib, offset);
    }

  return NULL;
}

GIFunctionInfo *
g_union_info_find_method (GIUnionInfo *info,
                          const gchar *name)
{
  gint offset;
  GIRealInfo *rinfo = (GIRealInfo *)info;
  Header *header = (Header *)rinfo->typelib->data;
  UnionBlob *blob = (UnionBlob *)&rinfo->typelib->data[rinfo->offset];

  offset = rinfo->offset + header->union_blob_size
    + blob->n_fields * header->field_blob_size;

  return find_method ((GIBaseInfo*)info, offset, blob->n_functions, name);
}

gsize
g_union_info_get_size (GIUnionInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  UnionBlob *blob = (UnionBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->size;
}

gsize
g_union_info_get_alignment (GIUnionInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  UnionBlob *blob = (UnionBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->alignment;
}
