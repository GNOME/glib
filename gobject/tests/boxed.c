#define GLIB_DISABLE_DEPRECATION_WARNINGS

#include <glib-object.h>

typedef struct _MyBoxed MyBoxed;

struct _MyBoxed
{
  gint ivalue;
  gchar *bla;
};

static gpointer
my_boxed_copy (gpointer orig)
{
  MyBoxed *a = orig;
  MyBoxed *b;

  b = g_slice_new (MyBoxed);
  b->ivalue = a->ivalue;
  b->bla = g_strdup (a->bla);

  return b;
}

static gint my_boxed_free_count;

static void
my_boxed_free (gpointer orig)
{
  MyBoxed *a = orig;

  g_free (a->bla);
  g_slice_free (MyBoxed, a);

  my_boxed_free_count++;
}

static GType my_boxed_get_type (void);
#define MY_TYPE_BOXED (my_boxed_get_type ())

G_DEFINE_BOXED_TYPE (MyBoxed, my_boxed, my_boxed_copy, my_boxed_free)

static void
test_define_boxed (void)
{
  MyBoxed a;
  MyBoxed *b;

  a.ivalue = 20;
  a.bla = g_strdup ("bla");

  b = g_boxed_copy (MY_TYPE_BOXED, &a);

  g_assert_cmpint (b->ivalue, ==, 20);
  g_assert_cmpstr (b->bla, ==, "bla");

  g_boxed_free (MY_TYPE_BOXED, b);

  g_free (a.bla);
}

static void
test_boxed_ownership (void)
{
  GValue value = G_VALUE_INIT;
  static MyBoxed boxed = { 10, "bla" };

  g_value_init (&value, MY_TYPE_BOXED);

  my_boxed_free_count = 0;

  g_value_set_static_boxed (&value, &boxed);
  g_value_reset (&value);

  g_assert_cmpint (my_boxed_free_count, ==, 0);

  g_value_set_boxed_take_ownership (&value, g_boxed_copy (MY_TYPE_BOXED, &boxed));
  g_value_reset (&value);
  g_assert_cmpint (my_boxed_free_count, ==, 1);

  g_value_take_boxed (&value, g_boxed_copy (MY_TYPE_BOXED, &boxed));
  g_value_reset (&value);
  g_assert_cmpint (my_boxed_free_count, ==, 2);

  g_value_set_boxed (&value, &boxed);
  g_value_reset (&value);
  g_assert_cmpint (my_boxed_free_count, ==, 3);
}

static void
my_callback (gpointer user_data)
{
}

static gint destroy_count;

static void
my_closure_notify (gpointer user_data, GClosure *closure)
{
  destroy_count++;
}

static void
test_boxed_closure (void)
{
  GClosure *closure;
  GClosure *closure2;
  GValue value = G_VALUE_INIT;

  g_value_init (&value, G_TYPE_CLOSURE);
  g_assert (G_VALUE_HOLDS_BOXED (&value));

  closure = g_cclosure_new (G_CALLBACK (my_callback), "bla", my_closure_notify);
  g_value_take_boxed (&value, closure);

  closure2 = g_value_get_boxed (&value);
  g_assert (closure2 == closure);

  closure2 = g_value_dup_boxed (&value);
  g_assert (closure2 == closure); /* closures use ref/unref for copy/free */
  g_closure_unref (closure2);

  g_value_unset (&value);
  g_assert_cmpint (destroy_count, ==, 1);
}

static void
test_boxed_date (void)
{
  GDate *date;
  GDate *date2;
  GValue value = G_VALUE_INIT;

  g_value_init (&value, G_TYPE_DATE);
  g_assert (G_VALUE_HOLDS_BOXED (&value));

  date = g_date_new_dmy (1, 3, 1970);
  g_value_take_boxed (&value, date);

  date2 = g_value_get_boxed (&value);
  g_assert (date2 == date);

  date2 = g_value_dup_boxed (&value);
  g_assert (date2 != date);
  g_assert (g_date_compare (date, date2) == 0);
  g_date_free (date2);

  g_value_unset (&value);
}

static void
test_boxed_value (void)
{
  GValue value1 = G_VALUE_INIT;
  GValue *value2;
  GValue value = G_VALUE_INIT;

  g_value_init (&value, G_TYPE_VALUE);
  g_assert (G_VALUE_HOLDS_BOXED (&value));

  g_value_init (&value1, G_TYPE_INT);
  g_value_set_int (&value1, 26);

  g_value_set_static_boxed (&value, &value1);

  value2 = g_value_get_boxed (&value);
  g_assert (value2 == &value1);

  value2 = g_value_dup_boxed (&value);
  g_assert (value2 != &value1);
  g_assert (G_VALUE_HOLDS_INT (value2));
  g_assert_cmpint (g_value_get_int (value2), ==, 26);
  g_boxed_free (G_TYPE_VALUE, value2);

  g_value_unset (&value);
}

static void
test_boxed_string (void)
{
  GString *v;
  GString *v2;
  GValue value = G_VALUE_INIT;

  g_value_init (&value, G_TYPE_GSTRING);
  g_assert (G_VALUE_HOLDS_BOXED (&value));

  v = g_string_new ("bla");
  g_value_take_boxed (&value, v);

  v2 = g_value_get_boxed (&value);
  g_assert (v2 == v);

  v2 = g_value_dup_boxed (&value);
  g_assert (v2 != v);
  g_assert (g_string_equal (v, v2));
  g_string_free (v2, TRUE);

  g_value_unset (&value);
}

static void
test_boxed_hashtable (void)
{
  GHashTable *v;
  GHashTable *v2;
  GValue value = G_VALUE_INIT;

  g_value_init (&value, G_TYPE_HASH_TABLE);
  g_assert (G_VALUE_HOLDS_BOXED (&value));

  v = g_hash_table_new (g_str_hash, g_str_equal);
  g_value_take_boxed (&value, v);

  v2 = g_value_get_boxed (&value);
  g_assert (v2 == v);

  v2 = g_value_dup_boxed (&value);
  g_assert (v2 == v);  /* hash tables use ref/unref for copy/free */
  g_hash_table_unref (v2);

  g_value_unset (&value);
}

static void
test_boxed_array (void)
{
  GArray *v;
  GArray *v2;
  GValue value = G_VALUE_INIT;

  g_value_init (&value, G_TYPE_ARRAY);
  g_assert (G_VALUE_HOLDS_BOXED (&value));

  v = g_array_new (TRUE, FALSE, 1);
  g_value_take_boxed (&value, v);

  v2 = g_value_get_boxed (&value);
  g_assert (v2 == v);

  v2 = g_value_dup_boxed (&value);
  g_assert (v2 == v);  /* arrays use ref/unref for copy/free */
  g_array_unref (v2);

  g_value_unset (&value);
}

static void
test_boxed_ptrarray (void)
{
  GPtrArray *v;
  GPtrArray *v2;
  GValue value = G_VALUE_INIT;

  g_value_init (&value, G_TYPE_PTR_ARRAY);
  g_assert (G_VALUE_HOLDS_BOXED (&value));

  v = g_ptr_array_new ();
  g_value_take_boxed (&value, v);

  v2 = g_value_get_boxed (&value);
  g_assert (v2 == v);

  v2 = g_value_dup_boxed (&value);
  g_assert (v2 == v);  /* ptr arrays use ref/unref for copy/free */
  g_ptr_array_unref (v2);

  g_value_unset (&value);
}

static void
test_boxed_regex (void)
{
  GRegex *v;
  GRegex *v2;
  GValue value = G_VALUE_INIT;

  g_value_init (&value, G_TYPE_REGEX);
  g_assert (G_VALUE_HOLDS_BOXED (&value));

  v = g_regex_new ("a+b+", 0, 0, NULL);
  g_value_take_boxed (&value, v);

  v2 = g_value_get_boxed (&value);
  g_assert (v2 == v);

  v2 = g_value_dup_boxed (&value);
  g_assert (v2 == v);  /* regexes use ref/unref for copy/free */
  g_regex_unref (v2);

  g_value_unset (&value);
}

static void
test_boxed_varianttype (void)
{
  GVariantType *v;
  GVariantType *v2;
  GValue value = G_VALUE_INIT;

  g_value_init (&value, G_TYPE_VARIANT_TYPE);
  g_assert (G_VALUE_HOLDS_BOXED (&value));

  v = g_variant_type_new ("mas");
  g_value_take_boxed (&value, v);

  v2 = g_value_get_boxed (&value);
  g_assert (v2 == v);

  v2 = g_value_dup_boxed (&value);
  g_assert (v2 != v);
  g_assert_cmpstr (g_variant_type_peek_string (v), ==, g_variant_type_peek_string (v2));
  g_variant_type_free (v2);

  g_value_unset (&value);
}

static void
test_boxed_datetime (void)
{
  GDateTime *v;
  GDateTime *v2;
  GValue value = G_VALUE_INIT;

  g_value_init (&value, G_TYPE_DATE_TIME);
  g_assert (G_VALUE_HOLDS_BOXED (&value));

  v = g_date_time_new_now_local ();
  g_value_take_boxed (&value, v);

  v2 = g_value_get_boxed (&value);
  g_assert (v2 == v);

  v2 = g_value_dup_boxed (&value);
  g_assert (v2 == v); /* datetime uses ref/unref for copy/free */
  g_date_time_unref (v2);

  g_value_unset (&value);
}

static void
test_boxed_error (void)
{
  GError *v;
  GError *v2;
  GValue value = G_VALUE_INIT;

  g_value_init (&value, G_TYPE_ERROR);
  g_assert (G_VALUE_HOLDS_BOXED (&value));

  v = g_error_new_literal (G_VARIANT_PARSE_ERROR,
                           G_VARIANT_PARSE_ERROR_NUMBER_TOO_BIG,
                           "Too damn big");
  g_value_take_boxed (&value, v);

  v2 = g_value_get_boxed (&value);
  g_assert (v2 == v);

  v2 = g_value_dup_boxed (&value);
  g_assert (v2 != v);
  g_assert_cmpint (v->domain, ==, v2->domain);
  g_assert_cmpint (v->code, ==, v2->code);
  g_assert_cmpstr (v->message, ==, v2->message);
  g_error_free (v2);

  g_value_unset (&value);
}

int
main (int argc, char *argv[])
{
  g_type_init ();
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/boxed/define", test_define_boxed);
  g_test_add_func ("/boxed/ownership", test_boxed_ownership);
  g_test_add_func ("/boxed/closure", test_boxed_closure);
  g_test_add_func ("/boxed/date", test_boxed_date);
  g_test_add_func ("/boxed/value", test_boxed_value);
  g_test_add_func ("/boxed/string", test_boxed_string);
  g_test_add_func ("/boxed/hashtable", test_boxed_hashtable);
  g_test_add_func ("/boxed/array", test_boxed_array);
  g_test_add_func ("/boxed/ptrarray", test_boxed_ptrarray);
  g_test_add_func ("/boxed/regex", test_boxed_regex);
  g_test_add_func ("/boxed/varianttype", test_boxed_varianttype);
  g_test_add_func ("/boxed/error", test_boxed_error);
  g_test_add_func ("/boxed/datetime", test_boxed_datetime);

  return g_test_run ();
}
