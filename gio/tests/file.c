#include <gio/gio.h>

static void
test_basic (void)
{
  GFile *file;
  gchar *s;

  file = g_file_new_for_path ("./some/directory/testfile");

  s = g_file_get_basename (file);
  g_assert_cmpstr (s, ==, "testfile");
  g_free (s);

  s = g_file_get_uri (file);
  g_assert (g_str_has_prefix (s, "file://"));
  g_assert (g_str_has_suffix (s, "/some/directory/testfile"));
  g_free (s);

  g_assert (g_file_has_uri_scheme (file, "file"));
  s = g_file_get_uri_scheme (file);
  g_assert_cmpstr (s, ==, "file");
  g_free (s);

  g_object_unref (file);
}

static void
test_parent (void)
{
  GFile *file;
  GFile *file2;
  GFile *parent;
  GFile *root;

  file = g_file_new_for_path ("./some/directory/testfile");
  file2 = g_file_new_for_path ("./some/directory");
  root = g_file_new_for_path ("/");

  g_assert (g_file_has_parent (file, file2));

  parent = g_file_get_parent (file);
  g_assert (g_file_equal (parent, file2));
  g_object_unref (parent);

  g_assert (g_file_get_parent (root) == NULL);

  g_object_unref (file);
  g_object_unref (file2);
  g_object_unref (root);
}

static void
test_child (void)
{
  GFile *file;
  GFile *child;
  GFile *child2;

  file = g_file_new_for_path ("./some/directory");
  child = g_file_get_child (file, "child");
  g_assert (g_file_has_parent (child, file));

  child2 = g_file_get_child_for_display_name (file, "child2", NULL);
  g_assert (g_file_has_parent (child2, file));

  g_object_unref (child);
  g_object_unref (child2);
  g_object_unref (file);
}

static void
test_type (void)
{
  GFile *file;
  GFileType type;

  file = g_file_new_for_path (SRCDIR "/file.c");
  type = g_file_query_file_type (file, 0, NULL);
  g_assert_cmpint (type, ==, G_FILE_TYPE_REGULAR);
  g_object_unref (file);

  file = g_file_new_for_path (SRCDIR "/schema-tests");
  type = g_file_query_file_type (file, 0, NULL);
  g_assert_cmpint (type, ==, G_FILE_TYPE_DIRECTORY);
  g_object_unref (file);
}

int
main (int argc, char *argv[])
{
  g_type_init ();

  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/file/basic", test_basic);
  g_test_add_func ("/file/parent", test_parent);
  g_test_add_func ("/file/child", test_child);
  g_test_add_func ("/file/type", test_type);

  return g_test_run ();
}
