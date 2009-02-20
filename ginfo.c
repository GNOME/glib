/* GObject introspection: Repository implementation
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

#include <stdlib.h>
#include <string.h>

#include <glib.h>
#include <glib-object.h>

#include "gtypelib.h"
#include "ginfo.h"

struct _GIBaseInfo 
{
  gint type;
  gint ref_count;
  GIRepository *repository;
  GIBaseInfo *container;

  GTypelib *typelib;
  guint32 offset;
};

struct _GIUnresolvedInfo
{
  gint type;
  gint ref_count;
  GIBaseInfo *container;

  const gchar *name;
  const gchar *namespace;
};

struct _GICallableInfo
{
  GIBaseInfo base;
};

struct _GIFunctionInfo
{
  GICallableInfo callable;
};

struct _GICallbackInfo
{
  GICallableInfo callable;
};

struct _GIRegisteredTypeInfo
{
  GIBaseInfo base;
};

struct _GIStructInfo
{
  GIRegisteredTypeInfo registered;
};

struct _GIEnumInfo
{
  GIRegisteredTypeInfo registered;
};

struct _GIObjectInfo
{
  GIRegisteredTypeInfo registered;
};

struct _GIInterfaceInfo
{
  GIRegisteredTypeInfo registered;
};

struct _GIConstantInfo
{
  GIBaseInfo base;
};

struct _GIValueInfo
{
  GIBaseInfo base;
};

struct _GISignalInfo
{
  GICallableInfo callable;
};

struct _GIVFuncInfo
{
  GICallableInfo callable;
};

struct _GIPropertyInfo
{
  GIBaseInfo base;
};

struct _GIFieldInfo
{
  GIBaseInfo base;
};

struct _GIArgInfo
{
  GIBaseInfo base;
};

struct _GITypeInfo
{
  GIBaseInfo base;
};

struct _GIUnionInfo
{
  GIRegisteredTypeInfo registered;
};


/* info creation */
GIBaseInfo *
g_info_new_full (GIInfoType     type,
		 GIRepository  *repository,
		 GIBaseInfo    *container,
		 GTypelib     *typelib, 
		 guint32        offset)
{
  GIBaseInfo *info;

  g_return_val_if_fail (container != NULL || repository != NULL, NULL);

  info = g_new0 (GIBaseInfo, 1);

  info->ref_count = 1;
  info->type = type;

  info->typelib = typelib;
  info->offset = offset;

  if (container)
    info->container = g_base_info_ref (container);

  info->repository = g_object_ref (repository);

  return info;
}

GIBaseInfo *
g_info_new (GIInfoType     type,
	    GIBaseInfo    *container,
	    GTypelib     *typelib, 
	    guint32        offset)
{
  return g_info_new_full (type, container->repository, container, typelib, offset);
}

static GIBaseInfo *
g_info_from_entry (GIRepository *repository,
		   GTypelib *typelib,
		   guint16    index)
{
  GIBaseInfo *result;
  DirEntry *entry = g_typelib_get_dir_entry (typelib, index);

  if (entry->local)
    result = g_info_new_full (entry->blob_type, repository, NULL, typelib, entry->offset);
  else
    {
      const gchar *namespace = g_typelib_get_string (typelib, entry->offset);
      const gchar *name = g_typelib_get_string (typelib, entry->name);

      result = g_irepository_find_by_name (repository, namespace, name);
      if (result == NULL)
	{
	  GIUnresolvedInfo *unresolved;

	  unresolved = g_new0 (GIUnresolvedInfo, 1);

	  unresolved->type = GI_INFO_TYPE_UNRESOLVED;
	  unresolved->ref_count = 1;
	  unresolved->container = NULL;
	  unresolved->name = name;
	  unresolved->namespace = namespace;

	  return (GIBaseInfo*)unresolved;
	}
      return result;
    }

  return result;
}

/* GIBaseInfo functions */
GIBaseInfo *
g_base_info_ref (GIBaseInfo *info)
{
  info->ref_count++;

  return info;
}

void
g_base_info_unref (GIBaseInfo *info)
{
  g_assert (info->ref_count > 0);
  info->ref_count--;

  if (!info->ref_count)
    {
      if (info->container)
	g_base_info_unref (info->container);

      if (info->repository)
        g_object_unref (info->repository);

      g_free (info);
    }
}

GIInfoType
g_base_info_get_type (GIBaseInfo *info)
{
  
  return info->type;
}

const gchar *
g_base_info_get_name (GIBaseInfo *info)
{
  g_assert (info->ref_count > 0);
  switch (info->type)
    {
    case GI_INFO_TYPE_FUNCTION:
    case GI_INFO_TYPE_CALLBACK:
    case GI_INFO_TYPE_STRUCT:
    case GI_INFO_TYPE_BOXED:
    case GI_INFO_TYPE_ENUM:
    case GI_INFO_TYPE_FLAGS:
    case GI_INFO_TYPE_OBJECT:
    case GI_INFO_TYPE_INTERFACE:
    case GI_INFO_TYPE_CONSTANT:
    case GI_INFO_TYPE_ERROR_DOMAIN:
    case GI_INFO_TYPE_UNION:
      {
	CommonBlob *blob = (CommonBlob *)&info->typelib->data[info->offset];

	return g_typelib_get_string (info->typelib, blob->name);
      }
      break;

    case GI_INFO_TYPE_VALUE:
      {
	ValueBlob *blob = (ValueBlob *)&info->typelib->data[info->offset];

	return g_typelib_get_string (info->typelib, blob->name);
      }
      break;

    case GI_INFO_TYPE_SIGNAL:
      {
	SignalBlob *blob = (SignalBlob *)&info->typelib->data[info->offset];

	return g_typelib_get_string (info->typelib, blob->name);
      }
      break;

    case GI_INFO_TYPE_PROPERTY:
      {
	PropertyBlob *blob = (PropertyBlob *)&info->typelib->data[info->offset];

	return g_typelib_get_string (info->typelib, blob->name);
      }
      break;

    case GI_INFO_TYPE_VFUNC:
      {
	VFuncBlob *blob = (VFuncBlob *)&info->typelib->data[info->offset];

	return g_typelib_get_string (info->typelib, blob->name);
      }
      break;

    case GI_INFO_TYPE_FIELD:
      {
	FieldBlob *blob = (FieldBlob *)&info->typelib->data[info->offset];
	
	return g_typelib_get_string (info->typelib, blob->name);
      }
      break;

    case GI_INFO_TYPE_ARG:
      {
	ArgBlob *blob = (ArgBlob *)&info->typelib->data[info->offset];
	
	return g_typelib_get_string (info->typelib, blob->name);
      }
      break;
    case GI_INFO_TYPE_UNRESOLVED:
      {
	GIUnresolvedInfo *unresolved = (GIUnresolvedInfo *)info;

	return unresolved->name;
      }
      break;
    case GI_INFO_TYPE_TYPE:
    default: ;
      g_assert_not_reached ();
      /* unnamed */
    }

  return NULL;
}

const gchar *
g_base_info_get_namespace (GIBaseInfo *info)
{
  Header *header = (Header *)info->typelib->data;

  g_assert (info->ref_count > 0);

  if (info->type == GI_INFO_TYPE_UNRESOLVED)
    {
      GIUnresolvedInfo *unresolved = (GIUnresolvedInfo *)info;
      
      return unresolved->namespace;
    }

  return g_typelib_get_string (info->typelib, header->namespace);
}

gboolean 
g_base_info_is_deprecated (GIBaseInfo *info)
{
  switch (info->type)
    {
    case GI_INFO_TYPE_FUNCTION:
    case GI_INFO_TYPE_CALLBACK:
    case GI_INFO_TYPE_STRUCT:
    case GI_INFO_TYPE_BOXED:
    case GI_INFO_TYPE_ENUM:
    case GI_INFO_TYPE_FLAGS:
    case GI_INFO_TYPE_OBJECT:
    case GI_INFO_TYPE_INTERFACE:
    case GI_INFO_TYPE_CONSTANT:
    case GI_INFO_TYPE_ERROR_DOMAIN:
      {
	CommonBlob *blob = (CommonBlob *)&info->typelib->data[info->offset];

	return blob->deprecated;
      }
      break;

    case GI_INFO_TYPE_VALUE:
      {
	ValueBlob *blob = (ValueBlob *)&info->typelib->data[info->offset];

	return blob->deprecated;
      }
      break;

    case GI_INFO_TYPE_SIGNAL:
      {
	SignalBlob *blob = (SignalBlob *)&info->typelib->data[info->offset];

	return blob->deprecated;
      }
      break;

    case GI_INFO_TYPE_PROPERTY:
      {
	PropertyBlob *blob = (PropertyBlob *)&info->typelib->data[info->offset];

	return blob->deprecated;
      }
      break;

    case GI_INFO_TYPE_VFUNC:
    case GI_INFO_TYPE_FIELD:
    case GI_INFO_TYPE_ARG:
    case GI_INFO_TYPE_TYPE:
    default: ;
      /* no deprecation flag for these */
    }
  
  return FALSE;
}

/**
 * g_base_info_get_attribute:
 * @info: A #GIBaseInfo
 * @name: A freeform string naming an attribute
 *
 * Retrieve an arbitrary attribute associated with this node.
 *
 * Return value: The value of the attribute, or %NULL if no such attribute exists
 */
const gchar *
g_base_info_get_attribute (GIBaseInfo   *info,
			   const gchar *name)
{
  GIAttributeIter iter = { 0, };
  gchar *curname, *curvalue;
  while (g_base_info_iterate_attributes (info, &iter, &curname, &curvalue))
    {
      if (strcmp (name, curname) == 0)
        return (const gchar*) curvalue;
    }

  return NULL;
}

static int
cmp_attribute (const void *av,
                const void *bv)
{
  const AttributeBlob *a = av;
  const AttributeBlob *b = bv;
 
  if (a->offset < b->offset)
    return -1;
  else if (a->offset == b->offset)
    return 0;
  else
    return 1;
}

static AttributeBlob *
find_first_attribute (GIBaseInfo *info)
{
  GIBaseInfo *base = (GIBaseInfo *)info;
  Header *header = (Header *)base->typelib->data;
  AttributeBlob blob, *first, *res, *previous;

  blob.offset = base->offset;
  
  first = (AttributeBlob *) &base->typelib->data[header->attributes];

  res = bsearch (&blob, first, header->n_attributes,
                 header->attribute_blob_size, cmp_attribute);

  if (res == NULL)
    return NULL;

  previous = res - 1;
  while (previous >= first && previous->offset == base->offset)
    {
      res = previous;
      previous = res - 1;
    }

  return res;
}

/**
 * g_base_info_iterate_attributes:
 * @info: A #GIBaseInfo
 * @iter: A #GIAttributeIter structure, must be initialized; see below
 * @name: (out) (transfer none): Returned name, must not be freed
 * @value: (out) (transfer none): Returned name, must not be freed
 *
 * Iterate over all attributes associated with this node.  The iterator
 * structure is typically stack allocated, and must have its first
 * member initialized to %NULL.
 *
 * Both the @name and @value should be treated as constants
 * and must not be freed.
 *
 * <example>
 * <title>Iterating over attributes</title>
 * <programlisting>
 * void
 * print_attributes (GIBaseInfo *info)
 * {
 *   GIAttributeIter iter = { 0, };
 *   char *name;
 *   char *value;
 *   while (g_base_info_iterate_attributes (info, &iter, &name, &value))
 *     {
 *       g_print ("attribute name: %s value: %s", name, value);
 *     }
 * }
 * </programlisting>
 * </example>
 *
 * Return value: %TRUE if there are more attributes, %FALSE otherwise
 */
gboolean
g_base_info_iterate_attributes (GIBaseInfo       *info,
                                 GIAttributeIter *iter,
                                 gchar           **name,
                                 gchar           **value)
{
  GIBaseInfo *base = (GIBaseInfo *)info;
  Header *header = (Header *)base->typelib->data;
  AttributeBlob *next, *after;

  after = (AttributeBlob *) &base->typelib->data[header->attributes +
                                                  header->n_attributes * header->attribute_blob_size];

  if (iter->data != NULL)
    next = (AttributeBlob *) iter->data;
  else
    next = find_first_attribute (info);

  if (next == NULL || next->offset != base->offset || next >= after)
    return FALSE;

  *name = (gchar*) g_typelib_get_string (base->typelib, next->name);
  *value = (gchar*) g_typelib_get_string (base->typelib, next->value);
  iter->data = next + 1;

  return TRUE;
}

GIBaseInfo *
g_base_info_get_container (GIBaseInfo *info)
{
  return info->container;
}

GTypelib *
g_base_info_get_typelib (GIBaseInfo *info)
{
  return info->typelib;
}

/* GIFunctionInfo functions */
const gchar *
g_function_info_get_symbol (GIFunctionInfo *info)
{
  GIBaseInfo *base = (GIBaseInfo *)info;
  FunctionBlob *blob = (FunctionBlob *)&base->typelib->data[base->offset];

  return g_typelib_get_string (base->typelib, blob->symbol);
}

GIFunctionInfoFlags
g_function_info_get_flags (GIFunctionInfo *info)
{
  GIFunctionInfoFlags flags;
  GIBaseInfo *base = (GIBaseInfo *)info;
  FunctionBlob *blob = (FunctionBlob *)&base->typelib->data[base->offset];
  
  flags = 0;

  /* Make sure we don't flag Constructors as methods */
  if (!blob->constructor && !blob->is_static)
    flags = flags | GI_FUNCTION_IS_METHOD;
    
  if (blob->constructor)
    flags = flags | GI_FUNCTION_IS_CONSTRUCTOR;

  if (blob->getter)
    flags = flags | GI_FUNCTION_IS_GETTER;

  if (blob->setter)
    flags = flags | GI_FUNCTION_IS_SETTER;

  if (blob->wraps_vfunc)
    flags = flags | GI_FUNCTION_WRAPS_VFUNC;

  if (blob->throws)
    flags = flags | GI_FUNCTION_THROWS;

  return flags;
}

GIPropertyInfo *
g_function_info_get_property (GIFunctionInfo *info)
{
  GIBaseInfo *base = (GIBaseInfo *)info;
  FunctionBlob *blob = (FunctionBlob *)&base->typelib->data[base->offset];
  GIInterfaceInfo *container = (GIInterfaceInfo *)base->container;
  
  return g_interface_info_get_property (container, blob->index);  
}

GIVFuncInfo *
g_function_info_get_vfunc (GIFunctionInfo *info)
{
  GIBaseInfo *base = (GIBaseInfo *)info;
  FunctionBlob *blob = (FunctionBlob *)&base->typelib->data[base->offset];
  GIInterfaceInfo *container = (GIInterfaceInfo *)base->container;
  
  return g_interface_info_get_vfunc (container, blob->index);  
}


/* GICallableInfo functions */
static guint32
signature_offset (GICallableInfo *info)
{
  int sigoff = -1;
  switch (info->base.type)
    {
    case GI_INFO_TYPE_FUNCTION:
      sigoff = G_STRUCT_OFFSET (FunctionBlob, signature);
      break;
    case GI_INFO_TYPE_VFUNC:
      sigoff = G_STRUCT_OFFSET (VFuncBlob, signature);
      break;
    case GI_INFO_TYPE_CALLBACK:
      sigoff = G_STRUCT_OFFSET (CallbackBlob, signature);
      break;
    case GI_INFO_TYPE_SIGNAL:
      sigoff = G_STRUCT_OFFSET (SignalBlob, signature);
      break;
    }
  if (sigoff >= 0)
    return *(guint32 *)&info->base.typelib->data[info->base.offset + sigoff];
  return 0;
}

GITypeInfo *
g_type_info_new (GIBaseInfo    *container,
		 GTypelib     *typelib,
		 guint32        offset)
{
  SimpleTypeBlob *type = (SimpleTypeBlob *)&typelib->data[offset];

  return (GITypeInfo *) g_info_new (GI_INFO_TYPE_TYPE, container, typelib, 
				    (type->reserved == 0 && type->reserved2 == 0) ? offset : type->offset);
}

/**
 * g_callable_info_get_return_type:
 * @info: a #GICallableInfo
 *
 * Get the return type of a callable item as
 * a #GITypeInfo
 *
 * Returns: a #GITypeInfo idexing the TypeBlob for the
 * return type of #info
 */
GITypeInfo *
g_callable_info_get_return_type (GICallableInfo *info)
{
  GIBaseInfo *base = (GIBaseInfo *)info;
  guint32 offset;

  offset = signature_offset (info);

  return g_type_info_new (base, base->typelib, offset);
}

/**
 * g_callable_info_may_return_null:
 * @info: a #GICallableInfo
 *
 * See if a callable could return NULL.
 *
 * Returns: TRUE if callable could return NULL
 */
gboolean
g_callable_info_may_return_null (GICallableInfo *info)
{
  GIBaseInfo *base = (GIBaseInfo *)info;
  SignatureBlob *blob = (SignatureBlob *)&base->typelib->data[signature_offset (info)];

  return blob->may_return_null;
}

/**
 * g_callable_info_get_caller_owns:
 * @info: a #GICallableInfo
 *
 * See whether the caller owns the return value
 * of this callable.
 *
 * Returns: TRUE if the caller owns the return value, FALSE otherwise.
 */
GITransfer
g_callable_info_get_caller_owns (GICallableInfo *info)
{
  GIBaseInfo *base = (GIBaseInfo *)info;
  SignatureBlob *blob = (SignatureBlob *)&base->typelib->data[signature_offset (info)];

  if (blob->caller_owns_return_value)
    return GI_TRANSFER_EVERYTHING;
  else if (blob->caller_owns_return_container)
    return GI_TRANSFER_CONTAINER;
  else
    return GI_TRANSFER_NOTHING;
}

/**
 * g_callable_info_get_n_args:
 * @info: a #GICallableInfo
 *
 * Get the number of arguments (both IN and OUT) for this callable.
 *
 * Returns: The number of arguments this callable expects.
 */
gint 
g_callable_info_get_n_args (GICallableInfo *info)
{
  GIBaseInfo *base = (GIBaseInfo *)info;
  gint offset;
  SignatureBlob *blob;

  offset = signature_offset (info);
  blob = (SignatureBlob *)&base->typelib->data[offset];

  return blob->n_arguments;
}

/**
 * g_callable_info_get_arg:
 * @info: a #GICallableInfo
 * @n: the argument index to fetch
 *
 * Get information about a particular argument of this callable.
 *
 * Returns: A #GIArgInfo indexing the typelib on the given argument.
 */
GIArgInfo *
g_callable_info_get_arg (GICallableInfo *info,
			 gint           n)
{
  GIBaseInfo *base = (GIBaseInfo *)info;
  Header *header = (Header *)base->typelib->data;
  gint offset;

  offset = signature_offset (info);
  
  return (GIArgInfo *) g_info_new (GI_INFO_TYPE_ARG, base, base->typelib, 
				   offset + header->signature_blob_size + n * header->arg_blob_size);
}

/* GIArgInfo function */
GIDirection
g_arg_info_get_direction (GIArgInfo *info)
{
  GIBaseInfo *base = (GIBaseInfo *)info;
  ArgBlob *blob = (ArgBlob *)&base->typelib->data[base->offset];
  
  if (blob->in && blob->out)
    return GI_DIRECTION_INOUT;
  else if (blob->out)
    return GI_DIRECTION_OUT;
  else
    return GI_DIRECTION_IN;
}

gboolean
g_arg_info_is_return_value (GIArgInfo *info)
{
  GIBaseInfo *base = (GIBaseInfo *)info;
  ArgBlob *blob = (ArgBlob *)&base->typelib->data[base->offset];
  
  return blob->return_value;
}

gboolean
g_arg_info_is_dipper (GIArgInfo *info)
{
  GIBaseInfo *base = (GIBaseInfo *)info;
  ArgBlob *blob = (ArgBlob *)&base->typelib->data[base->offset];
  
  return blob->dipper;
}

gboolean
g_arg_info_is_optional (GIArgInfo *info)
{
  GIBaseInfo *base = (GIBaseInfo *)info;
  ArgBlob *blob = (ArgBlob *)&base->typelib->data[base->offset];
  
  return blob->optional;
}

gboolean
g_arg_info_may_be_null (GIArgInfo *info)
{
  GIBaseInfo *base = (GIBaseInfo *)info;
  ArgBlob *blob = (ArgBlob *)&base->typelib->data[base->offset];
  
  return blob->allow_none;
}

GITransfer
g_arg_info_get_ownership_transfer (GIArgInfo *info)
{
  GIBaseInfo *base = (GIBaseInfo *)info;
  ArgBlob *blob = (ArgBlob *)&base->typelib->data[base->offset];

  if (blob->transfer_ownership)
    return GI_TRANSFER_EVERYTHING;
  else if (blob->transfer_container_ownership)
    return GI_TRANSFER_CONTAINER;
  else
    return GI_TRANSFER_NOTHING;
}

GIScopeType
g_arg_info_get_scope (GIArgInfo *info)
{
  GIBaseInfo *base = (GIBaseInfo *)info;
  ArgBlob *blob = (ArgBlob *)&base->typelib->data[base->offset];

  return blob->scope;
}

gint
g_arg_info_get_closure (GIArgInfo *info)
{
  GIBaseInfo *base = (GIBaseInfo *)info;
  ArgBlob *blob = (ArgBlob *)&base->typelib->data[base->offset];

  return blob->closure;
}

gint
g_arg_info_get_destroy (GIArgInfo *info)
{
  GIBaseInfo *base = (GIBaseInfo *)info;
  ArgBlob *blob = (ArgBlob *)&base->typelib->data[base->offset];

  return blob->destroy;
}

GITypeInfo *
g_arg_info_get_type (GIArgInfo *info)
{
  GIBaseInfo *base = (GIBaseInfo *)info;
 
  return g_type_info_new (base, base->typelib, base->offset + G_STRUCT_OFFSET (ArgBlob, arg_type));
}

/* GITypeInfo functions */
gboolean
g_type_info_is_pointer (GITypeInfo *info)
{
  GIBaseInfo *base = (GIBaseInfo *)info;
  SimpleTypeBlob *type = (SimpleTypeBlob *)&base->typelib->data[base->offset];
  
  if (type->reserved == 0 && type->reserved2 == 0)
    return type->pointer;
  else
    {
      InterfaceTypeBlob *iface = (InterfaceTypeBlob *)&base->typelib->data[base->offset];
      
      return iface->pointer;
    }
}

GITypeTag
g_type_info_get_tag (GITypeInfo *info)
{
  GIBaseInfo *base = (GIBaseInfo *)info;
  SimpleTypeBlob *type = (SimpleTypeBlob *)&base->typelib->data[base->offset];

  if (type->reserved == 0 && type->reserved2 == 0)
    return type->tag;
  else
    {
      InterfaceTypeBlob *iface = (InterfaceTypeBlob *)&base->typelib->data[base->offset];

      return iface->tag;
    }
}

GITypeInfo *
g_type_info_get_param_type (GITypeInfo *info,
			    gint       n)
{
  GIBaseInfo *base = (GIBaseInfo *)info;
  SimpleTypeBlob *type = (SimpleTypeBlob *)&base->typelib->data[base->offset];
  
  if (!(type->reserved == 0 && type->reserved2 == 0))
    {
      ParamTypeBlob *param = (ParamTypeBlob *)&base->typelib->data[base->offset];

      switch (param->tag)
	{
	case GI_TYPE_TAG_ARRAY: 
	case GI_TYPE_TAG_GLIST:
	case GI_TYPE_TAG_GSLIST:
	case GI_TYPE_TAG_GHASH:
	  return g_type_info_new (base, base->typelib,
	                          base->offset + sizeof (ParamTypeBlob)
	                          + sizeof (SimpleTypeBlob) * n);
	  break;
	  
	default: ;
	}
    }
      
  return NULL;
}

GIBaseInfo *
g_type_info_get_interface (GITypeInfo *info)
{
  GIBaseInfo *base = (GIBaseInfo *)info;
  SimpleTypeBlob *type = (SimpleTypeBlob *)&base->typelib->data[base->offset];
  
  if (!(type->reserved == 0 && type->reserved2 == 0))
    {
      InterfaceTypeBlob *blob = (InterfaceTypeBlob *)&base->typelib->data[base->offset];
      
      if (blob->tag == GI_TYPE_TAG_INTERFACE)
	return g_info_from_entry (base->repository, base->typelib, blob->interface);
    }

  return NULL;
}

gint
g_type_info_get_array_length (GITypeInfo *info)
{
  GIBaseInfo *base = (GIBaseInfo *)info;
  SimpleTypeBlob *type = (SimpleTypeBlob *)&base->typelib->data[base->offset];
  
  if (!(type->reserved == 0 && type->reserved2 == 0))
    {
      ArrayTypeBlob *blob = (ArrayTypeBlob *)&base->typelib->data[base->offset];

      if (blob->tag == GI_TYPE_TAG_ARRAY)
	{
	  if (blob->has_length)
	    return blob->length;
	}
    }

  return -1;
}

gint
g_type_info_get_array_fixed_size (GITypeInfo *info)
{
  GIBaseInfo *base = (GIBaseInfo *)info;
  SimpleTypeBlob *type = (SimpleTypeBlob *)&base->typelib->data[base->offset];
  
  if (!(type->reserved == 0 && type->reserved2 == 0))
    {
      ArrayTypeBlob *blob = (ArrayTypeBlob *)&base->typelib->data[base->offset];

      if (blob->tag == GI_TYPE_TAG_ARRAY)
	{
	  if (blob->has_size)
	    return blob->size;
	}
    }

  return -1;
}

gboolean
g_type_info_is_zero_terminated (GITypeInfo *info)
{
  GIBaseInfo *base = (GIBaseInfo *)info;
  SimpleTypeBlob *type = (SimpleTypeBlob *)&base->typelib->data[base->offset];
  
  if (!(type->reserved == 0 && type->reserved2 == 0))
    {
      ArrayTypeBlob *blob = (ArrayTypeBlob *)&base->typelib->data[base->offset];

      if (blob->tag == GI_TYPE_TAG_ARRAY)
	return blob->zero_terminated;
    }

  return FALSE;
}

gint
g_type_info_get_n_error_domains (GITypeInfo *info)
{
  GIBaseInfo *base = (GIBaseInfo *)info;
  SimpleTypeBlob *type = (SimpleTypeBlob *)&base->typelib->data[base->offset];
  
  if (!(type->reserved == 0 && type->reserved2 == 0))
    {
      ErrorTypeBlob *blob = (ErrorTypeBlob *)&base->typelib->data[base->offset];

      if (blob->tag == GI_TYPE_TAG_ERROR)
	return blob->n_domains;
    }

  return 0;
}

GIErrorDomainInfo *
g_type_info_get_error_domain (GITypeInfo *info,
			      gint       n)
{
  GIBaseInfo *base = (GIBaseInfo *)info;
  SimpleTypeBlob *type = (SimpleTypeBlob *)&base->typelib->data[base->offset];
  
  if (!(type->reserved == 0 && type->reserved2 == 0))
    {
      ErrorTypeBlob *blob = (ErrorTypeBlob *)&base->typelib->data[base->offset];

      if (blob->tag == GI_TYPE_TAG_ERROR)
	return (GIErrorDomainInfo *) g_info_from_entry (base->repository,
							base->typelib,
							blob->domains[n]);
    }

  return NULL;
}


/* GIErrorDomainInfo functions */
const gchar *
g_error_domain_info_get_quark (GIErrorDomainInfo *info)
{
  GIBaseInfo *base = (GIBaseInfo *)info;
  ErrorDomainBlob *blob = (ErrorDomainBlob *)&base->typelib->data[base->offset];

  return g_typelib_get_string (base->typelib, blob->get_quark);
}

GIInterfaceInfo *
g_error_domain_info_get_codes (GIErrorDomainInfo *info)
{
  GIBaseInfo *base = (GIBaseInfo *)info;
  ErrorDomainBlob *blob = (ErrorDomainBlob *)&base->typelib->data[base->offset];
  
  return (GIInterfaceInfo *) g_info_from_entry (base->repository,
						base->typelib, blob->error_codes);
}


/* GIValueInfo functions */ 
glong
g_value_info_get_value (GIValueInfo *info)
{
  GIBaseInfo *base = (GIBaseInfo *)info;
  ValueBlob *blob = (ValueBlob *)&base->typelib->data[base->offset];

  return (glong)blob->value;
}

/* GIFieldInfo functions */
GIFieldInfoFlags
g_field_info_get_flags (GIFieldInfo *info)
{
  GIFieldInfoFlags flags;

  GIBaseInfo *base = (GIBaseInfo *)info;
  FieldBlob *blob = (FieldBlob *)&base->typelib->data[base->offset];

  flags = 0;

  if (blob->readable)
    flags = flags | GI_FIELD_IS_READABLE;

  if (blob->writable)
    flags = flags | GI_FIELD_IS_WRITABLE;

  return flags;
}

gint
g_field_info_get_size (GIFieldInfo *info)
{
  GIBaseInfo *base = (GIBaseInfo *)info;
  FieldBlob *blob = (FieldBlob *)&base->typelib->data[base->offset];
  
  return blob->bits;
}

gint
g_field_info_get_offset (GIFieldInfo *info)
{
  GIBaseInfo *base = (GIBaseInfo *)info;
  FieldBlob *blob = (FieldBlob *)&base->typelib->data[base->offset];
  
  return blob->struct_offset;
}

GITypeInfo *
g_field_info_get_type (GIFieldInfo *info)
{
  GIBaseInfo *base = (GIBaseInfo *)info;
  
  return g_type_info_new (base, base->typelib, base->offset + G_STRUCT_OFFSET (FieldBlob, type));
}

/* GIRegisteredTypeInfo functions */
const gchar *
g_registered_type_info_get_type_name (GIRegisteredTypeInfo *info)
{
  GIBaseInfo *base = (GIBaseInfo *)info;
  RegisteredTypeBlob *blob = (RegisteredTypeBlob *)&base->typelib->data[base->offset];

  if (blob->gtype_name)
    return g_typelib_get_string (base->typelib, blob->gtype_name);

  return NULL;
}

const gchar *
g_registered_type_info_get_type_init (GIRegisteredTypeInfo *info)
{
  GIBaseInfo *base = (GIBaseInfo *)info;
  RegisteredTypeBlob *blob = (RegisteredTypeBlob *)&base->typelib->data[base->offset];

  if (blob->gtype_init)
    return g_typelib_get_string (base->typelib, blob->gtype_init);

  return NULL;
}

GType
g_registered_type_info_get_g_type (GIRegisteredTypeInfo *info)
{
  const char *type_init;
  GType (* get_type_func) (void);

  type_init = g_registered_type_info_get_type_init (info);  
  
  if (type_init == NULL)
    return G_TYPE_NONE;
  else if (!strcmp (type_init, "intern"))
    return G_TYPE_OBJECT;
  
  get_type_func = NULL;
  if (!g_typelib_symbol (((GIBaseInfo*)info)->typelib,
                         type_init,
                         (void**) &get_type_func))
    return G_TYPE_NONE;
  
  return (* get_type_func) ();
}

/* GIStructInfo functions */
gint
g_struct_info_get_n_fields (GIStructInfo *info)
{
  GIBaseInfo *base = (GIBaseInfo *)info;
  StructBlob *blob = (StructBlob *)&base->typelib->data[base->offset];
  
  return blob->n_fields;
}

GIFieldInfo *
g_struct_info_get_field (GIStructInfo *info,
			 gint         n)
{
  GIBaseInfo *base = (GIBaseInfo *)info;
  Header *header = (Header *)base->typelib->data;  
 
  return (GIFieldInfo *) g_info_new (GI_INFO_TYPE_FIELD, base, base->typelib, 
				     base->offset + header->struct_blob_size + 
				     n * header->field_blob_size);
}

gint
g_struct_info_get_n_methods (GIStructInfo *info)
{
  GIBaseInfo *base = (GIBaseInfo *)info;
  StructBlob *blob = (StructBlob *)&base->typelib->data[base->offset];
  
  return blob->n_methods;
}

GIFunctionInfo *
g_struct_info_get_method (GIStructInfo *info,
			  gint         n)
{
  GIBaseInfo *base = (GIBaseInfo *)info;
  StructBlob *blob = (StructBlob *)&base->typelib->data[base->offset];
  Header *header = (Header *)base->typelib->data;  
  gint offset;

  offset = base->offset + header->struct_blob_size 
    + blob->n_fields * header->field_blob_size 
    + n * header->function_blob_size;
  return (GIFunctionInfo *) g_info_new (GI_INFO_TYPE_FUNCTION, base, 
					base->typelib, offset);
}

static GIFunctionInfo *
find_method (GIBaseInfo   *base,
	     guint32       offset,
	     gint          n_methods,
	     const gchar  *name)
{
  /* FIXME hash */
  Header *header = (Header *)base->typelib->data;  
  gint i;

  for (i = 0; i < n_methods; i++)
    {
      FunctionBlob *fblob = (FunctionBlob *)&base->typelib->data[offset];
      const gchar *fname = (const gchar *)&base->typelib->data[fblob->name];

      if (strcmp (name, fname) == 0)
	return (GIFunctionInfo *) g_info_new (GI_INFO_TYPE_FUNCTION, base, 
					      base->typelib, offset);  
      
      offset += header->function_blob_size;
    }
      
  return NULL;
}

GIFunctionInfo *
g_struct_info_find_method (GIStructInfo *info,
			   const gchar  *name)
{
  gint offset;
  GIBaseInfo *base = (GIBaseInfo *)info;
  Header *header = (Header *)base->typelib->data;  
  StructBlob *blob = (StructBlob *)&base->typelib->data[base->offset];

  offset = base->offset + header->struct_blob_size
    + blob->n_fields * header->field_blob_size;

  return find_method (base, offset, blob->n_methods, name);
}

gsize
g_struct_info_get_size (GIStructInfo *info)
{
  GIBaseInfo *base = (GIBaseInfo *)info;
  StructBlob *blob = (StructBlob *)&base->typelib->data[base->offset];

  return blob->size;
}

gsize
g_struct_info_get_alignment (GIStructInfo *info)
{
  GIBaseInfo *base = (GIBaseInfo *)info;
  StructBlob *blob = (StructBlob *)&base->typelib->data[base->offset];

  return blob->alignment;
}

/**
 * g_struct_info_is_gtype_struct:
 * @info: GIStructInfo
 * 
 * Return true if this structure represents the "class structure" for some
 * #GObject or #GInterface.  This function is mainly useful to hide this kind of structure
 * from generated public APIs.
 *
 * Returns: %TRUE if this is a class struct, %FALSE otherwise
 */
gboolean
g_struct_info_is_gtype_struct (GIStructInfo *info)
{
  GIBaseInfo *base = (GIBaseInfo *)info;
  StructBlob *blob = (StructBlob *)&base->typelib->data[base->offset];

  return blob->is_gtype_struct;
}

gint
g_enum_info_get_n_values (GIEnumInfo *info)
{
  GIBaseInfo *base = (GIBaseInfo *)info;
  EnumBlob *blob = (EnumBlob *)&base->typelib->data[base->offset];

  return blob->n_values;
}

GIValueInfo *
g_enum_info_get_value (GIEnumInfo *info,
		       gint            n)
{
  GIBaseInfo *base = (GIBaseInfo *)info;
  Header *header = (Header *)base->typelib->data;  
  gint offset;

  offset = base->offset + header->enum_blob_size 
    + n * header->value_blob_size;
  return (GIValueInfo *) g_info_new (GI_INFO_TYPE_VALUE, base, base->typelib, offset);
}

/**
 * g_enum_info_get_storage_type:
 * @info: GIEnumInfo
 *
 * Gets the tag of the type used for the enum in the C ABI. This will
 * will be a signed or unsigned integral type.

 * Note that in the current implementation the width of the type is
 * computed correctly, but the signed or unsigned nature of the type
 * may not match the sign of the type used by the C compiler.
 *
 * Return Value: the storage type for the enumeration
 */
GITypeTag
g_enum_info_get_storage_type (GIEnumInfo *info)
{
  GIBaseInfo *base = (GIBaseInfo *)info;
  EnumBlob *blob = (EnumBlob *)&base->typelib->data[base->offset];

  return blob->storage_type;
}

/* GIObjectInfo functions */
GIObjectInfo *
g_object_info_get_parent (GIObjectInfo *info)
{
  GIBaseInfo *base = (GIBaseInfo *)info;
  ObjectBlob *blob = (ObjectBlob *)&base->typelib->data[base->offset];

  if (blob->parent)
    return (GIObjectInfo *) g_info_from_entry (base->repository,
					       base->typelib, blob->parent);
  else
    return NULL;
}

gboolean
g_object_info_get_abstract (GIObjectInfo    *info)
{
  GIBaseInfo *base = (GIBaseInfo *)info;
  ObjectBlob *blob = (ObjectBlob *)&base->typelib->data[base->offset];
  return blob->abstract != 0;
}

const gchar *
g_object_info_get_type_name (GIObjectInfo *info)
{
  GIBaseInfo *base = (GIBaseInfo *)info;
  ObjectBlob *blob = (ObjectBlob *)&base->typelib->data[base->offset];

  return g_typelib_get_string (base->typelib, blob->gtype_name);
}

const gchar *
g_object_info_get_type_init (GIObjectInfo *info)
{
  GIBaseInfo *base = (GIBaseInfo *)info;
  ObjectBlob *blob = (ObjectBlob *)&base->typelib->data[base->offset];

  return g_typelib_get_string (base->typelib, blob->gtype_init);
}

gint
g_object_info_get_n_interfaces (GIObjectInfo *info)
{
  GIBaseInfo *base = (GIBaseInfo *)info;
  ObjectBlob *blob = (ObjectBlob *)&base->typelib->data[base->offset];

  return blob->n_interfaces;
}

GIInterfaceInfo *
g_object_info_get_interface (GIObjectInfo *info,
			     gint          n)
{
  GIBaseInfo *base = (GIBaseInfo *)info;
  ObjectBlob *blob = (ObjectBlob *)&base->typelib->data[base->offset];

  return (GIInterfaceInfo *) g_info_from_entry (base->repository,
						base->typelib, blob->interfaces[n]);
}

gint
g_object_info_get_n_fields (GIObjectInfo *info)
{
  GIBaseInfo *base = (GIBaseInfo *)info;
  ObjectBlob *blob = (ObjectBlob *)&base->typelib->data[base->offset];

  return blob->n_fields;
}

GIFieldInfo *
g_object_info_get_field (GIObjectInfo *info,
			 gint          n)
{
  gint offset;
  GIBaseInfo *base = (GIBaseInfo *)info;
  Header *header = (Header *)base->typelib->data;  
  ObjectBlob *blob = (ObjectBlob *)&base->typelib->data[base->offset];
  
  offset = base->offset + header->object_blob_size
    + (blob->n_interfaces + blob->n_interfaces % 2) * 2
    + n * header->field_blob_size;
  
  return (GIFieldInfo *) g_info_new (GI_INFO_TYPE_FIELD, base, base->typelib, offset);
}

gint
g_object_info_get_n_properties (GIObjectInfo *info)
{
  GIBaseInfo *base = (GIBaseInfo *)info;
  ObjectBlob *blob = (ObjectBlob *)&base->typelib->data[base->offset];

  return blob->n_properties;  
}

GIPropertyInfo *
g_object_info_get_property (GIObjectInfo *info,
			    gint          n)
{
  gint offset;
  GIBaseInfo *base = (GIBaseInfo *)info;
  Header *header = (Header *)base->typelib->data;  
  ObjectBlob *blob = (ObjectBlob *)&base->typelib->data[base->offset];
  
  offset = base->offset + header->object_blob_size
    + (blob->n_interfaces + blob->n_interfaces % 2) * 2
    + blob->n_fields * header->field_blob_size
    + n * header->property_blob_size;

  return (GIPropertyInfo *) g_info_new (GI_INFO_TYPE_PROPERTY, base, 
					base->typelib, offset);
}

gint
g_object_info_get_n_methods (GIObjectInfo *info)
{
  GIBaseInfo *base = (GIBaseInfo *)info;
  ObjectBlob *blob = (ObjectBlob *)&base->typelib->data[base->offset];

  return blob->n_methods;
}

GIFunctionInfo *
g_object_info_get_method (GIObjectInfo *info,
			  gint          n)
{
  gint offset;
  GIBaseInfo *base = (GIBaseInfo *)info;
  Header *header = (Header *)base->typelib->data;  
  ObjectBlob *blob = (ObjectBlob *)&base->typelib->data[base->offset];
  
  offset = base->offset + header->object_blob_size
    + (blob->n_interfaces + blob->n_interfaces % 2) * 2
    + blob->n_fields * header->field_blob_size
    + blob->n_properties * header->property_blob_size
    + n * header->function_blob_size;

    return (GIFunctionInfo *) g_info_new (GI_INFO_TYPE_FUNCTION, base, 
					  base->typelib, offset);  
}

GIFunctionInfo *
g_object_info_find_method (GIObjectInfo *info,
			   const gchar  *name)
{
  gint offset;
  GIBaseInfo *base = (GIBaseInfo *)info;
  Header *header = (Header *)base->typelib->data;  
  ObjectBlob *blob = (ObjectBlob *)&base->typelib->data[base->offset];

  offset = base->offset + header->object_blob_size
    + (blob->n_interfaces + blob->n_interfaces % 2) * 2
    + blob->n_fields * header->field_blob_size +
    + blob->n_properties * header->property_blob_size;

  return find_method (base, offset, blob->n_methods, name);
}

gint
g_object_info_get_n_signals (GIObjectInfo *info)
{
  GIBaseInfo *base = (GIBaseInfo *)info;
  ObjectBlob *blob = (ObjectBlob *)&base->typelib->data[base->offset];

  return blob->n_signals;
}

GISignalInfo *
g_object_info_get_signal (GIObjectInfo *info,
			  gint          n)
{
  gint offset;
  GIBaseInfo *base = (GIBaseInfo *)info;
  Header *header = (Header *)base->typelib->data;  
  ObjectBlob *blob = (ObjectBlob *)&base->typelib->data[base->offset];
  
  offset = base->offset + header->object_blob_size
    + (blob->n_interfaces + blob->n_interfaces % 2) * 2
    + blob->n_fields * header->field_blob_size
    + blob->n_properties * header->property_blob_size
    + blob->n_methods * header->function_blob_size 
    + n * header->signal_blob_size;

  return (GISignalInfo *) g_info_new (GI_INFO_TYPE_SIGNAL, base, 
				      base->typelib, offset);  
}

gint
g_object_info_get_n_vfuncs (GIObjectInfo *info)
{
  GIBaseInfo *base = (GIBaseInfo *)info;
  ObjectBlob *blob = (ObjectBlob *)&base->typelib->data[base->offset];
  
  return blob->n_vfuncs;
}

GIVFuncInfo *
g_object_info_get_vfunc (GIObjectInfo *info,
			 gint          n)
{
  gint offset;
  GIBaseInfo *base = (GIBaseInfo *)info;
  Header *header = (Header *)base->typelib->data;  
  ObjectBlob *blob = (ObjectBlob *)&base->typelib->data[base->offset];
  
  offset = base->offset + header->object_blob_size
    + (blob->n_interfaces + blob->n_interfaces % 2) * 2
    + blob->n_fields * header->field_blob_size
    + blob->n_properties * header->property_blob_size
    + blob->n_methods * header->function_blob_size 
    + blob->n_signals * header->signal_blob_size 
    + n * header->vfunc_blob_size;

  return (GIVFuncInfo *) g_info_new (GI_INFO_TYPE_VFUNC, base, 
				     base->typelib, offset);  
}

gint
g_object_info_get_n_constants (GIObjectInfo *info)
{
  GIBaseInfo *base = (GIBaseInfo *)info;
  ObjectBlob *blob = (ObjectBlob *)&base->typelib->data[base->offset];
  
  return blob->n_constants;
}

GIConstantInfo *
g_object_info_get_constant (GIObjectInfo *info,
			    gint          n)
{
  gint offset;
  GIBaseInfo *base = (GIBaseInfo *)info;
  Header *header = (Header *)base->typelib->data;  
  ObjectBlob *blob = (ObjectBlob *)&base->typelib->data[base->offset];
  
  offset = base->offset + header->object_blob_size
    + (blob->n_interfaces + blob->n_interfaces % 2) * 2
    + blob->n_fields * header->field_blob_size
    + blob->n_properties * header->property_blob_size
    + blob->n_methods * header->function_blob_size 
    + blob->n_signals * header->signal_blob_size 
    + blob->n_vfuncs * header->vfunc_blob_size 
    + n * header->constant_blob_size;

  return (GIConstantInfo *) g_info_new (GI_INFO_TYPE_CONSTANT, base, 
					base->typelib, offset);  
}

/**
 * g_object_info_get_class_struct:
 * @info: A #GIObjectInfo to query
 * 
 * Every #GObject has two structures; an instance structure and a class
 * structure.  This function returns the metadata for the class structure.
 *
 * Returns: a #GIStructInfo for the class struct or %NULL if none found.
 */
GIStructInfo *
g_object_info_get_class_struct (GIObjectInfo *info)
{
  GIBaseInfo *base = (GIBaseInfo *)info;
  ObjectBlob *blob = (ObjectBlob *)&base->typelib->data[base->offset];

  if (blob->gtype_struct)
    return (GIStructInfo *) g_info_from_entry (base->repository,
                                               base->typelib, blob->gtype_struct);
  else
    return NULL;
}

/* GIInterfaceInfo functions */
gint
g_interface_info_get_n_prerequisites (GIInterfaceInfo *info)
{
  GIBaseInfo *base = (GIBaseInfo *)info;
  InterfaceBlob *blob = (InterfaceBlob *)&base->typelib->data[base->offset];

  return blob->n_prerequisites;
}

GIBaseInfo *
g_interface_info_get_prerequisite (GIInterfaceInfo *info,
				   gint            n)
{
  GIBaseInfo *base = (GIBaseInfo *)info;
  InterfaceBlob *blob = (InterfaceBlob *)&base->typelib->data[base->offset];

  return g_info_from_entry (base->repository,
			    base->typelib, blob->prerequisites[n]);
}


gint
g_interface_info_get_n_properties (GIInterfaceInfo *info)
{
  GIBaseInfo *base = (GIBaseInfo *)info;
  InterfaceBlob *blob = (InterfaceBlob *)&base->typelib->data[base->offset];

  return blob->n_properties;  
}

GIPropertyInfo *
g_interface_info_get_property (GIInterfaceInfo *info,
			       gint            n)
{
  gint offset;
  GIBaseInfo *base = (GIBaseInfo *)info;
  Header *header = (Header *)base->typelib->data;  
  InterfaceBlob *blob = (InterfaceBlob *)&base->typelib->data[base->offset];
  
  offset = base->offset + header->interface_blob_size
    + (blob->n_prerequisites + (blob->n_prerequisites % 2)) * 2
    + n * header->property_blob_size;

  return (GIPropertyInfo *) g_info_new (GI_INFO_TYPE_PROPERTY, base, 
					base->typelib, offset);
}

gint
g_interface_info_get_n_methods (GIInterfaceInfo *info)
{
  GIBaseInfo *base = (GIBaseInfo *)info;
  InterfaceBlob *blob = (InterfaceBlob *)&base->typelib->data[base->offset];

  return blob->n_methods;
}

GIFunctionInfo *
g_interface_info_get_method (GIInterfaceInfo *info,
			     gint            n)
{
  gint offset;
  GIBaseInfo *base = (GIBaseInfo *)info;
  Header *header = (Header *)base->typelib->data;  
  InterfaceBlob *blob = (InterfaceBlob *)&base->typelib->data[base->offset];
  
  offset = base->offset + header->interface_blob_size
    + (blob->n_prerequisites + (blob->n_prerequisites % 2)) * 2
    + blob->n_properties * header->property_blob_size 
    + n * header->function_blob_size;
  
  return (GIFunctionInfo *) g_info_new (GI_INFO_TYPE_FUNCTION, base, 
					base->typelib, offset);  
}

GIFunctionInfo *
g_interface_info_find_method (GIInterfaceInfo *info,
			      const gchar     *name)
{
  gint offset;
  GIBaseInfo *base = (GIBaseInfo *)info;
  Header *header = (Header *)base->typelib->data;  
  InterfaceBlob *blob = (InterfaceBlob *)&base->typelib->data[base->offset];

  offset = base->offset + header->interface_blob_size
    + (blob->n_prerequisites + (blob->n_prerequisites % 2)) * 2
    + blob->n_properties * header->property_blob_size;

  return find_method (base, offset, blob->n_methods, name);
}

gint
g_interface_info_get_n_signals (GIInterfaceInfo *info)
{
  GIBaseInfo *base = (GIBaseInfo *)info;
  InterfaceBlob *blob = (InterfaceBlob *)&base->typelib->data[base->offset];

  return blob->n_signals;
}

GISignalInfo *
g_interface_info_get_signal (GIInterfaceInfo *info,
			     gint            n)
{
  gint offset;
  GIBaseInfo *base = (GIBaseInfo *)info;
  Header *header = (Header *)base->typelib->data;  
  InterfaceBlob *blob = (InterfaceBlob *)&base->typelib->data[base->offset];
  
  offset = base->offset + header->interface_blob_size 
    + (blob->n_prerequisites + (blob->n_prerequisites % 2)) * 2
    + blob->n_properties * header->property_blob_size 
    + blob->n_methods * header->function_blob_size 
    + n * header->signal_blob_size;
  
  return (GISignalInfo *) g_info_new (GI_INFO_TYPE_SIGNAL, base, 
				      base->typelib, offset);  
}

gint
g_interface_info_get_n_vfuncs (GIInterfaceInfo *info)
{
  GIBaseInfo *base = (GIBaseInfo *)info;
  InterfaceBlob *blob = (InterfaceBlob *)&base->typelib->data[base->offset];

  return blob->n_vfuncs;
}

GIVFuncInfo *
g_interface_info_get_vfunc (GIInterfaceInfo *info,
			    gint            n)
{
  gint offset;
  GIBaseInfo *base = (GIBaseInfo *)info;
  Header *header = (Header *)base->typelib->data;  
  InterfaceBlob *blob = (InterfaceBlob *)&base->typelib->data[base->offset];
  
  offset = base->offset + header->interface_blob_size 
    + (blob->n_prerequisites + (blob->n_prerequisites % 2)) * 2
    + blob->n_properties * header->property_blob_size 
    + blob->n_methods * header->function_blob_size 
    + blob->n_signals * header->signal_blob_size
    + n * header->vfunc_blob_size;
  
  return (GIVFuncInfo *) g_info_new (GI_INFO_TYPE_VFUNC, base, 
				     base->typelib, offset);  
}

gint
g_interface_info_get_n_constants (GIInterfaceInfo *info)
{
  GIBaseInfo *base = (GIBaseInfo *)info;
  InterfaceBlob *blob = (InterfaceBlob *)&base->typelib->data[base->offset];
  
  return blob->n_constants;
}

GIConstantInfo *
g_interface_info_get_constant (GIInterfaceInfo *info,
			       gint             n)
{
  gint offset;
  GIBaseInfo *base = (GIBaseInfo *)info;
  Header *header = (Header *)base->typelib->data;  
  InterfaceBlob *blob = (InterfaceBlob *)&base->typelib->data[base->offset];
  
  offset = base->offset + header->interface_blob_size
    + (blob->n_prerequisites + (blob->n_prerequisites % 2)) * 2
    + blob->n_properties * header->property_blob_size
    + blob->n_methods * header->function_blob_size 
    + blob->n_signals * header->signal_blob_size 
    + blob->n_vfuncs * header->vfunc_blob_size 
    + n * header->constant_blob_size;

  return (GIConstantInfo *) g_info_new (GI_INFO_TYPE_CONSTANT, base, 
					base->typelib, offset);  
}

/**
 * g_interface_info_get_iface_struct:
 * @info: A #GIInterfaceInfo to query
 *
 * Returns the layout C structure associated with this #GInterface.
 *
 * Returns: A #GIStructInfo for the class struct or %NULL if none found.
 */
GIStructInfo *
g_interface_info_get_iface_struct (GIInterfaceInfo *info)
{
  GIBaseInfo *base = (GIBaseInfo *)info;
  InterfaceBlob *blob = (InterfaceBlob *)&base->typelib->data[base->offset];

  if (blob->gtype_struct)
    return (GIStructInfo *) g_info_from_entry (base->repository,
                                               base->typelib, blob->gtype_struct);
  else
    return NULL;
}

/* GIPropertyInfo functions */
GParamFlags
g_property_info_get_flags (GIPropertyInfo *info)
{
  GParamFlags flags;
  GIBaseInfo *base = (GIBaseInfo *)info;
  PropertyBlob *blob = (PropertyBlob *)&base->typelib->data[base->offset];
  
  flags = 0;

  if (blob->readable)
    flags = flags | G_PARAM_READABLE;

  if (blob->writable)
    flags = flags | G_PARAM_WRITABLE;

  if (blob->construct)
    flags = flags | G_PARAM_CONSTRUCT;

  if (blob->construct_only)
    flags = flags | G_PARAM_CONSTRUCT_ONLY;

  return flags;
}

GITypeInfo *
g_property_info_get_type (GIPropertyInfo *info)
{
  GIBaseInfo *base = (GIBaseInfo *)info;

  return g_type_info_new (base, base->typelib, base->offset + G_STRUCT_OFFSET (PropertyBlob, type));
}


/* GISignalInfo functions */
GSignalFlags
g_signal_info_get_flags (GISignalInfo *info)
{
  GSignalFlags flags;

  GIBaseInfo *base = (GIBaseInfo *)info;
  SignalBlob *blob = (SignalBlob *)&base->typelib->data[base->offset];

  flags = 0;

  if (blob->run_first)
    flags = flags | G_SIGNAL_RUN_FIRST;

  if (blob->run_last)
    flags = flags | G_SIGNAL_RUN_LAST;

  if (blob->run_cleanup)
    flags = flags | G_SIGNAL_RUN_CLEANUP;

  if (blob->no_recurse)
    flags = flags | G_SIGNAL_NO_RECURSE;

  if (blob->detailed)
    flags = flags | G_SIGNAL_DETAILED;

  if (blob->action)
    flags = flags | G_SIGNAL_ACTION;

  if (blob->no_hooks)
    flags = flags | G_SIGNAL_NO_HOOKS;

  return flags;
}

GIVFuncInfo *
g_signal_info_get_class_closure (GISignalInfo *info)
{
  GIBaseInfo *base = (GIBaseInfo *)info;
  SignalBlob *blob = (SignalBlob *)&base->typelib->data[base->offset];

  if (blob->has_class_closure)
    return g_interface_info_get_vfunc ((GIInterfaceInfo *)base->container, blob->class_closure);

  return NULL;
}

gboolean
g_signal_info_true_stops_emit (GISignalInfo *info)
{
  GIBaseInfo *base = (GIBaseInfo *)info;
  SignalBlob *blob = (SignalBlob *)&base->typelib->data[base->offset];

  return blob->true_stops_emit;
}

/* GIVFuncInfo functions */
GIVFuncInfoFlags
g_vfunc_info_get_flags (GIVFuncInfo *info)
{
  GIVFuncInfoFlags flags;

  GIBaseInfo *base = (GIBaseInfo *)info;
  VFuncBlob *blob = (VFuncBlob *)&base->typelib->data[base->offset];

  flags = 0;

  if (blob->must_chain_up)
    flags = flags | GI_VFUNC_MUST_CHAIN_UP;

  if (blob->must_be_implemented)
    flags = flags | GI_VFUNC_MUST_OVERRIDE;

  if (blob->must_not_be_implemented)
    flags = flags | GI_VFUNC_MUST_NOT_OVERRIDE;

  return flags;
}

gint
g_vfunc_info_get_offset (GIVFuncInfo *info)
{
  GIBaseInfo *base = (GIBaseInfo *)info;
  VFuncBlob *blob = (VFuncBlob *)&base->typelib->data[base->offset];
  
  return blob->struct_offset;
}

GISignalInfo *
g_vfunc_info_get_signal (GIVFuncInfo *info)
{
  GIBaseInfo *base = (GIBaseInfo *)info;
  VFuncBlob *blob = (VFuncBlob *)&base->typelib->data[base->offset];

  if (blob->class_closure)
    return g_interface_info_get_signal ((GIInterfaceInfo *)base->container, blob->signal);
  
  return NULL;
}


/* GIConstantInfo functions */
GITypeInfo *
g_constant_info_get_type (GIConstantInfo *info)
{
  GIBaseInfo *base = (GIBaseInfo *)info;
  
  return g_type_info_new (base, base->typelib, base->offset + 8);
}

gint
g_constant_info_get_value (GIConstantInfo *info, 
			   GArgument      *value)
{
  GIBaseInfo *base = (GIBaseInfo *)info;
  ConstantBlob *blob = (ConstantBlob *)&base->typelib->data[base->offset];

  /* FIXME non-basic types ? */
  if (blob->type.reserved == 0 && blob->type.reserved2 == 0)
    {
      if (blob->type.pointer)
	value->v_pointer = g_memdup (&base->typelib->data[blob->offset], blob->size);
      else
	{
	  switch (blob->type.tag)
	    {
	    case GI_TYPE_TAG_BOOLEAN:
	      value->v_boolean = *(gboolean*)&base->typelib->data[blob->offset];
	      break;
	    case GI_TYPE_TAG_INT8:
	      value->v_int8 = *(gint8*)&base->typelib->data[blob->offset];
	      break;
	    case GI_TYPE_TAG_UINT8:
	      value->v_uint8 = *(guint8*)&base->typelib->data[blob->offset];
	      break;
	    case GI_TYPE_TAG_INT16:
	      value->v_int16 = *(gint16*)&base->typelib->data[blob->offset];
	      break;
	    case GI_TYPE_TAG_UINT16:
	      value->v_uint16 = *(guint16*)&base->typelib->data[blob->offset];
	      break;
	    case GI_TYPE_TAG_INT32:
	      value->v_int32 = *(gint32*)&base->typelib->data[blob->offset];
	      break;
	    case GI_TYPE_TAG_UINT32:
	      value->v_uint32 = *(guint32*)&base->typelib->data[blob->offset];
	      break;
	    case GI_TYPE_TAG_INT64:
	      value->v_int64 = *(gint64*)&base->typelib->data[blob->offset];
	      break;
	    case GI_TYPE_TAG_UINT64:
	      value->v_uint64 = *(guint64*)&base->typelib->data[blob->offset];
	      break;
	    case GI_TYPE_TAG_FLOAT:
	      value->v_float = *(gfloat*)&base->typelib->data[blob->offset];
	      break;
	    case GI_TYPE_TAG_DOUBLE:
	      value->v_double = *(gdouble*)&base->typelib->data[blob->offset];
	      break;
	    case GI_TYPE_TAG_TIME_T:
	      value->v_long = *(long*)&base->typelib->data[blob->offset];
	      break;
	    case GI_TYPE_TAG_INT:
	      value->v_int = *(gint*)&base->typelib->data[blob->offset];
	      break;
	    case GI_TYPE_TAG_UINT:
	      value->v_uint = *(guint*)&base->typelib->data[blob->offset];
	      break;
	    case GI_TYPE_TAG_LONG:
	      value->v_long = *(glong*)&base->typelib->data[blob->offset];
	      break;
	    case GI_TYPE_TAG_ULONG:
	      value->v_ulong = *(gulong*)&base->typelib->data[blob->offset];
	      break;
	    }
	}
    }

  return blob->size;
}

/* GIUnionInfo functions */
gint
g_union_info_get_n_fields  (GIUnionInfo *info)
{
  GIBaseInfo *base = (GIBaseInfo *)info;
  UnionBlob *blob = (UnionBlob *)&base->typelib->data[base->offset];
  
  return blob->n_fields;
}

GIFieldInfo *
g_union_info_get_field (GIUnionInfo *info,
			gint         n)
{
  GIBaseInfo *base = (GIBaseInfo *)info;
  Header *header = (Header *)base->typelib->data;  
 
  return (GIFieldInfo *) g_info_new (GI_INFO_TYPE_FIELD, base, base->typelib, 
				     base->offset + header->union_blob_size + 
				     n * header->field_blob_size);
}

gint
g_union_info_get_n_methods (GIUnionInfo *info)
{
  GIBaseInfo *base = (GIBaseInfo *)info;
  UnionBlob *blob = (UnionBlob *)&base->typelib->data[base->offset];
  
  return blob->n_functions;
}

GIFunctionInfo *
g_union_info_get_method (GIUnionInfo *info,
			 gint         n)
{
  GIBaseInfo *base = (GIBaseInfo *)info;
  UnionBlob *blob = (UnionBlob *)&base->typelib->data[base->offset];
  Header *header = (Header *)base->typelib->data;  
  gint offset;

  offset = base->offset + header->union_blob_size 
    + blob->n_fields * header->field_blob_size 
    + n * header->function_blob_size;
  return (GIFunctionInfo *) g_info_new (GI_INFO_TYPE_FUNCTION, base, 
					base->typelib, offset);
}

gboolean
g_union_info_is_discriminated (GIUnionInfo *info)
{
  GIBaseInfo *base = (GIBaseInfo *)info;
  UnionBlob *blob = (UnionBlob *)&base->typelib->data[base->offset];
  
  return blob->discriminated;
}

gint
g_union_info_get_discriminator_offset (GIUnionInfo *info)
{
  GIBaseInfo *base = (GIBaseInfo *)info;
  UnionBlob *blob = (UnionBlob *)&base->typelib->data[base->offset];
  
  return blob->discriminator_offset;
}

GITypeInfo *
g_union_info_get_discriminator_type (GIUnionInfo *info)
{
  GIBaseInfo *base = (GIBaseInfo *)info;
 
  return g_type_info_new (base, base->typelib, base->offset + 24);
}

GIConstantInfo *
g_union_info_get_discriminator (GIUnionInfo *info,
				gint         n)
{
  GIBaseInfo *base = (GIBaseInfo *)info;
  UnionBlob *blob = (UnionBlob *)&base->typelib->data[base->offset];
  
  if (blob->discriminated)
    {
      Header *header = (Header *)base->typelib->data;  
      gint offset;

      offset = base->offset + header->union_blob_size 
	+ blob->n_fields * header->field_blob_size 
	+ blob->n_functions * header->function_blob_size
	+ n * header->constant_blob_size;
      
      return (GIConstantInfo *) g_info_new (GI_INFO_TYPE_CONSTANT, base, 
					    base->typelib, offset);  
    }

  return NULL;
}

GIFunctionInfo *
g_union_info_find_method (GIUnionInfo *info,
                          const gchar *name)
{
  gint offset;
  GIBaseInfo *base = (GIBaseInfo *)info;
  Header *header = (Header *)base->typelib->data;
  UnionBlob *blob = (UnionBlob *)&base->typelib->data[base->offset];

  offset = base->offset + header->union_blob_size
    + blob->n_fields * header->field_blob_size;

  return find_method (base, offset, blob->n_functions, name);
}

gsize
g_union_info_get_size (GIUnionInfo *info)
{
  GIBaseInfo *base = (GIBaseInfo *)info;
  UnionBlob *blob = (UnionBlob *)&base->typelib->data[base->offset];

  return blob->size;
}

gsize
g_union_info_get_alignment (GIUnionInfo *info)
{
  GIBaseInfo *base = (GIBaseInfo *)info;
  UnionBlob *blob = (UnionBlob *)&base->typelib->data[base->offset];

  return blob->alignment;
}
