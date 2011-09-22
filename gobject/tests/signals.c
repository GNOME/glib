#include <glib-object.h>

typedef enum {
  TEST_ENUM_NONE = 0,
  TEST_ENUM_FOO = 1,
  TEST_ENUM_BAR = 2
} TestEnum;

typedef enum {
  TEST_UNSIGNED_ENUM_FOO = 1,
  TEST_UNSIGNED_ENUM_BAR = 0x80000000
} TestUnsignedEnum;

GType
test_enum_get_type (void)
{
  static volatile gsize g_define_type_id__volatile = 0;

  if (g_once_init_enter (&g_define_type_id__volatile))
    {
      static const GFlagsValue values[] = {
        { TEST_ENUM_NONE, "TEST_ENUM_NONE", "none" },
        { TEST_ENUM_FOO, "TEST_ENUM_FOO", "foo" },
        { TEST_ENUM_BAR, "TEST_ENUM_BAR", "bar" },
        { 0, NULL, NULL }
      };
      GType g_define_type_id =
        g_flags_register_static (g_intern_static_string ("TestEnum"), values);
      g_once_init_leave (&g_define_type_id__volatile, g_define_type_id);
    }

  return g_define_type_id__volatile;
}

GType
test_unsigned_enum_get_type (void)
{
  static volatile gsize g_define_type_id__volatile = 0;

  if (g_once_init_enter (&g_define_type_id__volatile))
    {
      static const GFlagsValue values[] = {
        { TEST_UNSIGNED_ENUM_FOO, "TEST_UNSIGNED_ENUM_FOO", "foo" },
        { TEST_UNSIGNED_ENUM_BAR, "TEST_UNSIGNED_ENUM_BAR", "bar" },
        { 0, NULL, NULL }
      };
      GType g_define_type_id =
        g_flags_register_static (g_intern_static_string ("TestUnsignedEnum"), values);
      g_once_init_leave (&g_define_type_id__volatile, g_define_type_id);
    }

  return g_define_type_id__volatile;
}


typedef struct _Test Test;
typedef struct _TestClass TestClass;

struct _Test
{
  GObject parent_instance;
};

struct _TestClass
{
  GObjectClass parent_class;

  void (* variant_changed) (Test *, GVariant *);
};

static GType test_get_type (void);
G_DEFINE_TYPE (Test, test, G_TYPE_OBJECT)

static void
test_init (Test *test)
{
}

static void
test_class_init (TestClass *klass)
{
  g_signal_new ("generic-marshaller-1",
                G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_LAST | G_SIGNAL_MUST_COLLECT,
                0,
                NULL, NULL,
                NULL,
                G_TYPE_NONE,
                7,
                G_TYPE_CHAR, G_TYPE_UCHAR, G_TYPE_INT, G_TYPE_LONG, G_TYPE_POINTER, G_TYPE_DOUBLE, G_TYPE_FLOAT);
  g_signal_new ("generic-marshaller-2",
                G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_LAST | G_SIGNAL_MUST_COLLECT,
                0,
                NULL, NULL,
                NULL,
                G_TYPE_NONE,
                5,
                G_TYPE_INT, test_enum_get_type(), G_TYPE_INT, test_unsigned_enum_get_type (), G_TYPE_INT);
  g_signal_new ("variant-changed-no-slot",
                G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_LAST | G_SIGNAL_MUST_COLLECT,
                0,
                NULL, NULL,
                g_cclosure_marshal_VOID__VARIANT,
                G_TYPE_NONE,
                1,
                G_TYPE_VARIANT);
  g_signal_new ("variant-changed",
                G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_LAST | G_SIGNAL_MUST_COLLECT,
                G_STRUCT_OFFSET (TestClass, variant_changed),
                NULL, NULL,
                g_cclosure_marshal_VOID__VARIANT,
                G_TYPE_NONE,
                1,
                G_TYPE_VARIANT);
}

static void
test_variant_signal (void)
{
  Test *test;
  GVariant *v;

  /* Tests that the signal emission consumes the variant,
   * even if there are no handlers connected.
   */

  test = g_object_new (test_get_type (), NULL);

  v = g_variant_new_boolean (TRUE);
  g_variant_ref (v);
  g_assert (g_variant_is_floating (v));
  g_signal_emit_by_name (test, "variant-changed-no-slot", v);
  g_assert (!g_variant_is_floating (v));
  g_variant_unref (v);

  v = g_variant_new_boolean (TRUE);
  g_variant_ref (v);
  g_assert (g_variant_is_floating (v));
  g_signal_emit_by_name (test, "variant-changed", v);
  g_assert (!g_variant_is_floating (v));
  g_variant_unref (v);

  g_object_unref (test);
}

static void
on_generic_marshaller_1 (Test *obj,
			 gint8 v_schar,
			 guint8 v_uchar,
			 gint v_int,
			 glong v_long,
			 gpointer v_pointer,
			 gdouble v_double,
			 gfloat v_float,
			 gpointer user_data)
{
  g_assert_cmpint (v_schar, ==, 42);
  g_assert_cmpint (v_uchar, ==, 43);
  g_assert_cmpint (v_int, ==, 4096);
  g_assert_cmpint (v_long, ==, 8192);
  g_assert (v_pointer == NULL);
  g_assert_cmpfloat (v_double, >, 0.0);
  g_assert_cmpfloat (v_double, <, 1.0);
  g_assert_cmpfloat (v_float, >, 5.0);
  g_assert_cmpfloat (v_float, <, 6.0);
}
			 
static void
test_generic_marshaller_signal_1 (void)
{
  Test *test;
  test = g_object_new (test_get_type (), NULL);

  g_signal_connect (test, "generic-marshaller-1", G_CALLBACK (on_generic_marshaller_1), NULL);

  g_signal_emit_by_name (test, "generic-marshaller-1", 42, 43, 4096, 8192, NULL, 0.5, 5.5);

  g_object_unref (test);
}

static void
on_generic_marshaller_2 (Test *obj,
			 gint        v_int1,
			 TestEnum    v_enum,
			 gint        v_int2,
			 TestUnsignedEnum v_uenum,
			 gint        v_int3)
{
  g_assert_cmpint (v_int1, ==, 42);
  g_assert_cmpint (v_enum, ==, TEST_ENUM_BAR);
  g_assert_cmpint (v_int2, ==, 43);
  g_assert_cmpint (v_uenum, ==, TEST_UNSIGNED_ENUM_BAR);
  g_assert_cmpint (v_int3, ==, 44);
}

static void
test_generic_marshaller_signal_2 (void)
{
  Test *test;
  test = g_object_new (test_get_type (), NULL);

  g_signal_connect (test, "generic-marshaller-2", G_CALLBACK (on_generic_marshaller_2), NULL);

  g_signal_emit_by_name (test, "generic-marshaller-2", 42, TEST_ENUM_BAR, 43, TEST_UNSIGNED_ENUM_BAR, 44);

  g_object_unref (test);
}

/* --- */

int
main (int argc,
     char *argv[])
{
  g_type_init ();

  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/gobject/signals/variant", test_variant_signal);
  g_test_add_func ("/gobject/signals/generic-marshaller-1", test_generic_marshaller_signal_1);
  g_test_add_func ("/gobject/signals/generic-marshaller-2", test_generic_marshaller_signal_2);

  return g_test_run ();
}
