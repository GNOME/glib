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

#include <glib.h>

#include <girepository.h>
#include "girepository-private.h"
#include "gitypelib-internal.h"

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

  return _g_type_info_new ((GIBaseInfo*)info, rinfo->typelib, offset);
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

  _g_type_info_init (type, (GIBaseInfo*)info, rinfo->typelib, offset);
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
