/* GObject - GLib Type, Object, Parameter and Signal Library
 * Copyright (C) 1998, 1999, 2000 Tim Janik and Red Hat, Inc.
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
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * gvaluecollector.h: GValue varargs stubs
 */
#ifndef __G_VALUE_COLLECTOR_H__
#define __G_VALUE_COLLECTOR_H__


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


/* we may want to add aggregate types here some day, if requested
 * by users. the basic C types are covered already, everything
 * smaller than an int is promoted to an integer and floats are
 * always promoted to doubles for varargs call constructions.
 */
enum	/*< skip >*/
{
  G_VALUE_COLLECT_NONE,
  G_VALUE_COLLECT_INT,
  G_VALUE_COLLECT_LONG,
  G_VALUE_COLLECT_DOUBLE,
  G_VALUE_COLLECT_POINTER
};

union _GParamCValue
{
  gint     v_int;
  glong    v_long;
  gdouble  v_double;
  gpointer v_pointer;
};


/* G_PARAM_COLLECT_VALUE() collects a parameter's variable arguments
 * from a va_list. we have to implement the varargs collection as a
 * macro, because on some systems va_list variables cannot be passed
 * by reference.
 * param_value is supposed to be initialized according to the param
 * type to be collected.
 * the param_spec argument is optional, but probably needed by most
 * param class' param_collect_value() implementations.
 * var_args is the va_list variable and may be evaluated multiple times.
 * __error is a gchar** variable that will be modified to hold a g_new()
 * allocated error messages if something fails.
 */
#define G_PARAM_COLLECT_VALUE(param_value, param_spec, var_args, __error)		\
G_STMT_START {										\
  GValue *_value = (param_value);							\
  GParamSpecClass *_pclass = g_type_class_ref (_value->g_type);				\
  GParamSpec *_pspec = (param_spec);							\
  gchar *_error_msg = NULL;								\
  guint _collect_type = _pclass->collect_type;						\
  guint _nth_value = 0;									\
                                                                                        \
  if (_pspec)										\
    g_param_spec_ref (_pspec);								\
  g_value_reset (_value);								\
  while (_collect_type && !_error_msg)							\
    {											\
      GParamCValue _cvalue;								\
                                                                                        \
      memset (&_cvalue, 0, sizeof (_cvalue));						\
      switch (_collect_type)								\
	{										\
	case G_VALUE_COLLECT_INT:							\
	  _cvalue.v_int = va_arg ((var_args), gint);					\
	  break;									\
	case G_VALUE_COLLECT_LONG:							\
	  _cvalue.v_long = va_arg ((var_args), glong);					\
	  break;									\
	case G_VALUE_COLLECT_DOUBLE:							\
	  _cvalue.v_double = va_arg ((var_args), gdouble);				\
	  break;									\
	case G_VALUE_COLLECT_POINTER:							\
	  _cvalue.v_pointer = va_arg ((var_args), gpointer);				\
	  break;									\
	default:									\
	  _error_msg  = g_strdup_printf ("%s: invalid collect type (%d) used for %s",	\
					 G_STRLOC,					\
					 _collect_type,					\
					 "G_PARAM_COLLECT_VALUE()");			\
	  continue;									\
	}										\
      _error_msg = _pclass->param_collect_value (_value,				\
						 _pspec,				\
						 _nth_value++,				\
						 &_collect_type,			\
						 &_cvalue);				\
    }											\
  *(__error) = _error_msg;								\
  if (_pspec)										\
    g_param_spec_unref (_pspec);							\
  g_type_class_unref (_pclass);								\
} G_STMT_END


/* G_PARAM_LCOPY_VALUE() collects a parameter's variable argument
 * locations from a va_list. usage is analogous to G_PARAM_COLLECT_VALUE().
 */
#define G_PARAM_LCOPY_VALUE(param_value, param_spec, var_args, __error)			\
G_STMT_START {										\
  GValue *_value = (param_value);							\
  GParamSpecClass *_pclass = g_type_class_ref (_value->g_type);				\
  GParamSpec *_pspec = (param_spec);							\
  gchar *_error_msg = NULL;								\
  guint _lcopy_type = _pclass->lcopy_type;						\
  guint _nth_value = 0;									\
                                                                                        \
  if (_pspec)										\
    g_param_spec_ref (_pspec);								\
  while (_lcopy_type && !_error_msg)							\
    {											\
      GParamCValue _cvalue;								\
                                                                                        \
      memset (&_cvalue, 0, sizeof (_cvalue));						\
      switch (_lcopy_type)								\
	{										\
	case G_VALUE_COLLECT_INT:							\
	  _cvalue.v_int = va_arg ((var_args), gint);					\
	  break;									\
	case G_VALUE_COLLECT_LONG:							\
	  _cvalue.v_long = va_arg ((var_args), glong);					\
	  break;									\
	case G_VALUE_COLLECT_DOUBLE:							\
	  _cvalue.v_double = va_arg ((var_args), gdouble);				\
	  break;									\
	case G_VALUE_COLLECT_POINTER:							\
	  _cvalue.v_pointer = va_arg ((var_args), gpointer);				\
	  break;									\
	default:									\
	  _error_msg  = g_strdup_printf ("%s: invalid collect type (%d) used for %s",	\
					 G_STRLOC,					\
					 _lcopy_type,					\
					 "G_PARAM_LCOPY_VALUE()");			\
	  continue;									\
	}										\
      _error_msg = _pclass->param_lcopy_value (_value,					\
					       _pspec,					\
					       _nth_value++,				\
					       &_lcopy_type,				\
					       &_cvalue);				\
    }											\
  *(__error) = _error_msg;								\
  if (_pspec)										\
    g_param_spec_unref (_pspec);							\
  g_type_class_unref (_pclass);								\
} G_STMT_END



#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __G_VALUE_COLLECTOR_H__ */
