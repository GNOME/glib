/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

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
 * Author: Matthias Clasen <mclasen@redhat.com>
 */

#include <config.h>

#include <string.h>

#include "gemblemedicon.h"
#include "glibintl.h"

#include "gioalias.h"

/**
 * SECTION:gemblemedicon
 * @short_description: Icon with emblems
 * @include: gio/gio.h
 * @see_also: #GIcon, #GLoadableIcon, #GThemedIcon
 *
 * #GEmblemedIcon is an implementation of #GIcon that supports
 * adding an emblem to an icon. To add multiple emblems to an
 * icon, you can create nested #GemblemedIcon<!-- -->s. 
 *
 * Note that #GEmblemedIcon allows no control over the position
 * of the emblems. It is up to the rendering code to pick a position.
 **/

static void g_emblemed_icon_icon_iface_init (GIconIface *iface);

struct _GEmblemedIcon
{
  GObject parent_instance;

  GIcon *icon;
  GIcon *emblem;
};

struct _GEmblemedIconClass
{
  GObjectClass parent_class;
};

G_DEFINE_TYPE_WITH_CODE (GEmblemedIcon, g_emblemed_icon, G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (G_TYPE_ICON,
						g_emblemed_icon_icon_iface_init))


static void
g_emblemed_icon_finalize (GObject *object)
{
  GEmblemedIcon *emblemed;

  emblemed = G_EMBLEMED_ICON (object);

  g_object_unref (emblemed->icon);
  g_object_unref (emblemed->emblem);

  (*G_OBJECT_CLASS (g_emblemed_icon_parent_class)->finalize) (object);
}

static void
g_emblemed_icon_class_init (GEmblemedIconClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = g_emblemed_icon_finalize;
}

static void
g_emblemed_icon_init (GEmblemedIcon *emblemed)
{
}

/**
 * g_emblemed_icon_new:
 * @icon: a #GIcon.
 * @emblem: a #GIcon
 *
 * Creates a new emblemed icon for @icon with emblem @emblem.
 *
 * Returns: a new #GEmblemedIcon.
 *
 * Since: 2.18
 **/
GIcon *
g_emblemed_icon_new (GIcon *icon, 
                     GIcon *emblem)
{
  GEmblemedIcon *emblemed;

  g_return_val_if_fail (icon != NULL, NULL);
  g_return_val_if_fail (emblem != NULL, NULL);

  emblemed = G_EMBLEMED_ICON (g_object_new (G_TYPE_EMBLEMED_ICON, NULL));
  emblemed->icon = g_object_ref (icon);
  emblemed->emblem = g_object_ref (emblem);

  return G_ICON (emblemed);
}

/**
 * g_emblemed_icon_get_icon:
 * @icon: a #GEmblemedIcon.
 *
 * Gets the main icon for @icon.
 *
 * Returns: a #GIcon that is owend by @icon
 *
 * Since: 2.18
 **/
GIcon *
g_emblemed_icon_get_icon (GEmblemedIcon *icon)
{
  g_return_val_if_fail (G_IS_EMBLEMED_ICON (icon), NULL);

  return icon->icon;
}

/**
 * g_emblemed_icon_get_emblem:
 * @icon: a #GEmblemedIcon.
 *
 * Gets the emblem for @icon.
 *
 * Returns: a #GIcon that is owned by @icon
 *
 * Since: 2.18
 **/
GIcon *
g_emblemed_icon_get_emblem (GEmblemedIcon *icon)
{
  g_return_val_if_fail (G_IS_EMBLEMED_ICON (icon), NULL);

  return icon->emblem;
}

static guint
g_emblemed_icon_hash (GIcon *icon)
{
  GEmblemedIcon *emblemed = G_EMBLEMED_ICON (icon);
  guint hash;

  hash = g_icon_hash (emblemed->icon);
  hash ^= g_icon_hash (emblemed->emblem);

  return hash;
}

static gboolean
g_emblemed_icon_equal (GIcon *icon1,
                       GIcon *icon2)
{
  GEmblemedIcon *emblemed1 = G_EMBLEMED_ICON (icon1);
  GEmblemedIcon *emblemed2 = G_EMBLEMED_ICON (icon2);

  return g_icon_equal (emblemed1->icon, emblemed2->icon) &&
         g_icon_equal (emblemed1->emblem, emblemed2->emblem);
}

static void
g_emblemed_icon_icon_iface_init (GIconIface *iface)
{
  iface->hash = g_emblemed_icon_hash;
  iface->equal = g_emblemed_icon_equal;
}

#define __G_EMBLEMED_ICON_C__
#include "gioaliasdef.c"
