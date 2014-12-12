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

#define G_TYPE_KDBUS_WORKER                                (g_kdbus_worker_get_type ())
#define G_KDBUS_WORKER(inst)                               (G_TYPE_CHECK_INSTANCE_CAST ((inst),                     \
                                                            G_TYPE_KDBUS_WORKER, GKDBusWorker))
#define G_KDBUS_WORKER_CLASS(class)                        (G_TYPE_CHECK_CLASS_CAST ((class),                       \
                                                            G_TYPE_KDBUS_WORKER, GKDBusWorkerClass))
#define G_IS_KDBUS_WORKER(inst)                            (G_TYPE_CHECK_INSTANCE_TYPE ((inst),                     \
                                                            G_TYPE_KDBUS_WORKER))
#define G_IS_KDBUS_WORKER_CLASS(class)                     (G_TYPE_CHECK_CLASS_TYPE ((class),                       \
                                                            G_TYPE_KDBUS_WORKER))
#define G_KDBUS_WORKER_GET_CLASS(inst)                     (G_TYPE_INSTANCE_GET_CLASS ((inst),                      \
                                                            G_TYPE_KDBUS_WORKER, GKDBusWorkerClass))

typedef struct _GKDBusWorker                               GKDBusWorker;

GType                   g_kdbus_worker_get_type                         (void);

GKDBusWorker *          g_kdbus_worker_new                              (const gchar  *address,
                                                                         GError      **error);

#if 0
                                                                         GDBusCapabilityFlags                     capabilities,
                                                                         gboolean                                 initially_frozen,
                                                                         GDBusWorkerMessageReceivedCallback       message_received_callback,
                                                                         GDBusWorkerMessageAboutToBeSentCallback  message_about_to_be_sent_callback,
                                                                         GDBusWorkerDisconnectedCallback          disconnected_callback,
                                                                         gpointer                                 user_data);
#endif

void                    g_kdbus_worker_unfreeze                         (GKDBusWorker *worker);

gboolean                g_kdbus_worker_send_message                     (GKDBusWorker  *worker,
                                                                         GDBusMessage  *message,
                                                                         GError       **error);

void                    g_kdbus_worker_stop                             (GKDBusWorker *worker);

void                    g_kdbus_worker_flush_sync                       (GKDBusWorker *worker);

void                    g_kdbus_worker_close                            (GKDBusWorker       *worker,
                                                                         GCancellable       *cancellable,
                                                                         GSimpleAsyncResult *result);


gboolean                                _g_kdbus_open                       (GKDBusWorker   *kdbus,
                                                                             const gchar    *address,
                                                                             GError        **error);

void                                    _g_kdbus_close                      (GKDBusWorker         *kdbus);

gboolean                                _g_kdbus_is_closed                  (GKDBusWorker         *kdbus);

GVariant *                              _g_kdbus_Hello                      (GKDBusWorker  *worker,
                                                                             GError       **error);

GVariant *                              _g_kdbus_GetBusId                   (GKDBusWorker  *worker,
                                                                             GError          **error);

GVariant *                              _g_kdbus_RequestName                (GKDBusWorker     *worker,
                                                                             const gchar         *name,
                                                                             GBusNameOwnerFlags   flags,
                                                                             GError             **error);

GVariant *                              _g_kdbus_ReleaseName                (GKDBusWorker     *worker,
                                                                             const gchar         *name,
                                                                             GError             **error);

GVariant *                              _g_kdbus_GetListNames               (GKDBusWorker  *worker,
                                                                             guint             flags,
                                                                             GError          **error);

GVariant *                              _g_kdbus_GetListQueuedOwners        (GKDBusWorker  *worker,
                                                                             const gchar      *name,
                                                                             GError          **error);

GVariant *                              _g_kdbus_GetNameOwner               (GKDBusWorker  *worker,
                                                                             const gchar      *name,
                                                                             GError          **error);

GVariant *                              _g_kdbus_GetConnectionUnixProcessID (GKDBusWorker  *worker,
                                                                             const gchar      *name,
                                                                             GError          **error);

GVariant *                              _g_kdbus_GetConnectionUnixUser      (GKDBusWorker  *worker,
                                                                             const gchar      *name,
                                                                             GError          **error);

void                                    _g_kdbus_subscribe_name_acquired    (GKDBusWorker  *worker,
                                                                             const gchar      *name);

void                                    _g_kdbus_subscribe_name_lost        (GKDBusWorker  *worker,
                                                                             const gchar      *name);

void                                    _g_kdbus_unsubscribe_name_acquired  (GKDBusWorker  *worker);

void                                    _g_kdbus_unsubscribe_name_lost      (GKDBusWorker  *worker);

gchar *                                 _g_kdbus_hexdump_all_items          (GSList           *kdbus_msg_items);

G_END_DECLS

#endif /* __G_KDBUS_H__ */
