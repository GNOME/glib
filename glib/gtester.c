/* GLib testing framework runner
 * Copyright (C) 2007 Sven Herzberg
 * Copyright (C) 2007 Tim Janik
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#include <glib.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

/* the read buffer size in bytes */
#define READ_BUFFER_SIZE 4096

/* --- prototypes --- */
static void     parse_args      (gint           *argc_p,
                                 gchar        ***argv_p);

/* --- variables --- */
static GIOChannel  *ioc_report = NULL;
static gboolean     subtest_running = FALSE;
static gboolean     subtest_io_pending = FALSE;
static gboolean     gtester_quiet = FALSE;
static gboolean     gtester_verbose = FALSE;
static gboolean     gtester_list_tests = FALSE;
static gboolean     subtest_mode_fatal = FALSE;
static gboolean     subtest_mode_perf = FALSE;
static gboolean     subtest_mode_quick = TRUE;
static const gchar *subtest_seedstr = NULL;
static GSList      *subtest_paths = NULL;
static const gchar *outpu_filename = NULL;

/* --- functions --- */
static gboolean
child_report_cb (GIOChannel  *source,
                 GIOCondition condition,
                 gpointer     data)
{
  GIOStatus status = G_IO_STATUS_NORMAL;

  while (status == G_IO_STATUS_NORMAL)
    {
      gchar buffer[READ_BUFFER_SIZE];
      gsize length = 0;
      GError *error = NULL;
      status = g_io_channel_read_chars (source, buffer, sizeof (buffer), &length, &error);
      switch (status)
        {
        case G_IO_STATUS_NORMAL:
          write (2, buffer, length); /* passthrough child's stdout */
          break;
        case G_IO_STATUS_AGAIN:
          /* retry later */
          break;
        case G_IO_STATUS_ERROR:
          /* ignore, child closed fd or similar g_warning ("Error while reading data: %s", error->message); */
          /* fall through into EOF */
        case G_IO_STATUS_EOF:
          subtest_io_pending = FALSE;
          return FALSE;
        }
      g_clear_error (&error);
    }
  return TRUE;
}

static void
child_watch_cb (GPid     pid,
		gint     status,
		gpointer data)
{
  g_spawn_close_pid (pid);
  subtest_running = FALSE;
}

static gchar*
queue_gfree (GSList **slistp,
             gchar   *string)
{
  *slistp = g_slist_prepend (*slistp, string);
  return string;
}

static void
launch_test (const char *binary)
{
  GSList *slist, *free_list = NULL;
  GError *error = NULL;
  const gchar *argv[20 + g_slist_length (subtest_paths)];
  GPid pid = 0;
  gint i = 0, child_report = -1;

  /* setup argv */
  argv[i++] = binary;
  // argv[i++] = "--quiet";
  if (!subtest_mode_fatal)
    argv[i++] = "--keep-going";
  if (subtest_mode_quick)
    argv[i++] = "-m=quick";
  else
    argv[i++] = "-m=slow";
  if (subtest_mode_perf)
    argv[i++] = "-m=perf";
  if (subtest_seedstr)
    argv[i++] = queue_gfree (&free_list, g_strdup_printf ("--seed=%s", subtest_seedstr));
  for (slist = subtest_paths; slist; slist = slist->next)
    argv[i++] = queue_gfree (&free_list, g_strdup_printf ("-p=%s", (gchar*) slist->data));
  argv[i++] = NULL;

  /* child_report will be used to capture logging information from the
   * child binary. for the moment, we just use it to replicate stdout.
   */

  g_spawn_async_with_pipes (NULL, /* g_get_current_dir() */
                            (gchar**) argv,
                            NULL, /* envp */
                            G_SPAWN_DO_NOT_REAP_CHILD, /* G_SPAWN_SEARCH_PATH */
                            NULL, NULL, /* child_setup, user_data */
                            &pid,
                            NULL,       /* standard_input */
                            &child_report, /* standard_output */
                            NULL,       /* standard_error */
                            &error);
  g_slist_foreach (free_list, (void(*)(void*,void*)) g_free, NULL);
  g_slist_free (free_list);
  free_list = NULL;

  if (error)
    {
      if (subtest_mode_fatal)
        g_error ("Failed to execute test binary: %s: %s", argv[0], error->message);
      else
        g_warning ("Failed to execute test binary: %s: %s", argv[0], error->message);
      g_clear_error (&error);
      return;
    }
  subtest_running = TRUE;
  subtest_io_pending = TRUE;

  if (child_report >= 0)
    {
      ioc_report = g_io_channel_unix_new (child_report);
      g_io_channel_set_flags (ioc_report, G_IO_FLAG_NONBLOCK, NULL);
      g_io_add_watch_full (ioc_report, G_PRIORITY_DEFAULT - 1, G_IO_IN | G_IO_ERR | G_IO_HUP, child_report_cb, NULL, NULL);
      g_io_channel_unref (ioc_report);
    }
  g_child_watch_add_full (G_PRIORITY_DEFAULT + 1, pid, child_watch_cb, NULL, NULL);

  while (subtest_running ||             /* FALSE once child exits */
         subtest_io_pending ||          /* FALSE once ioc_report closes */
         g_main_context_pending (NULL)) /* TRUE while idler, etc are running */
    g_main_context_iteration (NULL, TRUE);
}

static void
usage (gboolean just_version)
{
  if (just_version)
    {
      g_print ("gtester version %d.%d.%d\n", GLIB_MAJOR_VERSION, GLIB_MINOR_VERSION, GLIB_MICRO_VERSION);
      return;
    }
  g_print ("Usage: gtester [OPTIONS] testprogram...\n");
  /*        12345678901234567890123456789012345678901234567890123456789012345678901234567890 */
  g_print ("Options:\n");
  g_print ("  -h, --help                  show this help message\n");
  g_print ("  -v, --version               print version informations\n");
  g_print ("  --g-fatal-warnings          make warnings fatal (abort)\n");
  g_print ("  -k, --keep-going            continue running after tests failed\n");
  g_print ("  -l                          list paths of available test cases\n");
  g_print ("  -m=perf, -m=slow, -m=quick  run test cases in mode perf, slow or quick (default)\n");
  g_print ("  -p=TESTPATH                 only start test cases matching TESTPATH\n");
  g_print ("  --seed=SEEDSTRING           start all tests with random number seed SEEDSTRING\n");
  g_print ("  -o=LOGFILE                  write the test log to LOGFILE\n");
  g_print ("  -q, --quiet                 suppress unnecessary output\n");
  g_print ("  --verbose                   produce additional output\n");
}

static void
parse_args (gint    *argc_p,
            gchar ***argv_p)
{
  guint argc = *argc_p;
  gchar **argv = *argv_p;
  guint i, e;
  /* parse known args */
  for (i = 1; i < argc; i++)
    {
      if (strcmp (argv[i], "--g-fatal-warnings") == 0)
        {
          GLogLevelFlags fatal_mask = (GLogLevelFlags) g_log_set_always_fatal ((GLogLevelFlags) G_LOG_FATAL_MASK);
          fatal_mask = (GLogLevelFlags) (fatal_mask | G_LOG_LEVEL_WARNING | G_LOG_LEVEL_CRITICAL);
          g_log_set_always_fatal (fatal_mask);
          argv[i] = NULL;
        }
      else if (strcmp (argv[i], "-h") == 0 || strcmp (argv[i], "--help") == 0)
        {
          usage (FALSE);
          exit (0);
          argv[i] = NULL;
        }
      else if (strcmp (argv[i], "-v") == 0 || strcmp (argv[i], "--version") == 0)
        {
          usage (TRUE);
          exit (0);
          argv[i] = NULL;
        }
      else if (strcmp (argv[i], "--keep-going") == 0 ||
               strcmp (argv[i], "-k") == 0)
        {
          subtest_mode_fatal = FALSE;
          argv[i] = NULL;
        }
      else if (strcmp ("-p", argv[i]) == 0 || strncmp ("-p=", argv[i], 3) == 0)
        {
          gchar *equal = argv[i] + 2;
          if (*equal == '=')
            subtest_paths = g_slist_prepend (subtest_paths, equal + 1);
          else if (i + 1 < argc)
            {
              argv[i++] = NULL;
              subtest_paths = g_slist_prepend (subtest_paths, argv[i]);
            }
          argv[i] = NULL;
        }
      else if (strcmp ("-o", argv[i]) == 0 || strncmp ("-o=", argv[i], 3) == 0)
        {
          gchar *equal = argv[i] + 2;
          if (*equal == '=')
            outpu_filename = equal + 1;
          else if (i + 1 < argc)
            {
              argv[i++] = NULL;
              outpu_filename = argv[i];
            }
          argv[i] = NULL;
        }
      else if (strcmp ("-m", argv[i]) == 0 || strncmp ("-m=", argv[i], 3) == 0)
        {
          gchar *equal = argv[i] + 2;
          const gchar *mode = "";
          if (*equal == '=')
            mode = equal + 1;
          else if (i + 1 < argc)
            {
              argv[i++] = NULL;
              mode = argv[i];
            }
          if (strcmp (mode, "perf") == 0)
            subtest_mode_perf = TRUE;
          else if (strcmp (mode, "slow") == 0)
            subtest_mode_quick = FALSE;
          else if (strcmp (mode, "quick") == 0)
            {
              subtest_mode_quick = TRUE;
              subtest_mode_perf = FALSE;
            }
          else
            g_error ("unknown test mode: -m %s", mode);
          argv[i] = NULL;
        }
      else if (strcmp ("-q", argv[i]) == 0 || strcmp ("--quiet", argv[i]) == 0)
        {
          gtester_quiet = TRUE;
          gtester_verbose = FALSE;
          argv[i] = NULL;
        }
      else if (strcmp ("--verbose", argv[i]) == 0)
        {
          gtester_quiet = FALSE;
          gtester_verbose = TRUE;
          argv[i] = NULL;
        }
      else if (strcmp ("-l", argv[i]) == 0)
        {
          gtester_list_tests = TRUE;
          argv[i] = NULL;
        }
      else if (strcmp ("--seed", argv[i]) == 0 || strncmp ("--seed=", argv[i], 7) == 0)
        {
          gchar *equal = argv[i] + 6;
          if (*equal == '=')
            subtest_seedstr = equal + 1;
          else if (i + 1 < argc)
            {
              argv[i++] = NULL;
              subtest_seedstr = argv[i];
            }
          argv[i] = NULL;
        }
    }
  /* collapse argv */
  e = 1;
  for (i = 1; i < argc; i++)
    if (argv[i])
      {
        argv[e++] = argv[i];
        if (i >= e)
          argv[i] = NULL;
      }
  *argc_p = e;
}

int
main (int    argc,
      char **argv)
{
  guint ui;

  g_set_prgname (argv[0]);
  parse_args (&argc, &argv);

  if (argc <= 1)
    {
      usage (FALSE);
      return 1;
    }

  for (ui = 1; ui < argc; ui++)
    {
      const char *binary = argv[ui];
      launch_test (binary);
    }

  /* we only get here on success or if !subtest_mode_fatal */
  return 0;
}
