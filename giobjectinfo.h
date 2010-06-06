/* GObject introspection: Object
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

#ifndef __GIOBJECTINFO_H__
#define __GIOBJECTINFO_H__

#if !defined (__GIREPOSITORY_H_INSIDE__) && !defined (GI_COMPILATION)
#error "Only <girepository.h> can be included directly."
#endif

#include <gitypes.h>

G_BEGIN_DECLS

/* GIObjectInfo */

#define GI_IS_OBJECT_INFO(info) \
    (g_base_info_get_type((GIBaseInfo*)info) ==  GI_INFO_TYPE_OBJECT)

const gchar *     g_object_info_get_type_name	 (GIObjectInfo *info);
const gchar *     g_object_info_get_type_init	 (GIObjectInfo *info);
gboolean          g_object_info_get_abstract     (GIObjectInfo *info);
GIObjectInfo *    g_object_info_get_parent       (GIObjectInfo *info);
gint              g_object_info_get_n_interfaces (GIObjectInfo *info);
GIInterfaceInfo * g_object_info_get_interface    (GIObjectInfo *info,
						  gint          n);
gint              g_object_info_get_n_fields     (GIObjectInfo *info);
GIFieldInfo *     g_object_info_get_field        (GIObjectInfo *info,
						  gint          n);
gint              g_object_info_get_n_properties (GIObjectInfo *info);
GIPropertyInfo *  g_object_info_get_property     (GIObjectInfo *info,
						  gint          n);
gint              g_object_info_get_n_methods    (GIObjectInfo *info);
GIFunctionInfo *  g_object_info_get_method       (GIObjectInfo *info,
						  gint          n);
GIFunctionInfo *  g_object_info_find_method      (GIObjectInfo *info,
						  const gchar  *name);
gint              g_object_info_get_n_signals    (GIObjectInfo *info);
GISignalInfo *    g_object_info_get_signal       (GIObjectInfo *info,
						  gint          n);
gint              g_object_info_get_n_vfuncs     (GIObjectInfo *info);
GIVFuncInfo *     g_object_info_get_vfunc        (GIObjectInfo *info,
						  gint          n);
GIVFuncInfo *     g_object_info_find_vfunc       (GIObjectInfo *info,
                                                  const gchar  *name);
gint              g_object_info_get_n_constants  (GIObjectInfo *info);
GIConstantInfo *  g_object_info_get_constant     (GIObjectInfo *info,
						  gint          n);
GIStructInfo *    g_object_info_get_class_struct (GIObjectInfo *info);


G_END_DECLS


#endif  /* __GIOBJECTINFO_H__ */

