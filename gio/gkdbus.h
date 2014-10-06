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

#ifndef __G_KDBUS_H__
#define __G_KDBUS_H__

#if !defined (GIO_COMPILATION)
#error "gkdbus.h is a private header file."
#endif

#include <gio/giotypes.h>
#include "gdbusprivate.h"

G_BEGIN_DECLS

#define G_TYPE_KDBUS                                       (_g_kdbus_get_type ())
#define G_KDBUS(o)                                         (G_TYPE_CHECK_INSTANCE_CAST ((o), G_TYPE_KDBUS, GKdbus))
#define G_KDBUS_CLASS(k)                                   (G_TYPE_CHECK_CLASS_CAST((k), G_TYPE_KDBUS, GKdbusClass))
#define G_KDBUS_GET_CLASS(o)                               (G_TYPE_INSTANCE_GET_CLASS ((o), G_TYPE_KDBUS, GKdbusClass))
#define G_IS_KDBUS(o)                                      (G_TYPE_CHECK_INSTANCE_TYPE ((o), G_TYPE_KDBUS))
#define G_IS_KDBUS_CLASS(k)                                (G_TYPE_CHECK_CLASS_TYPE ((k), G_TYPE_KDBUS))

typedef struct _GKdbus                                      GKdbus;
typedef struct _GKdbusClass                                 GKdbusClass;
typedef struct _GKdbusPrivate                               GKdbusPrivate;

struct _GKdbusClass
{
  GObjectClass parent_class;
};

struct _GKdbus
{
  GObject parent_instance;
  GKdbusPrivate *priv;
};

typedef struct
{
  gchar  *data;
  gsize   size;
} msg_part;

GType                                   _g_kdbus_get_type                  (void) G_GNUC_CONST;

gboolean                                _g_kdbus_open                      (GKdbus       *kdbus,
                                                                            const gchar  *address,
                                                                            GError      **error);

gboolean                                _g_kdbus_close                     (GKdbus  *kdbus,
                                                                            GError **error);

gboolean                                _g_kdbus_is_closed                 (GKdbus  *kdbus);


gssize                                  _g_kdbus_receive                   (GKdbus        *kdbus,
                                                                            GCancellable  *cancellable,
                                                                            GError       **error);

gsize                                   _g_kdbus_send                      (GDBusWorker   *worker,
                                                                            GKdbus        *kdbus,
                                                                            GDBusMessage  *dbus_msg,
                                                                            gchar         *blob,
                                                                            gsize          blob_size,
                                                                            GUnixFDList   *fd_list,
                                                                            GCancellable  *cancellable,
                                                                            GError       **error);

GSource *                               _g_kdbus_create_source             (GKdbus        *kdbus,
                                                                            GIOCondition   condition,
                                                                            GCancellable  *cancellable);

gchar *                                 _g_kdbus_hexdump_all_items         (GSList        *kdbus_msg_items);

void                                    _g_kdbus_release_kmsg              (GKdbus        *kdbus);

void                                    _g_kdbus_attach_fds_to_msg         (GKdbus        *kdbus,
                                                                            GUnixFDList  **fd_list);

GSList *                                _g_kdbus_get_last_msg_items        (GKdbus        *kdbus);

gchar *                                 _g_kdbus_get_last_msg_sender       (GKdbus        *kdbus);

gchar *                                 _g_kdbus_get_last_msg_destination  (GKdbus        *kdbus);

G_END_DECLS

#endif /* __G_KDBUS_H__ */
