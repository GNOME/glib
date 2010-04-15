#include <stdlib.h>
#include <locale.h>
#include <libintl.h>
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
  g_test_trap_assert_stderr ("*does not contain*");
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

      g_settings_get (settings, "greeting", "o", &str);
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

static gboolean changed_cb_called;

static void
changed_cb (GSettings   *settings,
            const gchar *key,
            gpointer     data)
{
  changed_cb_called = TRUE;

  g_assert_cmpstr (key, ==, data);
}

void
test_changes (void)
{
  GSettings *settings;
  GSettings *settings2;

  settings = g_settings_new ("org.gtk.test");

  g_signal_connect (settings, "changed",
                    G_CALLBACK (changed_cb), "greeting");

  changed_cb_called = FALSE;

  g_settings_set (settings, "greeting", "s", "new greeting");
  g_assert (changed_cb_called);

  settings2 = g_settings_new ("org.gtk.test");

  changed_cb_called = FALSE;

  g_settings_set (settings2, "greeting", "s", "hi");
  g_assert (changed_cb_called);

}

static gboolean changed_cb_called2;

static void
changed_cb2 (GSettings   *settings,
             const gchar *key,
             gpointer     data)
{
  gboolean *p = data;

  *p = TRUE;
}


void
test_delay_apply (void)
{
  GSettings *settings;
  GSettings *settings2;
  gchar *str;

  settings = g_settings_new ("org.gtk.test");
  settings2 = g_settings_new ("org.gtk.test");

  g_settings_set (settings2, "greeting", "s", "top o' the morning");

  changed_cb_called = FALSE;
  changed_cb_called2 = FALSE;

  g_signal_connect (settings, "changed",
                    G_CALLBACK (changed_cb2), &changed_cb_called);
  g_signal_connect (settings2, "changed",
                    G_CALLBACK (changed_cb2), &changed_cb_called2);

  g_settings_set_delay_apply (settings, TRUE);

  g_assert (g_settings_get_delay_apply (settings));
  g_assert (!g_settings_get_delay_apply (settings2));

  g_settings_set (settings, "greeting", "s", "greetings from test_delay_apply");

  g_assert (changed_cb_called);
  g_assert (!changed_cb_called2);

  g_settings_get (settings, "greeting", "s", &str);
  g_assert_cmpstr (str, ==, "greetings from test_delay_apply");

  g_settings_get (settings2, "greeting", "s", &str);
  g_assert_cmpstr (str, ==, "top o' the morning");

  g_assert (g_settings_get_has_unapplied (settings));
  g_assert (!g_settings_get_has_unapplied (settings2));

  changed_cb_called = FALSE;
  changed_cb_called2 = FALSE;

  g_settings_apply (settings);

  g_assert (!changed_cb_called);
  g_assert (changed_cb_called2);

  g_settings_get (settings, "greeting", "s", &str);
  g_assert_cmpstr (str, ==, "greetings from test_delay_apply");

  g_settings_get (settings2, "greeting", "s", &str);
  g_assert_cmpstr (str, ==, "greetings from test_delay_apply");

  g_assert (!g_settings_get_has_unapplied (settings));
  g_assert (!g_settings_get_has_unapplied (settings2));
}

static void
test_delay_revert (void)
{
  GSettings *settings;
  GSettings *settings2;
  gchar *str;

  settings = g_settings_new ("org.gtk.test");
  settings2 = g_settings_new ("org.gtk.test");

  g_settings_set (settings2, "greeting", "s", "top o' the morning");

  g_settings_set_delay_apply (settings, TRUE);

  g_settings_set (settings, "greeting", "s", "greetings from test_delay_revert");

  g_settings_get (settings, "greeting", "s", &str);
  g_assert_cmpstr (str, ==, "greetings from test_delay_revert");

  g_settings_get (settings2, "greeting", "s", &str);
  g_assert_cmpstr (str, ==, "top o' the morning");

  g_settings_revert (settings);

  g_settings_get (settings, "greeting", "s", &str);
  g_assert_cmpstr (str, ==, "top o' the morning");

  g_settings_get (settings2, "greeting", "s", &str);
  g_assert_cmpstr (str, ==, "top o' the morning");
}

static void
keys_changed_cb (GSettings    *settings,
                 const GQuark *keys,
                 gint          n_keys)
{
  gchar *str;

  g_assert_cmpint (n_keys, ==, 2);

  g_assert ((keys[0] == g_quark_from_static_string ("greeting") &&
             keys[1] == g_quark_from_static_string ("farewell")) ||
            (keys[1] == g_quark_from_static_string ("greeting") &&
             keys[0] == g_quark_from_static_string ("farewell")));

  g_settings_get (settings, "greeting", "s", &str);
  g_assert_cmpstr (str, ==, "greetings from test_atomic");

  g_settings_get (settings, "farewell", "s", &str);
  g_assert_cmpstr (str, ==, "atomic bye-bye");
}

static void
test_atomic (void)
{
  GSettings *settings;
  GSettings *settings2;
  gchar *str;

  settings = g_settings_new ("org.gtk.test");
  settings2 = g_settings_new ("org.gtk.test");

  g_settings_set (settings2, "greeting", "s", "top o' the morning");

  changed_cb_called = FALSE;
  changed_cb_called2 = FALSE;

  g_signal_connect (settings2, "keys-changed",
                    G_CALLBACK (keys_changed_cb), NULL);

  g_settings_set_delay_apply (settings, TRUE);

  g_settings_set (settings, "greeting", "s", "greetings from test_atomic");
  g_settings_set (settings, "farewell", "s", "atomic bye-bye");

  g_settings_apply (settings);

  g_settings_get (settings, "greeting", "s", &str);
  g_assert_cmpstr (str, ==, "greetings from test_atomic");

  g_settings_get (settings, "farewell", "s", &str);
  g_assert_cmpstr (str, ==, "atomic bye-bye");

  g_settings_get (settings2, "greeting", "s", &str);
  g_assert_cmpstr (str, ==, "greetings from test_atomic");

  g_settings_get (settings2, "farewell", "s", &str);
  g_assert_cmpstr (str, ==, "atomic bye-bye");
}

static gboolean
glib_translations_work (void)
{
  gchar *locale;
  gchar *orig = "Unnamed";
  gchar *str;

  locale = g_strdup (setlocale (LC_MESSAGES, NULL));
  setlocale (LC_MESSAGES, "de_DE");
  str = dgettext ("glib20", orig);
  setlocale (LC_MESSAGES, locale);
  g_free (locale);

  return str != orig;
}

static void
test_l10n (void)
{
  GSettings *settings;
  gchar *str;
  gchar *locale;

  if (!glib_translations_work ())
    {
      g_test_message ("Skipping localization tests because translations don't work");
      return;
    }

  bind_textdomain_codeset ("glib20", "UTF-8");

  locale = g_strdup (setlocale (LC_MESSAGES, NULL));

  settings = g_settings_new ("org.gtk.test.localized");

  setlocale (LC_MESSAGES, "C");
  g_settings_get (settings, "error_message", "s", &str);
  setlocale (LC_MESSAGES, locale);

  g_assert_cmpstr (str, ==, "Unnamed");
  str = NULL;

  setlocale (LC_MESSAGES, "de_DE");
  g_settings_get (settings, "error_message", "s", &str);
  setlocale (LC_MESSAGES, locale);

  g_assert_cmpstr (str, ==, "Unbenannt");

  g_free (locale);
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
  g_test_add_func ("/gsettings/changes", test_changes);
  g_test_add_func ("/gsettings/l10n", test_l10n);
  g_test_add_func ("/gsettings/delay-apply", test_delay_apply);
  g_test_add_func ("/gsettings/delay-revert", test_delay_revert);
  g_test_add_func ("/gsettings/atomic", test_atomic);

  return g_test_run ();
}
