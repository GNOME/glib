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
#include "girffi.h"
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
  rtype = g_type_info_get_ffi_type (tinfo);
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
	  atypes[i+offset] = g_type_info_get_ffi_type (tinfo);
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

  g_return_val_if_fail (return_value, FALSE);
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

static ffi_type *
value_to_ffi_type (const GValue *gvalue, gpointer *value)
{
  ffi_type *rettype = NULL;
  GType type = g_type_fundamental (G_VALUE_TYPE (gvalue));
  g_assert (type != G_TYPE_INVALID);

  switch (type)
    {
    case G_TYPE_BOOLEAN:
    case G_TYPE_CHAR:
    case G_TYPE_INT:
      rettype = &ffi_type_sint;
      *value = (gpointer)&(gvalue->data[0].v_int);
      break;
    case G_TYPE_UCHAR:
    case G_TYPE_UINT:
      rettype = &ffi_type_uint;
      *value = (gpointer)&(gvalue->data[0].v_uint);
      break;
    case G_TYPE_STRING:
    case G_TYPE_OBJECT:
    case G_TYPE_BOXED:
    case G_TYPE_POINTER:
      rettype = &ffi_type_pointer;
      *value = (gpointer)&(gvalue->data[0].v_pointer);
      break;
    case G_TYPE_FLOAT:
      rettype = &ffi_type_float;
      *value = (gpointer)&(gvalue->data[0].v_float);
      break;
    case G_TYPE_DOUBLE:
      rettype = &ffi_type_double;
      *value = (gpointer)&(gvalue->data[0].v_double);
      break;
    case G_TYPE_LONG:
      rettype = &ffi_type_slong;
      *value = (gpointer)&(gvalue->data[0].v_long);
      break;
    case G_TYPE_ULONG:
      rettype = &ffi_type_ulong;
      *value = (gpointer)&(gvalue->data[0].v_ulong);
      break;
    case G_TYPE_INT64:
      rettype = &ffi_type_sint64;
      *value = (gpointer)&(gvalue->data[0].v_int64);
      break;
    case G_TYPE_UINT64:
      rettype = &ffi_type_uint64;
      *value = (gpointer)&(gvalue->data[0].v_uint64);
      break;
    default:
      rettype = &ffi_type_pointer;
      *value = NULL;
      g_warning ("Unsupported fundamental type: %s", g_type_name (type));
      break;
    }
  return rettype;
}

static void
value_from_ffi_type (GValue *gvalue, gpointer *value)
{
  switch (g_type_fundamental (G_VALUE_TYPE (gvalue)))
    {
    case G_TYPE_INT:
      g_value_set_int (gvalue, *(gint*)value);
      break;
    case G_TYPE_FLOAT:
      g_value_set_float (gvalue, *(gfloat*)value);
      break;
    case G_TYPE_DOUBLE:
      g_value_set_double (gvalue, *(gdouble*)value);
      break;
    case G_TYPE_BOOLEAN:
      g_value_set_boolean (gvalue, *(gboolean*)value);
      break;
    case G_TYPE_STRING:
      g_value_set_string (gvalue, *(gchar**)value);
      break;
    case G_TYPE_CHAR:
      g_value_set_char (gvalue, *(gchar*)value);
      break;
    case G_TYPE_UCHAR:
      g_value_set_uchar (gvalue, *(guchar*)value);
      break;
    case G_TYPE_UINT:
      g_value_set_uint (gvalue, *(guint*)value);
      break;
    case G_TYPE_POINTER:
      g_value_set_pointer (gvalue, *(gpointer*)value);
      break;
    case G_TYPE_LONG:
      g_value_set_long (gvalue, *(glong*)value);
      break;
    case G_TYPE_ULONG:
      g_value_set_ulong (gvalue, *(gulong*)value);
      break;
    case G_TYPE_INT64:
      g_value_set_int64 (gvalue, *(gint64*)value);
      break;
    case G_TYPE_UINT64:
      g_value_set_uint64 (gvalue, *(guint64*)value);
      break;
    case G_TYPE_BOXED:
      g_value_set_boxed (gvalue, *(gpointer*)value);
      break;
    default:
      g_warning ("Unsupported fundamental type: %s",
                g_type_name (g_type_fundamental (G_VALUE_TYPE (gvalue))));
    }
}

void
gi_cclosure_marshal_generic (GClosure *closure,
                             GValue *return_gvalue,
                             guint n_param_values,
                             const GValue *param_values,
                             gpointer invocation_hint,
                             gpointer marshal_data)
{
  ffi_type *rtype;
  void *rvalue;
  int n_args;
  ffi_type **atypes;
  void **args;
  int i;
  ffi_cif cif;
  GCClosure *cc = (GCClosure*) closure;

  if (return_gvalue && G_VALUE_TYPE (return_gvalue)) 
    {
      rtype = value_to_ffi_type (return_gvalue, &rvalue);
    }
  else 
    {
      rtype = &ffi_type_void;
    }

  rvalue = g_alloca (MAX (rtype->size, sizeof (ffi_arg)));
  
  n_args = n_param_values + 1;
  atypes = g_alloca (sizeof (ffi_type *) * n_args);
  args =  g_alloca (sizeof (gpointer) * n_args);

  if (n_param_values > 0)
    {
      if (G_CCLOSURE_SWAP_DATA (closure))
        {
          atypes[n_args-1] = value_to_ffi_type (param_values + 0,  
                                                &args[n_args-1]);
          atypes[0] = &ffi_type_pointer;
          args[0] = &closure->data;
        }
      else
        {
          atypes[0] = value_to_ffi_type (param_values + 0, &args[0]);
          atypes[n_args-1] = &ffi_type_pointer;
          args[n_args-1] = &closure->data;
        }
    }
  else
    {
      atypes[0] = &ffi_type_pointer;
      args[0] = &closure->data;
    }

  for (i = 1; i < n_args - 1; i++)
    atypes[i] = value_to_ffi_type (param_values + i, &args[i]);

  if (ffi_prep_cif (&cif, FFI_DEFAULT_ABI, n_args, rtype, atypes) != FFI_OK)
    return;

  ffi_call (&cif, marshal_data ? marshal_data : cc->callback, rvalue, args);

  if (return_gvalue && G_VALUE_TYPE (return_gvalue))
    value_from_ffi_type (return_gvalue, rvalue);
}
