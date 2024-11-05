/*
* Copyright © 2016 Red Hat, Inc.
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
* Author: Matthias Clasen
*/

#include "config.h"
#include "gnotificationbackend.h"

#include "giomodule-priv.h"
#include "gioenumtypes.h"
#include "gicon.h"
#include "gdbusconnection.h"
#include "gapplication.h"
#include "gnotification-private.h"
#include "gportalsupport.h"
#include "gtask.h"

#define G_TYPE_PORTAL_NOTIFICATION_BACKEND  (g_portal_notification_backend_get_type ())
#define G_PORTAL_NOTIFICATION_BACKEND(o)    (G_TYPE_CHECK_INSTANCE_CAST ((o), G_TYPE_PORTAL_NOTIFICATION_BACKEND, GPortalNotificationBackend))

typedef struct _GPortalNotificationBackend GPortalNotificationBackend;
typedef GNotificationBackendClass       GPortalNotificationBackendClass;

struct _GPortalNotificationBackend
{
  GNotificationBackend parent;

  guint version;
};

GType g_portal_notification_backend_get_type (void);

G_DEFINE_TYPE_WITH_CODE (GPortalNotificationBackend, g_portal_notification_backend, G_TYPE_NOTIFICATION_BACKEND,
  _g_io_modules_ensure_extension_points_registered ();
  g_io_extension_point_implement (G_NOTIFICATION_BACKEND_EXTENSION_POINT_NAME,
                                 g_define_type_id, "portal", 110))

static void
get_properties_cb (GObject      *source_object,
                   GAsyncResult *result,
                   gpointer      user_data)
{
  GTask *task = G_TASK (user_data);
  GPortalNotificationBackend *backend = g_task_get_source_object (task);
  GError *error = NULL;
  GVariant *ret;
  GVariant *vardict;

  ret = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source_object), result, &error);
  if (!ret)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      g_clear_object (&task);
      return;
    }

  g_variant_get (ret, "(@a{sv})", &vardict);

  if (!g_variant_lookup (vardict, "version", "u", &backend->version))
    backend->version = 1;

  g_task_return_boolean (task, TRUE);

  g_clear_pointer (&ret, g_variant_unref);
  g_clear_pointer (&vardict, g_variant_unref);
  g_clear_object (&task);
}

static void
get_supported_features (GPortalNotificationBackend *backend,
                        GAsyncReadyCallback         callback,
                        gpointer                    user_data)
{
  GTask *task;

  task = g_task_new (backend, NULL, callback, user_data);
  g_task_set_source_tag (task, get_supported_features);

  if (backend->version != 0)
    {
      g_task_return_boolean (task, TRUE);
      g_clear_object (&task);
      return;
    }

  g_dbus_connection_call (G_NOTIFICATION_BACKEND (backend)->dbus_connection,
                          "org.freedesktop.portal.Desktop",
                          "/org/freedesktop/portal/desktop",
                          "org.freedesktop.DBus.Properties",
                          "GetAll",
                          g_variant_new ("(s)", "org.freedesktop.portal.Notification"),
                          G_VARIANT_TYPE ("(a{sv})"),
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          g_task_get_cancellable (task),
                          get_properties_cb,
                          g_object_ref (task));

  g_clear_object (&task);
}

static gboolean
get_supported_features_finish (GPortalNotificationBackend  *backend,
                               GAsyncResult                *result,
                               GError                     **error)
{
  g_return_val_if_fail (G_IS_NOTIFICATION_BACKEND (backend), FALSE);
  g_return_val_if_fail (g_task_is_valid (result, backend), FALSE);
  g_return_val_if_fail (g_task_get_source_tag (G_TASK (result)) == get_supported_features, FALSE);

  return g_task_propagate_boolean (G_TASK (result), error);
}

static void
get_supported_features_cb (GObject      *source_object,
                           GAsyncResult *result,
                           gpointer      user_data)
{
  GPortalNotificationBackend *backend = G_PORTAL_NOTIFICATION_BACKEND (source_object);
  GVariant *notification_serialized = user_data;
  GError *error = NULL;

  if (!get_supported_features_finish (backend, result, &error))
    {
      g_warning ("Failed to get notification portal version: %s", error->message);
      g_clear_pointer (&error, g_error_free);
      return;
    }

  g_dbus_connection_call (G_NOTIFICATION_BACKEND (backend)->dbus_connection,
                          "org.freedesktop.portal.Desktop",
                          "/org/freedesktop/portal/desktop",
                          "org.freedesktop.portal.Notification",
                          "AddNotification",
                          notification_serialized,
                          G_VARIANT_TYPE_UNIT,
                          G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);
}

static GVariant *
serialize_buttons (GNotification *notification)
{
  GVariantBuilder builder;
  guint n_buttons;
  guint i;

  n_buttons = g_notification_get_n_buttons (notification);

  if (n_buttons == 0)
    return NULL;

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("aa{sv}"));

  for (i = 0; i < n_buttons; i++)
    {
      gchar *label;
      gchar *action_name;
      GVariant *target = NULL;

      g_notification_get_button (notification, i, &label, &action_name, &target);

      g_variant_builder_open (&builder, G_VARIANT_TYPE ("a{sv}"));

      g_variant_builder_add (&builder, "{sv}", "label", g_variant_new_take_string (label));
      g_variant_builder_add (&builder, "{sv}", "action", g_variant_new_take_string (action_name));

      if (target)
        {
          g_variant_builder_add (&builder, "{sv}", "target", target);
          g_clear_pointer (&target, g_variant_unref);
        }

      g_variant_builder_close (&builder);
    }

  return g_variant_builder_end (&builder);
}

static GVariant *
serialize_priority (GNotification *notification)
{
  GEnumClass *enum_class;
  GEnumValue *value;
  GVariant *nick;

  enum_class = g_type_class_ref (G_TYPE_NOTIFICATION_PRIORITY);
  value = g_enum_get_value (enum_class, g_notification_get_priority (notification));
  g_assert (value != NULL);
  nick = g_variant_new_string (value->value_nick);
  g_type_class_unref (enum_class);

  return nick;
}

static GVariant *
serialize_notification (GNotification *notification)
{
  GVariantBuilder builder;
  const gchar *body;
  GIcon *icon;
  gchar *default_action = NULL;
  GVariant *default_action_target = NULL;
  GVariant *buttons = NULL;

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("a{sv}"));

  g_variant_builder_add (&builder, "{sv}", "title", g_variant_new_string (g_notification_get_title (notification)));

  if ((body = g_notification_get_body (notification)))
    g_variant_builder_add (&builder, "{sv}", "body", g_variant_new_string (body));

  if ((icon = g_notification_get_icon (notification)))
    {
      GVariant *serialized_icon = NULL;

      if ((serialized_icon = g_icon_serialize (icon)))
        {
          g_variant_builder_add (&builder, "{sv}", "icon", serialized_icon);
          g_clear_object (&icon);
        }
    }

  g_variant_builder_add (&builder, "{sv}", "priority", serialize_priority (notification));

  if (g_notification_get_default_action (notification, &default_action, &default_action_target))
    {
      g_variant_builder_add (&builder, "{sv}", "default-action",
                                               g_variant_new_take_string (g_steal_pointer (&default_action)));

      if (default_action_target)
        {
          g_variant_builder_add (&builder, "{sv}", "default-action-target",
                                                    default_action_target);
          g_clear_pointer (&default_action_target, g_variant_unref);
        }
    }

  if ((buttons = serialize_buttons (notification)))
    g_variant_builder_add (&builder, "{sv}", "buttons", buttons);

  return g_variant_builder_end (&builder);
}

static gboolean
g_portal_notification_backend_is_supported (void)
{
  return glib_should_use_portal ();
}

static void
g_portal_notification_backend_send_notification (GNotificationBackend *backend,
                                                 const gchar          *id,
                                                 GNotification        *notification)
{
  GVariant *notification_serialized;

  notification_serialized = g_variant_new ("(s@a{sv})",
                                           id,
                                           serialize_notification (notification));

  get_supported_features (G_PORTAL_NOTIFICATION_BACKEND (backend),
                          get_supported_features_cb,
                          notification_serialized);
}

static void
g_portal_notification_backend_withdraw_notification (GNotificationBackend *backend,
                                                     const gchar          *id)
{
  g_dbus_connection_call (backend->dbus_connection,
                          "org.freedesktop.portal.Desktop",
                          "/org/freedesktop/portal/desktop",
                          "org.freedesktop.portal.Notification",
                          "RemoveNotification",
                          g_variant_new ("(s)", id),
                          G_VARIANT_TYPE_UNIT,
                          G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);
}

static void
g_portal_notification_backend_init (GPortalNotificationBackend *backend)
{
}

static void
g_portal_notification_backend_class_init (GPortalNotificationBackendClass *class)
{
  GNotificationBackendClass *backend_class = G_NOTIFICATION_BACKEND_CLASS (class);

  backend_class->is_supported = g_portal_notification_backend_is_supported;
  backend_class->send_notification = g_portal_notification_backend_send_notification;
  backend_class->withdraw_notification = g_portal_notification_backend_withdraw_notification;
}
