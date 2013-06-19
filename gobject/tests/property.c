#include <glib-object.h>

typedef struct _TestObject              TestObject;
typedef struct _TestObjectPrivate       TestObjectPrivate;
typedef struct _TestObjectClass         TestObjectClass;

typedef enum {
  TEST_ENUM_ONE,
  TEST_ENUM_TWO,
  TEST_ENUM_THREE,

  TEST_ENUM_UNSET = -1
} TestEnum;

struct _TestObject
{
  GObject parent_instance;
};

struct _TestObjectClass
{
  GObjectClass parent_class;
};

struct _TestObjectPrivate
{
  int integer_val;

  double double_val;

  char *string_val;

  gboolean bool_val;

  TestEnum enum_val;
  gboolean enum_val_set;

  guint8 with_default;

  float width;
  float height;
};

GType test_enum_get_type (void);
GType test_object_get_type (void);

GType
test_enum_get_type (void)
{
  static volatile gsize g_define_type_id__volatile = 0;

  if (g_once_init_enter (&g_define_type_id__volatile))
    {
      static const GEnumValue values[] = {
        { TEST_ENUM_UNSET, "TEST_ENUM_UNSET", "unset" },
        { TEST_ENUM_ONE, "TEST_ENUM_ONE", "one" },
        { TEST_ENUM_TWO, "TEST_ENUM_TWO", "two" },
        { TEST_ENUM_THREE, "TEST_ENUM_THREE", "three" },
        { 0, NULL, NULL }
      };
      GType g_define_type_id =
        g_enum_register_static (g_intern_static_string ("TestEnum"), values);
      g_once_init_leave (&g_define_type_id__volatile, g_define_type_id);
    }

  return g_define_type_id__volatile;
}

G_DEFINE_TYPE_WITH_PRIVATE (TestObject, test_object, G_TYPE_OBJECT)

static void
test_object_finalize (GObject *gobject)
{
  TestObjectPrivate *priv = test_object_get_instance_private ((TestObject *) gobject);

  g_free (priv->string_val);

  if (priv->enum_val_set)
    g_assert (priv->enum_val != TEST_ENUM_UNSET);

  if (priv->enum_val != TEST_ENUM_UNSET)
    g_assert (priv->enum_val_set);

  G_OBJECT_CLASS (test_object_parent_class)->finalize (gobject);
}

static gboolean
test_object_set_enum_val_internal (gpointer obj,
                                   gint     val)
{
  TestObjectPrivate *priv = test_object_get_instance_private (obj);

  if (priv->enum_val == val)
    return FALSE;

  priv->enum_val = val;
  priv->enum_val_set = val != TEST_ENUM_UNSET;

  return TRUE;
}

static void
test_object_constructed (GObject *gobject)
{
  TestObject *self = (TestObject *) gobject;
  TestObjectPrivate *priv = test_object_get_instance_private (self);

  g_assert (priv->enum_val == TEST_ENUM_UNSET);
  g_assert (!priv->enum_val_set);
}

static void
test_object_class_init (TestObjectClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->constructed = test_object_constructed;
  gobject_class->finalize = test_object_finalize;

  G_DEFINE_PROPERTIES (TestObject, test_object, klass,
                       G_DEFINE_PROPERTY (TestObject,
                                          int,
                                          integer_val,
                                          G_PROPERTY_READWRITE)
                       G_DEFINE_PROPERTY (TestObject,
                                          double,
                                          double_val,
                                          G_PROPERTY_READWRITE)
                       G_DEFINE_PROPERTY (TestObject,
                                          string,
                                          string_val,
                                          G_PROPERTY_READWRITE | G_PROPERTY_COPY_SET)
                       G_DEFINE_PROPERTY (TestObject,
                                          boolean,
                                          bool_val,
                                          G_PROPERTY_READWRITE)
                       G_DEFINE_PROPERTY (TestObject,
                                          float,
                                          width,
                                          G_PROPERTY_READWRITE)
                       G_DEFINE_PROPERTY (TestObject,
                                          float,
                                          height,
                                          G_PROPERTY_READWRITE)
                       G_DEFINE_PROPERTY_EXTENDED (TestObject,
                                                   enum,
                                                   enum_val,
                                                   G_PRIVATE_OFFSET (TestObject, enum_val),
                                                   test_object_set_enum_val_internal,
                                                   NULL,
                                                   G_PROPERTY_READWRITE,
                                                   G_PROPERTY_DEFAULT (TEST_ENUM_UNSET)
                                                   G_PROPERTY_PREREQUISITE (test_enum_get_type ()))
                       G_DEFINE_PROPERTY (TestObject,
                                          boolean,
                                          enum_val_set,
                                          G_PROPERTY_READABLE);
                       G_DEFINE_PROPERTY_WITH_CODE (TestObject,
                                                    uint8,
                                                    with_default,
                                                    G_PROPERTY_READWRITE,
                                                    G_PROPERTY_DEFAULT (255)
                                                    G_PROPERTY_DESCRIBE ("With Default",
                                                                         "A property with a default value")))
}

static void
test_object_init (TestObject *self)
{
}

G_DECLARE_PROPERTY_GET_SET (TestObject, test_object, gboolean, bool_val)
G_DECLARE_PROPERTY_GET_SET (TestObject, test_object, float, width)
G_DECLARE_PROPERTY_GET_SET (TestObject, test_object, float, height)
G_DECLARE_PROPERTY_GET_SET (TestObject, test_object, TestEnum, enum_val)
G_DECLARE_PROPERTY_GET (TestObject, test_object, gboolean, enum_val_set)

G_DEFINE_PROPERTY_GET_SET (TestObject, test_object, gboolean, bool_val)
G_DEFINE_PROPERTY_GET_SET (TestObject, test_object, float, width)
G_DEFINE_PROPERTY_GET_SET (TestObject, test_object, float, height)
G_DEFINE_PROPERTY_GET_SET (TestObject, test_object, TestEnum, enum_val)
G_DEFINE_PROPERTY_INDIRECT_GET (TestObject, test_object, gboolean, enum_val_set)

typedef struct {
  TestObject parent_instance;
} TestDerived;

typedef struct {
  TestObjectClass parent_class;
} TestDerivedClass;

GType test_derived_get_type (void);

G_DEFINE_TYPE (TestDerived, test_derived, test_object_get_type ())

static void
test_derived_constructed (GObject *gobject)
{
  TestObject *self = (TestObject *) gobject;
  TestObjectPrivate *priv = test_object_get_instance_private (self);

  g_assert (priv->enum_val == TEST_ENUM_TWO);
  g_assert (priv->enum_val_set);

  /* do not chain up, or we trigger the assert */
}

static void
test_derived_class_init (TestDerivedClass *klass)
{
  G_OBJECT_CLASS (klass)->constructed = test_derived_constructed;

  g_object_class_override_property_default (G_OBJECT_CLASS (klass),
                                            "enum-val",
                                            TEST_ENUM_TWO);
  g_object_class_override_property_default (G_OBJECT_CLASS (klass),
                                            "with-default",
                                            128);
}

static void
test_derived_init (TestDerived *self)
{
}

/* test units start here */

static void
check_notify_emission (GObject *object,
                       GParamSpec *pspec,
                       gboolean *toggle)
{
  if (toggle != NULL)
    *toggle = TRUE;
}

static void
gproperty_construct (void)
{
  TestObject *obj = g_object_new (test_object_get_type (),
                                  "integer-val", 42,
                                  "bool-val", TRUE,
                                  "string-val", "Hello, world",
                                  "double-val", 3.14159,
                                  NULL);

  TestObjectPrivate *priv = test_object_get_instance_private (obj);

  g_assert_cmpint (priv->integer_val, ==, 42);
  g_assert (priv->bool_val);
  g_assert_cmpstr (priv->string_val, ==, "Hello, world");
  g_assert_cmpfloat ((float) priv->double_val, ==, 3.14159f);

  g_object_unref (obj);
}

static void
gproperty_object_set (void)
{
  TestObject *obj = g_object_new (test_object_get_type (), NULL);
  TestObjectPrivate *priv = test_object_get_instance_private (obj);

  gboolean did_emit_notify = FALSE;

  g_signal_connect (obj, "notify::string-val", G_CALLBACK (check_notify_emission), &did_emit_notify);

  g_object_set (obj, "string-val", "Hello!", NULL);
  g_assert_cmpstr (priv->string_val, ==, "Hello!");

  g_assert (did_emit_notify);

  did_emit_notify = FALSE;

  g_object_set (obj, "string-val", "Hello!", NULL);
  g_assert_cmpstr (priv->string_val, ==, "Hello!");

  g_assert (!did_emit_notify);

  g_object_unref (obj);
}

static void
gproperty_object_get (void)
{
  TestObject *obj = g_object_new (test_object_get_type (),
                                  "integer-val", 42,
                                  "string-val", "Hello!",
                                  NULL);
  int int_val = 0;
  char *str_val = NULL;

  g_object_get (obj, "integer-val", &int_val, NULL);
  g_assert_cmpint (int_val, ==, 42);

  g_object_get (obj, "string-val", &str_val, NULL);
  g_assert_cmpstr (str_val, ==, "Hello!");

  g_free (str_val);
  g_object_unref (obj);
}

static void
gproperty_explicit_set (void)
{
  TestObject *obj = g_object_new (test_object_get_type (), NULL);
  gboolean did_emit_notify = FALSE;
  TestEnum enum_val;

  g_signal_connect (obj, "notify::enum-val", G_CALLBACK (check_notify_emission), &did_emit_notify);

  g_object_set (obj, "enum-val", TEST_ENUM_THREE, NULL);
  g_assert_cmpint (test_object_get_enum_val (obj), ==, TEST_ENUM_THREE);
  g_assert (test_object_get_enum_val_set (obj));
  g_assert (did_emit_notify);

  did_emit_notify = FALSE;
  test_object_set_enum_val (obj, TEST_ENUM_THREE);
  g_object_get (obj, "enum-val", &enum_val, NULL);
  g_assert_cmpint (enum_val, ==, TEST_ENUM_THREE);
  g_assert (!did_emit_notify);

  g_object_unref (obj);
}

static void
gproperty_default_init (void)
{
  TestObject *obj;
  guint8 with_default = 0;

  obj = g_object_new (test_object_get_type (), NULL);
  g_object_get (obj, "with-default", &with_default, NULL);
  g_assert_cmpint (with_default, ==, 255);

  g_object_unref (obj);

  obj = g_object_new (test_object_get_type (), "with-default", 128, NULL);
  g_object_get (obj, "with-default", &with_default, NULL);
  g_assert_cmpint (with_default, ==, 128);

  g_object_unref (obj);
}

static void
gproperty_default_override (void)
{
  TestObject *obj;
  guint8 with_default = 0;

  if (g_test_verbose ())
    g_print ("*** Base type ***\n");

  obj = g_object_new (test_object_get_type (), NULL);
  g_object_get (obj, "with-default", &with_default, NULL);
  g_assert_cmpint (with_default, ==, 255);

  g_object_unref (obj);

  if (g_test_verbose ())
    g_print ("*** Derived type ***\n");

  obj = g_object_new (test_derived_get_type (), NULL);
  g_object_get (obj, "with-default", &with_default, NULL);
  g_assert_cmpint (with_default, ==, 128);

  g_object_unref (obj);
}

static void
gproperty_accessors_get_set (void)
{
  TestObject *obj = g_object_new (test_object_get_type (), NULL);
  gboolean did_emit_notify = FALSE;

  g_signal_connect (obj, "notify::bool-val", G_CALLBACK (check_notify_emission), &did_emit_notify);

  test_object_set_bool_val (obj, TRUE);

  g_assert (did_emit_notify);
  g_assert (test_object_get_bool_val (obj));

  did_emit_notify = FALSE;

  test_object_set_bool_val (obj, FALSE);

  g_assert (did_emit_notify);
  g_assert (!test_object_get_bool_val (obj));

  g_object_unref (obj);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_bug_base ("http://bugzilla.gnome.org/");

  g_test_add_func ("/gproperty/construct", gproperty_construct);
  g_test_add_func ("/gproperty/object-set", gproperty_object_set);
  g_test_add_func ("/gproperty/object-get", gproperty_object_get);
  g_test_add_func ("/gproperty/explicit-set", gproperty_explicit_set);
  g_test_add_func ("/gproperty/default/init", gproperty_default_init);
  g_test_add_func ("/gproperty/default/override", gproperty_default_override);
  g_test_add_func ("/gproperty/accessors/get-set", gproperty_accessors_get_set);

  return g_test_run ();
}
