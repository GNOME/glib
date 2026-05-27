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

#ifndef __G_VSOCK_SOCKET_ADDRESS_H__
#define __G_VSOCK_SOCKET_ADDRESS_H__

#if !defined (__GIO_GIO_H_INSIDE__) && !defined (GIO_COMPILATION)
#error "Only <gio/gio.h> can be included directly."
#endif

#include <gio/gsocketaddress.h>

G_BEGIN_DECLS

#define G_TYPE_VSOCK_SOCKET_ADDRESS         (g_vsock_socket_address_get_type ())
#define G_VSOCK_SOCKET_ADDRESS(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), G_TYPE_VSOCK_SOCKET_ADDRESS, GVsockSocketAddress))
#define G_VSOCK_SOCKET_ADDRESS_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), G_TYPE_VSOCK_SOCKET_ADDRESS, GVsockSocketAddressClass))
#define G_IS_VSOCK_SOCKET_ADDRESS(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), G_TYPE_VSOCK_SOCKET_ADDRESS))
#define G_IS_VSOCK_SOCKET_ADDRESS_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), G_TYPE_VSOCK_SOCKET_ADDRESS))
#define G_VSOCK_SOCKET_ADDRESS_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), G_TYPE_VSOCK_SOCKET_ADDRESS, GVsockSocketAddressClass))

/**
 * G_VSOCK_SOCKET_ADDRESS_CID_ANY:
 *
 * The wildcard VM address, mapping to Linux `VMADDR_CID_ANY`.
 *
 * Since: 2.90
 */
#define G_VSOCK_SOCKET_ADDRESS_CID_ANY ((guint32) -1) GIO_AVAILABLE_MACRO_IN_2_90

/**
 * G_VSOCK_SOCKET_ADDRESS_CID_HYPERVISOR:
 *
 * The hypervisor VM address, mapping to Linux `VMADDR_CID_HYPERVISOR`.
 *
 * Since: 2.90
 */
#define G_VSOCK_SOCKET_ADDRESS_CID_HYPERVISOR ((guint32) 0) GIO_AVAILABLE_MACRO_IN_2_90

/**
 * G_VSOCK_SOCKET_ADDRESS_CID_LOCAL:
 *
 * The local VM address, mapping to Linux `VMADDR_CID_LOCAL`.
 *
 * Since: 2.90
 */
#define G_VSOCK_SOCKET_ADDRESS_CID_LOCAL ((guint32) 1) GIO_AVAILABLE_MACRO_IN_2_90

/**
 * G_VSOCK_SOCKET_ADDRESS_CID_HOST:
 *
 * The host VM address, mapping to Linux `VMADDR_CID_HOST`.
 *
 * Since: 2.90
 */
#define G_VSOCK_SOCKET_ADDRESS_CID_HOST ((guint32) 2) GIO_AVAILABLE_MACRO_IN_2_90

/**
 * G_VSOCK_SOCKET_ADDRESS_PORT_ANY:
 *
 * The wildcard VM port, mapping to Linux `VMADDR_PORT_ANY`.
 *
 * Since: 2.90
 */
#define G_VSOCK_SOCKET_ADDRESS_PORT_ANY ((guint32) -1) GIO_AVAILABLE_MACRO_IN_2_90

typedef struct _GVsockSocketAddressClass   GVsockSocketAddressClass;
typedef struct _GVsockSocketAddressPrivate GVsockSocketAddressPrivate;

struct _GVsockSocketAddress
{
  GSocketAddress parent_instance;

  /*< private >*/
  GVsockSocketAddressPrivate *priv;
};

struct _GVsockSocketAddressClass
{
  GSocketAddressClass parent_class;
};

GIO_AVAILABLE_IN_2_90
GType           g_vsock_socket_address_get_type       (void) G_GNUC_CONST;
GIO_AVAILABLE_IN_2_90
GSocketAddress *g_vsock_socket_address_new            (guint32              cid,
                                                       guint32              port);
GIO_AVAILABLE_IN_2_90
GSocketAddress *g_vsock_socket_address_new_with_flags (guint32              cid,
                                                       guint32              port,
                                                       guint8               flags);
GIO_AVAILABLE_IN_2_90
guint32         g_vsock_socket_address_get_cid        (GVsockSocketAddress *address);
GIO_AVAILABLE_IN_2_90
guint32         g_vsock_socket_address_get_port       (GVsockSocketAddress *address);
GIO_AVAILABLE_IN_2_90
guint8          g_vsock_socket_address_get_flags      (GVsockSocketAddress *address);

G_END_DECLS

#endif /* __G_VSOCK_SOCKET_ADDRESS_H__ */
