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
*         Julian Sparber <jsparber@gnome.org>
*/

#include "config.h"
#include "gnotificationbackend.h"

#include <sys/mman.h>
#include <fcntl.h>

#include <glib.h>
#include <glib/gstdio.h>

#include "giomodule-priv.h"
#include "gioenumtypes.h"
#include "gicon.h"
#include "gdbusconnection.h"
#include "gapplication.h"
#include "gnotification-private.h"
#include "gportalsupport.h"
#include "gtask.h"
#include "gunixfdlist.h"
#include <gio/gunixoutputstream.h>

/* This is the max size the xdg portal allows for icons, so load icons with this size when needed */
#define ICON_SIZE 512

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

typedef struct {
  char *id;
  GNotification *notification;
} CallData;

static CallData*
call_data_new (const char    *id,
               GNotification *notification)
{
  CallData *data;

  data = g_new0 (CallData, 1);
  data->id = g_strdup (id);
  data->notification = g_object_ref (notification);

  return data;
}

static void
call_data_free (gpointer user_data)
{
  CallData *data = user_data;

  g_clear_pointer (&data->id, g_free);
  g_clear_object (&data->notification);

  g_free (data);
}

typedef struct {
  GUnixFDList *fd_list;
  GVariantBuilder *builder;
  gsize parse_ref;
} ParserData;

static ParserData*
parser_data_new (const char *id)
{
  ParserData *data;

  data = g_new0 (ParserData, 1);
  data->fd_list = g_unix_fd_list_new ();
  data->builder = g_variant_builder_new (G_VARIANT_TYPE ("(sa{sv})"));

  g_variant_builder_add (data->builder, "s", id);
  g_variant_builder_open (data->builder, G_VARIANT_TYPE ("a{sv}"));

  return data;
}

static void
parser_data_hold (ParserData *data)
{
  data->parse_ref++;
}

static gboolean
parser_data_release (ParserData *data)
{
  data->parse_ref--;

  if (data->parse_ref == 0)
    g_variant_builder_close (data->builder);

  return data->parse_ref == 0;
}

static void
parser_data_free (gpointer user_data)
{
  ParserData *data = user_data;

  g_clear_object (&data->fd_list);
  g_clear_pointer (&data->builder, g_variant_builder_unref);

  g_free (data);
}

static void
splice_cb (GObject      *source_object,
           GAsyncResult *res,
           gpointer      user_data)
{
  GTask *task = G_TASK (user_data);
  GError *error = NULL;

  if (g_output_stream_splice_finish (G_OUTPUT_STREAM (source_object), res, &error) == -1)
    g_task_return_error (task, g_steal_pointer (&error));
  else
    g_task_return_boolean (task, TRUE);

  g_object_unref (task);
}

static void
icon_load_cb (GObject      *source_object,
              GAsyncResult *res,
              gpointer      user_data)
{
  GTask *task = G_TASK (user_data);
  GOutputStream *stream_out = G_OUTPUT_STREAM (g_task_get_task_data (task));
  GError *error = NULL;
  GInputStream *stream_in = NULL;

  stream_in = g_loadable_icon_load_finish (G_LOADABLE_ICON (source_object), res, NULL, &error);
  if (!stream_in)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      g_object_unref (task);
      return;
    }

  g_output_stream_splice_async (stream_out,
                                G_INPUT_STREAM (stream_in),
                                G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE,
                                g_task_get_priority (task),
                                g_task_get_cancellable (task),
                                splice_cb,
                                g_object_ref (task));

  g_object_unref (stream_in);
  g_object_unref (task);
}

static int
bytes_to_memfd (const gchar  *name,
                GBytes       *bytes,
                GError      **error)
{
  int fd = -1;
  gconstpointer bytes_data;
  gpointer shm;
  gsize bytes_len;

  fd = memfd_create (name, MFD_ALLOW_SEALING);
  if (fd == -1)
    {
      int saved_errno;

      saved_errno = errno;
      g_set_error (error,
                   G_IO_ERROR,
                   g_io_error_from_errno (saved_errno),
                   "memfd_create: %s", g_strerror (saved_errno));
      return -1;
    }

  bytes_data = g_bytes_get_data (bytes, &bytes_len);

  if (ftruncate (fd, bytes_len) == -1)
    {
      int saved_errno;

      saved_errno = errno;
      g_set_error (error,
                   G_IO_ERROR,
                   g_io_error_from_errno (saved_errno),
                   "ftruncate: %s", g_strerror (saved_errno));
      close (fd);
      return -1;
    }

  shm = mmap (NULL, bytes_len, PROT_WRITE, MAP_SHARED, fd, 0);
  if (shm == MAP_FAILED)
    {
      int saved_errno;

      saved_errno = errno;
      g_set_error (error,
                   G_IO_ERROR,
                   g_io_error_from_errno (saved_errno),
                   "mmap: %s", g_strerror (saved_errno));
      close (fd);
      return -1;
    }

  memcpy (shm, bytes_data, bytes_len);

  if (munmap (shm, bytes_len) == -1)
    {
      int saved_errno;

      saved_errno = errno;
      g_set_error (error,
                   G_IO_ERROR,
                   g_io_error_from_errno (saved_errno),
                   "munmap: %s", g_strerror (saved_errno));
      close (fd);
      return -1;
    }

  return g_steal_fd (&fd);
}

static void
serialize_icon (GIcon               *icon,
                uint                 version,
                GAsyncReadyCallback  callback,
                gpointer             user_data)
{
  GTask *task = NULL;

  task = g_task_new (NULL, NULL, callback, user_data);
  g_task_set_source_tag (task, serialize_icon);

  if (G_IS_THEMED_ICON (icon))
    {
      g_task_set_task_data (task,
                            g_icon_serialize (icon),
                            (GDestroyNotify) g_variant_unref);
      g_task_return_boolean (task, TRUE);
    }
  else if (G_IS_BYTES_ICON (icon))
    {
      if (version < 2)
        {
          g_task_set_task_data (task,
                                g_icon_serialize (icon),
                                (GDestroyNotify) g_variant_unref);
          g_task_return_boolean (task, TRUE);
        }
      else
        {
          GOutputStream *stream_out = NULL;
          GError *error = NULL;
          int fd = -1;

          fd = bytes_to_memfd ("notification-icon",
                               g_bytes_icon_get_bytes (G_BYTES_ICON (icon)),
                               &error);
          if (fd == -1)
            {
              g_task_return_error (task, g_steal_pointer (&error));
              g_object_unref (task);
              return;
            }

          stream_out = g_unix_output_stream_new (g_steal_fd (&fd), TRUE);
          g_task_set_task_data (task, g_steal_pointer (&stream_out), g_object_unref);
          g_task_return_boolean (task, TRUE);
        }
    }
  else if (G_IS_LOADABLE_ICON (icon))
    {
      GOutputStream *stream_out = NULL;

      if (version < 2)
        {
          stream_out = g_memory_output_stream_new_resizable ();
        }
      else
        {
          int fd = -1;

          fd = memfd_create ("notification-icon", MFD_ALLOW_SEALING);
          if (fd == -1)
            {
              int saved_errno;

              saved_errno = errno;
              g_task_return_new_error (task,
                                       G_IO_ERROR,
                                       g_io_error_from_errno (saved_errno),
                                       "memfd_create: %s", g_strerror (saved_errno));

              g_object_unref (task);
              return;
            }

          stream_out = g_unix_output_stream_new (g_steal_fd (&fd), TRUE);
        }

      g_task_set_task_data (task, g_steal_pointer (&stream_out), g_object_unref);

      g_loadable_icon_load_async (G_LOADABLE_ICON (icon),
                                  ICON_SIZE,
                                  NULL,
                                  icon_load_cb,
                                  g_object_ref (task));
    }
  else
    {
      g_assert_not_reached ();
    }

  g_object_unref (task);
}

static GVariant*
serialize_icon_finish (GAsyncResult  *result,
                       GUnixFDList   *fd_list,
                       GError       **error)
{
  GTask *task = G_TASK (result);
  gpointer data = g_task_get_task_data (task);
  GVariant *res = NULL;

  g_return_val_if_fail (g_task_get_source_tag (task) == serialize_icon, NULL);

  if (!g_task_propagate_boolean (task, error))
    return NULL;

  g_assert (data != NULL);

  if (G_IS_MEMORY_OUTPUT_STREAM (data))
    {
      GBytes *bytes = NULL;
      GIcon *icon = NULL;

      bytes = g_memory_output_stream_steal_as_bytes (G_MEMORY_OUTPUT_STREAM (data));
      icon = g_bytes_icon_new (g_steal_pointer (&bytes));
      res = g_icon_serialize (icon),

      g_bytes_unref (bytes);
      g_clear_object (&icon);
    }
  else if (G_IS_UNIX_OUTPUT_STREAM (data))
    {
      int fd;
      int fd_in;

      fd = g_unix_output_stream_get_fd (G_UNIX_OUTPUT_STREAM (data));
      g_assert (fd != -1);

      if (lseek (fd, 0, SEEK_SET) == -1)
        {
          int saved_errno = errno;

          g_task_return_new_error (task,
                                   G_IO_ERROR,
                                   g_io_error_from_errno (saved_errno),
                                   "lseek: %s", g_strerror (saved_errno));
          return NULL;
       }

      fd_in = g_unix_fd_list_append (fd_list, fd, error);
      if (fd_in == -1)
        return NULL;

      res = g_variant_ref_sink (g_variant_new ("(sv)", "file-descriptor", g_variant_new_handle (fd_in)));
    }
  else
    {
      res = g_variant_ref (g_task_get_task_data (task));
    }

  return res;
}

static void
serialize_icon_cb (GObject      *source_object,
                   GAsyncResult *res,
                   gpointer      user_data)
{
  GTask *task = G_TASK (user_data);
  ParserData *data = g_task_get_task_data (task);
  GError *error = NULL;
  GVariant *icon_out = NULL;

  icon_out = serialize_icon_finish (res, data->fd_list, &error);

  if (!icon_out)
    {
      g_prefix_error_literal (&error, "Failed to serialize icon: ");
      g_task_return_error (task, g_steal_pointer (&error));
      g_object_unref (task);
      return;
    }

  g_variant_builder_add (data->builder, "{sv}", "icon", icon_out);

  if (parser_data_release (data))
    g_task_return_boolean (task, TRUE);

  g_clear_pointer (&icon_out, g_variant_unref);
  g_object_unref (task);
}

static void
file_read_cb (GObject      *source_object,
              GAsyncResult *res,
              gpointer      user_data)
{
  GTask *task = G_TASK (user_data);
  GOutputStream *stream_out = G_OUTPUT_STREAM (g_task_get_task_data (task));
  GError *error = NULL;
  GFileInputStream *stream_in = NULL;

  stream_in = g_file_read_finish (G_FILE (source_object), res, &error);
  if (!stream_in)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      g_object_unref (task);
      return;
    }

  g_output_stream_splice_async (stream_out,
                                G_INPUT_STREAM (stream_in),
                                G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE,
                                g_task_get_priority (task),
                                g_task_get_cancellable (task),
                                splice_cb,
                                g_object_ref (task));

  g_object_unref (stream_in);
  g_object_unref (task);
}


static void
serialize_sound (GNotificationSound               *sound,
                 GAsyncReadyCallback               callback,
                 gpointer                          user_data)
{
  GTask *task = NULL;
  GBytes *bytes = NULL;
  GFile *file = NULL;

  task = g_task_new (NULL, NULL, callback, user_data);
  g_task_set_source_tag (task, serialize_sound);

  if (sound == NULL)
    {
      g_task_set_task_data (task,
                            g_variant_take_ref (g_variant_new_string ("silent")),
                            (GDestroyNotify) g_variant_unref);
      g_task_return_boolean (task, TRUE);
    }
  else if (g_notification_sound_is_default (sound))
    {
      g_task_set_task_data (task,
                            g_variant_take_ref (g_variant_new_string ("default")),
                            (GDestroyNotify) g_variant_unref);
      g_task_return_boolean (task, TRUE);
    }
  else if ((bytes = g_notification_sound_get_bytes (sound)))
    {
      GOutputStream *stream_out = NULL;
      GError *error = NULL;
      int fd = -1;

      fd = bytes_to_memfd ("notification-media",
                           bytes,
                           &error);
      if (fd == -1)
        {
          g_task_return_error (task, g_steal_pointer (&error));
          g_object_unref (task);
          return;
        }

      stream_out = g_unix_output_stream_new (g_steal_fd (&fd), TRUE);
      g_task_set_task_data (task, g_steal_pointer (&stream_out), g_object_unref);
      g_task_return_boolean (task, TRUE);
    }
  else if ((file = g_notification_sound_get_file (sound)))
    {
      GOutputStream *stream_out = NULL;
      int fd = -1;

      fd = memfd_create ("notification-sound", MFD_ALLOW_SEALING);
      if (fd == -1)
        {
          int saved_errno;

          saved_errno = errno;
          g_task_return_new_error (task,
                                   G_IO_ERROR,
                                   g_io_error_from_errno (saved_errno),
                                   "memfd_create: %s", g_strerror (saved_errno));

          g_object_unref (task);
          return;
        }

      stream_out = g_unix_output_stream_new (g_steal_fd (&fd), TRUE);
      g_task_set_task_data (task, g_steal_pointer (&stream_out), g_object_unref);

      g_file_read_async (file,
                         g_task_get_priority (task),
                         NULL,
                         file_read_cb,
                         g_object_ref (task));
    }
  else
    {
      g_assert_not_reached ();
    }

  g_object_unref (task);
}

static GVariant*
serialize_sound_finish (GAsyncResult  *result,
                        GUnixFDList   *fd_list,
                        GError       **error)
{
  GTask *task = G_TASK (result);
  gpointer data;

  g_return_val_if_fail (g_task_get_source_tag (task) == serialize_sound, NULL);

  if (!g_task_propagate_boolean (G_TASK (result), error))
    return NULL;

  data = g_task_get_task_data (task);
  g_assert (data != NULL);

  if (G_IS_UNIX_OUTPUT_STREAM (data))
    {
      int fd;
      int fd_in;

      fd = g_unix_output_stream_get_fd (G_UNIX_OUTPUT_STREAM (data));
      if (lseek (fd, 0, SEEK_SET) == -1)
        {
          int saved_errno = errno;

          g_task_return_new_error (task,
                                   G_IO_ERROR,
                                   g_io_error_from_errno (saved_errno),
                                   "lseek: %s", g_strerror (saved_errno));
          return NULL;
        }

      fd_in = g_unix_fd_list_append (fd_list, fd, error);
      if (fd_in == -1)
        return NULL;

      return g_variant_ref_sink (g_variant_new ("(sv)", "file-descriptor", g_variant_new_handle (fd_in)));
    }
  else
    {
      g_assert (data != NULL);
      return g_variant_ref (data);
    }
}

static void
serialize_sound_cb (GObject      *source_object,
                    GAsyncResult *res,
                    gpointer      user_data)
{
  GTask *task = G_TASK (user_data);
  ParserData *data = g_task_get_task_data (task);
  GError *error = NULL;
  GVariant *sound_out = NULL;

  sound_out = serialize_sound_finish (res, data->fd_list, &error);

  if (!sound_out)
    {
      g_prefix_error_literal (&error, "Failed to serialize sound: ");
      g_task_return_error (task, g_steal_pointer (&error));
      g_object_unref (task);
      return;
    }

  g_variant_builder_add (data->builder, "{sv}", "sound", sound_out);

  if (parser_data_release (data))
    g_task_return_boolean (task, TRUE);

  g_clear_pointer (&sound_out, g_variant_unref);
  g_object_unref (task);
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
serialize_display_hint (GNotification *notification)
{
  GFlagsClass *flags_class = NULL;
  GFlagsValue *flags_value;
  GVariantBuilder builder;
  GNotificationDisplayHintFlags display_hint;
  gboolean should_show_as_new = TRUE;

  display_hint = g_notification_get_display_hint_flags (notification);

  /* If the only flag is to update the notification we don't need to set any display hints */
  if (display_hint == G_NOTIFICATION_DISPLAY_HINT_UPDATE)
    return NULL;

  flags_class = g_type_class_ref (G_TYPE_NOTIFICATION_DISPLAY_HINT_FLAGS);

  g_variant_builder_init (&builder, G_VARIANT_TYPE ("as"));

  while (display_hint != G_NOTIFICATION_DISPLAY_HINT_NONE &&
         (flags_value = g_flags_get_first_value (flags_class, display_hint)) != NULL)
     {
      /* The display-hint 'update' needs to be serialized as 'show-as-new'
       * because we have the opposite default than the portal */
      if (flags_value->value == G_NOTIFICATION_DISPLAY_HINT_UPDATE)
        should_show_as_new = FALSE;
       else
        g_variant_builder_add (&builder, "s", flags_value->value_nick);
      display_hint &= ~flags_value->value;
    }

  if (should_show_as_new)
    g_variant_builder_add (&builder, "s", "show-as-new");

  g_type_class_unref (flags_class);
  return g_variant_builder_end (&builder);
}

static void
serialize_notification (const char          *id,
                        GNotification       *notification,
                        uint                 version,
                        GAsyncReadyCallback  callback,
                        gpointer             user_data)
{
  ParserData *data;
  GTask *task;
  const gchar *body;
  const gchar *markup_body;
  GIcon *icon;
  GVariant *display_hint = NULL;
  gchar *default_action = NULL;
  GVariant *default_action_target = NULL;
  GVariant *buttons = NULL;
  GNotificationSound *sound = NULL;

  task = g_task_new (NULL, NULL, callback, user_data);
  g_task_set_source_tag (task, serialize_notification);

  data = parser_data_new (id);
  g_task_set_task_data (task,
                        data,
                        parser_data_free);
  parser_data_hold (data);

  g_variant_builder_add (data->builder, "{sv}", "title", g_variant_new_string (g_notification_get_title (notification)));

  /* Prefer the body with markup over the body */
  if (version > 1 && (markup_body = g_notification_get_body_with_markup (notification)))
    g_variant_builder_add (data->builder, "{sv}", "markup-body", g_variant_new_string (markup_body));
  else if ((body = g_notification_get_body (notification)))
    g_variant_builder_add (data->builder, "{sv}", "body", g_variant_new_string (body));

  if ((icon = g_notification_get_icon (notification)))
    {
      if (G_IS_THEMED_ICON (icon) || G_IS_BYTES_ICON (icon) || G_IS_LOADABLE_ICON (icon))
        {
          parser_data_hold (data);
          serialize_icon (icon,
                          version,
                          serialize_icon_cb,
                          g_object_ref (task));
        }
      else
        {
          g_warning ("Can’t add icon to portal notification: %s isn’t handled", g_type_name_from_instance ((GTypeInstance *)icon));
        }
    }

  sound = g_notification_get_sound (notification);
  /* For the portal a custom sound is considered a button */
  if (version > 1 && !(sound && g_notification_sound_get_custom (sound, NULL, NULL)))
    {
      parser_data_hold (data);
      serialize_sound (sound,
                       serialize_sound_cb,
                       g_object_ref (task));
    }

  g_variant_builder_add (data->builder, "{sv}", "priority", serialize_priority (notification));

  if ((display_hint = serialize_display_hint (notification)))
    g_variant_builder_add (data->builder, "{sv}", "display-hint", display_hint);

  if (g_notification_get_default_action (notification, &default_action, &default_action_target))
    {
      g_variant_builder_add (data->builder, "{sv}", "default-action",
                                               g_variant_new_take_string (g_steal_pointer (&default_action)));

      if (default_action_target)
        {
          g_variant_builder_add (data->builder, "{sv}", "default-action-target",
                                                    default_action_target);
          g_clear_pointer (&default_action_target, g_variant_unref);
        }
    }

  if ((buttons = serialize_buttons (notification)))
    g_variant_builder_add (data->builder, "{sv}", "buttons", buttons);

  if (parser_data_release (data))
    g_task_return_boolean (task, TRUE);

  g_clear_object (&task);
}

static GVariant *
serialize_notification_finish (GAsyncResult  *result,
                               GUnixFDList  **fd_list,
                               GError       **error)
{
  ParserData *data;
  g_return_val_if_fail (g_task_get_source_tag (G_TASK (result)) == serialize_notification, NULL);

  if (!g_task_propagate_boolean (G_TASK (result), error))
    return NULL;

  data = g_task_get_task_data (G_TASK (result));

  if (fd_list)
    *fd_list = g_steal_pointer (&data->fd_list);

  return g_variant_ref_sink (g_variant_builder_end (g_steal_pointer (&data->builder)));
}

static void
serialize_notification_cb (GObject      *source_object,
                           GAsyncResult *result,
                           gpointer      user_data)
{
  GNotificationBackend *backend = G_NOTIFICATION_BACKEND (user_data);
  GError *error = NULL;
  GUnixFDList *fd_list = NULL;
  GVariant *parameters = NULL;

  parameters = serialize_notification_finish (result, &fd_list, &error);

  if (!parameters)
    {
        g_warning ("Failed to send notification: %s", error->message);
        g_error_free (error);
        return;
    }

  g_dbus_connection_call_with_unix_fd_list (backend->dbus_connection,
                                            "org.freedesktop.portal.Desktop",
                                            "/org/freedesktop/portal/desktop",
                                            "org.freedesktop.portal.Notification",
                                            "AddNotification",
                                            parameters,
                                            NULL,
                                            G_DBUS_CALL_FLAGS_NONE,
                                            -1,
                                            fd_list,
                                            NULL,
                                            NULL,
                                            NULL);

  g_object_unref (backend);
  g_object_unref (fd_list);
  g_variant_unref (parameters);
}

static void
get_supported_features_cb (GObject      *source_object,
                           GAsyncResult *result,
                           gpointer      user_data)
{
  GPortalNotificationBackend *backend = G_PORTAL_NOTIFICATION_BACKEND (source_object);
  CallData *data = user_data;
  GError *error = NULL;

  if (!get_supported_features_finish (backend, result, &error))
    {
      g_warning ("Failed to get notification portal version: %s", error->message);
      g_clear_pointer (&error, g_error_free);
      call_data_free (data);
      return;
    }

  serialize_notification (data->id,
                          data->notification,
                          backend->version,
                          serialize_notification_cb,
                          g_object_ref (backend));

  call_data_free (data);
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
  get_supported_features (G_PORTAL_NOTIFICATION_BACKEND (backend),
                          get_supported_features_cb,
                          call_data_new (id, notification));
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
