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

union _GTypeCValue
{
  gint     v_int;
  glong    v_long;
  gdouble  v_double;
  gpointer v_pointer;
};


/* G_VALUE_COLLECT() collects a variable argument value
 * from a va_list. we have to implement the varargs collection as a
 * macro, because on some systems va_list variables cannot be passed
 * by reference.
 * value is supposed to be initialized according to the value
 * type to be collected.
 * var_args is the va_list variable and may be evaluated multiple times.
 * __error is a gchar** variable that will be modified to hold a g_new()
 * allocated error messages if something fails.
 */
#define G_VALUE_COLLECT(value, var_args, __error)					\
G_STMT_START {										\
  GValue *_value = (value);								\
  GTypeValueTable *_vtable = g_type_value_table_peek (G_VALUE_TYPE (_value));		\
  gchar *_error_msg = NULL;								\
  guint _collect_type = _vtable->collect_type;						\
  guint _nth_value = 0;									\
                                                                                        \
  g_value_reset (_value);								\
  while (_collect_type && !_error_msg)							\
    {											\
      GTypeCValue _cvalue;								\
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
					 "G_VALUE_COLLECT()");				\
	  continue;									\
	}										\
      _error_msg = _vtable->collect_value (_value,					\
					   _nth_value++,				\
					   &_collect_type,				\
					   &_cvalue);					\
    }											\
  *(__error) = _error_msg;								\
} G_STMT_END


/* G_VALUE_LCOPY() collects a value's variable argument
 * locations from a va_list. usage is analogous to G_VALUE_COLLECT().
 */
#define G_VALUE_LCOPY(value, var_args, __error)						\
G_STMT_START {										\
  GValue *_value = (value);								\
  GTypeValueTable *_vtable = g_type_value_table_peek (G_VALUE_TYPE (_value));		\
  gchar *_error_msg = NULL;								\
  guint _lcopy_type = _vtable->lcopy_type;						\
  guint _nth_value = 0;									\
                                                                                        \
  while (_lcopy_type && !_error_msg)							\
    {											\
      GTypeCValue _cvalue;								\
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
					 "G_VALUE_LCOPY()");				\
	  continue;									\
	}										\
      _error_msg = _vtable->lcopy_value (_value,					\
					 _nth_value++,					\
					 &_lcopy_type,					\
					 &_cvalue);					\
    }											\
  *(__error) = _error_msg;								\
} G_STMT_END



#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __G_VALUE_COLLECTOR_H__ */
