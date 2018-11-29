/* GLIB - Library of useful routines for C programming
 * Copyright (C) 2010 Ryan Lortie
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

#include <glib.h>

static void
test_listenv (void)
{
  GHashTable *table;
  gchar **list;
  gint i;

  g_test_summary ("Test g_get_environ() returns an array of unique keys, all "
                  "of which can be individually queried using g_getenv() to "
                  "return the same value.");

  table = g_hash_table_new_full (g_str_hash, g_str_equal,
                                 g_free, g_free);

  list = g_get_environ ();
  for (i = 0; list[i]; i++)
    {
      gchar **parts;

      parts = g_strsplit (list[i], "=", 2);
      g_assert_null (g_hash_table_lookup (table, parts[0]));
      if (g_strcmp0 (parts[0], ""))
        g_hash_table_insert (table, parts[0], parts[1]);
      g_free (parts);
    }
  g_strfreev (list);

  g_assert_cmpint (g_hash_table_size (table), >, 0);

  list = g_listenv ();
  for (i = 0; list[i]; i++)
    {
      const gchar *expected;
      const gchar *value;

      expected = g_hash_table_lookup (table, list[i]);
      value = g_getenv (list[i]);
      g_assert_cmpstr (value, ==, expected);
      g_hash_table_remove (table, list[i]);
    }
  g_assert_cmpint (g_hash_table_size (table), ==, 0);
  g_hash_table_unref (table);
  g_strfreev (list);
}

static void
test_getenv (void)
{
  const gchar *data;
  const gchar *variable = "TEST_G_SETENV";
  const gchar *value1 = "works";
  const gchar *value2 = "again";

  g_test_summary ("Test setting an environment variable using g_setenv(), and "
                  "that the updated value is queryable using g_getenv().");

  /* Check that TEST_G_SETENV is not already set */
  g_assert_null (g_getenv (variable));

  /* Check if g_setenv() failed */
  g_assert_cmpint (g_setenv (variable, value1, TRUE), !=, 0);

  data = g_getenv (variable);
  g_assert_nonnull (data);
  g_assert_cmpstr (data, ==, value1);

  g_assert_cmpint (g_setenv (variable, value2, FALSE), !=, 0);

  data = g_getenv (variable);
  g_assert_nonnull (data);
  g_assert_cmpstr (data, !=, value2);
  g_assert_cmpstr (data, ==, value1);

  g_assert_cmpint (g_setenv (variable, value2, TRUE), !=, 0);

  data = g_getenv (variable);
  g_assert_nonnull (data);
  g_assert_cmpstr (data, !=, value1);
  g_assert_cmpstr (data, ==, value2);

  g_unsetenv (variable);
  g_assert_null (g_getenv (variable));

  if (g_test_undefined ())
    {
      g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL,
                             "*assertion* != NULL*");
      g_assert_false (g_setenv (NULL, "baz", TRUE));
      g_test_assert_expected_messages ();

      g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL,
                             "*assertion* != NULL*");
      g_assert_false (g_setenv ("foo", NULL, TRUE));
      g_test_assert_expected_messages ();

      g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL,
                             "*assertion* == NULL*");
      g_assert_false (g_setenv ("foo=bar", "baz", TRUE));
      g_test_assert_expected_messages ();
    }

  g_assert_true (g_setenv ("foo", "bar=baz", TRUE));

  /* Different OSs return different values; some return NULL because the key
   * is invalid, but some are happy to return what we set above. */
  data = g_getenv ("foo=bar");
  if (data != NULL)
    g_assert_cmpstr (data, ==, "baz");
  else
    {
      data = g_getenv ("foo");
      g_assert_cmpstr (data, ==, "bar=baz");
    }

  if (g_test_undefined ())
    {
      g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL,
                             "*assertion* != NULL*");
      g_unsetenv (NULL);
      g_test_assert_expected_messages ();

      g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL,
                             "*assertion* == NULL*");
      g_unsetenv ("foo=bar");
      g_test_assert_expected_messages ();
    }

  g_unsetenv ("foo");
  g_assert_null (g_getenv ("foo"));
}

static void
test_setenv (void)
{
  const gchar *var, *value;

  var = "NOSUCHENVVAR";
  value = "value1";

  g_assert_null (g_getenv (var));
  g_setenv (var, value, FALSE);
  g_assert_cmpstr (g_getenv (var), ==, value);
  g_assert_true (g_setenv (var, "value2", FALSE));
  g_assert_cmpstr (g_getenv (var), ==, value);
  g_assert_true (g_setenv (var, "value2", TRUE));
  g_assert_cmpstr (g_getenv (var), ==, "value2");
  g_unsetenv (var);
  g_assert_null (g_getenv (var));
}

static void
test_environ_array (void)
{
  gchar **env;
  const gchar *value;

  g_test_summary ("Test getting and setting variables on a local envp array "
                  "(rather than the global envp).");

  env = g_new (gchar *, 1);
  env[0] = NULL;

  if (g_test_undefined ())
    {
      g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL,
                             "*assertion* != NULL*");
      g_environ_getenv (env, NULL);
      g_test_assert_expected_messages ();
    }

  value = g_environ_getenv (env, "foo");
  g_assert_null (value);

  if (g_test_undefined ())
    {
      gchar **undefined_env;

      g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL,
                             "*assertion* != NULL*");
      undefined_env = g_environ_setenv (env, NULL, "bar", TRUE);
      g_test_assert_expected_messages ();
      g_strfreev (undefined_env);

      g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL,
                             "*assertion* == NULL*");
      undefined_env = g_environ_setenv (env, "foo=fuz", "bar", TRUE);
      g_test_assert_expected_messages ();
      g_strfreev (undefined_env);

      g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL,
                             "*assertion* != NULL*");
      undefined_env = g_environ_setenv (env, "foo", NULL, TRUE);
      g_test_assert_expected_messages ();
      g_strfreev (undefined_env);

      g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL,
                             "*assertion* != NULL*");
      undefined_env = g_environ_unsetenv (env, NULL);
      g_test_assert_expected_messages ();
      g_strfreev (undefined_env);
    }

  env = g_environ_setenv (env, "foo", "bar", TRUE);
  value = g_environ_getenv (env, "foo");
  g_assert_cmpstr (value, ==, "bar");

  env = g_environ_setenv (env, "foo2", "bar2", FALSE);
  value = g_environ_getenv (env, "foo");
  g_assert_cmpstr (value, ==, "bar");
  value = g_environ_getenv (env, "foo2");
  g_assert_cmpstr (value, ==, "bar2");

  env = g_environ_setenv (env, "foo", "x", FALSE);
  value = g_environ_getenv (env, "foo");
  g_assert_cmpstr (value, ==, "bar");

  env = g_environ_setenv (env, "foo", "x", TRUE);
  value = g_environ_getenv (env, "foo");
  g_assert_cmpstr (value, ==, "x");

  env = g_environ_unsetenv (env, "foo2");
  value = g_environ_getenv (env, "foo2");
  g_assert_null (value);

  g_strfreev (env);
}

static void
test_environ_null (void)
{
  gchar **env;
  const gchar *value;

  g_test_summary ("Test getting and setting variables on a NULL envp array.");

  env = NULL;

  value = g_environ_getenv (env, "foo");
  g_assert_null (value);

  env = g_environ_setenv (NULL, "foo", "bar", TRUE);
  g_assert_nonnull (env);
  g_strfreev (env);

  env = g_environ_unsetenv (NULL, "foo");
  g_assert_null (env);
}

static void
test_environ_case (void)
{
  gchar **env;
  const gchar *value;

  g_test_summary ("Test that matching environment variables is case-insensitive "
                  "on Windows and not on other platforms, since envvars were "
                  "case-insensitive on DOS.");

  env = NULL;

  env = g_environ_setenv (env, "foo", "bar", TRUE);
  value = g_environ_getenv (env, "foo");
  g_assert_cmpstr (value, ==, "bar");

  value = g_environ_getenv (env, "Foo");
#ifdef G_OS_WIN32
  g_assert_cmpstr (value, ==, "bar");
#else
  g_assert_null (value);
#endif

  env = g_environ_setenv (env, "FOO", "x", TRUE);
  value = g_environ_getenv (env, "foo");
#ifdef G_OS_WIN32
  g_assert_cmpstr (value, ==, "x");
#else
  g_assert_cmpstr (value, ==, "bar");
#endif

  env = g_environ_unsetenv (env, "Foo");
  value = g_environ_getenv (env, "foo");
#ifdef G_OS_WIN32
  g_assert_null (value);
#else
  g_assert_cmpstr (value, ==, "bar");
#endif

  g_strfreev (env);
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/environ/listenv", test_listenv);
  g_test_add_func ("/environ/getenv", test_getenv);
  g_test_add_func ("/environ/setenv", test_setenv);
  g_test_add_func ("/environ/array", test_environ_array);
  g_test_add_func ("/environ/null", test_environ_null);
  g_test_add_func ("/environ/case", test_environ_case);

  return g_test_run ();
}
