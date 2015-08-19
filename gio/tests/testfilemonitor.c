#include <stdlib.h>
#include <gio/gio.h>

typedef struct
{
  gint event_type;
  gchar *file;
  gchar *other_file;
  gint step;
} RecordedEvent;

static void
free_recorded_event (RecordedEvent *event)
{
  g_free (event->file);
  g_free (event->other_file);
  g_free (event);
}

typedef struct
{
  GFile *file;
  GFileMonitor *monitor;
  GMainLoop *loop;
  gint step;
  GList *events;
  GString *output;
} TestData;

#if 0
static void
output_event (RecordedEvent *event)
{
  if (event->step >= 0)
    g_print (">>>> step %d\n", event->step);
  else
    {
      GTypeClass *class;

      class = g_type_class_ref (g_type_from_name ("GFileMonitorEvent"));
      g_print ("%s file=%s other_file=%s\n",
               g_enum_get_value (G_ENUM_CLASS (class), event->event_type)->value_nick,
               event->file,
               event->other_file);
      g_type_class_unref (class);
    }
}

static void
output_events (GList *list)
{
  GList *l;

  g_print (">>>output events\n");
  for (l = list; l; l = l->next)
    output_event ((RecordedEvent *)l->data);
}
#endif

/* a placeholder for temp file names we don't want to compare */
static const gchar DONT_CARE[] = "";

static void
check_expected_event (gint           i,
                      RecordedEvent *e1,
                      RecordedEvent *e2)
{
  g_assert_cmpint (e1->step, ==, e2->step);
  if (e1->step < 0)
    return;

  g_assert_cmpint (e1->event_type, ==, e2->event_type);

  if (e1->file != DONT_CARE)
    g_assert_cmpstr (e1->file, ==, e2->file);

  if (e1->other_file != DONT_CARE)
    g_assert_cmpstr (e1->other_file, ==, e2->other_file);
}

static void
check_expected_events (RecordedEvent *expected,
                       gsize          n_expected,
                       GList         *recorded)
{
  gint i;
  GList *l;

  g_assert_cmpint (n_expected, ==, g_list_length (recorded));

  for (i = 0, l = recorded; i < n_expected; i++, l = l->next)
    {
      RecordedEvent *e1 = &expected[i];
      RecordedEvent *e2 = (RecordedEvent *)l->data;

      check_expected_event (i, e1, e2);
    }
}

static void
record_event (TestData    *data,
              gint         event_type,
              const gchar *file,
              const gchar *other_file,
              gint         step)
{
  RecordedEvent *event;

  event = g_new0 (RecordedEvent, 1);
  event->event_type = event_type;
  event->file = g_strdup (file);
  event->other_file = g_strdup (other_file);
  event->step = step;

  data->events = g_list_append (data->events, event);
}

static void
monitor_changed (GFileMonitor      *monitor,
                 GFile             *file,
                 GFile             *other_file,
                 GFileMonitorEvent  event_type,
                 gpointer           user_data)
{
  TestData *data = user_data;
  gchar *basename, *other_base;

  basename = g_file_get_basename (file);
  if (other_file)
    other_base = g_file_get_basename (other_file);
  else
    other_base = NULL;

  record_event (data, event_type, basename, other_base, -1);

  g_free (basename);
  g_free (other_base);
}

static gboolean
atomic_replace_step (gpointer user_data)
{
  TestData *data = user_data;
  GError *error = NULL;

  switch (data->step)
    {
    case 0:
      record_event (data, -1, NULL, NULL, 0);
      g_file_replace_contents (data->file, "step 0", 6, NULL, FALSE, G_FILE_CREATE_NONE, NULL, NULL, &error);
      g_assert_no_error (error);
      break;
    case 1:
      record_event (data, -1, NULL, NULL, 1);
      g_file_replace_contents (data->file, "step 1", 6, NULL, FALSE, G_FILE_CREATE_NONE, NULL, NULL, &error);
      g_assert_no_error (error);
      break;
    case 2:
      record_event (data, -1, NULL, NULL, 2);
      g_main_loop_quit (data->loop);
      return G_SOURCE_REMOVE;
    }

  data->step++;

  return G_SOURCE_CONTINUE;
}

/* this is the output we expect from the above steps */
static RecordedEvent atomic_replace_output[] = {
  { -1, NULL, NULL, 0 },
  { G_FILE_MONITOR_EVENT_CREATED, "atomic_replace_file", NULL, -1 },
  { G_FILE_MONITOR_EVENT_CHANGED, "atomic_replace_file", NULL, -1 },
  { G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT, "atomic_replace_file", NULL, -1 },
  { -1, NULL, NULL, 1 },
  { G_FILE_MONITOR_EVENT_RENAMED, (gchar*)DONT_CARE, "atomic_replace_file", -1 },
  { -1, NULL, NULL, 2 },
};

static void
test_atomic_replace (void)
{
  GError *error = NULL;
  TestData data;

  data.output = g_string_new ("");
  data.step = 0;
  data.events = NULL;

  data.file = g_file_new_for_path ("atomic_replace_file");
  g_file_delete (data.file, NULL, NULL);

  data.monitor = g_file_monitor_file (data.file, G_FILE_MONITOR_WATCH_MOVES, NULL, &error);
  g_assert_no_error (error);

  g_file_monitor_set_rate_limit (data.monitor, 200);
  g_signal_connect (data.monitor, "changed", G_CALLBACK (monitor_changed), &data);

  data.loop = g_main_loop_new (NULL, TRUE);

  g_timeout_add (1000, atomic_replace_step, &data);

  g_main_loop_run (data.loop);

  /*output_events (data.events);*/
  check_expected_events (atomic_replace_output, G_N_ELEMENTS (atomic_replace_output), data.events);

  /* clean up */
  g_file_delete (data.file, NULL, NULL);

  g_list_free_full (data.events, (GDestroyNotify)free_recorded_event);
  g_main_loop_unref (data.loop);
  g_object_unref (data.monitor);
  g_object_unref (data.file);
  g_string_free (data.output, TRUE);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/monitor/atomic-replace", test_atomic_replace);

  return g_test_run ();
}
