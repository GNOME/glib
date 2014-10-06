/*  GIO - GLib Input, Output and Streaming Library
 *
 * Copyright (C) 2013 Samsung Electronics
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
 * Author: Michal Eljasiewicz   <m.eljasiewic@samsung.com>
 * Author: Lukasz Skalski       <l.skalski@samsung.com>
 */

#ifndef __G_KDBUS_CONNECTION_H__
#define __G_KDBUS_CONNECTION_H__

#if !defined (GIO_COMPILATION)
#error "gkdbusconnection.h is a private header file."
#endif

#include <glib-object.h>
#include <gio/gkdbus.h>
#include <gio/giostream.h>

G_BEGIN_DECLS

#define G_TYPE_KDBUS_CONNECTION              (_g_kdbus_connection_get_type ())
#define G_KDBUS_CONNECTION(o)                (G_TYPE_CHECK_INSTANCE_CAST ((o), G_TYPE_KDBUS_CONNECTION, GKdbusConnection))
#define G_KDBUS_CONNECTION_CLASS(k)          (G_TYPE_CHECK_CLASS_CAST((k), G_TYPE_KDBUS_CONNECTION, GKdbusConnectionClass))
#define G_KDBUS_CONNECTION_GET_CLASS(o)      (G_TYPE_INSTANCE_GET_CLASS ((o), G_TYPE_KDBUS_CONNECTION, GKdbusConnectionClass))
#define G_IS_KDBUS_CONNECTION(o)             (G_TYPE_CHECK_INSTANCE_TYPE ((o), G_TYPE_KDBUS_CONNECTION))
#define G_IS_KDBUS_CONNECTION_CLASS(k)       (G_TYPE_CHECK_CLASS_TYPE ((k), G_TYPE_KDBUS_CONNECTION))

typedef struct _GKdbusConnection              GKdbusConnection;
typedef struct _GKdbusConnectionClass         GKdbusConnectionClass;
typedef struct _GKdbusConnectionPrivate       GKdbusConnectionPrivate;

struct _GKdbusConnectionClass
{
  GIOStreamClass parent_class;
};

struct _GKdbusConnection
{
  GIOStream parent_instance;
  GKdbusConnectionPrivate *priv;
};


GType              _g_kdbus_connection_get_type                  (void) G_GNUC_CONST;

GKdbusConnection * _g_kdbus_connection_new                       (void);

gboolean           _g_kdbus_connection_connect                   (GKdbusConnection  *connection,
                                                                  const gchar       *address,
                                                                  GCancellable      *cancellable,
                                                                  GError           **error);

GKdbus *           _g_kdbus_connection_get_kdbus                 (GKdbusConnection  *connection);

G_END_DECLS

#endif /* __G_KDBUS_CONNECTION_H__ */
