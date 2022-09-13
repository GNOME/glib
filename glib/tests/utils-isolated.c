/* Copyright (C) 2022 Marco Trevisan
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
 * Author: Marco Trevisan <marco.trevisan@canonical.com>
 */

#include "config.h"

#include <glib/glib.h>

/* Test that all of the well-known directories returned by GLib
 * are returned as children of test_tmpdir when running with
 * %G_TEST_OPTION_ISOLATE_DIRS. This ensures that tests should
 * not interfere with each other in `/tmp` while running.
 */

const char *test_tmpdir;

static void
test_tmp_dir (void)
{
  g_assert_cmpstr (g_get_tmp_dir (), ==, test_tmpdir);
}

static void
test_home_dir (void)
{
  g_assert_true (g_str_has_prefix (g_get_home_dir (), test_tmpdir));
}

static void
test_user_cache_dir (void)
{
  g_assert_true (g_str_has_prefix (g_get_user_cache_dir (), test_tmpdir));
}

static void
test_system_config_dirs (void)
{
  const char *const *dir;

  for (dir = g_get_system_config_dirs (); *dir != NULL; dir++)
    g_assert_true (g_str_has_prefix (*dir, test_tmpdir));
}

static void
test_user_config_dir (void)
{
  g_assert_true (g_str_has_prefix (g_get_user_config_dir (), test_tmpdir));
}

static void
test_system_data_dirs (void)
{
  const char *const *dir;

  for (dir = g_get_system_data_dirs (); *dir != NULL; dir++)
    g_assert_true (g_str_has_prefix (*dir, test_tmpdir));
}

static void
test_user_data_dir (void)
{
  g_assert_true (g_str_has_prefix (g_get_user_data_dir (), test_tmpdir));
}

static void
test_user_state_dir (void)
{
  g_assert_true (g_str_has_prefix (g_get_user_state_dir (), test_tmpdir));
}

static void
test_user_runtime_dir (void)
{
  g_assert_true (g_str_has_prefix (g_get_user_runtime_dir (), test_tmpdir));
}


int
main (int   argc,
      char *argv[])
{
  g_setenv ("LC_ALL", "C", TRUE);
  g_test_init (&argc, &argv, G_TEST_OPTION_ISOLATE_DIRS, NULL);

  test_tmpdir = g_getenv ("G_TEST_TMPDIR");
  g_assert_nonnull (test_tmpdir);

  g_test_add_func ("/utils-isolated/tmp-dir", test_tmp_dir);
  g_test_add_func ("/utils-isolated/home-dir", test_home_dir);
  g_test_add_func ("/utils-isolated/user-cache-dir", test_user_cache_dir);
  g_test_add_func ("/utils-isolated/system-config-dirs", test_system_config_dirs);
  g_test_add_func ("/utils-isolated/user-config-dir", test_user_config_dir);
  g_test_add_func ("/utils-isolated/system-data-dirs", test_system_data_dirs);
  g_test_add_func ("/utils-isolated/user-data-dir", test_user_data_dir);
  g_test_add_func ("/utils-isolated/user-state-dir", test_user_state_dir);
  g_test_add_func ("/utils-isolated/user-runtime-dir", test_user_runtime_dir);
  return g_test_run ();
}
