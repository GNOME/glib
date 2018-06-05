#include <glib.h>

#ifndef G_OS_UNIX
#error This is a Unix-specific test
#endif

#include <gio/gio.h>
#include <gio/gunixmounts.h>

static void
test_trash_not_supported (void)
{
  GFile *file;
  GFileIOStream *stream;
  GUnixMountEntry *mount;
  GFileInfo *info;
  GError *error = NULL;

  /* The test assumes that tmp file is located on system internal mount. */
  file = g_file_new_tmp ("test-trashXXXXXX", &stream, &error);
  g_assert_no_error (error);

  g_assert_true (g_file_query_exists (file, NULL));

  mount = g_unix_mount_for (g_file_peek_path (file), NULL);
  g_assert_nonnull (mount);

  g_assert (g_unix_mount_is_system_internal (mount));
  g_unix_mount_free (mount);

  /* g_file_trash() shouldn't be supported on system internal mounts,
   * because those are not monitored by gvfsd-trash.
   */
  g_file_trash (file, NULL, &error);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED);
  g_test_message ("Error: %s", error->message);
  g_clear_error (&error);

  info = g_file_query_info (file,
                            G_FILE_ATTRIBUTE_ACCESS_CAN_TRASH,
                            G_FILE_QUERY_INFO_NONE,
                            NULL,
                            &error);
  g_assert_no_error (error);

  g_assert_false (g_file_info_get_attribute_boolean (info,
                                                     G_FILE_ATTRIBUTE_ACCESS_CAN_TRASH));

  g_io_stream_close (G_IO_STREAM (stream), NULL, &error);
  g_assert_no_error (error);

  g_object_unref (stream);
  g_object_unref (file);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_bug_base ("htps://gitlab.gnome.org/GNOME/glib/issues/");
  g_test_bug ("251");

  g_test_add_func ("/trash/not-supported", test_trash_not_supported);

  return g_test_run ();
}

