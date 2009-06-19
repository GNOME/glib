/* GObject introspection: Helper functions for ffi integration
 *
 * Copyright (C) 2008 Red Hat, Inc
 * Copyright (C) 2005 Matthias Clasen
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
#include <config.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include "girffi.h"
#include "girepository.h"

ffi_type *
g_ir_ffi_get_ffi_type (GITypeTag tag)
{
  switch (tag)
    {
    case GI_TYPE_TAG_VOID:
      return &ffi_type_void;
    case GI_TYPE_TAG_BOOLEAN:
      return &ffi_type_uint;
    case GI_TYPE_TAG_INT8:
      return &ffi_type_sint8;
    case GI_TYPE_TAG_UINT8:
      return &ffi_type_uint8;
    case GI_TYPE_TAG_INT16:
      return &ffi_type_sint16;
    case GI_TYPE_TAG_UINT16:
      return &ffi_type_uint16;
    case GI_TYPE_TAG_INT32:
      return &ffi_type_sint32;
    case GI_TYPE_TAG_UINT32:
      return &ffi_type_uint32;
    case GI_TYPE_TAG_INT64:
      return &ffi_type_sint64;
    case GI_TYPE_TAG_UINT64:
      return &ffi_type_uint64;
    case GI_TYPE_TAG_SHORT:
      return &ffi_type_sshort;
    case GI_TYPE_TAG_USHORT:
      return &ffi_type_ushort;
    case GI_TYPE_TAG_INT:
      return &ffi_type_sint;
    case GI_TYPE_TAG_UINT:
      return &ffi_type_uint;
    case GI_TYPE_TAG_SSIZE:
#if GLIB_SIZEOF_SIZE_T == 4
      return &ffi_type_sint32;
#elif GLIB_SIZEOF_SIZE_T == 8
      return &ffi_type_sint64;
#else
#  error "Unexpected size for size_t: not 4 or 8"
#endif
    case GI_TYPE_TAG_LONG:
      return &ffi_type_slong;
    case GI_TYPE_TAG_SIZE:
    case GI_TYPE_TAG_GTYPE:
#if GLIB_SIZEOF_SIZE_T == 4
      return &ffi_type_uint32;
#elif GLIB_SIZEOF_SIZE_T == 8
      return &ffi_type_uint64;
#else
#  error "Unexpected size for size_t: not 4 or 8"
#endif
    case GI_TYPE_TAG_TIME_T:
#if SIZEOF_TIME_T == 4
      return &ffi_type_sint32;
#elif SIZEOF_TIME_T == 8
      return &ffi_type_sint64;
#else
#  error "Unexpected size for time_t: not 4 or 8"
#endif
    case GI_TYPE_TAG_ULONG:
      return &ffi_type_ulong;
    case GI_TYPE_TAG_FLOAT:
      return &ffi_type_float;
    case GI_TYPE_TAG_DOUBLE:
      return &ffi_type_double;
    case GI_TYPE_TAG_UTF8:
    case GI_TYPE_TAG_FILENAME:
    case GI_TYPE_TAG_ARRAY:
    case GI_TYPE_TAG_INTERFACE:
    case GI_TYPE_TAG_GLIST:
    case GI_TYPE_TAG_GSLIST:
    case GI_TYPE_TAG_GHASH:
    case GI_TYPE_TAG_ERROR:
      return &ffi_type_pointer;
    }

  g_assert_not_reached ();

  return NULL;
}

/**
 * g_callable_info_get_ffi_arg_types:
 * @callable_info: a callable info from a typelib
 *
 * Return value: an array of ffi_type*. The array itself
 * should be freed using g_free() after use.
 */
ffi_type **
g_callable_info_get_ffi_arg_types (GICallableInfo *callable_info)
{
    ffi_type **arg_types;
    gint n_args, i;
    
    g_return_val_if_fail (callable_info != NULL, NULL);

    n_args = g_callable_info_get_n_args (callable_info);
    
    arg_types = (ffi_type **) g_new0 (ffi_type *, n_args + 1);
    
    for (i = 0; i < n_args; ++i)
      {
        GIArgInfo *arg_info = g_callable_info_get_arg (callable_info, i);
        GITypeInfo *arg_type = g_arg_info_get_type (arg_info);
        GITypeTag type_tag = g_type_info_get_tag (arg_type);
        arg_types[i] = g_ir_ffi_get_ffi_type (type_tag);
        g_base_info_unref ((GIBaseInfo *)arg_info);
        g_base_info_unref ((GIBaseInfo *)arg_type);
      }
    arg_types[n_args] = NULL;

    return arg_types;
}

/**
 * g_callable_info_get_ffi_return_type:
 * @callable_info: a callable info from a typelib
 *
 * Fetches the ffi_type for a corresponding return value of
 * a #GICallableInfo
 * Return value: the ffi_type for the return value
 */
ffi_type *
g_callable_info_get_ffi_return_type (GICallableInfo *callable_info)
{
  GITypeInfo *return_type;
  GITypeTag type_tag;

  g_return_val_if_fail (callable_info != NULL, NULL);

  return_type = g_callable_info_get_return_type (callable_info);
  type_tag = g_type_info_get_tag (return_type);
  return g_ir_ffi_get_ffi_type (type_tag);
}

/**
 * g_callable_info_prepare_closure:
 * @callable_info: a callable info from a typelib
 * @cif: a ffi_cif structure
 * @callback: the ffi callback
 * @user_data: data to be passed into the callback
 *
 * Prepares a callback for ffi invocation.
 *
 * Note: this function requires the heap to be executable, which
 *       might not function properly on systems with SELinux enabled.
 *
 * Return value: the ffi_closure or NULL on error.
 * The return value should be freed by calling g_callable_info_prepare_closure().
 */
ffi_closure *
g_callable_info_prepare_closure (GICallableInfo       *callable_info,
                                 ffi_cif              *cif,
                                 GIFFIClosureCallback  callback,
                                 gpointer              user_data)
{
  ffi_closure *closure;
  ffi_status status;
    
  g_return_val_if_fail (callable_info != NULL, FALSE);
  g_return_val_if_fail (cif != NULL, FALSE);
  g_return_val_if_fail (callback != NULL, FALSE);
    
  closure = mmap (NULL, sizeof (ffi_closure),
                  PROT_EXEC | PROT_READ | PROT_WRITE,
                  MAP_ANON | MAP_PRIVATE, -1, sysconf (_SC_PAGE_SIZE));
  if (!closure)
    {
      g_warning("mmap failed: %s\n", strerror(errno));
      return NULL;
    }

  status = ffi_prep_cif (cif, FFI_DEFAULT_ABI,
                         g_callable_info_get_n_args (callable_info),
                         g_callable_info_get_ffi_return_type (callable_info),
                         g_callable_info_get_ffi_arg_types (callable_info));
  if (status != FFI_OK)
    {
      g_warning("ffi_prep_cif failed: %d\n", status);
      munmap(closure, sizeof (closure));
      return NULL;
    }

  status = ffi_prep_closure (closure, cif, callback, user_data);
  if (status != FFI_OK)
    {
      g_warning ("ffi_prep_closure failed: %d\n", status);
      munmap(closure, sizeof (closure));
      return NULL;
    }

  if (mprotect(closure, sizeof (closure), PROT_READ | PROT_EXEC) == -1)
    {
      g_warning ("ffi_prep_closure failed: %s\n", strerror(errno));
      munmap(closure, sizeof (closure));
      return NULL;
    }
  
  return closure;
}

/**
 * g_callable_info_free_closure:
 * @callable_info: a callable info from a typelib
 * @closure: ffi closure
 *
 * Frees a ffi_closure returned from g_callable_info_prepare_closure()
 */
void
g_callable_info_free_closure (GICallableInfo *callable_info,
                              ffi_closure    *closure)
{
  munmap(closure, sizeof (closure));
}
