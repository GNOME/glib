/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 * GObject introspection: types
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

#pragma once

// #if !defined (__GITYPELIB_H_INSIDE__) && !defined (GI_COMPILATION)
// #error "Only <gitypelib.h> can be included directly."
// #endif

#include <glib.h>
#include <glib-object.h>

#include <girepository/gi-visibility.h>

G_BEGIN_DECLS

/* Documented in gibaseinfo.c */
typedef struct _GIBaseInfo GIBaseInfo;
typedef struct _GIBaseInfoClass GIBaseInfoClass;

/* Documented in gicallableinfo.c */
typedef struct _GICallableInfo GICallableInfo;
GI_AVAILABLE_IN_ALL GType gi_callable_info_get_type (void);

/* Documented in gifunctioninfo.c */
typedef struct _GIFunctionInfo GIFunctionInfo;
GI_AVAILABLE_IN_ALL GType gi_function_info_get_type (void);

/* Documented in gicallbackinfo.c */
typedef struct _GICallbackInfo GICallbackInfo;
GI_AVAILABLE_IN_ALL GType gi_callback_info_get_type (void);

/* Documented in giregisteredtypeinfo.c */
typedef struct _GIRegisteredTypeInfo GIRegisteredTypeInfo;
GI_AVAILABLE_IN_ALL GType gi_registered_type_info_get_type (void);

/* Documented in gistructinfo.c */
typedef struct _GIStructInfo GIStructInfo;
GI_AVAILABLE_IN_ALL GType gi_struct_info_get_type (void);

/* Documented in giunioninfo.c */
typedef struct _GIUnionInfo GIUnionInfo;
GI_AVAILABLE_IN_ALL GType gi_union_info_get_type (void);

/* Documented in gienuminfo.c */
typedef struct _GIEnumInfo GIEnumInfo;
GI_AVAILABLE_IN_ALL GType gi_enum_info_get_type (void);

/* Documented in giobjectinfo.c */
typedef struct _GIObjectInfo GIObjectInfo;
GI_AVAILABLE_IN_ALL GType gi_object_info_get_type (void);

/* Documented in giinterfaceinfo.c */
typedef struct _GIInterfaceInfo GIInterfaceInfo;
GI_AVAILABLE_IN_ALL GType gi_interface_info_get_type (void);

/* Documented in giconstantinfo.c */
typedef struct _GIConstantInfo GIConstantInfo;
GI_AVAILABLE_IN_ALL GType gi_constant_info_get_type (void);

/* Documented in givalueinfo.c */
typedef struct _GIValueInfo GIValueInfo;
GI_AVAILABLE_IN_ALL GType gi_value_info_get_type (void);

/* Documented in gisignalinfo.c */
typedef struct _GISignalInfo GISignalInfo;
GI_AVAILABLE_IN_ALL GType gi_signal_info_get_type (void);

/* Documented in givfuncinfo.c */
typedef struct _GIVFuncInfo GIVFuncInfo;
GI_AVAILABLE_IN_ALL GType gi_vfunc_info_get_type (void);

/* Documented in gipropertyinfo.c */
typedef struct _GIPropertyInfo GIPropertyInfo;
GI_AVAILABLE_IN_ALL GType gi_property_info_get_type (void);

/* Documented in gifieldinfo.c */
typedef struct _GIFieldInfo GIFieldInfo;
GI_AVAILABLE_IN_ALL GType gi_field_info_get_type (void);

/* Documented in giarginfo.c */
typedef struct _GIArgInfo GIArgInfo;
GI_AVAILABLE_IN_ALL GType gi_arg_info_get_type (void);

/* Documented in gitypeinfo.c */
typedef struct _GITypeInfo GITypeInfo;
GI_AVAILABLE_IN_ALL GType gi_type_info_get_type (void);

/* Documented in giunresolvedinfo.c */
typedef struct _GIUnresolvedInfo GIUnresolvedInfo;
GI_AVAILABLE_IN_ALL GType gi_unresolved_info_get_type (void);

union _GIArgument
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
};

/**
 * GIArgument:
 * @v_boolean: boolean value
 * @v_int8: 8-bit signed integer value
 * @v_uint8: 8-bit unsigned integer value
 * @v_int16: 16-bit signed integer value
 * @v_uint16: 16-bit unsigned integer value
 * @v_int32: 32-bit signed integer value
 * @v_uint32: 32-bit unsigned integer value
 * @v_int64: 64-bit signed integer value
 * @v_uint64: 64-bit unsigned integer value
 * @v_float: single float value
 * @v_double: double float value
 * @v_short: signed short integer value
 * @v_ushort: unsigned short integer value
 * @v_int: signed integer value
 * @v_uint: unsigned integer value
 * @v_long: signed long integer value
 * @v_ulong: unsigned long integer value
 * @v_ssize: sized `size_t` value
 * @v_size: unsigned `size_t` value
 * @v_string: nul-terminated string value
 * @v_pointer: arbitrary pointer value
 *
 * Stores an argument of varying type.
 *
 * Since: 2.80
 */
typedef union _GIArgument GIArgument;

/**
 * GIInfoType:
 * @GI_INFO_TYPE_INVALID: invalid type
 * @GI_INFO_TYPE_FUNCTION: function, see [class@GIRepository.FunctionInfo]
 * @GI_INFO_TYPE_CALLBACK: callback, see [class@GIRepository.FunctionInfo]
 * @GI_INFO_TYPE_STRUCT: struct, see [class@GIRepository.StructInfo]
 * @GI_INFO_TYPE_BOXED: boxed, see [class@GIRepository.StructInfo] or
 *   [class@GIRepository.UnionInfo]
 * @GI_INFO_TYPE_ENUM: enum, see [class@GIRepository.EnumInfo]
 * @GI_INFO_TYPE_FLAGS: flags, see [class@GIRepository.EnumInfo]
 * @GI_INFO_TYPE_OBJECT: object, see [class@GIRepository.ObjectInfo]
 * @GI_INFO_TYPE_INTERFACE: interface, see [class@GIRepository.InterfaceInfo]
 * @GI_INFO_TYPE_CONSTANT: constant, see [class@GIRepository.ConstantInfo]
 * @GI_INFO_TYPE_INVALID_0: deleted, used to be `GI_INFO_TYPE_ERROR_DOMAIN`.
 * @GI_INFO_TYPE_UNION: union, see [class@GIRepository.UnionInfo]
 * @GI_INFO_TYPE_VALUE: enum value, see [class@GIRepository.ValueInfo]
 * @GI_INFO_TYPE_SIGNAL: signal, see [class@GIRepository.SignalInfo]
 * @GI_INFO_TYPE_VFUNC: virtual function, see [class@GIRepository.VFuncInfo]
 * @GI_INFO_TYPE_PROPERTY: [class@GObject.Object] property, see
 *   [class@GIRepository.PropertyInfo]
 * @GI_INFO_TYPE_FIELD: struct or union field, see
 *   [class@GIRepository.FieldInfo]
 * @GI_INFO_TYPE_ARG: argument of a function or callback, see
 *   [class@GIRepository.ArgInfo]
 * @GI_INFO_TYPE_TYPE: type information, see [class@GIRepository.TypeInfo]
 * @GI_INFO_TYPE_UNRESOLVED: unresolved type, a type which is not present in
 *   the typelib, or any of its dependencies, see
 *   [class@GIRepository.UnresolvedInfo]
 * @GI_INFO_TYPE_CALLABLE: an abstract type representing any callable (function,
 *   callback, vfunc), see [class@GIRepository.CallableInfo]
 * @GI_INFO_TYPE_REGISTERED_TYPE: an abstract type representing any registered
 *   type (enum, interface, object, struct, union), see
 *   [class@GIRepository.RegisteredTypeInfo]
 *
 * The type of a [class@GIRepository.BaseInfo] struct.
 *
 * See [const@GIRepository.INFO_TYPE_N_TYPES] for the total number of elements
 * in this enum.
 *
 * Since: 2.80
 */
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
  GI_INFO_TYPE_INVALID_0,    /* 10 */
  GI_INFO_TYPE_UNION,
  GI_INFO_TYPE_VALUE,
  GI_INFO_TYPE_SIGNAL,
  GI_INFO_TYPE_VFUNC,
  GI_INFO_TYPE_PROPERTY,     /* 15 */
  GI_INFO_TYPE_FIELD,
  GI_INFO_TYPE_ARG,
  GI_INFO_TYPE_TYPE,
  GI_INFO_TYPE_UNRESOLVED,
  GI_INFO_TYPE_CALLABLE,     /* 20 */
  GI_INFO_TYPE_REGISTERED_TYPE,
  /* keep GI_INFO_TYPE_N_TYPES in sync with this */
} GIInfoType;

/**
 * GI_INFO_TYPE_N_TYPES:
 *
 * Number of entries in [enum@GIRepository.InfoType].
 *
 * Since: 2.80
 */
#define GI_INFO_TYPE_N_TYPES (GI_INFO_TYPE_REGISTERED_TYPE + 1)

/**
 * GITransfer:
 * @GI_TRANSFER_NOTHING: Transfer nothing from the callee (function or the type
 *   instance the property belongs to) to the caller. The callee retains the
 *   ownership of the transfer and the caller doesn’t need to do anything to
 *   free up the resources of this transfer.
 * @GI_TRANSFER_CONTAINER: Transfer the container (list, array, hash table) from
 *   the callee to the caller. The callee retains the ownership of the
 *   individual items in the container and the caller has to free up the
 *   container resources ([func@GLib.List.free],
 *   [func@GLib.HashTable.destroy], etc) of this transfer.
 * @GI_TRANSFER_EVERYTHING: Transfer everything, e.g. the container and its
 *   contents from the callee to the caller. This is the case when the callee
 *   creates a copy of all the data it returns. The caller is responsible for
 *   cleaning up the container and item resources of this transfer.
 *
 * `GITransfer` specifies who’s responsible for freeing the resources after an
 * ownership transfer is complete.
 *
 * The transfer is the exchange of data between two parts, from the callee to
 * the caller.
 *
 * The callee is either a function/method/signal or an object/interface where a
 * property is defined. The caller is the side accessing a property or calling a
 * function.
 *
 * In the case of a containing type such as a list, an array or a hash table the
 * container itself is specified differently from the items within the
 * container. Each container is freed differently, check the documentation for
 * the types themselves for information on how to free them.
 *
 * Since: 2.80
 */
typedef enum {
  GI_TRANSFER_NOTHING,
  GI_TRANSFER_CONTAINER,
  GI_TRANSFER_EVERYTHING
} GITransfer;

/**
 * GIDirection:
 * @GI_DIRECTION_IN: ‘in’ argument.
 * @GI_DIRECTION_OUT: ‘out’ argument.
 * @GI_DIRECTION_INOUT: ‘in and out’ argument.
 *
 * The direction of a [class@GIRepository.ArgInfo].
 *
 * Since: 2.80
 */
typedef enum  {
  GI_DIRECTION_IN,
  GI_DIRECTION_OUT,
  GI_DIRECTION_INOUT
} GIDirection;

/**
 * GIScopeType:
 * @GI_SCOPE_TYPE_INVALID: The argument is not of callback type.
 * @GI_SCOPE_TYPE_CALL: The callback and associated `user_data` is only
 *   used during the call to this function.
 * @GI_SCOPE_TYPE_ASYNC: The callback and associated `user_data` is
 *   only used until the callback is invoked, and the callback.
 *   is invoked always exactly once.
 * @GI_SCOPE_TYPE_NOTIFIED: The callback and associated
 *   `user_data` is used until the caller is notified via the
 *   [type@GLib.DestroyNotify].
 * @GI_SCOPE_TYPE_FOREVER: The callback and associated `user_data` is
 *   used until the process terminates
 *
 * Scope type of a [class@GIRepository.ArgInfo] representing callback,
 * determines how the callback is invoked and is used to decided when the invoke
 * structs can be freed.
 *
 * Since: 2.80
 */
typedef enum {
  GI_SCOPE_TYPE_INVALID,
  GI_SCOPE_TYPE_CALL,
  GI_SCOPE_TYPE_ASYNC,
  GI_SCOPE_TYPE_NOTIFIED,
  GI_SCOPE_TYPE_FOREVER
} GIScopeType;

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
 * @GI_TYPE_TAG_FLOAT: float
 * @GI_TYPE_TAG_DOUBLE: double floating point
 * @GI_TYPE_TAG_GTYPE: a [type@GObject.Type]
 * @GI_TYPE_TAG_UTF8: a UTF-8 encoded string
 * @GI_TYPE_TAG_FILENAME: a filename, encoded in the same encoding
 *   as the native filesystem is using.
 * @GI_TYPE_TAG_ARRAY: an array
 * @GI_TYPE_TAG_INTERFACE: an extended interface object
 * @GI_TYPE_TAG_GLIST: a [type@GLib.List]
 * @GI_TYPE_TAG_GSLIST: a [type@GLib.SList]
 * @GI_TYPE_TAG_GHASH: a [type@GLib.HashTable]
 * @GI_TYPE_TAG_ERROR: a [type@GLib.Error]
 * @GI_TYPE_TAG_UNICHAR: Unicode character
 *
 * The type tag of a [class@GIRepository.TypeInfo].
 *
 * Since: 2.80
 */
typedef enum {
  /* Basic types */
  GI_TYPE_TAG_VOID      =  0,
  GI_TYPE_TAG_BOOLEAN   =  1,
  GI_TYPE_TAG_INT8      =  2,  /* Start of GI_TYPE_TAG_IS_NUMERIC types */
  GI_TYPE_TAG_UINT8     =  3,
  GI_TYPE_TAG_INT16     =  4,
  GI_TYPE_TAG_UINT16    =  5,
  GI_TYPE_TAG_INT32     =  6,
  GI_TYPE_TAG_UINT32    =  7,
  GI_TYPE_TAG_INT64     =  8,
  GI_TYPE_TAG_UINT64    =  9,
  GI_TYPE_TAG_FLOAT     = 10,
  GI_TYPE_TAG_DOUBLE    = 11,  /* End of numeric types */
  GI_TYPE_TAG_GTYPE     = 12,
  GI_TYPE_TAG_UTF8      = 13,
  GI_TYPE_TAG_FILENAME  = 14,
  /* Non-basic types; compare with GI_TYPE_TAG_IS_BASIC */
  GI_TYPE_TAG_ARRAY     = 15,  /* container (see GI_TYPE_TAG_IS_CONTAINER) */
  GI_TYPE_TAG_INTERFACE = 16,
  GI_TYPE_TAG_GLIST     = 17,  /* container */
  GI_TYPE_TAG_GSLIST    = 18,  /* container */
  GI_TYPE_TAG_GHASH     = 19,  /* container */
  GI_TYPE_TAG_ERROR     = 20,
  /* Another basic type */
  GI_TYPE_TAG_UNICHAR   = 21
  /* Note - there is currently only room for 32 tags */
} GITypeTag;

/**
 * GI_TYPE_TAG_N_TYPES:
 *
 * Number of entries in [enum@GIRepository.TypeTag].
 *
 * Since: 2.80
 */
#define GI_TYPE_TAG_N_TYPES (GI_TYPE_TAG_UNICHAR+1)

/**
 * GIArrayType:
 * @GI_ARRAY_TYPE_C: a C array, `char[]` for instance
 * @GI_ARRAY_TYPE_ARRAY: a [type@GLib.Array] array
 * @GI_ARRAY_TYPE_PTR_ARRAY: a [type@GLib.PtrArray] array
 * @GI_ARRAY_TYPE_BYTE_ARRAY: a [type@GLib.ByteArray] array
 *
 * The type of array in a [class@GIRepository.TypeInfo].
 *
 * Since: 2.80
 */
typedef enum {
  GI_ARRAY_TYPE_C,
  GI_ARRAY_TYPE_ARRAY,
  GI_ARRAY_TYPE_PTR_ARRAY,
  GI_ARRAY_TYPE_BYTE_ARRAY
} GIArrayType;

/**
 * GIFieldInfoFlags:
 * @GI_FIELD_IS_READABLE: field is readable.
 * @GI_FIELD_IS_WRITABLE: field is writable.
 *
 * Flags for a [class@GIRepository.FieldInfo].
 *
 * Since: 2.80
 */

typedef enum
{
  GI_FIELD_IS_READABLE = 1 << 0,
  GI_FIELD_IS_WRITABLE = 1 << 1
} GIFieldInfoFlags;

/**
 * GIVFuncInfoFlags:
 * @GI_VFUNC_MUST_CHAIN_UP: chains up to the parent type
 * @GI_VFUNC_MUST_OVERRIDE: overrides
 * @GI_VFUNC_MUST_NOT_OVERRIDE: does not override
 * @GI_VFUNC_THROWS: includes a [type@GLib.Error]
 *
 * Flags of a [class@GIRepository.VFuncInfo] struct.
 *
 * Since: 2.80
 */
typedef enum
{
  GI_VFUNC_MUST_CHAIN_UP     = 1 << 0,
  GI_VFUNC_MUST_OVERRIDE     = 1 << 1,
  GI_VFUNC_MUST_NOT_OVERRIDE = 1 << 2,
  GI_VFUNC_THROWS =            1 << 3
} GIVFuncInfoFlags;

/**
 * GIFunctionInfoFlags:
 * @GI_FUNCTION_IS_METHOD: is a method.
 * @GI_FUNCTION_IS_CONSTRUCTOR: is a constructor.
 * @GI_FUNCTION_IS_GETTER: is a getter of a [class@GIRepository.PropertyInfo].
 * @GI_FUNCTION_IS_SETTER: is a setter of a [class@GIRepository.PropertyInfo].
 * @GI_FUNCTION_WRAPS_VFUNC: represents a virtual function.
 * @GI_FUNCTION_THROWS: the function may throw an error.
 *
 * Flags for a [class@GIRepository.FunctionInfo] struct.
 *
 * Since: 2.80
 */
typedef enum
{
  GI_FUNCTION_IS_METHOD      = 1 << 0,
  GI_FUNCTION_IS_CONSTRUCTOR = 1 << 1,
  GI_FUNCTION_IS_GETTER      = 1 << 2,
  GI_FUNCTION_IS_SETTER      = 1 << 3,
  GI_FUNCTION_WRAPS_VFUNC    = 1 << 4,
  GI_FUNCTION_THROWS         = 1 << 5
} GIFunctionInfoFlags;

G_END_DECLS
