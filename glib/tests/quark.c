/*
 * Copyright 2011, 2014, 2024 Red Hat, Inc.
 * Copyright 2022 Peter Bloomfield
 * Copyright 2026 Philip Withnall
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
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "glib.h"

static void
test_quark_basic (void)
{
  GQuark quark;
  const gchar *orig = "blargh";
  gchar *copy;
  const gchar *str;

  quark = g_quark_try_string ("no-such-quark");
  g_assert_cmpuint (quark, ==, 0);

  copy = g_strdup (orig);
  quark = g_quark_from_static_string (orig);
  g_assert_cmpuint (quark, !=, 0);
  g_assert_cmpuint (g_quark_from_string (orig), ==, quark);
  g_assert_cmpuint (g_quark_from_string (copy), ==, quark);
  g_assert_cmpuint (g_quark_try_string (orig), ==, quark);

  str = g_quark_to_string (quark);
  g_assert_cmpstr (str, ==, orig);

  g_free (copy);
}

static void
test_quark_string (void)
{
  const gchar *orig = "string1";
  gchar *copy;
  const gchar *str1;
  const gchar *str2;

  copy = g_strdup (orig);

  str1 = g_intern_static_string (orig);
  str2 = g_intern_string (copy);
  g_assert_cmpstr (str1, ==, str2);
  g_assert_true (str1 == str2);  /* also compare pointers */
  g_assert_cmpstr (str1, ==, orig);
  g_assert_true (str1 == orig);

  g_free (copy);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/quark/basic", test_quark_basic);
  g_test_add_func ("/quark/string", test_quark_string);

  return g_test_run ();
}
