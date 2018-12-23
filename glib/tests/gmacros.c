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

/* Test G_ALIGNAS() gives correct results on compiler GCC, C11 and MSVC compiler and
 * returns prama info message otherwise.
 *
 * This is necessary because the implementation of G_ALIGNAS() varies depending
 * on the compiler in use. As MSVC only accepts integer value and `ALIGNAS` won't
 * work if applied on type. We want all implementations to be consistent.
 *
 */

static void
test_alignas (void)
{
  #if defined(_MSC_VER)
    check_alignas (int alignas (8) a);
    g_assert_cmpint ( int alignas (8) a, == ,8 );
  #elif defined(STDC_VERSION__) && \
  (__STDC_VERSION__ >= 201112L) || !defined(_STRICT_ANSI__) && \
  !defined(__cplusplus) || defined(__GNUC__) || defined(_MSC_VER)
    check_alignas (struct { char a; int alignas (8) b; });
    g_assert_cmpint ( G_STRUCT_OFFSET struct s { int a; int alignas (8) bar; }, == ,8 );
  #else
    g_test_skip ("ALIGNAS is not supported by the compiler.")
  #endif
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/alignas/implementaion", test_alignas);

  return g_test_run ();
}

