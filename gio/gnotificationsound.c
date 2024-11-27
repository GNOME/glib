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

#include "gnotificationsound-private.h"
#include "gaction.h"
#include "gfile.h"
#include "gbytes.h"

/**
 * GNotificationSound:
 *
 * [class@Gio.NotificationSound] holds the sound that should be played when a notification
 * is displayed. Use [method@Gio.Notification.set_sound] to set it for a notification.
 *
 * Since: 2.85
 **/

typedef GObjectClass GNotificationSoundClass;

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

G_DEFINE_TYPE (GNotificationSound, g_notification_sound, G_TYPE_OBJECT)

static void
g_notification_sound_dispose (GObject *object)
{
  GNotificationSound *sound = G_NOTIFICATION_SOUND (object);

  if (sound->sound_type == SOUND_TYPE_FILE)
    {
      g_clear_object (&sound->file);
    }
  else if (sound->sound_type == SOUND_TYPE_BYTES)
    {
      g_clear_pointer (&sound->bytes, g_bytes_unref);
    }
  else if (sound->sound_type == SOUND_TYPE_CUSTOM)
    {
      g_clear_pointer (&sound->custom.action, g_free);
      g_clear_pointer (&sound->custom.target, g_variant_unref);
    }

  G_OBJECT_CLASS (g_notification_sound_parent_class)->dispose (object);
}

static void
g_notification_sound_class_init (GNotificationSoundClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = g_notification_sound_dispose;
}

static void
g_notification_sound_init (GNotificationSound *notification)
{
}

/**
 * g_notification_sound_new_from_file:
 * @file: A [iface@Gio.File] containing a sound in a common audio format
 *
 * [class@Gio.Notification] using this [class@Gio.NotificationSound] will play the sound in @file
 * when displayed.
 *
 * The sound format `ogg/opus`, `ogg/vorbis` and `wav/pcm` are guaranteed to be supported.
 * Other audio formats may be supported in future.
 *
 * Returns: a new [class@Gio.NotificationSound] instance
 *
 * Since: 2.85
 */
GNotificationSound *
g_notification_sound_new_from_file (GFile *file)
{
  GNotificationSound *sound;

  g_return_val_if_fail (G_IS_FILE (file), NULL);

  sound = g_object_new (G_TYPE_NOTIFICATION_SOUND, NULL);
  sound->file = g_object_ref (file);
  sound->sound_type = SOUND_TYPE_FILE;

  return sound;
}

/**
 * g_notification_sound_new_from_bytes:
 * @bytes: [struct@GLib.Bytes] containing a sound in common audio format
 *
 * [class@Gio.Notification] using this [class@Gio.NotificationSound] will play the sound in @bytes
 * when displayed.
 *
 * The sound format `ogg/opus`, `ogg/vorbis` and `wav/pcm` are guaranteed to be supported.
 * Other audio formats may be supported in future.
 *
 * Returns: a new [class@Gio.NotificationSound] instance
 *
 * Since: 2.85
 */
GNotificationSound *
g_notification_sound_new_from_bytes (GBytes *bytes)
{
  GNotificationSound *sound;

  g_return_val_if_fail (bytes != NULL, NULL);

  sound = g_object_new (G_TYPE_NOTIFICATION_SOUND, NULL);
  sound->bytes = g_bytes_ref (bytes);
  sound->sound_type = SOUND_TYPE_BYTES;

  return sound;
}

/**
 * g_notification_sound_new_default:
 *
 * [class@Gio.Notification] using this [class@Gio.NotificationSound] will play the default sound when displayed.
 *
 * Returns: a new [class@Gio.NotificationSound] instance.
 *
 * Since: 2.85
 */
GNotificationSound *
g_notification_sound_new_default (void)
{
  GNotificationSound *sound;

  sound = g_object_new (G_TYPE_NOTIFICATION_SOUND, NULL);
  sound->sound_type = SOUND_TYPE_DEFAULT;

  return sound;
}

/**
 * g_notification_sound_new_custom:
 *
 * [class@Gio.Notification] using this [class@Gio.NotificationSound]
 * will call @action with @target when the notification is presented to the user
 * and the app should play a sound immediately.
 *
 * Returns: a new [class@Gio.NotificationSound] instance.
 *
 * Since: 2.85
 */
GNotificationSound *
g_notification_sound_new_custom (const char *action,
                                 GVariant   *target)
{
  GNotificationSound *sound;
  g_return_val_if_fail (action != NULL && g_action_name_is_valid (action), NULL);

  if (!g_str_has_prefix (action, "app."))
    {
      g_warning ("%s: action '%s' does not start with 'app.'."
                 "This is unlikely to work properly.", G_STRFUNC, action);
    }

  sound = g_object_new (G_TYPE_NOTIFICATION_SOUND, NULL);
  sound->sound_type = SOUND_TYPE_CUSTOM;
  g_set_str (&sound->custom.action, action);
  g_clear_pointer (&sound->custom.target, g_variant_unref);
  sound->custom.target = g_variant_ref_sink (target);

  return sound;
}

/*< private >
 * g_notification_sound_get_bytes:
 * @sound: a [class@Gio.NotificationSound]
 *
 * Returns: (nullable): (transfer none): the bytes associated with @sound
 */
GBytes *
g_notification_sound_get_bytes (GNotificationSound *sound)
{
  g_return_val_if_fail (G_IS_NOTIFICATION_SOUND (sound), NULL);

  if (sound->sound_type == SOUND_TYPE_BYTES)
    return sound->bytes;
  else
    return NULL;
}

/*< private >
 * g_notification_sound_get_file:
 * @sound: a [class@Gio.NotificationSound]
 *
 * Returns: (nullable): (transfer none): the [iface@Gio.File] associated with @sound
 */
GFile *
g_notification_sound_get_file (GNotificationSound *sound)
{
  g_return_val_if_fail (G_IS_NOTIFICATION_SOUND (sound), NULL);

  if (sound->sound_type == SOUND_TYPE_FILE)
    return sound->file;
  else
    return NULL;
}

/*< private >
 * g_notification_sound_is_default:
 * @sound: a [class@Gio.NotificationSound]
 *
 * Returns: whether this @sound uses should play the default sound.
 */
gboolean
g_notification_sound_is_default (GNotificationSound *sound)
{
  g_return_val_if_fail (G_IS_NOTIFICATION_SOUND (sound), FALSE);

  return sound->sound_type == SOUND_TYPE_DEFAULT;
}

/*< private >
 * g_notification_sound_get_custom:
 * @sound: a [class@Gio.NotificationSound]
 *
 * Returns: whether this @sound uses a custom action that is called to play a sound.
 */
gboolean
g_notification_sound_get_custom (GNotificationSound  *sound,
                                 gchar              **action,
                                 GVariant           **target)
{
  g_return_val_if_fail (G_IS_NOTIFICATION_SOUND (sound), FALSE);

  if (sound->sound_type == SOUND_TYPE_CUSTOM)
    {
      if (action)
        *action = g_strdup (sound->custom.action);

      if (target)
        *target = sound->custom.target ? g_variant_ref (sound->custom.target) : NULL;

      return TRUE;
    }

  return FALSE;
}
