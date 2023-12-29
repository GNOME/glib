/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 * GObject introspection: Callable implementation
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

#include <stdlib.h>

#include <glib.h>

#include <girepository/girepository.h>
#include "gibaseinfo-private.h"
#include "girepository-private.h"
#include <gitypelib/gitypelib-private.h>
#include "girffi.h"
#include "gicallableinfo.h"

/* GICallableInfo functions */

/**
 * GICallableInfo:
 *
 * `GICallableInfo` represents an entity which is callable.
 *
 * Examples of callable are:
 *
 *  - functions ([class@GIRepository.FunctionInfo])
 *  - virtual functions ([class@GIRepository.VFuncInfo])
 *  - callbacks ([class@GIRepository.CallbackInfo]).
 *
 * A callable has a list of arguments ([class@GIRepository.ArgInfo]), a return
 * type, direction and a flag which decides if it returns `NULL`.
 *
 * Since: 2.80
 */

static guint32
signature_offset (GICallableInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo*)info;
  int sigoff = -1;

  switch (gi_base_info_get_info_type ((GIBaseInfo *) info))
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
    default:
      g_assert_not_reached ();
    }
  if (sigoff >= 0)
    return *(guint32 *)&rinfo->typelib->data[rinfo->offset + sigoff];
  return 0;
}

/**
 * gi_callable_info_can_throw_gerror:
 * @info: a #GICallableInfo
 *
 * Whether the callable can throw a [type@GLib.Error]
 *
 * Returns: `TRUE` if this `GICallableInfo` can throw a [type@GLib.Error]
 * Since: 2.80
 */
gboolean
gi_callable_info_can_throw_gerror (GICallableInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo*)info;
  SignatureBlob *signature;

  signature = (SignatureBlob *)&rinfo->typelib->data[signature_offset (info)];
  if (signature->throws)
    return TRUE;

  /* Functions and VFuncs store "throws" in their own blobs.
   * This info was additionally added to the SignatureBlob
   * to support the other callables. For Functions and VFuncs,
   * also check their legacy flag for compatibility.
   */
  switch (gi_base_info_get_info_type ((GIBaseInfo *) info)) {
  case GI_INFO_TYPE_FUNCTION:
    {
      FunctionBlob *blob;
      blob = (FunctionBlob *)&rinfo->typelib->data[rinfo->offset];
      return blob->throws;
    }
  case GI_INFO_TYPE_VFUNC:
    {
      VFuncBlob *blob;
      blob = (VFuncBlob *)&rinfo->typelib->data[rinfo->offset];
      return blob->throws;
    }
  case GI_INFO_TYPE_CALLBACK:
  case GI_INFO_TYPE_SIGNAL:
    return FALSE;
  default:
    g_assert_not_reached ();
  }
}

/**
 * gi_callable_info_is_method:
 * @info: a #GICallableInfo
 *
 * Determines if the callable info is a method.
 *
 * For [class@GIRepository.VFuncInfo]s, [class@GIRepository.CallbackInfo]s, and
 * [class@GIRepository.SignalInfo]s, this is always true. Otherwise, this looks
 * at the `GI_FUNCTION_IS_METHOD` flag on the [class@GIRepository.FunctionInfo].
 *
 * Concretely, this function returns whether
 * [method@GIRepository.CallableInfo.get_n_args] matches the number of arguments
 * in the raw C method. For methods, there is one more C argument than is
 * exposed by introspection: the `self` or `this` object.
 *
 * Returns: `TRUE` if @info is a method, `FALSE` otherwise
 * Since: 2.80
 */
gboolean
gi_callable_info_is_method (GICallableInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo*)info;
  switch (gi_base_info_get_info_type ((GIBaseInfo *) info)) {
  case GI_INFO_TYPE_FUNCTION:
    {
      FunctionBlob *blob;
      blob = (FunctionBlob *)&rinfo->typelib->data[rinfo->offset];
      return (!blob->constructor && !blob->is_static);
    }
  case GI_INFO_TYPE_VFUNC:
  case GI_INFO_TYPE_SIGNAL:
    return TRUE;
  case GI_INFO_TYPE_CALLBACK:
    return FALSE;
  default:
    g_assert_not_reached ();
  }
}

/**
 * gi_callable_info_get_return_type:
 * @info: a #GICallableInfo
 *
 * Obtain the return type of a callable item as a [class@GIRepository.TypeInfo].
 *
 * Returns: (transfer full): the [class@GIRepository.TypeInfo]. Free the struct
 *   by calling [method@GIRepository.BaseInfo.unref] when done.
 * Since: 2.80
 */
GITypeInfo *
gi_callable_info_get_return_type (GICallableInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  guint32 offset;

  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (GI_IS_CALLABLE_INFO (info), NULL);

  offset = signature_offset (info);

  return gi_type_info_new ((GIBaseInfo*)info, rinfo->typelib, offset);
}

/**
 * gi_callable_info_load_return_type:
 * @info: a #GICallableInfo
 * @type: (out caller-allocates): Initialized with return type of @info
 *
 * Obtain information about a return value of callable; this
 * function is a variant of [method@GIRepository.CallableInfo.get_return_type]
 * designed for stack allocation.
 *
 * The initialized @type must not be referenced after @info is deallocated.
 *
 * Since: 2.80
 */
void
gi_callable_info_load_return_type (GICallableInfo *info,
                                   GITypeInfo     *type)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  guint32 offset;

  g_return_if_fail (info != NULL);
  g_return_if_fail (GI_IS_CALLABLE_INFO (info));

  offset = signature_offset (info);

  gi_type_info_init ((GIBaseInfo *) type, (GIBaseInfo*)info, rinfo->typelib, offset);
}

/**
 * gi_callable_info_may_return_null:
 * @info: a #GICallableInfo
 *
 * See if a callable could return `NULL`.
 *
 * Returns: `TRUE` if callable could return `NULL`
 * Since: 2.80
 */
gboolean
gi_callable_info_may_return_null (GICallableInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  SignatureBlob *blob;

  g_return_val_if_fail (info != NULL, FALSE);
  g_return_val_if_fail (GI_IS_CALLABLE_INFO (info), FALSE);

  blob = (SignatureBlob *)&rinfo->typelib->data[signature_offset (info)];

  return blob->may_return_null;
}

/**
 * gi_callable_info_skip_return:
 * @info: a #GICallableInfo
 *
 * See if a callable’s return value is only useful in C.
 *
 * Returns: `TRUE` if return value is only useful in C.
 * Since: 2.80
 */
gboolean
gi_callable_info_skip_return (GICallableInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  SignatureBlob *blob;

  g_return_val_if_fail (info != NULL, FALSE);
  g_return_val_if_fail (GI_IS_CALLABLE_INFO (info), FALSE);

  blob = (SignatureBlob *)&rinfo->typelib->data[signature_offset (info)];

  return blob->skip_return;
}

/**
 * gi_callable_info_get_caller_owns:
 * @info: a #GICallableInfo
 *
 * See whether the caller owns the return value of this callable.
 *
 * [type@GIRepository.Transfer] contains a list of possible transfer values.
 *
 * Returns: the transfer mode for the return value of the callable
 * Since: 2.80
 */
GITransfer
gi_callable_info_get_caller_owns (GICallableInfo *info)
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
 * gi_callable_info_get_instance_ownership_transfer:
 * @info: a #GICallableInfo
 *
 * Obtains the ownership transfer for the instance argument.
 *
 * [type@GIRepository.Transfer] contains a list of possible transfer values.
 *
 * Returns: the transfer mode of the instance argument
 * Since: 2.80
 */
GITransfer
gi_callable_info_get_instance_ownership_transfer (GICallableInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo*) info;
  SignatureBlob *blob;

  g_return_val_if_fail (info != NULL, -1);
  g_return_val_if_fail (GI_IS_CALLABLE_INFO (info), -1);

  blob = (SignatureBlob *)&rinfo->typelib->data[signature_offset (info)];

  if (blob->instance_transfer_ownership)
    return GI_TRANSFER_EVERYTHING;
  else
    return GI_TRANSFER_NOTHING;
}

/**
 * gi_callable_info_get_n_args:
 * @info: a #GICallableInfo
 *
 * Obtain the number of arguments (both ‘in’ and ‘out’) for this callable.
 *
 * Returns: The number of arguments this callable expects.
 * Since: 2.80
 */
guint
gi_callable_info_get_n_args (GICallableInfo *info)
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
 * gi_callable_info_get_arg:
 * @info: a #GICallableInfo
 * @n: the argument index to fetch
 *
 * Obtain information about a particular argument of this callable.
 *
 * Returns: (transfer full): the [class@GIRepository.ArgInfo]. Free it with
 *   [method@GIRepository.BaseInfo.unref] when done.
 * Since: 2.80
 */
GIArgInfo *
gi_callable_info_get_arg (GICallableInfo *info,
                          guint           n)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  Header *header;
  gint offset;

  g_return_val_if_fail (info != NULL, NULL);
  g_return_val_if_fail (GI_IS_CALLABLE_INFO (info), NULL);

  offset = signature_offset (info);
  header = (Header *)rinfo->typelib->data;

  return (GIArgInfo *) gi_info_new (GI_INFO_TYPE_ARG, (GIBaseInfo*)info, rinfo->typelib,
                                    offset + header->signature_blob_size + n * header->arg_blob_size);
}

/**
 * gi_callable_info_load_arg:
 * @info: a #GICallableInfo
 * @n: the argument index to fetch
 * @arg: (out caller-allocates): Initialize with argument number @n
 *
 * Obtain information about a particular argument of this callable; this
 * function is a variant of [method@GIRepository.CallableInfo.get_arg] designed
 * for stack allocation.
 *
 * The initialized @arg must not be referenced after @info is deallocated.
 *
 * Since: 2.80
 */
void
gi_callable_info_load_arg (GICallableInfo *info,
                           guint           n,
                           GIArgInfo      *arg)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  Header *header;
  gint offset;

  g_return_if_fail (info != NULL);
  g_return_if_fail (GI_IS_CALLABLE_INFO (info));

  offset = signature_offset (info);
  header = (Header *)rinfo->typelib->data;

  gi_info_init ((GIRealInfo*)arg, GI_INFO_TYPE_ARG, rinfo->repository, (GIBaseInfo*)info, rinfo->typelib,
                offset + header->signature_blob_size + n * header->arg_blob_size);
}

/**
 * gi_callable_info_get_return_attribute:
 * @info: a #GICallableInfo
 * @name: a freeform string naming an attribute
 *
 * Retrieve an arbitrary attribute associated with the return value.
 *
 * Returns: (nullable): The value of the attribute, or `NULL` if no such
 *   attribute exists
 * Since: 2.80
 */
const gchar *
gi_callable_info_get_return_attribute (GICallableInfo *info,
                                       const gchar    *name)
{
  GIAttributeIter iter = { 0, };
  const char *curname, *curvalue;
  while (gi_callable_info_iterate_return_attributes (info, &iter, &curname, &curvalue))
    {
      if (g_strcmp0 (name, curname) == 0)
        return (const gchar*) curvalue;
    }

  return NULL;
}

/**
 * gi_callable_info_iterate_return_attributes:
 * @info: a #GICallableInfo
 * @iterator: (inout): a [type@GIRepository.AttributeIter] structure, must be
 *   initialized; see below
 * @name: (out) (transfer none): Returned name, must not be freed
 * @value: (out) (transfer none): Returned name, must not be freed
 *
 * Iterate over all attributes associated with the return value.
 *
 * The iterator structure is typically stack allocated, and must have its
 * first member initialized to `NULL`.
 *
 * Both the @name and @value should be treated as constants
 * and must not be freed.
 *
 * See [method@GIRepository.BaseInfo.iterate_attributes] for an example of how
 * to use a similar API.
 *
 * Returns: `TRUE` if there are more attributes
 * Since: 2.80
 */
gboolean
gi_callable_info_iterate_return_attributes (GICallableInfo   *info,
                                            GIAttributeIter  *iterator,
                                            const char      **name,
                                            const char      **value)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  Header *header = (Header *)rinfo->typelib->data;
  AttributeBlob *next, *after;
  guint32 blob_offset;

  after = (AttributeBlob *) &rinfo->typelib->data[header->attributes +
                                                  header->n_attributes * header->attribute_blob_size];

  blob_offset = signature_offset (info);

  if (iterator->data != NULL)
    next = (AttributeBlob *) iterator->data;
  else
    next = gi_typelib_attribute_blob_find_first (((GIBaseInfo *) info)->typelib, blob_offset);

  if (next == NULL || next->offset != blob_offset || next >= after)
    return FALSE;

  *name = gi_typelib_get_string (rinfo->typelib, next->name);
  *value = gi_typelib_get_string (rinfo->typelib, next->value);
  iterator->data = next + 1;

  return TRUE;
}

/**
 * gi_type_tag_extract_ffi_return_value:
 * @return_tag: [type@GIRepository.TypeTag] of the return value
 * @interface_type: [type@GIRepository.InfoType] of the underlying interface type
 * @ffi_value: pointer to [type@GIRepository.FFIReturnValue] union containing
 *   the return value from `ffi_call()`
 * @arg: (out caller-allocates): pointer to an allocated
 *   [class@GIRepository.Argument]
 *
 * Extract the correct bits from an `ffi_arg` return value into
 * [class@GIRepository.Argument].
 *
 * See: https://bugzilla.gnome.org/show_bug.cgi?id=665152
 *
 * Also see [`ffi_call()`](man:ffi_call(3)): the storage requirements for return
 * values are ‘special’.
 *
 * The @interface_type argument only applies if @return_tag is
 * `GI_TYPE_TAG_INTERFACE`. Otherwise it is ignored.
 *
 * Since: 2.80
 */
void
gi_type_tag_extract_ffi_return_value (GITypeTag         return_tag,
                                      GIInfoType        interface_type,
                                      GIFFIReturnValue *ffi_value,
                                      GIArgument       *arg)
{
    switch (return_tag) {
    case GI_TYPE_TAG_INT8:
        arg->v_int8 = (gint8) ffi_value->v_long;
        break;
    case GI_TYPE_TAG_UINT8:
        arg->v_uint8 = (guint8) ffi_value->v_ulong;
        break;
    case GI_TYPE_TAG_INT16:
        arg->v_int16 = (gint16) ffi_value->v_long;
        break;
    case GI_TYPE_TAG_UINT16:
        arg->v_uint16 = (guint16) ffi_value->v_ulong;
        break;
    case GI_TYPE_TAG_INT32:
        arg->v_int32 = (gint32) ffi_value->v_long;
        break;
    case GI_TYPE_TAG_UINT32:
    case GI_TYPE_TAG_BOOLEAN:
    case GI_TYPE_TAG_UNICHAR:
        arg->v_uint32 = (guint32) ffi_value->v_ulong;
        break;
    case GI_TYPE_TAG_INT64:
        arg->v_int64 = (gint64) ffi_value->v_int64;
        break;
    case GI_TYPE_TAG_UINT64:
        arg->v_uint64 = (guint64) ffi_value->v_uint64;
        break;
    case GI_TYPE_TAG_FLOAT:
        arg->v_float = ffi_value->v_float;
        break;
    case GI_TYPE_TAG_DOUBLE:
        arg->v_double = ffi_value->v_double;
        break;
    case GI_TYPE_TAG_INTERFACE:
        switch(interface_type) {
        case GI_INFO_TYPE_ENUM:
        case GI_INFO_TYPE_FLAGS:
            arg->v_int32 = (gint32) ffi_value->v_long;
            break;
        default:
            arg->v_pointer = (gpointer) ffi_value->v_pointer;
            break;
        }
        break;
    default:
        arg->v_pointer = (gpointer) ffi_value->v_pointer;
        break;
    }
}

/**
 * gi_type_info_extract_ffi_return_value:
 * @return_info: [type@GIRepository.TypeInfo] describing the return type
 * @ffi_value: pointer to [type@GIRepository.FFIReturnValue] union containing
 *   the return value from `ffi_call()`
 * @arg: (out caller-allocates): pointer to an allocated
 *   [class@GIRepository.Argument]
 *
 * Extract the correct bits from an `ffi_arg` return value into
 * [class@GIRepository.Argument].
 *
 * See: https://bugzilla.gnome.org/show_bug.cgi?id=665152
 *
 * Also see [`ffi_call()`](man:ffi_call(3)): the storage requirements for return
 * values are ‘special’.
 *
 * Since: 2.80
 */
void
gi_type_info_extract_ffi_return_value (GITypeInfo       *return_info,
                                       GIFFIReturnValue *ffi_value,
                                       GIArgument       *arg)
{
  GITypeTag return_tag = gi_type_info_get_tag (return_info);
  GIInfoType interface_type = GI_INFO_TYPE_INVALID;

  if (return_tag == GI_TYPE_TAG_INTERFACE)
    {
      GIBaseInfo *interface_info = gi_type_info_get_interface (return_info);
      interface_type = gi_base_info_get_info_type (interface_info);
      gi_base_info_unref (interface_info);
    }

  gi_type_tag_extract_ffi_return_value (return_tag, interface_type,
                                        ffi_value, arg);
}

/**
 * gi_callable_info_invoke:
 * @info: a #GICallableInfo
 * @function: function pointer to call
 * @in_args: (array length=n_in_args): array of ‘in’ arguments
 * @n_in_args: number of arguments in @in_args
 * @out_args: (array length=n_out_args): array of ‘out’ arguments allocated by
 *   the caller, to be populated with outputted values
 * @n_out_args: number of arguments in @out_args
 * @return_value: (out caller-allocates) (not optional) (nullable): return
 *   location for the return value from the callable; `NULL` may be returned if
 *   the callable returns that
 * @is_method: `TRUE` if @info is a method
 * @throws: `TRUE` if @info may throw a [type@GLib.Error]
 * @error: return location for a [type@GLib.Error], or `NULL`
 *
 * Invoke the given `GICallableInfo` by calling the given @function pointer.
 *
 * The set of arguments passed to @function will be constructed according to the
 * introspected type of the `GICallableInfo`, using @in_args, @out_args,
 * @is_method, @throws and @error.
 *
 * Returns: `TRUE` if the callable was executed successfully and didn’t throw
 *   a [type@GLib.Error]; `FALSE` if @error is set
 * Since: 2.80
 */
gboolean
gi_callable_info_invoke (GICallableInfo    *info,
                         gpointer           function,
                         const GIArgument  *in_args,
                         gsize              n_in_args,
                         const GIArgument  *out_args,
                         gsize              n_out_args,
                         GIArgument        *return_value,
                         gboolean           is_method,
                         gboolean           throws,
                         GError           **error)
{
  ffi_cif cif;
  ffi_type *rtype;
  ffi_type **atypes;
  GITypeInfo *tinfo;
  GITypeInfo *rinfo;
  GITypeTag rtag;
  GIArgInfo *ainfo;
  gsize n_args, n_invoke_args, in_pos, out_pos, i;
  gpointer *args;
  gboolean success = FALSE;
  GError *local_error = NULL;
  gpointer error_address = &local_error;
  GIFFIReturnValue ffi_return_value;
  gpointer return_value_p; /* Will point inside the union return_value */

  rinfo = gi_callable_info_get_return_type ((GICallableInfo *)info);
  rtype = gi_type_info_get_ffi_type (rinfo);
  rtag = gi_type_info_get_tag(rinfo);

  in_pos = 0;
  out_pos = 0;

  n_args = gi_callable_info_get_n_args ((GICallableInfo *)info);
  if (is_method)
    {
      if (n_in_args == 0)
        {
          g_set_error (error,
                       GI_INVOKE_ERROR,
                       GI_INVOKE_ERROR_ARGUMENT_MISMATCH,
                       "Too few \"in\" arguments (handling this)");
          goto out;
        }
      n_invoke_args = n_args+1;
      in_pos++;
    }
  else
    n_invoke_args = n_args;

  if (throws)
    /* Add an argument for the GError */
    n_invoke_args ++;

  atypes = g_alloca (sizeof (ffi_type*) * n_invoke_args);
  args = g_alloca (sizeof (gpointer) * n_invoke_args);

  if (is_method)
    {
      atypes[0] = &ffi_type_pointer;
      args[0] = (gpointer) &in_args[0];
    }
  for (i = 0; i < n_args; i++)
    {
      int offset = (is_method ? 1 : 0);
      ainfo = gi_callable_info_get_arg ((GICallableInfo *)info, i);
      switch (gi_arg_info_get_direction (ainfo))
        {
        case GI_DIRECTION_IN:
          tinfo = gi_arg_info_get_type_info (ainfo);
          atypes[i+offset] = gi_type_info_get_ffi_type (tinfo);
          gi_base_info_unref ((GIBaseInfo *)ainfo);
          gi_base_info_unref ((GIBaseInfo *)tinfo);

          if (in_pos >= n_in_args)
            {
              g_set_error (error,
                           GI_INVOKE_ERROR,
                           GI_INVOKE_ERROR_ARGUMENT_MISMATCH,
                           "Too few \"in\" arguments (handling in)");
              goto out;
            }

          args[i+offset] = (gpointer)&in_args[in_pos];
          in_pos++;

          break;
        case GI_DIRECTION_OUT:
          atypes[i+offset] = &ffi_type_pointer;
          gi_base_info_unref ((GIBaseInfo *)ainfo);

          if (out_pos >= n_out_args)
            {
              g_set_error (error,
                           GI_INVOKE_ERROR,
                           GI_INVOKE_ERROR_ARGUMENT_MISMATCH,
                           "Too few \"out\" arguments (handling out)");
              goto out;
            }

          args[i+offset] = (gpointer)&out_args[out_pos];
          out_pos++;
          break;
        case GI_DIRECTION_INOUT:
          atypes[i+offset] = &ffi_type_pointer;
          gi_base_info_unref ((GIBaseInfo *)ainfo);

          if (in_pos >= n_in_args)
            {
              g_set_error (error,
                           GI_INVOKE_ERROR,
                           GI_INVOKE_ERROR_ARGUMENT_MISMATCH,
                           "Too few \"in\" arguments (handling inout)");
              goto out;
            }

          if (out_pos >= n_out_args)
            {
              g_set_error (error,
                           GI_INVOKE_ERROR,
                           GI_INVOKE_ERROR_ARGUMENT_MISMATCH,
                           "Too few \"out\" arguments (handling inout)");
              goto out;
            }

          args[i+offset] = (gpointer)&in_args[in_pos];
          in_pos++;
          out_pos++;
          break;
        default:
          gi_base_info_unref ((GIBaseInfo *)ainfo);
          g_assert_not_reached ();
        }
    }

  if (throws)
    {
      args[n_invoke_args - 1] = &error_address;
      atypes[n_invoke_args - 1] = &ffi_type_pointer;
    }

  if (in_pos < n_in_args)
    {
      g_set_error (error,
                   GI_INVOKE_ERROR,
                   GI_INVOKE_ERROR_ARGUMENT_MISMATCH,
                   "Too many \"in\" arguments (at end)");
      goto out;
    }
  if (out_pos < n_out_args)
    {
      g_set_error (error,
                   GI_INVOKE_ERROR,
                   GI_INVOKE_ERROR_ARGUMENT_MISMATCH,
                   "Too many \"out\" arguments (at end)");
      goto out;
    }

  if (ffi_prep_cif (&cif, FFI_DEFAULT_ABI, n_invoke_args, rtype, atypes) != FFI_OK)
    goto out;

  g_return_val_if_fail (return_value, FALSE);
  /* See comment for GIFFIReturnValue above */
  switch (rtag)
    {
    case GI_TYPE_TAG_FLOAT:
      return_value_p = &ffi_return_value.v_float;
      break;
    case GI_TYPE_TAG_DOUBLE:
      return_value_p = &ffi_return_value.v_double;
      break;
    case GI_TYPE_TAG_INT64:
    case GI_TYPE_TAG_UINT64:
      return_value_p = &ffi_return_value.v_uint64;
      break;
    default:
      return_value_p = &ffi_return_value.v_long;
    }
  ffi_call (&cif, function, return_value_p, args);

  if (local_error)
    {
      g_propagate_error (error, local_error);
      success = FALSE;
    }
  else
    {
      gi_type_info_extract_ffi_return_value (rinfo, &ffi_return_value, return_value);
      success = TRUE;
    }
 out:
  gi_base_info_unref ((GIBaseInfo *)rinfo);
  return success;
}

void
gi_callable_info_class_init (gpointer g_class,
                             gpointer class_data)
{
  GIBaseInfoClass *info_class = g_class;

  info_class->info_type = GI_INFO_TYPE_CALLABLE;
}
