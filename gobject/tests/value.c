#define GLIB_VERSION_MIN_REQUIRED       GLIB_VERSION_2_30
#include <glib-object.h>
#include <gobject/gvaluecollector.h>

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
test_enum_transformation (void)
{
  GType type;
  GValue orig = G_VALUE_INIT;
  GValue xform = G_VALUE_INIT;
  GEnumValue values[] = { {0,"0","0"}, {1,"1","1"}};

 type = g_enum_register_static ("TestEnum", values);

 g_value_init (&orig, type);
 g_value_set_enum (&orig, 1);

 memset (&xform, 0, sizeof (GValue));
 g_value_init (&xform, G_TYPE_CHAR);
 g_value_transform (&orig, &xform);
 g_assert (g_value_get_char (&xform) == 1);
 g_assert (g_value_get_schar (&xform) == 1);

 memset (&xform, 0, sizeof (GValue));
 g_value_init (&xform, G_TYPE_UCHAR);
 g_value_transform (&orig, &xform);
 g_assert (g_value_get_uchar (&xform) == 1);

 memset (&xform, 0, sizeof (GValue));
 g_value_init (&xform, G_TYPE_INT);
 g_value_transform (&orig, &xform);
 g_assert (g_value_get_int (&xform) == 1);

 memset (&xform, 0, sizeof (GValue));
 g_value_init (&xform, G_TYPE_UINT);
 g_value_transform (&orig, &xform);
 g_assert (g_value_get_uint (&xform) == 1);

 memset (&xform, 0, sizeof (GValue));
 g_value_init (&xform, G_TYPE_LONG);
 g_value_transform (&orig, &xform);
 g_assert (g_value_get_long (&xform) == 1);

 memset (&xform, 0, sizeof (GValue));
 g_value_init (&xform, G_TYPE_ULONG);
 g_value_transform (&orig, &xform);
 g_assert (g_value_get_ulong (&xform) == 1);

 memset (&xform, 0, sizeof (GValue));
 g_value_init (&xform, G_TYPE_INT64);
 g_value_transform (&orig, &xform);
 g_assert (g_value_get_int64 (&xform) == 1);

 memset (&xform, 0, sizeof (GValue));
 g_value_init (&xform, G_TYPE_UINT64);
 g_value_transform (&orig, &xform);
 g_assert (g_value_get_uint64 (&xform) == 1);
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
    g_assert_cmpint (g_value_get_int (&a->values[i]), <=,
                     g_value_get_int (&a->values[i+1]));

  a2 = g_value_array_copy (a);
  for (i = 0; i < a->n_values; i++)
    g_assert_cmpint (g_value_get_int (&a->values[i]), ==,
                     g_value_get_int (&a2->values[i]));

  g_value_array_free (a);
  g_value_array_free (a2);
}

static void
test_gtype_value (void)
{
  GType type;
  GValue value = G_VALUE_INIT;
  GValue copy = G_VALUE_INIT;

  g_value_init (&value, G_TYPE_GTYPE);

  g_value_set_gtype (&value, G_TYPE_BOXED);
  type = g_value_get_gtype (&value);
  g_assert (type == G_TYPE_BOXED);

  g_value_init (&copy, G_TYPE_GTYPE);
  g_value_copy (&value, &copy);
  type = g_value_get_gtype (&copy);
  g_assert (type == G_TYPE_BOXED);
}

static gchar *
collect (GValue *value, ...)
{
  gchar *error;
  va_list var_args;

  error = NULL;

  va_start (var_args, value);
  G_VALUE_COLLECT (value, var_args, 0, &error);
  va_end (var_args);

  return error;
}

static gchar *
lcopy (GValue *value, ...)
{
  gchar *error;
  va_list var_args;

  error = NULL;

  va_start (var_args, value);
  G_VALUE_LCOPY (value, var_args, 0, &error);
  va_end (var_args);

  return error;
}

static void
test_collection (void)
{
  GValue value = G_VALUE_INIT;
  gchar *error;

  g_value_init (&value, G_TYPE_CHAR);
  error = collect (&value, 'c');
  g_assert (error == NULL);
  g_assert (g_value_get_char (&value) == 'c');

  g_value_unset (&value);
  g_value_init (&value, G_TYPE_UCHAR);
  error = collect (&value, 129);
  g_assert (error == NULL);
  g_assert (g_value_get_uchar (&value) == 129);

  g_value_unset (&value);
  g_value_init (&value, G_TYPE_BOOLEAN);
  error = collect (&value, TRUE);
  g_assert (error == NULL);
  g_assert (g_value_get_boolean (&value) == TRUE);

  g_value_unset (&value);
  g_value_init (&value, G_TYPE_INT);
  error = collect (&value, G_MAXINT);
  g_assert (error == NULL);
  g_assert (g_value_get_int (&value) == G_MAXINT);

  g_value_unset (&value);
  g_value_init (&value, G_TYPE_UINT);
  error = collect (&value, G_MAXUINT);
  g_assert (error == NULL);
  g_assert (g_value_get_uint (&value) == G_MAXUINT);

  g_value_unset (&value);
  g_value_init (&value, G_TYPE_LONG);
  error = collect (&value, G_MAXLONG);
  g_assert (error == NULL);
  g_assert (g_value_get_long (&value) == G_MAXLONG);

  g_value_unset (&value);
  g_value_init (&value, G_TYPE_ULONG);
  error = collect (&value, G_MAXULONG);
  g_assert (error == NULL);
  g_assert (g_value_get_ulong (&value) == G_MAXULONG);

  g_value_unset (&value);
  g_value_init (&value, G_TYPE_INT64);
  error = collect (&value, G_MAXINT64);
  g_assert (error == NULL);
  g_assert (g_value_get_int64 (&value) == G_MAXINT64);

  g_value_unset (&value);
  g_value_init (&value, G_TYPE_UINT64);
  error = collect (&value, G_MAXUINT64);
  g_assert (error == NULL);
  g_assert (g_value_get_uint64 (&value) == G_MAXUINT64);

  g_value_unset (&value);
  g_value_init (&value, G_TYPE_FLOAT);
  error = collect (&value, G_MAXFLOAT);
  g_assert (error == NULL);
  g_assert (g_value_get_float (&value) == G_MAXFLOAT);

  g_value_unset (&value);
  g_value_init (&value, G_TYPE_DOUBLE);
  error = collect (&value, G_MAXDOUBLE);
  g_assert (error == NULL);
  g_assert (g_value_get_double (&value) == G_MAXDOUBLE);

  g_value_unset (&value);
  g_value_init (&value, G_TYPE_STRING);
  error = collect (&value, "string ?");
  g_assert (error == NULL);
  g_assert (strcmp (g_value_get_string (&value), "string ?") == 0);

  g_value_unset (&value);
  g_value_init (&value, G_TYPE_GTYPE);
  error = collect (&value, G_TYPE_BOXED);
  g_assert (error == NULL);
  g_assert (g_value_get_gtype (&value) == G_TYPE_BOXED);

  g_value_unset (&value);
  g_value_init (&value, G_TYPE_VARIANT);
  error = collect (&value, g_variant_new_uint32 (42));
  g_assert (error == NULL);
  g_assert (g_variant_is_of_type (g_value_get_variant (&value), G_VARIANT_TYPE ("u")));
  g_assert_cmpuint (g_variant_get_uint32 (g_value_get_variant (&value)), ==, 42);

  g_value_unset (&value);
}

static void
test_copying (void)
{
  GValue value = G_VALUE_INIT;
  gchar *error;

  {
    gchar c = 0;

    g_value_init (&value, G_TYPE_CHAR);
    g_value_set_char (&value, 'c');
    error = lcopy (&value, &c);
    g_assert (error == NULL);
    g_assert (c == 'c');
  }

  {
    guchar c = 0;

    g_value_unset (&value);
    g_value_init (&value, G_TYPE_UCHAR);
    g_value_set_uchar (&value, 129);
    error = lcopy (&value, &c);
    g_assert (error == NULL);
    g_assert (c == 129);
  }

  {
    gint c = 0;

    g_value_unset (&value);
    g_value_init (&value, G_TYPE_INT);
    g_value_set_int (&value, G_MAXINT);
    error = lcopy (&value, &c);
    g_assert (error == NULL);
    g_assert (c == G_MAXINT);
  }

  {
    guint c = 0;

    g_value_unset (&value);
    g_value_init (&value, G_TYPE_UINT);
    g_value_set_uint (&value, G_MAXUINT);
    error = lcopy (&value, &c);
    g_assert (error == NULL);
    g_assert (c == G_MAXUINT);
  }

  {
    glong c = 0;

    g_value_unset (&value);
    g_value_init (&value, G_TYPE_LONG);
    g_value_set_long (&value, G_MAXLONG);
    error = lcopy (&value, &c);
    g_assert (error == NULL);
    g_assert (c == G_MAXLONG);
  }

  {
    gulong c = 0;

    g_value_unset (&value);
    g_value_init (&value, G_TYPE_ULONG);
    g_value_set_ulong (&value, G_MAXULONG);
    error = lcopy (&value, &c);
    g_assert (error == NULL);
    g_assert (c == G_MAXULONG);
  }

  {
    gint64 c = 0;

    g_value_unset (&value);
    g_value_init (&value, G_TYPE_INT64);
    g_value_set_int64 (&value, G_MAXINT64);
    error = lcopy (&value, &c);
    g_assert (error == NULL);
    g_assert (c == G_MAXINT64);
  }

  {
    guint64 c = 0;

    g_value_unset (&value);
    g_value_init (&value, G_TYPE_UINT64);
    g_value_set_uint64 (&value, G_MAXUINT64);
    error = lcopy (&value, &c);
    g_assert (error == NULL);
    g_assert (c == G_MAXUINT64);
  }

  {
    gfloat c = 0;

    g_value_unset (&value);
    g_value_init (&value, G_TYPE_FLOAT);
    g_value_set_float (&value, G_MAXFLOAT);
    error = lcopy (&value, &c);
    g_assert (error == NULL);
    g_assert (c == G_MAXFLOAT);
  }

  {
    gdouble c = 0;

    g_value_unset (&value);
    g_value_init (&value, G_TYPE_DOUBLE);
    g_value_set_double (&value, G_MAXDOUBLE);
    error = lcopy (&value, &c);
    g_assert (error == NULL);
    g_assert (c == G_MAXDOUBLE);
  }

  {
    gchar *c = NULL;

    g_value_unset (&value);
    g_value_init (&value, G_TYPE_STRING);
    g_value_set_string (&value, "string ?");
    error = lcopy (&value, &c);
    g_assert (error == NULL);
    g_assert (strcmp (c, "string ?") == 0);
    g_free (c);
  }

  {
    GType c = G_TYPE_NONE;

    g_value_unset (&value);
    g_value_init (&value, G_TYPE_GTYPE);
    g_value_set_gtype (&value, G_TYPE_BOXED);
    error = lcopy (&value, &c);
    g_assert (error == NULL);
    g_assert (c == G_TYPE_BOXED);
  }

  {
    GVariant *c = NULL;

    g_value_unset (&value);
    g_value_init (&value, G_TYPE_VARIANT);
    g_value_set_variant (&value, g_variant_new_uint32 (42));
    error = lcopy (&value, &c);
    g_assert (error == NULL);
    g_assert (c != NULL);
    g_assert (g_variant_is_of_type (c, G_VARIANT_TYPE ("u")));
    g_assert_cmpuint (g_variant_get_uint32 (c), ==, 42);
    g_variant_unref (c);
    g_value_unset (&value);
  }
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/value/basic", test_value_basic);
  g_test_add_func ("/value/enum-transformation", test_enum_transformation);
  g_test_add_func ("/value/array/basic", test_valuearray_basic);
  g_test_add_func ("/value/gtype", test_gtype_value);
  g_test_add_func ("/value/collection", test_collection);
  g_test_add_func ("/value/copying", test_copying);

  return g_test_run ();
}
