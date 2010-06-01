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
#include <gibaseinfo.h>
#include <gifunctioninfo.h>
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

/* GICallableInfo */

#define GI_IS_CALLABLE_INFO(info) \
    ((g_base_info_get_type((GIBaseInfo*)info) == GI_INFO_TYPE_FUNCTION) || \
     (g_base_info_get_type((GIBaseInfo*)info) == GI_INFO_TYPE_CALLBACK) || \
     (g_base_info_get_type((GIBaseInfo*)info) == GI_INFO_TYPE_SIGNAL) || \
     (g_base_info_get_type((GIBaseInfo*)info) == GI_INFO_TYPE_VFUNC))

/**
 * GITransfer:
 * @GI_TRANSFER_NOTHING: transfer nothing to the caller
 * @GI_TRANSFER_CONTAINER: transfer the container (eg list, array,
 * hashtable), but no the contents to the caller.
 * @GI_TRANSFER_EVERYTHING: transfer everything to the caller.
 *
 * Represent the transfer ownership information of a #GICallableInfo or
 * a #GIArgInfo.
 */
typedef enum {
  GI_TRANSFER_NOTHING,
  GI_TRANSFER_CONTAINER,
  GI_TRANSFER_EVERYTHING
} GITransfer;

GITypeInfo *           g_callable_info_get_return_type (GICallableInfo *info);
void                   g_callable_info_load_return_type (GICallableInfo *info,
                                                         GITypeInfo     *type);
GITransfer             g_callable_info_get_caller_owns (GICallableInfo *info);
gboolean               g_callable_info_may_return_null (GICallableInfo *info);
gint                   g_callable_info_get_n_args      (GICallableInfo *info);
GIArgInfo *            g_callable_info_get_arg         (GICallableInfo *info,
                                                        gint            n);
void                   g_callable_info_load_arg        (GICallableInfo *info,
                                                        gint            n,
                                                        GIArgInfo      *arg);

/* GIArgInfo */

#define GI_IS_ARG_INFO(info) \
    (g_base_info_get_type((GIBaseInfo*)info) ==  GI_INFO_TYPE_ARG)

/**
 * GIDirection:
 * @GI_DIRECTION_IN: in argument.
 * @GI_DIRECTION_OUT: out argument.
 * @GI_DIRECTION_INOUT: in and out argument.
 *
 * The direction of a #GIArgInfo.
 */
typedef enum  {
  GI_DIRECTION_IN,
  GI_DIRECTION_OUT,
  GI_DIRECTION_INOUT
} GIDirection;

/**
 * GIScopeType:
 * @GI_SCOPE_TYPE_INVALID: The argument is not of callback type.
 * @GI_SCOPE_TYPE_CALL: The callback and associated user_data is only
 * used during the call to this function.
 * @GI_SCOPE_TYPE_ASYNC: The callback and associated user_data is
 * only used until the callback is invoked, and the callback.
 * is invoked always exactly once.
 * @GI_SCOPE_TYPE_NOTIFIED: The callback and and associated
 * user_data is used until the caller is notfied via the destroy_notify.
 *
 * Scope type of a #GIArgInfo representing callback, determines how the
 * callback is invoked and is used to decided when the invoke structs
 * can be freed.
 */
typedef enum {
  GI_SCOPE_TYPE_INVALID,
  GI_SCOPE_TYPE_CALL,
  GI_SCOPE_TYPE_ASYNC,
  GI_SCOPE_TYPE_NOTIFIED
} GIScopeType;

GIDirection            g_arg_info_get_direction          (GIArgInfo *info);
gboolean               g_arg_info_is_return_value        (GIArgInfo *info);
gboolean               g_arg_info_is_optional            (GIArgInfo *info);
gboolean               g_arg_info_is_caller_allocates    (GIArgInfo *info);
gboolean               g_arg_info_may_be_null            (GIArgInfo *info);
GITransfer             g_arg_info_get_ownership_transfer (GIArgInfo *info);
GIScopeType            g_arg_info_get_scope              (GIArgInfo *info);
gint                   g_arg_info_get_closure            (GIArgInfo *info);
gint                   g_arg_info_get_destroy            (GIArgInfo *info);
GITypeInfo *           g_arg_info_get_type               (GIArgInfo *info);
void                   g_arg_info_load_type              (GIArgInfo *info,
                                                          GITypeInfo *type);

/* GITypeInfo */

#define GI_IS_TYPE_INFO(info) \
    (g_base_info_get_type((GIBaseInfo*)info) ==  GI_INFO_TYPE_TYPE)

/**
 * GITypeTag:
 * @GI_TYPE_TAG_VOID: void
 * @GI_TYPE_TAG_BOOLEAN: boolean
 * @GI_TYPE_TAG_INT8: 8-bit signed integer
 * @GI_TYPE_TAG_UINT8: 8-bit unsigned integer
 * @GI_TYPE_TAG_INT16: 16-bit signed integer
 * @GI_TYPE_TAG_UINT16: 16-bit unsigned integer
 * @GI_TYPE_TAG_INT32: 32-bit signed integer
 * @GI_TYPE_TAG_UINT32: 32-bit unsigned integer
 * @GI_TYPE_TAG_INT64: 64-bit signed integer
 * @GI_TYPE_TAG_UINT64: 64-bit unsigned integer
 * @GI_TYPE_TAG_SHORT: signed short
 * @GI_TYPE_TAG_USHORT: unsigned hosrt
 * @GI_TYPE_TAG_INT: signed integer
 * @GI_TYPE_TAG_UINT: unsigned integer
 * @GI_TYPE_TAG_LONG: signed long
 * @GI_TYPE_TAG_ULONG: unsigned long
 * @GI_TYPE_TAG_SSIZE: ssize_t
 * @GI_TYPE_TAG_SIZE: size_t
 * @GI_TYPE_TAG_FLOAT: float
 * @GI_TYPE_TAG_DOUBLE: double floating point
 * @GI_TYPE_TAG_TIME_T: time_t
 * @GI_TYPE_TAG_GTYPE: a #GType
 * @GI_TYPE_TAG_UTF8: a UTF-8 encoded string
 * @GI_TYPE_TAG_FILENAME: a filename, encoded in the same encoding
 * as the native filesystem is using.
 * @GI_TYPE_TAG_ARRAY: an array
 * @GI_TYPE_TAG_INTERFACE: an extended interface object
 * @GI_TYPE_TAG_GLIST: a #GList
 * @GI_TYPE_TAG_GSLIST: a #GSList
 * @GI_TYPE_TAG_GHASH: a #GHashTable
 * @GI_TYPE_TAG_ERROR: a #GError
 *
 * The type tag of a #GITypeInfo.
 */
typedef enum {
  /* Basic types */
  GI_TYPE_TAG_VOID      =  0,
  GI_TYPE_TAG_BOOLEAN   =  1,
  GI_TYPE_TAG_INT8      =  2,
  GI_TYPE_TAG_UINT8     =  3,
  GI_TYPE_TAG_INT16     =  4,
  GI_TYPE_TAG_UINT16    =  5,
  GI_TYPE_TAG_INT32     =  6,
  GI_TYPE_TAG_UINT32    =  7,
  GI_TYPE_TAG_INT64     =  8,
  GI_TYPE_TAG_UINT64    =  9,
  GI_TYPE_TAG_SHORT     = 10,
  GI_TYPE_TAG_USHORT    = 11,
  GI_TYPE_TAG_INT       = 12,
  GI_TYPE_TAG_UINT      = 13,
  GI_TYPE_TAG_LONG      = 14,
  GI_TYPE_TAG_ULONG     = 15,
  GI_TYPE_TAG_SSIZE     = 16,
  GI_TYPE_TAG_SIZE      = 17,
  GI_TYPE_TAG_FLOAT     = 18,
  GI_TYPE_TAG_DOUBLE    = 19,
  GI_TYPE_TAG_TIME_T    = 20,
  GI_TYPE_TAG_GTYPE     = 21,
  GI_TYPE_TAG_UTF8      = 22,
  GI_TYPE_TAG_FILENAME  = 23,
  /* Non-basic types */
  GI_TYPE_TAG_ARRAY     = 24,
  GI_TYPE_TAG_INTERFACE = 25,
  GI_TYPE_TAG_GLIST     = 26,
  GI_TYPE_TAG_GSLIST    = 27,
  GI_TYPE_TAG_GHASH     = 28,
  GI_TYPE_TAG_ERROR     = 29
  /* Note - there is only room currently for 32 tags.
   * See docs/typelib-format.txt SimpleTypeBlob definition */
} GITypeTag;

/**
 * GIArrayType:
 * @GI_ARRAY_TYPE_C: a C array, char[] for instance
 * @GI_ARRAY_TYPE_ARRAY: a @GArray array
 * @GI_ARRAY_TYPE_PTR_ARRAY: a #GPtrArray array
 * @GI_ARRAY_TYPE_BYTE_ARRAY: a #GByteArray array
 *
 * The type of array in a #GITypeInfo.
 */
typedef enum {
  GI_ARRAY_TYPE_C,
  GI_ARRAY_TYPE_ARRAY,
  GI_ARRAY_TYPE_PTR_ARRAY,
  GI_ARRAY_TYPE_BYTE_ARRAY
} GIArrayType;

#define G_TYPE_TAG_IS_BASIC(tag) (tag < GI_TYPE_TAG_ARRAY)

const gchar*           g_type_tag_to_string            (GITypeTag   type);

gboolean               g_type_info_is_pointer          (GITypeInfo *info);
GITypeTag              g_type_info_get_tag             (GITypeInfo *info);
GITypeInfo *           g_type_info_get_param_type      (GITypeInfo *info,
						        gint       n);
GIBaseInfo *           g_type_info_get_interface       (GITypeInfo *info);
gint                   g_type_info_get_array_length    (GITypeInfo *info);
gint                   g_type_info_get_array_fixed_size(GITypeInfo *info);
gboolean               g_type_info_is_zero_terminated  (GITypeInfo *info);
GIArrayType            g_type_info_get_array_type      (GITypeInfo *info);

gint                   g_type_info_get_n_error_domains (GITypeInfo *info);
GIErrorDomainInfo     *g_type_info_get_error_domain    (GITypeInfo *info,
							gint       n);

/* GIErrorDomainInfo */

#define GI_IS_ERROR_DOMAIN_INFO(info) \
    (g_base_info_get_type((GIBaseInfo*)info) ==  GI_INFO_TYPE_ERROR_DOMAIN)

const gchar *          g_error_domain_info_get_quark   (GIErrorDomainInfo *info);
GIInterfaceInfo *           g_error_domain_info_get_codes (GIErrorDomainInfo *info);


/* GIValueInfo */

#define GI_IS_VALUE_INFO(info) \
    (g_base_info_get_type((GIBaseInfo*)info) ==  GI_INFO_TYPE_VALUE)

glong                  g_value_info_get_value      (GIValueInfo *info);


/* GIFieldInfo */

#define GI_IS_FIELD_INFO(info) \
    (g_base_info_get_type((GIBaseInfo*)info) ==  GI_INFO_TYPE_FIELD)

/**
 * GIFieldInfoFlags:
 * @GI_FIELD_IS_READABLE: field is readable.
 * @GI_FIELD_IS_WRITABLE: field is writable.
 *
 * Flags for a #GIFieldInfo.
 */

typedef enum
{
  GI_FIELD_IS_READABLE = 1 << 0,
  GI_FIELD_IS_WRITABLE = 1 << 1
} GIFieldInfoFlags;

GIFieldInfoFlags       g_field_info_get_flags      (GIFieldInfo *info);
gint                   g_field_info_get_size       (GIFieldInfo *info);
gint                   g_field_info_get_offset     (GIFieldInfo *info);
GITypeInfo *           g_field_info_get_type       (GIFieldInfo *info);

gboolean g_field_info_get_field (GIFieldInfo     *field_info,
				 gpointer         mem,
				 GArgument       *value);
gboolean g_field_info_set_field (GIFieldInfo     *field_info,
				 gpointer         mem,
				 const GArgument *value);

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

/* GIRegisteredTypeInfo */

#define GI_IS_REGISTERED_TYPE_INFO(info) \
    ((g_base_info_get_type((GIBaseInfo*)info) == GI_INFO_TYPE_ENUM) || \
     (g_base_info_get_type((GIBaseInfo*)info) == GI_INFO_TYPE_INTERFACE) || \
     (g_base_info_get_type((GIBaseInfo*)info) == GI_INFO_TYPE_OBJECT) || \
     (g_base_info_get_type((GIBaseInfo*)info) == GI_INFO_TYPE_STRUCT) || \
     (g_base_info_get_type((GIBaseInfo*)info) == GI_INFO_TYPE_UNION))

const gchar *          g_registered_type_info_get_type_name (GIRegisteredTypeInfo *info);
const gchar *          g_registered_type_info_get_type_init (GIRegisteredTypeInfo *info);
GType                  g_registered_type_info_get_g_type    (GIRegisteredTypeInfo *info);

/* GIEnumInfo */

#define GI_IS_ENUM_INFO(info) \
    ((g_base_info_get_type((GIBaseInfo*)info) ==  GI_INFO_TYPE_ENUM) || \
     (g_base_info_get_type((GIBaseInfo*)info) ==  GI_INFO_TYPE_FLAGS))

gint                   g_enum_info_get_n_values             (GIEnumInfo      *info);
GIValueInfo  *         g_enum_info_get_value                (GIEnumInfo      *info,
							     gint            n);
GITypeTag              g_enum_info_get_storage_type         (GIEnumInfo      *info);

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

/**
 * GIVFuncInfoFlags:
 * @GI_VFUNC_MUST_CHAIN_UP: chains up to the parent type
 * @GI_VFUNC_MUST_OVERRIDE: overrides
 * @GI_VFUNC_MUST_NOT_OVERRIDE: does not override
 *
 * Flags of a #GIVFuncInfo struct.
 */
typedef enum
{
  GI_VFUNC_MUST_CHAIN_UP     = 1 << 0,
  GI_VFUNC_MUST_OVERRIDE     = 1 << 1,
  GI_VFUNC_MUST_NOT_OVERRIDE = 1 << 2
} GIVFuncInfoFlags;

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

