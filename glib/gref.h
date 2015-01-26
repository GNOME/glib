/* gref.h: Reference counted memory areas
 *
 * Copyright 2015  Emmanuele Bassi
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the licence, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __G_REF_H__
#define __G_REF_H__

#if !defined (__GLIB_H_INSIDE__) && !defined (GLIB_COMPILATION)
#error "Only <glib.h> can be included directly."
#endif

#include <glib/gtypes.h>

G_BEGIN_DECLS

GLIB_AVAILABLE_IN_2_44
gpointer        g_ref_pointer_alloc             (gsize          alloc_size,
                                                 GDestroyNotify notify) G_GNUC_MALLOC G_GNUC_ALLOC_SIZE (1);
GLIB_AVAILABLE_IN_2_44
gpointer        g_ref_pointer_alloc0            (gsize          alloc_size,
                                                 GDestroyNotify notify) G_GNUC_MALLOC G_GNUC_ALLOC_SIZE (1);

#define g_ref_pointer_new(Type,free_func)       ((Type *) g_ref_pointer_alloc (sizeof (Type), (GDestroyNotify) free_func))

#define g_ref_pointer_new0(Type,free_func)      ((Type *) g_ref_pointer_alloc0 (sizeof (Type), (GDestroyNotify) free_func))

GLIB_AVAILABLE_IN_2_44
gpointer        g_ref_pointer_take              (gconstpointer  data,
                                                 gsize          alloc_size,
                                                 GDestroyNotify notify) G_GNUC_WARN_UNUSED_RESULT;
GLIB_AVAILABLE_IN_2_44
void            g_ref_pointer_make_atomic       (gpointer       ref);

GLIB_AVAILABLE_IN_2_44
gpointer        g_ref_pointer_acquire           (gpointer       ref);
GLIB_AVAILABLE_IN_2_44
void            g_ref_pointer_release           (gpointer       ref);

GLIB_AVAILABLE_IN_2_44
void            g_ref_count_init        (volatile int  *refcount,
                                         gboolean       atomic);
GLIB_AVAILABLE_IN_2_44
void            g_ref_count_inc         (volatile int  *refcount);
GLIB_AVAILABLE_IN_2_44
gboolean        g_ref_count_dec         (volatile int  *refcount);
GLIB_AVAILABLE_IN_2_44
void            g_ref_count_make_atomic (volatile int  *refcount);

G_END_DECLS

#endif /* __G_REF_H__ */
