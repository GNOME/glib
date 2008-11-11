/* GObject introspection: Compute structure offsets
 *
 * Copyright (C) 2008 Red Hat, Inc.
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

#include "girffi.h"
#include "girnode.h"

/* The C standard specifies that an enumeration can be any char or any signed
 * or unsigned integer type capable of resresenting all the values of the
 * enumeration. We use test enumerations to figure out what choices the
 * compiler makes.
 */

typedef enum {
  ENUM_1 = 1 /* compiler could use int8, uint8, int16, uint16, int32, uint32 */
} Enum1;

typedef enum {
  ENUM_2 = 128 /* compiler could use uint8, int16, uint16, int32, uint32 */
} Enum2;

typedef enum {
  ENUM_3 = 257 /* compiler could use int16, uint16, int32, uint32 */
} Enum3;

typedef enum {
  ENUM_4 = G_MAXSHORT + 1 /* compiler could use uint16, int32, uint32 */
} Enum4;

typedef enum {
  ENUM_5 = G_MAXUSHORT + 1 /* compiler could use int32, uint32 */
} Enum5;

typedef enum {
  ENUM_6 = ((guint)G_MAXINT) + 1 /* compiler could use uint32 */
} Enum6;

/* GIrNodeValue has guint32 values, so if it matters to the ABI whether
 * constant values are signed, we are in trouble. And we don't handle
 * enums with > 32 bit values. */

#if 0
typedef enum {
  ENUM_7 = -1 /* compiler could use int8, int16, int32 */
} Enum7;

/* etc... */
#endif

static gboolean
get_enum_size_alignment (GIrNodeEnum *enum_node,
			 gint        *size,
			 gint        *alignment)
{
  GList *l;
  guint32 max_value = 0;
  int width;
  ffi_type *type_ffi;

  for (l = enum_node->values; l; l = l->next)
    {
      GIrNodeValue *value = l->data;
      if (value->value > max_value)
	max_value = value->value;
    }

  if (max_value < 128)
    width = sizeof (Enum1);
  else if (max_value < 256)
    width = sizeof (Enum2);
  else if (max_value < G_MAXSHORT)
    width = sizeof (Enum3);
  else if (max_value < G_MAXUSHORT)
    width = sizeof (Enum4);
  else if (max_value < G_MAXINT)
    width = sizeof (Enum5);
  else
    width = sizeof (Enum6);

  if (width == 1)
    type_ffi = &ffi_type_sint8;
  else if (width == 2)
    type_ffi = &ffi_type_sint16;
  else if (width == 4)
    type_ffi = &ffi_type_sint32;
  else if (width == 8)
    type_ffi = &ffi_type_sint64;
  else
    g_error ("Unexpected enum width %d", width);

  *size = type_ffi->size;
  *alignment = type_ffi->alignment;

  return TRUE;
}

static gboolean
get_interface_size_alignment (GIrNodeType *type,
			      GIrModule   *module,
			      GList       *modules,
			      gint        *size,
			      gint        *alignment)
{
  GIrNode *iface;
  GIrModule *iface_module;

  if (!g_ir_find_node (module, modules, type->interface, &iface, &iface_module))
    {
      g_warning ("Type for type name '%s' not found", type->interface);
      *size = -1;
      *alignment = -1;
      return FALSE;
    }

  g_ir_node_compute_offsets (iface, iface_module,
			     iface_module == module ? modules : NULL);

  switch (iface->type)
    {
    case G_IR_NODE_BOXED:
      {
	GIrNodeBoxed *boxed = (GIrNodeBoxed *)iface;
	*size = boxed->size;
	*alignment = boxed->alignment;
	break;
      }
    case G_IR_NODE_STRUCT:
      {
	GIrNodeStruct *struct_ = (GIrNodeStruct *)iface;
	*size = struct_->size;
	*alignment = struct_->alignment;
	break;
      }
    case G_IR_NODE_UNION:
      {
	GIrNodeUnion *union_ = (GIrNodeUnion *)iface;
	*size = union_->size;
	*alignment = union_->alignment;
	break;
      }
    case G_IR_NODE_ENUM:
    case G_IR_NODE_FLAGS:
      {
	return get_enum_size_alignment ((GIrNodeEnum *)iface,
					size, alignment);
      }
    case G_IR_NODE_CALLBACK:
      {
	*size = ffi_type_pointer.size;
	*alignment = ffi_type_pointer.alignment;
	break;
      }
    default:
      {
	g_warning ("Unexpected non-pointer field of type %s in structure",
		   g_ir_node_type_to_string (iface->type));
	*size = -1;
	*alignment = -1;
	break;
      }
    }

  return *alignment != -1;
}

static gboolean
get_field_size_alignment (GIrNodeField *field,
			  GIrModule    *module,
			  GList        *modules,
			  gint         *size,
			  gint         *alignment)
{
  GIrNodeType *type = field->type;
  ffi_type *type_ffi;

  if (type->is_pointer)
    {
      type_ffi = &ffi_type_pointer;
    }
  else
    {
      if (type->tag == GI_TYPE_TAG_INTERFACE)
	{
	  return get_interface_size_alignment (type,
					       module, modules,
					       size, alignment);
	}
      else
	{
	  type_ffi = g_ir_ffi_get_ffi_type (type->tag);

	  if (type_ffi == &ffi_type_void)
	    {
	      g_warning ("field '%s' has void type", ((GIrNode *)field)->name);
	      *size = -1;
	      *alignment = -1;
	      return FALSE;
	    }
	  else if (type_ffi == &ffi_type_pointer)
	    {
	      g_warning ("non-pointer field '%s' has unhandled type %s",
			 ((GIrNode *)field)->name,
			 g_type_tag_to_string (type->tag));
	      *size = -1;
	      *alignment = -1;
	      return FALSE;
	    }
	}
    }

  g_assert (type_ffi);
  *size = type_ffi->size;
  *alignment = type_ffi->alignment;

  return TRUE;
}

#define ALIGN(n, align) (((n) + (align) - 1) & ~((align) - 1))

static gboolean
compute_struct_field_offsets (GList       *members,
			      GIrModule   *module,
			      GList       *modules,
			      gint        *size_out,
			      gint        *alignment_out)
{
  int size = 0;
  int alignment = 1;
  GList *l;
  gboolean have_error = FALSE;

  for (l = members; l; l = l->next)
    {
      GIrNode *member = (GIrNode *)l->data;

      if (member->type == G_IR_NODE_FIELD)
	{
	  GIrNodeField *field = (GIrNodeField *)member;

	  if (!have_error)
	    {
	      int member_size;
	      int member_alignment;

	      if (get_field_size_alignment (field,
					    module, modules,
					    &member_size, &member_alignment))
		{
		  size = ALIGN (size, member_alignment);
		  alignment = MAX (alignment, member_alignment);
		  field->offset = size;
		  size += member_size;
		}
	      else
		have_error = TRUE;
	    }

	  if (have_error)
	    field->offset = -1;
	}
      else if (member->type == G_IR_NODE_CALLBACK)
	{
	  size = ffi_type_pointer.size;
	  alignment = ffi_type_pointer.alignment;
	}
    }

  /* Structs are tail-padded out to a multiple of their alignment */
  size = ALIGN (size, alignment);

  if (!have_error)
    {
      *size_out = size;
      *alignment_out = alignment;
    }
  else
    {
      *size_out = -1;
      *alignment_out = -1;
    }

  return !have_error;
}

static gboolean
compute_union_field_offsets (GList       *members,
			     GIrModule   *module,
			     GList       *modules,
			     gint        *size_out,
			     gint        *alignment_out)
{
  int size = 0;
  int alignment = 1;
  GList *l;
  gboolean have_error = FALSE;

  for (l = members; l; l = l->next)
    {
      GIrNode *member = (GIrNode *)l->data;

      if (member->type == G_IR_NODE_FIELD)
	{
	  GIrNodeField *field = (GIrNodeField *)member;

	  if (!have_error)
	    {
	      int member_size;
	      int member_alignment;

	      if (get_field_size_alignment (field,
					    module, modules,
					    &member_size, &member_alignment))
		{
		  size = MAX (size, member_size);
		  alignment = MAX (alignment, member_alignment);
		}
	      else
		have_error = TRUE;
	    }
	}
    }

  /* Unions are tail-padded out to a multiple of their alignment */
  size = ALIGN (size, alignment);

  if (!have_error)
    {
      *size_out = size;
      *alignment_out = alignment;
    }
  else
    {
      *size_out = -1;
      *alignment_out = -1;
    }

  return !have_error;
}

/**
 * g_ir_node_compute_offsets:
 * @node: a #GIrNode
 * @module: Current module being processed
 * @moudles: all currently loaded modules
 *
 * If a node is a a structure or union, makes sure that the field
 * offsets have been computed, and also computes the overall size and
 * alignment for the type.
 */
void
g_ir_node_compute_offsets (GIrNode   *node,
			   GIrModule *module,
		   GList     *modules)
{
  switch (node->type)
    {
    case G_IR_NODE_BOXED:
      {
	GIrNodeBoxed *boxed = (GIrNodeBoxed *)node;

	if (boxed->alignment != 0) /* Already done */
	  return;

	compute_struct_field_offsets (boxed->members,
				      module, modules,
				      &boxed->size, &boxed->alignment);
	break;
      }
    case G_IR_NODE_STRUCT:
      {
	GIrNodeStruct *struct_ = (GIrNodeStruct *)node;

	if (struct_->alignment != 0)
	  return;

	compute_struct_field_offsets (struct_->members,
				      module, modules,
				      &struct_->size, &struct_->alignment);
	break;
      }
    case G_IR_NODE_UNION:
      {
	GIrNodeUnion *union_ = (GIrNodeUnion *)node;

	if (union_->alignment != 0)
	  return;

	compute_union_field_offsets (union_->members,
				     module, modules,
				     &union_->size, &union_->alignment);
	break;
      }
    default:
      /* Nothing to do */
      return;
    }
}
