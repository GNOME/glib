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
#include "gicon.h"

#include "glibintl.h"

static void g_icon_base_init (gpointer g_class);
static void g_icon_class_init (gpointer g_class,
			       gpointer class_data);

GType
g_icon_get_type (void)
{
  static GType icon_type = 0;

  if (! icon_type)
    {
      static const GTypeInfo icon_info =
      {
        sizeof (GIconIface), /* class_size */
	g_icon_base_init,   /* base_init */
	NULL,		/* base_finalize */
	g_icon_class_init,
	NULL,		/* class_finalize */
	NULL,		/* class_data */
	0,
	0,              /* n_preallocs */
	NULL
      };

      icon_type =
	g_type_register_static (G_TYPE_INTERFACE, I_("GIcon"),
				&icon_info, 0);

      g_type_interface_add_prerequisite (icon_type, G_TYPE_OBJECT);
    }

  return icon_type;
}

static void
g_icon_class_init (gpointer g_class,
		   gpointer class_data)
{
}

static void
g_icon_base_init (gpointer g_class)
{
}

/**
 * g_icon_hash:
 * @icon: #gconstpointer to an icon object.
 * 
 * Returns: a #guint containing a hash for the @icon, suitable for 
 * use in a #GHashTable or similar data structure.
 **/
guint
g_icon_hash (gconstpointer icon)
{
  GIconIface *iface;

  g_return_val_if_fail (G_IS_ICON (icon), 0);

  iface = G_ICON_GET_IFACE (icon);

  return (* iface->hash) ((GIcon *)icon);
}

/**
 * g_icon_equal:
 * @icon1: pointer to the first #GIcon.
 * @icon2: pointer to the second #GIcon.
 * 
 * Returns: %TRUE if @icon1 is equal to @icon2. %FALSE otherwise.
 **/
gboolean
g_icon_equal (GIcon *icon1,
	      GIcon *icon2)
{
  GIconIface *iface;

  if (icon1 == NULL && icon2 == NULL)
    return TRUE;

  if (icon1 == NULL || icon2 == NULL)
    return FALSE;
  
  if (G_TYPE_FROM_INSTANCE (icon1) != G_TYPE_FROM_INSTANCE (icon2))
    return FALSE;

  iface = G_ICON_GET_IFACE (icon1);
  
  return (* iface->equal) (icon1, icon2);
}

