#include <errno.h>
#include <gio/gio.h>
#include <glib/gstdio.h>

#ifdef G_OS_WIN32
/* Only test if we have openat(), otherwise no guarantees */
# define SKIP_TESTS
#endif

#ifndef SKIP_TESTS

static void
cleanup_skeleton (const gchar *dir)
{
  gchar *command = g_strdup_printf ("rm -rf '%s'", dir);
  system (command);
}

static gchar *
create_skeleton (void)
{
  static const gchar *skeleton[] = {
    "a", "a/b", "a/b/c",
  };
  gchar *tmpl = g_strdup ("test-file-enumerator-XXXXXX");
  guint i;

  if (g_mkdtemp (tmpl) == NULL)
    g_error ("Failed to create skeleton directory: %s",
             g_strerror (errno));

  for (i = 0; i < G_N_ELEMENTS (skeleton); i++)
    {
      gchar *path = g_build_filename (tmpl, skeleton[i], NULL);
      if (g_mkdir_with_parents (path, 0750) != 0)
        g_error ("Failed to create directory %s", path);
      g_free (path);
    }

  return g_steal_pointer (&tmpl);
}

static void
test_enumerate_children (void)
{
  gchar *base = create_skeleton ();
  GFile *file = g_file_new_for_path (base);
  GFile *a = g_file_get_child (file, "a");
  GFile *c = g_file_get_child (file, "c");
  GFileEnumerator *enum_a = NULL;
  GFileEnumerator *enum_a_b = NULL;
  GError *error = NULL;
  gboolean ret;

  /*
   * - Create an enumerator for "a"
   * - Move "a" to "c"
   * - Make sure "b" enumerator can be created from enum_a
   */

  enum_a = g_file_enumerate_children (a,
                                      G_FILE_ATTRIBUTE_STANDARD_NAME,
                                      G_FILE_QUERY_INFO_NONE,
                                      NULL, &error);
  g_assert_no_error (error);
  g_assert_true (G_IS_FILE_ENUMERATOR (enum_a));

  /* Create child enumerator, ensure it works */
  enum_a_b = g_file_enumerator_enumerate_children (enum_a,
                                                   "b",
                                                   G_FILE_ATTRIBUTE_STANDARD_NAME,
                                                   G_FILE_QUERY_INFO_NONE,
                                                   NULL, &error);
  g_assert_no_error (error);
  g_assert_true (G_IS_FILE_ENUMERATOR (enum_a_b));
  g_clear_object (&enum_a_b);

  ret = g_file_move (a, c, G_FILE_COPY_NONE, NULL, NULL, NULL, &error);
  g_assert_no_error (error);
  g_assert_true (ret);

  /* Create child enumerator, ensure it works */
  enum_a_b = g_file_enumerator_enumerate_children (enum_a,
                                                   "b",
                                                   G_FILE_ATTRIBUTE_STANDARD_NAME,
                                                   G_FILE_QUERY_INFO_NONE,
                                                   NULL, &error);
  g_assert_no_error (error);
  g_assert_true (G_IS_FILE_ENUMERATOR (enum_a_b));
  g_clear_object (&enum_a_b);

  /* New a enumerator should fail */
  g_clear_object (&enum_a);
  enum_a = g_file_enumerate_children (a,
                                      G_FILE_ATTRIBUTE_STANDARD_NAME,
                                      G_FILE_QUERY_INFO_NONE,
                                      NULL, &error);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND);
  g_assert_null (enum_a);

  g_clear_object (&enum_a);
  g_clear_object (&enum_a_b);
  g_clear_object (&a);
  g_clear_object (&c);
  g_clear_object (&file);
  cleanup_skeleton (base);
  g_free (base);
}

static void
enumerate_children_cb (GObject      *object,
                       GAsyncResult *result,
                       gpointer      user_data)
{
  GMainLoop *main_loop = user_data;
  GFileEnumerator *enumerator = G_FILE_ENUMERATOR (object);
  GFileEnumerator *child_enumerator;
  GError *error = NULL;

  child_enumerator = g_file_enumerator_enumerate_children_finish (enumerator, result, &error);
  g_assert_no_error (error);
  g_assert_true (G_IS_FILE_ENUMERATOR (child_enumerator));

  g_main_loop_quit (main_loop);
  g_main_loop_unref (main_loop);
}

static void
test_enumerate_children_async (void)
{
  GMainLoop *main_loop = g_main_loop_new (NULL, FALSE);
  gchar *base = create_skeleton ();
  GFile *file = g_file_new_for_path (base);
  GFile *a = g_file_get_child (file, "a");
  GFile *c = g_file_get_child (file, "c");
  GFileEnumerator *enum_a = NULL;
  GFileEnumerator *enum_a_b = NULL;
  GError *error = NULL;
  gboolean ret;

  /*
   * - Create an enumerator for "a"
   * - Move "a" to "c"
   * - Make sure "b" enumerator can be created from enum_a
   */

  enum_a = g_file_enumerate_children (a,
                                      G_FILE_ATTRIBUTE_STANDARD_NAME,
                                      G_FILE_QUERY_INFO_NONE,
                                      NULL, &error);
  g_assert_no_error (error);
  g_assert_true (G_IS_FILE_ENUMERATOR (enum_a));

  /* Create child enumerator, ensure it works */
  enum_a_b = g_file_enumerator_enumerate_children (enum_a,
                                                   "b",
                                                   G_FILE_ATTRIBUTE_STANDARD_NAME,
                                                   G_FILE_QUERY_INFO_NONE,
                                                   NULL, &error);
  g_assert_no_error (error);
  g_assert_true (G_IS_FILE_ENUMERATOR (enum_a_b));
  g_clear_object (&enum_a_b);

  ret = g_file_move (a, c, G_FILE_COPY_NONE, NULL, NULL, NULL, &error);
  g_assert_no_error (error);
  g_assert_true (ret);

  /* Create child enumerator, ensure it works */
  g_file_enumerator_enumerate_children_async (enum_a,
                                              "b",
                                              G_FILE_ATTRIBUTE_STANDARD_NAME,
                                              G_FILE_QUERY_INFO_NONE,
                                              G_PRIORITY_DEFAULT,
                                              NULL,
                                              enumerate_children_cb,
                                              g_main_loop_ref (main_loop));

  g_main_loop_run (main_loop);

  g_clear_object (&enum_a);
  g_clear_object (&enum_a_b);
  g_clear_object (&a);
  g_clear_object (&c);
  g_clear_object (&file);
  cleanup_skeleton (base);
  g_free (base);

  g_main_loop_unref (main_loop);
}

#endif

gint
main (gint   argc,
      gchar *argv[])
{
  g_test_init (&argc, &argv, NULL);

#ifndef SKIP_TESTS
  g_test_add_func ("/Gio/LocalFileEnumerator/enumerate_children", test_enumerate_children);
  g_test_add_func ("/Gio/LocalFileEnumerator/enumerate_children_async", test_enumerate_children_async);
#endif

  return g_test_run ();
}
