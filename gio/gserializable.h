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

#ifndef __G_SERIALIZABLE_H__
#define __G_SERIALIZABLE_H__

#if !defined (__GIO_GIO_H_INSIDE__) && !defined (GIO_COMPILATION)
#error "Only <gio/gio.h> can be included directly."
#endif

#include <gio/giotypes.h>

G_BEGIN_DECLS

#define G_TYPE_SERIALIZABLE             (g_serializable_get_type ())
#define G_SERIALIZABLE(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), G_TYPE_SERIALIZABLE, GSerializable))
#define G_IS_SERIALIZABLE(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), G_TYPE_SERIALIZABLE))
#define G_SERIALIZABLE_GET_IFACE(obj)   (G_TYPE_INSTANCE_GET_INTERFACE ((obj), G_TYPE_SERIALIZABLE, GSerializableInterface))

/**
 * GSerializable:
 *
 * Interface for serializable types.
 *
 * Since: 2.38
 */
typedef struct _GSerializableInterface  GSerializableInterface;

/**
 * GSerializableInterface:
 * @serialize: virtual function used to encode a #GSerializable instance
 *   data into a #GEncoder
 * @deserialize: virtual function used to decode a #GSerializable instance
 *   from the data inside a #GEncoder
 *
 * Provides an interface for serializable types.
 *
 * Since: 2.38
 */
struct _GSerializableInterface
{
  GTypeInterface g_iface;

  void     (* serialize)   (GSerializable  *serializable,
                            GEncoder       *encoder);
  gboolean (* deserialize) (GSerializable  *serializable,
                            GEncoder       *encoder,
                            GError        **error);
};

GLIB_AVAILABLE_IN_2_38
GType g_serializable_get_type (void) G_GNUC_CONST;

GLIB_AVAILABLE_IN_2_38
void            g_serializable_serialize        (GSerializable *serializable,
                                                 GEncoder      *encoder);
GLIB_AVAILABLE_IN_2_38
gboolean        g_serializable_deserialize      (GSerializable *serializable,
                                                 GEncoder      *encoder,
                                                 GError        **error);

G_END_DECLS

#endif /* __G_SERIALIZABLE_H__ */
