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

#ifndef __G_NOTIFICATION_SOUND_H__
#define __G_NOTIFICATION_SOUND_H__

#if !defined (__GIO_GIO_H_INSIDE__) && !defined (GIO_COMPILATION)
#error "Only <gio/gio.h> can be included directly."
#endif

#include <gio/giotypes.h>

G_BEGIN_DECLS

#define G_TYPE_NOTIFICATION_SOUND         (g_notification_sound_get_type ())
#define G_NOTIFICATION_SOUND(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), G_TYPE_NOTIFICATION_SOUND, GNotificationSound))
#define G_IS_NOTIFICATION_SOUND(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), G_TYPE_NOTIFICATION_SOUND))

GIO_AVAILABLE_IN_2_85
GType                   g_notification_sound_get_type    (void) G_GNUC_CONST;

GIO_AVAILABLE_IN_2_85
GNotificationSound * g_notification_sound_new_from_file  (GFile *file);

GIO_AVAILABLE_IN_2_85
GNotificationSound * g_notification_sound_new_from_bytes (GBytes *bytes);

GIO_AVAILABLE_IN_2_85
GNotificationSound * g_notification_sound_new_default    (void);

GIO_AVAILABLE_IN_2_85
GNotificationSound * g_notification_sound_new_custom     (const char *action,
                                                          GVariant   *target);

G_END_DECLS

#endif /* __G_NOTIFICATION_SOUND_H__ */
