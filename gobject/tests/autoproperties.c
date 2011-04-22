#include <stdlib.h>
#include <glib-object.h>

/* XXX: yuck, but a lot of the functions are macro-ized */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-prototypes"

typedef struct _TestObject              TestObject;
typedef struct _TestObjectPrivate       TestObjectPrivate;
typedef struct _TestObjectClass         TestObjectClass;

typedef enum {
  TEST_ENUM_VALUE_FOO = -1,
  TEST_ENUM_VALUE_BAR =  0,
  TEST_ENUM_VALUE_BAZ =  1
} TestEnumValue;

typedef enum {
  TEST_FLAGS_VALUE_FOO = 0,
  TEST_FLAGS_VALUE_BAR = 1 << 0,
  TEST_FLAGS_VALUE_BAZ = 1 << 1
} TestFlagsValue;

typedef struct {
  int x, y, width, height;

  int ref_count;
} TestBoxed;

struct _TestObject
{
  GObject parent_instance;

  TestObjectPrivate *priv;
};

struct _TestObjectClass
{
  GObjectClass parent_class;
};

struct _TestObjectPrivate
{
  gint dummy;

  gint foo;

  gboolean bar;

  gchar *str;
  gboolean str_set;

  gint8 single_byte;
  gint16 double_byte;
  gint32 four_bytes;

  float width;
  double x_align;

  TestEnumValue enum_value;
  TestFlagsValue flags_value;

  TestBoxed *boxed;
};

enum
{
  PROP_0,

  PROP_FOO,
  PROP_BAR,
  PROP_STR,
  PROP_STR_SET,
  PROP_BAZ,
  PROP_SINGLE_BYTE,
  PROP_DOUBLE_BYTE,
  PROP_FOUR_BYTES,
  PROP_WIDTH,
  PROP_X_ALIGN,
  PROP_ENUM_VALUE,
  PROP_FLAGS_VALUE,
  PROP_BOXED,

  LAST_PROP
};

GType test_enum_value_get_type (void); /* for -Wmissing-prototypes */

GType
test_enum_value_get_type (void)
{
  static volatile gsize g_define_type_id__volatile = 0;

  if (g_once_init_enter (&g_define_type_id__volatile))
    {
      static const GEnumValue values[] = {
        { TEST_ENUM_VALUE_FOO, "TEST_ENUM_VALUE_FOO", "foo" },
        { TEST_ENUM_VALUE_BAR, "TEST_ENUM_VALUE_BAR", "bar" },
        { TEST_ENUM_VALUE_BAZ, "TEST_ENUM_VALUE_BAZ", "baz" },
        { 0, NULL, NULL }
      };
      GType g_define_type_id =
        g_enum_register_static (g_intern_static_string ("TestEnumValue"), values);
      g_once_init_leave (&g_define_type_id__volatile, g_define_type_id);
    }

  return g_define_type_id__volatile;
}

GType test_flags_value_get_type (void); /* for -Wmissing-prototypes */

GType
test_flags_value_get_type (void)
{
  static volatile gsize g_define_type_id__volatile = 0;

  if (g_once_init_enter (&g_define_type_id__volatile))
    {
      static const GFlagsValue values[] = {
        { TEST_FLAGS_VALUE_FOO, "TEST_FLAGS_VALUE_FOO", "foo" },
        { TEST_FLAGS_VALUE_BAR, "TEST_FLAGS_VALUE_BAR", "bar" },
        { TEST_FLAGS_VALUE_BAZ, "TEST_FLAGS_VALUE_BAZ", "baz" },
        { 0, NULL, NULL }
      };
      GType g_define_type_id =
        g_flags_register_static (g_intern_static_string ("TestFlagsValue"), values);
      g_once_init_leave (&g_define_type_id__volatile, g_define_type_id);
    }

  return g_define_type_id__volatile;
}

static TestBoxed *
test_boxed_new (int x,
                int y,
                int width,
                int height)
{
  TestBoxed *retval = g_new (TestBoxed, 1);

  retval->x = x;
  retval->y = y;
  retval->width = width;
  retval->height = height;
  retval->ref_count = 1;

  return retval;
}

static gpointer
test_boxed_copy (gpointer data)
{
  if (data != NULL)
    {
      TestBoxed *boxed = data;

      if (g_test_verbose ())
        g_print ("*** copy of boxed %p (ref count: %d) ***\n", boxed, boxed->ref_count);

      if (boxed->ref_count < 0)
        return test_boxed_new (boxed->x, boxed->y, boxed->width, boxed->height);

      boxed->ref_count += 1;
    }

  return data;
}

static void
test_boxed_free (gpointer data)
{
  if (data != NULL)
    {
      TestBoxed *boxed = data;

      if (g_test_verbose ())
        g_print ("*** free of boxed %p (ref count: %d) ***\n", boxed, boxed->ref_count);

      if (boxed->ref_count < 0)
        return;

      boxed->ref_count -= 1;

      if (boxed->ref_count == 0)
        g_free (boxed);
    }
}

GType test_boxed_get_type (void); /* for -Wmissing-prototypes */

G_DEFINE_BOXED_TYPE (TestBoxed, test_boxed, test_boxed_copy, test_boxed_free)

static GParamSpec *test_object_properties[LAST_PROP] = { NULL, };

GType test_object_get_type (void); /* for -Wmissing-prototypes */

G_DEFINE_TYPE (TestObject, test_object, G_TYPE_OBJECT)

G_DEFINE_PROPERTY_GET_SET (TestObject, test_object, int, foo)
G_DEFINE_PROPERTY_GET_SET (TestObject, test_object, gboolean, bar)
G_DEFINE_PROPERTY_GET (TestObject, test_object, gboolean, str_set)
G_DEFINE_PROPERTY_GET_SET (TestObject, test_object, gint8, single_byte)
G_DEFINE_PROPERTY_GET_SET (TestObject, test_object, gint16, double_byte)
G_DEFINE_PROPERTY_GET_SET (TestObject, test_object, gint32, four_bytes)
G_DEFINE_PROPERTY_GET_SET (TestObject, test_object, float, width)
G_DEFINE_PROPERTY_GET_SET (TestObject, test_object, double, x_align)
G_DEFINE_PROPERTY_GET_SET (TestObject, test_object, TestEnumValue, enum_value)
G_DEFINE_PROPERTY_GET_SET (TestObject, test_object, TestFlagsValue, flags_value)

G_DEFINE_PROPERTY_SET (TestObject, test_object, const TestBoxed *, boxed)

void
test_object_get_boxed (TestObject *self,
                       TestBoxed  *value)
{
  TestBoxed *boxed;

  g_property_get (G_PROPERTY (test_object_properties[PROP_BOXED]), self, &boxed);

  /* make sure that g_property_get() didn't copy/ref the pointer */
  g_assert (boxed == self->priv->boxed);
  g_assert_cmpint (boxed->ref_count, ==, self->priv->boxed->ref_count);

  *value = *boxed;
}

gboolean
test_object_set_str (TestObject  *self,
                     const gchar *value)
{
  TestObjectPrivate *priv;

  g_return_val_if_fail (G_TYPE_CHECK_INSTANCE_TYPE (self, test_object_get_type ()), FALSE);

  priv = self->priv;

  if (g_strcmp0 (priv->str, value) == 0)
    return FALSE;

  g_free (priv->str);
  priv->str = g_strdup (value);

  if (priv->str != NULL)
    priv->str_set = TRUE;
  else
    priv->str_set = FALSE;

  g_object_notify_by_pspec (G_OBJECT (self), test_object_properties[PROP_STR_SET]);

  return TRUE;
}

G_DEFINE_PROPERTY_GET (TestObject, test_object, const gchar *, str);

static void
test_object_finalize (GObject *gobject)
{
  TestObjectPrivate *priv = ((TestObject *) gobject)->priv;

  test_boxed_free (priv->boxed);
  g_free (priv->str);

  G_OBJECT_CLASS (test_object_parent_class)->finalize (gobject);
}

static void
test_object_class_init (TestObjectClass *klass)
{
  G_OBJECT_CLASS (klass)->finalize = test_object_finalize;

  g_type_class_add_private (klass, sizeof (TestObjectPrivate));

  test_object_properties[PROP_FOO] =
    g_int_property_new ("foo", G_PROPERTY_READWRITE,
                        G_STRUCT_OFFSET (TestObjectPrivate, foo),
                        NULL, NULL);
  g_property_set_range (G_PROPERTY (test_object_properties[PROP_FOO]), -1, 100);
  g_property_set_default (G_PROPERTY (test_object_properties[PROP_FOO]), 50);

  test_object_properties[PROP_BAR] =
    g_boolean_property_new ("bar", G_PROPERTY_READWRITE,
                            G_STRUCT_OFFSET (TestObjectPrivate, bar),
                            NULL, NULL);

  test_object_properties[PROP_STR] =
    g_string_property_new ("str", G_PROPERTY_READWRITE,
                           G_STRUCT_OFFSET (TestObjectPrivate, str),
                           (GPropertyStringSet) test_object_set_str,
                           NULL);

  test_object_properties[PROP_STR_SET] =
    g_boolean_property_new ("str-set", G_PROPERTY_READABLE,
                            G_STRUCT_OFFSET (TestObjectPrivate, str_set),
                            NULL, NULL);

  test_object_properties[PROP_BAZ] =
    g_int_property_new ("baz", G_PROPERTY_READWRITE,
                        G_STRUCT_OFFSET (TestObjectPrivate, foo),
                        NULL, NULL);

  test_object_properties[PROP_SINGLE_BYTE] =
    g_int8_property_new ("single-byte", G_PROPERTY_READWRITE,
                         G_STRUCT_OFFSET (TestObjectPrivate, single_byte),
                         NULL, NULL);

  test_object_properties[PROP_DOUBLE_BYTE] =
    g_int16_property_new ("double-byte", G_PROPERTY_READWRITE,
                          G_STRUCT_OFFSET (TestObjectPrivate, double_byte),
                          NULL, NULL);

  test_object_properties[PROP_FOUR_BYTES] =
    g_int32_property_new ("four-bytes", G_PROPERTY_READWRITE,
                          G_STRUCT_OFFSET (TestObjectPrivate, four_bytes),
                          NULL, NULL);

  test_object_properties[PROP_WIDTH] =
    g_float_property_new ("width", G_PROPERTY_READWRITE,
                          G_STRUCT_OFFSET (TestObjectPrivate, width),
                          NULL, NULL);
  g_property_set_range (G_PROPERTY (test_object_properties[PROP_WIDTH]), 0.0, G_MAXFLOAT);

  test_object_properties[PROP_X_ALIGN] =
    g_double_property_new ("x-align", G_PROPERTY_READWRITE,
                           G_STRUCT_OFFSET (TestObjectPrivate, x_align),
                           NULL, NULL);
  g_property_set_range (G_PROPERTY (test_object_properties[PROP_X_ALIGN]), 0.0, 1.0);
  g_property_set_default (G_PROPERTY (test_object_properties[PROP_X_ALIGN]), 0.5);

  test_object_properties[PROP_ENUM_VALUE] =
    g_enum_property_new ("enum-value", G_PROPERTY_READWRITE,
                         G_STRUCT_OFFSET (TestObjectPrivate, enum_value),
                         NULL, NULL);
  g_property_set_prerequisite (G_PROPERTY (test_object_properties[PROP_ENUM_VALUE]),
                               test_enum_value_get_type ());
  g_property_set_default (G_PROPERTY (test_object_properties[PROP_ENUM_VALUE]),
                          TEST_ENUM_VALUE_BAR);

  test_object_properties[PROP_FLAGS_VALUE] =
    g_flags_property_new ("flags-value", G_PROPERTY_READWRITE,
                          G_STRUCT_OFFSET (TestObjectPrivate, flags_value),
                          NULL, NULL);
  g_property_set_prerequisite (G_PROPERTY (test_object_properties[PROP_FLAGS_VALUE]),
                               test_flags_value_get_type ());
  g_property_set_default (G_PROPERTY (test_object_properties[PROP_FLAGS_VALUE]),
                          TEST_FLAGS_VALUE_FOO);

  test_object_properties[PROP_BOXED] =
    g_boxed_property_new ("boxed", G_PROPERTY_READWRITE | G_PROPERTY_COPY_SET,
                          G_STRUCT_OFFSET (TestObjectPrivate, boxed),
                          NULL, NULL);
  g_property_set_prerequisite (G_PROPERTY (test_object_properties[PROP_BOXED]),
                               test_boxed_get_type ());

  g_object_class_install_properties (G_OBJECT_CLASS (klass),
                                     G_N_ELEMENTS (test_object_properties),
                                     test_object_properties);
}

static void
test_object_init (TestObject *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, test_object_get_type (), TestObjectPrivate);

  g_property_get_default (G_PROPERTY (test_object_properties[PROP_FOO]),
                          self,
                          &(self->priv->foo));
  g_property_get_default (G_PROPERTY (test_object_properties[PROP_X_ALIGN]),
                          self,
                          &(self->priv->x_align));
  g_property_get_default (G_PROPERTY (test_object_properties[PROP_ENUM_VALUE]),
                          self,
                          &(self->priv->enum_value));
  g_property_get_default (G_PROPERTY (test_object_properties[PROP_FLAGS_VALUE]),
                          self,
                          &(self->priv->flags_value));
}

static void
autoproperties_base (void)
{
  TestObject *t = g_object_new (test_object_get_type (), NULL);

  g_assert (G_TYPE_CHECK_INSTANCE_TYPE (t, test_object_get_type ()));

  g_object_unref (t);
}

static void
autoproperties_constructor (void)
{
  TestObject *t = g_object_new (test_object_get_type (),
                                "str", "Hello, World!",
                                "x-align", 1.0,
                                NULL);

  g_assert (G_TYPE_CHECK_INSTANCE_TYPE (t, test_object_get_type ()));
  g_assert_cmpstr (test_object_get_str (t), ==, "Hello, World!");
  g_assert_cmpfloat (test_object_get_x_align (t), ==, 1.0);

  g_object_unref (t);
}

typedef TestObject      TestDerived;
typedef TestObjectClass TestDerivedClass;

G_DEFINE_TYPE (TestDerived, test_derived, test_object_get_type ())

static void
test_derived_class_init (TestDerivedClass *klass)
{
  g_object_class_override_property_default (G_OBJECT_CLASS (klass), "foo", -1);
  g_object_class_override_property_default (G_OBJECT_CLASS (klass), "enum-value", TEST_ENUM_VALUE_BAZ);
}

static void
test_derived_init (TestDerived *self)
{
  GValue value = { 0, };

  g_value_init (&value, g_property_get_value_type (G_PROPERTY (test_object_properties[PROP_FOO])));
  g_property_get_default_value_for_type (G_PROPERTY (test_object_properties[PROP_FOO]),
                                         test_derived_get_type (),
                                         &value);

  g_assert_cmpint (g_value_get_int (&value), !=, 50);
  g_assert_cmpint (g_value_get_int (&value), ==, -1);

  test_object_set_foo ((TestObject *) self, g_value_get_int (&value));

  g_value_unset (&value);

  test_object_set_enum_value ((TestObject *) self, TEST_ENUM_VALUE_BAZ);
}

static void
autoproperties_default (void)
{
  TestObject *t = g_object_new (test_object_get_type (), NULL);

  g_assert (g_type_is_a (G_OBJECT_TYPE (t), test_object_get_type ()));
  g_assert_cmpint (test_object_get_foo (t), ==, 50);
  g_assert_cmpfloat (test_object_get_x_align (t), ==, 0.5f);
  g_assert (test_object_get_enum_value (t) == TEST_ENUM_VALUE_BAR);
  g_assert (test_object_get_flags_value (t) == TEST_FLAGS_VALUE_FOO);

  g_object_unref (t);

  t = g_object_new (test_derived_get_type (), NULL);

  g_assert (g_type_is_a (G_OBJECT_TYPE (t), test_object_get_type ()));
  g_assert (g_type_is_a (G_OBJECT_TYPE (t), test_derived_get_type ()));
  g_assert_cmpint (test_object_get_foo (t), ==, -1);
  g_assert (test_object_get_enum_value (t) == TEST_ENUM_VALUE_BAZ);

  g_object_unref (t);
}

static void
autoproperties_range (void)
{
  TestObject *t = g_object_new (test_object_get_type (), NULL);
  GProperty *p;
  gint i_min, i_max;
  gdouble d_min, d_max;

  p = (GProperty *) g_object_class_find_property (G_OBJECT_GET_CLASS (t), "foo");
  g_assert (G_IS_PROPERTY (p));

  g_property_get_range (p, &i_min, &i_max);
  g_assert_cmpint (i_min, ==, -1);
  g_assert_cmpint (i_max, ==, 100);

  p = (GProperty *) g_object_class_find_property (G_OBJECT_GET_CLASS (t), "x-align");
  g_assert (G_IS_PROPERTY (p));

  g_property_get_range (p, &d_min, &d_max);
  g_assert_cmpfloat (d_min, ==, 0.0);
  g_assert_cmpfloat (d_max, ==, 1.0);

  g_object_unref (t);
}

static void
autoproperties_accessors (void)
{
  TestObject *t = g_object_new (test_object_get_type (), NULL);

  test_object_set_foo (t, 42);
  g_assert_cmpint (test_object_get_foo (t), ==, 42);

  test_object_set_str (t, "hello");
  g_assert_cmpstr (test_object_get_str (t), ==, "hello");
  g_assert (test_object_get_str_set (t));

  g_assert (!test_object_get_bar (t));

  test_object_set_single_byte (t, 64);
  g_assert_cmpint (test_object_get_single_byte (t), ==, 64);

  test_object_set_double_byte (t, G_MAXINT16 / 2);
  g_assert_cmpint (test_object_get_double_byte (t), ==, G_MAXINT16 / 2);

  test_object_set_four_bytes (t, 47);
  g_assert_cmpint (test_object_get_four_bytes (t), ==, 47);

  test_object_set_width (t, 640);
  g_assert_cmpfloat (test_object_get_width (t), ==, 640.0f);

  test_object_set_x_align (t, 1.0);
  g_assert_cmpfloat (test_object_get_x_align (t), ==, 1.0);

  g_object_unref (t);
}

static void
autoproperties_validate (void)
{
  if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDOUT|G_TEST_TRAP_SILENCE_STDERR))
    {
      TestObject *t = g_object_new (test_object_get_type (), NULL);
      test_object_set_foo (t, 101);
      g_object_unref (t);
      exit (0);
    }
  g_test_trap_assert_failed ();

  if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDOUT|G_TEST_TRAP_SILENCE_STDERR))
    {
      TestObject *t = g_object_new (test_object_get_type (), NULL);
      test_object_set_foo (t, -10);
      g_object_unref (t);
      exit (0);
    }
  g_test_trap_assert_failed ();
}

static void
autoproperties_object_set (void)
{
  TestObject *t = g_object_new (test_object_get_type (), NULL);
  TestBoxed boxed = { 0, 0, 200, 200, -1 };
  TestBoxed check;

  g_object_set (t,
                "foo", 42,
                "bar", TRUE,
                "flags-value", (TEST_FLAGS_VALUE_BAR | TEST_FLAGS_VALUE_BAZ),
                "boxed", &boxed,
                NULL);

  g_assert_cmpint (test_object_get_foo (t), ==, 42);
  g_assert (test_object_get_bar (t));
  g_assert ((test_object_get_flags_value (t) & TEST_FLAGS_VALUE_BAZ) != 0);
  test_object_get_boxed (t, &check);
  g_assert_cmpint (boxed.y, ==, check.y);
  g_assert_cmpint (boxed.width, ==, check.width);

  g_object_unref (t);
}

static void
autoproperties_object_get (void)
{
  TestObject *t = g_object_new (test_object_get_type (), NULL);
  TestBoxed *boxed;
  gdouble x_align;
  gfloat width;

  g_object_get (t, "x-align", &x_align, "width", &width, "boxed", &boxed, NULL);
  g_assert_cmpfloat (x_align, ==, 0.5);
  g_assert_cmpfloat (width, ==, 0);
  g_assert (boxed == NULL);

  g_object_unref (t);
}

#pragma GCC diagnostic pop

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_bug_base ("http://bugzilla.gnome.org/");

  g_test_add_func ("/auto-properties/base", autoproperties_base);
  g_test_add_func ("/auto-properties/constructor", autoproperties_constructor);
  g_test_add_func ("/auto-properties/default", autoproperties_default);
  g_test_add_func ("/auto-properties/range", autoproperties_range);
  g_test_add_func ("/auto-properties/accessors", autoproperties_accessors);
  g_test_add_func ("/auto-properties/validate", autoproperties_validate);
  g_test_add_func ("/auto-properties/object-set", autoproperties_object_set);
  g_test_add_func ("/auto-properties/object-get", autoproperties_object_get);

  return g_test_run ();
}
