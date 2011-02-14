#include <glib.h>
#include <string.h>

static void
test_empty (void)
{
  GMappedFile *file;
  GError *error;

  error = NULL;
  file = g_mapped_file_new ("empty", FALSE, &error);
  g_assert_no_error (error);

  g_assert (g_mapped_file_get_contents (file) == NULL);

  g_mapped_file_free (file);
}

static void
test_nonexisting (void)
{
  GMappedFile *file;
  GError *error;

  error = NULL;
  file = g_mapped_file_new ("no-such-file", FALSE, &error);
  g_assert_error (error, G_FILE_ERROR, G_FILE_ERROR_NOENT);
  g_clear_error (&error);
  g_assert (file == NULL);
}

static void
test_writable (void)
{
  GMappedFile *file;
  GError *error;
  gchar *contents;
  const gchar *old = "MMMMMMMMMMMMMMMMMMMMMMMMM";
  const gchar *new = "abcdefghijklmnopqrstuvxyz";

  error = NULL;
  file = g_mapped_file_new ("4096-random-bytes", TRUE, &error);
  g_assert_no_error (error);

  contents = g_mapped_file_get_contents (file);
  g_assert (strncmp (contents, old, strlen (old)) == 0);

  memcpy (contents, new, strlen (new));
  g_assert (strncmp (contents, new, strlen (new)) == 0);

  g_mapped_file_free (file);

  error = NULL;
  file = g_mapped_file_new ("4096-random-bytes", TRUE, &error);
  g_assert_no_error (error);

  contents = g_mapped_file_get_contents (file);
  g_assert (strncmp (contents, old, strlen (old)) == 0);

  g_mapped_file_free (file);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/mappedfile/empty", test_empty);
  g_test_add_func ("/mappedfile/nonexisting", test_nonexisting);
  g_test_add_func ("/mappedfile/writable", test_writable);

  return g_test_run ();
}
