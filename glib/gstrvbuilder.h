/*
 * Copyright © 2020 Canonical Ltd.
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

#ifndef __G_STRVBUILDER_H__
#define __G_STRVBUILDER_H__

#if !defined (__GLIB_H_INSIDE__) && !defined (GLIB_COMPILATION)
#error "Only <glib.h> can be included directly."
#endif

#include <glib/gstrfuncs.h>
#include <glib/gtypes.h>

G_BEGIN_DECLS

typedef struct _GStrvBuilder GStrvBuilder;

GLIB_AVAILABLE_IN_2_66
GStrvBuilder *                  g_strv_builder_new     (void);

GLIB_AVAILABLE_IN_2_66
void                            g_strv_builder_unref   (GStrvBuilder *builder);

GLIB_AVAILABLE_IN_2_66
GStrvBuilder                   *g_strv_builder_ref     (GStrvBuilder *builder);

GLIB_AVAILABLE_IN_2_66
void                            g_strv_builder_append  (GStrvBuilder *builder,
                                                        const gchar  *value);

GLIB_AVAILABLE_IN_2_66
void                            g_strv_builder_prepend (GStrvBuilder *builder,
                                                        const gchar  *value);

GLIB_AVAILABLE_IN_2_66
void                            g_strv_builder_insert  (GStrvBuilder *builder,
                                                        gint          index,
                                                        const gchar  *value);

GLIB_AVAILABLE_IN_2_66
GStrv                           g_strv_builder_end     (GStrvBuilder *builder);

G_END_DECLS

#endif /* __G_STRVBUILDER_H__ */
