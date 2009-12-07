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

#include <glib-object.h>
#include <gmodule.h>

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

typedef struct _GIBaseInfoStub       GIBaseInfo;

struct _GIBaseInfoStub {
  gint32 dummy1;
  gint32 dummy2;
  gpointer dummy3;
  gpointer dummy4;
  gpointer dummy5;
  guint32  dummy6;
  guint32  dummy7;
  gpointer padding[4];
};

typedef GIBaseInfo GICallableInfo;
typedef GIBaseInfo GIFunctionInfo;
typedef GIBaseInfo GICallbackInfo;
typedef GIBaseInfo GIRegisteredTypeInfo;
typedef GIBaseInfo GIStructInfo;
typedef GIBaseInfo GIUnionInfo;
typedef GIBaseInfo GIEnumInfo;
typedef GIBaseInfo GIObjectInfo;
typedef GIBaseInfo GIInterfaceInfo;
typedef GIBaseInfo GIConstantInfo;
typedef GIBaseInfo GIValueInfo;
typedef GIBaseInfo GISignalInfo;
typedef GIBaseInfo GIVFuncInfo;
typedef GIBaseInfo GIPropertyInfo;
typedef GIBaseInfo GIFieldInfo;
typedef GIBaseInfo GIArgInfo;
typedef GIBaseInfo GITypeInfo;
typedef GIBaseInfo GIErrorDomainInfo;

typedef struct _GIUnresolvedInfo     GIUnresolvedInfo;
typedef struct _GTypelib            GTypelib;

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

/* Typelib */

GTypelib *   g_typelib_new_from_memory       (guchar       *memory,
                                                gsize         len);
GTypelib *   g_typelib_new_from_const_memory (const guchar *memory,
                                                gsize         len);
GTypelib *   g_typelib_new_from_mapped_file  (GMappedFile  *mfile);
void          g_typelib_free                  (GTypelib    *typelib);

gboolean      g_typelib_symbol                (GTypelib    *typelib,
                                               const gchar *symbol_name,
                                               gpointer    *symbol);
const gchar * g_typelib_get_namespace         (GTypelib    *typelib);

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

/* Types of objects registered in the repository */

typedef enum 
{
  GI_INFO_TYPE_INVALID,
  GI_INFO_TYPE_FUNCTION,
  GI_INFO_TYPE_CALLBACK,
  GI_INFO_TYPE_STRUCT,
  GI_INFO_TYPE_BOXED,
  GI_INFO_TYPE_ENUM,         /*  5 */
  GI_INFO_TYPE_FLAGS,
  GI_INFO_TYPE_OBJECT,
  GI_INFO_TYPE_INTERFACE,
  GI_INFO_TYPE_CONSTANT,
  GI_INFO_TYPE_ERROR_DOMAIN, /* 10 */
  GI_INFO_TYPE_UNION,
  GI_INFO_TYPE_VALUE,
  GI_INFO_TYPE_SIGNAL,
  GI_INFO_TYPE_VFUNC,
  GI_INFO_TYPE_PROPERTY,     /* 15 */
  GI_INFO_TYPE_FIELD,
  GI_INFO_TYPE_ARG,
  GI_INFO_TYPE_TYPE,
  GI_INFO_TYPE_UNRESOLVED
} GIInfoType;


/* GIBaseInfo */

typedef struct {
  gpointer data;
  gpointer data2;
  gpointer data3;
  gpointer data4;
} GIAttributeIter;

GIBaseInfo *           g_base_info_ref              (GIBaseInfo   *info);
void                   g_base_info_unref            (GIBaseInfo   *info);
GIInfoType             g_base_info_get_type         (GIBaseInfo   *info);
const gchar *          g_base_info_get_name         (GIBaseInfo   *info);
const gchar *          g_base_info_get_namespace    (GIBaseInfo   *info);
gboolean               g_base_info_is_deprecated    (GIBaseInfo   *info);
const gchar *          g_base_info_get_attribute    (GIBaseInfo   *info,
                                                     const gchar  *name);
gboolean               g_base_info_iterate_attributes (GIBaseInfo      *info,
                                                       GIAttributeIter *iterator,
                                                       char           **name,
                                                       char          **value);
GIBaseInfo *           g_base_info_get_container    (GIBaseInfo   *info);
GTypelib *             g_base_info_get_typelib      (GIBaseInfo   *info);
gboolean               g_base_info_equal            (GIBaseInfo   *info1,
                                                     GIBaseInfo   *info2);

GIBaseInfo *           g_info_new                   (GIInfoType    type,
						     GIBaseInfo   *container,
						     GTypelib     *typelib, 
						     guint32       offset);


/* GIFunctionInfo */

typedef enum
{
  GI_FUNCTION_IS_METHOD      = 1 << 0,
  GI_FUNCTION_IS_CONSTRUCTOR = 1 << 1,
  GI_FUNCTION_IS_GETTER      = 1 << 2,
  GI_FUNCTION_IS_SETTER      = 1 << 3,
  GI_FUNCTION_WRAPS_VFUNC    = 1 << 4,
  GI_FUNCTION_THROWS         = 1 << 5
} GIFunctionInfoFlags;

const gchar *           g_function_info_get_symbol     (GIFunctionInfo *info);
GIFunctionInfoFlags     g_function_info_get_flags      (GIFunctionInfo *info);
GIPropertyInfo *        g_function_info_get_property   (GIFunctionInfo *info);
GIVFuncInfo *           g_function_info_get_vfunc      (GIFunctionInfo *info);

typedef union 
{
  gboolean v_boolean;
  gint8    v_int8;
  guint8   v_uint8;
  gint16   v_int16;
  guint16  v_uint16;
  gint32   v_int32;
  guint32  v_uint32;
  gint64   v_int64;
  guint64  v_uint64;
  gfloat   v_float;
  gdouble  v_double;
  gshort   v_short;
  gushort  v_ushort;
  gint     v_int;
  guint    v_uint;
  glong    v_long;
  gulong   v_ulong;
  gssize   v_ssize;
  gsize    v_size;
  gchar *  v_string;
  gpointer v_pointer;
} GArgument;

#define G_INVOKE_ERROR (g_invoke_error_quark ())
GQuark g_invoke_error_quark (void);

typedef enum
{
  G_INVOKE_ERROR_FAILED,
  G_INVOKE_ERROR_SYMBOL_NOT_FOUND,
  G_INVOKE_ERROR_ARGUMENT_MISMATCH
} GInvokeError;

gboolean              g_function_info_invoke         (GIFunctionInfo *info, 
						      const GArgument  *in_args,
						      int               n_in_args,
						      const GArgument  *out_args,
						      int               n_out_args,
						      GArgument        *return_value,
						      GError          **error);


/* GICallableInfo */

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

typedef enum  {
  GI_DIRECTION_IN,
  GI_DIRECTION_OUT,
  GI_DIRECTION_INOUT
} GIDirection;

typedef enum {
  GI_SCOPE_TYPE_INVALID, /* The argument is not of callback type */
  GI_SCOPE_TYPE_CALL, /* The callback and associated user_data is only used during the
                         call to this function */
  GI_SCOPE_TYPE_ASYNC, /* The callback and associated user_data is 
                          only used until the callback is invoked, and the callback 
                          is invoked always exactly once. */
  GI_SCOPE_TYPE_NOTIFIED /* The callback and and associated user_data is
                            used until the caller is notfied via the destroy_notify */
} GIScopeType;

GIDirection            g_arg_info_get_direction          (GIArgInfo *info);
gboolean               g_arg_info_is_dipper              (GIArgInfo *info);
gboolean               g_arg_info_is_return_value        (GIArgInfo *info);
gboolean               g_arg_info_is_optional            (GIArgInfo *info);
gboolean               g_arg_info_may_be_null            (GIArgInfo *info);
GITransfer             g_arg_info_get_ownership_transfer (GIArgInfo *info);
GIScopeType            g_arg_info_get_scope              (GIArgInfo *info);
gint                   g_arg_info_get_closure            (GIArgInfo *info);
gint                   g_arg_info_get_destroy            (GIArgInfo *info);
GITypeInfo *           g_arg_info_get_type               (GIArgInfo *info);
void                   g_arg_info_load_type              (GIArgInfo *info,
                                                          GITypeInfo *type);


/* GITypeInfo */

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

gint                   g_type_info_get_n_error_domains (GITypeInfo *info);
GIErrorDomainInfo     *g_type_info_get_error_domain    (GITypeInfo *info,
							gint       n);

/* GIErrorDomainInfo */

const gchar *          g_error_domain_info_get_quark   (GIErrorDomainInfo *info);
GIInterfaceInfo *           g_error_domain_info_get_codes (GIErrorDomainInfo *info);


/* GIValueInfo */
 
glong                  g_value_info_get_value      (GIValueInfo *info);


/* GIFieldInfo */

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

/* GIRegisteredTypeInfo */

const gchar *          g_registered_type_info_get_type_name (GIRegisteredTypeInfo *info);
const gchar *          g_registered_type_info_get_type_init (GIRegisteredTypeInfo *info);
GType                  g_registered_type_info_get_g_type    (GIRegisteredTypeInfo *info);

/* GIEnumInfo */

gint                   g_enum_info_get_n_values             (GIEnumInfo      *info);
GIValueInfo  *         g_enum_info_get_value                (GIEnumInfo      *info,
							     gint            n);
GITypeTag              g_enum_info_get_storage_type         (GIEnumInfo      *info);

/* GIObjectInfo */

const gchar *          g_object_info_get_type_name 	    (GIObjectInfo    *info);
const gchar *          g_object_info_get_type_init 	    (GIObjectInfo    *info);
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

GParamFlags             g_property_info_get_flags                (GIPropertyInfo         *info);
GITypeInfo *            g_property_info_get_type                 (GIPropertyInfo         *info);


/* GISignalInfo */

GSignalFlags            g_signal_info_get_flags                  (GISignalInfo           *info);
GIVFuncInfo *           g_signal_info_get_class_closure          (GISignalInfo           *info);
gboolean                g_signal_info_true_stops_emit            (GISignalInfo           *info);


/* GIVFuncInfo */

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

GITypeInfo *            g_constant_info_get_type                 (GIConstantInfo         *info);
gint                    g_constant_info_get_value                (GIConstantInfo         *info,
								  GArgument             *value);


G_END_DECLS

#endif  /* __G_IREPOSITORY_H__ */

