/* GObject introspection: Interface implementation
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
#include "girffi.h"

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

  return _g_base_info_find_method ((GIBaseInfo*)info, offset, blob->n_methods, name);
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

  return _g_base_info_find_vfunc (rinfo, offset, blob->n_vfuncs, name);
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

