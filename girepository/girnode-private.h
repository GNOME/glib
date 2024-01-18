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

/**
 * GIIrOffsetsState:
 * @GI_IR_OFFSETS_UNKNOWN: offsets have not been calculated yet
 * @GI_IR_OFFSETS_COMPUTED: offsets have been successfully calculated
 * @GI_IR_OFFSETS_FAILED: calculating the offsets failed
 * @GI_IR_OFFSETS_IN_PROGRESS: offsets are currently being calculated (used to
 *   detect type recursion)
 *
 * State tracking for calculating size and alignment of
 * [type@GIRepository.IrNode]s.
 *
 * Since: 2.80
 */
typedef enum
{
  GI_IR_OFFSETS_UNKNOWN,
  GI_IR_OFFSETS_COMPUTED,
  GI_IR_OFFSETS_FAILED,
  GI_IR_OFFSETS_IN_PROGRESS,
} GIIrOffsetsState;

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

  uint8_t deprecated : 1;
  uint8_t is_varargs : 1; /* Not in typelib yet */

  uint8_t is_method : 1;
  uint8_t is_setter : 1;
  uint8_t is_getter : 1;
  uint8_t is_constructor : 1;
  uint8_t wraps_vfunc : 1;
  uint8_t throws : 1;
  uint8_t instance_transfer_full : 1;

  char *symbol;
  char *property;

  GIIrNodeParam *result;
  GList *parameters;
};

struct _GIIrNodeType
{
  GIIrNode node;

  uint8_t is_pointer : 1;
  uint8_t is_basic : 1;
  uint8_t is_array : 1;
  uint8_t is_glist : 1;
  uint8_t is_gslist : 1;
  uint8_t is_ghashtable : 1;
  uint8_t is_interface : 1;
  uint8_t is_error : 1;
  int tag;

  char *unparsed;

  uint8_t zero_terminated : 1;
  uint8_t has_length : 1;
  int length;
  uint8_t has_size : 1;
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

  uint8_t in : 1;
  uint8_t out : 1;
  uint8_t caller_allocates : 1;
  uint8_t optional : 1;
  uint8_t retval : 1;
  uint8_t nullable : 1;
  uint8_t skip : 1;
  uint8_t transfer : 1;
  uint8_t shallow_transfer : 1;
  GIScopeType scope : 3;

  int8_t closure;
  int8_t destroy;

  GIIrNodeType *type;
};

struct _GIIrNodeProperty
{
  GIIrNode node;

  uint8_t deprecated : 1;

  char *name;
  uint8_t readable : 1;
  uint8_t writable : 1;
  uint8_t construct : 1;
  uint8_t construct_only : 1;
  uint8_t transfer : 1;
  uint8_t shallow_transfer : 1;

  char *setter;
  char *getter;

  GIIrNodeType *type;
};

struct _GIIrNodeSignal
{
  GIIrNode node;

  uint8_t deprecated : 1;

  uint8_t run_first : 1;
  uint8_t run_last : 1;
  uint8_t run_cleanup : 1;
  uint8_t no_recurse : 1;
  uint8_t detailed : 1;
  uint8_t action : 1;
  uint8_t no_hooks : 1;
  uint8_t instance_transfer_full : 1;

  uint8_t has_class_closure : 1;
  uint8_t true_stops_emit : 1;

  int class_closure;

  GList *parameters;
  GIIrNodeParam *result;
};

struct _GIIrNodeVFunc
{
  GIIrNode node;

  uint8_t is_varargs : 1; /* Not in typelib yet */
  uint8_t must_chain_up : 1;
  uint8_t must_be_implemented : 1;
  uint8_t must_not_be_implemented : 1;
  uint8_t is_class_closure : 1;
  uint8_t throws : 1;
  uint8_t instance_transfer_full : 1;

  char *invoker;

  GList *parameters;
  GIIrNodeParam *result;

  int offset;
};

struct _GIIrNodeField
{
  GIIrNode node;

  uint8_t readable : 1;
  uint8_t writable : 1;
  int bits;
  int offset;
  GIIrNodeFunction *callback;

  GIIrNodeType *type;
};

struct _GIIrNodeInterface
{
  GIIrNode node;

  uint8_t abstract : 1;
  uint8_t deprecated : 1;
  uint8_t fundamental : 1;
  uint8_t final_ : 1;

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

  size_t alignment;
  size_t size;
  GIIrOffsetsState offsets_state;

  GList *members;
};

struct _GIIrNodeValue
{
  GIIrNode node;

  uint8_t deprecated : 1;

  int64_t value;
};

struct _GIIrNodeConstant
{
  GIIrNode node;

  uint8_t deprecated : 1;

  GIIrNodeType *type;

  char *value;
};

struct _GIIrNodeEnum
{
  GIIrNode node;

  uint8_t deprecated : 1;
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

  uint8_t deprecated : 1;

  char *gtype_name;
  char *gtype_init;

  size_t alignment;
  size_t size;
  GIIrOffsetsState offsets_state;

  GList *members;
};

struct _GIIrNodeStruct
{
  GIIrNode node;

  uint8_t deprecated : 1;
  uint8_t disguised : 1;
  uint8_t opaque : 1;
  uint8_t pointer : 1;
  uint8_t is_gtype_struct : 1;
  uint8_t foreign : 1;

  char *gtype_name;
  char *gtype_init;

  char *copy_func;
  char *free_func;

  size_t alignment;
  size_t size;
  GIIrOffsetsState offsets_state;

  GList *members;
};

struct _GIIrNodeUnion
{
  GIIrNode node;

  uint8_t deprecated : 1;

  GList *members;
  GList *discriminators;

  char *gtype_name;
  char *gtype_init;

  char *copy_func;
  char *free_func;

  size_t alignment;
  size_t size;
  GIIrOffsetsState offsets_state;

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
                                      uint8_t     *data,
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
