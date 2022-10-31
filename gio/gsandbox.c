/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright 2022 Canonical Ltd
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
 */

#include "config.h"

#include "gsandbox.h"

static gboolean
is_flatpak (void)
{
  return g_file_test ("/.flatpak-info", G_FILE_TEST_EXISTS);
}

static gboolean
is_snap (void)
{
  const gchar *snap_path;
  gchar *yaml_path;
  gboolean result;

  snap_path = g_getenv ("SNAP");
  if (snap_path == NULL)
    return FALSE;

  yaml_path = g_build_filename (snap_path, "meta", "snap.yaml", NULL);
  result = g_file_test (yaml_path, G_FILE_TEST_EXISTS);
  g_free (yaml_path);

  return result;
}

/*
 * glib_get_sandbox_type:
 *
 * Gets the type of sandbox this process is running inside.
 *
 * Checking for sandboxes may involve doing blocking I/O calls, but should not take
 * any significant time.
 *
 * The sandbox will not change over the lifetime of the process, so calling this
 * function once and reusing the result is valid.
 *
 * If this process is not sandboxed then @G_SANDBOX_TYPE_UNKNOWN will be returned.
 * This is because this function only detects known sandbox types in #GSandboxType.
 * It may be updated in the future if new sandboxes come into use.
 *
 * Returns: a #GSandboxType.
 */
GSandboxType
glib_get_sandbox_type (void)
{
  if (is_flatpak ())
    return G_SANDBOX_TYPE_FLATPAK;
  else if (is_snap ())
    return G_SANDBOX_TYPE_SNAP;
  else
    return G_SANDBOX_TYPE_UNKNOWN;
}
