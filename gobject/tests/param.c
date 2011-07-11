#include <glib-object.h>

static void
test_param_value (void)
{
  GParamSpec *p, *p2;
  GParamSpec *pp;
  GValue value = { 0, };

  g_value_init (&value, G_TYPE_PARAM);
  g_assert (G_VALUE_HOLDS_PARAM (&value));

  p = g_param_spec_int ("my-int", "My Int", "Blurb", 0, 20, 10, G_PARAM_READWRITE);

  g_value_take_param (&value, p);
  p2 = g_value_get_param (&value);
  g_assert (p2 == p);

  pp = g_param_spec_uint ("my-uint", "My UInt", "Blurb", 0, 10, 5, G_PARAM_READWRITE);
  g_value_set_param (&value, pp);

  p2 = g_value_dup_param (&value);
  g_assert (p2 == pp); /* param specs use ref/unref for copy/free */
  g_param_spec_unref (p2);

  g_value_unset (&value);
}

static gint destroy_count;

static void
my_destroy (gpointer data)
{
  destroy_count++;
}

static void
test_param_qdata (void)
{
  GParamSpec *p;
  gchar *bla;
  GQuark q;

  q = g_quark_from_string ("bla");

  p = g_param_spec_int ("my-int", "My Int", "Blurb", 0, 20, 10, G_PARAM_READWRITE);
  g_param_spec_set_qdata (p, q, "bla");
  bla = g_param_spec_get_qdata (p, q);
  g_assert_cmpstr (bla, ==, "bla");

  g_assert_cmpint (destroy_count, ==, 0);
  g_param_spec_set_qdata_full (p, q, "bla", my_destroy);
  g_param_spec_set_qdata_full (p, q, "blabla", my_destroy);
  g_assert_cmpint (destroy_count, ==, 1);
  g_assert_cmpstr (g_param_spec_steal_qdata (p, q), ==, "blabla");
  g_assert_cmpint (destroy_count, ==, 1);
  g_assert (g_param_spec_get_qdata (p, q) == NULL);

  g_param_spec_ref_sink (p);

  g_param_spec_unref (p);
}

static void
test_param_validate (void)
{
  GParamSpec *p;
  GValue value = { 0, };

  p = g_param_spec_int ("my-int", "My Int", "Blurb", 0, 20, 10, G_PARAM_READWRITE);

  g_value_init (&value, G_TYPE_INT);
  g_value_set_int (&value, 100);
  g_assert (!g_param_value_defaults (p, &value));
  g_assert (g_param_value_validate (p, &value));
  g_assert_cmpint (g_value_get_int (&value), ==, 20);

  g_param_value_set_default (p, &value);
  g_assert (g_param_value_defaults (p, &value));
  g_assert_cmpint (g_value_get_int (&value), ==, 10);

  g_param_spec_unref (p);
}

static void
test_param_strings (void)
{
  GParamSpec *p;

  /* test canonicalization */
  p = g_param_spec_int ("my_int:bla", "My Int", "Blurb", 0, 20, 10, G_PARAM_READWRITE);

  g_assert_cmpstr (g_param_spec_get_name (p), ==, "my-int-bla");
  g_assert_cmpstr (g_param_spec_get_nick (p), ==, "My Int");
  g_assert_cmpstr (g_param_spec_get_blurb (p), ==, "Blurb");

  g_param_spec_unref (p);

  /* test nick defaults to name */
  p = g_param_spec_int ("my-int", NULL, NULL, 0, 20, 10, G_PARAM_READWRITE);

  g_assert_cmpstr (g_param_spec_get_name (p), ==, "my-int");
  g_assert_cmpstr (g_param_spec_get_nick (p), ==, "my-int");
  g_assert (g_param_spec_get_blurb (p) == NULL);

  g_param_spec_unref (p);
}

static void
test_param_convert (void)
{
  GParamSpec *p;
  GValue v1 = { 0, };
  GValue v2 = { 0, };

  p = g_param_spec_int ("my-int", "My Int", "Blurb", 0, 20, 10, G_PARAM_READWRITE);
  g_value_init (&v1, G_TYPE_UINT);
  g_value_set_uint (&v1, 43);

  g_value_init (&v2, G_TYPE_INT);
  g_value_set_int (&v2, -4);

  g_assert (!g_param_value_convert (p, &v1, &v2, TRUE));
  g_assert_cmpint (g_value_get_int (&v2), ==, -4);

  g_assert (g_param_value_convert (p, &v1, &v2, FALSE));
  g_assert_cmpint (g_value_get_int (&v2), ==, 20);

  g_param_spec_unref (p);
}

static void
test_value_transform (void)
{
  GValue src = { 0, };
  GValue dest = { 0, };

#define CHECK_INT_CONVERSION(type, getter, value)                       \
  g_assert (g_value_type_transformable (G_TYPE_INT, type));             \
  g_value_init (&src, G_TYPE_INT);                                      \
  g_value_init (&dest, type);                                           \
  g_value_set_int (&src, value);                                        \
  g_assert (g_value_transform (&src, &dest));                           \
  g_assert_cmpint (g_value_get_##getter (&dest), ==, value);            \
  g_value_unset (&src);                                                 \
  g_value_unset (&dest);

  CHECK_INT_CONVERSION(G_TYPE_CHAR, char, -124)
  CHECK_INT_CONVERSION(G_TYPE_CHAR, char, 124)
  CHECK_INT_CONVERSION(G_TYPE_UCHAR, uchar, 0)
  CHECK_INT_CONVERSION(G_TYPE_UCHAR, uchar, 255)
  CHECK_INT_CONVERSION(G_TYPE_INT, int, -12345)
  CHECK_INT_CONVERSION(G_TYPE_INT, int, 12345)
  CHECK_INT_CONVERSION(G_TYPE_UINT, uint, 0)
  CHECK_INT_CONVERSION(G_TYPE_UINT, uint, 12345)
  CHECK_INT_CONVERSION(G_TYPE_LONG, long, -12345678)
  CHECK_INT_CONVERSION(G_TYPE_ULONG, ulong, 12345678)
  CHECK_INT_CONVERSION(G_TYPE_INT64, int64, -12345678)
  CHECK_INT_CONVERSION(G_TYPE_UINT64, uint64, 12345678)
  CHECK_INT_CONVERSION(G_TYPE_FLOAT, float, 12345678)
  CHECK_INT_CONVERSION(G_TYPE_DOUBLE, double, 12345678)

#define CHECK_UINT_CONVERSION(type, getter, value)                      \
  g_assert (g_value_type_transformable (G_TYPE_UINT, type));            \
  g_value_init (&src, G_TYPE_UINT);                                     \
  g_value_init (&dest, type);                                           \
  g_value_set_uint (&src, value);                                       \
  g_assert (g_value_transform (&src, &dest));                           \
  g_assert_cmpuint (g_value_get_##getter (&dest), ==, value);           \
  g_value_unset (&src);                                                 \
  g_value_unset (&dest);

  CHECK_UINT_CONVERSION(G_TYPE_CHAR, char, 124)
  CHECK_UINT_CONVERSION(G_TYPE_CHAR, char, 124)
  CHECK_UINT_CONVERSION(G_TYPE_UCHAR, uchar, 0)
  CHECK_UINT_CONVERSION(G_TYPE_UCHAR, uchar, 255)
  CHECK_UINT_CONVERSION(G_TYPE_INT, int, 12345)
  CHECK_UINT_CONVERSION(G_TYPE_INT, int, 12345)
  CHECK_UINT_CONVERSION(G_TYPE_UINT, uint, 0)
  CHECK_UINT_CONVERSION(G_TYPE_UINT, uint, 12345)
  CHECK_UINT_CONVERSION(G_TYPE_LONG, long, 12345678)
  CHECK_UINT_CONVERSION(G_TYPE_ULONG, ulong, 12345678)
  CHECK_UINT_CONVERSION(G_TYPE_INT64, int64, 12345678)
  CHECK_UINT_CONVERSION(G_TYPE_UINT64, uint64, 12345678)
  CHECK_UINT_CONVERSION(G_TYPE_FLOAT, float, 12345678)
  CHECK_UINT_CONVERSION(G_TYPE_DOUBLE, double, 12345678)

#define CHECK_LONG_CONVERSION(type, getter, value)                      \
  g_assert (g_value_type_transformable (G_TYPE_LONG, type));            \
  g_value_init (&src, G_TYPE_LONG);                                     \
  g_value_init (&dest, type);                                           \
  g_value_set_long (&src, value);                                       \
  g_assert (g_value_transform (&src, &dest));                           \
  g_assert_cmpint (g_value_get_##getter (&dest), ==, value);            \
  g_value_unset (&src);                                                 \
  g_value_unset (&dest);

  CHECK_LONG_CONVERSION(G_TYPE_CHAR, char, -124)
  CHECK_LONG_CONVERSION(G_TYPE_CHAR, char, 124)
  CHECK_LONG_CONVERSION(G_TYPE_UCHAR, uchar, 0)
  CHECK_LONG_CONVERSION(G_TYPE_UCHAR, uchar, 255)
  CHECK_LONG_CONVERSION(G_TYPE_INT, int, -12345)
  CHECK_LONG_CONVERSION(G_TYPE_INT, int, 12345)
  CHECK_LONG_CONVERSION(G_TYPE_UINT, uint, 0)
  CHECK_LONG_CONVERSION(G_TYPE_UINT, uint, 12345)
  CHECK_LONG_CONVERSION(G_TYPE_LONG, long, -12345678)
  CHECK_LONG_CONVERSION(G_TYPE_ULONG, ulong, 12345678)
  CHECK_LONG_CONVERSION(G_TYPE_INT64, int64, -12345678)
  CHECK_LONG_CONVERSION(G_TYPE_UINT64, uint64, 12345678)
  CHECK_LONG_CONVERSION(G_TYPE_FLOAT, float, 12345678)
  CHECK_LONG_CONVERSION(G_TYPE_DOUBLE, double, 12345678)

#define CHECK_ULONG_CONVERSION(type, getter, value)                     \
  g_assert (g_value_type_transformable (G_TYPE_ULONG, type));           \
  g_value_init (&src, G_TYPE_ULONG);                                    \
  g_value_init (&dest, type);                                           \
  g_value_set_ulong (&src, value);                                      \
  g_assert (g_value_transform (&src, &dest));                           \
  g_assert_cmpuint (g_value_get_##getter (&dest), ==, value);           \
  g_value_unset (&src);                                                 \
  g_value_unset (&dest);

  CHECK_ULONG_CONVERSION(G_TYPE_CHAR, char, 124)
  CHECK_ULONG_CONVERSION(G_TYPE_CHAR, char, 124)
  CHECK_ULONG_CONVERSION(G_TYPE_UCHAR, uchar, 0)
  CHECK_ULONG_CONVERSION(G_TYPE_UCHAR, uchar, 255)
  CHECK_ULONG_CONVERSION(G_TYPE_INT, int, -12345)
  CHECK_ULONG_CONVERSION(G_TYPE_INT, int, 12345)
  CHECK_ULONG_CONVERSION(G_TYPE_UINT, uint, 0)
  CHECK_ULONG_CONVERSION(G_TYPE_UINT, uint, 12345)
  CHECK_ULONG_CONVERSION(G_TYPE_LONG, long, 12345678)
  CHECK_ULONG_CONVERSION(G_TYPE_ULONG, ulong, 12345678)
  CHECK_ULONG_CONVERSION(G_TYPE_INT64, int64, 12345678)
  CHECK_ULONG_CONVERSION(G_TYPE_UINT64, uint64, 12345678)
  CHECK_ULONG_CONVERSION(G_TYPE_FLOAT, float, 12345678)
  CHECK_ULONG_CONVERSION(G_TYPE_DOUBLE, double, 12345678)

#define CHECK_INT64_CONVERSION(type, getter, value)                     \
  g_assert (g_value_type_transformable (G_TYPE_INT64, type));           \
  g_value_init (&src, G_TYPE_INT64);                                    \
  g_value_init (&dest, type);                                           \
  g_value_set_int64 (&src, value);                                      \
  g_assert (g_value_transform (&src, &dest));                           \
  g_assert_cmpint (g_value_get_##getter (&dest), ==, value);            \
  g_value_unset (&src);                                                 \
  g_value_unset (&dest);

  CHECK_INT64_CONVERSION(G_TYPE_CHAR, char, -124)
  CHECK_INT64_CONVERSION(G_TYPE_CHAR, char, 124)
  CHECK_INT64_CONVERSION(G_TYPE_UCHAR, uchar, 0)
  CHECK_INT64_CONVERSION(G_TYPE_UCHAR, uchar, 255)
  CHECK_INT64_CONVERSION(G_TYPE_INT, int, -12345)
  CHECK_INT64_CONVERSION(G_TYPE_INT, int, 12345)
  CHECK_INT64_CONVERSION(G_TYPE_UINT, uint, 0)
  CHECK_INT64_CONVERSION(G_TYPE_UINT, uint, 12345)
  CHECK_INT64_CONVERSION(G_TYPE_LONG, long, -12345678)
  CHECK_INT64_CONVERSION(G_TYPE_ULONG, ulong, 12345678)
  CHECK_INT64_CONVERSION(G_TYPE_INT64, int64, -12345678)
  CHECK_INT64_CONVERSION(G_TYPE_UINT64, uint64, 12345678)
  CHECK_INT64_CONVERSION(G_TYPE_FLOAT, float, 12345678)
  CHECK_INT64_CONVERSION(G_TYPE_DOUBLE, double, 12345678)

#define CHECK_UINT64_CONVERSION(type, getter, value)                    \
  g_assert (g_value_type_transformable (G_TYPE_UINT64, type));          \
  g_value_init (&src, G_TYPE_UINT64);                                   \
  g_value_init (&dest, type);                                           \
  g_value_set_uint64 (&src, value);                                     \
  g_assert (g_value_transform (&src, &dest));                           \
  g_assert_cmpuint (g_value_get_##getter (&dest), ==, value);           \
  g_value_unset (&src);                                                 \
  g_value_unset (&dest);

  CHECK_UINT64_CONVERSION(G_TYPE_CHAR, char, -124)
  CHECK_UINT64_CONVERSION(G_TYPE_CHAR, char, 124)
  CHECK_UINT64_CONVERSION(G_TYPE_UCHAR, uchar, 0)
  CHECK_UINT64_CONVERSION(G_TYPE_UCHAR, uchar, 255)
  CHECK_UINT64_CONVERSION(G_TYPE_INT, int, -12345)
  CHECK_UINT64_CONVERSION(G_TYPE_INT, int, 12345)
  CHECK_UINT64_CONVERSION(G_TYPE_UINT, uint, 0)
  CHECK_UINT64_CONVERSION(G_TYPE_UINT, uint, 12345)
  CHECK_UINT64_CONVERSION(G_TYPE_LONG, long, -12345678)
  CHECK_UINT64_CONVERSION(G_TYPE_ULONG, ulong, 12345678)
  CHECK_UINT64_CONVERSION(G_TYPE_INT64, int64, -12345678)
  CHECK_UINT64_CONVERSION(G_TYPE_UINT64, uint64, 12345678)
  CHECK_UINT64_CONVERSION(G_TYPE_FLOAT, float, 12345678)
  CHECK_UINT64_CONVERSION(G_TYPE_DOUBLE, double, 12345678)

#define CHECK_FLOAT_CONVERSION(type, getter, value)                    \
  g_assert (g_value_type_transformable (G_TYPE_FLOAT, type));          \
  g_value_init (&src, G_TYPE_FLOAT);                                   \
  g_value_init (&dest, type);                                          \
  g_value_set_float (&src, value);                                     \
  g_assert (g_value_transform (&src, &dest));                          \
  g_assert_cmpfloat (g_value_get_##getter (&dest), ==, value);         \
  g_value_unset (&src);                                                \
  g_value_unset (&dest);

  CHECK_FLOAT_CONVERSION(G_TYPE_CHAR, char, -124)
  CHECK_FLOAT_CONVERSION(G_TYPE_CHAR, char, 124)
  CHECK_FLOAT_CONVERSION(G_TYPE_UCHAR, uchar, 0)
  CHECK_FLOAT_CONVERSION(G_TYPE_UCHAR, uchar, 255)
  CHECK_FLOAT_CONVERSION(G_TYPE_INT, int, -12345)
  CHECK_FLOAT_CONVERSION(G_TYPE_INT, int, 12345)
  CHECK_FLOAT_CONVERSION(G_TYPE_UINT, uint, 0)
  CHECK_FLOAT_CONVERSION(G_TYPE_UINT, uint, 12345)
  CHECK_FLOAT_CONVERSION(G_TYPE_LONG, long, -12345678)
  CHECK_FLOAT_CONVERSION(G_TYPE_ULONG, ulong, 12345678)
  CHECK_FLOAT_CONVERSION(G_TYPE_INT64, int64, -12345678)
  CHECK_FLOAT_CONVERSION(G_TYPE_UINT64, uint64, 12345678)
  CHECK_FLOAT_CONVERSION(G_TYPE_FLOAT, float, 12345678)
  CHECK_FLOAT_CONVERSION(G_TYPE_DOUBLE, double, 12345678)

#define CHECK_DOUBLE_CONVERSION(type, getter, value)                    \
  g_assert (g_value_type_transformable (G_TYPE_DOUBLE, type));          \
  g_value_init (&src, G_TYPE_DOUBLE);                                   \
  g_value_init (&dest, type);                                           \
  g_value_set_double (&src, value);                                     \
  g_assert (g_value_transform (&src, &dest));                           \
  g_assert_cmpfloat (g_value_get_##getter (&dest), ==, value);          \
  g_value_unset (&src);                                                 \
  g_value_unset (&dest);

  CHECK_DOUBLE_CONVERSION(G_TYPE_CHAR, char, -124)
  CHECK_DOUBLE_CONVERSION(G_TYPE_CHAR, char, 124)
  CHECK_DOUBLE_CONVERSION(G_TYPE_UCHAR, uchar, 0)
  CHECK_DOUBLE_CONVERSION(G_TYPE_UCHAR, uchar, 255)
  CHECK_DOUBLE_CONVERSION(G_TYPE_INT, int, -12345)
  CHECK_DOUBLE_CONVERSION(G_TYPE_INT, int, 12345)
  CHECK_DOUBLE_CONVERSION(G_TYPE_UINT, uint, 0)
  CHECK_DOUBLE_CONVERSION(G_TYPE_UINT, uint, 12345)
  CHECK_DOUBLE_CONVERSION(G_TYPE_LONG, long, -12345678)
  CHECK_DOUBLE_CONVERSION(G_TYPE_ULONG, ulong, 12345678)
  CHECK_DOUBLE_CONVERSION(G_TYPE_INT64, int64, -12345678)
  CHECK_DOUBLE_CONVERSION(G_TYPE_UINT64, uint64, 12345678)
  CHECK_DOUBLE_CONVERSION(G_TYPE_FLOAT, float, 12345678)
  CHECK_DOUBLE_CONVERSION(G_TYPE_DOUBLE, double, 12345678)

#define CHECK_BOOLEAN_CONVERSION(type, setter, value)                   \
  g_assert (g_value_type_transformable (type, G_TYPE_BOOLEAN));         \
  g_value_init (&src, type);                                            \
  g_value_init (&dest, G_TYPE_BOOLEAN);                                 \
  g_value_set_##setter (&src, value);                                   \
  g_assert (g_value_transform (&src, &dest));                           \
  g_assert_cmpint (g_value_get_boolean (&dest), ==, TRUE);              \
  g_value_set_##setter (&src, 0);                                       \
  g_assert (g_value_transform (&src, &dest));                           \
  g_assert_cmpint (g_value_get_boolean (&dest), ==, FALSE);             \
  g_value_unset (&src);                                                 \
  g_value_unset (&dest);

  CHECK_BOOLEAN_CONVERSION(G_TYPE_INT, int, -12345)
  CHECK_BOOLEAN_CONVERSION(G_TYPE_UINT, uint, 12345)
  CHECK_BOOLEAN_CONVERSION(G_TYPE_LONG, long, -12345678)
  CHECK_BOOLEAN_CONVERSION(G_TYPE_ULONG, ulong, 12345678)
  CHECK_BOOLEAN_CONVERSION(G_TYPE_INT64, int64, -12345678)
  CHECK_BOOLEAN_CONVERSION(G_TYPE_UINT64, uint64, 12345678)

#define CHECK_STRING_CONVERSION(int_type, setter, int_value)            \
  g_assert (g_value_type_transformable (int_type, G_TYPE_STRING));      \
  g_value_init (&src, int_type);                                        \
  g_value_init (&dest, G_TYPE_STRING);                                  \
  g_value_set_##setter (&src, int_value);                               \
  g_assert (g_value_transform (&src, &dest));                           \
  g_assert_cmpstr (g_value_get_string (&dest), ==, #int_value);         \
  g_value_unset (&src);                                                 \
  g_value_unset (&dest);

  CHECK_STRING_CONVERSION(G_TYPE_INT, int, -12345)
  CHECK_STRING_CONVERSION(G_TYPE_UINT, uint, 12345)
  CHECK_STRING_CONVERSION(G_TYPE_LONG, long, -12345678)
  CHECK_STRING_CONVERSION(G_TYPE_ULONG, ulong, 12345678)
  CHECK_STRING_CONVERSION(G_TYPE_INT64, int64, -12345678)
  CHECK_STRING_CONVERSION(G_TYPE_UINT64, uint64, 12345678)
  CHECK_STRING_CONVERSION(G_TYPE_FLOAT, float, 0.500000)
  CHECK_STRING_CONVERSION(G_TYPE_DOUBLE, double, -1.234567)

  g_assert (!g_value_type_transformable (G_TYPE_STRING, G_TYPE_CHAR));
  g_value_init (&src, G_TYPE_STRING);
  g_value_init (&dest, G_TYPE_CHAR);
  g_value_set_static_string (&src, "bla");
  g_value_set_char (&dest, 'c');
  g_assert (!g_value_transform (&src, &dest));
  g_assert_cmpint (g_value_get_char (&dest), ==, 'c');
  g_value_unset (&src);
  g_value_unset (&dest);
}

int
main (int argc, char *argv[])
{
  g_type_init ();
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/param/value", test_param_value);
  g_test_add_func ("/param/strings", test_param_strings);
  g_test_add_func ("/param/qdata", test_param_qdata);
  g_test_add_func ("/param/validate", test_param_validate);
  g_test_add_func ("/param/convert", test_param_convert);
  g_test_add_func ("/value/transform", test_value_transform);

  return g_test_run ();
}
