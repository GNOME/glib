#include <stdlib.h>
#include <gio.h>
#include <gstdio.h>

static void
test_basic (void)
{
  gchar *str = NULL;
  GSettings *settings;

  settings = g_settings_new ("org.gtk.test");

  g_settings_get (settings, "greeting", "s", &str);
  g_assert_cmpstr (str, ==, "Hello, earthlings");

  g_settings_set (settings, "greeting", "s", "goodbye world");
  g_settings_get (settings, "greeting", "s", &str);
  g_assert_cmpstr (str, ==, "goodbye world");

  if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR))
    {
      settings = g_settings_new ("org.gtk.test");
      g_settings_set (settings, "greeting", "i", 555);
      abort ();
    }
  g_test_trap_assert_failed ();
  g_test_trap_assert_stderr ("*correct_type*");

  g_settings_get (settings, "greeting", "s", &str);
  g_assert_cmpstr (str, ==, "goodbye world");
  g_settings_set (settings, "greeting", "s", "this is the end");
}

static void
test_unknown_key (void)
{
  if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR))
    {
      GSettings *settings;
      GVariant *value;

      settings = g_settings_new ("org.gtk.test");
      value = g_settings_get_value (settings, "no_such_key");

      g_object_unref (settings);
    }
  g_test_trap_assert_failed ();
  g_test_trap_assert_stderr ("*CRITICAL*");
}

void
test_no_schema (void)
{
  if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR))
    {
      GSettings *settings;

      settings = g_settings_new ("no.such.schema");
    }

  g_test_trap_assert_failed ();
  g_test_trap_assert_stderr ("*Settings schema 'no.such.schema' is not installed*");
}

static void
test_wrong_type (void)
{
  if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR))
    {
      GSettings *settings;
      gchar *str = NULL;

      settings = g_settings_new ("org.gtk.test");

      g_settings_get (settings, "greetings", "o", &str);
    }
  g_test_trap_assert_failed ();
  g_test_trap_assert_stderr ("*CRITICAL*");

  if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR))
    {
      GSettings *settings;
      gchar *str = NULL;

      settings = g_settings_new ("org.gtk.test");

      g_settings_set (settings, "greetings", "o", &str);
    }
  g_test_trap_assert_failed ();
  g_test_trap_assert_stderr ("*CRITICAL*");
}

static void
test_basic_types (void)
{
  GSettings *settings;
  gboolean b;
  guint8 byte;
  gint16 i16;
  guint16 u16;
  gint32 i32;
  guint32 u32;
  gint64 i64;
  guint64 u64;
  gdouble d;
  gchar *str;

  settings = g_settings_new ("org.gtk.test.basic-types");

  g_settings_get (settings, "test_boolean", "b", &b);
  g_assert_cmpint (b, ==, 1);

  g_settings_set (settings, "test_boolean", "b", 0);
  g_settings_get (settings, "test_boolean", "b", &b);
  g_assert_cmpint (b, ==, 0);

  g_settings_get (settings, "test_byte", "y", &byte);
  g_assert_cmpint (byte, ==, 25);

  g_settings_set (settings, "test_byte", "y", G_MAXUINT8);
  g_settings_get (settings, "test_byte", "y", &byte);
  g_assert_cmpint (byte, ==, G_MAXUINT8);

  g_settings_get (settings, "test_int16", "n", &i16);
  g_assert_cmpint (i16, ==, -1234);

  g_settings_set (settings, "test_int16", "n", G_MININT16);
  g_settings_get (settings, "test_int16", "n", &i16);
  g_assert_cmpint (i16, ==, G_MININT16);

  g_settings_set (settings, "test_int16", "n", G_MAXINT16);
  g_settings_get (settings, "test_int16", "n", &i16);
  g_assert_cmpint (i16, ==, G_MAXINT16);

  g_settings_get (settings, "test_uint16", "q", &u16);
  g_assert_cmpuint (u16, ==, 1234);

  g_settings_set (settings, "test_uint16", "q", G_MAXUINT16);
  g_settings_get (settings, "test_uint16", "q", &u16);
  g_assert_cmpuint (u16, ==, G_MAXUINT16);

  g_settings_get (settings, "test_int32", "i", &i32);
  g_assert_cmpint (i32, ==, -123456);

  g_settings_set (settings, "test_int32", "i", G_MININT32);
  g_settings_get (settings, "test_int32", "i", &i32);
  g_assert_cmpint (i32, ==, G_MININT32);

  g_settings_set (settings, "test_int32", "i", G_MAXINT32);
  g_settings_get (settings, "test_int32", "i", &i32);
  g_assert_cmpint (i32, ==, G_MAXINT32);

  g_settings_get (settings, "test_uint32", "u", &u32);
  g_assert_cmpuint (u32, ==, 123456);

  g_settings_set (settings, "test_uint32", "u", G_MAXUINT32);
  g_settings_get (settings, "test_uint32", "u", &u32);
  g_assert_cmpuint (u32, ==, G_MAXUINT32);

  g_settings_get (settings, "test_int64", "x", &i64);
  g_assert_cmpuint (i64, ==, -123456789);

  g_settings_set (settings, "test_int64", "x", G_MININT64);
  g_settings_get (settings, "test_int64", "x", &i64);
  g_assert_cmpuint (i64, ==, G_MININT64);

  g_settings_set (settings, "test_int64", "x", G_MAXINT64);
  g_settings_get (settings, "test_int64", "x", &i64);
  g_assert_cmpuint (i64, ==, G_MAXINT64);

  g_settings_get (settings, "test_uint64", "t", &u64);
  g_assert_cmpuint (u64, ==, 123456789);

  g_settings_set (settings, "test_uint64", "t", G_MAXUINT64);
  g_settings_get (settings, "test_uint64", "t", &u64);
  g_assert_cmpuint (u64, ==, G_MAXUINT64);

  g_settings_get (settings, "test_double", "d", &d);
  g_assert_cmpfloat (d, ==, 123.456);

  g_settings_set (settings, "test_double", "d", G_MINDOUBLE);
  g_settings_get (settings, "test_double", "d", &d);
  g_assert_cmpfloat (d, ==, G_MINDOUBLE);

  g_settings_set (settings, "test_double", "d", G_MAXDOUBLE);
  g_settings_get (settings, "test_double", "d", &d);
  g_assert_cmpfloat (d, ==, G_MAXDOUBLE);

  g_settings_get (settings, "test_string", "s", &str);
  g_assert_cmpstr (str, ==, "a string, it seems");

  g_settings_get (settings, "test_objectpath", "o", &str);
  g_assert_cmpstr (str, ==, "/a/object/path");
}

static void
test_complex_types (void)
{
  GSettings *settings;
  gchar *s;
  gint i1, i2;
  GVariantIter *iter = NULL;

  settings = g_settings_new ("org.gtk.test.complex-types");

  g_settings_get (settings, "test_tuple", "(s(ii))", &s, &i1, &i2);
  g_assert_cmpstr (s, ==, "one");
  g_assert_cmpint (i1,==, 2);
  g_assert_cmpint (i2,==, 3);

  g_settings_set (settings, "test_tuple", "(s(ii))", "none", 0, 0);
  g_settings_get (settings, "test_tuple", "(s(ii))", &s, &i1, &i2);
  g_assert_cmpstr (s, ==, "none");
  g_assert_cmpint (i1,==, 0);
  g_assert_cmpint (i2,==, 0);

  g_settings_get (settings, "test_array", "ai", &iter);
  g_assert_cmpint (g_variant_iter_n_children (iter), ==, 6);
  g_assert (g_variant_iter_next (iter, "i", &i1));
  g_assert_cmpint (i1, ==, 0);
  g_assert (g_variant_iter_next (iter, "i", &i1));
  g_assert_cmpint (i1, ==, 1);
  g_assert (g_variant_iter_next (iter, "i", &i1));
  g_assert_cmpint (i1, ==, 2);
  g_assert (g_variant_iter_next (iter, "i", &i1));
  g_assert_cmpint (i1, ==, 3);
  g_assert (g_variant_iter_next (iter, "i", &i1));
  g_assert_cmpint (i1, ==, 4);
  g_assert (g_variant_iter_next (iter, "i", &i1));
  g_assert_cmpint (i1, ==, 5);
  g_assert (!g_variant_iter_next (iter, "i", &i1));
  g_variant_iter_free (iter);
}

int
main (int argc, char *argv[])
{
  g_setenv ("GSETTINGS_SCHEMA_DIR", ".", TRUE);
  g_setenv ("GSETTINGS_BACKEND", "memory", TRUE);
  g_setenv ("GSETTINGS_MEMORY_BACKEND_STORE", "./store", TRUE);

  g_remove ("./store");

  g_type_init ();
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/gsettings/basic", test_basic);
  g_test_add_func ("/gsettings/no-schema", test_no_schema);
  g_test_add_func ("/gsettings/unknown-key", test_unknown_key);
  g_test_add_func ("/gsettings/wrong-type", test_wrong_type);
  g_test_add_func ("/gsettings/basic-types", test_basic_types);
  g_test_add_func ("/gsettings/complex-types", test_complex_types);

  return g_test_run ();
}
