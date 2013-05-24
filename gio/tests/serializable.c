#include <gio/gio.h>

typedef struct {
  GObject parent_instance;

  gboolean bool_value;
  char *str_value;
  gdouble double_value;
} TestObject;

typedef struct {
  GObjectClass parent_class;
} TestObjectClass;

GType test_object_get_type (void);

static void test_object_serializable_init (GSerializableInterface *iface);

G_DEFINE_TYPE_WITH_CODE (TestObject, test_object, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_SERIALIZABLE, test_object_serializable_init))

static void
test_object_serialize (GSerializable *serializable,
                       GEncoder      *encoder)
{
  TestObject *self = (TestObject *) serializable;

  g_encoder_add_key_bool (encoder, "bool-value", self->bool_value);
  g_encoder_add_key_string (encoder, "str-value", self->str_value);
  g_encoder_add_key_double (encoder, "double-value", self->double_value);
}

static gboolean
test_object_deserialize (GSerializable *serializable,
                         GEncoder      *encoder,
                         GError       **error)
{
  TestObject *self = (TestObject *) serializable;

  g_encoder_get_key_bool (encoder, "bool-value", &self->bool_value);
  g_encoder_get_key_string (encoder, "str-value", &self->str_value);
  g_encoder_get_key_double (encoder, "double-value", &self->double_value);

  return TRUE;
}

static void
test_object_serializable_init (GSerializableInterface *iface)
{
  iface->serialize = test_object_serialize;
  iface->deserialize = test_object_deserialize;
}

static void
test_object_finalize (GObject *gobject)
{
  TestObject *self = (TestObject *) gobject;

  g_free (self->str_value);

  G_OBJECT_CLASS (test_object_parent_class)->finalize (gobject);
}

static void
test_object_class_init (TestObjectClass *klass)
{
  G_OBJECT_CLASS (klass)->finalize = test_object_finalize;
}

static void
test_object_init (TestObject *self)
{
}

static TestObject *
test_object_new (void)
{
  return g_object_new (test_object_get_type (), NULL);
}

static void
test_object_set_bool_value (TestObject *obj,
                            gboolean    value)
{
  value = !!value;

  obj->bool_value = value;
}

static gboolean
test_object_get_bool_value (TestObject *obj)
{
  return obj->bool_value;
}

static void
test_object_set_str_value (TestObject *obj,
                           const char *value)
{
  char *str_value = g_strdup (value);

  g_free (obj->str_value);
  obj->str_value = str_value;
}

static const char *
test_object_get_str_value (TestObject *obj)
{
  return obj->str_value;
}

static void
test_object_set_double_value (TestObject *obj,
                              double      value)
{
  obj->double_value = value;
}

static gdouble
test_object_get_double_value (TestObject *obj)
{
  return obj->double_value;
}

/* tests */
static void
serializable_roundtrip (void)
{
  GEncoder *encoder;
  TestObject *obj;
  GError *error;
  GBytes *buffer;

  obj = test_object_new ();
  test_object_set_bool_value (obj, TRUE);
  test_object_set_str_value (obj, "Hello, World");
  test_object_set_double_value (obj, 3.14159);

  encoder = g_binary_encoder_new ();
  g_serializable_serialize (G_SERIALIZABLE (obj), encoder);

  error = NULL;
  buffer = g_encoder_write_to_bytes (encoder, &error);
  g_assert_no_error (error);
  g_object_unref (encoder);
  g_object_unref (obj);

  if (g_test_verbose ())
    g_print ("*** buffer: '%s' ***\n",
             (const char *) g_bytes_get_data (buffer, NULL));

  encoder = g_binary_encoder_new ();
  g_encoder_read_from_bytes (encoder, buffer, &error);
  g_assert_no_error (error);

  obj = test_object_new ();
  g_serializable_deserialize (G_SERIALIZABLE (obj), encoder, &error);
  g_assert_no_error (error);

  g_assert (test_object_get_bool_value (obj));
  g_assert_cmpstr (test_object_get_str_value (obj), ==, "Hello, World");
  g_assert_cmpfloat ((float) test_object_get_double_value (obj), ==, 3.14159f);

  g_bytes_unref (buffer);
  g_object_unref (encoder);
  g_object_unref (obj);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/serializable/roundtrip", serializable_roundtrip);

  return g_test_run ();
}
