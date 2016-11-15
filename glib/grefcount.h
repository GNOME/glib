/* GLib - Library of useful routines for C programming
 * Copyright 2016  Emmanuele Bassi
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
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __G_REF_COUNT_H__
#define __G_REF_COUNT_H__

#if !defined (__GLIB_H_INSIDE__) && !defined (GLIB_COMPILATION)
#error "Only <glib.h> can be included directly."
#endif

#include <glib/gutils.h>

G_BEGIN_DECLS

GLIB_AVAILABLE_IN_2_52
void            g_ref_counter_init              (volatile int *ref_count,
                                                 gboolean      is_atomic);
GLIB_AVAILABLE_IN_2_52
void            g_ref_counter_acquire           (volatile int *ref_count);
GLIB_AVAILABLE_IN_2_52
gboolean        g_ref_counter_release           (volatile int *ref_count);
GLIB_AVAILABLE_IN_2_52
void            g_ref_counter_make_atomic       (volatile int *ref_count);
GLIB_AVAILABLE_IN_2_52
gboolean        g_ref_counter_is_atomic         (volatile int *ref_count);

#define g_ref_new(Type,Notify) \
  (Type *) g_ref_alloc (sizeof (Type), Notify)

#define g_ref_new0(Type,Notify) \
  (Type *) g_ref_alloc0 (sizeof (Type), Notify)

GLIB_AVAILABLE_IN_2_52
gpointer        g_ref_alloc                     (gsize          size,
                                                 GDestroyNotify notify);
GLIB_AVAILABLE_IN_2_52
gpointer        g_ref_alloc0                    (gsize          size,
                                                 GDestroyNotify notify);
GLIB_AVAILABLE_IN_2_52
gpointer        g_ref_dup                       (gconstpointer  data,
                                                 gsize          size,
                                                 GDestroyNotify notify);
GLIB_AVAILABLE_IN_2_52
gpointer        g_ref_acquire                   (gpointer       ref);
GLIB_AVAILABLE_IN_2_52
void            g_ref_release                   (gpointer       ref);

#define g_atomic_ref_new(Type,Notify) \
  (Type *) g_atomic_ref_alloc (sizeof (Type), Notify)

#define g_atomic_ref_new0(Type,Notify) \
  (Type *) g_atomic_ref_alloc0 (sizeof (Type), Notify)

GLIB_AVAILABLE_IN_2_52
gpointer        g_atomic_ref_alloc              (gsize          size,
                                                 GDestroyNotify notify);
GLIB_AVAILABLE_IN_2_52
gpointer        g_atomic_ref_alloc0             (gsize          size,
                                                 GDestroyNotify notify);
GLIB_AVAILABLE_IN_2_52
gpointer        g_atomic_ref_dup                (gconstpointer  data,
                                                 gsize          size,
                                                 GDestroyNotify notify);
GLIB_AVAILABLE_IN_2_52
gpointer        g_atomic_ref_acquire            (gpointer       ref);
GLIB_AVAILABLE_IN_2_52
void            g_atomic_ref_release            (gpointer       ref);

G_END_DECLS

#endif /* __G_REF_COUNT_H__ */
