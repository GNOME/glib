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

#include "config.h"

#include "gnotificationsound-private.h"
#include "gfile.h"
#include "gbytes.h"

/**
 * GNotificationSound:
 *
 * Since: 2.82
 **/

typedef GObjectClass GNotificationSoundClass;

struct _GNotificationSound
{
  GObject parent;

  gboolean silent;
  GFile *file;
  GBytes *bytes;
};

G_DEFINE_TYPE (GNotificationSound, g_notification_sound, G_TYPE_OBJECT)

static void
g_notification_sound_dispose (GObject *object)
{
  GNotificationSound *notification = G_NOTIFICATION_SOUND (object);

  g_clear_object (&notification->file);
  g_clear_pointer (&notification->bytes, g_bytes_unref);

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
 * @file: A sound file in the format ogg/opus, ogg/vorbis or wav/pcm
 *
 * Returns: a new #GNotificationSound instance
 *
 * Since: 2.82
 */
GNotificationSound *
g_notification_sound_new_from_file (GFile *file)
{
  GNotificationSound *sound;

  g_return_val_if_fail (G_IS_FILE (file), NULL);

  sound = g_object_new (G_TYPE_NOTIFICATION_SOUND, NULL);
  sound->file = g_object_ref (file);

  return sound;
}

/**
 * g_notification_sound_new_from_bytes:
 * @bytes: #GBytes containing a sound in the format ogg/opus, ogg/vorbis or wav/pcm
 *
 * Returns: a new #GNotificationSound instance
 *
 * Since: 2.82
 */
GNotificationSound *
g_notification_sound_new_from_bytes (GBytes *bytes)
{
  GNotificationSound *sound;

  g_return_val_if_fail (bytes != NULL, NULL);

  sound = g_object_new (G_TYPE_NOTIFICATION_SOUND, NULL);
  sound->bytes = g_bytes_ref (bytes);

  return sound;
}

/**
 * g_notification_sound_set_sound_silent:
 *
 * Returns: a new #GNotificationSound instance
 *
 * Since: 2.82
 */
GNotificationSound *
g_notification_sound_new_silent ()
{
  GNotificationSound *sound;

  sound = g_object_new (G_TYPE_NOTIFICATION_SOUND, NULL);
  sound->silent = TRUE;

  return sound;
}

/*< private >
 * g_notification_sound_get_bytes:
 * @notification: a #GNotificationSound
 *
 * Returns: (nullable): (transfer none): the bytes associated with @sound
 */
GBytes *
g_notification_sound_get_bytes (GNotificationSound *sound)
{
  g_return_val_if_fail (G_IS_NOTIFICATION_SOUND (sound), NULL);

  return sound->bytes;
}

/*< private >
 * g_notification_sound_get_file:
 * @notification: a #GNotificationSound
 *
 * Returns: (nullable): (transfer none): the file associated with @sound
 */
GFile *
g_notification_sound_get_file (GNotificationSound *sound)
{
  g_return_val_if_fail (G_IS_NOTIFICATION_SOUND (sound), NULL);

  return sound->file;
}

/*< private >
 * g_notification_sound_get_silent:
 * @notification: a #GNotificationSound
 *
 * Returns: whether this @sound is silent.
 */
gboolean
g_notification_sound_get_silent (GNotificationSound *sound)
{
  g_return_val_if_fail (G_IS_NOTIFICATION_SOUND (sound), FALSE);

  return sound->silent;
}
