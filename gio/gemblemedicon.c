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
 *         Clemens N. Buss <cebuzz@gmail.com>
 */

#include <config.h>

#include <string.h>

#include "gemblemedicon.h"
#include "glibintl.h"
#include "gioerror.h"


/**
 * SECTION:gemblemedicon
 * @short_description: Icon with emblems
 * @include: gio/gio.h
 * @see_also: #GIcon, #GLoadableIcon, #GThemedIcon, #GEmblem
 *
 * #GEmblemedIcon is an implementation of #GIcon that supports
 * adding an emblem to an icon. Adding multiple emblems to an
 * icon is ensured via g_emblemed_icon_add_emblem(). 
 *
 * Note that #GEmblemedIcon allows no control over the position
 * of the emblems. See also #GEmblem for more information.
 **/

enum {
  PROP_GICON = 1,
  NUM_PROPERTIES
};

struct _GEmblemedIconPrivate {
  GIcon *icon;
  GList *emblems;
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

static void g_emblemed_icon_icon_iface_init (GIconIface *iface);

G_DEFINE_TYPE_WITH_CODE (GEmblemedIcon, g_emblemed_icon, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_ICON,
                         g_emblemed_icon_icon_iface_init))


static void
g_emblemed_icon_finalize (GObject *object)
{
  GEmblemedIcon *emblemed;

  emblemed = G_EMBLEMED_ICON (object);

  g_object_unref (emblemed->priv->icon);
  g_list_free_full (emblemed->priv->emblems, g_object_unref);

  (*G_OBJECT_CLASS (g_emblemed_icon_parent_class)->finalize) (object);
}

static void
g_emblemed_icon_set_property (GObject  *object,
                              guint property_id,
                              const GValue *value,
                              GParamSpec *pspec)
{
  GEmblemedIcon *self = G_EMBLEMED_ICON (object);

  switch (property_id)
    {
    case PROP_GICON:
      self->priv->icon = g_value_dup_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
g_emblemed_icon_get_property (GObject  *object,
                              guint property_id,
                              GValue *value,
                              GParamSpec *pspec)
{
  GEmblemedIcon *self = G_EMBLEMED_ICON (object);

  switch (property_id)
    {
    case PROP_GICON:
      g_value_set_object (value, self->priv->icon);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
g_emblemed_icon_class_init (GEmblemedIconClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = g_emblemed_icon_finalize;
  gobject_class->set_property = g_emblemed_icon_set_property;
  gobject_class->get_property = g_emblemed_icon_get_property;

  properties[PROP_GICON] =
    g_param_spec_object ("gicon",
                         P_("The base GIcon"),
                         P_("The GIcon to attach emblems to"),
                         G_TYPE_ICON,
                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, NUM_PROPERTIES, properties);

  g_type_class_add_private (klass, sizeof (GEmblemedIconPrivate));
}

static void
g_emblemed_icon_init (GEmblemedIcon *emblemed)
{
  emblemed->priv =
    G_TYPE_INSTANCE_GET_PRIVATE (emblemed, G_TYPE_EMBLEMED_ICON,
                                 GEmblemedIconPrivate);
}

/**
 * g_emblemed_icon_new:
 * @icon: a #GIcon
 * @emblem: (allow-none): a #GEmblem, or %NULL
 *
 * Creates a new emblemed icon for @icon with the emblem @emblem.
 *
 * Returns: (transfer full) (type GEmblemedIcon): a new #GIcon
 *
 * Since: 2.18
 **/
GIcon *
g_emblemed_icon_new (GIcon   *icon,
                     GEmblem *emblem)
{
  GEmblemedIcon *emblemed;
  
  g_return_val_if_fail (G_IS_ICON (icon), NULL);
  g_return_val_if_fail (!G_IS_EMBLEM (icon), NULL);

  emblemed = G_EMBLEMED_ICON (g_object_new (G_TYPE_EMBLEMED_ICON,
                                            "gicon", icon,
                                            NULL));

  if (emblem != NULL)
    g_emblemed_icon_add_emblem (emblemed, emblem);

  return G_ICON (emblemed);
}


/**
 * g_emblemed_icon_get_icon:
 * @emblemed: a #GEmblemedIcon
 *
 * Gets the main icon for @emblemed.
 *
 * Returns: (transfer none): a #GIcon that is owned by @emblemed
 *
 * Since: 2.18
 **/
GIcon *
g_emblemed_icon_get_icon (GEmblemedIcon *emblemed)
{
  g_return_val_if_fail (G_IS_EMBLEMED_ICON (emblemed), NULL);

  return emblemed->priv->icon;
}

/**
 * g_emblemed_icon_get_emblems:
 * @emblemed: a #GEmblemedIcon
 *
 * Gets the list of emblems for the @icon.
 *
 * Returns: (element-type Gio.Emblem) (transfer none): a #GList of
 *          #GEmblem <!-- -->s that is owned by @emblemed
 *
 * Since: 2.18
 **/

GList *
g_emblemed_icon_get_emblems (GEmblemedIcon *emblemed)
{
  g_return_val_if_fail (G_IS_EMBLEMED_ICON (emblemed), NULL);

  return emblemed->priv->emblems;
}

/**
 * g_emblemed_icon_clear_emblems:
 * @emblemed: a #GEmblemedIcon
 *
 * Removes all the emblems from @icon.
 *
 * Since: 2.28
 **/
void
g_emblemed_icon_clear_emblems (GEmblemedIcon *emblemed)
{
  g_return_if_fail (G_IS_EMBLEMED_ICON (emblemed));

  if (emblemed->priv->emblems == NULL)
    return;

  g_list_free_full (emblemed->priv->emblems, g_object_unref);
  emblemed->priv->emblems = NULL;
}

static gint
g_emblem_comp (GEmblem *a,
               GEmblem *b)
{
  guint hash_a = g_icon_hash (G_ICON (a));
  guint hash_b = g_icon_hash (G_ICON (b));

  if(hash_a < hash_b)
    return -1;

  if(hash_a == hash_b)
    return 0;

  return 1;
}

/**
 * g_emblemed_icon_add_emblem:
 * @emblemed: a #GEmblemedIcon
 * @emblem: a #GEmblem
 *
 * Adds @emblem to the #GList of #GEmblem <!-- -->s.
 *
 * Since: 2.18
 **/
void 
g_emblemed_icon_add_emblem (GEmblemedIcon *emblemed,
                            GEmblem       *emblem)
{
  g_return_if_fail (G_IS_EMBLEMED_ICON (emblemed));
  g_return_if_fail (G_IS_EMBLEM (emblem));

  g_object_ref (emblem);
  emblemed->priv->emblems = g_list_insert_sorted (emblemed->priv->emblems, emblem,
                                                  (GCompareFunc) g_emblem_comp);
}

static guint
g_emblemed_icon_hash (GIcon *icon)
{
  GEmblemedIcon *emblemed = G_EMBLEMED_ICON (icon);
  GList *list;
  guint hash = g_icon_hash (emblemed->priv->icon);

  for (list = emblemed->priv->emblems; list != NULL; list = list->next)
    hash ^= g_icon_hash (G_ICON (list->data));

  return hash;
}

static gboolean
g_emblemed_icon_equal (GIcon *icon1,
                       GIcon *icon2)
{
  GEmblemedIcon *emblemed1 = G_EMBLEMED_ICON (icon1);
  GEmblemedIcon *emblemed2 = G_EMBLEMED_ICON (icon2);
  GList *list1, *list2;

  if (!g_icon_equal (emblemed1->priv->icon, emblemed2->priv->icon))
    return FALSE;

  list1 = emblemed1->priv->emblems;
  list2 = emblemed2->priv->emblems;

  while (list1 && list2)
  {
    if (!g_icon_equal (G_ICON (list1->data), G_ICON (list2->data)))
        return FALSE;
    
    list1 = list1->next;
    list2 = list2->next;
  }
  
  return list1 == NULL && list2 == NULL;
}

static gboolean
g_emblemed_icon_to_tokens (GIcon *icon,
                           GPtrArray *tokens,
                           gint  *out_version)
{
  GEmblemedIcon *emblemed_icon = G_EMBLEMED_ICON (icon);
  GList *l;
  char *s;

  /* GEmblemedIcons are encoded as
   *
   *   <encoded_icon> [<encoded_emblem_icon>]*
   */

  g_return_val_if_fail (out_version != NULL, FALSE);

  *out_version = 0;

  s = g_icon_to_string (emblemed_icon->priv->icon);
  if (s == NULL)
    return FALSE;

  g_ptr_array_add (tokens, s);

  for (l = emblemed_icon->priv->emblems; l != NULL; l = l->next)
    {
      GIcon *emblem_icon = G_ICON (l->data);

      s = g_icon_to_string (emblem_icon);
      if (s == NULL)
        return FALSE;
      
      g_ptr_array_add (tokens, s);
    }

  return TRUE;
}

static GIcon *
g_emblemed_icon_from_tokens (gchar  **tokens,
                             gint     num_tokens,
                             gint     version,
                             GError **error)
{
  GEmblemedIcon *emblemed_icon;
  int n;

  emblemed_icon = NULL;

  if (version != 0)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_ARGUMENT,
                   _("Can't handle version %d of GEmblemedIcon encoding"),
                   version);
      goto fail;
    }

  if (num_tokens < 1)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_ARGUMENT,
                   _("Malformed number of tokens (%d) in GEmblemedIcon encoding"),
                   num_tokens);
      goto fail;
    }

  emblemed_icon = g_object_new (G_TYPE_EMBLEMED_ICON, NULL);
  emblemed_icon->priv->icon = g_icon_new_for_string (tokens[0], error);
  if (emblemed_icon->priv->icon == NULL)
    goto fail;

  for (n = 1; n < num_tokens; n++)
    {
      GIcon *emblem;

      emblem = g_icon_new_for_string (tokens[n], error);
      if (emblem == NULL)
        goto fail;

      if (!G_IS_EMBLEM (emblem))
        {
          g_set_error_literal (error,
                               G_IO_ERROR,
                               G_IO_ERROR_INVALID_ARGUMENT,
                               _("Expected a GEmblem for GEmblemedIcon"));
          g_object_unref (emblem);
          goto fail;
        }

      emblemed_icon->priv->emblems = g_list_append (emblemed_icon->priv->emblems, emblem);
    }

  return G_ICON (emblemed_icon);

 fail:
  if (emblemed_icon != NULL)
    g_object_unref (emblemed_icon);
  return NULL;
}

static void
g_emblemed_icon_icon_iface_init (GIconIface *iface)
{
  iface->hash = g_emblemed_icon_hash;
  iface->equal = g_emblemed_icon_equal;
  iface->to_tokens = g_emblemed_icon_to_tokens;
  iface->from_tokens = g_emblemed_icon_from_tokens;
}
