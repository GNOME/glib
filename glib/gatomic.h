/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * g_atomic_*: atomic operations.
 * Copyright (C) 2003 Sebastian Wilhelmi
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
 
/*
 * Modified by the GLib Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GLib Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GLib at ftp://ftp.gtk.org/pub/gtk/.
 */
 
#ifndef __G_ATOMIC_H__
#define __G_ATOMIC_H__
 
#include <glib/gtypes.h>

G_BEGIN_DECLS
 
gint     g_atomic_int_exchange_and_add         (gint     *atomic, 
						gint      val);
void     g_atomic_int_add                      (gint     *atomic, 
						gint      val);
gboolean g_atomic_int_compare_and_exchange     (gint     *atomic, 
						gint      oldval, 
						gint      newval);
gboolean g_atomic_pointer_compare_and_exchange (gpointer *atomic, 
						gpointer  oldval, 
						gpointer  newval);

#ifdef G_ATOMIC_OP_MEMORY_BARRIER_NEEDED
gint     g_atomic_int_get                      (gint     *atomic);
gpointer g_atomic_pointer_get                  (gpointer *atomic);
#else /* !G_ATOMIC_OP_MEMORY_BARRIER_NEEDED */
# define g_atomic_int_get(atomic) (*(atomic))
# define g_atomic_pointer_get(atomic) (*(atomic))
#endif /* G_ATOMIC_OP_MEMORY_BARRIER_NEEDED */

#define g_atomic_int_inc(atomic) (g_atomic_int_add ((atomic), 1))
#define g_atomic_int_dec_and_test(atomic)				\
  (g_atomic_int_exchange_and_add ((atomic), -1) == 1)

G_END_DECLS
 
#endif /* __G_ATOMIC_H__ */
