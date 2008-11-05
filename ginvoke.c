/* GObject introspection: Invoke functionality
 *
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

#include <stdlib.h>

#include <glib.h>
#include <glib-object.h>

#include "girepository.h"
#include "gtypelib.h"
#include "config.h"

GQuark
g_invoke_error_quark (void)
{
  static GQuark quark = 0;
  if (quark == 0)
    quark = g_quark_from_static_string ("g-invoke-error-quark");
  return quark;
}

#include "ffi.h"

static ffi_type *
get_ffi_type (GITypeInfo *info)
{
  ffi_type *rettype;

  if (g_type_info_is_pointer (info))
    rettype = &ffi_type_pointer;
  else
    switch (g_type_info_get_tag (info))
      {
      case GI_TYPE_TAG_VOID:
	rettype = &ffi_type_void;
	break;
      case GI_TYPE_TAG_BOOLEAN:
	rettype = &ffi_type_uint;
	break;
      case GI_TYPE_TAG_INT8:
	rettype = &ffi_type_sint8;
	break;
      case GI_TYPE_TAG_UINT8:
	rettype = &ffi_type_uint8;
	break;
      case GI_TYPE_TAG_INT16:
	rettype = &ffi_type_sint16;
	break;
      case GI_TYPE_TAG_UINT16:
	rettype = &ffi_type_uint16;
	break;
      case GI_TYPE_TAG_INT32:
	rettype = &ffi_type_sint32;
	break;
      case GI_TYPE_TAG_UINT32:
	rettype = &ffi_type_uint32;
	break;
      case GI_TYPE_TAG_INT64:
	rettype = &ffi_type_sint64;
	break;
      case GI_TYPE_TAG_UINT64:
	rettype = &ffi_type_uint64;
	break;
      case GI_TYPE_TAG_INT:
	rettype = &ffi_type_sint;
	break;
      case GI_TYPE_TAG_UINT:
	rettype = &ffi_type_uint;
	break;
      case GI_TYPE_TAG_SSIZE: /* FIXME */
      case GI_TYPE_TAG_LONG:
	rettype = &ffi_type_slong;
	break;
      case GI_TYPE_TAG_SIZE: /* FIXME */
      case GI_TYPE_TAG_TIME_T: /* May not be portable */
      case GI_TYPE_TAG_ULONG:
	rettype = &ffi_type_ulong;
	break;
      case GI_TYPE_TAG_FLOAT:
	rettype = &ffi_type_float;
	break;
      case GI_TYPE_TAG_DOUBLE:
	rettype = &ffi_type_double;
	break;
      case GI_TYPE_TAG_UTF8:
      case GI_TYPE_TAG_FILENAME:
      case GI_TYPE_TAG_ARRAY:
      case GI_TYPE_TAG_INTERFACE:
      case GI_TYPE_TAG_GLIST:
      case GI_TYPE_TAG_GSLIST:
      case GI_TYPE_TAG_GHASH:
      case GI_TYPE_TAG_ERROR:
	rettype = &ffi_type_pointer;
	break;
      default:
	g_assert_not_reached ();
      }

  return rettype;
}

/**
 * g_function_info_invoke:
 * @info: a #GIFunctionInfo describing the function to invoke
 * @in_args: an array of #GArgument<!-- -->s, one for each in 
 *    parameter of @info. If there are no in parameter, @in_args
 *    can be %NULL
 * @n_in_args: the length of the @in_args array
 * @out_args: an array of #GArgument<!-- -->s, one for each out
 *    parameter of @info. If there are no out parameters, @out_args
 *    may be %NULL 
 * @n_out_args: the length of the @out_args array
 * @return_value: return location for the return value of the 
 *    function. If the function returns void, @return_value may be
 *    %NULL
 * @error: return location for detailed error information, or %NULL
 *
 * Invokes the function described in @info with the given 
 * arguments. Note that inout parameters must appear in both
 * argument lists. This function uses dlsym() to obtain a pointer
 * to the function, so the library or shared object containing the 
 * described function must either be linked to the caller, or must 
 * have been dlopen()<!-- -->ed before calling this function.
 *
 * Returns: %TRUE if the function has been invoked, %FALSE if an
 *   error occurred. 
 */
gboolean 
g_function_info_invoke (GIFunctionInfo *info, 
			const GArgument  *in_args,
			int               n_in_args,
			const GArgument  *out_args,
			int               n_out_args,
			GArgument        *return_value,
			GError          **error)
{
  ffi_cif cif;
  ffi_type *rtype;
  ffi_type **atypes;
  const gchar *symbol;
  gpointer func;
  GITypeInfo *tinfo;
  GIArgInfo *ainfo;
  gboolean is_method;
  gboolean throws;
  gint n_args, n_invoke_args, in_pos, out_pos, i;
  gpointer *args;
  gboolean success = FALSE;
  GError *local_error = NULL;
  gpointer error_address = &local_error;

  symbol = g_function_info_get_symbol (info);

  if (!g_typelib_symbol (g_base_info_get_typelib((GIBaseInfo *) info),
                         symbol, &func))
    {
      g_set_error (error,
                   G_INVOKE_ERROR,
                   G_INVOKE_ERROR_SYMBOL_NOT_FOUND,
                   "Could not locate %s: %s", symbol, g_module_error ());

      return FALSE;
    }

  is_method = (g_function_info_get_flags (info) & GI_FUNCTION_IS_METHOD) != 0
    && (g_function_info_get_flags (info) & GI_FUNCTION_IS_CONSTRUCTOR) == 0;
  throws = g_function_info_get_flags (info) & GI_FUNCTION_THROWS;

  tinfo = g_callable_info_get_return_type ((GICallableInfo *)info);
  rtype = get_ffi_type (tinfo);
  g_base_info_unref ((GIBaseInfo *)tinfo);

  in_pos = 0;
  out_pos = 0;

  n_args = g_callable_info_get_n_args ((GICallableInfo *)info);
  if (is_method)
    {
      if (n_in_args == 0)
	{
	  g_set_error (error,
		       G_INVOKE_ERROR,
		       G_INVOKE_ERROR_ARGUMENT_MISMATCH,
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
      ainfo = g_callable_info_get_arg ((GICallableInfo *)info, i);
      switch (g_arg_info_get_direction (ainfo))
	{
	case GI_DIRECTION_IN:
	  tinfo = g_arg_info_get_type (ainfo);
	  atypes[i+offset] = get_ffi_type (tinfo);
	  g_base_info_unref ((GIBaseInfo *)tinfo);

	  if (in_pos >= n_in_args)
	    {
	      g_set_error (error,
			   G_INVOKE_ERROR,
			   G_INVOKE_ERROR_ARGUMENT_MISMATCH,
			   "Too few \"in\" arguments (handling in)");
	      goto out;
	    }

	  args[i+offset] = (gpointer)&in_args[in_pos];
	  in_pos++;
	  
	  break;
	case GI_DIRECTION_OUT:
	  atypes[i+offset] = &ffi_type_pointer;

	  if (out_pos >= n_out_args)
	    {
	      g_set_error (error,
			   G_INVOKE_ERROR,
			   G_INVOKE_ERROR_ARGUMENT_MISMATCH,
			   "Too few \"out\" arguments (handling out)");	      
	      goto out;
	    }

	  args[i+offset] = (gpointer)&out_args[out_pos];
	  out_pos++;	  
	  break;
	case GI_DIRECTION_INOUT:
	  atypes[i+offset] = &ffi_type_pointer;

	  if (in_pos >= n_in_args)
	    {
	      g_set_error (error,
			   G_INVOKE_ERROR,
			   G_INVOKE_ERROR_ARGUMENT_MISMATCH,
			   "Too few \"in\" arguments (handling inout)");
	      goto out;
	    }

	  if (out_pos >= n_out_args)
	    {
	      g_set_error (error,
			   G_INVOKE_ERROR,
			   G_INVOKE_ERROR_ARGUMENT_MISMATCH,
			   "Too few \"out\" arguments (handling inout)");	      
	      goto out;
	    }
	  
	  args[i+offset] = (gpointer)&in_args[in_pos];
	  in_pos++;	  
	  out_pos++;	  
	  break;
	default:
	  g_assert_not_reached ();
	}
      g_base_info_unref ((GIBaseInfo *)ainfo);
    }

  if (throws)
    {
      args[n_invoke_args - 1] = &error_address;
      atypes[n_invoke_args - 1] = &ffi_type_pointer;
    }

  if (in_pos < n_in_args)
    {
      g_set_error (error,
		   G_INVOKE_ERROR,
		   G_INVOKE_ERROR_ARGUMENT_MISMATCH,
		   "Too many \"in\" arguments (at end)");
      goto out;
    }
  if (out_pos < n_out_args)
    {
      g_set_error (error,
		   G_INVOKE_ERROR,
		   G_INVOKE_ERROR_ARGUMENT_MISMATCH,
		   "Too many \"out\" arguments (at end)");	      
      goto out;
    }

  if (ffi_prep_cif (&cif, FFI_DEFAULT_ABI, n_invoke_args, rtype, atypes) != FFI_OK)
    goto out;

  ffi_call (&cif, func, return_value, args);

  if (local_error)
    {
      g_propagate_error (error, local_error);
      success = FALSE;
    }
  else
    {
      success = TRUE;
    }
 out:
  return success;
}
