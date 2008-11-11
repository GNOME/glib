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
#include "girffi.h"

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
#  error "Unexpected time for time_t: not 4 or 8"
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
