/* GObject introspection: Constant implementation
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

GITypeInfo *
g_constant_info_get_type (GIConstantInfo *info)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;

  return _g_type_info_new ((GIBaseInfo*)info, rinfo->typelib, rinfo->offset + 8);
}

gint
g_constant_info_get_value (GIConstantInfo *info,
			   GArgument      *value)
{
  GIRealInfo *rinfo = (GIRealInfo *)info;
  ConstantBlob *blob = (ConstantBlob *)&rinfo->typelib->data[rinfo->offset];

  /* FIXME non-basic types ? */
  if (blob->type.flags.reserved == 0 && blob->type.flags.reserved2 == 0)
    {
      if (blob->type.flags.pointer)
	value->v_pointer = g_memdup (&rinfo->typelib->data[blob->offset], blob->size);
      else
	{
	  switch (blob->type.flags.tag)
	    {
	    case GI_TYPE_TAG_BOOLEAN:
	      value->v_boolean = *(gboolean*)&rinfo->typelib->data[blob->offset];
	      break;
	    case GI_TYPE_TAG_INT8:
	      value->v_int8 = *(gint8*)&rinfo->typelib->data[blob->offset];
	      break;
	    case GI_TYPE_TAG_UINT8:
	      value->v_uint8 = *(guint8*)&rinfo->typelib->data[blob->offset];
	      break;
	    case GI_TYPE_TAG_INT16:
	      value->v_int16 = *(gint16*)&rinfo->typelib->data[blob->offset];
	      break;
	    case GI_TYPE_TAG_UINT16:
	      value->v_uint16 = *(guint16*)&rinfo->typelib->data[blob->offset];
	      break;
	    case GI_TYPE_TAG_INT32:
	      value->v_int32 = *(gint32*)&rinfo->typelib->data[blob->offset];
	      break;
	    case GI_TYPE_TAG_UINT32:
	      value->v_uint32 = *(guint32*)&rinfo->typelib->data[blob->offset];
	      break;
	    case GI_TYPE_TAG_INT64:
	      value->v_int64 = *(gint64*)&rinfo->typelib->data[blob->offset];
	      break;
	    case GI_TYPE_TAG_UINT64:
	      value->v_uint64 = *(guint64*)&rinfo->typelib->data[blob->offset];
	      break;
	    case GI_TYPE_TAG_FLOAT:
	      value->v_float = *(gfloat*)&rinfo->typelib->data[blob->offset];
	      break;
	    case GI_TYPE_TAG_DOUBLE:
	      value->v_double = *(gdouble*)&rinfo->typelib->data[blob->offset];
	      break;
	    case GI_TYPE_TAG_TIME_T:
	      value->v_long = *(long*)&rinfo->typelib->data[blob->offset];
	      break;
	    case GI_TYPE_TAG_SHORT:
	      value->v_short = *(gshort*)&rinfo->typelib->data[blob->offset];
	      break;
	    case GI_TYPE_TAG_USHORT:
	      value->v_ushort = *(gushort*)&rinfo->typelib->data[blob->offset];
	      break;
	    case GI_TYPE_TAG_INT:
	      value->v_int = *(gint*)&rinfo->typelib->data[blob->offset];
	      break;
	    case GI_TYPE_TAG_UINT:
	      value->v_uint = *(guint*)&rinfo->typelib->data[blob->offset];
	      break;
	    case GI_TYPE_TAG_LONG:
	      value->v_long = *(glong*)&rinfo->typelib->data[blob->offset];
	      break;
	    case GI_TYPE_TAG_ULONG:
	      value->v_ulong = *(gulong*)&rinfo->typelib->data[blob->offset];
	      break;
	    }
	}
    }

  return blob->size;
}

