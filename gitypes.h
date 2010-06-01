/* GObject introspection: types
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

#ifndef __GITYPES_H__
#define __GITYPES_H__

#if !defined (__GIREPOSITORY_H_INSIDE__) && !defined (GI_COMPILATION)
#error "Only <girepository.h> can be included directly."
#endif

#include <gibaseinfo.h>

G_BEGIN_DECLS

typedef struct _GIBaseInfoStub GIBaseInfo;

/**
 * GICallableInfo:
 *
 * Represents a callable, either #GIFunctionInfo, #GICallbackInfo or
 * #GIVFuncInfo.
 */
typedef GIBaseInfo GICallableInfo;

/**
 * GIFunctionInfo:
 *
 * Represents a function, eg arguments and return value.
 */
typedef GIBaseInfo GIFunctionInfo;

/**
 * GICallbackInfo:
 *
 * Represents a callback, eg arguments and return value.
 */
typedef GIBaseInfo GICallbackInfo;

/**
 * GIRegisteredTypeInfo:
 *
 * Represent a registered type.
 */
typedef GIBaseInfo GIRegisteredTypeInfo;

/**
 * GIStructInfo:
 *
 * Represents a struct.
 */
typedef GIBaseInfo GIStructInfo;

/**
 * GIUnionInfo:
 *
 * Represents a union.
 */
typedef GIBaseInfo GIUnionInfo;

/**
 * GIEnumInfo:
 *
 * Represents an enum or a flag.
 */
typedef GIBaseInfo GIEnumInfo;

/**
 * GIObjectInfo:
 *
 * Represents an object.
 */
typedef GIBaseInfo GIObjectInfo;

/**
 * GIInterfaceInfo:
 *
 * Represents an interface.
 */
typedef GIBaseInfo GIInterfaceInfo;

/**
 * GIConstantInfo:
 *
 * Represents a constant.
 */
typedef GIBaseInfo GIConstantInfo;

/**
 * GIValueInfo:
 *
 * Represents a enum value of a #GIEnumInfo.
 */
typedef GIBaseInfo GIValueInfo;

/**
 * GISignalInfo:
 *
 * Represents a signal.
 */
typedef GIBaseInfo GISignalInfo;

/**
 * GIVFuncInfo
 *
 * Represents a virtual function.
 */
typedef GIBaseInfo GIVFuncInfo;

/**
 * GIPropertyInfo:
 *
 * Represents a property of a #GIObjectInfo or a #GIInterfaceInfo.
 */
typedef GIBaseInfo GIPropertyInfo;

/**
 * GIFieldInfo:
 *
 * Represents a field of a #GIStructInfo or a #GIUnionInfo.
 */
typedef GIBaseInfo GIFieldInfo;

/**
 * GIArgInfo:
 *
 * Represents an argument.
 */
typedef GIBaseInfo GIArgInfo;

/**
 * GITypeInfo:
 *
 * Represents type information, direction, transfer etc.
 */
typedef GIBaseInfo GITypeInfo;

/**
 * GIErrorDomainInfo:
 *
 * Represents a #GError error domain.
 */
typedef GIBaseInfo GIErrorDomainInfo;

/**
 * GIUnresolvedInfo:
 *
 * Represents a unresolved type in a typelib.
 */
typedef struct _GIUnresolvedInfo GIUnresolvedInfo;

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

/* Types of objects registered in the repository */

/**
 * GIInfoType:
 * @GI_INFO_TYPE_INVALID: invalid type
 * @GI_INFO_TYPE_FUNCTION: function, see #GIFunctionInfo
 * @GI_INFO_TYPE_CALLBACK: callback, see #GIFunctionInfo
 * @GI_INFO_TYPE_STRUCT: struct, see #GIStructInfo
 * @GI_INFO_TYPE_BOXED: boxed, see #GIStructInfo or #GIUnionInfo
 * @GI_INFO_TYPE_ENUM: enum, see #GIEnumInfo
 * @GI_INFO_TYPE_FLAGS: flags, see #GIEnumInfo
 * @GI_INFO_TYPE_OBJECT: object, see #GIObjectInfo
 * @GI_INFO_TYPE_INTERFACE: interface, see #GIInterfaceInfo
 * @GI_INFO_TYPE_CONSTANT: contant, see #GIConstantInfo
 * @GI_INFO_TYPE_ERROR_DOMAIN: error domain for a #GError, see #GIErrorDomainInfo
 * @GI_INFO_TYPE_UNION: union, see #GIUnionInfo
 * @GI_INFO_TYPE_VALUE: enum value, see #GIValueInfo
 * @GI_INFO_TYPE_SIGNAL: signal, see #GISignalInfo
 * @GI_INFO_TYPE_VFUNC: virtual function, see #GIVFuncInfo
 * @GI_INFO_TYPE_PROPERTY: GObject property, see #GIPropertyInfo
 * @GI_INFO_TYPE_FIELD: struct or union field, see #GIFieldInfo
 * @GI_INFO_TYPE_ARG: argument of a function or callback, see #GIArgInfo
 * @GI_INFO_TYPE_TYPE: type information, see #GITypeInfo
 * @GI_INFO_TYPE_UNRESOLVED: unresolved type, a type which is not present in
 * the typelib, or any of its dependencies.
 *
 * The type of a GIBaseInfo struct.
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


G_END_DECLS

#endif  /* __GITYPES_H__ */

