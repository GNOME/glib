/*
 * Copyright © 2024 GNOME Foundation Inc.
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
 * Authors:
 *       Julian Sparber <jsparber@gnome.org>
 */

#ifndef __G_NOTIFICATION_SOUND_PRIVATE_H__
#define __G_NOTIFICATION_SOUND_PRIVATE_H__

#include "gnotificationsound.h"

GBytes *   g_notification_sound_get_bytes  (GNotificationSound *sound);
GFile *    g_notification_sound_get_file   (GNotificationSound *sound);
gboolean   g_notification_sound_is_default (GNotificationSound *sound);
gboolean   g_notification_sound_get_custom (GNotificationSound  *sound,
                                            gchar              **action,
                                            GVariant           **target);

#endif
