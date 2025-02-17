/*
 * Copyright © 2013 Lars Uebernickel
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Lars Uebernickel <lars@uebernic.de>
 */

#ifndef __G_NOTIFICATION_SERVER_H__
#define __G_NOTIFICATION_SERVER_H__

#include <glib-object.h>
#include <gio/gunixfdlist.h>

#define G_TYPE_NOTIFICATION_SERVER  (g_notification_server_get_type ())
#define G_NOTIFICATION_SERVER(o)    (G_TYPE_CHECK_INSTANCE_CAST ((o), G_TYPE_NOTIFICATION_SERVER, GNotificationServer))
#define G_IS_NOTIFICATION_SERVER(o) (G_TYPE_CHECK_INSTANCE_TYPE ((o), G_TYPE_NOTIFICATION_SERVER))

typedef struct _GNotificationServer GNotificationServer;

GType                   g_notification_server_get_type                  (void);

GNotificationServer *   g_notification_server_new                       (const gchar *backend_name,
                                                                         guint        backend_version);

void                    g_notification_server_stop                      (GNotificationServer *server);

gboolean                g_notification_server_get_is_running            (GNotificationServer *server);

GUnixFDList *           g_notification_server_get_unix_fd_list_for_notification (GNotificationServer *server,
                                                                                 GVariant            *notification);
gchar **                g_notification_server_list_applications         (GNotificationServer *server);

gchar **                g_notification_server_list_notifications        (GNotificationServer *server,
                                                                         const gchar         *app_id);

#endif
