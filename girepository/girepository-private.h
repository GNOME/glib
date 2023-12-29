/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 * GObject introspection: Private headers
 *
 * Copyright (C) 2010 Johan Dahlin
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

#include <ffi.h>
#include <glib.h>

#define __GIREPOSITORY_H_INSIDE__

#include <girepository/gibaseinfo.h>
#include <girepository/girepository.h>
#include <girepository/gitypeinfo.h>
#include <gitypelib/gitypelib.h>
#include <gitypelib/gitypelib-private.h>

/* FIXME: For now, GIRealInfo is a compatibility define. This will eventually
 * be removed. */
typedef struct _GIBaseInfo GIRealInfo;

/* We changed a gint32 -> gint in the structure below, which should be
 * valid everywhere we care about.
 */
G_STATIC_ASSERT (sizeof (int) == sizeof (gint32));

/*
 * We just use one structure for all of the info object
 * types; in general, we should be reading data directly
 * from the typelib, and not having computed data in
 * per-type structures.
 */
struct _GIBaseInfo
{
  /*< private >*/
  GTypeInstance parent_instance;
  gatomicrefcount ref_count;

  GIRepository *repository;
  GIBaseInfo *container;

  GITypelib *typelib;
  guint32 offset;

  guint32 type_is_embedded : 1; /* Used by GITypeInfo */
};

/* Subtypes */
struct _GICallableInfo
{
  GIBaseInfo parent;
};

void gi_callable_info_class_init (gpointer g_class,
                                  gpointer class_data);

struct _GIFunctionInfo
{
  GIBaseInfo parent;
};

void gi_function_info_class_init (gpointer g_class,
                                  gpointer class_data);

struct _GICallbackInfo
{
  GIBaseInfo parent;
};

void gi_callback_info_class_init (gpointer g_class,
                                  gpointer class_data);

struct _GIRegisteredTypeInfo
{
  GIBaseInfo parent;
};

void gi_registered_type_info_class_init (gpointer g_class,
                                         gpointer class_data);

struct _GIStructInfo
{
  GIBaseInfo parent;
};

void gi_struct_info_class_init (gpointer g_class,
                                gpointer class_data);

struct _GIUnionInfo
{
  GIBaseInfo parent;
};

void gi_union_info_class_init (gpointer g_class,
                               gpointer class_data);

struct _GIEnumInfo
{
  GIBaseInfo parent;
};

void gi_enum_info_class_init (gpointer g_class,
                              gpointer class_data);

struct _GIObjectInfo
{
  GIBaseInfo parent;
};

void gi_object_info_class_init (gpointer g_class,
                                gpointer class_data);

struct _GIInterfaceInfo
{
  GIBaseInfo parent;
};

void gi_interface_info_class_init (gpointer g_class,
                                   gpointer class_data);

struct _GIConstantInfo
{
  GIBaseInfo parent;
};

void gi_constant_info_class_init (gpointer g_class,
                                  gpointer class_data);

struct _GIValueInfo
{
  GIBaseInfo parent;
};

void gi_value_info_class_init (gpointer g_class,
                               gpointer class_data);

struct _GISignalInfo
{
  GIBaseInfo parent;
};

void gi_signal_info_class_init (gpointer g_class,
                                gpointer class_data);

struct _GIVFuncInfo
{
  GIBaseInfo parent;
};

void gi_vfunc_info_class_init (gpointer g_class,
                               gpointer class_data);

struct _GIPropertyInfo
{
  GIBaseInfo parent;
};

void gi_property_info_class_init (gpointer g_class,
                                  gpointer class_data);

struct _GIFieldInfo
{
  GIBaseInfo parent;
};

void gi_field_info_class_init (gpointer g_class,
                               gpointer class_data);

struct _GIArgInfo
{
  GIBaseInfo parent;
};

void gi_arg_info_class_init (gpointer g_class,
                             gpointer class_data);

struct _GITypeInfo
{
  GIBaseInfo parent;
};

void gi_type_info_class_init (gpointer g_class,
                              gpointer class_data);

struct _GIUnresolvedInfo
{
  GIBaseInfo parent;

  const gchar *name;
  const gchar *namespace;
};

void gi_unresolved_info_class_init (gpointer g_class,
                                    gpointer class_data);

void         gi_info_init       (GIRealInfo   *info,
                                 GIInfoType    type,
                                 GIRepository *repository,
                                 GIBaseInfo   *container,
                                 GITypelib    *typelib,
                                 guint32       offset);

GIBaseInfo * gi_info_from_entry (GIRepository *repository,
                                 GITypelib    *typelib,
                                 guint16       index);

GIBaseInfo * gi_info_new_full   (GIInfoType    type,
                                 GIRepository *repository,
                                 GIBaseInfo   *container,
                                 GITypelib    *typelib,
                                 guint32       offset);

GITypeInfo * gi_type_info_new   (GIBaseInfo *container,
                                 GITypelib  *typelib,
                                 guint32     offset);

void         gi_type_info_init  (GIBaseInfo *info,
                                 GIBaseInfo *container,
                                 GITypelib  *typelib,
                                 guint32     offset);

GIFunctionInfo * gi_base_info_find_method (GIBaseInfo  *base,
                                           guint32      offset,
                                           guint        n_methods,
                                           const gchar *name);

GIVFuncInfo * gi_base_info_find_vfunc (GIRealInfo  *rinfo,
                                       guint32      offset,
                                       guint        n_vfuncs,
                                       const gchar *name);
