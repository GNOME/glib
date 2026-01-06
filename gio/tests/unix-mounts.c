/* GLib testing framework examples and tests
 *
 * Copyright © 2017 Endless Mobile, Inc.
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

#include <glib.h>

#ifndef G_OS_UNIX
#error This is a Unix-specific test
#endif

#include <errno.h>
#include <locale.h>
#include <glib/gstdio.h>
#include <gio/gio.h>
#include <gio/gunixmounts.h>

#include "../gunixmounts-private.h"

static void
test_is_system_fs_type (void)
{
  g_assert_true (g_unix_is_system_fs_type ("tmpfs"));
  g_assert_false (g_unix_is_system_fs_type ("ext4"));

  /* Check that some common network file systems aren’t considered ‘system’. */
  g_assert_false (g_unix_is_system_fs_type ("cifs"));
  g_assert_false (g_unix_is_system_fs_type ("nfs"));
  g_assert_false (g_unix_is_system_fs_type ("nfs4"));
  g_assert_false (g_unix_is_system_fs_type ("smbfs"));
}

static void
test_is_system_device_path (void)
{
  g_assert_true (g_unix_is_system_device_path ("devpts"));
  g_assert_false (g_unix_is_system_device_path ("/"));
}

static void
test_system_mount_paths_sorted (void)
{
  size_t i;
  size_t n_paths = G_N_ELEMENTS (system_mount_paths);

  g_test_summary ("Verify that system_mount_paths array is sorted for bsearch");

  for (i = 1; i < n_paths; i++)
    {
      int cmp = strcmp (system_mount_paths[i - 1], system_mount_paths[i]);
      if (cmp > 0)
        {
          g_fprintf (stderr, "system_mount_paths array is not sorted: "
                              "\"%s\" should come before \"%s\"",
                              system_mount_paths[i - 1],
                              system_mount_paths[i]);
          g_test_fail ();
          return;
        }
    }
}

int
main (int   argc,
      char *argv[])
{
  setlocale (LC_ALL, "");

  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/unix-mounts/is-system-fs-type", test_is_system_fs_type);
  g_test_add_func ("/unix-mounts/is-system-device-path", test_is_system_device_path);
  g_test_add_func ("/unix-mounts/system-mount-paths-sorted", test_system_mount_paths_sorted);

  return g_test_run ();
}
