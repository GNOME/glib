#include <gio/gio.h>

static gchar **gio_watch_dirs;
static gchar **gio_watch_files;
static gchar **gio_watch_direct;
static gchar **gio_watch_silent;
static gchar **gio_watch_default;
static gboolean gio_watch_no_moves;
static gboolean gio_watch_mounts;

static const GOptionEntry gio_watch_entries[] = {
  { "dir", 'd', 0, G_OPTION_ARG_FILENAME_ARRAY, &gio_watch_dirs,
      "Monitor a directory (default: depends on type)", "FILENAME" },
  { "file", 'f', 0, G_OPTION_ARG_FILENAME_ARRAY, &gio_watch_files,
      "Monitor a file (default: depends on type)", "FILENAME" },
  { "direct", 'D', 0, G_OPTION_ARG_FILENAME_ARRAY, &gio_watch_direct,
      "Monitor a file directly (notices changes made via hardlinks)", "FILENAME" },
  { "silent", 's', 0, G_OPTION_ARG_FILENAME_ARRAY, &gio_watch_silent,
      "Monitors a file directly, but doesn't report changes", "FILENAME" },
  { "no-moves", 'n', 0, G_OPTION_ARG_NONE, &gio_watch_no_moves,
      "Report moves and renames as simple deleted/created events" },
  { "mounts", 'm', 0, G_OPTION_ARG_NONE, &gio_watch_mounts,
      "Watch for mount events" },
  { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &gio_watch_default },
  { NULL }
};

static void
gio_watch_callback (GFileMonitor      *monitor,
                    GFile             *child,
                    GFile             *other,
                    GFileMonitorEvent  event_type,
                    gpointer           user_data)
{
  gchar *child_str;
  gchar *other_str;

  g_assert (child);

  if (g_file_is_native (child))
    child_str = g_file_get_path (child);
  else
    child_str = g_file_get_uri (child);

  if (other)
    {
      if (g_file_is_native (other))
        other_str = g_file_get_path (other);
      else
        other_str = g_file_get_uri (other);
    }
  else
    other_str = g_strdup ("(none)");

  g_print ("%s: ", (gchar *) user_data);
  switch (event_type)
    {
    case G_FILE_MONITOR_EVENT_CHANGED:
      g_assert (!other);
      g_print ("%s: changed", child_str);
      break;
    case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT:
      g_assert (!other);
      g_print ("%s: changes done", child_str);
      break;
    case G_FILE_MONITOR_EVENT_DELETED:
      g_assert (!other);
      g_print ("%s: deleted", child_str);
      break;
    case G_FILE_MONITOR_EVENT_CREATED:
      g_assert (!other);
      g_print ("%s: created", child_str);
      break;
    case G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED:
      g_assert (!other);
      g_print ("%s: attributes changed", child_str);
      break;
    case G_FILE_MONITOR_EVENT_PRE_UNMOUNT:
      g_assert (!other);
      g_print ("%s: pre-unmount", child_str);
      break;
    case G_FILE_MONITOR_EVENT_UNMOUNTED:
      g_assert (!other);
      g_print ("%s: unmounted", child_str);
      break;
    case G_FILE_MONITOR_EVENT_MOVED_IN:
      g_print ("%s: moved in", child_str);
      if (other)
        g_print (" (from %s)", other_str);
      break;
    case G_FILE_MONITOR_EVENT_MOVED_OUT:
      g_print ("%s: moved out", child_str);
      if (other)
        g_print (" (to %s)", other_str);
      break;
    case G_FILE_MONITOR_EVENT_RENAMED:
      g_assert (other);
      g_print ("%s: renamed to %s\n", child_str, other_str);
      break;

    case G_FILE_MONITOR_EVENT_MOVED:
    default:
      g_assert_not_reached ();
    }

  g_free (child_str);
  g_free (other_str);
  g_print ("\n");
}

typedef enum
{
  WATCH_DIR,
  WATCH_FILE,
  WATCH_AUTO
} WatchType;

static gboolean
add_watch (const gchar       *cmdline,
           WatchType          watch_type,
           GFileMonitorFlags  flags,
           gboolean           connect_handler)
{
  GFileMonitor *monitor = NULL;
  GError *error = NULL;
  GFile *file;

  file = g_file_new_for_commandline_arg (cmdline);

  if (watch_type == WATCH_AUTO)
    {
      GFileInfo *info;
      guint32 type;

      info = g_file_query_info (file, G_FILE_ATTRIBUTE_STANDARD_TYPE, G_FILE_QUERY_INFO_NONE, NULL, &error);
      if (!info)
        goto err;

      type = g_file_info_get_attribute_uint32 (info, G_FILE_ATTRIBUTE_STANDARD_TYPE);
      watch_type = (type == G_FILE_TYPE_DIRECTORY) ? WATCH_DIR : WATCH_FILE;
    }

  if (watch_type == WATCH_DIR)
    monitor = g_file_monitor_directory (file, flags, NULL, &error);
  else
    monitor = g_file_monitor (file, flags, NULL, &error);

  if (!monitor)
    goto err;

  if (connect_handler)
    g_signal_connect (monitor, "changed", G_CALLBACK (gio_watch_callback), g_strdup (cmdline));

  monitor = NULL; /* leak */

  return TRUE;

err:
  g_printerr ("error: %s: %s", cmdline, error->message);
  g_error_free (error);

  return FALSE;
}

static int
gio_watch (int     argc,
           gchar **argv)
{
  GOptionContext *context;
  GError *error = NULL;
  GFileMonitorFlags flags;
  guint total = 0;
  guint i;

  context = g_option_context_new ("FILENAMES... - monitor files and directories");
  g_option_context_add_main_entries (context, gio_watch_entries, NULL);
  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      g_printerr ("option parsing failed: %s\n", error->message);
      g_error_free (error);
      return 1;
    }

  flags = (gio_watch_no_moves ? 0 : G_FILE_MONITOR_WATCH_MOVES) |
          (gio_watch_mounts ? G_FILE_MONITOR_WATCH_MOUNTS : 0);

  if (gio_watch_dirs)
    {
      for (i = 0; gio_watch_dirs[i]; i++)
        if (!add_watch (gio_watch_dirs[i], WATCH_DIR, flags, TRUE))
          return 1;
      total++;
    }

  if (gio_watch_files)
    {
      for (i = 0; gio_watch_files[i]; i++)
        if (!add_watch (gio_watch_files[i], WATCH_FILE, flags, TRUE))
          return 1;
      total++;
    }

  if (gio_watch_direct)
    {
      for (i = 0; gio_watch_direct[i]; i++)
        if (!add_watch (gio_watch_direct[i], WATCH_FILE, flags | G_FILE_MONITOR_WATCH_HARD_LINKS, TRUE))
          return 1;
      total++;
    }

  if (gio_watch_silent)
    {
      for (i = 0; gio_watch_silent[i]; i++)
        if (!add_watch (gio_watch_silent[i], WATCH_FILE, flags | G_FILE_MONITOR_WATCH_HARD_LINKS, FALSE))
          return 1;
      total++;
    }

  if (gio_watch_default)
    {
      for (i = 0; gio_watch_default[i]; i++)
        if (!add_watch (gio_watch_default[i], WATCH_AUTO, flags, TRUE))
          return 1;
      total++;
    }

  if (!total)
    {
      g_printerr ("error: must give at least one file to monitor\n");
      return 1;
    }

  while (TRUE)
    g_main_context_iteration (NULL, TRUE);
}

int
main (int    argc,
      char **argv)
{
  gchar *cmd;

  if (argc < 2 || !g_str_equal (argv[1], "watch"))
    {
      g_printerr ("must say 'watch'\n");
      return 1;

    }

  cmd = g_strconcat (argv[0], " ", argv[1], NULL);
  argv[1] = cmd;

  return gio_watch (argc - 1, argv + 1);
}
