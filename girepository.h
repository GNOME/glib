/* GObject introspection: Repository
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

#ifndef __G_IREPOSITORY_H__
#define __G_IREPOSITORY_H__

#define __GIREPOSITORY_H_INSIDE__

#include <glib-object.h>
#include <gmodule.h>
#include <giarginfo.h>
#include <gibaseinfo.h>
#include <gicallableinfo.h>
#include <gienuminfo.h>
#include <gierrordomaininfo.h>
#include <gifieldinfo.h>
#include <gifunctioninfo.h>
#include <giregisteredtypeinfo.h>
#include <gitypeinfo.h>
#include <gitypelib.h>
#include <gitypes.h>

G_BEGIN_DECLS

#define G_TYPE_IREPOSITORY              (g_irepository_get_type ())
#define G_IREPOSITORY(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), G_TYPE_IREPOSITORY, GIRepository))
#define G_IREPOSITORY_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), G_TYPE_IREPOSITORY, GIRepositoryClass))
#define G_IS_IREPOSITORY(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), G_TYPE_IREPOSITORY))
#define G_IS_IREPOSITORY_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), G_TYPE_IREPOSITORY))
#define G_IREPOSITORY_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), G_TYPE_IREPOSITORY, GIRepositoryClass))

typedef struct _GIRepository         GIRepository;
typedef struct _GIRepositoryClass    GIRepositoryClass;
typedef struct _GIRepositoryPrivate  GIRepositoryPrivate;

struct _GIRepository
{
  GObject parent;

  /*< private >*/
  GIRepositoryPrivate *priv;
};

struct _GIRepositoryClass
{
  GObjectClass parent;
};

/**
 * GIRepositoryLoadFlags
 * @G_IREPOSITORY_LOAD_FLAG_LAZY: Load the types lazily.
 *
 * Flags that controlls how a typelib is loaded by
 * GIRepositry, used by g_irepository_load_typelib().
 */
typedef enum
{
  G_IREPOSITORY_LOAD_FLAG_LAZY = 1 << 0
} GIRepositoryLoadFlags;

/* Repository */

GType         g_irepository_get_type      (void) G_GNUC_CONST;
GIRepository *g_irepository_get_default   (void);
void          g_irepository_prepend_search_path (const char *directory);
GSList *      g_irepository_get_search_path     (void);
const char *  g_irepository_load_typelib  (GIRepository *repository,
					   GTypelib     *typelib,
					   GIRepositoryLoadFlags flags,
					   GError      **error);
gboolean      g_irepository_is_registered (GIRepository *repository,
					   const gchar  *namespace_,
					   const gchar  *version);
GIBaseInfo *  g_irepository_find_by_name  (GIRepository *repository,
					   const gchar  *namespace_,
					   const gchar  *name);
GTypelib *    g_irepository_require       (GIRepository *repository,
					   const gchar  *namespace_,
					   const gchar  *version,
					   GIRepositoryLoadFlags flags,
					   GError      **error);
gchar      ** g_irepository_get_dependencies (GIRepository *repository,
					      const gchar  *namespace_);
gchar      ** g_irepository_get_loaded_namespaces (GIRepository *repository);
GIBaseInfo *  g_irepository_find_by_gtype (GIRepository *repository,
					   GType         gtype);
gint          g_irepository_get_n_infos   (GIRepository *repository,
					   const gchar  *namespace_);
GIBaseInfo *  g_irepository_get_info      (GIRepository *repository,
					   const gchar  *namespace_,
					   gint          index);
const gchar * g_irepository_get_typelib_path   (GIRepository *repository,
						const gchar  *namespace_);
const gchar * g_irepository_get_shared_library (GIRepository *repository,
						const gchar  *namespace_);
const gchar * g_irepository_get_c_prefix (GIRepository *repository,
                                          const gchar  *namespace_);
const gchar * g_irepository_get_version (GIRepository *repository,
					 const gchar  *namespace_);

GOptionGroup * g_irepository_get_option_group (void);

gboolean       g_irepository_dump  (const char *arg, GError **error);

/**
 * GIRepositoryError:
 * @G_IREPOSITORY_ERROR_TYPELIB_NOT_FOUND: the typelib could not be found.
 * @G_IREPOSITORY_ERROR_NAMESPACE_MISMATCH: the namespace does not match the
 * requested namespace.
 * @G_IREPOSITORY_ERROR_NAMESPACE_VERSION_CONFLICT: the version of the
 * typelib does not match the requested version.
 * @G_IREPOSITORY_ERROR_LIBRARY_NOT_FOUND: the library used by the typelib
 * could not be found.
 */
typedef enum
{
  G_IREPOSITORY_ERROR_TYPELIB_NOT_FOUND,
  G_IREPOSITORY_ERROR_NAMESPACE_MISMATCH,
  G_IREPOSITORY_ERROR_NAMESPACE_VERSION_CONFLICT,
  G_IREPOSITORY_ERROR_LIBRARY_NOT_FOUND
} GIRepositoryError;

#define G_IREPOSITORY_ERROR (g_irepository_error_quark ())

GQuark g_irepository_error_quark (void);


/* Global utility functions */

void gi_cclosure_marshal_generic (GClosure       *closure,
                                  GValue         *return_gvalue,
                                  guint           n_param_values,
                                  const GValue   *param_values,
                                  gpointer        invocation_hint,
                                  gpointer        marshal_data);

/* GIUnionInfo */

#define GI_IS_UNION_INFO(info) \
    (g_base_info_get_type((GIBaseInfo*)info) ==  GI_INFO_TYPE_UNION)

gint                   g_union_info_get_n_fields  (GIUnionInfo *info);
GIFieldInfo *          g_union_info_get_field     (GIUnionInfo *info,
					           gint         n);
gint                   g_union_info_get_n_methods (GIUnionInfo *info);
GIFunctionInfo *       g_union_info_get_method    (GIUnionInfo *info,
						   gint         n);
gboolean               g_union_info_is_discriminated (GIUnionInfo *info);
gint                   g_union_info_get_discriminator_offset (GIUnionInfo *info);
GITypeInfo *           g_union_info_get_discriminator_type (GIUnionInfo *info);
GIConstantInfo *       g_union_info_get_discriminator      (GIUnionInfo *info,
					                    gint         n);
GIFunctionInfo *       g_union_info_find_method    (GIUnionInfo *info,
                                                    const gchar *name);
gsize                  g_union_info_get_size       (GIUnionInfo *info);
gsize                  g_union_info_get_alignment  (GIUnionInfo *info);


/* GIStructInfo */

#define GI_IS_STRUCT_INFO(info) \
    (g_base_info_get_type((GIBaseInfo*)info) ==  GI_INFO_TYPE_STRUCT)

gint                   g_struct_info_get_n_fields  (GIStructInfo *info);
GIFieldInfo *          g_struct_info_get_field     (GIStructInfo *info,
						    gint         n);
gint                   g_struct_info_get_n_methods (GIStructInfo *info);
GIFunctionInfo *       g_struct_info_get_method    (GIStructInfo *info,
						    gint         n);
GIFunctionInfo *       g_struct_info_find_method   (GIStructInfo *info,
						    const gchar *name);
gsize                  g_struct_info_get_size      (GIStructInfo *info);
gsize                  g_struct_info_get_alignment (GIStructInfo *info);
gboolean               g_struct_info_is_gtype_struct (GIStructInfo *info);
gboolean               g_struct_info_is_foreign    (GIStructInfo *info);

/* GIObjectInfo */

#define GI_IS_OBJECT_INFO(info) \
    (g_base_info_get_type((GIBaseInfo*)info) ==  GI_INFO_TYPE_OBJECT)

const gchar *          g_object_info_get_type_name	    (GIObjectInfo    *info);
const gchar *          g_object_info_get_type_init	    (GIObjectInfo    *info);
gboolean               g_object_info_get_abstract           (GIObjectInfo    *info);
GIObjectInfo *         g_object_info_get_parent             (GIObjectInfo    *info);
gint                   g_object_info_get_n_interfaces       (GIObjectInfo    *info);
GIInterfaceInfo *      g_object_info_get_interface          (GIObjectInfo    *info,
							     gint            n);
gint                   g_object_info_get_n_fields           (GIObjectInfo    *info);
GIFieldInfo *          g_object_info_get_field              (GIObjectInfo    *info,
							     gint            n);
gint                   g_object_info_get_n_properties       (GIObjectInfo    *info);
GIPropertyInfo *       g_object_info_get_property           (GIObjectInfo    *info,
							     gint            n);
gint                   g_object_info_get_n_methods          (GIObjectInfo    *info);
GIFunctionInfo *       g_object_info_get_method             (GIObjectInfo    *info,
							     gint            n);
GIFunctionInfo *       g_object_info_find_method            (GIObjectInfo *info,
							     const gchar *name);
gint                   g_object_info_get_n_signals          (GIObjectInfo    *info);
GISignalInfo *         g_object_info_get_signal             (GIObjectInfo    *info,
							     gint            n);
gint                   g_object_info_get_n_vfuncs           (GIObjectInfo    *info);
GIVFuncInfo *          g_object_info_get_vfunc              (GIObjectInfo    *info,
							     gint            n);
GIVFuncInfo *          g_object_info_find_vfunc             (GIObjectInfo *info,
                                                             const gchar *name);
gint                   g_object_info_get_n_constants        (GIObjectInfo    *info);
GIConstantInfo *       g_object_info_get_constant           (GIObjectInfo    *info,
							     gint            n);
GIStructInfo *         g_object_info_get_class_struct       (GIObjectInfo    *info);


/* GIInterfaceInfo */

#define GI_IS_INTERFACE_INFO(info) \
    (g_base_info_get_type((GIBaseInfo*)info) ==  GI_INFO_TYPE_INTERFACE)

gint                   g_interface_info_get_n_prerequisites (GIInterfaceInfo *info);
GIBaseInfo *           g_interface_info_get_prerequisite    (GIInterfaceInfo *info,
							     gint        n);
gint                   g_interface_info_get_n_properties    (GIInterfaceInfo *info);
GIPropertyInfo *       g_interface_info_get_property        (GIInterfaceInfo *info,
							     gint        n);
gint                   g_interface_info_get_n_methods       (GIInterfaceInfo *info);
GIFunctionInfo *       g_interface_info_get_method          (GIInterfaceInfo *info,
							     gint        n);
GIFunctionInfo *       g_interface_info_find_method         (GIInterfaceInfo *info,
						             const gchar *name);
gint                   g_interface_info_get_n_signals       (GIInterfaceInfo *info);
GISignalInfo *         g_interface_info_get_signal          (GIInterfaceInfo *info,
							     gint        n);
gint                   g_interface_info_get_n_vfuncs        (GIInterfaceInfo *info);
GIVFuncInfo *          g_interface_info_get_vfunc           (GIInterfaceInfo *info,
							     gint        n);
GIVFuncInfo *          g_interface_info_find_vfunc          (GIInterfaceInfo *info,
                                                             const gchar *name);
gint                   g_interface_info_get_n_constants     (GIInterfaceInfo *info);
GIConstantInfo *       g_interface_info_get_constant        (GIInterfaceInfo *info,
							     gint        n);

GIStructInfo *         g_interface_info_get_iface_struct    (GIInterfaceInfo *info);


/* GIPropertyInfo  */

#define GI_IS_PROPERTY_INFO(info) \
    (g_base_info_get_type((GIBaseInfo*)info) ==  GI_INFO_TYPE_PROPERTY)

GParamFlags             g_property_info_get_flags                (GIPropertyInfo         *info);
GITypeInfo *            g_property_info_get_type                 (GIPropertyInfo         *info);


/* GISignalInfo */

#define GI_IS_SIGNAL_INFO(info) \
    (g_base_info_get_type((GIBaseInfo*)info) ==  GI_INFO_TYPE_SIGNAL)

GSignalFlags            g_signal_info_get_flags                  (GISignalInfo           *info);
GIVFuncInfo *           g_signal_info_get_class_closure          (GISignalInfo           *info);
gboolean                g_signal_info_true_stops_emit            (GISignalInfo           *info);


/* GIVFuncInfo */

#define GI_IS_VFUNC_INFO(info) \
    (g_base_info_get_type((GIBaseInfo*)info) ==  GI_INFO_TYPE_VFUNC)

GIVFuncInfoFlags        g_vfunc_info_get_flags                   (GIVFuncInfo            *info);
gint                    g_vfunc_info_get_offset                  (GIVFuncInfo            *info);
GISignalInfo *          g_vfunc_info_get_signal                  (GIVFuncInfo            *info);
GIFunctionInfo *        g_vfunc_info_get_invoker                 (GIVFuncInfo            *info);


/* GIConstantInfo */

#define GI_IS_CONSTANT_INFO(info) \
    (g_base_info_get_type((GIBaseInfo*)info) ==  GI_INFO_TYPE_CONSTANT)

GITypeInfo *            g_constant_info_get_type                 (GIConstantInfo         *info);
gint                    g_constant_info_get_value                (GIConstantInfo         *info,
								  GArgument             *value);


G_END_DECLS


#endif  /* __G_IREPOSITORY_H__ */

