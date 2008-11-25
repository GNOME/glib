/* GObject introspection: Typelib info functions
 *
 * Copyright (C) 2008 Red Hat, Inc
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

#ifndef __GIRINFO_H__
#define __GIRINFO_H__

#include "girepository.h"

G_BEGIN_DECLS

GITypeInfo *
g_type_info_new (GIBaseInfo    *container,
		 GTypelib     *typelib,
		 guint32        offset);

GIBaseInfo *
g_info_new_full (GIInfoType     type,
		 GIRepository  *repository,
		 GIBaseInfo    *container,
		 GTypelib      *typelib,
		 guint32        offset);

G_END_DECLS

#endif
