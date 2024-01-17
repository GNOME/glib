/*
 * Copyright 2023 GNOME Foundation, Inc.
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
 * Author: Philip Withnall <pwithnall@gnome.org>
 */

#include "glib.h"
#include "girepository.h"

static void
test_repository_basic (void)
{
  GIRepository *repository;
  char *gobject_typelib_dir = NULL;
  const char * const * search_paths;
  GITypelib *typelib = NULL;
  char **namespaces = NULL;
  const char *expected_namespaces[] = { "GLib", NULL };
  GError *local_error = NULL;
  char **versions;
  size_t n_versions;

  g_test_summary ("Test basic opening of a repository and requiring a typelib");

  gobject_typelib_dir = g_test_build_filename (G_TEST_BUILT, "..", "..", "introspection", NULL);
  g_test_message ("Using GI_TYPELIB_DIR = %s", gobject_typelib_dir);
  gi_repository_prepend_search_path (gobject_typelib_dir);

  repository = gi_repository_new ();
  g_assert_nonnull (repository);

  versions = gi_repository_enumerate_versions (repository, "SomeInvalidNamespace", &n_versions);
  g_assert_nonnull (versions);
  g_assert_cmpstrv (versions, ((char *[]){NULL}));
  g_assert_cmpuint (n_versions, ==, 0);
  g_clear_pointer (&versions, g_strfreev);

  versions = gi_repository_enumerate_versions (repository, "GLib", NULL);
  g_assert_nonnull (versions);
  g_assert_cmpstrv (versions, ((char *[]){"2.0", NULL}));
  g_clear_pointer (&versions, g_strfreev);

  search_paths = gi_repository_get_search_path (NULL);
  g_assert_nonnull (search_paths);
  g_assert_cmpuint (g_strv_length ((char **) search_paths), >, 0);
  g_assert_cmpstr (search_paths[0], ==, gobject_typelib_dir);

  typelib = gi_repository_require (repository, "GLib", "2.0", 0, &local_error);
  g_assert_no_error (local_error);
  g_assert_nonnull (typelib);

  namespaces = gi_repository_get_loaded_namespaces (repository);
  g_assert_cmpstrv (namespaces, expected_namespaces);
  g_strfreev (namespaces);

  g_free (gobject_typelib_dir);
  g_clear_object (&repository);
}

static void
test_repository_info (void)
{
  GIRepository *repository;
  char *gobject_typelib_dir = NULL;
  GITypelib *typelib = NULL;
  GIObjectInfo *object_info = NULL;
  GISignalInfo *signal_info = NULL;
  GIFunctionInfo *method_info = NULL;
  GError *local_error = NULL;

  g_test_summary ("Test retrieving some basic info blobs from a typelib");

  gobject_typelib_dir = g_test_build_filename (G_TEST_BUILT, "..", "..", "introspection", NULL);
  g_test_message ("Using GI_TYPELIB_DIR = %s", gobject_typelib_dir);
  gi_repository_prepend_search_path (gobject_typelib_dir);
  g_free (gobject_typelib_dir);

  repository = gi_repository_new ();
  g_assert_nonnull (repository);

  typelib = gi_repository_require (repository, "GObject", "2.0", 0, &local_error);
  g_assert_no_error (local_error);
  g_assert_nonnull (typelib);

  object_info = (GIObjectInfo *) gi_repository_find_by_name (repository, "GObject", "Object");
  g_assert_nonnull (object_info);

  g_assert_cmpint (gi_base_info_get_info_type ((GIBaseInfo *) object_info), ==, GI_INFO_TYPE_OBJECT);
  g_assert_cmpstr (gi_base_info_get_name ((GIBaseInfo *) object_info), ==, "Object");
  g_assert_cmpstr (gi_base_info_get_namespace ((GIBaseInfo *) object_info), ==, "GObject");

  signal_info = gi_object_info_find_signal (object_info, "notify");
  g_assert_nonnull (signal_info);

  g_assert_cmpint (gi_signal_info_get_flags (signal_info), ==,
                   G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE | G_SIGNAL_DETAILED | G_SIGNAL_NO_HOOKS | G_SIGNAL_ACTION);

  g_assert_cmpuint (gi_object_info_get_n_methods (object_info), >, 2);

  method_info = gi_object_info_find_method (object_info, "get_property");
  g_assert_nonnull (method_info);
  g_assert_true (gi_callable_info_is_method ((GICallableInfo *) method_info));
  g_assert_cmpuint (gi_callable_info_get_n_args ((GICallableInfo *) method_info), ==, 2);
  g_clear_pointer (&method_info, gi_base_info_unref);

  method_info = gi_object_info_get_method (object_info,
                                           gi_object_info_get_n_methods (object_info) - 1);
  g_assert_true (gi_callable_info_is_method ((GICallableInfo *) method_info));
  g_assert_cmpuint (gi_callable_info_get_n_args ((GICallableInfo *) method_info), >, 0);
  g_clear_pointer (&method_info, gi_base_info_unref);

  gi_base_info_unref (signal_info);
  gi_base_info_unref (object_info);
  g_clear_object (&repository);
}

static void
test_repository_dependencies (void)
{
  GIRepository *repository;
  GITypelib *typelib;
  GError *error = NULL;
  char *gobject_typelib_dir = NULL;
  char **dependencies;

  g_test_summary ("Test ensures namespace dependencies are correctly exposed");

  gobject_typelib_dir = g_test_build_filename (G_TEST_BUILT, "..", "..", "gobject", NULL);
  g_test_message ("Using GI_TYPELIB_DIR = %s", gobject_typelib_dir);
  gi_repository_prepend_search_path (gobject_typelib_dir);
  g_free (gobject_typelib_dir);

  repository = gi_repository_new ();
  g_assert_nonnull (repository);

  typelib = gi_repository_require (repository, "GObject", "2.0", 0, &error);
  g_assert_no_error (error);
  g_assert_nonnull (typelib);

  dependencies = gi_repository_get_dependencies (repository, "GObject");
  g_assert_cmpuint (g_strv_length (dependencies), ==, 1);
  g_assert_true (g_strv_contains ((const char **) dependencies, "GLib-2.0"));

  g_clear_error (&error);
  g_clear_object (&repository);
  g_clear_pointer (&dependencies, g_strfreev);
}

int
main (int   argc,
      char *argv[])
{
  /* Isolate from the system typelibs and GIRs. */
  g_setenv ("GI_TYPELIB_PATH", "/dev/null", TRUE);
  g_setenv ("GI_GIR_PATH", "/dev/null", TRUE);

  g_test_init (&argc, &argv, G_TEST_OPTION_ISOLATE_DIRS, NULL);

  g_test_add_func ("/repository/basic", test_repository_basic);
  g_test_add_func ("/repository/info", test_repository_info);
  g_test_add_func ("/repository/dependencies", test_repository_dependencies);

  return g_test_run ();
}
