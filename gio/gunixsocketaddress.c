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
 * @short_description: UNIX GSocketAddress
 *
 * Support for UNIX-domain (aka local) sockets.
 */

/**
 * GUnixSocketAddress:
 *
 * A UNIX-domain (local) socket address, corresponding to a
 * <type>struct sockaddr_un</type>.
 */
G_DEFINE_TYPE (GUnixSocketAddress, g_unix_socket_address, G_TYPE_SOCKET_ADDRESS);

enum
{
  PROP_0,
  PROP_PATH,
  PROP_PATH_AS_ARRAY,
  PROP_ABSTRACT,
};

#define UNIX_PATH_MAX sizeof (((struct sockaddr_un *) 0)->sun_path)

struct _GUnixSocketAddressPrivate
{
  char path[UNIX_PATH_MAX]; /* Not including the initial zero in abstract case, so
			       we can guarantee zero termination of abstract
			       pathnames in the get_path() API */
  gsize path_len; /* Not including any terminating zeros */
  gboolean abstract;
};

static void
g_unix_socket_address_set_property (GObject      *object,
				    guint         prop_id,
				    const GValue *value,
				    GParamSpec   *pspec)
{
  GUnixSocketAddress *address = G_UNIX_SOCKET_ADDRESS (object);
  const char *str;
  GByteArray *array;
  gsize len;

  switch (prop_id)
    {
    case PROP_PATH:
      str = g_value_get_string (value);
      if (str)
	{
	  g_strlcpy (address->priv->path, str,
		     sizeof (address->priv->path));
	  address->priv->path_len = strlen (address->priv->path);
	}
      break;

    case PROP_PATH_AS_ARRAY:
      array = g_value_get_boxed (value);

      if (array)
	{
	  /* Clip to fit in UNIX_PATH_MAX with zero termination or first byte */
	  len = MIN (array->len, UNIX_PATH_MAX-1);

	  /* Remove any trailing zeros from path_len */
	  while (len > 0 && array->data[len-1] == 0)
	    len--;

	  memcpy (address->priv->path, array->data, len);
	  address->priv->path[len] = 0; /* Ensure null-terminated */
	  address->priv->path_len = len;
	}
      break;

    case PROP_ABSTRACT:
      address->priv->abstract = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
g_unix_socket_address_get_property (GObject    *object,
				    guint       prop_id,
				    GValue     *value,
				    GParamSpec *pspec)
{
  GUnixSocketAddress *address = G_UNIX_SOCKET_ADDRESS (object);
  GByteArray *array;

  switch (prop_id)
    {
      case PROP_PATH:
	g_value_set_string (value, address->priv->path);
	break;

      case PROP_PATH_AS_ARRAY:
	array = g_byte_array_sized_new (address->priv->path_len);
	g_byte_array_append (array, (guint8 *)address->priv->path, address->priv->path_len);
	g_value_take_boxed (value, array);
	break;

      case PROP_ABSTRACT:
	g_value_set_boolean (value, address->priv->abstract);
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

static gssize
g_unix_socket_address_get_native_size (GSocketAddress *address)
{
  return sizeof (struct sockaddr_un);
}

static gboolean
g_unix_socket_address_to_native (GSocketAddress *address,
				 gpointer        dest,
				 gsize           destlen,
				 GError        **error)
{
  GUnixSocketAddress *addr = G_UNIX_SOCKET_ADDRESS (address);
  struct sockaddr_un *sock;

  if (destlen < sizeof (*sock))
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NO_SPACE,
			   _("Not enough space for socket address"));
      return FALSE;
    }

  if (addr->priv->abstract &&
      !g_unix_socket_address_abstract_names_supported ())
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
			   _("Abstract unix domain socket addresses not supported on this system"));
      return FALSE;
    }

  sock = (struct sockaddr_un *) dest;
  sock->sun_family = AF_UNIX;
  memset (sock->sun_path, 0, sizeof (sock->sun_path));
  if (addr->priv->abstract)
    {
      sock->sun_path[0] = 0;
      memcpy (sock->sun_path+1, addr->priv->path, addr->priv->path_len);
    }
  else
    strcpy (sock->sun_path, addr->priv->path);

  return TRUE;
}

static void
g_unix_socket_address_class_init (GUnixSocketAddressClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GSocketAddressClass *gsocketaddress_class = G_SOCKET_ADDRESS_CLASS (klass);

  g_type_class_add_private (klass, sizeof (GUnixSocketAddressPrivate));

  gobject_class->set_property = g_unix_socket_address_set_property;
  gobject_class->get_property = g_unix_socket_address_get_property;

  gsocketaddress_class->get_family = g_unix_socket_address_get_family;
  gsocketaddress_class->to_native = g_unix_socket_address_to_native;
  gsocketaddress_class->get_native_size = g_unix_socket_address_get_native_size;

  g_object_class_install_property (gobject_class,
				   PROP_PATH,
				   g_param_spec_string ("path",
							P_("Path"),
							P_("UNIX socket path"),
							NULL,
							G_PARAM_READWRITE |
							G_PARAM_CONSTRUCT_ONLY |
							G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_PATH_AS_ARRAY,
				   g_param_spec_boxed ("path-as-array",
						       P_("Path array"),
						       P_("UNIX socket path, as byte array"),
						       G_TYPE_BYTE_ARRAY,
						       G_PARAM_READWRITE |
						       G_PARAM_CONSTRUCT_ONLY |
						       G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_ABSTRACT,
				   g_param_spec_boolean ("abstract",
							 P_("Abstract"),
							 P_("Whether or not this is an abstract address"),
							 FALSE,
							 G_PARAM_READWRITE |
							 G_PARAM_CONSTRUCT_ONLY |
							 G_PARAM_STATIC_STRINGS));
}

static void
g_unix_socket_address_init (GUnixSocketAddress *address)
{
  address->priv = G_TYPE_INSTANCE_GET_PRIVATE (address,
					       G_TYPE_UNIX_SOCKET_ADDRESS,
					       GUnixSocketAddressPrivate);

  memset (address->priv->path, 0, sizeof (address->priv->path));
  address->priv->path_len = -1;
}

/**
 * g_unix_socket_address_new:
 * @path: the socket path
 *
 * Creates a new #GUnixSocketAddress for @path.
 *
 * To create abstract socket addresses, on systems that support that,
 * use g_unix_socket_address_new_abstract().
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
		       "abstract", FALSE,
		       NULL);
}

/**
 * g_unix_socket_address_new_abstract:
 * @path: the abstract name
 * @path_len: the length of @path, or -1
 *
 * Creates a new abstract #GUnixSocketAddress for @path.
 *
 * Unix domain sockets are generally visible in the filesystem. However, some
 * systems support abstract socket name which are not visible in the
 * filesystem and not affected by the filesystem permissions, visibility, etc.
 *
 * Note that not all systems (really only Linux) support abstract
 * socket names, so if you use them on other systems function calls may
 * return %G_IO_ERROR_NOT_SUPPORTED errors. You can use
 * g_unix_socket_address_abstract_names_supported() to see if abstract
 * names are supported.
 *
 * If @path_len is -1 then @path is assumed to be a zero terminated
 * string (although in general abstract names need not be zero terminated
 * and can have embedded nuls). All bytes after @path_len up to the max size
 * of an abstract unix domain name is filled with zero bytes.
 *
 * Returns: a new #GUnixSocketAddress
 *
 * Since: 2.22
 */
GSocketAddress *
g_unix_socket_address_new_abstract (const gchar *path,
				    int path_len)
{
  GSocketAddress *address;
  GByteArray *array;

  if (path_len == -1)
    path_len = strlen (path);

  array = g_byte_array_sized_new (path_len);

  g_byte_array_append (array, (guint8 *)path, path_len);

  address = g_object_new (G_TYPE_UNIX_SOCKET_ADDRESS,
			  "path-as-array", array,
			  "abstract", TRUE,
			  NULL);

  g_byte_array_unref (array);

  return address;
}

/**
 * g_unix_socket_address_get_path:
 * @address: a #GInetSocketAddress
 *
 * Gets @address's path, or for abstract sockets the "name".
 *
 * Guaranteed to be zero-terminated, but an abstract socket
 * may contain embedded zeros, and thus you should use
 * g_unix_socket_address_get_path_len() to get the true length
 * of this string.
 *
 * Returns: the path for @address
 *
 * Since: 2.22
 */
const char *
g_unix_socket_address_get_path (GUnixSocketAddress *address)
{
  return address->priv->path;
}

/**
 * g_unix_socket_address_get_path_len:
 * @address: a #GInetSocketAddress
 *
 * Gets the length of @address's path.
 *
 * For details, see g_unix_socket_address_get_path().
 *
 * Returns: the length of the path
 *
 * Since: 2.22
 */
gsize
g_unix_socket_address_get_path_len (GUnixSocketAddress *address)
{
  return address->priv->path_len;
}

/**
 * g_unix_socket_address_get_is_abstract:
 * @address: a #GInetSocketAddress
 *
 * Gets @address's path.
 *
 * Returns: %TRUE if the address is abstract, %FALSE otherwise
 *
 * Since: 2.22
 */
gboolean
g_unix_socket_address_get_is_abstract (GUnixSocketAddress *address)
{
  return address->priv->abstract;
}

/**
 * g_unix_socket_address_abstract_names_supported:
 *
 * Checks if abstract unix domain socket names are supported.
 *
 * Returns: %TRUE if supported, %FALSE otherwise
 *
 * Since: 2.22
 */
gboolean
g_unix_socket_address_abstract_names_supported (void)
{
#ifdef __linux__
  return TRUE;
#else
  return FALSE;
#endif
}

#define __G_UNIX_SOCKET_ADDRESS_C__
#include "gioaliasdef.c"
