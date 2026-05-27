/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright 2026 Christian Hergert
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <config.h>

#include "gvsocksocketaddress.h"

#include <string.h>

#include "gioerror.h"
#include "glibintl.h"
#include "gsocketconnectable.h"

#ifdef HAVE_LINUX_VM_SOCKETS_H
#include <linux/vm_sockets.h>
#endif

/**
 * GVsockSocketAddress:
 *
 * A virtio-vsock socket address, corresponding to Linux `struct sockaddr_vm`.
 *
 * `GVsockSocketAddress` is available on all platforms, but can only be
 * converted to a native socket address on Linux systems with
 * `<linux/vm_sockets.h>` support.
 *
 * Since: 2.90
 */

enum
{
  PROP_0,
  PROP_CID,
  PROP_PORT,
  PROP_FLAGS
};

struct _GVsockSocketAddressPrivate
{
  guint32 cid;
  guint32 port;
  guint8 flags;
};

static void   g_vsock_socket_address_connectable_iface_init (GSocketConnectableIface *iface);
static char  *g_vsock_socket_address_connectable_to_string  (GSocketConnectable      *connectable);

G_DEFINE_TYPE_WITH_CODE (GVsockSocketAddress, g_vsock_socket_address, G_TYPE_SOCKET_ADDRESS,
                         G_ADD_PRIVATE (GVsockSocketAddress)
                         G_IMPLEMENT_INTERFACE (G_TYPE_SOCKET_CONNECTABLE,
                                                g_vsock_socket_address_connectable_iface_init))

static void
g_vsock_socket_address_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  GVsockSocketAddress *address = G_VSOCK_SOCKET_ADDRESS (object);

  switch (prop_id)
    {
    case PROP_CID:
      address->priv->cid = g_value_get_uint (value);
      break;

    case PROP_PORT:
      address->priv->port = g_value_get_uint (value);
      break;

    case PROP_FLAGS:
      address->priv->flags = (guint8) g_value_get_uchar (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
g_vsock_socket_address_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  GVsockSocketAddress *address = G_VSOCK_SOCKET_ADDRESS (object);

  switch (prop_id)
    {
    case PROP_CID:
      g_value_set_uint (value, address->priv->cid);
      break;

    case PROP_PORT:
      g_value_set_uint (value, address->priv->port);
      break;

    case PROP_FLAGS:
      g_value_set_uchar (value, address->priv->flags);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static GSocketFamily
g_vsock_socket_address_get_family (GSocketAddress *address)
{
  return G_SOCKET_FAMILY_VSOCK;
}

static gssize
g_vsock_socket_address_get_native_size (GSocketAddress *address)
{
#ifdef HAVE_LINUX_VM_SOCKETS_H
  return sizeof (struct sockaddr_vm);
#else
  return -1;
#endif
}

static gboolean
g_vsock_socket_address_to_native (GSocketAddress  *address,
                                  gpointer         dest,
                                  gsize            destlen,
                                  GError         **error)
{
#ifdef HAVE_LINUX_VM_SOCKETS_H
  GVsockSocketAddress *addr = G_VSOCK_SOCKET_ADDRESS (address);
  struct sockaddr_vm *sock = dest;

  if (destlen < sizeof (*sock))
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NO_SPACE,
                           _("Not enough space for socket address"));
      return FALSE;
    }

  memset (sock, 0, sizeof (*sock));
  sock->svm_family = AF_VSOCK;
  sock->svm_cid = addr->priv->cid;
  sock->svm_port = addr->priv->port;
  sock->svm_flags = addr->priv->flags;

  return TRUE;
#else
  g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                       _("Vsock socket addresses not supported on this system"));
  return FALSE;
#endif
}

static void
g_vsock_socket_address_class_init (GVsockSocketAddressClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GSocketAddressClass *gsocketaddress_class = G_SOCKET_ADDRESS_CLASS (klass);

  gobject_class->set_property = g_vsock_socket_address_set_property;
  gobject_class->get_property = g_vsock_socket_address_get_property;

  gsocketaddress_class->get_family = g_vsock_socket_address_get_family;
  gsocketaddress_class->to_native = g_vsock_socket_address_to_native;
  gsocketaddress_class->get_native_size = g_vsock_socket_address_get_native_size;

  /**
   * GVsockSocketAddress:cid:
   *
   * The vsock context ID.
   *
   * Since: 2.90
   */
  g_object_class_install_property (gobject_class, PROP_CID,
                                   g_param_spec_uint ("cid", NULL, NULL,
                                                      0,
                                                      G_MAXUINT32,
                                                      0,
                                                      G_PARAM_READWRITE |
                                                      G_PARAM_CONSTRUCT_ONLY |
                                                      G_PARAM_STATIC_STRINGS));

  /**
   * GVsockSocketAddress:port:
   *
   * The vsock port.
   *
   * Since: 2.90
   */
  g_object_class_install_property (gobject_class, PROP_PORT,
                                   g_param_spec_uint ("port", NULL, NULL,
                                                      0,
                                                      G_MAXUINT32,
                                                      0,
                                                      G_PARAM_READWRITE |
                                                      G_PARAM_CONSTRUCT_ONLY |
                                                      G_PARAM_STATIC_STRINGS));

  /**
   * GVsockSocketAddress:flags:
   *
   * The vsock flags.
   *
   * Since: 2.90
   */
  g_object_class_install_property (gobject_class, PROP_FLAGS,
                                   g_param_spec_uchar ("flags", NULL, NULL,
                                                       0,
                                                       G_MAXUINT8,
                                                       0,
                                                       G_PARAM_READWRITE |
                                                       G_PARAM_CONSTRUCT_ONLY |
                                                       G_PARAM_STATIC_STRINGS));
}

static void
g_vsock_socket_address_connectable_iface_init (GSocketConnectableIface *iface)
{
  GSocketConnectableIface *parent_iface = g_type_interface_peek_parent (iface);

  iface->enumerate = parent_iface->enumerate;
  iface->proxy_enumerate = parent_iface->proxy_enumerate;
  iface->to_string = g_vsock_socket_address_connectable_to_string;
}

static char *
g_vsock_socket_address_connectable_to_string (GSocketConnectable *connectable)
{
  GVsockSocketAddress *address = G_VSOCK_SOCKET_ADDRESS (connectable);

  return g_strdup_printf ("%u:%u", address->priv->cid, address->priv->port);
}

static void
g_vsock_socket_address_init (GVsockSocketAddress *address)
{
  address->priv = g_vsock_socket_address_get_instance_private (address);
}

/**
 * g_vsock_socket_address_new:
 * @cid: the vsock context ID
 * @port: the vsock port
 *
 * Creates a new #GVsockSocketAddress for @cid and @port.
 *
 * Returns: (transfer full): a new #GVsockSocketAddress
 *
 * Since: 2.90
 */
GSocketAddress *
g_vsock_socket_address_new (guint32 cid,
                            guint32 port)
{
  return g_vsock_socket_address_new_with_flags (cid, port, 0);
}

/**
 * g_vsock_socket_address_new_with_flags:
 * @cid: the vsock context ID
 * @port: the vsock port
 * @flags: the vsock flags
 *
 * Creates a new #GVsockSocketAddress for @cid, @port, and @flags.
 *
 * Returns: (transfer full): a new #GVsockSocketAddress
 *
 * Since: 2.90
 */
GSocketAddress *
g_vsock_socket_address_new_with_flags (guint32 cid,
                                       guint32 port,
                                       guint8  flags)
{
  return g_object_new (G_TYPE_VSOCK_SOCKET_ADDRESS,
                       "cid", cid,
                       "port", port,
                       "flags", flags,
                       NULL);
}

/**
 * g_vsock_socket_address_get_cid:
 * @address: a #GVsockSocketAddress
 *
 * Gets @address's vsock context ID.
 *
 * Returns: the vsock context ID
 *
 * Since: 2.90
 */
guint32
g_vsock_socket_address_get_cid (GVsockSocketAddress *address)
{
  g_return_val_if_fail (G_IS_VSOCK_SOCKET_ADDRESS (address), 0);

  return address->priv->cid;
}

/**
 * g_vsock_socket_address_get_port:
 * @address: a #GVsockSocketAddress
 *
 * Gets @address's vsock port.
 *
 * Returns: the vsock port
 *
 * Since: 2.90
 */
guint32
g_vsock_socket_address_get_port (GVsockSocketAddress *address)
{
  g_return_val_if_fail (G_IS_VSOCK_SOCKET_ADDRESS (address), 0);

  return address->priv->port;
}

/**
 * g_vsock_socket_address_get_flags:
 * @address: a #GVsockSocketAddress
 *
 * Gets @address's vsock flags.
 *
 * Returns: the vsock flags
 *
 * Since: 2.90
 */
guint8
g_vsock_socket_address_get_flags (GVsockSocketAddress *address)
{
  g_return_val_if_fail (G_IS_VSOCK_SOCKET_ADDRESS (address), 0);

  return address->priv->flags;
}
