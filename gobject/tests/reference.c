#include <glib-object.h>

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

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);

  g_type_init ();

  g_test_add_func ("/object/clear", test_clear);

  return g_test_run ();
}
