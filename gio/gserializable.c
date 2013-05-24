/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright (C) 2013  Emmanuele Bassi <ebassi@gnome.org>
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
 */

/**
 * SECTION:gserializable
 * @Title: #GSerializable
 *
 * #GSerializable provides an interface for objects that can be serialized (or
 * "encoded") and deserialized (or "decoded") through a #GEncoder instance.
 */

#include "config.h"

#include "gserializable.h"
#include "gencoder.h"
#include "glibintl.h"

G_DEFINE_INTERFACE (GSerializable, g_serializable, G_TYPE_OBJECT)

static void
g_serializable_real_serialize (GSerializable *serializable,
                               GEncoder      *encoder)
{
}

static gboolean
g_serializable_real_deserialize (GSerializable  *serializable,
                                 GEncoder       *encoder,
                                 GError        **error)
{
  return FALSE;
}

static void
g_serializable_default_init (GSerializableInterface *iface)
{
  iface->serialize = g_serializable_real_serialize;
  iface->deserialize = g_serializable_real_deserialize;
}

/**
 * g_serializable_serialize:
 * @serializable: a #GSerializable
 * @encoder: a #GEncoder
 *
 * Asks the @serializable instance to encode itself into @encoder.
 *
 * Since: 2.38
 */
void
g_serializable_serialize (GSerializable *serializable,
                          GEncoder      *encoder)
{
  g_return_if_fail (G_IS_SERIALIZABLE (serializable));
  g_return_if_fail (G_IS_ENCODER (encoder));

  G_SERIALIZABLE_GET_IFACE (serializable)->serialize (serializable, encoder);
}

/**
 * g_serializable_deserialize:
 * @serializable: a #GSerializable
 * @encoder: a #GEncoder
 *
 * Asks the @serializable instance to decncode itself from @encoder.
 *
 * Since: 2.38
 */
gboolean
g_serializable_deserialize (GSerializable  *serializable,
                            GEncoder       *encoder,
                            GError        **error)
{
  g_return_val_if_fail (G_IS_SERIALIZABLE (serializable), FALSE);
  g_return_val_if_fail (G_IS_ENCODER (encoder), FALSE);

  return G_SERIALIZABLE_GET_IFACE (serializable)->deserialize (serializable, encoder, error);
}
