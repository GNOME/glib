/* Unit tests for utilities
 * Copyright (C) 2010 Red Hat, Inc.
 *
 * This work is provided "as is"; redistribution and modification
 * in whole or in part, in any medium, physical or electronic is
 * permitted without restriction.
 *
 * This work is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * In no event shall the authors or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even
 * if advised of the possibility of such damage.
 *
 * Author: Matthias Clasen
 */

#include "glib.h"

static void
test_utf8_strlen (void)
{
  const gchar *string = "\xe2\x82\xa0gh\xe2\x82\xa4jl";

  g_assert_cmpint (g_utf8_strlen (string, -1), ==, 6);
  g_assert_cmpint (g_utf8_strlen (string, 0), ==, 0);
  g_assert_cmpint (g_utf8_strlen (string, 1), ==, 0);
  g_assert_cmpint (g_utf8_strlen (string, 2), ==, 0);
  g_assert_cmpint (g_utf8_strlen (string, 3), ==, 1);
  g_assert_cmpint (g_utf8_strlen (string, 4), ==, 2);
  g_assert_cmpint (g_utf8_strlen (string, 5), ==, 3);
  g_assert_cmpint (g_utf8_strlen (string, 6), ==, 3);
  g_assert_cmpint (g_utf8_strlen (string, 7), ==, 3);
  g_assert_cmpint (g_utf8_strlen (string, 8), ==, 4);
  g_assert_cmpint (g_utf8_strlen (string, 9), ==, 5);
  g_assert_cmpint (g_utf8_strlen (string, 10), ==, 6);
}

static void
test_utf8_strncpy (void)
{
  const gchar *string = "\xe2\x82\xa0gh\xe2\x82\xa4jl";
  gchar dest[20];

  g_utf8_strncpy (dest, string, 0);
  g_assert_cmpstr (dest, ==, "");

  g_utf8_strncpy (dest, string, 1);
  g_assert_cmpstr (dest, ==, "\xe2\x82\xa0");

  g_utf8_strncpy (dest, string, 2);
  g_assert_cmpstr (dest, ==, "\xe2\x82\xa0g");

  g_utf8_strncpy (dest, string, 3);
  g_assert_cmpstr (dest, ==, "\xe2\x82\xa0gh");

  g_utf8_strncpy (dest, string, 4);
  g_assert_cmpstr (dest, ==, "\xe2\x82\xa0gh\xe2\x82\xa4");

  g_utf8_strncpy (dest, string, 5);
  g_assert_cmpstr (dest, ==, "\xe2\x82\xa0gh\xe2\x82\xa4j");

  g_utf8_strncpy (dest, string, 6);
  g_assert_cmpstr (dest, ==, "\xe2\x82\xa0gh\xe2\x82\xa4jl");

  g_utf8_strncpy (dest, string, 20);
  g_assert_cmpstr (dest, ==, "\xe2\x82\xa0gh\xe2\x82\xa4jl");
}

static void
test_utf8_strrchr (void)
{
  const gchar *string = "\xe2\x82\xa0gh\xe2\x82\xa4jl\xe2\x82\xa4jl";

  g_assert (g_utf8_strrchr (string, -1, 'j') == string + 13);
  g_assert (g_utf8_strrchr (string, -1, 8356) == string + 10);
  g_assert (g_utf8_strrchr (string, 9, 8356) == string + 5);
  g_assert (g_utf8_strrchr (string, 3, 'j') == NULL);
  g_assert (g_utf8_strrchr (string, -1, 'x') == NULL);
}

static void
test_unichar_validate (void)
{
  g_assert (g_unichar_validate ('j'));
  g_assert (g_unichar_validate (8356));
  g_assert (g_unichar_validate (8356));
  g_assert (!g_unichar_validate (0xfdd1));
  g_assert (g_unichar_validate (917760));
  g_assert (!g_unichar_validate (0x110000));
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/utf8/strlen", test_utf8_strlen);
  g_test_add_func ("/utf8/strncpy", test_utf8_strncpy);
  g_test_add_func ("/utf8/strrchr", test_utf8_strrchr);
  g_test_add_func ("/unicode/validate", test_unichar_validate);

  return g_test_run();
}
