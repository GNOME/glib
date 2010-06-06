/* GObject introspection: Registered Type implementation
 *
 * Copyright (C) 2005 Matthias Clasen
 * Copyright (C) 2008,2009 Red Hat, Inc.
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

#include <glib.h>

#include <girepository.h>
#include "girepository-private.h"
#include "gitypelib-internal.h"
#include "girffi.h"

const gchar *
g_registered_type_info_get_type_name (GIRegisteredTypeInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  RegisteredTypeBlob *blob = (RegisteredTypeBlob *)&rinfo->typelib->data[rinfo->offset];

  if (blob->gtype_name)
    return g_typelib_get_string (rinfo->typelib, blob->gtype_name);

  return NULL;
}

const gchar *
g_registered_type_info_get_type_init (GIRegisteredTypeInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  RegisteredTypeBlob *blob = (RegisteredTypeBlob *)&rinfo->typelib->data[rinfo->offset];

  if (blob->gtype_init)
    return g_typelib_get_string (rinfo->typelib, blob->gtype_init);

  return NULL;
}

GType
g_registered_type_info_get_g_type (GIRegisteredTypeInfo *info)
{
  const char *type_init;
  GType (* get_type_func) (void);
  GIRealInfo *rinfo = (GIRealInfo*)info;

  type_init = g_registered_type_info_get_type_init (info);

  if (type_init == NULL)
    return G_TYPE_NONE;
  else if (!strcmp (type_init, "intern"))
    return G_TYPE_OBJECT;

  get_type_func = NULL;
  if (!g_typelib_symbol (rinfo->typelib,
                         type_init,
                         (void**) &get_type_func))
    return G_TYPE_NONE;

  return (* get_type_func) ();
}

