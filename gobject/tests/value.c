#define GLIB_VERSION_MIN_REQUIRED       GLIB_VERSION_2_30
#include <glib-object.h>

static void
test_value_basic (void)
{
  GValue value = G_VALUE_INIT;

  g_assert_false (G_IS_VALUE (&value));
  g_assert_false (G_VALUE_HOLDS_INT (&value));
  g_value_unset (&value);
  g_assert_false (G_IS_VALUE (&value));
  g_assert_false (G_VALUE_HOLDS_INT (&value));

  g_value_init (&value, G_TYPE_INT);
  g_assert_true (G_IS_VALUE (&value));
  g_assert_true (G_VALUE_HOLDS_INT (&value));
  g_assert_false (G_VALUE_HOLDS_UINT (&value));
  g_assert_cmpint (g_value_get_int (&value), ==, 0);

  g_value_set_int (&value, 10);
  g_assert_cmpint (g_value_get_int (&value), ==, 10);

  g_value_reset (&value);
  g_assert_true (G_IS_VALUE (&value));
  g_assert_true (G_VALUE_HOLDS_INT (&value));
  g_assert_cmpint (g_value_get_int (&value), ==, 0);

  g_value_unset (&value);
  g_assert_false (G_IS_VALUE (&value));
  g_assert_false (G_VALUE_HOLDS_INT (&value));
}

static void
test_value_string (void)
{
  const gchar *static1 = "static1";
  const gchar *static2 = "static2";
  const gchar *storedstr;
  const gchar *copystr;
  gchar *str1, *str2;
  GValue value = G_VALUE_INIT;
  GValue copy = G_VALUE_INIT;

  g_test_summary ("Test that G_TYPE_STRING GValue copy properly");

  /*
   * Regular strings (ownership not passed)
   */

  /* Create a regular string gvalue and make sure it copies the provided string */
  g_value_init (&value, G_TYPE_STRING);
  g_assert_true (G_VALUE_HOLDS_STRING (&value));

  /* The string contents should be empty at this point */
  storedstr = g_value_get_string (&value);
  g_assert_true (storedstr == NULL);

  g_value_set_string (&value, static1);
  /* The contents should be a copy of the same string */
  storedstr = g_value_get_string (&value);
  g_assert_true (storedstr != static1);
  g_assert_cmpstr (storedstr, ==, static1);
  /* Check g_value_dup_string() provides a copy */
  str1 = g_value_dup_string (&value);
  g_assert_true (storedstr != str1);
  g_assert_cmpstr (str1, ==, static1);
  g_free (str1);

  /* Copying a regular string gvalue should copy the contents */
  g_value_init (&copy, G_TYPE_STRING);
  g_value_copy (&value, &copy);
  copystr = g_value_get_string (&copy);
  g_assert_true (copystr != storedstr);
  g_assert_cmpstr (copystr, ==, static1);
  g_value_unset (&copy);

  /* Setting a new string should change the contents */
  g_value_set_string (&value, static2);
  /* The contents should be a copy of that *new* string */
  storedstr = g_value_get_string (&value);
  g_assert_true (storedstr != static2);
  g_assert_cmpstr (storedstr, ==, static2);

  /* Setting a static string over that should also change it (test for
   * coverage and valgrind) */
  g_value_set_static_string (&value, static1);
  storedstr = g_value_get_string (&value);
  g_assert_true (storedstr != static2);
  g_assert_cmpstr (storedstr, ==, static1);

  /* Giving a string directly (ownership passed) should replace the content */
  str2 = g_strdup (static2);
  g_value_take_string (&value, str2);
  storedstr = g_value_get_string (&value);
  g_assert_true (storedstr != static2);
  g_assert_cmpstr (storedstr, ==, str2);

  g_value_unset (&value);

  /*
   * Regular strings (ownership passed)
   */

  g_value_init (&value, G_TYPE_STRING);
  g_assert_true (G_VALUE_HOLDS_STRING (&value));
  str1 = g_strdup (static1);
  g_value_take_string (&value, str1);
  /* The contents should be the string we provided */
  storedstr = g_value_get_string (&value);
  g_assert_true (storedstr == str1);
  /* But g_value_dup_string() should provide a copy */
  str2 = g_value_dup_string (&value);
  g_assert_true (storedstr != str2);
  g_assert_cmpstr (str2, ==, static1);
  g_free (str2);

  /* Copying a regular string gvalue (even with ownership passed) should copy
   * the contents */
  g_value_init (&copy, G_TYPE_STRING);
  g_value_copy (&value, &copy);
  copystr = g_value_get_string (&copy);
  g_assert_true (copystr != storedstr);
  g_assert_cmpstr (copystr, ==, static1);
  g_value_unset (&copy);

  /* Setting a new regular string should change the contents */
  g_value_set_string (&value, static2);
  /* The contents should be a copy of that *new* string */
  storedstr = g_value_get_string (&value);
  g_assert_true (storedstr != static2);
  g_assert_cmpstr (storedstr, ==, static2);

  g_value_unset (&value);

  /*
   * Static strings
   */
  g_value_init (&value, G_TYPE_STRING);
  g_assert_true (G_VALUE_HOLDS_STRING (&value));
  g_value_set_static_string (&value, static1);
  /* The contents should be the string we provided */
  storedstr = g_value_get_string (&value);
  g_assert_true (storedstr == static1);
  /* But g_value_dup_string() should provide a copy */
  str2 = g_value_dup_string (&value);
  g_assert_true (storedstr != str2);
  g_assert_cmpstr (str2, ==, static1);
  g_free (str2);

  /* Copying a static string gvalue should *actually* copy the contents */
  g_value_init (&copy, G_TYPE_STRING);
  g_value_copy (&value, &copy);
  copystr = g_value_get_string (&copy);
  g_assert_true (copystr != static1);
  g_value_unset (&copy);

  /* Setting a new string should change the contents */
  g_value_set_static_string (&value, static2);
  /* The contents should be a copy of that *new* string */
  storedstr = g_value_get_string (&value);
  g_assert_true (storedstr != static1);
  g_assert_cmpstr (storedstr, ==, static2);

  g_value_unset (&value);

  /*
   * Interned/Canonical strings
   */
  static1 = g_intern_static_string (static1);
  g_value_init (&value, G_TYPE_STRING);
  g_assert_true (G_VALUE_HOLDS_STRING (&value));
  g_value_set_interned_string (&value, static1);
  g_assert_true (G_VALUE_IS_INTERNED_STRING (&value));
  /* The contents should be the string we provided */
  storedstr = g_value_get_string (&value);
  g_assert_true (storedstr == static1);
  /* But g_value_dup_string() should provide a copy */
  str2 = g_value_dup_string (&value);
  g_assert_true (storedstr != str2);
  g_assert_cmpstr (str2, ==, static1);
  g_free (str2);

  /* Copying an interned string gvalue should *not* copy the contents
   * and should still be an interned string */
  g_value_init (&copy, G_TYPE_STRING);
  g_value_copy (&value, &copy);
  g_assert_true (G_VALUE_IS_INTERNED_STRING (&copy));
  copystr = g_value_get_string (&copy);
  g_assert_true (copystr == static1);
  g_value_unset (&copy);

  /* Setting a new interned string should change the contents */
  static2 = g_intern_static_string (static2);
  g_value_set_interned_string (&value, static2);
  g_assert_true (G_VALUE_IS_INTERNED_STRING (&value));
  /* The contents should be the interned string */
  storedstr = g_value_get_string (&value);
  g_assert_cmpstr (storedstr, ==, static2);

  /* Setting a new regular string should change the contents */
  g_value_set_string (&value, static2);
  g_assert_false (G_VALUE_IS_INTERNED_STRING (&value));
  /* The contents should be a copy of that *new* string */
  storedstr = g_value_get_string (&value);
  g_assert_true (storedstr != static2);
  g_assert_cmpstr (storedstr, ==, static2);

  g_value_unset (&value);
}

static gint
cmpint (gconstpointer a, gconstpointer b)
{
  const GValue *aa = a;
  const GValue *bb = b;

  return g_value_get_int (aa) - g_value_get_int (bb);
}

static void
test_valuearray_basic (void)
{
  GValueArray *a;
  GValueArray *a2;
  GValue v = G_VALUE_INIT;
  GValue *p;
  gint i;

  a = g_value_array_new (20);

  g_value_init (&v, G_TYPE_INT);
  for (i = 0; i < 100; i++)
    {
      g_value_set_int (&v, i);
      g_value_array_append (a, &v);
    }

  g_assert_cmpint (a->n_values, ==, 100);
  p = g_value_array_get_nth (a, 5);
  g_assert_cmpint (g_value_get_int (p), ==, 5);

  for (i = 20; i < 100; i+= 5)
    g_value_array_remove (a, 100 - i);

  for (i = 100; i < 150; i++)
    {
      g_value_set_int (&v, i);
      g_value_array_prepend (a, &v);
    }

  g_value_array_sort (a, cmpint);
  for (i = 0; i < a->n_values - 1; i++)
    g_assert_cmpint (g_value_get_int (&a->values[i]), <=, g_value_get_int (&a->values[i+1]));

  a2 = g_value_array_copy (a);
  for (i = 0; i < a->n_values; i++)
    g_assert_cmpint (g_value_get_int (&a->values[i]), ==, g_value_get_int (&a2->values[i]));

  g_value_array_free (a);
  g_value_array_free (a2);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/value/basic", test_value_basic);
  g_test_add_func ("/value/string", test_value_string);
  g_test_add_func ("/value/array/basic", test_valuearray_basic);

  return g_test_run ();
}
