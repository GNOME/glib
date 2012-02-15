#define GLIB_DISABLE_DEPRECATION_WARNINGS

#include <config.h>
#include <glib.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

static void
test_basic (void)
{
  GBytes *file;
  GError *error;

  error = NULL;
  file = g_mapped_file_new (SRCDIR "/empty", FALSE, &error);
  g_assert_no_error (error);

  g_bytes_ref (file);
  g_bytes_unref (file);

  g_bytes_unref (file);
}

static void
test_empty (void)
{
  GBytes *file;
  GError *error;

  error = NULL;
  file = g_mapped_file_new (SRCDIR "/empty", FALSE, &error);
  g_assert_no_error (error);

  g_assert (g_bytes_get_data (file, NULL) == NULL);

  g_mapped_file_free (file);
}

static void
test_device (void)
{
  GError *error = NULL;
  GBytes *file;

  file = g_mapped_file_new ("/dev/null", FALSE, &error);
  g_assert_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL);
  g_assert (file == NULL);
  g_error_free (error);
}

static void
test_nonexisting (void)
{
  GBytes *file;
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
  GBytes *file;
  GError *error;
  gchar *contents;
  gsize size;
  const gchar *old = "MMMMMMMMMMMMMMMMMMMMMMMMM";
  const gchar *new = "abcdefghijklmnopqrstuvxyz";

  if (access (SRCDIR "/4096-random-bytes", W_OK) != 0)
    {
      g_test_message ("Skipping writable mapping test");
      return;
    }

  error = NULL;
  file = g_mapped_file_new (SRCDIR "/4096-random-bytes", TRUE, &error);
  g_assert_no_error (error);

  contents = (gchar *) g_bytes_get_data (file, &size);
  g_assert_cmpint (size, ==, 4096);
  g_assert (strncmp (contents, old, strlen (old)) == 0);

  memcpy (contents, new, strlen (new));
  g_assert (strncmp (contents, new, strlen (new)) == 0);

  g_mapped_file_free (file);

  error = NULL;
  file = g_mapped_file_new (SRCDIR "/4096-random-bytes", TRUE, &error);
  g_assert_no_error (error);

  contents = (gchar *) g_bytes_get_data (file, &size);
  g_assert_cmpint (size, ==, 4096);
  g_assert (strncmp (contents, old, strlen (old)) == 0);

  g_mapped_file_free (file);
}

static void
test_writable_fd (void)
{
  GBytes *file;
  GError *error;
  gchar *contents;
  gsize size;
  const gchar *old = "MMMMMMMMMMMMMMMMMMMMMMMMM";
  const gchar *new = "abcdefghijklmnopqrstuvxyz";
  int fd;

  if (access (SRCDIR "/4096-random-bytes", W_OK) != 0)
    {
      g_test_message ("Skipping writable mapping test");
      return;
    }

  error = NULL;
  fd = open (SRCDIR "/4096-random-bytes", O_RDWR, 0);
  g_assert (fd != -1);
  file = g_mapped_file_new_from_fd (fd, TRUE, &error);
  g_assert_no_error (error);

  contents = (gchar *) g_bytes_get_data (file, &size);
  g_assert_cmpint (size, ==, 4096);
  g_assert (strncmp (contents, old, strlen (old)) == 0);

  memcpy (contents, new, strlen (new));
  g_assert (strncmp (contents, new, strlen (new)) == 0);

  g_mapped_file_free (file);
  close (fd);

  error = NULL;
  fd = open (SRCDIR "/4096-random-bytes", O_RDWR, 0);
  g_assert (fd != -1);
  file = g_mapped_file_new_from_fd (fd, TRUE, &error);
  g_assert_no_error (error);

  contents = (gchar *) g_bytes_get_data (file, &size);
  g_assert_cmpint (size, ==, 4096);
  g_assert (strncmp (contents, old, strlen (old)) == 0);

  g_mapped_file_free (file);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/mappedfile/basic", test_basic);
  g_test_add_func ("/mappedfile/empty", test_empty);
  g_test_add_func ("/mappedfile/device", test_device);
  g_test_add_func ("/mappedfile/nonexisting", test_nonexisting);
  g_test_add_func ("/mappedfile/writable", test_writable);
  g_test_add_func ("/mappedfile/writable_fd", test_writable_fd);

  return g_test_run ();
}
