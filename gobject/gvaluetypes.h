/* GObject - GLib Type, Object, Parameter and Signal Library
 * Copyright (C) 1997, 1998, 1999, 2000 Tim Janik and Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * gvaluetypes.h: GLib default values
 */
#ifndef __G_VALUETYPES_H__
#define __G_VALUETYPES_H__


#include	<gobject/gvalue.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


/* --- type macros --- */
#define G_IS_VALUE_CHAR(value)		(G_TYPE_CHECK_VALUE_TYPE ((value), G_TYPE_CHAR))
#define G_IS_VALUE_UCHAR(value)		(G_TYPE_CHECK_VALUE_TYPE ((value), G_TYPE_UCHAR))
#define G_IS_VALUE_BOOLEAN(value)	(G_TYPE_CHECK_VALUE_TYPE ((value), G_TYPE_BOOLEAN))
#define G_IS_VALUE_INT(value)		(G_TYPE_CHECK_VALUE_TYPE ((value), G_TYPE_INT))
#define G_IS_VALUE_UINT(value)		(G_TYPE_CHECK_VALUE_TYPE ((value), G_TYPE_UINT))
#define G_IS_VALUE_LONG(value)		(G_TYPE_CHECK_VALUE_TYPE ((value), G_TYPE_LONG))
#define G_IS_VALUE_ULONG(value)		(G_TYPE_CHECK_VALUE_TYPE ((value), G_TYPE_ULONG))
#define G_IS_VALUE_FLOAT(value)		(G_TYPE_CHECK_VALUE_TYPE ((value), G_TYPE_FLOAT))
#define G_IS_VALUE_DOUBLE(value)	(G_TYPE_CHECK_VALUE_TYPE ((value), G_TYPE_DOUBLE))
#define G_IS_VALUE_STRING(value)	(G_TYPE_CHECK_VALUE_TYPE ((value), G_TYPE_STRING))
#define G_IS_VALUE_POINTER(value)	(G_TYPE_CHECK_VALUE_TYPE ((value), G_TYPE_POINTER))


/* --- prototypes --- */
void		g_value_set_char		(GValue		*value,
						 gint8		 v_char);
gint8		g_value_get_char		(const GValue	*value);
void		g_value_set_uchar		(GValue		*value,
						 guint8		 v_uchar);
guint8		g_value_get_uchar		(const GValue	*value);
void		g_value_set_boolean		(GValue		*value,
						 gboolean	 v_boolean);
gboolean	g_value_get_boolean		(const GValue	*value);
void		g_value_set_int			(GValue		*value,
						 gint		 v_int);
gint		g_value_get_int			(const GValue	*value);
void		g_value_set_uint		(GValue		*value,
						 guint		 v_uint);
guint		g_value_get_uint		(const GValue	*value);
void		g_value_set_long		(GValue		*value,
						 glong		 v_long);
glong		g_value_get_long		(const GValue	*value);
void		g_value_set_ulong		(GValue		*value,
						 gulong		 v_ulong);
gulong		g_value_get_ulong		(const GValue	*value);
void		g_value_set_float		(GValue		*value,
						 gfloat		 v_float);
gfloat		g_value_get_float		(const GValue	*value);
void		g_value_set_double		(GValue		*value,
						 gdouble	 v_double);
gdouble		g_value_get_double		(const GValue	*value);
void		g_value_set_string		(GValue		*value,
						 const gchar	*v_string);
void		g_value_set_static_string	(GValue		*value,
						 const gchar	*v_string);
gchar*		g_value_get_string		(const GValue	*value);
gchar*		g_value_dup_string		(const GValue	*value);
void            g_value_set_pointer     	(GValue         *value,
						 gpointer        v_pointer);
gpointer        g_value_get_pointer     	(const GValue   *value);





#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __G_VALUETYPES_H__ */
