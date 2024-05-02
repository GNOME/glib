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

#ifndef __G_NOTIFICATION_H__
#define __G_NOTIFICATION_H__

#if !defined (__GIO_GIO_H_INSIDE__) && !defined (GIO_COMPILATION)
#error "Only <gio/gio.h> can be included directly."
#endif

#include <gio/giotypes.h>
#include <gio/gioenums.h>

G_BEGIN_DECLS

#define G_TYPE_NOTIFICATION         (g_notification_get_type ())
#define G_NOTIFICATION(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), G_TYPE_NOTIFICATION, GNotification))
#define G_IS_NOTIFICATION(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), G_TYPE_NOTIFICATION))

GIO_AVAILABLE_IN_2_40
GType                   g_notification_get_type                         (void) G_GNUC_CONST;

GIO_AVAILABLE_IN_2_40
GNotification *         g_notification_new                              (const gchar *title);

GIO_AVAILABLE_IN_2_40
void                    g_notification_set_title                        (GNotification *notification,
                                                                         const gchar   *title);

GIO_AVAILABLE_IN_2_40
void                    g_notification_set_body                         (GNotification *notification,
                                                                         const gchar   *body);

GIO_AVAILABLE_IN_2_85
void                    g_notification_set_body_with_markup             (GNotification *notification,
                                                                         const gchar   *markup_body);

GIO_AVAILABLE_IN_2_40
void                    g_notification_set_icon                         (GNotification *notification,
                                                                         GIcon         *icon);

GIO_AVAILABLE_IN_2_85
void                    g_notification_set_sound                        (GNotification      *notification,
                                                                         GNotificationSound *sound);

GIO_DEPRECATED_IN_2_42_FOR(g_notification_set_priority)
void                    g_notification_set_urgent                       (GNotification *notification,
                                                                         gboolean       urgent);

GIO_AVAILABLE_IN_2_42
void                    g_notification_set_priority                     (GNotification         *notification,
                                                                         GNotificationPriority  priority);

GIO_AVAILABLE_IN_2_85
void                    g_notification_set_display_hint_flags           (GNotification                 *notification,
                                                                         GNotificationDisplayHintFlags  flags);

GIO_AVAILABLE_IN_2_70
void                    g_notification_set_category                     (GNotification *notification,
                                                                         const gchar   *category);

GIO_AVAILABLE_IN_2_40
void                    g_notification_add_button                       (GNotification *notification,
                                                                         const gchar   *label,
                                                                         const gchar   *detailed_action);

GIO_AVAILABLE_IN_2_40
void                    g_notification_add_button_with_target           (GNotification *notification,
                                                                         const gchar   *label,
                                                                         const gchar   *action,
                                                                         const gchar   *target_format,
                                                                         ...);

GIO_AVAILABLE_IN_2_40
void                    g_notification_add_button_with_target_value     (GNotification *notification,
                                                                         const gchar   *label,
                                                                         const gchar   *action,
                                                                         GVariant      *target);

GIO_AVAILABLE_IN_2_40
void                    g_notification_set_default_action               (GNotification *notification,
                                                                         const gchar   *detailed_action);

GIO_AVAILABLE_IN_2_40
void                    g_notification_set_default_action_and_target    (GNotification *notification,
                                                                         const gchar   *action,
                                                                         const gchar   *target_format,
                                                                         ...);

GIO_AVAILABLE_IN_2_40
void                 g_notification_set_default_action_and_target_value (GNotification *notification,
                                                                         const gchar   *action,
                                                                         GVariant      *target);

/**
 * G_NOTIFICATION_CATEGORY_IM_RECEIVED:
 *
 * Intended for instant messaging apps displaying notifications for received messages.
 *
 * Since: 2.84
 */
#define G_NOTIFICATION_CATEGORY_IM_RECEIVED                 "im.received"

/**
 * G_NOTIFICATION_CATEGORY_ALARM_RINGING:
 *
 * Intended for alarm clock apps when an alarm is ringing.
 *
 * Since: 2.84
 */
#define G_NOTIFICATION_CATEGORY_ALARM_RINGING               "alarm.ringing"

/**
 * G_NOTIFICATION_CATEGORY_CALL_INCOMING:
 *
 * Intended for call apps to notify the user about an incoming call.
 *
 * Since: 2.84
 */
#define G_NOTIFICATION_CATEGORY_CALL_INCOMING               "call.incoming"

/**
 * G_NOTIFICATION_CATEGORY_CALL_OUTGOING:
 *
 * Intended for call apps to notify the user about an ongoing call.
 *
 * Since: 2.84
 */
#define G_NOTIFICATION_CATEGORY_CALL_OUTGOING               "call.ongoing"

/**
 * G_NOTIFICATION_CATEGORY_CALL_MISSED:
 *
 * Intended for call apps to notify the user about a missed call.
 *
 * Since: 2.84
 */
#define G_NOTIFICATION_CATEGORY_CALL_UNANSWERED             "call.unanswered"

/**
 * G_NOTIFICATION_CATEGORY_WEATHER_WARNING_EXTREME:
 *
 * Intended to be used to notify the user about extreme weather conditions.
 *
 * Since: 2.84
 */
#define G_NOTIFICATION_CATEGORY_WEATHER_WARNING_EXTREME     "weather.warning.extreme"

/**
 * G_NOTIFICATION_CATEGORY_CELLBROADCAST_DANGER_SEVERE:
 *
 * Intended to be used to notify users about severe danger warnings broadcasted by the cell network.
 *
 * Since: 2.84
 */
#define G_NOTIFICATION_CATEGORY_CELLBROADCAST_DANGER_SEVERE "cellbroadcast.danger.extreme"

/**
 * G_NOTIFICATION_CATEGORY_CELLBROADCAST_AMBER_ALERT:
 *
 * Intended to be used to notify users about amber alerts broadcasted by the cell network.
 *
 * Since: 2.84
 */
#define G_NOTIFICATION_CATEGORY_CELLBROADCAST_AMBER_ALERT   "cellbroadcast.amber-alert"

/**
 * G_NOTIFICATION_CATEGORY_CELLBROADCAST_TEST:
 *
 * Intended to be used to notify users about tests broadcasted by the cell network.
 *
 * Since: 2.84
 */
#define G_NOTIFICATION_CATEGORY_CELLBROADCAST_TEST          "cellbroadcast.test"

/**
 * G_NOTIFICATION_CATEGORY_OS_BATTERY_LOW:
 *
 * Intended to be used to indicate that the system is low on battery.
 *
 * Since: 2.84
 */
#define G_NOTIFICATION_CATEGORY_OS_BATTERY_LOW              "os.battery.low"

/**
 * G_NOTIFICATION_CATEGORY_BROWSER_WEB_NOTIFICATION:
 *
 * Intended to be used by browsers to mark notifications sent by websites via
 * the [Notifications API](https://developer.mozilla.org/en-US/docs/Web/API/Notifications_API).
 *
 * Since: 2.84
 */
#define G_NOTIFICATION_CATEGORY_BROWSER_WEB_NOTIFICATION    "browser.web-notification"

G_END_DECLS

#endif
