/* GObject introspection: Public typelib API
 *
 * Copyright (C) 2005 Matthias Clasen
 * Copyright (C) 2008,2009 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
..skipping...
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

#ifndef __GITYPELIB_H__
#define __GITYPELIB_H__

#if !defined (__GIREPOSITORY_H_INSIDE__) && !defined (GI_COMPILATION)
#error "Only <girepository.h> can be included directly."
#endif

#include <glib.h>

G_BEGIN_DECLS

typedef struct _GTypelib GTypelib;

GTypelib *    g_typelib_new_from_memory       (guchar       *memory,
                                               gsize         len);
GTypelib *    g_typelib_new_from_const_memory (const guchar *memory,
                                               gsize         len);
GTypelib *    g_typelib_new_from_mapped_file  (GMappedFile  *mfile);
void          g_typelib_free                  (GTypelib     *typelib);

gboolean      g_typelib_symbol                (GTypelib     *typelib,
                                               const gchar  *symbol_name,
                                               gpointer     *symbol);
const gchar * g_typelib_get_namespace         (GTypelib     *typelib);


G_END_DECLS

#endif  /* __GITYPELIB_H__ */

