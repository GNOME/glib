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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * gvalue.h: generic GValue functions
 */
#ifndef __G_VALUE_H__
#define __G_VALUE_H__


#include	<gobject/gtype.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


/* --- type macros --- */
#define	G_TYPE_IS_VALUE(type)		(g_type_value_table_peek (type) != NULL)
#define	G_IS_VALUE(value)		(G_TYPE_IS_VALUE (G_VALUE_TYPE (value))) // FIXME
#define	G_VALUE_TYPE(value)		(G_TYPE_FROM_CLASS (value))
#define	G_VALUE_TYPE_NAME(value)	(g_type_name (G_VALUE_TYPE (value)))


/* --- typedefs & structures --- */
typedef void (*GValueExchange) (GValue	*value1,
				GValue	*value2);
struct _GValue
{
  /*< private >*/
  GType		g_type;

  /* public for GTypeValueTable methods */
  union {
    gint	v_int;
    guint	v_uint;
    glong	v_long;
    gulong	v_ulong;
    gfloat	v_float;
    gdouble	v_double;
    gpointer	v_pointer;
  } data[4];
};


/* --- prototypes --- */
void            g_value_init	   	(GValue       *value,
					 GType         g_type);
void            g_value_copy    	(const GValue *src_value,
					 GValue       *dest_value);
gboolean	g_value_convert		(const GValue *src_value,
					 GValue       *dest_value);
void            g_value_reset   	(GValue       *value);
void            g_value_unset   	(GValue       *value);


/* --- implementation details --- */
gboolean g_values_exchange		(GValue       *value1,
					 GValue       *value2);
gboolean g_value_types_exchangable	(GType         value_type1,
					 GType         value_type2);
void     g_value_register_exchange_func	(GType         value_type1,
					 GType         value_type2,
					 GValueExchange func);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __G_VALUE_H__ */
