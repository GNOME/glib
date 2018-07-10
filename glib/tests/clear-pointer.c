#include <glib.h>

static void
test_g_clear_pointer_cast (void)
{
  GHashTable *hash_table = NULL;

  g_test_bug ("1425");

  hash_table = g_hash_table_new (g_str_hash, g_str_equal);

  g_assert_nonnull (hash_table);

  g_clear_pointer (&hash_table, (void (*) (GHashTable *)) g_hash_table_destroy);

  g_assert_null (hash_table);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  g_test_bug_base ("https://gitlab.gnome.org/GNOME/glib/issues/");

  g_test_add_func ("/clear_pointer/cast", test_g_clear_pointer_cast);

  return g_test_run ();
}
