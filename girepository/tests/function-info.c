/*
 * Copyright 2024 Philip Chimento
 * Copyright 2024 GNOME Foundation, Inc.
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
#include "girffi.h"

static GIRepository *
load_typelib_from_builddir (const char *name,
                            const char *version)
{
  GIRepository *repository;
  char *gobject_typelib_dir = NULL;
  GITypelib *typelib = NULL;
  GError *local_error = NULL;

  gobject_typelib_dir = g_test_build_filename (G_TEST_BUILT, "..", "..", "introspection", NULL);
  g_test_message ("Using GI_TYPELIB_DIR = %s", gobject_typelib_dir);
  gi_repository_prepend_search_path (gobject_typelib_dir);
  g_free (gobject_typelib_dir);

  repository = gi_repository_new ();
  g_assert_nonnull (repository);

  typelib = gi_repository_require (repository, name, version, 0, &local_error);
  g_assert_no_error (local_error);
  g_assert_nonnull (typelib);

  return g_steal_pointer (&repository);
}

static void
test_function_info_invoker (void)
{
  GIRepository *repository;
  GIFunctionInfo *function_info = NULL;
  GIFunctionInvoker invoker;
  GError *local_error = NULL;

  g_test_summary ("Test preparing a function invoker");

  repository = load_typelib_from_builddir ("GLib", "2.0");

  function_info = (GIFunctionInfo *) gi_repository_find_by_name (repository, "GLib", "get_locale_variants");
  g_assert_nonnull (function_info);

  gi_function_info_prep_invoker (function_info, &invoker, &local_error);
  g_assert_no_error (local_error);

  gi_function_invoker_clear (&invoker);
  g_clear_pointer (&function_info, gi_base_info_unref);

  g_clear_object (&repository);
}

int
main (int   argc,
      char *argv[])
{
  /* Isolate from the system typelibs and GIRs. */
  g_setenv ("GI_TYPELIB_PATH", "/dev/null", TRUE);
  g_setenv ("GI_GIR_PATH", "/dev/null", TRUE);

  g_test_init (&argc, &argv, G_TEST_OPTION_ISOLATE_DIRS, NULL);

  g_test_add_func ("/function-info/invoker", test_function_info_invoker);

  return g_test_run ();
}
