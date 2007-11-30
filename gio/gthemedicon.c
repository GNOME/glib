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

#include "gthemedicon.h"

#include "gioalias.h"

/**
 * SECTION:gthemedicon
 * @short_description: icon theming support
 * @see_also: #GIcon, #GLoadableIcon
 *
 * #GThemedIcon is an implementation of #GIcon that supports icon themes.
 * #GThemedIcon contains a list of all of the icons present in an icon
 * theme, so that icons can be looked up quickly. #GThemedIcon does
 * not provide actual pixmaps for icons, just the icon names.
 **/

static void g_themed_icon_icon_iface_init (GIconIface *iface);

struct _GThemedIcon
{
  GObject parent_instance;
  
  char **names;
};

struct _GThemedIconClass
{
  GObjectClass parent_class;
};

G_DEFINE_TYPE_WITH_CODE (GThemedIcon, g_themed_icon, G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (G_TYPE_ICON,
						g_themed_icon_icon_iface_init))
  
static void
g_themed_icon_finalize (GObject *object)
{
  GThemedIcon *themed;

  themed = G_THEMED_ICON (object);

  g_strfreev (themed->names);
  
  if (G_OBJECT_CLASS (g_themed_icon_parent_class)->finalize)
    (*G_OBJECT_CLASS (g_themed_icon_parent_class)->finalize) (object);
}

static void
g_themed_icon_class_init (GThemedIconClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  
  gobject_class->finalize = g_themed_icon_finalize;
}

static void
g_themed_icon_init (GThemedIcon *themed)
{
}

/**
 * g_themed_icon_new:
 * @iconname: a string containing an icon name.
 * 
 * Creates a new themed icon for @iconname.
 * 
 * Returns: a new #GThemedIcon.
 **/
GIcon *
g_themed_icon_new (const char *iconname)
{
  GThemedIcon *themed;

  g_return_val_if_fail (iconname != NULL, NULL);

  themed = g_object_new (G_TYPE_THEMED_ICON, NULL);
  themed->names = g_new (char *, 2);
  themed->names[0] = g_strdup (iconname);
  themed->names[1] = NULL;
  
  return G_ICON (themed);
}

/**
 * g_themed_icon_new_from_names:
 * @iconnames: an array of strings containing icon names.
 * @len: the number of elements in the @iconnames array.
 * 
 * Creates a new themed icon for @iconnames.
 * 
 * Returns: a new #GThemedIcon.
 **/
GIcon *
g_themed_icon_new_from_names (char **iconnames, 
                              int    len)
{
  GThemedIcon *themed;
  int i;

  g_return_val_if_fail (iconnames != NULL, NULL);
  
  themed = g_object_new (G_TYPE_THEMED_ICON, NULL);
  if (len == -1)
    themed->names = g_strdupv (iconnames);
  else
    {
      themed->names = g_new (char *, len + 1);
      for (i = 0; i < len; i++)
	themed->names[i] = g_strdup (iconnames[i]);
      themed->names[i] = NULL;
    }
  
  
  return G_ICON (themed);
}

/**
 * g_themed_icon_get_names:
 * @icon: a #GThemedIcon.
 * 
 * Gets the names of icons from within @icon.
 * 
 * Returns: a list of icon names.
 **/
const char * const *
g_themed_icon_get_names (GThemedIcon *icon)
{
  g_return_val_if_fail (G_IS_THEMED_ICON (icon), NULL);
  return (const char * const *)icon->names;
}

static guint
g_themed_icon_hash (GIcon *icon)
{
  GThemedIcon *themed = G_THEMED_ICON (icon);
  guint hash;
  int i;

  hash = 0;

  for (i = 0; themed->names[i] != NULL; i++)
    hash ^= g_str_hash (themed->names[i]);
  
  return hash;
}

static gboolean
g_themed_icon_equal (GIcon *icon1,
                     GIcon *icon2)
{
  GThemedIcon *themed1 = G_THEMED_ICON (icon1);
  GThemedIcon *themed2 = G_THEMED_ICON (icon2);
  int i;

  for (i = 0; themed1->names[i] != NULL && themed2->names[i] != NULL; i++)
    {
      if (!g_str_equal (themed1->names[i], themed2->names[i]))
	return FALSE;
    }

  return themed1->names[i] == NULL && themed2->names[i] == NULL;
}

static void
g_themed_icon_icon_iface_init (GIconIface *iface)
{
  iface->hash = g_themed_icon_hash;
  iface->equal = g_themed_icon_equal;
}

#define __G_THEMED_ICON_C__
#include "gioaliasdef.c"
