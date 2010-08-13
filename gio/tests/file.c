#include <string.h>
#include <gio/gio.h>
#include <gio/gfiledescriptorbased.h>

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


typedef struct
{
  GFile *file;
  GFileMonitor *monitor;
  GOutputStream *ostream;
  GInputStream *istream;
  GMainLoop *loop;

  gint monitor_created;
  gint monitor_deleted;
  gint monitor_changed;
  gchar *monitor_path;
  gchar *data;
  gchar *buffer;
} CreateDeleteData;

static void
monitor_changed (GFileMonitor      *monitor,
                 GFile             *file,
                 GFile             *other_file,
                 GFileMonitorEvent  event_type,
                 gpointer           user_data)
{
  CreateDeleteData *data = user_data;

  g_assert_cmpstr (data->monitor_path, ==, g_file_get_path (file));

  if (event_type == G_FILE_MONITOR_EVENT_CREATED)
    data->monitor_created++;
  if (event_type == G_FILE_MONITOR_EVENT_DELETED)
    data->monitor_deleted++;
  if (event_type == G_FILE_MONITOR_EVENT_CHANGED)
    data->monitor_changed++;
}


static gboolean
quit_idle (gpointer user_data)
{
  CreateDeleteData *data = user_data;

  g_main_loop_quit (data->loop);

  return FALSE;
}

static void
iclosed_cb (GObject      *source,
            GAsyncResult *res,
            gpointer      user_data)
{
  CreateDeleteData *data = user_data;
  GError *error;
  gboolean ret;

  error = NULL;
  ret = g_input_stream_close_finish (data->istream, res, &error);
  g_assert_no_error (error);
  g_assert (ret);

  g_assert (g_input_stream_is_closed (data->istream));

  ret = g_file_delete (data->file, NULL, &error);
  g_assert (ret);
  g_assert_no_error (error);

  /* work around file monitor bug:
   * inotify events are only processed every 1000 ms, regardless
   * of the rate limit set on the file monitor
   */
  g_timeout_add (2000, quit_idle, data);
}

static void
read_cb (GObject      *source,
         GAsyncResult *res,
         gpointer      user_data)
{
  CreateDeleteData *data = user_data;
  GError *error;
  gssize size;

  error = NULL;
  size = g_input_stream_read_finish (data->istream, res, &error);
  g_assert_no_error (error);
  g_assert_cmpint (size, ==, strlen (data->data));
  g_assert_cmpstr (data->buffer, ==, data->data);

  g_assert (!g_input_stream_is_closed (data->istream));

  g_input_stream_close_async (data->istream, 0, NULL, iclosed_cb, data);
}

static void
opened_cb (GObject      *source,
           GAsyncResult *res,
           gpointer      user_data)
{
  CreateDeleteData *data = user_data;
  GError *error;

  error = NULL;
  data->istream = (GInputStream *)g_file_read_finish (data->file, res, &error);
  g_assert_no_error (error);

  data->buffer = g_new0 (gchar, strlen (data->data) + 1);
  g_input_stream_read_async (data->istream,
                             data->buffer,
                             strlen (data->data),
                             0,
                             NULL,
                             read_cb,
                             data);
}

static void
oclosed_cb (GObject      *source,
            GAsyncResult *res,
            gpointer      user_data)
{
  CreateDeleteData *data = user_data;
  GError *error;
  gboolean ret;

  error = NULL;
  ret = g_output_stream_close_finish (data->ostream, res, &error);
  g_assert_no_error (error);
  g_assert (ret);
  g_assert (g_output_stream_is_closed (data->ostream));

  g_file_read_async (data->file, 0, NULL, opened_cb, data);
}

static void
written_cb (GObject      *source,
            GAsyncResult *res,
            gpointer      user_data)
{
  CreateDeleteData *data = user_data;
  gssize size;
  GError *error;

  error = NULL;
  size = g_output_stream_write_finish (data->ostream, res, &error);
  g_assert_no_error (error);
  g_assert_cmpint (size, ==, strlen (data->data));
  g_assert (!g_output_stream_is_closed (data->ostream));

  g_output_stream_close_async (data->ostream, 0, NULL, oclosed_cb, data);
}

static void
created_cb (GObject      *source,
            GAsyncResult *res,
            gpointer      user_data)
{
  CreateDeleteData *data = user_data;
  GError *error;

  error = NULL;
  data->ostream = (GOutputStream *)g_file_create_finish (G_FILE (source), res, &error);
  g_assert_no_error (error);
  g_assert (g_file_query_exists  (data->file, NULL));

  data->data = "abcdefghijklmnopqrstuvxyz";
  g_output_stream_write_async (data->ostream,
                               data->data,
                               strlen (data->data),
                               0,
                               NULL,
                               written_cb,
                               data);
}

static gboolean
stop_timeout (gpointer data)
{
  g_assert_not_reached ();

  return FALSE;
}

/*
 * This test does a fully async create-write-read-delete.
 * Callbackistan.
 */
static void
test_create_delete (void)
{
  gint fd;
  GError *error;
  CreateDeleteData *data;

  data = g_new0 (CreateDeleteData, 1);

  error = NULL;
  fd = g_file_open_tmp ("g_file_create_delete_XXXXXX", &data->monitor_path, &error);
  g_assert_no_error (error);
  unlink (data->monitor_path);
  close (fd);

  data->file = g_file_new_for_path (data->monitor_path);
  g_assert (!g_file_query_exists  (data->file, NULL));

  data->monitor = g_file_monitor_file (data->file, 0, NULL, &error);
  g_file_monitor_set_rate_limit (data->monitor, 100);
  g_assert_no_error (error);

  g_signal_connect (data->monitor, "changed", G_CALLBACK (monitor_changed), data);

  data->loop = g_main_loop_new (NULL, FALSE);

  g_timeout_add (5000, stop_timeout, NULL);

  g_file_create_async (data->file, 0, 0, NULL, created_cb, data);

  g_main_loop_run (data->loop);

  g_assert_cmpint (data->monitor_created, ==, 1);
  g_assert_cmpint (data->monitor_deleted, ==, 1);
  g_assert_cmpint (data->monitor_changed, >, 0);

  g_main_loop_unref (data->loop);
  g_file_monitor_cancel (data->monitor);
  g_object_unref (data->monitor);
  g_object_unref (data->ostream);
  g_object_unref (data->istream);
  g_object_unref (data->file);
  g_free (data->monitor_path);
  g_free (data->buffer);
  g_free (data);
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
  g_test_add_func ("/file/create-delete", test_create_delete);

  return g_test_run ();
}
