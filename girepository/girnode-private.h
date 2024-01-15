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
  char *name;
  GIIrModule *module;

  uint32_t offset; /* Assigned as we build the typelib */

  GHashTable *attributes;
};

struct _GIIrNodeXRef
{
  GIIrNode node;

  char *namespace;
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

  char *symbol;
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
  int tag;

  char *unparsed;

  gboolean zero_terminated;
  gboolean has_length;
  int length;
  gboolean has_size;
  int size;
  int array_type;

  GIIrNodeType *parameter_type1;
  GIIrNodeType *parameter_type2;

  char *giinterface;
  char **errors;
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

  int8_t closure;
  int8_t destroy;

  GIIrNodeType *type;
};

struct _GIIrNodeProperty
{
  GIIrNode node;

  gboolean deprecated;

  char *name;
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

  int class_closure;

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

  int offset;
};

struct _GIIrNodeField
{
  GIIrNode node;

  gboolean readable;
  gboolean writable;
  int bits;
  int offset;
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

  char *gtype_name;
  char *gtype_init;

  char *ref_func;
  char *unref_func;
  char *set_value_func;
  char *get_value_func;

  char *parent;
  char *glib_type_struct;

  GList *interfaces;
  GList *prerequisites;

  int alignment;
  int size;

  GList *members;
};

struct _GIIrNodeValue
{
  GIIrNode node;

  gboolean deprecated;

  int64_t value;
};

struct _GIIrNodeConstant
{
  GIIrNode node;

  gboolean deprecated;

  GIIrNodeType *type;

  char *value;
};

struct _GIIrNodeEnum
{
  GIIrNode node;

  gboolean deprecated;
  int storage_type;

  char *gtype_name;
  char *gtype_init;
  char *error_domain;

  GList *values;
  GList *methods;
};

struct _GIIrNodeBoxed
{
  GIIrNode node;

  gboolean deprecated;

  char *gtype_name;
  char *gtype_init;

  int alignment;
  int size;

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

  char *gtype_name;
  char *gtype_init;

  char *copy_func;
  char *free_func;

  int alignment;
  int size;

  GList *members;
};

struct _GIIrNodeUnion
{
  GIIrNode node;

  gboolean deprecated;

  GList *members;
  GList *discriminators;

  char *gtype_name;
  char *gtype_init;

  char *copy_func;
  char *free_func;

  int alignment;
  int size;

  int discriminator_offset;
  GIIrNodeType *discriminator_type;
};


GIIrNode *gi_ir_node_new             (GIIrNodeTypeId  type,
                                      GIIrModule     *module);
void      gi_ir_node_free            (GIIrNode    *node);
uint32_t  gi_ir_node_get_size        (GIIrNode    *node);
uint32_t  gi_ir_node_get_full_size   (GIIrNode    *node);
void      gi_ir_node_build_typelib   (GIIrNode         *node,
                                      GIIrNode         *parent,
                                      GIIrTypelibBuild *build,
                                      uint32_t         *offset,
                                      uint32_t         *offset2,
                                      uint16_t         *count2);
int       gi_ir_node_cmp             (GIIrNode *node,
                                      GIIrNode *other);
gboolean  gi_ir_node_can_have_member (GIIrNode *node);
void      gi_ir_node_add_member      (GIIrNode         *node,
                                      GIIrNodeFunction *member);
uint32_t  gi_ir_write_string         (const char  *str,
                                      GHashTable  *strings,
                                      guchar      *data,
                                      uint32_t    *offset);

const char * gi_ir_node_param_direction_string (GIIrNodeParam * node);
const char * gi_ir_node_type_to_string         (GIIrNodeTypeId type);

GIIrNode *gi_ir_find_node (GIIrTypelibBuild *build,
                           GIIrModule       *module,
                           const char       *name);

/* In giroffsets.c */

void gi_ir_node_compute_offsets (GIIrTypelibBuild *build,
                                 GIIrNode         *node);


G_END_DECLS
