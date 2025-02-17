/*
 * Copyright © 2013 Lars Uebernickel
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
 * Authors: Lars Uebernickel <lars@uebernic.de>
 *          Julian Sparber <jsparber@gnome.org>
 */

#include <glib.h>
#include <gio/gunixfdlist.h>

#include "gnotification-server.h"
#include "gdbus-sessionbus.h"

#define TEST_DATA "some test data"

struct _GNotification
{
  GObject parent;

  gchar *title;
  gchar *body;
  gchar *markup_body;
  GIcon *icon;
  GNotificationSound *sound;
  GNotificationPriority priority;
  gchar *category;
  GNotificationDisplayHintFlags display_hint;
  GPtrArray *buttons;
  gchar *default_action;
  GVariant *default_action_target;
  gchar *response_action;
  GVariant *response_action_target;
};

typedef enum
{
  SOUND_TYPE_DEFAULT,
  SOUND_TYPE_FILE,
  SOUND_TYPE_BYTES,
  SOUND_TYPE_CUSTOM,
} SoundType;

struct _GNotificationSound
{
  GObject parent;

  SoundType sound_type;
  union {
    GFile *file;
    GBytes *bytes;
    struct {
      gchar *action;
      GVariant *target;
    } custom;
  };
};

typedef struct
{
  gchar *label;
  gchar *purpose;
  gchar *action_name;
  GVariant *target;
} Button;

typedef struct
{
  gchar *id;
  GNotification *notification;
  GMainLoop *loop;
} TestData;

static void
test_data_free (gpointer pointer)
{
  TestData *data = pointer;

  g_clear_pointer (&data->id, g_free);
  g_clear_object (&data->notification);
  g_main_loop_unref (data->loop);
}

static void
send_and_wait (TestData      *data,
               GApplication  *application,
               const gchar   *id,
               GNotification *notification)
{
  g_clear_pointer (&data->id, g_free);
  data->id = g_strdup (id);
  g_set_object (&data->notification, notification);

  g_application_send_notification (application, id, notification);

  while (g_main_context_iteration (g_main_loop_get_context (data->loop), TRUE))
    {
      if (data->id == NULL && data->notification == NULL)
        {
          break;
        }
    }
}

static void
send_and_wait_finish (TestData *data)
{
  g_clear_pointer (&data->id, g_free);
  g_clear_object (&data->notification);

  g_main_context_wakeup (g_main_loop_get_context (data->loop));
}

static GFile *
get_test_file (void) {
  GFile *file = NULL;
  g_autoptr(GFileIOStream) iostream = NULL;
  GOutputStream *stream = NULL;

  file = g_file_new_tmp ("notification-testXXXXXX", &iostream, NULL);
  stream = g_io_stream_get_output_stream (G_IO_STREAM (iostream));
  g_output_stream_write_all (stream, TEST_DATA, strlen (TEST_DATA), NULL, NULL, NULL);
  g_output_stream_close (stream, NULL, NULL);

  return file;
}

static void
activate_app (GApplication *application,
              gpointer      user_data)
{
  TestData *data = user_data;
  g_autoptr(GNotification) notification = NULL;
  g_autoptr(GIcon) icon = NULL;
  g_autoptr(GBytes) bytes = NULL;
  g_autoptr(GNotificationSound) sound = NULL;
  g_autoptr(GFile) file = NULL;

  bytes = g_bytes_new_static (TEST_DATA, strlen (TEST_DATA));
  file = get_test_file ();

  notification = g_notification_new ("Test");
  send_and_wait (data, application, "test1", notification);

  notification = g_notification_new ("Test2");
  send_and_wait (data, application, "test2", notification);

  g_application_withdraw_notification (application, "test1");

  notification = g_notification_new ("Test3");
  send_and_wait (data, application, "test3", notification);

  notification = g_notification_new ("Test4");
  icon = g_themed_icon_new ("i-c-o-n");
  g_notification_set_icon (notification, icon);
  g_clear_object (&icon);
  g_notification_set_body (notification, "body");
  g_notification_set_body_with_markup (notification, "markup-body");
  g_notification_set_priority (notification, G_NOTIFICATION_PRIORITY_URGENT);
  g_notification_set_default_action_and_target (notification, "app.action", "i", 42);
  g_notification_add_button_with_purpose_and_target_value (notification,
                                                           "label",
                                                           "x-gnome.purpose",
                                                           "app.action2",
                                                           g_variant_new_string ("bla"));
  g_notification_set_category (notification, "x-gnome.category");
  g_notification_set_display_hint_flags (notification, G_NOTIFICATION_DISPLAY_HINT_TRANSIENT);
  send_and_wait (data, application, "test4", notification);

  notification = g_notification_new ("Test5");
  icon = g_file_icon_new (file);
  g_notification_set_icon (notification, icon);
  g_clear_object (&icon);
  send_and_wait (data, application, "test5", notification);

  notification = g_notification_new ("Test6");
  icon = g_bytes_icon_new (bytes);
  g_notification_set_icon (notification, icon);
  g_clear_object (&icon);
  send_and_wait (data, application, "test6", notification);

  notification = g_notification_new ("Test7");
  sound = g_notification_sound_new_default ();
  g_notification_set_sound (notification, sound);
  g_clear_object (&sound);
  send_and_wait (data, application, "test7", notification);

  notification = g_notification_new ("Test8");
  sound = g_notification_sound_new_from_file (file);
  g_notification_set_sound (notification, sound);
  g_clear_object (&sound);
  send_and_wait (data, application, "test8", notification);

  notification = g_notification_new ("Test9");
  sound = g_notification_sound_new_from_bytes (bytes);
  g_notification_set_sound (notification, sound);
  g_clear_object (&sound);
  send_and_wait (data, application, "test9", notification);

  notification = g_notification_new ("test10");
  sound = g_notification_sound_new_custom ("app.play-custom-sound", g_variant_new_string ("some target"));
  g_notification_set_sound (notification, sound);
  g_clear_object (&sound);
  send_and_wait (data, application, "test10", notification);

  notification = g_notification_new ("test11");
  g_notification_set_response_action_for_text (notification, "app.response", g_variant_new_string("some target"));
  send_and_wait (data, application, "test11", notification);

  send_and_wait (data, application, NULL, notification);

  g_dbus_connection_flush_sync (g_application_get_dbus_connection (application), NULL, NULL);

  g_assert_true (g_file_delete (file, NULL, NULL));
  g_main_loop_quit (data->loop);
}

static void
notification_received (GNotificationServer *server,
                       const gchar         *app_id,
                       const gchar         *notification_id,
                       GVariant            *notification,
                       gpointer             user_data)
{
  TestData *exp_data = user_data;
  struct _GNotification *exp_notification;
  gboolean has_response_action = FALSE;

  g_assert_nonnull (exp_data);
  exp_notification = (struct _GNotification *)exp_data->notification;
  g_assert_nonnull (exp_notification);

  if (exp_data->id)
    g_assert_cmpstr (exp_data->id, ==, notification_id);
  else
    g_assert_true (g_dbus_is_guid (notification_id));

  if (exp_notification->title)
    {
      const gchar *title;
      g_assert_true (g_variant_lookup (notification, "title", "&s", &title));
      g_assert_cmpstr (title, ==, exp_notification->title);
    }

  if (exp_notification->body && !exp_notification->markup_body)
    {
      const gchar *body;
      g_assert_true (g_variant_lookup (notification, "body", "&s", &body));
      g_assert_cmpstr (body, ==, exp_notification->body);
    }

  if (exp_notification->markup_body)
    {
      const gchar *body;
      g_assert_true (g_variant_lookup (notification, "markup-body", "&s", &body));
      g_assert_cmpstr (body, ==, exp_notification->markup_body);
    }

  if (exp_notification->icon)
    {
      g_autoptr(GVariant) serialized_icon = NULL;

      serialized_icon = g_variant_lookup_value (notification, "icon", NULL);
      if (G_IS_THEMED_ICON (exp_notification->icon))
        {
          g_autoptr(GIcon) icon = NULL;

          icon = g_icon_deserialize (serialized_icon);
          g_assert_true (g_icon_equal (exp_notification->icon, icon));
        }
      else
        {
          g_autoptr(GError) error = NULL;
          g_autoptr(GBytes) bytes = NULL;
          g_autoptr(GBytes) exp_bytes = NULL;
          GUnixFDList *fd_list;
          int fd;
          int fd_id;
          gchar *key;
          g_autoptr(GVariant) handle = NULL;
          g_autoptr(GMappedFile) mapped = NULL;

          g_assert_true (g_variant_is_of_type (serialized_icon, G_VARIANT_TYPE("(sv)")));
          g_variant_get (serialized_icon, "(&sv)", &key, &handle);
          g_assert_cmpstr (key, ==, "file-descriptor");

          fd_list = g_notification_server_get_unix_fd_list_for_notification (server, notification);
          g_assert_nonnull (fd_list);
          fd_id = g_variant_get_handle (handle);
          fd = g_unix_fd_list_get (fd_list, fd_id, &error);
          g_assert_no_error (error);
          g_assert_cmpint (fd, >, -1);

          mapped = g_mapped_file_new_from_fd (fd, FALSE, &error);
          g_assert_no_error (error);
          bytes = g_mapped_file_get_bytes (mapped);

          if (G_IS_BYTES_ICON (exp_notification->icon))
            {
              exp_bytes = g_bytes_ref (g_bytes_icon_get_bytes (G_BYTES_ICON (exp_notification->icon)));
            }
          else if (G_IS_FILE_ICON (exp_notification->icon))
            {
              GFile *file;

              file = g_file_icon_get_file (G_FILE_ICON (exp_notification->icon));
              exp_bytes = g_file_load_bytes (file, NULL, NULL, &error);
              g_assert_no_error (error);
            }
          g_assert_true (g_bytes_equal (exp_bytes, bytes));
        }
    }

  if (exp_notification->sound)
    {
      struct _GNotificationSound *exp_sound = (struct _GNotificationSound *)exp_notification->sound;
      g_autoptr(GVariant) serialized_sound = NULL;
      g_autoptr(GError) error = NULL;
      g_autoptr(GBytes) bytes = NULL;

      serialized_sound = g_variant_lookup_value (notification, "sound", NULL);
      if (exp_sound->sound_type == SOUND_TYPE_FILE || exp_sound->sound_type == SOUND_TYPE_BYTES)
        {
          GUnixFDList *fd_list;
          int fd;
          int fd_id;
          gchar *key;
          g_autoptr(GVariant) handle = NULL;
          g_autoptr(GMappedFile) mapped = NULL;

          g_assert_true (g_variant_is_of_type (serialized_sound, G_VARIANT_TYPE("(sv)")));
          g_variant_get (serialized_sound, "(&sv)", &key, &handle);
          g_assert_cmpstr (key, ==, "file-descriptor");

          fd_list = g_notification_server_get_unix_fd_list_for_notification (server, notification);
          g_assert_nonnull (fd_list);
          fd_id = g_variant_get_handle (handle);
          fd = g_unix_fd_list_get (fd_list, fd_id, &error);
          g_assert_no_error (error);
          g_assert_cmpint (fd, >, -1);

          mapped = g_mapped_file_new_from_fd (fd, FALSE, &error);
          g_assert_no_error (error);
          bytes = g_mapped_file_get_bytes (mapped);
        }
      else if (exp_sound->sound_type == SOUND_TYPE_DEFAULT)
        {
          const char *key;

          g_assert_true (g_variant_is_of_type (serialized_sound, G_VARIANT_TYPE("s")));
          key = g_variant_get_string (serialized_sound, NULL);
          g_assert_cmpstr (key, ==, "default");
        }
      else if (exp_sound->sound_type == SOUND_TYPE_CUSTOM)
        {
          /* The portal uses a button with a specific purpose for custom sound action */
        }
      else
        {
          g_assert_not_reached ();
        }

      if (exp_sound->sound_type == SOUND_TYPE_FILE)
        {
          g_autoptr(GBytes) exp_bytes = NULL;
          exp_bytes = g_file_load_bytes (exp_sound->file, NULL, NULL, &error);
          g_assert_no_error (error);
          g_assert_true (g_bytes_equal (exp_bytes, bytes));
        }
      else if (exp_sound->sound_type == SOUND_TYPE_BYTES)
        {
          g_assert_true (g_bytes_equal (exp_sound->bytes, bytes));
        }
    }
  else
    {
      const char *key;
      g_autoptr(GVariant) serialized_sound = NULL;

      serialized_sound = g_variant_lookup_value (notification, "sound", NULL);
      if (serialized_sound)
        {
          g_assert_true (g_variant_is_of_type (serialized_sound, G_VARIANT_TYPE("s")));
          key = g_variant_get_string (serialized_sound, NULL);

          g_assert_cmpstr (key, ==, "silent");
        }
    }

  if (exp_notification->priority)
  {
      g_autoptr(GEnumClass) enum_class = NULL;
      GEnumValue *enum_value;
      const gchar *priority = NULL;
      g_assert_true (g_variant_lookup (notification, "priority", "&s", &priority));

      enum_class = g_type_class_ref (G_TYPE_NOTIFICATION_PRIORITY);
      g_assert_nonnull (enum_class);
      enum_value = g_enum_get_value_by_nick (enum_class, priority);
      g_assert_nonnull (enum_value);

      g_assert_true ((GNotificationPriority) enum_value->value == exp_notification->priority);
    }

  if (exp_notification->display_hint)
    {
      g_autoptr(GFlagsClass) flags_class = NULL;
      GNotificationDisplayHintFlags display_hint = G_NOTIFICATION_DISPLAY_HINT_UPDATE;
      const gchar** flags = NULL;
      gsize i;
      g_assert_true (g_variant_lookup (notification, "display-hint", "^a&s", &flags));

      flags_class = g_type_class_ref (G_TYPE_NOTIFICATION_DISPLAY_HINT_FLAGS);
      g_assert_nonnull (flags_class);

      for (i = 0; flags[i]; i++)
        {
          GFlagsValue *flags_value;

          if (g_strcmp0 (flags[i], "show-as-new") == 0)
            {
              display_hint &= ~G_NOTIFICATION_DISPLAY_HINT_UPDATE;
              continue;
            }

          flags_value = g_flags_get_value_by_nick (flags_class, flags[i]);
          g_assert_nonnull (flags_value);
          display_hint |= flags_value->value;
        }

      g_assert_true (display_hint == exp_notification->display_hint);
    }

  if (exp_notification->category)
    {
      const gchar *category;
      g_assert_true (g_variant_lookup (notification, "category", "&s", &category));
      g_assert_cmpstr (category, ==, exp_notification->category);
    }

  if (exp_notification->default_action)
    {
      const gchar *default_action;
      g_assert_true (g_variant_lookup (notification, "default-action", "&s", &default_action));
      g_assert_cmpstr (default_action, ==, exp_notification->default_action);
    }

  if (exp_notification->default_action_target)
    {
      g_autoptr(GVariant) default_action_target = NULL;
      default_action_target = g_variant_lookup_value (notification, "default-action-target", NULL);
      g_assert_true (g_variant_equal (default_action_target, exp_notification->default_action_target));
    }

  if (exp_notification->default_action_target)
    {
      g_autoptr(GVariant) default_action_target = NULL;
      default_action_target = g_variant_lookup_value (notification, "default-action-target", NULL);
      g_assert_true (g_variant_equal (default_action_target, exp_notification->default_action_target));
    }

  // Custom sound is a special system button for the portal
  if ((exp_notification->buttons && exp_notification->buttons->len > 0) ||
      (exp_notification->sound && exp_notification->sound->sound_type == SOUND_TYPE_CUSTOM) ||
      exp_notification->response_action)
    {
      gsize i;
      g_autoptr(GVariant) buttons = NULL;
      buttons = g_variant_lookup_value (notification, "buttons", G_VARIANT_TYPE("aa{sv}"));
      g_assert_nonnull (buttons);

      for (i = 0; i < g_variant_n_children (buttons); i++)
        {
          Button *exp_button;
          g_autoptr(GVariant) button = NULL;
          const gchar *label = NULL;
          const gchar *purpose = NULL;
          const gchar *action_name = NULL;
          g_autoptr(GVariant) action_target = NULL;

          button = g_variant_get_child_value (buttons, i);
          g_assert_nonnull (button);

          if (g_variant_lookup (button, "purpose", "&s", &purpose) &&
              g_strcmp0 (purpose, "system.custom-alert") == 0)
            {
              g_assert_nonnull (exp_notification->sound);
              g_assert_false (g_variant_lookup (button, "label", "&s", &label));

              g_assert_true (g_variant_lookup (button, "action", "&s", &action_name));
              g_assert_cmpstr (action_name, ==, exp_notification->sound->custom.action);

              action_target = g_variant_lookup_value (button, "target", NULL);
              g_assert_true (g_variant_equal (action_target, exp_notification->sound->custom.target));

              continue;
            }

          if (exp_notification->response_action)
            {
              g_assert_false (g_variant_lookup (button, "label", "&s", &label));

              g_assert_true (g_variant_lookup (button, "action", "&s", &action_name));
              g_assert_cmpstr (action_name, ==, exp_notification->response_action);

              if (exp_notification->response_action_target)
                {
                  action_target = g_variant_lookup_value (button, "target", NULL);
                  g_assert_true (g_variant_equal (action_target, exp_notification->response_action_target));
                }
              else
                {
                  g_assert_null (g_variant_lookup_value (button, "target", NULL));
                }

              has_response_action = TRUE;
              continue;
            }

          exp_button = (Button*)g_ptr_array_index (exp_notification->buttons, i);
          g_assert_nonnull (exp_button);

          if (exp_button->label)
            {
              g_assert_true (g_variant_lookup (button, "label", "&s", &label));
              g_assert_cmpstr (label, ==, exp_button->label);
            }

          if (exp_button->purpose)
            {
              g_assert_true (g_variant_lookup (button, "purpose", "&s", &purpose));
              g_assert_cmpstr (purpose, ==, exp_button->purpose);
            }

          if (exp_button->action_name)
            {
              g_assert_true (g_variant_lookup (button, "action", "&s", &action_name));
              g_assert_cmpstr (action_name, ==, exp_button->action_name);
            }

          action_target = g_variant_lookup_value (button, "target", NULL);
          g_assert_true (g_variant_equal (action_target, exp_button->target));
        }
    }

  if (exp_notification->response_action)
    g_assert_true (has_response_action);

  send_and_wait_finish (exp_data);
}

static void
notification_removed (GNotificationServer *server,
                      const gchar         *app_id,
                      const gchar         *notification_id,
                      gpointer             user_data)
{
  gint *count = user_data;

  g_assert_cmpstr (notification_id, ==, "test1");

  (*count)++;
}

static void
server_notify_is_running (GObject    *object,
                          GParamSpec *pspec,
                          gpointer    user_data)
{
  GNotificationServer *server = G_NOTIFICATION_SERVER (object);
  GApplication *app;

  g_assert_true (g_notification_server_get_is_running (server));

  app = g_application_new ("org.gtk.TestApplication", G_APPLICATION_DEFAULT_FLAGS);
  g_signal_connect (app, "activate", G_CALLBACK (activate_app), user_data);

  g_application_run (app, 0, NULL);

  g_object_unref (app);
}

static void
basic (void)
{
  TestData *data;
  GNotificationServer *server;
  GMainLoop *loop;
  gint removed_count = 0;

  session_bus_up ();

  loop = g_main_loop_new (NULL, FALSE);

  data = g_new0 (TestData, 1);
  data->loop = g_main_loop_ref (loop);

  server = g_notification_server_new ("portal", 2);
  g_signal_connect (server, "notification-received", G_CALLBACK (notification_received), data);
  g_signal_connect (server, "notification-removed", G_CALLBACK (notification_removed), &removed_count);
  g_signal_connect (server, "notify::is-running", G_CALLBACK (server_notify_is_running), data);

  g_main_loop_run (loop);

  test_data_free (data);

  g_assert_cmpint (removed_count, ==, 1);

  g_object_unref (server);
  g_main_loop_unref (loop);
  session_bus_down ();
}

int main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_setenv ("GIO_USE_PORTALS", "1", TRUE);

  g_test_add_func ("/portal-notification-backend/basic", basic);

  return g_test_run ();
}
