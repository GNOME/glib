/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright 2016 Red Hat, Inc.
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
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <unistd.h>

#include "gportalsupport.h"

gboolean
glib_should_use_portal (void)
{
  const char *use_portal;
  char *path;

  path = g_strdup_printf ("%s/flatpak-info", g_get_user_runtime_dir ());
  if (g_file_test (path, G_FILE_TEST_EXISTS))
    use_portal = "1";
  else
    {
      use_portal = g_getenv ("GTK_USE_PORTAL");
      if (!use_portal)
        use_portal = "";
    }
  g_free (path);

  return g_str_equal (use_portal, "1");
}

gboolean
glib_network_available_in_sandbox (void)
{
  char *path;
  GKeyFile *keyfile;
  gboolean available = TRUE;

  path = g_strdup_printf ("%s/flatpak-info", g_get_user_runtime_dir ());

  keyfile = g_key_file_new ();
  if (g_key_file_load_from_file (keyfile, path, G_KEY_FILE_NONE, NULL))
    {
      char **shared = NULL;

      shared = g_key_file_get_string_list (keyfile, "Context", "shared", NULL, NULL);
      if (shared)
        {
          available = g_strv_contains ((const char * const *)shared, "network");
          g_strfreev (shared);
        }
    }

  g_key_file_free (keyfile);
  g_free (path);

  return available;
}

