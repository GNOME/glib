/*
 * Copyright © 2020 Canonical Ltd.
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
 */

#include "glib.h"

static void
test_strvbuilder_empty (void)
{
  GStrvBuilder *builder;
  GStrv result;

  builder = g_strv_builder_new ();
  result = g_strv_builder_end (builder);
  g_assert_nonnull (result);
  g_assert_cmpint (g_strv_length (result), ==, 0);

  g_strfreev (result);
  g_strv_builder_unref (builder);
}

static void
test_strvbuilder_append (void)
{
  GStrvBuilder *builder;
  GStrv result;
  const gchar *expected[] = { "one", "two", "three", NULL };

  builder = g_strv_builder_new ();
  g_strv_builder_append (builder, "one");
  g_strv_builder_append (builder, "two");
  g_strv_builder_append (builder, "three");
  result = g_strv_builder_end (builder);
  g_assert_nonnull (result);
  g_assert_true (g_strv_equal ((const gchar * const*) result, expected));

  g_strfreev (result);
  g_strv_builder_unref (builder);
}

static void
test_strvbuilder_prepend (void)
{
  GStrvBuilder *builder;
  GStrv result;
  const gchar *expected[] = { "one", "two", "three", NULL };

  builder = g_strv_builder_new ();
  g_strv_builder_prepend (builder, "three");
  g_strv_builder_prepend (builder, "two");
  g_strv_builder_prepend (builder, "one");
  result = g_strv_builder_end (builder);
  g_assert_nonnull (result);
  g_assert_true (g_strv_equal ((const gchar * const*) result, expected));

  g_strfreev (result);
  g_strv_builder_unref (builder);
}

static void
test_strvbuilder_insert (void)
{
  GStrvBuilder *builder;
  GStrv result;
  const gchar *expected[] = { "one", "two", "three", NULL };

  builder = g_strv_builder_new ();
  g_strv_builder_insert (builder, 0, "one");
  g_strv_builder_insert (builder, 1, "three");
  g_strv_builder_insert (builder, 1, "two");
  result = g_strv_builder_end (builder);
  g_assert_nonnull (result);
  g_assert_true (g_strv_equal ((const gchar * const*) result, expected));

  g_strfreev (result);
  g_strv_builder_unref (builder);
}

static void
test_strvbuilder_ref (void)
{
  GStrvBuilder *builder;

  builder = g_strv_builder_new ();
  g_strv_builder_ref (builder);
  g_strv_builder_unref (builder);
  g_strv_builder_unref (builder);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/strvbuilder/empty", test_strvbuilder_empty);
  g_test_add_func ("/strvbuilder/append", test_strvbuilder_append);
  g_test_add_func ("/strvbuilder/prepend", test_strvbuilder_prepend);
  g_test_add_func ("/strvbuilder/insert", test_strvbuilder_insert);
  g_test_add_func ("/strvbuilder/ref", test_strvbuilder_ref);

  return g_test_run ();
}
