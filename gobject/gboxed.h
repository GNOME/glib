/* GObject - GLib Type, Object, Parameter and Signal Library
 * Copyright (C) 2000-2001 Red Hat, Inc.
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
 */
#ifndef __G_BOXED_H__
#define __G_BOXED_H__

#include        <gobject/gtype.h>

G_BEGIN_DECLS

/* --- type macros --- */
#define G_TYPE_IS_BOXED(type)	   (G_TYPE_FUNDAMENTAL (type) == G_TYPE_BOXED)
#define G_VALUE_HOLDS_BOXED(value) (G_TYPE_CHECK_VALUE_TYPE ((value), G_TYPE_BOXED))


/* --- typedefs --- */
typedef struct _GBoxed	GBoxed;
typedef gpointer (*GBoxedInitFunc)	(void);
typedef gpointer (*GBoxedCopyFunc)	(gpointer	 boxed);
typedef void     (*GBoxedFreeFunc)	(gpointer	 boxed);


/* --- prototypes --- */
GBoxed*		g_boxed_copy			(GType		 boxed_type,
						 gconstpointer	 src_boxed);
void		g_boxed_free			(GType		 boxed_type,
						 gpointer	 boxed);
void		g_value_set_boxed		(GValue		*value,
						 gconstpointer	 boxed);
void		g_value_set_static_boxed	(GValue		*value,
						 gconstpointer	 boxed);
gpointer	g_value_get_boxed		(const GValue	*value);
gpointer	g_value_dup_boxed		(GValue		*value);


/* --- convenience --- */
GType	g_boxed_type_register_static		(const gchar	*name,
						 GBoxedInitFunc	 boxed_init,
						 GBoxedCopyFunc	 boxed_copy,
						 GBoxedFreeFunc	 boxed_free,
						 gboolean	 is_refcounted);


/* --- marshaller specific --- */
void	g_value_set_boxed_take_ownership	(GValue		*value,
						 gconstpointer	 boxed);



G_END_DECLS

#endif	/* __G_BOXED_H__ */
