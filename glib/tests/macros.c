/* GLib testing framework examples and tests
 *
 * Copyright © 2018 Endless Mobile, Inc.
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
 * Author: Philip Withnall <withnall@endlessm.com>
 */

#include <glib.h>

/* Test that G_STATIC_ASSERT_EXPR can be used as an expression */
static void
test_assert_static (void)
{
  G_STATIC_ASSERT (4 == 4);
  if (G_STATIC_ASSERT_EXPR (1 == 1), sizeof (gchar) == 2)
    g_assert_not_reached ();
}

/* Test G_ALIGNOF() gives the same results as the G_STRUCT_OFFSET fallback. This
 * should be the minimal alignment for the given type.
 *
 * This is necessary because the implementation of G_ALIGNOF() varies depending
 * on the compiler in use. We want all implementations to be consistent.
 *
 * In the case that the compiler uses the G_STRUCT_OFFSET fallback, this test
 * is a no-op. */
static void
test_alignof_fallback (void)
{
#define check_alignof(type) \
  g_assert_cmpint (G_ALIGNOF (type), ==, G_STRUCT_OFFSET (struct { char a; type b; }, b))

  check_alignof (char);
  check_alignof (int);
  check_alignof (float);
  check_alignof (double);
  check_alignof (struct { char a; int b; });
}

static void
test_struct_sizeof_member (void)
{
    G_STATIC_ASSERT (G_SIZEOF_MEMBER (struct { char a; int b; }, a) == sizeof (char));
    g_assert_cmpint (G_SIZEOF_MEMBER (struct { char a; int b; }, b), ==, sizeof (int));
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/alignof/fallback", test_alignof_fallback);
  g_test_add_func ("/assert/static", test_assert_static);
  g_test_add_func ("/struct/sizeof_member", test_struct_sizeof_member);

  return g_test_run ();
}
