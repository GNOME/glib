/* GIO - GLib Input, Output and Streaming Library
 * 
 * Copyright (C) 2006-2007 Red Hat, Inc.
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
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Alexander Larsson <alexl@redhat.com>
 */

#include <config.h>
#include "gasyncresult.h"
#include "glibintl.h"

static void g_async_result_base_init (gpointer g_class);
static void g_async_result_class_init (gpointer g_class,
				       gpointer class_data);

GType
g_async_result_get_type (void)
{
  static GType async_result_type = 0;

  if (! async_result_type)
    {
      static const GTypeInfo async_result_info =
      {
        sizeof (GAsyncResultIface), /* class_size */
	g_async_result_base_init,   /* base_init */
	NULL,		            /* base_finalize */
	g_async_result_class_init,
	NULL,		/* class_finalize */
	NULL,		/* class_data */
	0,
	0,              /* n_preallocs */
	NULL
      };

      async_result_type =
	g_type_register_static (G_TYPE_INTERFACE, I_("GAsyncResult"),
				&async_result_info, 0);

      g_type_interface_add_prerequisite (async_result_type, G_TYPE_OBJECT);
    }

  return async_result_type;
}

static void
g_async_result_class_init (gpointer g_class,
			   gpointer class_data)
{
}

static void
g_async_result_base_init (gpointer g_class)
{
}

/**
 * g_async_result_get_user_data:
 * @res: a #GAsyncResult.
 * 
 * Returns: the user data for the given @res, or
 * %NULL on failure. 
 **/
gpointer
g_async_result_get_user_data (GAsyncResult *res)
{
  GAsyncResultIface *iface;

  g_return_val_if_fail (G_IS_ASYNC_RESULT (res), NULL);

  iface = G_ASYNC_RESULT_GET_IFACE (res);

  return (* iface->get_user_data) (res);
}

/**
 * g_async_result_get_source_object:
 * @res: a #GAsyncResult.
 * 
 * Returns: the source object for the @res.
 **/
GObject *
g_async_result_get_source_object (GAsyncResult *res)
{
  GAsyncResultIface *iface;

  g_return_val_if_fail (G_IS_ASYNC_RESULT (res), NULL);

  iface = G_ASYNC_RESULT_GET_IFACE (res);

  return (* iface->get_source_object) (res);
}
