/* GObject introspection: Property implementation
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

GParamFlags
g_property_info_get_flags (GIPropertyInfo *info)
{
  GParamFlags flags;
  GIRealInfo *rinfo = (GIRealInfo *)info;
  PropertyBlob *blob = (PropertyBlob *)&rinfo->typelib->data[rinfo->offset];

  flags = 0;

  if (blob->readable)
    flags = flags | G_PARAM_READABLE;

  if (blob->writable)
    flags = flags | G_PARAM_WRITABLE;

  if (blob->construct)
    flags = flags | G_PARAM_CONSTRUCT;

  if (blob->construct_only)
    flags = flags | G_PARAM_CONSTRUCT_ONLY;

  return flags;
}

GITypeInfo *
g_property_info_get_type (GIPropertyInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;

  return _g_type_info_new ((GIBaseInfo*)info, rinfo->typelib, rinfo->offset + G_STRUCT_OFFSET (PropertyBlob, type));
}

