/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 * GObject introspection: Argument implementation
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

#include "config.h"

#include <glib.h>

#include "gitypelib-internal.h"
#include "girepository-private.h"
#include "giarginfo.h"


/* GIArgInfo functions */

/**
 * SECTION:giarginfo
 * @title: GIArgInfo
 * @short_description: Struct representing an argument
 *
 * GIArgInfo represents an argument of a callable.
 *
 * An argument is always part of a #GICallableInfo.
 */

/**
 * gi_arg_info_get_direction:
 * @info: a #GIArgInfo
 *
 * Obtain the direction of the argument. Check #GIDirection for possible
 * direction values.
 *
 * Returns: the direction
 */
GIDirection
gi_arg_info_get_direction (GIArgInfo *info)
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
 * gi_arg_info_is_return_value:
 * @info: a #GIArgInfo
 *
 * Obtain if the argument is a return value. It can either be a
 * parameter or a return value.
 *
 * Returns: %TRUE if it is a return value
 */
gboolean
gi_arg_info_is_return_value (GIArgInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  ArgBlob *blob;

  g_return_val_if_fail (info != NULL, FALSE);
  g_return_val_if_fail (GI_IS_ARG_INFO (info), FALSE);

  blob = (ArgBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->return_value;
}

/**
 * gi_arg_info_is_caller_allocates:
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
gi_arg_info_is_caller_allocates (GIArgInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  ArgBlob *blob;

  g_return_val_if_fail (info != NULL, FALSE);
  g_return_val_if_fail (GI_IS_ARG_INFO (info), FALSE);

  blob = (ArgBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->caller_allocates;
}

/**
 * gi_arg_info_is_optional:
 * @info: a #GIArgInfo
 *
 * Obtain if the argument is optional.  For 'out' arguments this means
 * that you can pass %NULL in order to ignore the result.
 *
 * Returns: %TRUE if it is an optional argument
 */
gboolean
gi_arg_info_is_optional (GIArgInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  ArgBlob *blob;

  g_return_val_if_fail (info != NULL, FALSE);
  g_return_val_if_fail (GI_IS_ARG_INFO (info), FALSE);

  blob = (ArgBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->optional;
}

/**
 * gi_arg_info_may_be_null:
 * @info: a #GIArgInfo
 *
 * Obtain if the type of the argument includes the possibility of %NULL.
 * For 'in' values this means that %NULL is a valid value.  For 'out'
 * values, this means that %NULL may be returned.
 *
 * See also gi_arg_info_is_optional().
 *
 * Returns: %TRUE if the value may be %NULL
 */
gboolean
gi_arg_info_may_be_null (GIArgInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  ArgBlob *blob;

  g_return_val_if_fail (info != NULL, FALSE);
  g_return_val_if_fail (GI_IS_ARG_INFO (info), FALSE);

  blob = (ArgBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->nullable;
}

/**
 * gi_arg_info_is_skip:
 * @info: a #GIArgInfo
 *
 * Obtain if an argument is only useful in C.
 *
 * Returns: %TRUE if argument is only useful in C.
 * Since: 1.30
 */
gboolean
gi_arg_info_is_skip (GIArgInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  ArgBlob *blob;

  g_return_val_if_fail (info != NULL, FALSE);
  g_return_val_if_fail (GI_IS_ARG_INFO (info), FALSE);

  blob = (ArgBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->skip;
}

/**
 * gi_arg_info_get_ownership_transfer:
 * @info: a #GIArgInfo
 *
 * Obtain the ownership transfer for this argument.
 * #GITransfer contains a list of possible values.
 *
 * Returns: the transfer
 */
GITransfer
gi_arg_info_get_ownership_transfer (GIArgInfo *info)
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
 * gi_arg_info_get_scope:
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
gi_arg_info_get_scope (GIArgInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  ArgBlob *blob;

  g_return_val_if_fail (info != NULL, -1);
  g_return_val_if_fail (GI_IS_ARG_INFO (info), -1);

  blob = (ArgBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->scope;
}

/**
 * gi_arg_info_get_closure:
 * @info: a #GIArgInfo
 *
 * Obtain the index of the user data argument. This is only valid
 * for arguments which are callbacks.
 *
 * Returns: index of the user data argument or -1 if there is none
 */
gint
gi_arg_info_get_closure (GIArgInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  ArgBlob *blob;

  g_return_val_if_fail (info != NULL, -1);
  g_return_val_if_fail (GI_IS_ARG_INFO (info), -1);

  blob = (ArgBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->closure;
}

/**
 * gi_arg_info_get_destroy:
 * @info: a #GIArgInfo
 *
 * Obtains the index of the #GDestroyNotify argument. This is only valid
 * for arguments which are callbacks.
 *
 * Returns: index of the #GDestroyNotify argument or -1 if there is none
 */
gint
gi_arg_info_get_destroy (GIArgInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  ArgBlob *blob;

  g_return_val_if_fail (info != NULL, -1);
  g_return_val_if_fail (GI_IS_ARG_INFO (info), -1);

  blob = (ArgBlob *)&rinfo->typelib->data[rinfo->offset];

  return blob->destroy;
}

/**
 * gi_arg_info_get_type:
 * @info: a #GIArgInfo
 *
 * Obtain the type information for @info.
 *
 * Returns: (transfer full): the #GITypeInfo holding the type
 *   information for @info, free it with gi_base_info_unref()
 *   when done.
 */
GITypeInfo *
gi_arg_info_get_type (GIArgInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;

  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (GI_IS_ARG_INFO (info), NULL);

  return _gi_type_info_new ((GIBaseInfo*)info, rinfo->typelib, rinfo->offset + G_STRUCT_OFFSET (ArgBlob, arg_type));
}

/**
 * gi_arg_info_load_type:
 * @info: a #GIArgInfo
 * @type: (out caller-allocates): Initialized with information about type of @info
 *
 * Obtain information about a the type of given argument @info; this
 * function is a variant of gi_arg_info_get_type() designed for stack
 * allocation.
 *
 * The initialized @type must not be referenced after @info is deallocated.
 */
void
gi_arg_info_load_type (GIArgInfo  *info,
                       GITypeInfo *type)
{
  GIRealInfo *rinfo = (GIRealInfo*) info;

  g_return_if_fail (info != NULL);
  g_return_if_fail (GI_IS_ARG_INFO (info));

  _gi_type_info_init (type, (GIBaseInfo*)info, rinfo->typelib, rinfo->offset + G_STRUCT_OFFSET (ArgBlob, arg_type));
}
