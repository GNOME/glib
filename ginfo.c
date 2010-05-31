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

#include "gitypelib-internal.h"
#include "ginfo.h"
#include "girepository-private.h"

/* GICallableInfo functions */

/**
 * SECTION:gicallableinfo
 * @Short_description: Struct representing a callable
 * @Title: GICallableInfo
 *
 * GICallableInfo represents an entity which is callable.
 * Currently a function (#GIFunctionInfo), virtual function,
 * (#GIVirtualFunc) or callback (#GICallbackInfo).
 *
 * A callable has a list of arguments (#GIArgInfo), a return type,
 * direction and a flag which decides if it returns null.
 *
 */
static guint32
signature_offset (GICallableInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo*)info;
  int sigoff = -1;

  switch (rinfo->type)
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
    return *(guint32 *)&rinfo->typelib->data[rinfo->offset + sigoff];
  return 0;
}

GITypeInfo *
g_type_info_new (GIBaseInfo    *container,
                 GTypelib      *typelib,
		 guint32        offset)
{
  SimpleTypeBlob *type = (SimpleTypeBlob *)&typelib->data[offset];

  return (GITypeInfo *) g_info_new (GI_INFO_TYPE_TYPE, container, typelib,
                                    (type->flags.reserved == 0 && type->flags.reserved2 == 0) ? offset : type->offset);
}

static void
g_type_info_init (GIBaseInfo *info,
                  GIBaseInfo *container,
                  GTypelib   *typelib,
                  guint32     offset)
{
  GIRealInfo *rinfo = (GIRealInfo*)container;
  SimpleTypeBlob *type = (SimpleTypeBlob *)&typelib->data[offset];

  _g_info_init ((GIRealInfo*)info, GI_INFO_TYPE_TYPE, rinfo->repository, container, typelib,
                (type->flags.reserved == 0 && type->flags.reserved2 == 0) ? offset : type->offset);
}

/**
 * g_callable_info_get_return_type:
 * @info: a #GICallableInfo
 *
 * Obtain the return type of a callable item as a #GITypeInfo.
 *
 * Returns: (transfer full): the #GITypeInfo. Free the struct by calling
 * g_base_info_unref() when done.
 */
GITypeInfo *
g_callable_info_get_return_type (GICallableInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  guint32 offset;

  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (GI_IS_CALLABLE_INFO (info), NULL);

  offset = signature_offset (info);

  return g_type_info_new ((GIBaseInfo*)info, rinfo->typelib, offset);
}


/**
 * g_callable_info_load_return_type:
 * @info: a #GICallableInfo
 * @type: (out caller-allocates): Initialized with return type of @info
 *
 * Obtain information about a return value of callable; this
 * function is a variant of g_callable_info_get_return_type() designed for stack
 * allocation.
 *
 * The initialized @type must not be referenced after @info is deallocated.
 */
void
g_callable_info_load_return_type (GICallableInfo *info,
                                  GITypeInfo     *type)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  guint32 offset;

  g_return_if_fail (info != NULL);
  g_return_if_fail (GI_IS_CALLABLE_INFO (info));

  offset = signature_offset (info);

  g_type_info_init (type, (GIBaseInfo*)info, rinfo->typelib, offset);
}

/**
 * g_callable_info_may_return_null:
 * @info: a #GICallableInfo
 *
 * See if a callable could return %NULL.
 *
 * Returns: %TRUE if callable could return %NULL
 */
gboolean
g_callable_info_may_return_null (GICallableInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  SignatureBlob *blob;

  g_return_val_if_fail (info != NULL, FALSE);
  g_return_val_if_fail (GI_IS_CALLABLE_INFO (info), FALSE);

  blob = (SignatureBlob *)&rinfo->typelib->data[signature_offset (info)];

  return blob->may_return_null;
}

/**
 * g_callable_info_get_caller_owns:
 * @info: a #GICallableInfo
 *
 * See whether the caller owns the return value of this callable.
 * #GITransfer contains a list of possible transfer values.
 *
 * Returns: %TRUE if the caller owns the return value, %FALSE otherwise.
 */
GITransfer
g_callable_info_get_caller_owns (GICallableInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo*) info;
  SignatureBlob *blob;

  g_return_val_if_fail (info != NULL, -1);
  g_return_val_if_fail (GI_IS_CALLABLE_INFO (info), -1);

  blob = (SignatureBlob *)&rinfo->typelib->data[signature_offset (info)];

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
 * Obtain the number of arguments (both IN and OUT) for this callable.
 *
 * Returns: The number of arguments this callable expects.
 */
gint
g_callable_info_get_n_args (GICallableInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  gint offset;
  SignatureBlob *blob;

  g_return_val_if_fail (info != NULL, -1);
  g_return_val_if_fail (GI_IS_CALLABLE_INFO (info), -1);

  offset = signature_offset (info);
  blob = (SignatureBlob *)&rinfo->typelib->data[offset];

  return blob->n_arguments;
}

/**
 * g_callable_info_get_arg:
 * @info: a #GICallableInfo
 * @n: the argument index to fetch
 *
 * Obtain information about a particular argument of this callable.
 *
 * Returns: (transfer full): the #GIArgInfo. Free it with
 * g_base_info_unref() when done.
 */
GIArgInfo *
g_callable_info_get_arg (GICallableInfo *info,
			 gint            n)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  Header *header;
  gint offset;

  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (GI_IS_CALLABLE_INFO (info), NULL);

  offset = signature_offset (info);
  header = (Header *)rinfo->typelib->data;

  return (GIArgInfo *) g_info_new (GI_INFO_TYPE_ARG, (GIBaseInfo*)info, rinfo->typelib,
				   offset + header->signature_blob_size + n * header->arg_blob_size);
}

/**
 * g_callable_info_load_arg:
 * @info: a #GICallableInfo
 * @n: the argument index to fetch
 * @arg: (out caller-allocates): Initialize with argument number @n
 *
 * Obtain information about a particular argument of this callable; this
 * function is a variant of g_callable_info_get_arg() designed for stack
 * allocation.
 *
 * The initialized @arg must not be referenced after @info is deallocated.
 */
void
g_callable_info_load_arg (GICallableInfo *info,
                          gint            n,
                          GIArgInfo      *arg)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  Header *header;
  gint offset;

  g_return_if_fail (info != NULL);
  g_return_if_fail (GI_IS_CALLABLE_INFO (info));

  offset = signature_offset (info);
  header = (Header *)rinfo->typelib->data;

  _g_info_init ((GIRealInfo*)arg, GI_INFO_TYPE_ARG, rinfo->repository, (GIBaseInfo*)info, rinfo->typelib,
                offset + header->signature_blob_size + n * header->arg_blob_size);
}

/* GIArgInfo function */

/**
 * SECTION:giarginfo
 * @Short_description: Struct representing an argument
 * @Title: GIArgInfo
 *
 * GIArgInfo represents an argument. An argument is always
 * part of a #GICallableInfo.
 *
 *
 */

/**
 * g_arg_info_get_direction:
 * @info: a #GIArgInfo
 *
 * Obtain the direction of the argument. Check #GIDirection for possible
 * direction values.
 *
 * Returns: the direction
 */
GIDirection
g_arg_info_get_direction (GIArgInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  ArgBlob *blob;

  g_return_val_if_fail (info != NULL, -1);
  g_return_val_if_fail (GI_IS_ARG_INFO (info), -1);

  blob = (ArgBlob *)&rinfo->typelib->data[rinfo->offset];

  if (blob->in && blob->out)
    return GI_DIRECTION_INOUT;
  else if (blob->out)
    return GI_DIRECTION_OUT;
  else
    return GI_DIRECTION_IN;
}

/**
 * g_arg_info_is_return_value:
 * @info: a #GIArgInfo
 *
 * Obtain if the argument is a return value. It can either be a
 * parameter or a return value.
 *
 * Returns: %TRUE if it is a return value
 */
gboolean
g_arg_info_is_return_value (GIArgInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  ArgBlob *blob;

  g_return_val_if_fail (info != NULL, FALSE);
  g_return_val_if_fail (GI_IS_ARG_INFO (info), FALSE);

  blob = (ArgBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->return_value;
}

/**
 * g_arg_info_is_caller_allocates:
 * @info: a #GIArgInfo
 *
 * Obtain if the argument is a pointer to a struct or object that will
 * receive an output of a function.  The default assumption for
 * %GI_DIRECTION_OUT arguments which have allocation is that the
 * callee allocates; if this is %TRUE, then the caller must allocate.
 *
 * Returns: %TRUE if caller is required to have allocated the argument
 */
gboolean
g_arg_info_is_caller_allocates (GIArgInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  ArgBlob *blob;

  g_return_val_if_fail (info != NULL, FALSE);
  g_return_val_if_fail (GI_IS_ARG_INFO (info), FALSE);

  blob = (ArgBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->caller_allocates;
}

/**
 * g_arg_info_is_optional:
 * @info: a #GIArgInfo
 *
 * Obtain if the argument is optional.
 *
 * Returns: %TRUE if it is an optional argument
 */
gboolean
g_arg_info_is_optional (GIArgInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  ArgBlob *blob;

  g_return_val_if_fail (info != NULL, FALSE);
  g_return_val_if_fail (GI_IS_ARG_INFO (info), FALSE);

  blob = (ArgBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->optional;
}

/**
 * g_arg_info_may_be_null:
 * @info: a #GIArgInfo
 *
 * Obtain if the argument accepts %NULL.
 *
 * Returns: %TRUE if it accepts %NULL
 */
gboolean
g_arg_info_may_be_null (GIArgInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  ArgBlob *blob;

  g_return_val_if_fail (info != NULL, FALSE);
  g_return_val_if_fail (GI_IS_ARG_INFO (info), FALSE);

  blob = (ArgBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->allow_none;
}

/**
 * g_arg_info_get_ownership_transfer:
 * @info: a #GIArgInfo
 *
 * Obtain the ownership transfer for this argument.
 * #GITransfer contains a list of possible values.
 *
 * Returns: the transfer
 */
GITransfer
g_arg_info_get_ownership_transfer (GIArgInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  ArgBlob *blob;

  g_return_val_if_fail (info != NULL, -1);
  g_return_val_if_fail (GI_IS_ARG_INFO (info), -1);

  blob = (ArgBlob *)&rinfo->typelib->data[rinfo->offset];

  if (blob->transfer_ownership)
    return GI_TRANSFER_EVERYTHING;
  else if (blob->transfer_container_ownership)
    return GI_TRANSFER_CONTAINER;
  else
    return GI_TRANSFER_NOTHING;
}

/**
 * g_arg_info_get_scope:
 * @info: a #GIArgInfo
 *
 * Obtain the scope type for this argument. The scope type explains
 * how a callback is going to be invoked, most importantly when
 * the resources required to invoke it can be freed.
 * #GIScopeType contains a list of possible values.
 *
 * Returns: the scope type
 */
GIScopeType
g_arg_info_get_scope (GIArgInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  ArgBlob *blob;

  g_return_val_if_fail (info != NULL, -1);
  g_return_val_if_fail (GI_IS_ARG_INFO (info), -1);

  blob = (ArgBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->scope;
}

/**
 * g_arg_info_get_closure:
 * @info: a #GIArgInfo
 *
 * Obtain the index of the user data argument. This is only valid
 * for arguments which are callbacks.
 *
 * Returns: index of the user data argument or -1 if there is none
 */
gint
g_arg_info_get_closure (GIArgInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  ArgBlob *blob;

  g_return_val_if_fail (info != NULL, -1);
  g_return_val_if_fail (GI_IS_ARG_INFO (info), -1);

  blob = (ArgBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->closure;
}

/**
 * g_arg_info_get_destroy:
 * @info: a #GIArgInfo
 *
 * Obtains the index of the #GDestroyNotify argument. This is only valid
 * for arguments which are callbacks.
 *
 * Returns: index of the #GDestroyNotify argument or -1 if there is none
 */
gint
g_arg_info_get_destroy (GIArgInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  ArgBlob *blob;

  g_return_val_if_fail (info != NULL, -1);
  g_return_val_if_fail (GI_IS_ARG_INFO (info), -1);

  blob = (ArgBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->destroy;
}

/**
 * g_arg_info_get_type:
 * @info: a #GIArgInfo
 *
 * Obtain the type information for @info.
 *
 * Returns: (transfer full): the #GIArgInfo, free it with
 * g_base_info_unref() when done.
 */
GITypeInfo *
g_arg_info_get_type (GIArgInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;

  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (GI_IS_ARG_INFO (info), NULL);

  return g_type_info_new ((GIBaseInfo*)info, rinfo->typelib, rinfo->offset + G_STRUCT_OFFSET (ArgBlob, arg_type));
}

/**
 * g_arg_info_load_type:
 * @info: a #GIArgInfo
 * @type: (out caller-allocates): Initialized with information about type of @info
 *
 * Obtain information about a the type of given argument @info; this
 * function is a variant of g_arg_info_get_type() designed for stack
 * allocation.
 *
 * The initialized @type must not be referenced after @info is deallocated.
 */
void
g_arg_info_load_type (GIArgInfo  *info,
                      GITypeInfo *type)
{
  GIRealInfo *rinfo = (GIRealInfo*) info;

  g_return_if_fail (info != NULL);
  g_return_if_fail (GI_IS_ARG_INFO (info));

  g_type_info_init (type, (GIBaseInfo*)info, rinfo->typelib, rinfo->offset + G_STRUCT_OFFSET (ArgBlob, arg_type));
}

/* GITypeInfo functions */

/**
 * SECTION:gitypeinfo
 * @Short_description: Struct representing a type
 * @Title: GITypeInfo
 *
 * GITypeInfo represents a type. You can retrieve a type info from
 * an argument (see #GIArgInfo), a functions return value (see #GIFunctionInfo),
 * a field (see #GIFieldInfo), a property (see #GIPropertyInfo), a constant
 * (see #GIConstantInfo) or for a union discriminator (see #GIUnionInfo).
 *
 * A type can either be a of a basic type which is a standard C primitive
 * type or an interface type. For interface types you need to call
 * g_type_info_get_interface() to get a reference to the base info for that
 * interface.
 *
 */

/**
 * g_type_info_is_pointer:
 * @info: a #GITypeInfo
 *
 * Obtain if the type is passed as a reference.
 *
 * Returns: %TRUE if it is a pointer
 */
gboolean
g_type_info_is_pointer (GITypeInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  SimpleTypeBlob *type;

  g_return_val_if_fail (info != NULL, FALSE);
  g_return_val_if_fail (GI_IS_TYPE_INFO (info), FALSE);

  type = (SimpleTypeBlob *)&rinfo->typelib->data[rinfo->offset];

  if (type->flags.reserved == 0 && type->flags.reserved2 == 0)
    return type->flags.pointer;
  else
    {
      InterfaceTypeBlob *iface = (InterfaceTypeBlob *)&rinfo->typelib->data[rinfo->offset];

      return iface->pointer;
    }
}

/**
 * g_type_info_get_tag:
 * @info: a #GITypeInfo
 *
 * Obtain the type tag for the type. See #GITypeTag for a list
 * of type tags.
 *
 * Returns: the type tag
 */
GITypeTag
g_type_info_get_tag (GITypeInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  SimpleTypeBlob *type;

  g_return_val_if_fail (info != NULL, GI_TYPE_TAG_BOOLEAN);
  g_return_val_if_fail (GI_IS_TYPE_INFO (info), GI_TYPE_TAG_BOOLEAN);

  type = (SimpleTypeBlob *)&rinfo->typelib->data[rinfo->offset];

  if (rinfo->type_is_embedded)
    return GI_TYPE_TAG_INTERFACE;
  else if (type->flags.reserved == 0 && type->flags.reserved2 == 0)
    return type->flags.tag;
  else
    {
      InterfaceTypeBlob *iface = (InterfaceTypeBlob *)&rinfo->typelib->data[rinfo->offset];

      return iface->tag;
    }
}

/**
 * g_type_info_get_param_type:
 * @info: a #GITypeInfo
 * @n: index of the parameter
 *
 * Obtain the parameter type @n.
 *
 * Returns: the param type info
 */
GITypeInfo *
g_type_info_get_param_type (GITypeInfo *info,
                            gint        n)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  SimpleTypeBlob *type;

  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (GI_IS_TYPE_INFO (info), NULL);

  type = (SimpleTypeBlob *)&rinfo->typelib->data[rinfo->offset];

  if (!(type->flags.reserved == 0 && type->flags.reserved2 == 0))
    {
      ParamTypeBlob *param = (ParamTypeBlob *)&rinfo->typelib->data[rinfo->offset];

      switch (param->tag)
        {
          case GI_TYPE_TAG_ARRAY:
          case GI_TYPE_TAG_GLIST:
          case GI_TYPE_TAG_GSLIST:
          case GI_TYPE_TAG_GHASH:
            return g_type_info_new ((GIBaseInfo*)info, rinfo->typelib,
                                    rinfo->offset + sizeof (ParamTypeBlob)
                                    + sizeof (SimpleTypeBlob) * n);
            break;
          default:
            break;
        }
    }

  return NULL;
}

/**
 * g_type_info_get_interface:
 * @info: a #GITypeInfo
 *
 * For types which have #GI_TYPE_TAG_INTERFACE such as GObjects and boxed values,
 * this function returns full information about the referenced type.  You can then
 * inspect the type of the returned #GIBaseInfo to further query whether it is
 * a concrete GObject, a GInterface, a structure, etc. using g_base_info_get_type().
 *
 * Returns: (transfer full): the #GIBaseInfo, or %NULL. Free it with
 * g_base_info_unref() when done.
 */
GIBaseInfo *
g_type_info_get_interface (GITypeInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;

  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (GI_IS_TYPE_INFO (info), NULL);

  /* For embedded types, the given offset is a pointer to the actual blob,
   * after the end of the field.  In that case we know it's a "subclass" of
   * CommonBlob, so use that to determine the info type.
   */
  if (rinfo->type_is_embedded)
    {
      CommonBlob *common = (CommonBlob *)&rinfo->typelib->data[rinfo->offset];
      GIInfoType info_type;

      switch (common->blob_type)
        {
          case BLOB_TYPE_CALLBACK:
            info_type = GI_INFO_TYPE_CALLBACK;
            break;
          default:
            g_assert_not_reached ();
            return NULL;
        }
      return (GIBaseInfo *) g_info_new (info_type, (GIBaseInfo*)info, rinfo->typelib,
                                        rinfo->offset);
    }
  else
    {
      SimpleTypeBlob *type = (SimpleTypeBlob *)&rinfo->typelib->data[rinfo->offset];
      if (!(type->flags.reserved == 0 && type->flags.reserved2 == 0))
        {
          InterfaceTypeBlob *blob = (InterfaceTypeBlob *)&rinfo->typelib->data[rinfo->offset];

          if (blob->tag == GI_TYPE_TAG_INTERFACE)
            return _g_info_from_entry (rinfo->repository, rinfo->typelib, blob->interface);
        }
    }

  return NULL;
}

/**
 * g_type_info_get_array_length:
 * @info: a #GITypeInfo
 *
 * Obtain the array length of the type. The type tag must be a
 * #GI_TYPE_TAG_ARRAY or -1 will returned.
 *
 * Returns: the array length, or -1 if the type is not an array
 */
gint
g_type_info_get_array_length (GITypeInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  SimpleTypeBlob *type;

  g_return_val_if_fail (info != NULL, -1);
  g_return_val_if_fail (GI_IS_TYPE_INFO (info), -1);

  type = (SimpleTypeBlob *)&rinfo->typelib->data[rinfo->offset];

  if (!(type->flags.reserved == 0 && type->flags.reserved2 == 0))
    {
      ArrayTypeBlob *blob = (ArrayTypeBlob *)&rinfo->typelib->data[rinfo->offset];

      if (blob->tag == GI_TYPE_TAG_ARRAY)
	{
	  if (blob->has_length)
	    return blob->dimensions.length;
	}
    }

  return -1;
}

/**
 * g_type_info_get_array_fixed_size:
 * @info: a #GITypeInfo
 *
 * Obtain the fixed array size of the type. The type tag must be a
 * #GI_TYPE_TAG_ARRAY or -1 will returned.
 *
 * Returns: the size or -1 if it's not an array
 */
gint
g_type_info_get_array_fixed_size (GITypeInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  SimpleTypeBlob *type;

  g_return_val_if_fail (info != NULL, 0);
  g_return_val_if_fail (GI_IS_TYPE_INFO (info), 0);

  type = (SimpleTypeBlob *)&rinfo->typelib->data[rinfo->offset];

  if (!(type->flags.reserved == 0 && type->flags.reserved2 == 0))
    {
      ArrayTypeBlob *blob = (ArrayTypeBlob *)&rinfo->typelib->data[rinfo->offset];

      if (blob->tag == GI_TYPE_TAG_ARRAY)
	{
	  if (blob->has_size)
	    return blob->dimensions.size;
	}
    }

  return -1;
}

/**
 * g_type_info_is_zero_terminated:
 * @info: a #GITypeInfo
 *
 * Obtain if the last element of the array is %NULL. The type tag must be a
 * #GI_TYPE_TAG_ARRAY or %FALSE will returned.
 *
 * Returns: %TRUE if zero terminated
 */
gboolean
g_type_info_is_zero_terminated (GITypeInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  SimpleTypeBlob *type;

  g_return_val_if_fail (info != NULL, FALSE);
  g_return_val_if_fail (GI_IS_TYPE_INFO (info), FALSE);

  type = (SimpleTypeBlob *)&rinfo->typelib->data[rinfo->offset];

  if (!(type->flags.reserved == 0 && type->flags.reserved2 == 0))
    {
      ArrayTypeBlob *blob = (ArrayTypeBlob *)&rinfo->typelib->data[rinfo->offset];

      if (blob->tag == GI_TYPE_TAG_ARRAY)
	return blob->zero_terminated;
    }

  return FALSE;
}

/**
 * g_type_info_get_array_type:
 * @info: a #GITypeInfo
 *
 * Obtain the array type for this type. See #GIArrayType for a list of
 * possible values. If the type tag of this type is not array, -1 will be
 * returned.
 *
 * Returns: the array type or -1
 */
GIArrayType
g_type_info_get_array_type (GITypeInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  SimpleTypeBlob *type;

  g_return_val_if_fail (info != NULL, -1);
  g_return_val_if_fail (GI_IS_TYPE_INFO (info), -1);

  type = (SimpleTypeBlob *)&rinfo->typelib->data[rinfo->offset];

  if (!(type->flags.reserved == 0 && type->flags.reserved2 == 0))
    {
      ArrayTypeBlob *blob = (ArrayTypeBlob *)&rinfo->typelib->data[rinfo->offset];
      g_return_val_if_fail (blob->tag == GI_TYPE_TAG_ARRAY, -1);

      return blob->array_type;
    }

  return -1;
}

/**
 * g_type_info_get_n_error_domains:
 * @info: a #GITypeInfo
 *
 * Obtain the number of error domains for this type. The type tag
 * must be a #GI_TYPE_TAG_ERROR or -1 will be returned.
 *
 * Returns: number of error domains or -1
 */
gint
g_type_info_get_n_error_domains (GITypeInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  SimpleTypeBlob *type;

  g_return_val_if_fail (info != NULL, 0);
  g_return_val_if_fail (GI_IS_TYPE_INFO (info), 0);

  type = (SimpleTypeBlob *)&rinfo->typelib->data[rinfo->offset];

  if (!(type->flags.reserved == 0 && type->flags.reserved2 == 0))
    {
      ErrorTypeBlob *blob = (ErrorTypeBlob *)&rinfo->typelib->data[rinfo->offset];

      if (blob->tag == GI_TYPE_TAG_ERROR)
	return blob->n_domains;
    }

  return 0;
}

/**
 * g_type_info_get_error_domain:
 * @info: a #GITypeInfo
 * @n: index of error domain
 *
 * Obtain the error domains at index @n for this type. The type tag
 * must be a #GI_TYPE_TAG_ERROR or -1 will be returned.
 *
 * Returns: (transfer full): the error domain or %NULL if type tag is wrong,
 * free the struct with g_base_info_unref() when done.
 */
GIErrorDomainInfo *
g_type_info_get_error_domain (GITypeInfo *info,
                              gint        n)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  SimpleTypeBlob *type;

  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (GI_IS_TYPE_INFO (info), NULL);

  type = (SimpleTypeBlob *)&rinfo->typelib->data[rinfo->offset];

  if (!(type->flags.reserved == 0 && type->flags.reserved2 == 0))
    {
      ErrorTypeBlob *blob = (ErrorTypeBlob *)&rinfo->typelib->data[rinfo->offset];

      if (blob->tag == GI_TYPE_TAG_ERROR)
        return (GIErrorDomainInfo *) _g_info_from_entry (rinfo->repository,
                                                         rinfo->typelib,
                                                         blob->domains[n]);
    }

  return NULL;
}


/* GIErrorDomainInfo functions */

/**
 * SECTION:gierrordomaininfo
 * @Short_description: Struct representing an error domain
 * @Title: GIErrorDomainInfo
 *
 * A GIErrorDomainInfo struct represents a domain of a #GError.
 * An error domain is associated with a #GQuark and contains a pointer
 * to an enum with all the error codes.
 */

/**
 * g_error_domain_info_get_quark:
 * @info: a #GIErrorDomainInfo
 *
 * Obtain a string representing the quark for this error domain.
 * %NULL will be returned if the type tag is wrong or if a quark is
 * missing in the typelib.
 *
 * Returns: the quark represented as a string or %NULL
 */
const gchar *
g_error_domain_info_get_quark (GIErrorDomainInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  ErrorDomainBlob *blob;

  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (GI_IS_ERROR_DOMAIN_INFO (info), NULL);

  blob = (ErrorDomainBlob *)&rinfo->typelib->data[rinfo->offset];

  return g_typelib_get_string (rinfo->typelib, blob->get_quark);
}

/**
 * g_error_domain_info_get_codes:
 * @info: a #GIErrorDomainInfo
 *
 * Obtain the enum containing all the error codes for this error domain.
 * The return value will have a #GIInfoType of %GI_INFO_TYPE_ERROR_DOMAIN
 *
 * Returns: (transfer full): the error domain or %NULL if type tag is wrong,
 * free the struct with g_base_info_unref() when done.
 */
GIInterfaceInfo *
g_error_domain_info_get_codes (GIErrorDomainInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  ErrorDomainBlob *blob;

  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (GI_IS_ERROR_DOMAIN_INFO (info), NULL);

  blob = (ErrorDomainBlob *)&rinfo->typelib->data[rinfo->offset];

  return (GIInterfaceInfo *) _g_info_from_entry (rinfo->repository,
                                                 rinfo->typelib, blob->error_codes);
}


/* GIEnumInfo and GIValueInfo functions */

/**
 * SECTION:gienuminfo
 * @Short_description: Structs representing an enumeration and its values
 * @Title: GIEnumInfo
 *
 * A GIEnumInfo represents an enumeration and a GIValueInfo struct represents a value
 * of an enumeration. The GIEnumInfo contains a set of values and a type
 * The GIValueInfo is fetched by calling g_enum_info_get_value() on a #GIEnumInfo.
 */

/**
* g_enum_info_get_n_values:
* @info: a #GIEnumInfo
*
* Obtain the number of values this enumeration contains.
*
* Returns: the number of enumeration values
*/
gint
g_enum_info_get_n_values (GIEnumInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  EnumBlob *blob;

  g_return_val_if_fail (info != NULL, 0);
  g_return_val_if_fail (GI_IS_ENUM_INFO (info), 0);

  blob = (EnumBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->n_values;
}

/**
 * g_enum_info_get_value:
 * @info: a #GIEnumInfo
 * @n: index of value to fetch
 *
 * Obtain a value for this enumeration.
 *
 * Returns: (transfer full): the enumeration value or %NULL if type tag is wrong,
 * free the struct with g_base_info_unref() when done.
 */
GIValueInfo *
g_enum_info_get_value (GIEnumInfo *info,
		       gint        n)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  Header *header;
  gint offset;

  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (GI_IS_ENUM_INFO (info), NULL);

  header = (Header *)rinfo->typelib->data;
  offset = rinfo->offset + header->enum_blob_size
    + n * header->value_blob_size;

  return (GIValueInfo *) g_info_new (GI_INFO_TYPE_VALUE, (GIBaseInfo*)info, rinfo->typelib, offset);
}

/**
 * g_enum_info_get_storage_type:
 * @info: a #GIEnumInfo
 *
 * Obtain the tag of the type used for the enum in the C ABI. This will
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
  GIRealInfo *rinfo = (GIRealInfo *)info;
  EnumBlob *blob;

  g_return_val_if_fail (info != NULL, GI_TYPE_TAG_BOOLEAN);
  g_return_val_if_fail (GI_IS_ENUM_INFO (info), GI_TYPE_TAG_BOOLEAN);

  blob = (EnumBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->storage_type;
}

/**
 * g_value_info_get_value:
 * @info: a #GIValueInfo
 *
 * Obtain the enumeration value of the #GIValueInfo.
 *
 * Returns: the enumeration value
 */
glong
g_value_info_get_value (GIValueInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  ValueBlob *blob;

  g_return_val_if_fail (info != NULL, -1);
  g_return_val_if_fail (GI_IS_VALUE_INFO (info), -1);

  blob = (ValueBlob *)&rinfo->typelib->data[rinfo->offset];

  return (glong)blob->value;
}

/* GIFieldInfo functions */

/**
 * SECTION:gifieldinfo
 * @Short_description: Struct representing a struct or union field
 * @Title: GIFieldInfo
 *
 * A GIFieldInfo struct represents a field of a struct (see #GIStructInfo),
 * union (see #GIUnionInfo) or an object (see #GIObjectInfo). The GIFieldInfo
 * is fetched by calling g_struct_info_get_field(), g_union_info_get_field()
 * or g_object_info_get_value().
 * A field has a size, type and a struct offset asssociated and a set of flags,
 * which is currently #GI_FIELD_IS_READABLE or #GI_FIELD_IS_WRITABLE.
 */

GIFieldInfoFlags
g_field_info_get_flags (GIFieldInfo *info)
{
  GIFieldInfoFlags flags;

  GIRealInfo *rinfo = (GIRealInfo *)info;
  FieldBlob *blob = (FieldBlob *)&rinfo->typelib->data[rinfo->offset];

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
  GIRealInfo *rinfo = (GIRealInfo *)info;
  FieldBlob *blob = (FieldBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->bits;
}

gint
g_field_info_get_offset (GIFieldInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  FieldBlob *blob = (FieldBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->struct_offset;
}

GITypeInfo *
g_field_info_get_type (GIFieldInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  Header *header = (Header *)rinfo->typelib->data;
  FieldBlob *blob = (FieldBlob *)&rinfo->typelib->data[rinfo->offset];
  GIRealInfo *type_info;

  if (blob->has_embedded_type)
    {
      type_info = (GIRealInfo *) g_info_new (GI_INFO_TYPE_TYPE,
                                                (GIBaseInfo*)info, rinfo->typelib,
                                                rinfo->offset + header->field_blob_size);
      type_info->type_is_embedded = TRUE;
    }
  else
    return g_type_info_new ((GIBaseInfo*)info, rinfo->typelib, rinfo->offset + G_STRUCT_OFFSET (FieldBlob, type));

  return (GIBaseInfo*)type_info;
}

/* GIRegisteredTypeInfo functions */
const gchar *
g_registered_type_info_get_type_name (GIRegisteredTypeInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  RegisteredTypeBlob *blob = (RegisteredTypeBlob *)&rinfo->typelib->data[rinfo->offset];

  if (blob->gtype_name)
    return g_typelib_get_string (rinfo->typelib, blob->gtype_name);

  return NULL;
}

const gchar *
g_registered_type_info_get_type_init (GIRegisteredTypeInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  RegisteredTypeBlob *blob = (RegisteredTypeBlob *)&rinfo->typelib->data[rinfo->offset];

  if (blob->gtype_init)
    return g_typelib_get_string (rinfo->typelib, blob->gtype_init);

  return NULL;
}

GType
g_registered_type_info_get_g_type (GIRegisteredTypeInfo *info)
{
  const char *type_init;
  GType (* get_type_func) (void);
  GIRealInfo *rinfo = (GIRealInfo*)info;

  type_init = g_registered_type_info_get_type_init (info);

  if (type_init == NULL)
    return G_TYPE_NONE;
  else if (!strcmp (type_init, "intern"))
    return G_TYPE_OBJECT;

  get_type_func = NULL;
  if (!g_typelib_symbol (rinfo->typelib,
                         type_init,
                         (void**) &get_type_func))
    return G_TYPE_NONE;

  return (* get_type_func) ();
}

/* GIStructInfo functions */
gint
g_struct_info_get_n_fields (GIStructInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  StructBlob *blob = (StructBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->n_fields;
}

static gint32
g_struct_get_field_offset (GIStructInfo *info,
			   gint         n)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  Header *header = (Header *)rinfo->typelib->data;
  guint32 offset = rinfo->offset + header->struct_blob_size;
  gint i;
  FieldBlob *field_blob;

  for (i = 0; i < n; i++)
    {
      field_blob = (FieldBlob *)&rinfo->typelib->data[offset];
      offset += header->field_blob_size;
      if (field_blob->has_embedded_type)
        offset += header->callback_blob_size;
    }

  return offset;
}

GIFieldInfo *
g_struct_info_get_field (GIStructInfo *info,
                         gint          n)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;

  return (GIFieldInfo *) g_info_new (GI_INFO_TYPE_FIELD, (GIBaseInfo*)info, rinfo->typelib,
                                     g_struct_get_field_offset (info, n));
}

gint
g_struct_info_get_n_methods (GIStructInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  StructBlob *blob = (StructBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->n_methods;
}

GIFunctionInfo *
g_struct_info_get_method (GIStructInfo *info,
			  gint         n)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  StructBlob *blob = (StructBlob *)&rinfo->typelib->data[rinfo->offset];
  Header *header = (Header *)rinfo->typelib->data;
  gint offset;

  offset = g_struct_get_field_offset (info, blob->n_fields) + n * header->function_blob_size;
  return (GIFunctionInfo *) g_info_new (GI_INFO_TYPE_FUNCTION, (GIBaseInfo*)info,
                                        rinfo->typelib, offset);
}

static GIFunctionInfo *
find_method (GIBaseInfo   *base,
             guint32       offset,
             gint          n_methods,
             const gchar  *name)
{
  /* FIXME hash */
  GIRealInfo *rinfo = (GIRealInfo*)base;
  Header *header = (Header *)rinfo->typelib->data;
  gint i;

  for (i = 0; i < n_methods; i++)
    {
      FunctionBlob *fblob = (FunctionBlob *)&rinfo->typelib->data[offset];
      const gchar *fname = (const gchar *)&rinfo->typelib->data[fblob->name];

      if (strcmp (name, fname) == 0)
        return (GIFunctionInfo *) g_info_new (GI_INFO_TYPE_FUNCTION, base,
			                      rinfo->typelib, offset);

      offset += header->function_blob_size;
    }

  return NULL;
}

GIFunctionInfo *
g_struct_info_find_method (GIStructInfo *info,
			   const gchar  *name)
{
  gint offset;
  GIRealInfo *rinfo = (GIRealInfo *)info;
  Header *header = (Header *)rinfo->typelib->data;
  StructBlob *blob = (StructBlob *)&rinfo->typelib->data[rinfo->offset];

  offset = rinfo->offset + header->struct_blob_size
    + blob->n_fields * header->field_blob_size;

  return find_method ((GIBaseInfo*)info, offset, blob->n_methods, name);
}

gsize
g_struct_info_get_size (GIStructInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  StructBlob *blob = (StructBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->size;
}

gsize
g_struct_info_get_alignment (GIStructInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  StructBlob *blob = (StructBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->alignment;
}

gboolean
g_struct_info_is_foreign (GIStructInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  StructBlob *blob = (StructBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->foreign;
}

/**
 * g_struct_info_is_gtype_struct:
 * @info: a #GIStructInfo
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
  GIRealInfo *rinfo = (GIRealInfo *)info;
  StructBlob *blob = (StructBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->is_gtype_struct;
}

/* GIObjectInfo functions */
GIObjectInfo *
g_object_info_get_parent (GIObjectInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  ObjectBlob *blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];

  if (blob->parent)
    return (GIObjectInfo *) _g_info_from_entry (rinfo->repository,
                                                rinfo->typelib, blob->parent);
  else
    return NULL;
}

gboolean
g_object_info_get_abstract (GIObjectInfo    *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  ObjectBlob *blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];
  return blob->abstract != 0;
}

const gchar *
g_object_info_get_type_name (GIObjectInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  ObjectBlob *blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];

  return g_typelib_get_string (rinfo->typelib, blob->gtype_name);
}

const gchar *
g_object_info_get_type_init (GIObjectInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  ObjectBlob *blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];

  return g_typelib_get_string (rinfo->typelib, blob->gtype_init);
}

gint
g_object_info_get_n_interfaces (GIObjectInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  ObjectBlob *blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->n_interfaces;
}

GIInterfaceInfo *
g_object_info_get_interface (GIObjectInfo *info,
			     gint          n)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  ObjectBlob *blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];

  return (GIInterfaceInfo *) _g_info_from_entry (rinfo->repository,
						 rinfo->typelib, blob->interfaces[n]);
}

gint
g_object_info_get_n_fields (GIObjectInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  ObjectBlob *blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->n_fields;
}

GIFieldInfo *
g_object_info_get_field (GIObjectInfo *info,
			 gint          n)
{
  gint offset;
  GIRealInfo *rinfo = (GIRealInfo *)info;
  Header *header = (Header *)rinfo->typelib->data;
  ObjectBlob *blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];

  offset = rinfo->offset + header->object_blob_size
    + (blob->n_interfaces + blob->n_interfaces % 2) * 2
    + n * header->field_blob_size;

  return (GIFieldInfo *) g_info_new (GI_INFO_TYPE_FIELD, (GIBaseInfo*)info, rinfo->typelib, offset);
}

gint
g_object_info_get_n_properties (GIObjectInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  ObjectBlob *blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->n_properties;
}

GIPropertyInfo *
g_object_info_get_property (GIObjectInfo *info,
			    gint          n)
{
  gint offset;
  GIRealInfo *rinfo = (GIRealInfo *)info;
  Header *header = (Header *)rinfo->typelib->data;
  ObjectBlob *blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];

  offset = rinfo->offset + header->object_blob_size
    + (blob->n_interfaces + blob->n_interfaces % 2) * 2
    + blob->n_fields * header->field_blob_size
    + n * header->property_blob_size;

  return (GIPropertyInfo *) g_info_new (GI_INFO_TYPE_PROPERTY, (GIBaseInfo*)info,
					rinfo->typelib, offset);
}

gint
g_object_info_get_n_methods (GIObjectInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  ObjectBlob *blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->n_methods;
}

GIFunctionInfo *
g_object_info_get_method (GIObjectInfo *info,
			  gint          n)
{
  gint offset;
  GIRealInfo *rinfo = (GIRealInfo *)info;
  Header *header = (Header *)rinfo->typelib->data;
  ObjectBlob *blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];

  offset = rinfo->offset + header->object_blob_size
    + (blob->n_interfaces + blob->n_interfaces % 2) * 2
    + blob->n_fields * header->field_blob_size
    + blob->n_properties * header->property_blob_size
    + n * header->function_blob_size;

    return (GIFunctionInfo *) g_info_new (GI_INFO_TYPE_FUNCTION, (GIBaseInfo*)info,
					  rinfo->typelib, offset);
}

GIFunctionInfo *
g_object_info_find_method (GIObjectInfo *info,
			   const gchar  *name)
{
  gint offset;
  GIRealInfo *rinfo = (GIRealInfo *)info;
  Header *header = (Header *)rinfo->typelib->data;
  ObjectBlob *blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];

  offset = rinfo->offset + header->object_blob_size
    + (blob->n_interfaces + blob->n_interfaces % 2) * 2
    + blob->n_fields * header->field_blob_size +
    + blob->n_properties * header->property_blob_size;

  return find_method ((GIBaseInfo*)info, offset, blob->n_methods, name);
}

gint
g_object_info_get_n_signals (GIObjectInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  ObjectBlob *blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->n_signals;
}

GISignalInfo *
g_object_info_get_signal (GIObjectInfo *info,
			  gint          n)
{
  gint offset;
  GIRealInfo *rinfo = (GIRealInfo *)info;
  Header *header = (Header *)rinfo->typelib->data;
  ObjectBlob *blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];

  offset = rinfo->offset + header->object_blob_size
    + (blob->n_interfaces + blob->n_interfaces % 2) * 2
    + blob->n_fields * header->field_blob_size
    + blob->n_properties * header->property_blob_size
    + blob->n_methods * header->function_blob_size
    + n * header->signal_blob_size;

  return (GISignalInfo *) g_info_new (GI_INFO_TYPE_SIGNAL, (GIBaseInfo*)info,
				      rinfo->typelib, offset);
}

gint
g_object_info_get_n_vfuncs (GIObjectInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  ObjectBlob *blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->n_vfuncs;
}

GIVFuncInfo *
g_object_info_get_vfunc (GIObjectInfo *info,
			 gint          n)
{
  gint offset;
  GIRealInfo *rinfo = (GIRealInfo *)info;
  Header *header = (Header *)rinfo->typelib->data;
  ObjectBlob *blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];

  offset = rinfo->offset + header->object_blob_size
    + (blob->n_interfaces + blob->n_interfaces % 2) * 2
    + blob->n_fields * header->field_blob_size
    + blob->n_properties * header->property_blob_size
    + blob->n_methods * header->function_blob_size
    + blob->n_signals * header->signal_blob_size
    + n * header->vfunc_blob_size;

  return (GIVFuncInfo *) g_info_new (GI_INFO_TYPE_VFUNC, (GIBaseInfo*)info,
				     rinfo->typelib, offset);
}

static GIVFuncInfo *
find_vfunc (GIRealInfo   *rinfo,
            guint32       offset,
            gint          n_vfuncs,
            const gchar  *name)
{
  /* FIXME hash */
  Header *header = (Header *)rinfo->typelib->data;
  gint i;

  for (i = 0; i < n_vfuncs; i++)
    {
      VFuncBlob *fblob = (VFuncBlob *)&rinfo->typelib->data[offset];
      const gchar *fname = (const gchar *)&rinfo->typelib->data[fblob->name];

      if (strcmp (name, fname) == 0)
        return (GIVFuncInfo *) g_info_new (GI_INFO_TYPE_VFUNC, (GIBaseInfo*) rinfo,
                                           rinfo->typelib, offset);

      offset += header->vfunc_blob_size;
    }

  return NULL;
}

/**
 * g_object_info_find_vfunc:
 * @info: a #GIObjectInfo
 * @name: The name of a virtual function to find.
 *
 * Locate a virtual function slot with name @name. Note that the namespace
 * for virtuals is distinct from that of methods; there may or may not be
 * a concrete method associated for a virtual. If there is one, it may
 * be retrieved using g_vfunc_info_get_invoker(), otherwise %NULL will be
 * returned.
 * See the documentation for g_vfunc_info_get_invoker() for more
 * information on invoking virtuals.
 *
 * Returns: (transfer full): the #GIVFuncInfo, or %NULL. Free it with
 * g_base_info_unref() when done.
 */
GIVFuncInfo *
g_object_info_find_vfunc (GIObjectInfo *info,
                          const gchar  *name)
{
  gint offset;
  GIRealInfo *rinfo = (GIRealInfo *)info;
  Header *header = (Header *)rinfo->typelib->data;
  ObjectBlob *blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];

  offset = rinfo->offset + header->object_blob_size
    + (blob->n_interfaces + blob->n_interfaces % 2) * 2
    + blob->n_fields * header->field_blob_size
    + blob->n_properties * header->property_blob_size
    + blob->n_methods * header->function_blob_size
    + blob->n_signals * header->signal_blob_size;

  return find_vfunc (rinfo, offset, blob->n_vfuncs, name);
}

gint
g_object_info_get_n_constants (GIObjectInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  ObjectBlob *blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->n_constants;
}

GIConstantInfo *
g_object_info_get_constant (GIObjectInfo *info,
			    gint          n)
{
  gint offset;
  GIRealInfo *rinfo = (GIRealInfo *)info;
  Header *header = (Header *)rinfo->typelib->data;
  ObjectBlob *blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];

  offset = rinfo->offset + header->object_blob_size
    + (blob->n_interfaces + blob->n_interfaces % 2) * 2
    + blob->n_fields * header->field_blob_size
    + blob->n_properties * header->property_blob_size
    + blob->n_methods * header->function_blob_size
    + blob->n_signals * header->signal_blob_size
    + blob->n_vfuncs * header->vfunc_blob_size
    + n * header->constant_blob_size;

  return (GIConstantInfo *) g_info_new (GI_INFO_TYPE_CONSTANT, (GIBaseInfo*)info,
					rinfo->typelib, offset);
}

/**
 * g_object_info_get_class_struct:
 * @info: a #GIObjectInfo
 *
 * Every #GObject has two structures; an instance structure and a class
 * structure.  This function returns the metadata for the class structure.
 *
 * Returns: (transfer full): the #GIStructInfo or %NULL. Free with
 * g_base_info_unref() when done.
 */
GIStructInfo *
g_object_info_get_class_struct (GIObjectInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  ObjectBlob *blob = (ObjectBlob *)&rinfo->typelib->data[rinfo->offset];

  if (blob->gtype_struct)
    return (GIStructInfo *) _g_info_from_entry (rinfo->repository,
                                                rinfo->typelib, blob->gtype_struct);
  else
    return NULL;
}

/* GIInterfaceInfo functions */
gint
g_interface_info_get_n_prerequisites (GIInterfaceInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  InterfaceBlob *blob = (InterfaceBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->n_prerequisites;
}

GIBaseInfo *
g_interface_info_get_prerequisite (GIInterfaceInfo *info,
				   gint            n)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  InterfaceBlob *blob = (InterfaceBlob *)&rinfo->typelib->data[rinfo->offset];

  return _g_info_from_entry (rinfo->repository,
			     rinfo->typelib, blob->prerequisites[n]);
}


gint
g_interface_info_get_n_properties (GIInterfaceInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  InterfaceBlob *blob = (InterfaceBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->n_properties;
}

GIPropertyInfo *
g_interface_info_get_property (GIInterfaceInfo *info,
			       gint            n)
{
  gint offset;
  GIRealInfo *rinfo = (GIRealInfo *)info;
  Header *header = (Header *)rinfo->typelib->data;
  InterfaceBlob *blob = (InterfaceBlob *)&rinfo->typelib->data[rinfo->offset];

  offset = rinfo->offset + header->interface_blob_size
    + (blob->n_prerequisites + (blob->n_prerequisites % 2)) * 2
    + n * header->property_blob_size;

  return (GIPropertyInfo *) g_info_new (GI_INFO_TYPE_PROPERTY, (GIBaseInfo*)info,
					rinfo->typelib, offset);
}

gint
g_interface_info_get_n_methods (GIInterfaceInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  InterfaceBlob *blob = (InterfaceBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->n_methods;
}

GIFunctionInfo *
g_interface_info_get_method (GIInterfaceInfo *info,
			     gint            n)
{
  gint offset;
  GIRealInfo *rinfo = (GIRealInfo *)info;
  Header *header = (Header *)rinfo->typelib->data;
  InterfaceBlob *blob = (InterfaceBlob *)&rinfo->typelib->data[rinfo->offset];

  offset = rinfo->offset + header->interface_blob_size
    + (blob->n_prerequisites + (blob->n_prerequisites % 2)) * 2
    + blob->n_properties * header->property_blob_size
    + n * header->function_blob_size;

  return (GIFunctionInfo *) g_info_new (GI_INFO_TYPE_FUNCTION, (GIBaseInfo*)info,
					rinfo->typelib, offset);
}

GIFunctionInfo *
g_interface_info_find_method (GIInterfaceInfo *info,
			      const gchar     *name)
{
  gint offset;
  GIRealInfo *rinfo = (GIRealInfo *)info;
  Header *header = (Header *)rinfo->typelib->data;
  InterfaceBlob *blob = (InterfaceBlob *)&rinfo->typelib->data[rinfo->offset];

  offset = rinfo->offset + header->interface_blob_size
    + (blob->n_prerequisites + (blob->n_prerequisites % 2)) * 2
    + blob->n_properties * header->property_blob_size;

  return find_method ((GIBaseInfo*)info, offset, blob->n_methods, name);
}

gint
g_interface_info_get_n_signals (GIInterfaceInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  InterfaceBlob *blob = (InterfaceBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->n_signals;
}

GISignalInfo *
g_interface_info_get_signal (GIInterfaceInfo *info,
			     gint            n)
{
  gint offset;
  GIRealInfo *rinfo = (GIRealInfo *)info;
  Header *header = (Header *)rinfo->typelib->data;
  InterfaceBlob *blob = (InterfaceBlob *)&rinfo->typelib->data[rinfo->offset];

  offset = rinfo->offset + header->interface_blob_size
    + (blob->n_prerequisites + (blob->n_prerequisites % 2)) * 2
    + blob->n_properties * header->property_blob_size
    + blob->n_methods * header->function_blob_size
    + n * header->signal_blob_size;

  return (GISignalInfo *) g_info_new (GI_INFO_TYPE_SIGNAL, (GIBaseInfo*)info,
				      rinfo->typelib, offset);
}

gint
g_interface_info_get_n_vfuncs (GIInterfaceInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  InterfaceBlob *blob = (InterfaceBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->n_vfuncs;
}

GIVFuncInfo *
g_interface_info_get_vfunc (GIInterfaceInfo *info,
			    gint            n)
{
  gint offset;
  GIRealInfo *rinfo = (GIRealInfo *)info;
  Header *header = (Header *)rinfo->typelib->data;
  InterfaceBlob *blob = (InterfaceBlob *)&rinfo->typelib->data[rinfo->offset];

  offset = rinfo->offset + header->interface_blob_size
    + (blob->n_prerequisites + (blob->n_prerequisites % 2)) * 2
    + blob->n_properties * header->property_blob_size
    + blob->n_methods * header->function_blob_size
    + blob->n_signals * header->signal_blob_size
    + n * header->vfunc_blob_size;

  return (GIVFuncInfo *) g_info_new (GI_INFO_TYPE_VFUNC, (GIBaseInfo*)info,
				     rinfo->typelib, offset);
}

/**
 * g_interface_info_find_vfunc:
 * @info: a #GIObjectInfo
 * @name: The name of a virtual function to find.
 *
 * Locate a virtual function slot with name @name. See the documentation
 * for g_object_info_find_vfunc() for more information on virtuals.
 *
 * Returns: (transfer full): the #GIVFuncInfo, or %NULL. Free it with
 * g_base_info_unref() when done.
 */
GIVFuncInfo *
g_interface_info_find_vfunc (GIInterfaceInfo *info,
                             const gchar  *name)
{
  gint offset;
  GIRealInfo *rinfo = (GIRealInfo *)info;
  Header *header = (Header *)rinfo->typelib->data;
  InterfaceBlob *blob = (InterfaceBlob *)&rinfo->typelib->data[rinfo->offset];

  offset = rinfo->offset + header->interface_blob_size
    + (blob->n_prerequisites + blob->n_prerequisites % 2) * 2
    + blob->n_properties * header->property_blob_size
    + blob->n_methods * header->function_blob_size
    + blob->n_signals * header->signal_blob_size;

  return find_vfunc (rinfo, offset, blob->n_vfuncs, name);
}

gint
g_interface_info_get_n_constants (GIInterfaceInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  InterfaceBlob *blob = (InterfaceBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->n_constants;
}

GIConstantInfo *
g_interface_info_get_constant (GIInterfaceInfo *info,
			       gint             n)
{
  gint offset;
  GIRealInfo *rinfo = (GIRealInfo *)info;
  Header *header = (Header *)rinfo->typelib->data;
  InterfaceBlob *blob = (InterfaceBlob *)&rinfo->typelib->data[rinfo->offset];

  offset = rinfo->offset + header->interface_blob_size
    + (blob->n_prerequisites + (blob->n_prerequisites % 2)) * 2
    + blob->n_properties * header->property_blob_size
    + blob->n_methods * header->function_blob_size
    + blob->n_signals * header->signal_blob_size
    + blob->n_vfuncs * header->vfunc_blob_size
    + n * header->constant_blob_size;

  return (GIConstantInfo *) g_info_new (GI_INFO_TYPE_CONSTANT, (GIBaseInfo*)info,
					rinfo->typelib, offset);
}

/**
 * g_interface_info_get_iface_struct:
 * @info: a #GIInterfaceInfo
 *
 * Returns the layout C structure associated with this #GInterface.
 *
 * Returns: (transfer full): the #GIStructInfo or %NULL. Free it with
 * g_base_info_unref() when done.
 */
GIStructInfo *
g_interface_info_get_iface_struct (GIInterfaceInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  InterfaceBlob *blob = (InterfaceBlob *)&rinfo->typelib->data[rinfo->offset];

  if (blob->gtype_struct)
    return (GIStructInfo *) _g_info_from_entry (rinfo->repository,
                                                rinfo->typelib, blob->gtype_struct);
  else
    return NULL;
}

/* GIPropertyInfo functions */
GParamFlags
g_property_info_get_flags (GIPropertyInfo *info)
{
  GParamFlags flags;
  GIRealInfo *rinfo = (GIRealInfo *)info;
  PropertyBlob *blob = (PropertyBlob *)&rinfo->typelib->data[rinfo->offset];

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
  GIRealInfo *rinfo = (GIRealInfo *)info;

  return g_type_info_new ((GIBaseInfo*)info, rinfo->typelib, rinfo->offset + G_STRUCT_OFFSET (PropertyBlob, type));
}


/* GISignalInfo functions */
GSignalFlags
g_signal_info_get_flags (GISignalInfo *info)
{
  GSignalFlags flags;

  GIRealInfo *rinfo = (GIRealInfo *)info;
  SignalBlob *blob = (SignalBlob *)&rinfo->typelib->data[rinfo->offset];

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
  GIRealInfo *rinfo = (GIRealInfo *)info;
  SignalBlob *blob = (SignalBlob *)&rinfo->typelib->data[rinfo->offset];

  if (blob->has_class_closure)
    return g_interface_info_get_vfunc ((GIInterfaceInfo *)rinfo->container, blob->class_closure);

  return NULL;
}

gboolean
g_signal_info_true_stops_emit (GISignalInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  SignalBlob *blob = (SignalBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->true_stops_emit;
}

/* GIVFuncInfo functions */
GIVFuncInfoFlags
g_vfunc_info_get_flags (GIVFuncInfo *info)
{
  GIVFuncInfoFlags flags;

  GIRealInfo *rinfo = (GIRealInfo *)info;
  VFuncBlob *blob = (VFuncBlob *)&rinfo->typelib->data[rinfo->offset];

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
  GIRealInfo *rinfo = (GIRealInfo *)info;
  VFuncBlob *blob = (VFuncBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->struct_offset;
}

GISignalInfo *
g_vfunc_info_get_signal (GIVFuncInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  VFuncBlob *blob = (VFuncBlob *)&rinfo->typelib->data[rinfo->offset];

  if (blob->class_closure)
    return g_interface_info_get_signal ((GIInterfaceInfo *)rinfo->container, blob->signal);

  return NULL;
}

/**
 * g_vfunc_info_get_invoker:
 * @info: a #GIVFuncInfo
 *
 * If this virtual function has an associated invoker method, this
 * method will return it.  An invoker method is a C entry point.
 *
 * Not all virtuals will have invokers.
 *
 * Returns: (transfer full): the #GIVFuncInfo or %NULL. Free it with
 * g_base_info_unref() when done.
 */
GIFunctionInfo *
g_vfunc_info_get_invoker (GIVFuncInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  VFuncBlob *blob = (VFuncBlob *)&rinfo->typelib->data[rinfo->offset];
  GIBaseInfo *container = rinfo->container;
  GIInfoType parent_type;

  /* 1023 = 0x3ff is the maximum of the 10 bits for invoker index */
  if (blob->invoker == 1023)
    return NULL;

  parent_type = g_base_info_get_type (container);
  if (parent_type == GI_INFO_TYPE_OBJECT)
    return g_object_info_get_method ((GIObjectInfo*)container, blob->invoker);
  else if (parent_type == GI_INFO_TYPE_INTERFACE)
    return g_interface_info_get_method ((GIInterfaceInfo*)container, blob->invoker);
  else
    g_assert_not_reached ();
}

/* GIConstantInfo functions */
GITypeInfo *
g_constant_info_get_type (GIConstantInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;

  return g_type_info_new ((GIBaseInfo*)info, rinfo->typelib, rinfo->offset + 8);
}

gint
g_constant_info_get_value (GIConstantInfo *info,
			   GArgument      *value)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  ConstantBlob *blob = (ConstantBlob *)&rinfo->typelib->data[rinfo->offset];

  /* FIXME non-basic types ? */
  if (blob->type.flags.reserved == 0 && blob->type.flags.reserved2 == 0)
    {
      if (blob->type.flags.pointer)
	value->v_pointer = g_memdup (&rinfo->typelib->data[blob->offset], blob->size);
      else
	{
	  switch (blob->type.flags.tag)
	    {
	    case GI_TYPE_TAG_BOOLEAN:
	      value->v_boolean = *(gboolean*)&rinfo->typelib->data[blob->offset];
	      break;
	    case GI_TYPE_TAG_INT8:
	      value->v_int8 = *(gint8*)&rinfo->typelib->data[blob->offset];
	      break;
	    case GI_TYPE_TAG_UINT8:
	      value->v_uint8 = *(guint8*)&rinfo->typelib->data[blob->offset];
	      break;
	    case GI_TYPE_TAG_INT16:
	      value->v_int16 = *(gint16*)&rinfo->typelib->data[blob->offset];
	      break;
	    case GI_TYPE_TAG_UINT16:
	      value->v_uint16 = *(guint16*)&rinfo->typelib->data[blob->offset];
	      break;
	    case GI_TYPE_TAG_INT32:
	      value->v_int32 = *(gint32*)&rinfo->typelib->data[blob->offset];
	      break;
	    case GI_TYPE_TAG_UINT32:
	      value->v_uint32 = *(guint32*)&rinfo->typelib->data[blob->offset];
	      break;
	    case GI_TYPE_TAG_INT64:
	      value->v_int64 = *(gint64*)&rinfo->typelib->data[blob->offset];
	      break;
	    case GI_TYPE_TAG_UINT64:
	      value->v_uint64 = *(guint64*)&rinfo->typelib->data[blob->offset];
	      break;
	    case GI_TYPE_TAG_FLOAT:
	      value->v_float = *(gfloat*)&rinfo->typelib->data[blob->offset];
	      break;
	    case GI_TYPE_TAG_DOUBLE:
	      value->v_double = *(gdouble*)&rinfo->typelib->data[blob->offset];
	      break;
	    case GI_TYPE_TAG_TIME_T:
	      value->v_long = *(long*)&rinfo->typelib->data[blob->offset];
	      break;
	    case GI_TYPE_TAG_SHORT:
	      value->v_short = *(gshort*)&rinfo->typelib->data[blob->offset];
	      break;
	    case GI_TYPE_TAG_USHORT:
	      value->v_ushort = *(gushort*)&rinfo->typelib->data[blob->offset];
	      break;
	    case GI_TYPE_TAG_INT:
	      value->v_int = *(gint*)&rinfo->typelib->data[blob->offset];
	      break;
	    case GI_TYPE_TAG_UINT:
	      value->v_uint = *(guint*)&rinfo->typelib->data[blob->offset];
	      break;
	    case GI_TYPE_TAG_LONG:
	      value->v_long = *(glong*)&rinfo->typelib->data[blob->offset];
	      break;
	    case GI_TYPE_TAG_ULONG:
	      value->v_ulong = *(gulong*)&rinfo->typelib->data[blob->offset];
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
  GIRealInfo *rinfo = (GIRealInfo *)info;
  UnionBlob *blob = (UnionBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->n_fields;
}

GIFieldInfo *
g_union_info_get_field (GIUnionInfo *info,
			gint         n)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  Header *header = (Header *)rinfo->typelib->data;

  return (GIFieldInfo *) g_info_new (GI_INFO_TYPE_FIELD, (GIBaseInfo*)info, rinfo->typelib,
				     rinfo->offset + header->union_blob_size +
				     n * header->field_blob_size);
}

gint
g_union_info_get_n_methods (GIUnionInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  UnionBlob *blob = (UnionBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->n_functions;
}

GIFunctionInfo *
g_union_info_get_method (GIUnionInfo *info,
			 gint         n)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  UnionBlob *blob = (UnionBlob *)&rinfo->typelib->data[rinfo->offset];
  Header *header = (Header *)rinfo->typelib->data;
  gint offset;

  offset = rinfo->offset + header->union_blob_size
    + blob->n_fields * header->field_blob_size
    + n * header->function_blob_size;
  return (GIFunctionInfo *) g_info_new (GI_INFO_TYPE_FUNCTION, (GIBaseInfo*)info,
					rinfo->typelib, offset);
}

gboolean
g_union_info_is_discriminated (GIUnionInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  UnionBlob *blob = (UnionBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->discriminated;
}

gint
g_union_info_get_discriminator_offset (GIUnionInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  UnionBlob *blob = (UnionBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->discriminator_offset;
}

GITypeInfo *
g_union_info_get_discriminator_type (GIUnionInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;

  return g_type_info_new ((GIBaseInfo*)info, rinfo->typelib, rinfo->offset + 24);
}

GIConstantInfo *
g_union_info_get_discriminator (GIUnionInfo *info,
				gint         n)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  UnionBlob *blob = (UnionBlob *)&rinfo->typelib->data[rinfo->offset];

  if (blob->discriminated)
    {
      Header *header = (Header *)rinfo->typelib->data;
      gint offset;

      offset = rinfo->offset + header->union_blob_size
	+ blob->n_fields * header->field_blob_size
	+ blob->n_functions * header->function_blob_size
	+ n * header->constant_blob_size;

      return (GIConstantInfo *) g_info_new (GI_INFO_TYPE_CONSTANT, (GIBaseInfo*)info,
					    rinfo->typelib, offset);
    }

  return NULL;
}

GIFunctionInfo *
g_union_info_find_method (GIUnionInfo *info,
                          const gchar *name)
{
  gint offset;
  GIRealInfo *rinfo = (GIRealInfo *)info;
  Header *header = (Header *)rinfo->typelib->data;
  UnionBlob *blob = (UnionBlob *)&rinfo->typelib->data[rinfo->offset];

  offset = rinfo->offset + header->union_blob_size
    + blob->n_fields * header->field_blob_size;

  return find_method ((GIBaseInfo*)info, offset, blob->n_functions, name);
}

gsize
g_union_info_get_size (GIUnionInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  UnionBlob *blob = (UnionBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->size;
}

gsize
g_union_info_get_alignment (GIUnionInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  UnionBlob *blob = (UnionBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->alignment;
}
