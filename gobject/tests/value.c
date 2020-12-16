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
  guint i;

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

/* We create some dummy objects with this relationship:
 *
 *               GObject           TestInterface
 *              /       \         /  /
 *     TestObjectA     TestObjectB  /
 *      /       \                  /
 * TestObjectA1 TestObjectA2-------   
 *
 * ie: TestObjectA1 and TestObjectA2 are subclasses of TestObjectA
 * and TestObjectB is related to neither. TestObjectA2 and TestObjectB
 * implement TestInterface
 */

typedef GTypeInterface TestInterfaceInterface;
static GType test_interface_get_type (void);
G_DEFINE_INTERFACE (TestInterface, test_interface, G_TYPE_OBJECT)
static void test_interface_default_init (TestInterfaceInterface *iface) { }

static GType test_object_a_get_type (void);
typedef GObject TestObjectA; typedef GObjectClass TestObjectAClass;
G_DEFINE_TYPE (TestObjectA, test_object_a, G_TYPE_OBJECT)
static void test_object_a_class_init (TestObjectAClass *class) { }
static void test_object_a_init (TestObjectA *a) { }

static GType test_object_b_get_type (void);
typedef GObject TestObjectB; typedef GObjectClass TestObjectBClass;
static void test_object_b_iface_init (TestInterfaceInterface *iface) { }
G_DEFINE_TYPE_WITH_CODE (TestObjectB, test_object_b, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (test_interface_get_type (), test_object_b_iface_init))
static void test_object_b_class_init (TestObjectBClass *class) { }
static void test_object_b_init (TestObjectB *b) { }

static GType test_object_a1_get_type (void);
typedef GObject TestObjectA1; typedef GObjectClass TestObjectA1Class;
G_DEFINE_TYPE (TestObjectA1, test_object_a1, test_object_a_get_type ())
static void test_object_a1_class_init (TestObjectA1Class *class) { }
static void test_object_a1_init (TestObjectA1 *c) { }

static GType test_object_a2_get_type (void);
typedef GObject TestObjectA2; typedef GObjectClass TestObjectA2Class;
static void test_object_a2_iface_init (TestInterfaceInterface *iface) { }
G_DEFINE_TYPE_WITH_CODE (TestObjectA2, test_object_a2, test_object_a_get_type (),
                         G_IMPLEMENT_INTERFACE (test_interface_get_type (), test_object_a2_iface_init))
static void test_object_a2_class_init (TestObjectA2Class *class) { }
static void test_object_a2_init (TestObjectA2 *b) { }

static void
test_value_transform_object (void)
{
  GValue src = G_VALUE_INIT;
  GValue dest = G_VALUE_INIT;
  GObject *object;
  guint i, s, d;
  GType types[] = {
    G_TYPE_OBJECT,
    test_interface_get_type (),
    test_object_a_get_type (),
    test_object_b_get_type (),
    test_object_a1_get_type (),
    test_object_a2_get_type ()
  };

  for (i = 0; i < G_N_ELEMENTS (types); i++)
    {
      if (!G_TYPE_IS_CLASSED (types[i]))
        continue;

      object = g_object_new (types[i], NULL);

      for (s = 0; s < G_N_ELEMENTS (types); s++)
        {
          if (!G_TYPE_CHECK_INSTANCE_TYPE (object, types[s]))
            continue;

          g_value_init (&src, types[s]);
          g_value_set_object (&src, object);

          for (d = 0; d < G_N_ELEMENTS (types); d++)
            {
              g_test_message ("Next: %s object in GValue of %s to GValue of %s", g_type_name (types[i]), g_type_name (types[s]), g_type_name (types[d]));
              g_assert_true (g_value_type_transformable (types[s], types[d]));
              g_value_init (&dest, types[d]);
              g_assert_true (g_value_transform (&src, &dest));
              g_assert_cmpint (g_value_get_object (&dest) != NULL, ==, G_TYPE_CHECK_INSTANCE_TYPE (object, types[d]));
              g_value_unset (&dest);
            }
          g_value_unset (&src);
        }

      g_object_unref (object);
    }
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/value/basic", test_value_basic);
  g_test_add_func ("/value/string", test_value_string);
  g_test_add_func ("/value/array/basic", test_valuearray_basic);
  g_test_add_func ("/value/transform-object", test_value_transform_object);

  return g_test_run ();
}
