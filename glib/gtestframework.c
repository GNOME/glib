/* GLib testing framework examples
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
#include "config.h"
#include "gtestframework.h"
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif /* HAVE_SYS_SELECT_H */

/* --- structures --- */
struct GTestCase
{
  gchar  *name;
  guint   fixture_size;
  void (*fixture_setup) (void*);
  void (*fixture_test) (void*);
  void (*fixture_teardown) (void*);
};
struct GTestSuite
{
  gchar  *name;
  GSList *suites;
  GSList *cases;
};

/* --- prototypes --- */
static void                     test_run_seed           (const gchar *rseed);
static void                     test_trap_clear         (void);
static guint8*                  g_test_log_dump         (GTestLogMsg *msg,
                                                         guint       *len);

/* --- variables --- */
static int         test_stdmsg = 1;
static gboolean    test_mode_quick = TRUE;
static gboolean    test_mode_perf = FALSE;
static gboolean    test_mode_fatal = TRUE;
static gboolean    g_test_initialized = FALSE;
static gboolean    g_test_run_once = TRUE;
static gboolean    test_run_quiet = FALSE;
static gboolean    test_run_verbose = FALSE;
static gboolean    test_run_list = FALSE;
static gchar      *test_run_output = NULL;
static gchar      *test_run_seedstr = NULL;
static GRand      *test_run_rand = NULL;
static gchar      *test_run_name = "";
static guint       test_run_forks = 0;
static GTimer     *test_user_timer = NULL;
static double      test_user_stamp = 0;
static GSList     *test_paths = NULL;
static GTestSuite *test_suite_root = NULL;
static GSList     *test_run_free_queue = NULL;
static int         test_trap_last_status = 0;
static int         test_trap_last_pid = 0;
static char       *test_trap_last_stdout = NULL;
static char       *test_trap_last_stderr = NULL;
static gboolean    test_debug_log = FALSE;

/* --- functions --- */
static const char*
test_log_bit (GTestLogType lbit)
{
  switch (lbit)
    {
    case G_TEST_LOG_START_BINARY:       return "binary";
    case G_TEST_LOG_LIST_CASE:          return "list";
    case G_TEST_LOG_START_CASE:         return "start";
    case G_TEST_LOG_STOP_CASE:          return "stop";
    case G_TEST_LOG_MIN_RESULT:         return "minperf";
    case G_TEST_LOG_MAX_RESULT:         return "maxperf";
    default:                            return "???";
    }
}

static void
g_test_log_send (guint         n_bytes,
                 const guint8 *buffer)
{
  if (test_debug_log)
    {
      GTestLogBuffer *lbuffer = g_test_log_buffer_new();
      GTestLogMsg *msg;
      guint ui;
      g_test_log_buffer_push (lbuffer, n_bytes, buffer);
      msg = g_test_log_buffer_pop (lbuffer);
      g_assert (msg != NULL); // FIXME: should be g_awrn_if_fail
      g_assert (lbuffer->data->len == 0); // FIXME: should be g_awrn_if_fail
      g_test_log_buffer_free (lbuffer);
      /* print message */
      g_printerr ("{*LOG(%s)", test_log_bit (msg->log_type));
      for (ui = 0; ui < msg->n_strings; ui++)
        g_printerr (":{%s}", msg->strings[ui]);
      if (msg->n_nums)
        {
          g_printerr (":(");
          for (ui = 0; ui < msg->n_nums; ui++)
            g_printerr ("%s%.16Lg", ui ? ";" : "", msg->nums[ui]);
          g_printerr (")");
        }
      g_printerr (":LOG*}\n");
      g_test_log_msg_free (msg);
    }
}

static void
g_test_log (GTestLogType lbit,
            const gchar *string1,
            const gchar *string2,
            guint        n_args,
            long double *largs)
{
  gboolean fail = lbit == G_TEST_LOG_STOP_CASE && largs[0] != 0;
  GTestLogMsg msg;
  gchar *astrings[3] = { NULL, NULL, NULL };
  guint8 *dbuffer;
  guint32 dbufferlen;

  switch (lbit)
    {
    case G_TEST_LOG_STOP_CASE:
      if (!test_run_quiet)
        g_print ("%s\n", fail ? "FAIL" : "OK");
      if (fail && test_mode_fatal)
        abort();
      break;
    case G_TEST_LOG_MIN_RESULT:
      if (test_run_verbose)
        g_print ("(MINPERF:%s)\n", string1);
      break;
    case G_TEST_LOG_MAX_RESULT:
      if (test_run_verbose)
        g_print ("(MAXPERF:%s)\n", string1);
      break;
    default: ;
    }

  msg.log_type = lbit;
  msg.n_strings = (string1 != NULL) + (string1 && string2);
  msg.strings = astrings;
  astrings[0] = (gchar*) string1;
  astrings[1] = astrings[0] ? (gchar*) string2 : NULL;
  msg.n_nums = n_args;
  msg.nums = largs;
  dbuffer = g_test_log_dump (&msg, &dbufferlen);
  g_test_log_send (dbufferlen, dbuffer);
  g_free (dbuffer);

  switch (lbit)
    {
    case G_TEST_LOG_START_CASE:
      if (!test_run_quiet)
        g_print ("%s: ", string1);
      break;
    default: ;
    }
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
      else if (strcmp (argv[i], "--keep-going") == 0 ||
               strcmp (argv[i], "-k") == 0)
        {
          test_mode_fatal = FALSE;
          argv[i] = NULL;
        }
      else if (strcmp (argv[i], "--debug-log") == 0)
        {
          test_debug_log = TRUE;
          argv[i] = NULL;
        }
      else if (strcmp ("-p", argv[i]) == 0 || strncmp ("-p=", argv[i], 3) == 0)
        {
          gchar *equal = argv[i] + 2;
          if (*equal == '=')
            test_paths = g_slist_prepend (test_paths, equal + 1);
          else if (i + 1 < argc)
            {
              argv[i++] = NULL;
              test_paths = g_slist_prepend (test_paths, argv[i]);
            }
          argv[i] = NULL;
        }
      else if (strcmp ("-o", argv[i]) == 0 || strncmp ("-o=", argv[i], 3) == 0)
        {
          gchar *equal = argv[i] + 2;
          if (*equal == '=')
            test_run_output = equal + 1;
          else if (i + 1 < argc)
            {
              argv[i++] = NULL;
              test_run_output = argv[i];
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
            test_mode_perf = TRUE;
          else if (strcmp (mode, "slow") == 0)
            test_mode_quick = FALSE;
          else if (strcmp (mode, "quick") == 0)
            {
              test_mode_quick = TRUE;
              test_mode_perf = FALSE;
            }
          else
            g_error ("unknown test mode: -m %s", mode);
          argv[i] = NULL;
        }
      else if (strcmp ("-q", argv[i]) == 0 || strcmp ("--quiet", argv[i]) == 0)
        {
          test_run_quiet = TRUE;
          test_run_verbose = FALSE;
          argv[i] = NULL;
        }
      else if (strcmp ("--verbose", argv[i]) == 0)
        {
          test_run_quiet = FALSE;
          test_run_verbose = TRUE;
          argv[i] = NULL;
        }
      else if (strcmp ("-l", argv[i]) == 0)
        {
          test_run_list = TRUE;
          argv[i] = NULL;
        }
      else if (strcmp ("--seed", argv[i]) == 0 || strncmp ("--seed=", argv[i], 7) == 0)
        {
          gchar *equal = argv[i] + 6;
          if (*equal == '=')
            test_run_seedstr = equal + 1;
          else if (i + 1 < argc)
            {
              argv[i++] = NULL;
              test_run_seedstr = argv[i];
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

void
g_test_init (int            *argc,
             char         ***argv,
             ...)
{
  static char seedstr[4 + 4 * 8 + 1];
  va_list args;
  gpointer vararg1;
  g_return_if_fail (argc != NULL);
  g_return_if_fail (argv != NULL);
  g_return_if_fail (g_test_initialized == FALSE);
  g_test_initialized = TRUE;

  va_start (args, argv);
  vararg1 = va_arg (args, gpointer); /* reserved for future extensions */
  va_end (args);
  g_return_if_fail (vararg1 == NULL);

  /* setup random seed string */
  g_snprintf (seedstr, sizeof (seedstr), "R02S%08x%08x%08x%08x", g_random_int(), g_random_int(), g_random_int(), g_random_int());
  test_run_seedstr = seedstr;

  /* parse args, sets up mode, changes seed, etc. */
  parse_args (argc, argv);
  if (!g_get_prgname())
    g_set_prgname ((*argv)[0]);

  /* verify GRand reliability, needed for reliable seeds */
  if (1)
    {
      GRand *rg = g_rand_new_with_seed (0xc8c49fb6);
      guint32 t1 = g_rand_int (rg), t2 = g_rand_int (rg), t3 = g_rand_int (rg), t4 = g_rand_int (rg);
      // g_print ("GRand-current: 0x%x 0x%x 0x%x 0x%x\n", t1, t2, t3, t4);
      if (t1 != 0xfab39f9b || t2 != 0xb948fb0e || t3 != 0x3d31be26 || t4 != 0x43a19d66)
        g_warning ("random numbers are not GRand-2.2 compatible, seeds may be broken (check $G_RANDOM_VERSION)");
      g_rand_free (rg);
    }

  /* check rand seed */
  test_run_seed (test_run_seedstr);

  /* report program start */
  g_test_log (G_TEST_LOG_START_BINARY, g_get_prgname(), test_run_seedstr, 0, NULL);
}

static void
test_run_seed (const gchar *rseed)
{
  guint seed_failed = 0;
  if (test_run_rand)
    g_rand_free (test_run_rand);
  test_run_rand = NULL;
  while (strchr (" \t\v\r\n\f", *rseed))
    rseed++;
  if (strncmp (rseed, "R02S", 4) == 0)  // seed for random generator 02 (GRand-2.2)
    {
      const char *s = rseed + 4;
      if (strlen (s) >= 32)             // require 4 * 8 chars
        {
          guint32 seedarray[4];
          gchar *p, hexbuf[9] = { 0, };
          memcpy (hexbuf, s + 0, 8);
          seedarray[0] = g_ascii_strtoull (hexbuf, &p, 16);
          seed_failed += p != NULL && *p != 0;
          memcpy (hexbuf, s + 8, 8);
          seedarray[1] = g_ascii_strtoull (hexbuf, &p, 16);
          seed_failed += p != NULL && *p != 0;
          memcpy (hexbuf, s + 16, 8);
          seedarray[2] = g_ascii_strtoull (hexbuf, &p, 16);
          seed_failed += p != NULL && *p != 0;
          memcpy (hexbuf, s + 24, 8);
          seedarray[3] = g_ascii_strtoull (hexbuf, &p, 16);
          seed_failed += p != NULL && *p != 0;
          if (!seed_failed)
            {
              test_run_rand = g_rand_new_with_seed_array (seedarray, 4);
              return;
            }
        }
    }
  g_error ("Unknown or invalid random seed: %s", rseed);
}

gint32
g_test_rand_int (void)
{
  return g_rand_int (test_run_rand);
}

gint32
g_test_rand_int_range (gint32          begin,
                       gint32          end)
{
  return g_rand_int_range (test_run_rand, begin, end);
}

double
g_test_rand_double (void)
{
  return g_rand_double (test_run_rand);
}

double
g_test_rand_double_range (double          range_start,
                          double          range_end)
{
  return g_rand_double_range (test_run_rand, range_start, range_end);
}

void
g_test_timer_start (void)
{
  if (!test_user_timer)
    test_user_timer = g_timer_new();
  test_user_stamp = 0;
  g_timer_start (test_user_timer);
}

double
g_test_timer_elapsed (void)
{
  test_user_stamp = test_user_timer ? g_timer_elapsed (test_user_timer, NULL) : 0;
  return test_user_stamp;
}

double
g_test_timer_last (void)
{
  return test_user_stamp;
}

void
g_test_minimized_result (double          minimized_quantity,
                         const char     *format,
                         ...)
{
  long double largs = minimized_quantity;
  gchar *buffer;
  va_list args;
  va_start (args, format);
  buffer = g_strdup_vprintf (format, args);
  va_end (args);
  g_test_log (G_TEST_LOG_MIN_RESULT, buffer, NULL, 1, &largs);
  g_free (buffer);
}

void
g_test_maximized_result (double          maximized_quantity,
                         const char     *format,
                         ...)
{
  long double largs = maximized_quantity;
  gchar *buffer;
  va_list args;
  va_start (args, format);
  buffer = g_strdup_vprintf (format, args);
  va_end (args);
  g_test_log (G_TEST_LOG_MAX_RESULT, buffer, NULL, 1, &largs);
  g_free (buffer);
}

GTestSuite*
g_test_get_root (void)
{
  if (!test_suite_root)
    {
      test_suite_root = g_test_create_suite ("root");
      g_free (test_suite_root->name);
      test_suite_root->name = g_strdup ("");
    }
  return test_suite_root;
}

int
g_test_run (void)
{
  return g_test_run_suite (g_test_get_root());
}

GTestCase*
g_test_create_case (const char     *test_name,
                    gsize           data_size,
                    void          (*data_setup) (void),
                    void          (*data_test) (void),
                    void          (*data_teardown) (void))
{
  g_return_val_if_fail (test_name != NULL, NULL);
  g_return_val_if_fail (strchr (test_name, '/') == NULL, NULL);
  g_return_val_if_fail (test_name[0] != 0, NULL);
  g_return_val_if_fail (data_test != NULL, NULL);
  GTestCase *tc = g_slice_new0 (GTestCase);
  tc->name = g_strdup (test_name);
  tc->fixture_size = data_size;
  tc->fixture_setup = (void*) data_setup;
  tc->fixture_test = (void*) data_test;
  tc->fixture_teardown = (void*) data_teardown;
  return tc;
}

void
g_test_add_vtable (const char     *testpath,
                   gsize           data_size,
                   void          (*data_setup)    (void),
                   void          (*fixture_test_func) (void),
                   void          (*data_teardown) (void))
{
  gchar **segments;
  guint ui;
  GTestSuite *suite;

  g_return_if_fail (testpath != NULL);
  g_return_if_fail (testpath[0] == '/');
  g_return_if_fail (fixture_test_func != NULL);

  suite = g_test_get_root();
  segments = g_strsplit (testpath, "/", -1);
  for (ui = 0; segments[ui] != NULL; ui++)
    {
      const char *seg = segments[ui];
      gboolean islast = segments[ui + 1] == NULL;
      if (islast && !seg[0])
        g_error ("invalid test case path: %s", testpath);
      else if (!seg[0])
        continue;       // initial or duplicate slash
      else if (!islast)
        {
          GTestSuite *csuite = g_test_create_suite (seg);
          g_test_suite_add_suite (suite, csuite);
          suite = csuite;
        }
      else /* islast */
        {
          GTestCase *tc = g_test_create_case (seg, data_size, data_setup, fixture_test_func, data_teardown);
          g_test_suite_add (suite, tc);
        }
    }
  g_strfreev (segments);
}

void
g_test_add_func (const char     *testpath,
                 void          (*test_func) (void))
{
  g_return_if_fail (testpath != NULL);
  g_return_if_fail (testpath[0] == '/');
  g_return_if_fail (test_func != NULL);
  g_test_add_vtable (testpath, 0, NULL, test_func, NULL);
}

GTestSuite*
g_test_create_suite (const char *suite_name)
{
  g_return_val_if_fail (suite_name != NULL, NULL);
  g_return_val_if_fail (strchr (suite_name, '/') == NULL, NULL);
  g_return_val_if_fail (suite_name[0] != 0, NULL);
  GTestSuite *ts = g_slice_new0 (GTestSuite);
  ts->name = g_strdup (suite_name);
  return ts;
}

void
g_test_suite_add (GTestSuite     *suite,
                  GTestCase      *test_case)
{
  g_return_if_fail (suite != NULL);
  g_return_if_fail (test_case != NULL);
  suite->cases = g_slist_prepend (suite->cases, test_case);
}

void
g_test_suite_add_suite (GTestSuite     *suite,
                        GTestSuite     *nestedsuite)
{
  g_return_if_fail (suite != NULL);
  g_return_if_fail (nestedsuite != NULL);
  suite->suites = g_slist_prepend (suite->suites, nestedsuite);
}

void
g_test_queue_free (gpointer gfree_pointer)
{
  if (gfree_pointer)
    test_run_free_queue = g_slist_prepend (test_run_free_queue, gfree_pointer);
}

static int
test_case_run (GTestCase *tc)
{
  gchar *old_name;
  old_name = test_run_name;
  test_run_name = g_strconcat (old_name, "/", tc->name, NULL);
  if (test_run_list)
    {
      g_print ("%s\n", test_run_name);
      g_test_log (G_TEST_LOG_LIST_CASE, test_run_name, NULL, 0, NULL);
    }
  else
    {
      GTimer *test_run_timer = g_timer_new();
      long double largs[3];
      g_test_log (G_TEST_LOG_START_CASE, test_run_name, NULL, 0, NULL);
      test_run_forks = 0;
      g_timer_start (test_run_timer);
      void *fixture = g_malloc0 (tc->fixture_size);
      test_run_seed (test_run_seedstr);
      if (tc->fixture_setup)
        tc->fixture_setup (fixture);
      tc->fixture_test (fixture);
      test_trap_clear();
      while (test_run_free_queue)
        {
          gpointer freeme = test_run_free_queue->data;
          test_run_free_queue = g_slist_delete_link (test_run_free_queue, test_run_free_queue);
          g_free (freeme);
        }
      if (tc->fixture_teardown)
        tc->fixture_teardown (fixture);
      g_free (fixture);
      g_timer_stop (test_run_timer);
      largs[0] = 0; // OK
      largs[1] = test_run_forks;
      largs[2] = g_timer_elapsed (test_run_timer, NULL);
      g_test_log (G_TEST_LOG_STOP_CASE, NULL, NULL, G_N_ELEMENTS (largs), largs);
    }
  g_free (test_run_name);
  test_run_name = old_name;
  /* FIXME: need reporting here */
  return 0;
}

static int
g_test_run_suite_internal (GTestSuite *suite,
                           const char *path)
{
  guint n_bad = 0, n_good = 0, bad_suite = 0, l;
  gchar *rest, *old_name = test_run_name;
  GSList *slist, *reversed;
  g_return_val_if_fail (suite != NULL, -1);
  while (path[0] == '/')
    path++;
  l = strlen (path);
  rest = strchr (path, '/');
  l = rest ? MIN (l, rest - path) : l;
  test_run_name = suite->name[0] == 0 ? g_strdup (test_run_name) : g_strconcat (old_name, "/", suite->name, NULL);
  reversed = g_slist_reverse (g_slist_copy (suite->cases));
  for (slist = reversed; slist; slist = slist->next)
    {
      GTestCase *tc = slist->data;
      guint n = l ? strlen (tc->name) : 0;
      if (l == n && strncmp (path, tc->name, n) == 0)
        {
          n_good++;
          n_bad += test_case_run (tc) != 0;
        }
    }
  g_slist_free (reversed);
  reversed = g_slist_reverse (g_slist_copy (suite->suites));
  for (slist = reversed; slist; slist = slist->next)
    {
      GTestSuite *ts = slist->data;
      guint n = l ? strlen (ts->name) : 0;
      if (l == n && strncmp (path, ts->name, n) == 0)
        bad_suite += g_test_run_suite_internal (ts, rest ? rest : "") != 0;
    }
  g_slist_free (reversed);
  g_free (test_run_name);
  test_run_name = old_name;
  return n_bad || bad_suite;
}

int
g_test_run_suite (GTestSuite *suite)
{
  guint n_bad = 0;
  g_return_val_if_fail (g_test_initialized == TRUE, -1);
  g_return_val_if_fail (g_test_run_once == TRUE, -1);
  g_test_run_once = FALSE;
  if (!test_paths)
    test_paths = g_slist_prepend (test_paths, "");
  while (test_paths)
    {
      const char *rest, *path = test_paths->data;
      guint l, n = strlen (suite->name);
      test_paths = g_slist_delete_link (test_paths, test_paths);
      while (path[0] == '/')
        path++;
      if (!n) /* root suite, run unconditionally */
        {
          n_bad += 0 != g_test_run_suite_internal (suite, path);
          continue;
        }
      /* regular suite, match path */
      rest = strchr (path, '/');
      l = strlen (path);
      l = rest ? MIN (l, rest - path) : l;
      if (!l || l == n && strncmp (path, suite->name, n) == 0)
        n_bad += 0 != g_test_run_suite_internal (suite, rest ? rest : "");
    }
  return n_bad;
}

void
g_assertion_message (const char     *domain,
                     const char     *file,
                     int             line,
                     const char     *func,
                     const char     *message)
{
  char lstr[32];
  g_snprintf (lstr, 32, "%d", line);
  char *s = g_strconcat (domain ? domain : "", domain && domain[0] ? ":" : "",
                         file, ":", lstr, ":",
                         func, func[0] ? ":" : "",
                         " ", message, NULL);
  g_printerr ("**\n** %s\n", s);
  g_free (s);
  abort();
}

void
g_assertion_message_expr (const char     *domain,
                          const char     *file,
                          int             line,
                          const char     *func,
                          const char     *expr)
{
  char *s = g_strconcat ("assertion failed: (", expr, ")", NULL);
  g_assertion_message (domain, file, line, func, s);
  g_free (s);
}

void
g_assertion_message_cmpnum (const char     *domain,
                            const char     *file,
                            int             line,
                            const char     *func,
                            const char     *expr,
                            long double     arg1,
                            const char     *cmp,
                            long double     arg2,
                            char            numtype)
{
  char *s = NULL;
  switch (numtype)
    {
    case 'i':   s = g_strdup_printf ("assertion failed (%s): (%.0Lf %s %.0Lf)", expr, arg1, cmp, arg2); break;
    case 'x':   s = g_strdup_printf ("assertion failed (%s): (0x%08Lx %s 0x%08Lx)", expr, (guint64) arg1, cmp, (guint64) arg2); break;
    case 'f':   s = g_strdup_printf ("assertion failed (%s): (%.9Lg %s %.9Lg)", expr, arg1, cmp, arg2); break;
      /* ideally use: floats=%.7g double=%.17g */
    }
  g_assertion_message (domain, file, line, func, s);
  g_free (s);
}

void
g_assertion_message_cmpstr (const char     *domain,
                            const char     *file,
                            int             line,
                            const char     *func,
                            const char     *expr,
                            const char     *arg1,
                            const char     *cmp,
                            const char     *arg2)
{
  char *a1, *a2, *s, *t1 = NULL, *t2 = NULL;
  a1 = arg1 ? g_strconcat ("\"", t1 = g_strescape (arg1, NULL), "\"", NULL) : g_strdup ("NULL");
  a2 = arg2 ? g_strconcat ("\"", t2 = g_strescape (arg2, NULL), "\"", NULL) : g_strdup ("NULL");
  g_free (t1);
  g_free (t2);
  s = g_strdup_printf ("assertion failed (%s): (%s %s %s)", expr, a1, cmp, a2);
  g_free (a1);
  g_free (a2);
  g_assertion_message (domain, file, line, func, s);
  g_free (s);
}

int
g_strcmp0 (const char     *str1,
           const char     *str2)
{
  if (!str1)
    return -(str1 != str2);
  if (!str2)
    return str1 != str2;
  return strcmp (str1, str2);
}

static int /* 0 on success */
kill_child (int  pid,
            int *status,
            int  patience)
{
  int wr;
  if (patience >= 3)    /* try graceful reap */
    {
      if (waitpid (pid, status, WNOHANG) > 0)
        return 0;
    }
  if (patience >= 2)    /* try SIGHUP */
    {
      kill (pid, SIGHUP);
      if (waitpid (pid, status, WNOHANG) > 0)
        return 0;
      g_usleep (20 * 1000); /* give it some scheduling/shutdown time */
      if (waitpid (pid, status, WNOHANG) > 0)
        return 0;
      g_usleep (50 * 1000); /* give it some scheduling/shutdown time */
      if (waitpid (pid, status, WNOHANG) > 0)
        return 0;
      g_usleep (100 * 1000); /* give it some scheduling/shutdown time */
      if (waitpid (pid, status, WNOHANG) > 0)
        return 0;
    }
  if (patience >= 1)    /* try SIGTERM */
    {
      kill (pid, SIGTERM);
      if (waitpid (pid, status, WNOHANG) > 0)
        return 0;
      g_usleep (200 * 1000); /* give it some scheduling/shutdown time */
      if (waitpid (pid, status, WNOHANG) > 0)
        return 0;
      g_usleep (400 * 1000); /* give it some scheduling/shutdown time */
      if (waitpid (pid, status, WNOHANG) > 0)
        return 0;
    }
  /* finish it off */
  kill (pid, SIGKILL);
  do
    wr = waitpid (pid, status, 0);
  while (wr < 0 && errno == EINTR);
  return wr;
}

static inline int
g_string_must_read (GString *gstring,
                    int      fd)
{
#define STRING_BUFFER_SIZE     4096
  char buf[STRING_BUFFER_SIZE];
  gssize bytes;
 again:
  bytes = read (fd, buf, sizeof (buf));
  if (bytes == 0)
    return 0; /* EOF, calling this function assumes data is available */
  else if (bytes > 0)
    {
      g_string_append_len (gstring, buf, bytes);
      return 1;
    }
  else if (bytes < 0 && errno == EINTR)
    goto again;
  else /* bytes < 0 */
    {
      g_warning ("failed to read() from child process (%d): %s", test_trap_last_pid, g_strerror (errno));
      return 1; /* ignore error after warning */
    }
}

static inline void
g_string_write_out (GString *gstring,
                    int      outfd,
                    int     *stringpos)
{
  if (*stringpos < gstring->len)
    {
      int r;
      do
        r = write (outfd, gstring->str + *stringpos, gstring->len - *stringpos);
      while (r < 0 && errno == EINTR);
      *stringpos += MAX (r, 0);
    }
}

static int
sane_dup2 (int fd1,
           int fd2)
{
  int ret;
  do
    ret = dup2 (fd1, fd2);
  while (ret < 0 && errno == EINTR);
  return ret;
}

static void
test_trap_clear (void)
{
  test_trap_last_status = 0;
  test_trap_last_pid = 0;
  g_free (test_trap_last_stdout);
  test_trap_last_stdout = NULL;
  g_free (test_trap_last_stderr);
  test_trap_last_stderr = NULL;
}

static guint64
test_time_stamp (void)
{
  GTimeVal tv;
  guint64 stamp;
  g_get_current_time (&tv);
  stamp = tv.tv_sec;
  stamp = stamp * 1000000 + tv.tv_usec;
  return stamp;
}

gboolean
g_test_trap_fork (guint64        usec_timeout,
                  GTestTrapFlags test_trap_flags)
{
  int stdout_pipe[2] = { -1, -1 };
  int stderr_pipe[2] = { -1, -1 };
  int stdtst_pipe[2] = { -1, -1 };
  test_trap_clear();
  if (pipe (stdout_pipe) < 0 || pipe (stderr_pipe) < 0 || pipe (stdtst_pipe) < 0)
    g_error ("failed to create pipes to fork test program: %s", g_strerror (errno));
  signal (SIGCHLD, SIG_DFL);
  test_trap_last_pid = fork ();
  if (test_trap_last_pid < 0)
    g_error ("failed to fork test program: %s", g_strerror (errno));
  if (test_trap_last_pid == 0)  /* child */
    {
      int fd0 = -1;
      close (stdout_pipe[0]);
      close (stderr_pipe[0]);
      close (stdtst_pipe[0]);
      if (!(test_trap_flags & G_TEST_TRAP_INHERIT_STDIN))
        fd0 = open ("/dev/null", O_RDONLY);
      if (sane_dup2 (stdout_pipe[1], 1) < 0 || sane_dup2 (stderr_pipe[1], 2) < 0 || (fd0 >= 0 && sane_dup2 (fd0, 0) < 0))
        g_error ("failed to dup2() in forked test program: %s", g_strerror (errno));
      if (fd0 >= 3)
        close (fd0);
      if (stdout_pipe[1] >= 3)
        close (stdout_pipe[1]);
      if (stderr_pipe[1] >= 3)
        close (stderr_pipe[1]);
      test_stdmsg = stdtst_pipe[1];
      return TRUE;
    }
  else                          /* parent */
    {
      GString *sout = g_string_new (NULL);
      GString *serr = g_string_new (NULL);
      GString *stst = g_string_new (NULL);
      guint64 sstamp;
      int soutpos = 0, serrpos = 0, ststpos = 0, wr, need_wait = TRUE;
      test_run_forks++;
      close (stdout_pipe[1]);
      close (stderr_pipe[1]);
      close (stdtst_pipe[1]);
      sstamp = test_time_stamp();
      /* read data until we get EOF on all pipes */
      while (stdout_pipe[0] >= 0 || stderr_pipe[0] >= 0 || stdtst_pipe[0] > 0)
        {
          fd_set fds;
          struct timeval tv;
          FD_ZERO (&fds);
          if (stdout_pipe[0] >= 0)
            FD_SET (stdout_pipe[0], &fds);
          if (stderr_pipe[0] >= 0)
            FD_SET (stderr_pipe[0], &fds);
          if (stdtst_pipe[0] >= 0)
            FD_SET (stdtst_pipe[0], &fds);
          tv.tv_sec = 0;
          tv.tv_usec = MIN (usec_timeout ? usec_timeout : 1000000, 100 * 1000); // sleep at most 0.5 seconds to catch clock skews, etc.
          int ret = select (MAX (MAX (stdout_pipe[0], stderr_pipe[0]), stdtst_pipe[0]) + 1, &fds, NULL, NULL, &tv);
          if (ret < 0 && errno != EINTR)
            {
              g_warning ("Unexpected error in select() while reading from child process (%d): %s", test_trap_last_pid, g_strerror (errno));
              break;
            }
          if (stdout_pipe[0] >= 0 && FD_ISSET (stdout_pipe[0], &fds) &&
              g_string_must_read (sout, stdout_pipe[0]) == 0)
            {
              close (stdout_pipe[0]);
              stdout_pipe[0] = -1;
            }
          if (stderr_pipe[0] >= 0 && FD_ISSET (stderr_pipe[0], &fds) &&
              g_string_must_read (serr, stderr_pipe[0]) == 0)
            {
              close (stderr_pipe[0]);
              stderr_pipe[0] = -1;
            }
          if (stdtst_pipe[0] >= 0 && FD_ISSET (stdtst_pipe[0], &fds) &&
              g_string_must_read (stst, stdtst_pipe[0]) == 0)
            {
              close (stdtst_pipe[0]);
              stdtst_pipe[0] = -1;
            }
          if (!(test_trap_flags & G_TEST_TRAP_SILENCE_STDOUT))
            g_string_write_out (sout, 1, &soutpos);
          if (!(test_trap_flags & G_TEST_TRAP_SILENCE_STDERR))
            g_string_write_out (serr, 2, &serrpos);
          if (TRUE) // FIXME: needs capturing into log file
            g_string_write_out (stst, 1, &ststpos);
          if (usec_timeout)
            {
              guint64 nstamp = test_time_stamp();
              int status = 0;
              sstamp = MIN (sstamp, nstamp); // guard against backwards clock skews
              if (usec_timeout < nstamp - sstamp)
                {
                  /* timeout reached, need to abort the child now */
                  kill_child (test_trap_last_pid, &status, 3);
                  test_trap_last_status = 1024; /* timeout */
                  if (0 && WIFSIGNALED (status))
                    g_printerr ("%s: child timed out and received: %s\n", G_STRFUNC, g_strsignal (WTERMSIG (status)));
                  need_wait = FALSE;
                  break;
                }
            }
        }
      close (stdout_pipe[0]);
      close (stderr_pipe[0]);
      close (stdtst_pipe[0]);
      if (need_wait)
        {
          int status = 0;
          do
            wr = waitpid (test_trap_last_pid, &status, 0);
          while (wr < 0 && errno == EINTR);
          if (WIFEXITED (status)) /* normal exit */
            test_trap_last_status = WEXITSTATUS (status); /* 0..255 */
          else if (WIFSIGNALED (status))
            test_trap_last_status = (WTERMSIG (status) << 12); /* signalled */
          else /* WCOREDUMP (status) */
            test_trap_last_status = 512; /* coredump */
        }
      test_trap_last_stdout = g_string_free (sout, FALSE);
      test_trap_last_stderr = g_string_free (serr, FALSE);
      g_string_free (stst, TRUE);
      return FALSE;
    }
}

gboolean
g_test_trap_has_passed (void)
{
  return test_trap_last_status == 0; /* exit_status == 0 && !signal && !coredump */
}

gboolean
g_test_trap_reached_timeout (void)
{
  return 0 != (test_trap_last_status & 1024); /* timeout flag */
}

void
g_test_trap_assertions (const char     *domain,
                        const char     *file,
                        int             line,
                        const char     *func,
                        gboolean        must_pass,
                        gboolean        must_fail,
                        const char     *stdout_pattern,
                        const char     *stderr_pattern)
{
  if (test_trap_last_pid == 0)
    g_error ("child process failed to exit after g_test_trap_fork() and before g_test_trap_assert*()");
  if (must_pass && !g_test_trap_has_passed())
    {
      char *msg = g_strdup_printf ("child process (%d) of test trap failed unexpectedly", test_trap_last_pid);
      g_assertion_message (domain, file, line, func, msg);
      g_free (msg);
    }
  if (must_fail && g_test_trap_has_passed())
    {
      char *msg = g_strdup_printf ("child process (%d) did not fail as expected", test_trap_last_pid);
      g_assertion_message (domain, file, line, func, msg);
      g_free (msg);
    }
  if (stdout_pattern && !g_pattern_match_simple (stdout_pattern, test_trap_last_stdout))
    {
      char *msg = g_strdup_printf ("stdout of child process (%d) failed to match: %s", test_trap_last_pid, stdout_pattern);
      g_assertion_message (domain, file, line, func, msg);
      g_free (msg);
    }
  if (stderr_pattern && !g_pattern_match_simple (stderr_pattern, test_trap_last_stderr))
    {
      char *msg = g_strdup_printf ("stderr of child process (%d) failed to match: %s", test_trap_last_pid, stderr_pattern);
      g_assertion_message (domain, file, line, func, msg);
      g_free (msg);
    }
}

static void
gstring_overwrite_int (GString *gstring,
                       guint    pos,
                       guint32  vuint)
{
  vuint = g_htonl (vuint);
  g_string_overwrite_len (gstring, pos, (const gchar*) &vuint, 4);
}

static void
gstring_append_int (GString *gstring,
                    guint32  vuint)
{
  vuint = g_htonl (vuint);
  g_string_append_len (gstring, (const gchar*) &vuint, 4);
}

static void
gstring_append_double (GString *gstring,
                       double   vdouble)
{
  union { double vdouble; guint64 vuint64; } u;
  u.vdouble = vdouble;
  u.vuint64 = GUINT64_TO_BE (u.vuint64);
  g_string_append_len (gstring, (const gchar*) &u.vuint64, 8);
}

static guint8*
g_test_log_dump (GTestLogMsg *msg,
                 guint       *len)
{
  GString *gstring = g_string_sized_new (1024);
  guint ui;
  gstring_append_int (gstring, 0);              /* message length */
  gstring_append_int (gstring, msg->log_type);
  gstring_append_int (gstring, msg->n_strings);
  gstring_append_int (gstring, msg->n_nums);
  gstring_append_int (gstring, 0);      /* reserved */
  for (ui = 0; ui < msg->n_strings; ui++)
    {
      guint l = strlen (msg->strings[ui]);
      gstring_append_int (gstring, l);
      g_string_append_len (gstring, msg->strings[ui], l);
    }
  for (ui = 0; ui < msg->n_nums; ui++)
    gstring_append_double (gstring, msg->nums[ui]);
  *len = gstring->len;
  gstring_overwrite_int (gstring, 0, *len);     /* message length */
  return (guint8*) g_string_free (gstring, FALSE);
}

static inline long double
net_double (const gchar **ipointer)
{
  union { guint64 vuint64; double vdouble; } u;
  guint64 aligned_int64;
  memcpy (&aligned_int64, *ipointer, 8);
  *ipointer += 8;
  u.vuint64 = GUINT64_FROM_BE (aligned_int64);
  return u.vdouble;
}

static inline guint32
net_int (const gchar **ipointer)
{
  guint32 aligned_int;
  memcpy (&aligned_int, *ipointer, 4);
  *ipointer += 4;
  return g_ntohl (aligned_int);
}

static gboolean
g_test_log_extract (GTestLogBuffer *tbuffer)
{
  const gchar *p = tbuffer->data->str;
  GTestLogMsg msg;
  guint mlength;
  if (tbuffer->data->len < 4 * 5)
    return FALSE;
  mlength = net_int (&p);
  if (tbuffer->data->len < mlength)
    return FALSE;
  msg.log_type = net_int (&p);
  msg.n_strings = net_int (&p);
  msg.n_nums = net_int (&p);
  if (net_int (&p) == 0)
    {
      guint ui;
      msg.strings = g_new0 (gchar*, msg.n_strings + 1);
      msg.nums = g_new0 (long double, msg.n_nums);
      for (ui = 0; ui < msg.n_strings; ui++)
        {
          guint sl = net_int (&p);
          msg.strings[ui] = g_strndup (p, sl);
          p += sl;
        }
      for (ui = 0; ui < msg.n_nums; ui++)
        msg.nums[ui] = net_double (&p);
      if (p <= tbuffer->data->str + mlength)
        {
          g_string_erase (tbuffer->data, 0, mlength);
          tbuffer->msgs = g_slist_prepend (tbuffer->msgs, g_memdup (&msg, sizeof (msg)));
          return TRUE;
        }
    }
  g_free (msg.nums);
  g_strfreev (msg.strings);
  g_error ("corrupt log stream from test program");
  return FALSE;
}

GTestLogBuffer*
g_test_log_buffer_new (void)
{
  GTestLogBuffer *tb = g_new0 (GTestLogBuffer, 1);
  tb->data = g_string_sized_new (1024);
  return tb;
}

void
g_test_log_buffer_free (GTestLogBuffer *tbuffer)
{
  g_return_if_fail (tbuffer != NULL);
  while (tbuffer->msgs)
    g_test_log_msg_free (g_test_log_buffer_pop (tbuffer));
  g_string_free (tbuffer->data, TRUE);
  g_free (tbuffer);
}

void
g_test_log_buffer_push (GTestLogBuffer *tbuffer,
                        guint           n_bytes,
                        const guint8   *bytes)
{
  g_return_if_fail (tbuffer != NULL);
  if (n_bytes)
    {
      gboolean more_messages;
      g_return_if_fail (bytes != NULL);
      g_string_append_len (tbuffer->data, (const gchar*) bytes, n_bytes);
      do
        more_messages = g_test_log_extract (tbuffer);
      while (more_messages);
    }
}

GTestLogMsg*
g_test_log_buffer_pop (GTestLogBuffer *tbuffer)
{
  GTestLogMsg *msg = NULL;
  g_return_val_if_fail (tbuffer != NULL, NULL);
  if (tbuffer->msgs)
    {
      GSList *slist = g_slist_last (tbuffer->msgs);
      msg = slist->data;
      tbuffer->msgs = g_slist_delete_link (tbuffer->msgs, slist);
    }
  return msg;
}

void
g_test_log_msg_free (GTestLogMsg *tmsg)
{
  g_return_if_fail (tmsg != NULL);
  g_strfreev (tmsg->strings);
  g_free (tmsg->nums);
  g_free (tmsg);
}
