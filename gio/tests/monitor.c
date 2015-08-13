#include <gio/gio.h>

typedef struct {
  GFile *file;
  GOutputStream *stream;
  GMainLoop *loop;
  gint state;
} MonitorData;

typedef struct {
  MonitorData *from;
  MonitorData *to;
} MoveMonitorsData;

static gboolean
move_file_idle (gpointer data)
{
  MoveMonitorsData *move_monitor_data = data;
  MonitorData *d_from = move_monitor_data->from;
  MonitorData *d_to = move_monitor_data->to;
  GFile *dest;
  GError *error = NULL;

  g_assert (d_from->state == 0);
  g_assert (d_to->state == 0);

  dest = g_file_get_child  (d_to->file, g_file_get_basename (d_from->file));
  g_print ("\nfrom %s to %s\n", g_file_get_path (d_from->file), g_file_get_path (dest));
  g_file_move (d_from->file, dest, G_FILE_COPY_NO_FALLBACK_FOR_MOVE, NULL, NULL, NULL, &error);
  g_assert_no_error (error);

  d_from->state = 1;
  d_to->state = 1;

  g_object_unref (dest);

  return G_SOURCE_REMOVE;
}

static gboolean
create_file_idle (gpointer data)
{
  MonitorData *d = data;
  GError *error = NULL;

  g_assert (d->state == 0);

  d->stream = (GOutputStream*)g_file_create (d->file, 0, NULL, &error);
  g_assert_no_error (error);

  d->state = 1;

  return G_SOURCE_REMOVE;
}

static gboolean
write_file_idle (gpointer data)
{
  MonitorData *d = data;
  GError *error = NULL;

  g_assert (d->state == 2);

  g_output_stream_write (d->stream, "abcd", 4, NULL, &error);
  g_assert_no_error (error);
  g_object_unref (d->stream);
  d->stream = NULL;

  d->state = 3;


  return G_SOURCE_REMOVE;
}

static gboolean
delete_file_idle (gpointer data)
{
  MonitorData *d = data;
  GError *error = NULL;

  g_assert (d->state == 4);

  g_file_delete (d->file, NULL, &error);
  g_assert_no_error (error);

  d->state = 5;

  return G_SOURCE_REMOVE;
}

static void
changed_move_cb (GFileMonitor      *monitor,
                 GFile             *file,
                 GFile             *other_file,
                 GFileMonitorEvent  event,
                 gpointer           data)
{
  MonitorData *d = data;

      g_print ("\nevent %d ----- %s ------- %s\n",
	       event,
	       g_file_get_path (file),
	       other_file != NULL ? g_file_get_path (other_file) : "no other file");
  switch (event)
    {
    case G_FILE_MONITOR_EVENT_MOVED_IN:
    case G_FILE_MONITOR_EVENT_MOVED_OUT:
      break;
    default:
      //g_assert_not_reached ();
      break;
    }

  d->state = event;
}

static void
changed_cb (GFileMonitor      *monitor,
            GFile             *file,
            GFile             *other_file,
            GFileMonitorEvent  event,
            gpointer           data)
{
  MonitorData *d = data;

  switch (d->state)
    {
    case 1:
      g_assert (event == G_FILE_MONITOR_EVENT_CREATED);
      d->state = 2;
      g_idle_add (write_file_idle, data);
      break;
    case 3:
      g_assert (event == G_FILE_MONITOR_EVENT_CHANGED ||
                event == G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT);
      if (event == G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT)
        {
          d->state = 4;
          g_idle_add (delete_file_idle, data);
        }
      break;
    case 5:
      g_assert (event == G_FILE_MONITOR_EVENT_DELETED);
      d->state = 6;
      if (d->loop)
        g_main_loop_quit (d->loop);
      break;
    default:
      g_assert_not_reached ();
      break;
    }
}

static void
file_changed_cb (GFileMonitor      *monitor,
                 GFile             *file,
                 GFile             *other_file,
                 GFileMonitorEvent  event,
                 gpointer           data)
{
  gint *state = data;

  switch (*state)
    {
    case 0:
      g_assert (event == G_FILE_MONITOR_EVENT_CREATED);
      *state = 1;
      break;
    case 1:
      g_assert (event == G_FILE_MONITOR_EVENT_CHANGED ||
                event == G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT);
      if (event == G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT)
        *state = 2;
      break;
    case 2:
      g_assert (event == G_FILE_MONITOR_EVENT_DELETED);
      *state = 3;
      break;
    default:
      g_assert_not_reached ();
      break;
    }
}

static void
test_directory_monitor (void)
{
  gchar *path;
  GFile *file;
  GFile *child;
  GFileMonitor *dir_monitor;
  GFileMonitor *file_monitor;
  GError *error = NULL;
  MonitorData data;
  gint state;
  GMainLoop *loop;

  path = g_mkdtemp (g_strdup ("file_monitor_XXXXXX"));
  file = g_file_new_for_path (path);
  dir_monitor = g_file_monitor_directory (file, 0, NULL, &error);
  g_assert_no_error (error);

  child = g_file_get_child (file, "test-file");
  file_monitor = g_file_monitor_file (child, 0, NULL, &error);
  g_assert_no_error (error);

  loop = g_main_loop_new (NULL, FALSE);

  g_signal_connect (dir_monitor, "changed", G_CALLBACK (changed_cb), &data);
  g_signal_connect (file_monitor, "changed", G_CALLBACK (file_changed_cb), &state);

  data.loop = loop;
  data.file = child;
  data.state = 0;
  state = 0;

  g_idle_add (create_file_idle, &data);

  g_main_loop_run (loop);

  g_assert_cmpint (data.state, ==, 6);
  g_assert_cmpint (state, ==, 3);

  g_main_loop_unref (loop);
  g_object_unref (dir_monitor);
  g_object_unref (file_monitor);
  g_object_unref (child);
  g_object_unref (file);
  g_free (path);
}

static void
test_directory_moves_monitor (void)
{
  gchar *path;
  GFile *directory_1;
  GFile *directory_2;
  GFile *directory_3;
  GFileMonitor *dir_monitor_1;
  GFileMonitor *dir_monitor_2;
  GFileMonitor *dir_monitor_3;
  GError *error = NULL;
  MonitorData data_1;
  MonitorData data_2;
  MonitorData data_3;
  MoveMonitorsData move_monitor_data;
  GMainLoop *loop;

  loop = g_main_loop_new (NULL, FALSE);

  path = g_mkdtemp (g_strdup ("directory_1_XXXXXX"));
  directory_1 = g_file_new_for_path (path);

  data_1.loop = loop;
  data_1.file = directory_1;
  data_1.state = 0;

  path = g_mkdtemp (g_build_path ("/", path, "directory_2_XXXXXX", NULL));
  directory_2 = g_file_new_for_path (path);

  data_2.loop = loop;
  data_2.file = directory_2;
  data_2.state = 0;

  path = g_mkdtemp (g_build_path ("/", path, "directory_3_XXXXXX", NULL));
  directory_3 = g_file_new_for_path (path);

  data_3.loop = loop;
  data_3.file = directory_3;
  data_3.state = 0;

  dir_monitor_1 = g_file_monitor (directory_1, G_FILE_MONITOR_WATCH_MOVES, NULL, &error);
  g_assert_no_error (error);
  dir_monitor_2 = g_file_monitor (directory_2, G_FILE_MONITOR_WATCH_MOVES, NULL, &error);
  g_assert_no_error (error);
  dir_monitor_3 = g_file_monitor (directory_3, G_FILE_MONITOR_WATCH_MOVES, NULL, &error);
  g_assert_no_error (error);

  g_signal_connect (dir_monitor_1, "changed", G_CALLBACK (changed_move_cb), &data_1);
  g_signal_connect (dir_monitor_2, "changed", G_CALLBACK (changed_move_cb), &data_2);
  g_signal_connect (dir_monitor_3, "changed", G_CALLBACK (changed_move_cb), &data_3);

  move_monitor_data.from = &data_3;
  move_monitor_data.to = &data_1;
  g_idle_add (move_file_idle, &move_monitor_data);

  g_main_loop_run (loop);

  g_assert_cmpint (data_2.state, ==, G_FILE_MONITOR_EVENT_MOVED_OUT);
  g_assert_cmpint (data_1.state, ==, G_FILE_MONITOR_EVENT_MOVED_IN);

  g_main_loop_unref (loop);
  g_object_unref (dir_monitor_1);
  g_object_unref (dir_monitor_2);
  g_object_unref (dir_monitor_3);
  g_free (path);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/monitor/directory", test_directory_monitor);
  g_test_add_func ("/monitor/directory_moves", test_directory_moves_monitor);

  return g_test_run ();
}
