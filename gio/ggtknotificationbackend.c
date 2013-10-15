/*
* Copyright © 2013 Lars Uebernickel
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
* Authors: Lars Uebernickel <lars@uebernic.de>
*/

#include "gnotificationbackend.h"

#include "giomodule-priv.h"
#include "gdbusconnection.h"
#include "gapplication.h"
#include "gnotification-private.h"

#define G_TYPE_GTK_NOTIFICATION_BACKEND  (g_gtk_notification_backend_get_type ())
#define G_GTK_NOTIFICATION_BACKEND(o)    (G_TYPE_CHECK_INSTANCE_CAST ((o), G_TYPE_GTK_NOTIFICATION_BACKEND, GGtkNotificationBackend))

typedef struct _GGtkNotificationBackend GGtkNotificationBackend;
typedef GNotificationBackendClass       GGtkNotificationBackendClass;

struct _GGtkNotificationBackend
{
  GNotificationBackend parent;

  GDBusConnection *session_bus;
};

GType g_gtk_notification_backend_get_type (void);

G_DEFINE_TYPE_WITH_CODE (GGtkNotificationBackend, g_gtk_notification_backend, G_TYPE_NOTIFICATION_BACKEND,
  _g_io_modules_ensure_extension_points_registered ();
  g_io_extension_point_implement (G_NOTIFICATION_BACKEND_EXTENSION_POINT_NAME,
                                 g_define_type_id, "gtk", 100))

static void
g_gtk_notification_backend_dispose (GObject *object)
{
  GGtkNotificationBackend *backend = G_GTK_NOTIFICATION_BACKEND (object);

  g_clear_object (&backend->session_bus);

  G_OBJECT_CLASS (g_gtk_notification_backend_parent_class)->dispose (object);
}

static gboolean
g_gtk_notification_backend_is_supported (void)
{
  GDBusConnection *session_bus;
  GVariant *reply;

  /* Find out if the notification server is running. This is a
   * synchronous call because gio extension points don't support asnyc
   * backend verification. This is only run once and only contacts the
   * dbus daemon. */

  session_bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
  if (session_bus == NULL)
    return FALSE;

  reply = g_dbus_connection_call_sync (session_bus, "org.freedesktop.DBus", "/", "org.freedesktop.DBus",
                                       "GetNameOwner", g_variant_new ("(s)", "org.gtk.Notifications"),
                                       G_VARIANT_TYPE ("(s)"), G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL);

  g_object_unref (session_bus);

  if (reply)
    {
      g_variant_unref (reply);
      return TRUE;
    }
  else
    return FALSE;
}

static void
g_gtk_notification_backend_send_notification (GNotificationBackend *backend,
                                              const gchar          *id,
                                              GNotification        *notification)
{
  GApplication *application;
  GVariant *params;

  application = g_notification_backend_get_application (backend);

  params = g_variant_new ("(ss@a{sv})", g_application_get_application_id (application),
                                        id,
                                        g_notification_serialize (notification));

  g_dbus_connection_call (g_notification_backend_get_dbus_connection (backend),
                          "org.gtk.Notifications", "/org/gtk/Notifications",
                          "org.gtk.Notifications", "AddNotification", params,
                          G_VARIANT_TYPE_UNIT,
                          G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);
}

static void
g_gtk_notification_backend_withdraw_notification (GNotificationBackend *backend,
                                                  const gchar          *id)
{
  GGtkNotificationBackend *self = G_GTK_NOTIFICATION_BACKEND (backend);
  GApplication *application;
  GVariant *params;

  application = g_notification_backend_get_application (backend);

  params = g_variant_new ("(ss)", g_application_get_application_id (application), id);

  g_dbus_connection_call (self->session_bus, "org.gtk.Notifications",
                          "/org/gtk/Notifications", "org.gtk.Notifications",
                          "RemoveNotification", params, G_VARIANT_TYPE_UNIT,
                          G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);
}

static void
g_gtk_notification_backend_init (GGtkNotificationBackend *backend)
{
}

static void
g_gtk_notification_backend_class_init (GGtkNotificationBackendClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GNotificationBackendClass *backend_class = G_NOTIFICATION_BACKEND_CLASS (class);

  object_class->dispose = g_gtk_notification_backend_dispose;

  backend_class->is_supported = g_gtk_notification_backend_is_supported;
  backend_class->send_notification = g_gtk_notification_backend_send_notification;
  backend_class->withdraw_notification = g_gtk_notification_backend_withdraw_notification;
}
