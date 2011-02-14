#include <glib-object.h>

static void
test_fundamentals (void)
{
  g_assert (G_TYPE_IS_FUNDAMENTAL (G_TYPE_NONE));
  g_assert (G_TYPE_IS_FUNDAMENTAL (G_TYPE_INTERFACE));
  g_assert (G_TYPE_IS_FUNDAMENTAL (G_TYPE_CHAR));
  g_assert (G_TYPE_IS_FUNDAMENTAL (G_TYPE_UCHAR));
  g_assert (G_TYPE_IS_FUNDAMENTAL (G_TYPE_BOOLEAN));
  g_assert (G_TYPE_IS_FUNDAMENTAL (G_TYPE_INT));
  g_assert (G_TYPE_IS_FUNDAMENTAL (G_TYPE_UINT));
  g_assert (G_TYPE_IS_FUNDAMENTAL (G_TYPE_LONG));
  g_assert (G_TYPE_IS_FUNDAMENTAL (G_TYPE_ULONG));
  g_assert (G_TYPE_IS_FUNDAMENTAL (G_TYPE_INT64));
  g_assert (G_TYPE_IS_FUNDAMENTAL (G_TYPE_UINT64));
  g_assert (G_TYPE_IS_FUNDAMENTAL (G_TYPE_ENUM));
  g_assert (G_TYPE_IS_FUNDAMENTAL (G_TYPE_FLAGS));
  g_assert (G_TYPE_IS_FUNDAMENTAL (G_TYPE_FLOAT));
  g_assert (G_TYPE_IS_FUNDAMENTAL (G_TYPE_DOUBLE));
  g_assert (G_TYPE_IS_FUNDAMENTAL (G_TYPE_STRING));
  g_assert (G_TYPE_IS_FUNDAMENTAL (G_TYPE_POINTER));
  g_assert (G_TYPE_IS_FUNDAMENTAL (G_TYPE_BOXED));
  g_assert (G_TYPE_IS_FUNDAMENTAL (G_TYPE_PARAM));
  g_assert (G_TYPE_IS_FUNDAMENTAL (G_TYPE_OBJECT));
  g_assert (G_TYPE_OBJECT == g_object_get_type ());
  g_assert (G_TYPE_IS_FUNDAMENTAL (G_TYPE_VARIANT));
  g_assert (G_TYPE_VARIANT == g_variant_get_gtype ());
  g_assert (G_TYPE_IS_DERIVED (G_TYPE_INITIALLY_UNOWNED));

  g_assert (g_type_fundamental_next () == G_TYPE_MAKE_FUNDAMENTAL (G_TYPE_RESERVED_USER_FIRST));
}

static void
test_type_qdata (void)
{
  gchar *data;

  g_type_set_qdata (G_TYPE_ENUM, g_quark_from_string ("bla"), "bla");
  data = g_type_get_qdata (G_TYPE_ENUM, g_quark_from_string ("bla"));
  g_assert_cmpstr (data, ==, "bla");
}

static void
test_type_query (void)
{
  GTypeQuery query;

  g_type_query (G_TYPE_ENUM, &query);
  g_assert_cmpint (query.type, ==, G_TYPE_ENUM);
  g_assert_cmpstr (query.type_name, ==, "GEnum");
  g_assert_cmpint (query.class_size, ==, sizeof (GEnumClass));
  g_assert_cmpint (query.instance_size, ==, 0);
}

typedef struct _MyObject MyObject;
typedef struct _MyObjectClass MyObjectClass;
typedef struct _MyObjectClassPrivate MyObjectClassPrivate;

struct _MyObject
{
  GObject parent_instance;

  gint count;
};

struct _MyObjectClass
{
  GObjectClass parent_class;
};

struct _MyObjectClassPrivate
{
  gint secret_class_count;
};

G_DEFINE_TYPE_WITH_CODE (MyObject, my_object, G_TYPE_OBJECT,
                         g_type_add_class_private (g_define_type_id, sizeof (MyObjectClassPrivate)) );

static void
my_object_init (MyObject *obj)
{
  obj->count = 42;
}

static void
my_object_class_init (MyObjectClass *klass)
{
}

static void
test_class_private (void)
{
  GObject *obj;
  MyObjectClass *class;
  MyObjectClassPrivate *priv;

  obj = g_object_new (my_object_get_type (), NULL);

  class = g_type_class_ref (my_object_get_type ());
  priv = G_TYPE_CLASS_GET_PRIVATE (class, my_object_get_type (), MyObjectClassPrivate);
  priv->secret_class_count = 13;
  g_type_class_unref (class);

  g_object_unref (obj);

  g_assert_cmpint (g_type_qname (my_object_get_type ()), ==, g_quark_from_string ("MyObject"));
}

static void
test_clear (void)
{
  GObject *o = NULL;
  GObject *tmp;

  g_clear_object (&o);
  g_assert (o == NULL);

  tmp = g_object_new (G_TYPE_OBJECT, NULL);
  g_assert_cmpint (tmp->ref_count, ==, 1);
  o = g_object_ref (tmp);
  g_assert (o != NULL);

  g_assert_cmpint (tmp->ref_count, ==, 2);
  g_clear_object (&o);
  g_assert_cmpint (tmp->ref_count, ==, 1);
  g_assert (o == NULL);

  g_object_unref (tmp);
}

static void
test_clear_function (void)
{
  volatile GObject *o = NULL;
  GObject *tmp;

  (g_clear_object) (&o);
  g_assert (o == NULL);

  tmp = g_object_new (G_TYPE_OBJECT, NULL);
  g_assert_cmpint (tmp->ref_count, ==, 1);
  o = g_object_ref (tmp);
  g_assert (o != NULL);

  g_assert_cmpint (tmp->ref_count, ==, 2);
  (g_clear_object) (&o);
  g_assert_cmpint (tmp->ref_count, ==, 1);
  g_assert (o == NULL);

  g_object_unref (tmp);
}

static void
test_object_value (void)
{
  GObject *v;
  GObject *v2;
  GValue value = { 0, };

  g_value_init (&value, G_TYPE_OBJECT);

  v = g_object_new (G_TYPE_OBJECT, NULL);
  g_value_take_object (&value, v);

  v2 = g_value_get_object (&value);
  g_assert (v2 == v);

  v2 = g_value_dup_object (&value);
  g_assert (v2 == v);  /* objects use ref/unref for copy/free */
  g_object_unref (v2);

  g_value_unset (&value);
}

static void
test_initially_unowned (void)
{
  GObject *obj;

  obj = g_object_new (G_TYPE_INITIALLY_UNOWNED, NULL);
  g_assert (g_object_is_floating (obj));
  g_assert_cmpint (obj->ref_count, ==, 1);

  g_object_ref_sink (obj);
  g_assert (!g_object_is_floating (obj));
  g_assert_cmpint (obj->ref_count, ==, 1);

  g_object_ref_sink (obj);
  g_assert (!g_object_is_floating (obj));
  g_assert_cmpint (obj->ref_count, ==, 2);

  g_object_unref (obj);
  g_assert_cmpint (obj->ref_count, ==, 1);

  g_object_force_floating (obj);
  g_assert (g_object_is_floating (obj));
  g_assert_cmpint (obj->ref_count, ==, 1);

  g_object_ref_sink (obj);
  g_object_unref (obj);
}

static void
test_weak_pointer (void)
{
  GObject *obj;
  gpointer weak;
  gpointer weak2;

  weak = weak2 = obj = g_object_new (G_TYPE_OBJECT, NULL);
  g_assert_cmpint (obj->ref_count, ==, 1);

  g_object_add_weak_pointer (obj, &weak);
  g_object_add_weak_pointer (obj, &weak2);
  g_assert_cmpint (obj->ref_count, ==, 1);
  g_assert (weak == obj);
  g_assert (weak2 == obj);

  g_object_remove_weak_pointer (obj, &weak2);
  g_assert_cmpint (obj->ref_count, ==, 1);
  g_assert (weak == obj);
  g_assert (weak2 == obj);

  g_object_unref (obj);
  g_assert (weak == NULL);
  g_assert (weak2 == obj);
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);

  g_type_init ();

  g_test_add_func ("/type/fundamentals", test_fundamentals);
  g_test_add_func ("/type/qdata", test_type_qdata);
  g_test_add_func ("/type/query", test_type_query);
  g_test_add_func ("/type/class-private", test_class_private);
  g_test_add_func ("/object/clear", test_clear);
  g_test_add_func ("/object/clear-function", test_clear_function);
  g_test_add_func ("/object/value", test_object_value);
  g_test_add_func ("/object/initially-unowned", test_initially_unowned);
  g_test_add_func ("/object/weak-pointer", test_weak_pointer);

  return g_test_run ();
}
