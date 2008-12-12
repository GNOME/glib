/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright (C) 2008 Christian Kellner, Samuel Cormier-Iijima
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
 * Authors: Christian Kellner <gicmo@gnome.org>
 *          Samuel Cormier-Iijima <sciyoshi@gmail.com>
 */

#include <config.h>
#include <glib.h>
#include <string.h>

#include "gunixsocketaddress.h"
#include "glibintl.h"
#include "gnetworkingprivate.h"

#include "gioalias.h"

/**
 * SECTION:gunixsocketaddress
 * @short_description: Unix socket addresses
 *
 * Support for UNIX-domain (aka local) sockets.
 **/

/**
 * GUnixSocketAddress:
 *
 * A UNIX-domain (local) socket address, corresponding to a
 * <type>struct sockaddr_un</type>.
 **/
G_DEFINE_TYPE (GUnixSocketAddress, g_unix_socket_address, G_TYPE_SOCKET_ADDRESS);

enum
{
  PROP_0,
  PROP_PATH,
};

struct _GUnixSocketAddressPrivate
{
  char *path;
};

static void
g_unix_socket_address_finalize (GObject *object)
{
  GUnixSocketAddress *address G_GNUC_UNUSED = G_UNIX_SOCKET_ADDRESS (object);

  g_free (address->priv->path);

  if (G_OBJECT_CLASS (g_unix_socket_address_parent_class)->finalize)
    (G_OBJECT_CLASS (g_unix_socket_address_parent_class)->finalize) (object);
}

static void
g_unix_socket_address_dispose (GObject *object)
{
  GUnixSocketAddress *address G_GNUC_UNUSED = G_UNIX_SOCKET_ADDRESS (object);

  if (G_OBJECT_CLASS (g_unix_socket_address_parent_class)->dispose)
    (*G_OBJECT_CLASS (g_unix_socket_address_parent_class)->dispose) (object);
}

static void
g_unix_socket_address_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  GUnixSocketAddress *address = G_UNIX_SOCKET_ADDRESS (object);

  switch (prop_id)
    {
      case PROP_PATH:
        g_value_set_string (value, address->priv->path);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static GSocketFamily
g_unix_socket_address_get_family (GSocketAddress *address)
{
  g_assert (PF_UNIX == G_SOCKET_FAMILY_UNIX);

  return G_SOCKET_FAMILY_UNIX;
}

static void
g_unix_socket_address_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  GUnixSocketAddress *address = G_UNIX_SOCKET_ADDRESS (object);

  switch (prop_id)
    {
      case PROP_PATH:
        g_free (address->priv->path);
        address->priv->path = g_value_dup_string (value);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static gssize
g_unix_socket_address_get_native_size (GSocketAddress *address)
{
  return sizeof (struct sockaddr_un);
}

static gboolean
g_unix_socket_address_to_native (GSocketAddress *address,
				 gpointer        dest,
				 gsize           destlen)
{
  GUnixSocketAddress *addr = G_UNIX_SOCKET_ADDRESS (address);
  struct sockaddr_un *sock;

  if (destlen < sizeof (*sock))
    return FALSE;

  sock = (struct sockaddr_un *) dest;
  sock->sun_family = AF_UNIX;
  g_strlcpy (sock->sun_path, addr->priv->path, sizeof (sock->sun_path));

  return TRUE;
}

static void
g_unix_socket_address_class_init (GUnixSocketAddressClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GSocketAddressClass *gsocketaddress_class = G_SOCKET_ADDRESS_CLASS (klass);

  g_type_class_add_private (klass, sizeof (GUnixSocketAddressPrivate));

  gobject_class->finalize = g_unix_socket_address_finalize;
  gobject_class->dispose = g_unix_socket_address_dispose;
  gobject_class->set_property = g_unix_socket_address_set_property;
  gobject_class->get_property = g_unix_socket_address_get_property;

  gsocketaddress_class->get_family = g_unix_socket_address_get_family;
  gsocketaddress_class->to_native = g_unix_socket_address_to_native;
  gsocketaddress_class->get_native_size = g_unix_socket_address_get_native_size;

  g_object_class_install_property (gobject_class,
                                   PROP_PATH,
                                   g_param_spec_string ("path",
                                                        _("Path"),
                                                        _("UNIX socket path"),
                                                        NULL,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB));
}

static void
g_unix_socket_address_init (GUnixSocketAddress *address)
{
  address->priv = G_TYPE_INSTANCE_GET_PRIVATE (address,
                                               G_TYPE_UNIX_SOCKET_ADDRESS,
                                               GUnixSocketAddressPrivate);

  address->priv->path = NULL;
}

/**
 * g_unix_socket_address_new:
 * @path: the socket path
 *
 * Creates a new #GUnixSocketAddress for @path.
 *
 * Returns: a new #GUnixSocketAddress
 *
 * Since: 2.22
 */
GSocketAddress *
g_unix_socket_address_new (const gchar *path)
{
  return g_object_new (G_TYPE_UNIX_SOCKET_ADDRESS,
		       "path", path,
		       NULL);
}

#define __G_UNIX_SOCKET_ADDRESS_C__
#include "gioaliasdef.c"
