#include <glib-object.h>

typedef struct _TestObject              TestObject;
typedef struct _TestObjectPrivate       TestObjectPrivate;
typedef struct _TestObjectClass         TestObjectClass;

typedef enum {
  TEST_ENUM_UNSET,
  TEST_ENUM_ONE,
  TEST_ENUM_TWO,
  TEST_ENUM_THREE
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

  char *str_val;

  gboolean bool_val;

  TestEnum enum_val;
  guint enum_val_set : 1;
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

enum
{
  PROP_0,

  PROP_INTEGER_VAL,
  PROP_DOUBLE_VAL,
  PROP_STRING_VAL,
  PROP_BOOL_VAL,
  PROP_ENUM_VAL,

  LAST_PROP
};

static GParamSpec *test_object_properties[LAST_PROP] = { NULL, };

static void
test_object_finalize (GObject *gobject)
{
  TestObjectPrivate *priv = test_object_get_instance_private ((TestObject *) gobject);

  g_free (priv->str_val);

  if (priv->enum_val != TEST_ENUM_UNSET)
    g_assert (priv->enum_val_set);

  G_OBJECT_CLASS (test_object_parent_class)->finalize (gobject);
}

static gboolean
test_object_set_enum_val (gpointer obj,
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
test_object_class_init (TestObjectClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = test_object_finalize;

  test_object_properties[PROP_INTEGER_VAL] =
    g_int_property_new ("integer-val",
                        G_PROPERTY_READWRITE,
                        G_STRUCT_OFFSET (TestObjectPrivate, integer_val),
                        NULL, NULL);

  test_object_properties[PROP_DOUBLE_VAL] =
    g_double_property_new ("double-val",
                           G_PROPERTY_READWRITE,
                           G_STRUCT_OFFSET (TestObjectPrivate, double_val),
                           NULL, NULL);

  test_object_properties[PROP_STRING_VAL] =
    g_string_property_new ("string-val",
                           G_PROPERTY_READWRITE | G_PROPERTY_COPY_SET,
                           G_STRUCT_OFFSET (TestObjectPrivate, str_val),
                           NULL, NULL);

  test_object_properties[PROP_BOOL_VAL] =
    g_boolean_property_new ("bool-val",
                            G_PROPERTY_READWRITE,
                            G_STRUCT_OFFSET (TestObjectPrivate, bool_val),
                            NULL, NULL);

  test_object_properties[PROP_ENUM_VAL] =
    g_enum_property_new ("enum-val",
                         G_PROPERTY_READWRITE,
                         G_STRUCT_OFFSET (TestObjectPrivate, enum_val),
                         test_object_set_enum_val,
                         NULL);
  g_property_set_prerequisite ((GProperty *) test_object_properties[PROP_ENUM_VAL],
                               test_enum_get_type ());

  g_object_class_install_properties (gobject_class, LAST_PROP, test_object_properties);
}

static void
test_object_init (TestObject *self)
{
  TestObjectPrivate *priv = test_object_get_private (self);

  priv->enum_val = TEST_ENUM_UNSET;
}

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
  g_assert_cmpstr (priv->str_val, ==, "Hello, world");
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
  g_assert_cmpstr (priv->str_val, ==, "Hello!");

  g_assert (did_emit_notify);

  did_emit_notify = FALSE;

  g_object_set (obj, "string-val", "Hello!", NULL);
  g_assert_cmpstr (priv->str_val, ==, "Hello!");

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

  TestObjectPrivate *priv =
    g_type_instance_get_private ((GTypeInstance *) obj, test_object_get_type ());

  g_signal_connect (obj, "notify::enum-val", G_CALLBACK (check_notify_emission), &did_emit_notify);

  g_object_set (obj, "enum-val", TEST_ENUM_THREE, NULL);
  g_assert_cmpint (priv->enum_val, ==, TEST_ENUM_THREE);
  g_assert (priv->enum_val_set);

  g_assert (did_emit_notify);

  did_emit_notify = FALSE;
  g_object_set (obj, "enum-val", TEST_ENUM_THREE, NULL);
  g_assert (!did_emit_notify);

  g_object_get (obj, "enum-val", &enum_val, NULL);
  g_assert_cmpint (enum_val, ==, TEST_ENUM_THREE);

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

  return g_test_run ();
}
