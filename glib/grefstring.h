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

#ifndef __G_REF_STRING_H__
#define __G_REF_STRING_H__

#if !defined (__GLIB_H_INSIDE__) && !defined (GLIB_COMPILATION)
#error "Only <glib.h> can be included directly."
#endif

#include <glib/gutils.h>

G_BEGIN_DECLS

GLIB_AVAILABLE_IN_2_52
char *  g_ref_string_new        (const char *str);

GLIB_AVAILABLE_IN_2_52
char *  g_ref_string_ref        (char       *str);
GLIB_AVAILABLE_IN_2_52
void    g_ref_string_unref      (char       *str);

G_END_DECLS

#endif /* __G_REF_STRING_H__ */
