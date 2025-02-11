/*
 * Copyright 2024 GNOME Foundation, Inc.
 * Copyright 2024 Evan Welsh
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
 * Author: Evan Welsh <contact@evanwelsh.com>
 */

#include "girepository.h"
#include "test-common.h"

static void
test_callable_get_sync_function_for_async_function (RepositoryFixture *fx,
                                                    const void        *unused)
{
  GIBaseInfo *info;

  info = gi_repository_find_by_name (fx->repository, "Gio", "File");
  g_assert_nonnull (info);
  g_assert_true (GI_IS_INTERFACE_INFO (info));

  GICallableInfo *callable_info = (GICallableInfo *) gi_interface_info_find_method ((GIInterfaceInfo *) info, "load_contents_async");
  g_assert_nonnull (callable_info);

  g_assert_true (gi_callable_info_is_async (callable_info));

  GICallableInfo *sync_info = gi_callable_info_get_sync_function (callable_info);
  g_assert_nonnull (sync_info);

  GICallableInfo *finish_info = gi_callable_info_get_finish_function (callable_info);
  g_assert_nonnull (finish_info);

  g_assert_cmpstr (gi_base_info_get_name ((GIBaseInfo *) sync_info), ==, "load_contents");
  g_assert_cmpstr (gi_base_info_get_name ((GIBaseInfo *) finish_info), ==, "load_contents_finish");

  GICallableInfo *async_info = gi_callable_info_get_async_function (sync_info);

  g_assert_cmpstr (gi_base_info_get_name ((GIBaseInfo *) async_info), ==, "load_contents_async");

  gi_base_info_unref (async_info);
  gi_base_info_unref (sync_info);
  gi_base_info_unref (finish_info);
  gi_base_info_unref (callable_info);
  gi_base_info_unref (info);
}

static void
test_callable_get_async_function_for_sync_function (RepositoryFixture *fx,
                                                    const void        *unused)
{
  GIBaseInfo *info;

  info = gi_repository_find_by_name (fx->repository, "Gio", "File");
  g_assert_nonnull (info);
  g_assert_true (g_type_is_a (G_TYPE_FROM_INSTANCE (info), gi_interface_info_get_type ()));

  GICallableInfo *callable_info = (GICallableInfo *) gi_interface_info_find_method ((GIInterfaceInfo *) info, "load_contents");

  {
    GICallableInfo *async_func = gi_callable_info_get_async_function (callable_info);
    g_assert_nonnull (async_func);

    GICallableInfo *finish_func = gi_callable_info_get_finish_function (callable_info);
    g_assert_null (finish_func);

    GICallableInfo *sync_func = gi_callable_info_get_sync_function (callable_info);
    g_assert_null (sync_func);

    gi_base_info_unref ((GIBaseInfo *) async_func);
  }

  GICallableInfo *async_info = gi_callable_info_get_async_function (callable_info);

  {
    GICallableInfo *async_func = gi_callable_info_get_async_function (async_info);
    g_assert_null (async_func);

    GICallableInfo *finish_func = gi_callable_info_get_finish_function (async_info);
    g_assert_nonnull (finish_func);

    GICallableInfo *sync_func = gi_callable_info_get_sync_function (async_info);
    g_assert_nonnull (sync_func);

    gi_base_info_unref ((GIBaseInfo *) finish_func);
    gi_base_info_unref ((GIBaseInfo *) sync_func);
  }

  g_assert_cmpstr (gi_base_info_get_name ((GIBaseInfo *) async_info), ==, "load_contents_async");

  GICallableInfo *sync_info = gi_callable_info_get_sync_function (async_info);

  {
    GICallableInfo *async_func = gi_callable_info_get_async_function (sync_info);
    g_assert_nonnull (async_func);

    GICallableInfo *finish_func = gi_callable_info_get_finish_function (sync_info);
    g_assert_null (finish_func);

    GICallableInfo *sync_func = gi_callable_info_get_sync_function (sync_info);
    g_assert_null (sync_func);

    gi_base_info_unref ((GIBaseInfo *) async_func);
  }

  g_assert_cmpstr (gi_base_info_get_name ((GIBaseInfo *) sync_info), ==, "load_contents");

  gi_base_info_unref ((GIBaseInfo *) async_info);
  gi_base_info_unref ((GIBaseInfo *) sync_info);
  gi_base_info_unref ((GIBaseInfo *) callable_info);
  gi_base_info_unref (info);
}

static void
test_callable_info_is_method (RepositoryFixture *fx,
                              const void *unused)
{
  GIBaseInfo *info;
  GIFunctionInfo *func_info;
  GIVFuncInfo *vfunc_info;
  GISignalInfo *sig_info;
  GIBaseInfo *cb_info;

  info = gi_repository_find_by_name (fx->repository, "Gio", "ActionGroup");
  g_assert_nonnull (info);

  func_info = gi_interface_info_find_method (GI_INTERFACE_INFO (info), "action_added");
  g_assert_nonnull (func_info);

  g_assert_true (gi_callable_info_is_method (GI_CALLABLE_INFO (func_info)));

  vfunc_info = gi_interface_info_find_vfunc (GI_INTERFACE_INFO (info), "action_added");
  g_assert_nonnull (vfunc_info);

  g_assert_true (gi_callable_info_is_method (GI_CALLABLE_INFO (vfunc_info)));

  sig_info = gi_interface_info_find_signal (GI_INTERFACE_INFO (info), "action-added");
  g_assert_nonnull (sig_info);

  g_assert_true (gi_callable_info_is_method (GI_CALLABLE_INFO (sig_info)));

  cb_info = gi_repository_find_by_name (fx->repository, "Gio", "AsyncReadyCallback");
  g_assert_nonnull (cb_info);

  g_assert_false (gi_callable_info_is_method (GI_CALLABLE_INFO (cb_info)));

  gi_base_info_unref (info);
  gi_base_info_unref ((GIBaseInfo *) func_info);
  gi_base_info_unref ((GIBaseInfo *) vfunc_info);
  gi_base_info_unref ((GIBaseInfo *) sig_info);
  gi_base_info_unref (cb_info);
}

static void
test_callable_info_static_method (RepositoryFixture *fx,
                                  const void *unused)
{
  GIBaseInfo *info;
  GIFunctionInfo *func_info;

  info = gi_repository_find_by_name (fx->repository, "Gio", "Application");
  g_assert_nonnull (info);

  func_info = gi_object_info_find_method (GI_OBJECT_INFO (info), "get_default");
  g_assert_nonnull (func_info);

  g_assert_false (gi_callable_info_is_method (GI_CALLABLE_INFO (func_info)));

  gi_base_info_unref (info);
  gi_base_info_unref ((GIBaseInfo *) func_info);
}

static void
test_callable_info_static_vfunc (RepositoryFixture *fx,
                                 const void *unused)
{
  GIBaseInfo *info;
  GIVFuncInfo *vfunc_info;

  g_test_bug ("https://gitlab.gnome.org/GNOME/gobject-introspection/-/merge_requests/361");

  info = gi_repository_find_by_name (fx->repository, "Gio", "Icon");
  g_assert_nonnull (info);

  vfunc_info = gi_interface_info_find_vfunc (GI_INTERFACE_INFO (info), "from_tokens");
  if (!vfunc_info)
    {
      g_test_skip ("g-ir-scanner is not new enough");
      gi_base_info_unref (info);
      return;
    }
  g_assert_nonnull (vfunc_info);

  g_assert_false (gi_callable_info_is_method (GI_CALLABLE_INFO (vfunc_info)));

  gi_base_info_unref (info);
  gi_base_info_unref ((GIBaseInfo *) vfunc_info);
}

int
main (int argc, char **argv)
{
  repository_init (&argc, &argv);

  ADD_REPOSITORY_TEST ("/callable-info/sync-function", test_callable_get_sync_function_for_async_function, &typelib_load_spec_gio);
  ADD_REPOSITORY_TEST ("/callable-info/async-function", test_callable_get_async_function_for_sync_function, &typelib_load_spec_gio);
  ADD_REPOSITORY_TEST ("/callable-info/is-method", test_callable_info_is_method, &typelib_load_spec_gio);
  ADD_REPOSITORY_TEST ("/callable-info/static-method", test_callable_info_static_method, &typelib_load_spec_gio);
  ADD_REPOSITORY_TEST ("/callable-info/static-vfunc", test_callable_info_static_vfunc, &typelib_load_spec_gio);

  return g_test_run ();
}
