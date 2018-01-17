/* grefcount.h: Reference counting
 *
 * Copyright 2018  Emmanuele Bassi
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GREFCOUNT_H__
#define __GREFCOUNT_H__

#if !defined (__GLIB_H_INSIDE__) && !defined (GLIB_COMPILATION)
#error "Only <glib.h> can be included directly."
#endif

#include <glib/gtypes.h>

G_BEGIN_DECLS

GLIB_AVAILABLE_IN_2_56
void            g_ref_count_init                (grefcount       *rc);
GLIB_AVAILABLE_IN_2_56
void            g_ref_count_inc                 (grefcount       *rc);
GLIB_AVAILABLE_IN_2_56
gboolean        g_ref_count_dec                 (grefcount       *rc);
GLIB_AVAILABLE_IN_2_56
gboolean        g_ref_count_compare             (grefcount       *rc,
                                                 gint             val);

GLIB_AVAILABLE_IN_2_56
void            g_atomic_ref_count_init         (gatomicrefcount *arc);
GLIB_AVAILABLE_IN_2_56
void            g_atomic_ref_count_inc          (gatomicrefcount *arc);
GLIB_AVAILABLE_IN_2_56
gboolean        g_atomic_ref_count_dec          (gatomicrefcount *arc);
GLIB_AVAILABLE_IN_2_56
gboolean        g_atomic_ref_count_compare      (gatomicrefcount *arc,
                                                 gint             val);

G_END_DECLS

#endif /* __GREFCOUNT_H__ */
