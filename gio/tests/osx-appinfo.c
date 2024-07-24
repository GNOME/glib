/* 
 * Copyright (C) 2024 GNOME Foundation
 * Copyright (C) 2024 Arjan Molenaar
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

#include <gio/gio.h>
#include <gio/gosxappinfo.h>


static void
async_result_cb (GObject      *source_object,
                 GAsyncResult *result,
                 gpointer      user_data)
{
  GAsyncResult **result_out = user_data;

  g_assert (*result_out == NULL);
  *result_out = g_object_ref (result);
  g_main_context_wakeup (g_main_context_get_thread_default ());
}

static void
test_launch_async (void)
{
  GAppInfo *app_info;
  GAppLaunchContext *context;
  GAsyncResult *result = NULL;
  GError *error = NULL;

  app_info = g_app_info_get_default_for_uri_scheme("file");
  g_assert_nonnull (app_info);
  g_assert_true (G_IS_OSX_APP_INFO (app_info));

  context = g_app_launch_context_new ();

  g_app_info_launch_uris_async (G_APP_INFO (app_info), NULL, context, NULL, async_result_cb, &result);

  while (result == NULL)
    g_main_context_iteration (NULL, TRUE);

  g_assert_true (g_app_info_launch_uris_finish (G_APP_INFO (app_info), result, &error));
  g_assert_no_error (error);

  g_clear_error (&error);
  g_clear_object (&result);
  g_clear_object (&context);
  g_clear_object (&app_info);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, G_TEST_OPTION_ISOLATE_DIRS, NULL);

  g_test_add_func ("/osx-app-info/launch-async", test_launch_async);

  return g_test_run ();
}
