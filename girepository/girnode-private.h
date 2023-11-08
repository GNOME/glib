/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 * GObject introspection: Parsed GIR
 *
 * Copyright (C) 2005 Matthias Clasen
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

#include <glib.h>

#include "girmodule-private.h"

G_BEGIN_DECLS

typedef struct _GIIrNode GIIrNode;
typedef struct _GIIrNodeFunction GIIrNodeFunction;
typedef struct _GIIrNodeParam GIIrNodeParam;
typedef struct _GIIrNodeType GIIrNodeType;
typedef struct _GIIrNodeInterface GIIrNodeInterface;
typedef struct _GIIrNodeSignal GIIrNodeSignal;
typedef struct _GIIrNodeProperty GIIrNodeProperty;
typedef struct _GIIrNodeVFunc GIIrNodeVFunc;
typedef struct _GIIrNodeField GIIrNodeField;
typedef struct _GIIrNodeValue GIIrNodeValue;
typedef struct _GIIrNodeEnum GIIrNodeEnum;
typedef struct _GIIrNodeBoxed GIIrNodeBoxed;
typedef struct _GIIrNodeStruct GIIrNodeStruct;
typedef struct _GIIrNodeConstant GIIrNodeConstant;
typedef struct _GIIrNodeXRef GIIrNodeXRef;
typedef struct _GIIrNodeUnion GIIrNodeUnion;

typedef enum
{
  GI_IR_NODE_INVALID      =  0,
  GI_IR_NODE_FUNCTION     =  1,
  GI_IR_NODE_CALLBACK     =  2,
  GI_IR_NODE_STRUCT       =  3,
  GI_IR_NODE_BOXED        =  4,
  GI_IR_NODE_ENUM         =  5,
  GI_IR_NODE_FLAGS        =  6,
  GI_IR_NODE_OBJECT       =  7,
  GI_IR_NODE_INTERFACE    =  8,
  GI_IR_NODE_CONSTANT     =  9,
  GI_IR_NODE_INVALID_0    = 10, /* DELETED - used to be ERROR_DOMAIN */
  GI_IR_NODE_UNION        = 11,
  GI_IR_NODE_PARAM        = 12,
  GI_IR_NODE_TYPE         = 13,
  GI_IR_NODE_PROPERTY     = 14,
  GI_IR_NODE_SIGNAL       = 15,
  GI_IR_NODE_VALUE        = 16,
  GI_IR_NODE_VFUNC        = 17,
  GI_IR_NODE_FIELD        = 18,
  GI_IR_NODE_XREF         = 19
} GIIrNodeTypeId;

struct _GIIrNode
{
  GIIrNodeTypeId type;
  gchar *name;
  GIIrModule *module;

  guint32 offset; /* Assigned as we build the typelib */

  GHashTable *attributes;
};

struct _GIIrNodeXRef
{
  GIIrNode node;

  gchar *namespace;
};

struct _GIIrNodeFunction
{
  GIIrNode node;

  gboolean deprecated;
  gboolean is_varargs; /* Not in typelib yet */

  gboolean is_method;
  gboolean is_setter;
  gboolean is_getter;
  gboolean is_constructor;
  gboolean wraps_vfunc;
  gboolean throws;
  gboolean instance_transfer_full;

  gchar *symbol;
  char *property;

  GIIrNodeParam *result;
  GList *parameters;
};

struct _GIIrNodeType
{
  GIIrNode node;

  gboolean is_pointer;
  gboolean is_basic;
  gboolean is_array;
  gboolean is_glist;
  gboolean is_gslist;
  gboolean is_ghashtable;
  gboolean is_interface;
  gboolean is_error;
  gint tag;

  gchar *unparsed;

  gboolean zero_terminated;
  gboolean has_length;
  gint length;
  gboolean has_size;
  gint size;
  gint array_type;

  GIIrNodeType *parameter_type1;
  GIIrNodeType *parameter_type2;

  gchar *giinterface;
  gchar **errors;
};

struct _GIIrNodeParam
{
  GIIrNode node;

  gboolean in;
  gboolean out;
  gboolean caller_allocates;
  gboolean optional;
  gboolean retval;
  gboolean nullable;
  gboolean skip;
  gboolean transfer;
  gboolean shallow_transfer;
  GIScopeType scope;

  gint8 closure;
  gint8 destroy;

  GIIrNodeType *type;
};

struct _GIIrNodeProperty
{
  GIIrNode node;

  gboolean deprecated;

  gchar *name;
  gboolean readable;
  gboolean writable;
  gboolean construct;
  gboolean construct_only;
  gboolean transfer;
  gboolean shallow_transfer;

  char *setter;
  char *getter;

  GIIrNodeType *type;
};

struct _GIIrNodeSignal
{
  GIIrNode node;

  gboolean deprecated;

  gboolean run_first;
  gboolean run_last;
  gboolean run_cleanup;
  gboolean no_recurse;
  gboolean detailed;
  gboolean action;
  gboolean no_hooks;
  gboolean instance_transfer_full;

  gboolean has_class_closure;
  gboolean true_stops_emit;

  gint class_closure;

  GList *parameters;
  GIIrNodeParam *result;
};

struct _GIIrNodeVFunc
{
  GIIrNode node;

  gboolean is_varargs; /* Not in typelib yet */
  gboolean must_chain_up;
  gboolean must_be_implemented;
  gboolean must_not_be_implemented;
  gboolean is_class_closure;
  gboolean throws;
  gboolean instance_transfer_full;

  char *invoker;

  GList *parameters;
  GIIrNodeParam *result;

  gint offset;
};

struct _GIIrNodeField
{
  GIIrNode node;

  gboolean readable;
  gboolean writable;
  gint bits;
  gint offset;
  GIIrNodeFunction *callback;

  GIIrNodeType *type;
};

struct _GIIrNodeInterface
{
  GIIrNode node;

  gboolean abstract;
  gboolean deprecated;
  gboolean fundamental;
  gboolean final_;

  gchar *gtype_name;
  gchar *gtype_init;

  gchar *ref_func;
  gchar *unref_func;
  gchar *set_value_func;
  gchar *get_value_func;

  gchar *parent;
  gchar *glib_type_struct;

  GList *interfaces;
  GList *prerequisites;

  gint alignment;
  gint size;

  GList *members;
};

struct _GIIrNodeValue
{
  GIIrNode node;

  gboolean deprecated;

  gint64 value;
};

struct _GIIrNodeConstant
{
  GIIrNode node;

  gboolean deprecated;

  GIIrNodeType *type;

  gchar *value;
};

struct _GIIrNodeEnum
{
  GIIrNode node;

  gboolean deprecated;
  gint storage_type;

  gchar *gtype_name;
  gchar *gtype_init;
  gchar *error_domain;

  GList *values;
  GList *methods;
};

struct _GIIrNodeBoxed
{
  GIIrNode node;

  gboolean deprecated;

  gchar *gtype_name;
  gchar *gtype_init;

  gint alignment;
  gint size;

  GList *members;
};

struct _GIIrNodeStruct
{
  GIIrNode node;

  gboolean deprecated;
  gboolean disguised;
  gboolean opaque;
  gboolean pointer;
  gboolean is_gtype_struct;
  gboolean foreign;

  gchar *gtype_name;
  gchar *gtype_init;

  gchar *copy_func;
  gchar *free_func;

  gint alignment;
  gint size;

  GList *members;
};

struct _GIIrNodeUnion
{
  GIIrNode node;

  gboolean deprecated;

  GList *members;
  GList *discriminators;

  gchar *gtype_name;
  gchar *gtype_init;

  gchar *copy_func;
  gchar *free_func;

  gint alignment;
  gint size;

  gint discriminator_offset;
  GIIrNodeType *discriminator_type;
};


GIIrNode *_gi_ir_node_new             (GIIrNodeTypeId  type,
                                       GIIrModule     *module);
void      _gi_ir_node_free            (GIIrNode    *node);
guint32   _gi_ir_node_get_size        (GIIrNode    *node);
guint32   _gi_ir_node_get_full_size   (GIIrNode    *node);
void      _gi_ir_node_build_typelib   (GIIrNode         *node,
                                       GIIrNode         *parent,
                                       GIIrTypelibBuild *build,
                                       guint32          *offset,
                                       guint32          *offset2,
                                       guint16          *count2);
int       _gi_ir_node_cmp             (GIIrNode *node,
                                       GIIrNode *other);
gboolean  _gi_ir_node_can_have_member (GIIrNode *node);
void      _gi_ir_node_add_member      (GIIrNode         *node,
                                       GIIrNodeFunction *member);
guint32   _gi_ir_write_string         (const gchar *str,
                                       GHashTable  *strings,
                                       guchar      *data,
                                       guint32     *offset);

const gchar * _gi_ir_node_param_direction_string (GIIrNodeParam * node);
const gchar * _gi_ir_node_type_to_string         (GIIrNodeTypeId type);

GIIrNode *_gi_ir_find_node (GIIrTypelibBuild *build,
                            GIIrModule       *module,
                            const char       *name);

/* In giroffsets.c */

void _gi_ir_node_compute_offsets (GIIrTypelibBuild *build,
                                  GIIrNode         *node);


G_END_DECLS
