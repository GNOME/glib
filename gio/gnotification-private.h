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

#ifndef __G_NOTIFICATION_PRIVATE_H__
#define __G_NOTIFICATION_PRIVATE_H__

#include "gnotificationsound-private.h"
#include "gnotification.h"

const gchar *           g_notification_get_id                           (GNotification *notification);

const gchar *           g_notification_get_title                        (GNotification *notification);

const gchar *           g_notification_get_body                         (GNotification *notification);

const gchar *           g_notification_get_body_with_markup             (GNotification *notification);

const gchar *           g_notification_get_category                     (GNotification *notification);

GIcon *                 g_notification_get_icon                         (GNotification *notification);

GNotificationSound *    g_notification_get_sound                        (GNotification *notification);

GNotificationPriority   g_notification_get_priority                     (GNotification *notification);

GNotificationDisplayHintFlags g_notification_get_display_hint_flags     (GNotification *notification);

guint                   g_notification_get_n_buttons                    (GNotification *notification);

void                    g_notification_get_button                       (GNotification  *notification,
                                                                         gint            index,
                                                                         gchar         **label,
                                                                         gchar         **purpose,
                                                                         gchar         **action,
                                                                         GVariant      **target);

gint                    g_notification_get_button_with_action           (GNotification *notification,
                                                                         const gchar   *action);

gboolean                g_notification_get_default_action               (GNotification  *notification,
                                                                         gchar         **action,
                                                                         GVariant      **target);

GVariant *              g_notification_serialize                        (GNotification *notification);

#endif
