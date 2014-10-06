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

#include "config.h"
#include "gkdbusconnection.h"
#include "gunixconnection.h"

/**
 * SECTION:gkdbusconnection
 * @short_description: A kdbus connection
 * @include: gio/gio.h
 * @see_also: #GIOStream, #GKdbusClient
 *
 * #GKdbusConnection is a #GIOStream for a connected kdbus bus.
 */

#define g_kdbus_connection_get_type _g_kdbus_connection_get_type
G_DEFINE_TYPE (GKdbusConnection, g_kdbus_connection, G_TYPE_IO_STREAM);

struct _GKdbusConnectionPrivate
{
  GKdbus    *kdbus;
  gboolean   in_dispose;
};


/**
 * g_kdbus_connection_new:
 *
 */
GKdbusConnection *
_g_kdbus_connection_new (void)
{
  return g_object_new(G_TYPE_KDBUS_CONNECTION,NULL);
}


/**
 * g_kdbus_connection_connect:
 *
 */
gboolean
_g_kdbus_connection_connect (GKdbusConnection  *connection,
                             const gchar       *address,
                             GCancellable      *cancellable,
                             GError           **error)
{
  g_return_val_if_fail (G_IS_KDBUS_CONNECTION (connection), FALSE);

  return _g_kdbus_open (connection->priv->kdbus,address,error);
}


/**
 * g_kdbus_connection_constructed:
 *
 */
static void
g_kdbus_connection_constructed (GObject  *object)
{
  GKdbusConnection *connection = G_KDBUS_CONNECTION (object);

  g_assert (connection->priv->kdbus != NULL);
}


/**
 * g_kdbus_connection_finalize:
 *
 */
static void
g_kdbus_connection_finalize (GObject  *object)
{
  GKdbusConnection *connection = G_KDBUS_CONNECTION (object);

  g_object_unref (connection->priv->kdbus);

  G_OBJECT_CLASS (g_kdbus_connection_parent_class)->finalize (object);
}


/**
 * g_kdbus_connection_dispose:
 *
 */
static void
g_kdbus_connection_dispose (GObject  *object)
{
  GKdbusConnection *connection = G_KDBUS_CONNECTION (object);

  connection->priv->in_dispose = TRUE;

  G_OBJECT_CLASS (g_kdbus_connection_parent_class)
    ->dispose (object);

  connection->priv->in_dispose = FALSE;
}


/**
 * g_kdbus_connection_close:
 *
 */
static gboolean
g_kdbus_connection_close (GIOStream     *stream,
                          GCancellable  *cancellable,
                          GError       **error)
{
  GKdbusConnection *connection = G_KDBUS_CONNECTION (stream);

  if (connection->priv->in_dispose)
    return TRUE;

  return _g_kdbus_close (connection->priv->kdbus, error);
}


/**
 * g_kdbus_connection_close_async:
 *
 */
static void
g_kdbus_connection_close_async (GIOStream            *stream,
                                int                   io_priority,
                                GCancellable         *cancellable,
                                GAsyncReadyCallback   callback,
                                gpointer              user_data)
{
  GTask *task;
  GIOStreamClass *class;
  GError *error;

  class = G_IO_STREAM_GET_CLASS (stream);

  task = g_task_new (stream, cancellable, callback, user_data);

  error = NULL;
  if (class->close_fn &&
      !class->close_fn (stream, cancellable, &error))
    g_task_return_error (task, error);
  else
    g_task_return_boolean (task, TRUE);

  g_object_unref (task);
}


/**
 * g_kdbus_connection_close_finish:
 *
 */
static gboolean
g_kdbus_connection_close_finish (GIOStream     *stream,
                                 GAsyncResult  *result,
                                 GError       **error)
{
  return g_task_propagate_boolean (G_TASK (result), error);
}


/**
 * g_kdbus_connection_class_init:
 *
 */
static void
g_kdbus_connection_class_init (GKdbusConnectionClass  *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GIOStreamClass *stream_class = G_IO_STREAM_CLASS (klass);

  g_type_class_add_private (klass, sizeof (GKdbusConnectionPrivate));

  gobject_class->constructed = g_kdbus_connection_constructed;
  gobject_class->finalize = g_kdbus_connection_finalize;
  gobject_class->dispose = g_kdbus_connection_dispose;

  stream_class->close_fn = g_kdbus_connection_close;
  stream_class->close_async = g_kdbus_connection_close_async;
  stream_class->close_finish = g_kdbus_connection_close_finish;
}


/**
 * g_kdbus_connection_init:
 *
 */
static void
g_kdbus_connection_init (GKdbusConnection  *connection)
{
  connection->priv = G_TYPE_INSTANCE_GET_PRIVATE (connection,
                                                  G_TYPE_KDBUS_CONNECTION,
                                                  GKdbusConnectionPrivate);
  connection->priv->kdbus = g_object_new(G_TYPE_KDBUS,NULL);
}


/**
 * g_kdbus_connection_get_kdbus:
 *
 */
GKdbus *
_g_kdbus_connection_get_kdbus (GKdbusConnection  *connection)
{
  g_return_val_if_fail (G_IS_KDBUS_CONNECTION (connection), NULL);

  return connection->priv->kdbus;
}
