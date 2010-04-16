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
  g_free (str);
  str = NULL;

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
  g_free (str);
  str = NULL;

  g_settings_set (settings, "greeting", "s", "this is the end");
  g_object_unref (settings);
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

      g_assert (value == NULL);

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

      g_assert (settings == NULL);
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

      g_assert (str == NULL);
    }
  g_test_trap_assert_failed ();
  g_test_trap_assert_stderr ("*CRITICAL*");

  if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR))
    {
      GSettings *settings;

      settings = g_settings_new ("org.gtk.test");

      g_settings_set (settings, "greetings", "o", "/a/path");
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

  g_settings_get (settings, "test-boolean", "b", &b);
  g_assert_cmpint (b, ==, 1);

  g_settings_set (settings, "test-boolean", "b", 0);
  g_settings_get (settings, "test-boolean", "b", &b);
  g_assert_cmpint (b, ==, 0);

  g_settings_get (settings, "test-byte", "y", &byte);
  g_assert_cmpint (byte, ==, 25);

  g_settings_set (settings, "test-byte", "y", G_MAXUINT8);
  g_settings_get (settings, "test-byte", "y", &byte);
  g_assert_cmpint (byte, ==, G_MAXUINT8);

  g_settings_get (settings, "test-int16", "n", &i16);
  g_assert_cmpint (i16, ==, -1234);

  g_settings_set (settings, "test-int16", "n", G_MININT16);
  g_settings_get (settings, "test-int16", "n", &i16);
  g_assert_cmpint (i16, ==, G_MININT16);

  g_settings_set (settings, "test-int16", "n", G_MAXINT16);
  g_settings_get (settings, "test-int16", "n", &i16);
  g_assert_cmpint (i16, ==, G_MAXINT16);

  g_settings_get (settings, "test-uint16", "q", &u16);
  g_assert_cmpuint (u16, ==, 1234);

  g_settings_set (settings, "test-uint16", "q", G_MAXUINT16);
  g_settings_get (settings, "test-uint16", "q", &u16);
  g_assert_cmpuint (u16, ==, G_MAXUINT16);

  g_settings_get (settings, "test-int32", "i", &i32);
  g_assert_cmpint (i32, ==, -123456);

  g_settings_set (settings, "test-int32", "i", G_MININT32);
  g_settings_get (settings, "test-int32", "i", &i32);
  g_assert_cmpint (i32, ==, G_MININT32);

  g_settings_set (settings, "test-int32", "i", G_MAXINT32);
  g_settings_get (settings, "test-int32", "i", &i32);
  g_assert_cmpint (i32, ==, G_MAXINT32);

  g_settings_get (settings, "test-uint32", "u", &u32);
  g_assert_cmpuint (u32, ==, 123456);

  g_settings_set (settings, "test-uint32", "u", G_MAXUINT32);
  g_settings_get (settings, "test-uint32", "u", &u32);
  g_assert_cmpuint (u32, ==, G_MAXUINT32);

  g_settings_get (settings, "test-int64", "x", &i64);
  g_assert_cmpuint (i64, ==, -123456789);

  g_settings_set (settings, "test-int64", "x", G_MININT64);
  g_settings_get (settings, "test-int64", "x", &i64);
  g_assert_cmpuint (i64, ==, G_MININT64);

  g_settings_set (settings, "test-int64", "x", G_MAXINT64);
  g_settings_get (settings, "test-int64", "x", &i64);
  g_assert_cmpuint (i64, ==, G_MAXINT64);

  g_settings_get (settings, "test-uint64", "t", &u64);
  g_assert_cmpuint (u64, ==, 123456789);

  g_settings_set (settings, "test-uint64", "t", G_MAXUINT64);
  g_settings_get (settings, "test-uint64", "t", &u64);
  g_assert_cmpuint (u64, ==, G_MAXUINT64);

  g_settings_get (settings, "test-double", "d", &d);
  g_assert_cmpfloat (d, ==, 123.456);

  g_settings_set (settings, "test-double", "d", G_MINDOUBLE);
  g_settings_get (settings, "test-double", "d", &d);
  g_assert_cmpfloat (d, ==, G_MINDOUBLE);

  g_settings_set (settings, "test-double", "d", G_MAXDOUBLE);
  g_settings_get (settings, "test-double", "d", &d);
  g_assert_cmpfloat (d, ==, G_MAXDOUBLE);

  g_settings_get (settings, "test-string", "s", &str);
  g_assert_cmpstr (str, ==, "a string, it seems");
  g_free (str);
  str = NULL;

  g_settings_get (settings, "test-objectpath", "o", &str);
  g_assert_cmpstr (str, ==, "/a/object/path");
  g_object_unref (settings);
  g_free (str);
  str = NULL;
}

static void
test_complex_types (void)
{
  GSettings *settings;
  gchar *s;
  gint i1, i2;
  GVariantIter *iter = NULL;

  settings = g_settings_new ("org.gtk.test.complex-types");

  g_settings_get (settings, "test-tuple", "(s(ii))", &s, &i1, &i2);
  g_assert_cmpstr (s, ==, "one");
  g_assert_cmpint (i1,==, 2);
  g_assert_cmpint (i2,==, 3);
  g_free (s) ;
  s = NULL;

  g_settings_set (settings, "test-tuple", "(s(ii))", "none", 0, 0);
  g_settings_get (settings, "test-tuple", "(s(ii))", &s, &i1, &i2);
  g_assert_cmpstr (s, ==, "none");
  g_assert_cmpint (i1,==, 0);
  g_assert_cmpint (i2,==, 0);
  g_free (s);
  s = NULL;

  g_settings_get (settings, "test-array", "ai", &iter);
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

  g_object_unref (settings);
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

  g_object_unref (settings2);
  g_object_unref (settings);
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

  g_settings_delay (settings);

  g_settings_set (settings, "greeting", "s", "greetings from test_delay_apply");

  g_assert (changed_cb_called);
  g_assert (!changed_cb_called2);

  g_settings_get (settings, "greeting", "s", &str);
  g_assert_cmpstr (str, ==, "greetings from test_delay_apply");
  g_free (str);
  str = NULL;

  g_settings_get (settings2, "greeting", "s", &str);
  g_assert_cmpstr (str, ==, "top o' the morning");
  g_free (str);
  str = NULL;

  g_assert (g_settings_get_has_unapplied (settings));
  g_assert (!g_settings_get_has_unapplied (settings2));

  changed_cb_called = FALSE;
  changed_cb_called2 = FALSE;

  g_settings_apply (settings);

  g_assert (!changed_cb_called);
  g_assert (changed_cb_called2);

  g_settings_get (settings, "greeting", "s", &str);
  g_assert_cmpstr (str, ==, "greetings from test_delay_apply");
  g_free (str);
  str = NULL;

  g_settings_get (settings2, "greeting", "s", &str);
  g_assert_cmpstr (str, ==, "greetings from test_delay_apply");
  g_free (str);
  str = NULL;

  g_assert (!g_settings_get_has_unapplied (settings));
  g_assert (!g_settings_get_has_unapplied (settings2));

  g_object_unref (settings2);
  g_object_unref (settings);
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

  g_settings_delay (settings);

  g_settings_set (settings, "greeting", "s", "greetings from test_delay_revert");

  g_settings_get (settings, "greeting", "s", &str);
  g_assert_cmpstr (str, ==, "greetings from test_delay_revert");
  g_free (str);
  str = NULL;

  g_settings_get (settings2, "greeting", "s", &str);
  g_assert_cmpstr (str, ==, "top o' the morning");
  g_free (str);
  str = NULL;

  g_settings_revert (settings);

  g_settings_get (settings, "greeting", "s", &str);
  g_assert_cmpstr (str, ==, "top o' the morning");
  g_free (str);
  str = NULL;

  g_settings_get (settings2, "greeting", "s", &str);
  g_assert_cmpstr (str, ==, "top o' the morning");
  g_free (str);
  str = NULL;

  g_object_unref (settings2);
  g_object_unref (settings);
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
  g_free (str);
  str = NULL;

  g_settings_get (settings, "farewell", "s", &str);
  g_assert_cmpstr (str, ==, "atomic bye-bye");
  g_free (str);
  str = NULL;
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

  g_signal_connect (settings2, "change-event",
                    G_CALLBACK (keys_changed_cb), NULL);

  g_settings_delay (settings);

  g_settings_set (settings, "greeting", "s", "greetings from test_atomic");
  g_settings_set (settings, "farewell", "s", "atomic bye-bye");

  g_settings_apply (settings);

  g_settings_get (settings, "greeting", "s", &str);
  g_assert_cmpstr (str, ==, "greetings from test_atomic");
  g_free (str);
  str = NULL;

  g_settings_get (settings, "farewell", "s", &str);
  g_assert_cmpstr (str, ==, "atomic bye-bye");
  g_free (str);
  str = NULL;

  g_settings_get (settings2, "greeting", "s", &str);
  g_assert_cmpstr (str, ==, "greetings from test_atomic");
  g_free (str);
  str = NULL;

  g_settings_get (settings2, "farewell", "s", &str);
  g_assert_cmpstr (str, ==, "atomic bye-bye");
  g_free (str);
  str = NULL;

  g_object_unref (settings2);
  g_object_unref (settings);
}

static gboolean
glib_translations_work (void)
{
  gchar *locale;
  gchar *orig = "Unnamed";
  gchar *str;

  locale = g_strdup (setlocale (LC_MESSAGES, NULL));
  setlocale (LC_MESSAGES, "de");
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
  g_settings_get (settings, "error-message", "s", &str);
  setlocale (LC_MESSAGES, locale);

  g_assert_cmpstr (str, ==, "Unnamed");
  g_free (str);
  str = NULL;

  setlocale (LC_MESSAGES, "de");
  g_settings_get (settings, "error-message", "s", &str);
  setlocale (LC_MESSAGES, locale);

  g_assert_cmpstr (str, ==, "Unbenannt");
  g_object_unref (settings);
  g_free (str);
  str = NULL;

  g_free (locale);
}

enum
{
  PROP_0,
  PROP_BOOL,
  PROP_INT,
  PROP_DOUBLE,
  PROP_STRING
};

typedef struct
{
  GObject parent_instance;

  gboolean bool_prop;
  gint int_prop;
  gdouble double_prop;
  gchar *string_prop;
} TestObject;

typedef struct
{
  GObjectClass parent_class;
} TestObjectClass;

G_DEFINE_TYPE (TestObject, test_object, G_TYPE_OBJECT)

static void
test_object_init (TestObject *object)
{
}

static void
test_object_finalize (GObject *object)
{
  TestObject *testo = (TestObject*)object;
  g_free (testo->string_prop);
  G_OBJECT_CLASS (test_object_parent_class)->finalize (object);
}

static void
test_object_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  TestObject *test_object = (TestObject *)object;

  switch (prop_id)
    {
    case PROP_BOOL:
      g_value_set_boolean (value, test_object->bool_prop);
      break;
    case PROP_INT:
      g_value_set_int (value, test_object->int_prop);
      break;
    case PROP_DOUBLE:
      g_value_set_double (value, test_object->double_prop);
      break;
    case PROP_STRING:
      g_value_set_string (value, test_object->string_prop);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
test_object_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  TestObject *test_object = (TestObject *)object;

  switch (prop_id)
    {
    case PROP_BOOL:
      test_object->bool_prop = g_value_get_boolean (value);
      break;
    case PROP_INT:
      test_object->int_prop = g_value_get_int (value);
      break;
    case PROP_DOUBLE:
      test_object->double_prop = g_value_get_double (value);
      break;
    case PROP_STRING:
      g_free (test_object->string_prop);
      test_object->string_prop = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
test_object_class_init (TestObjectClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  gobject_class->get_property = test_object_get_property;
  gobject_class->set_property = test_object_set_property;
  gobject_class->finalize = test_object_finalize;

  g_object_class_install_property (gobject_class, PROP_BOOL,
    g_param_spec_boolean ("bool", "", "", FALSE, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_INT,
    g_param_spec_int ("int", "", "", -G_MAXINT, G_MAXINT, 0, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_DOUBLE,
    g_param_spec_double ("double", "", "", -G_MAXDOUBLE, G_MAXDOUBLE, 0.0, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class, PROP_STRING,
    g_param_spec_string ("string", "", "", NULL, G_PARAM_READWRITE));
}

static TestObject *
test_object_new (void)
{
  return (TestObject*)g_object_new (test_object_get_type (), NULL);
}

static void
test_simple_binding (void)
{
  TestObject *obj;
  GSettings *settings;
  gboolean b;
  gint i;
  gdouble d;
  gchar *s;

  settings = g_settings_new ("org.gtk.test.binding");
  obj = test_object_new ();

  g_settings_bind (settings, "bool", obj, "bool", G_SETTINGS_BIND_DEFAULT);

  g_object_set (obj, "bool", TRUE, NULL);
  g_assert_cmpint (g_settings_get_boolean (settings, "bool"), ==, TRUE);

  g_settings_set_boolean (settings, "bool", FALSE);
  g_object_get (obj, "bool", &b, NULL);
  g_assert_cmpint (b, ==, FALSE);

  g_settings_bind (settings, "int", obj, "int", G_SETTINGS_BIND_DEFAULT);

  g_object_set (obj, "int", 12345, NULL);
  g_assert_cmpint (g_settings_get_int (settings, "int"), ==, 12345);

  g_settings_set_int (settings, "int", 54321);
  g_object_get (obj, "int", &i, NULL);
  g_assert_cmpint (i, ==, 54321);

  g_settings_bind (settings, "string", obj, "string", G_SETTINGS_BIND_DEFAULT);

  g_object_set (obj, "string", "bu ba", NULL);
  s = g_settings_get_string (settings, "string");
  g_assert_cmpstr (s, ==, "bu ba");
  g_free (s);

  g_settings_set_string (settings, "string", "bla bla");
  g_object_get (obj, "string", &s, NULL);
  g_assert_cmpstr (s, ==, "bla bla");
  g_free (s);

  g_settings_bind (settings, "double", obj, "double", G_SETTINGS_BIND_DEFAULT);

  g_object_set (obj, "double", 203e7, NULL);
  g_assert_cmpfloat (g_settings_get_double (settings, "double"), ==, 203e7);

  g_settings_set_double (settings, "double", 207e3);
  g_object_get (obj, "double", &d, NULL);
  g_assert_cmpfloat (d, ==, 207e3);

  g_object_unref (obj);
  g_object_unref (settings);
}

static void
test_directional_binding (void)
{
  TestObject *obj;
  GSettings *settings;
  gboolean b;
  gint i;

  settings = g_settings_new ("org.gtk.test.binding");
  obj = test_object_new ();

  g_object_set (obj, "bool", FALSE, NULL);
  g_settings_set_boolean (settings, "bool", FALSE);

  g_settings_bind (settings, "bool", obj, "bool", G_SETTINGS_BIND_GET);

  g_settings_set_boolean (settings, "bool", TRUE);
  g_object_get (obj, "bool", &b, NULL);
  g_assert_cmpint (b, ==, TRUE);

  g_object_set (obj, "bool", FALSE, NULL);
  g_assert_cmpint (g_settings_get_boolean (settings, "bool"), ==, TRUE);

  g_object_set (obj, "int", 20, NULL);
  g_settings_set_int (settings, "int", 20);

  g_settings_bind (settings, "int", obj, "int", G_SETTINGS_BIND_SET);

  g_object_set (obj, "int", 32, NULL);
  g_assert_cmpint (g_settings_get_int (settings, "int"), ==, 32);

  g_settings_set_int (settings, "int", 20);
  g_object_get (obj, "int", &i, NULL);
  g_assert_cmpint (i, ==, 32);

  g_object_unref (obj);
  g_object_unref (settings);
}

static void
test_typesafe_binding (void)
{
  if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR))
    {
      TestObject *obj;
      GSettings *settings;

      settings = g_settings_new ("org.gtk.test.binding");
      obj = test_object_new ();

      g_settings_bind (settings, "string", obj, "int", G_SETTINGS_BIND_DEFAULT);

      g_object_unref (obj);
      g_object_unref (settings);
    }
  g_test_trap_assert_failed ();
  g_test_trap_assert_stderr ("*not compatible*");
}

static gboolean
string_to_bool (GValue   *value,
                GVariant *variant,
                gpointer  user_data)
{
  const gchar *s;

  s = g_variant_get_string (variant, NULL);
  g_value_set_boolean (value, g_strcmp0 (s, "true") == 0);

  return TRUE;
}

static GVariant *
bool_to_string (const GValue       *value,
                const GVariantType *expected_type,
                gpointer            user_data)
{
  if (g_value_get_boolean (value))
    return g_variant_new_string ("true");
  else
    return g_variant_new_string ("false");
}

static void
test_custom_binding (void)
{
  TestObject *obj;
  GSettings *settings;
  gchar *s;
  gboolean b;

  settings = g_settings_new ("org.gtk.test.binding");
  obj = test_object_new ();

  g_settings_set_string (settings, "string", "true");

  g_settings_bind_with_mapping (settings, "string",
                                obj, "bool",
                                G_SETTINGS_BIND_DEFAULT,
                                string_to_bool,
                                bool_to_string,
                                NULL, NULL);

  g_settings_set_string (settings, "string", "false");
  g_object_get (obj, "bool", &b, NULL);
  g_assert_cmpint (b, ==, FALSE);

  g_settings_set_string (settings, "string", "not true");
  g_object_get (obj, "bool", &b, NULL);
  g_assert_cmpint (b, ==, FALSE);

  g_object_set (obj, "bool", TRUE, NULL);
  s = g_settings_get_string (settings, "string");
  g_assert_cmpstr (s, ==, "true");

  g_object_unref (obj);
  g_object_unref (settings);
}

int
main (int argc, char *argv[])
{
  g_setenv ("GSETTINGS_SCHEMA_DIR", ".", TRUE);
  g_setenv ("GSETTINGS_BACKEND", "keyfile", TRUE);
  g_setenv ("GSETTINGS_KEYFILE_BACKEND_STORE", "./store", TRUE);

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
  g_test_add_func ("/gsettings/simple-binding", test_simple_binding);
  g_test_add_func ("/gsettings/directional-binding", test_directional_binding);
  g_test_add_func ("/gsettings/typesafe-binding", test_typesafe_binding);
  g_test_add_func ("/gsettings/custom-binding", test_custom_binding);

  return g_test_run ();
}
