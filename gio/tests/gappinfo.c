/* GLib testing framework examples and tests
 *
 * Copyright © 2018 Tapasweni Pathak
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
 * Author: Tapasweni Pathak <tapaswenipathak@gmail.com>
 */


/* This test g_app_info_get_supported_types_full
 */
static void
test_g_app_info_get_supported_types_full (GBoolean val)
{
    g_assert_cmpint ();
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/g_app/supported_types_full", test_g_app_info_get_supported_types_full);

  return g_test_run ();
}

