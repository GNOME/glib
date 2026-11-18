/*
 * Copyright 2026 Philip Withnall
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
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "fuzz.h"
#include "gio/gdesktopappinfo.h"

int
LLVMFuzzerTestOneInput (const unsigned char *data, size_t size)
{
  GKeyFile *key_file = NULL;
  GAppInfo *app_info = NULL;
  GList *files = NULL;  /* (element-type GFile) */

  /* Ensure that any launched children are wrapped in /bin/true so they are no-ops,
   * and that D-Bus activation isn’t possible. */
  g_setenv ("GIO_LAUNCH_DESKTOP", "/bin/true", TRUE);
  g_setenv ("DBUS_SESSION_BUS_ADDRESS", "/dev/null", TRUE);

  fuzz_set_logging_func ();

  key_file = g_key_file_new ();
  if (!g_key_file_load_from_data (key_file, (const char *) data, size, G_KEY_FILE_NONE, NULL))
    {
      g_key_file_unref (key_file);
      return 0;
    }

  app_info = G_APP_INFO (g_desktop_app_info_new_from_keyfile (key_file));
  g_key_file_unref (key_file);

  if (app_info == NULL)
    return 0;

  /* Try launching the appinfo, to test parsing of the Exec= key. */
  for (unsigned int i = 0; i < 10; i++)
    {
      char *path = g_strdup_printf ("/some/path%u", i);
      GFile *file = g_file_new_for_path (path);
      files = g_list_prepend (files, g_steal_pointer (&file));
      g_free (path);
    }

  g_app_info_launch (app_info, files, NULL, NULL);

  g_clear_list (&files, (GDestroyNotify) g_object_unref);
  g_clear_object (&app_info);

  return 0;
}
