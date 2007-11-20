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
#include <string.h>
#include <stdlib.h>

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

/* --- variables --- */
static gboolean    test_mode_quick = TRUE;
static gboolean    test_mode_perf = FALSE;
static gboolean    test_mode_fatal = TRUE;
static gboolean    g_test_initialized = FALSE;
static gboolean    g_test_run_once = TRUE;
static gboolean    test_run_quiet = FALSE;
static gboolean    test_run_list = FALSE;
static gchar      *test_run_output = NULL;
static gchar      *test_run_seed = NULL;
static gchar      *test_run_name = "";
static GSList     *test_paths = NULL;
static GTestSuite *test_suite_root = NULL;
static GSList     *test_run_free_queue = NULL;

/* --- functions --- */
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
          argv[i] = NULL;
        }
      else if (strcmp ("-l", argv[i]) == 0)
        {
          test_run_list = TRUE;
          argv[i] = NULL;
        }
      else if (strcmp ("-seed", argv[i]) == 0 || strncmp ("-seed=", argv[i], 6) == 0)
        {
          gchar *equal = argv[i] + 5;
          if (*equal == '=')
            test_run_seed = equal + 1;
          else if (i + 1 < argc)
            {
              argv[i++] = NULL;
              test_run_seed = argv[i];
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

  parse_args (argc, argv);


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
  gchar *old_name = test_run_name;
  test_run_name = g_strconcat (old_name, "/", tc->name, NULL);
  void *fixture = g_malloc0 (tc->fixture_size);
  if (tc->fixture_setup)
    tc->fixture_setup (fixture);
  tc->fixture_test (fixture);
  if (tc->fixture_teardown)
    tc->fixture_teardown (fixture);
  while (test_run_free_queue)
    {
      gpointer freeme = test_run_free_queue->data;
      test_run_free_queue = g_slist_delete_link (test_run_free_queue, test_run_free_queue);
      g_free (freeme);
    }
  g_free (fixture);
  g_free (test_run_name);
  test_run_name = old_name;
  return 0;
}

static int
g_test_run_suite_internal (GTestSuite *suite)
{
  guint n_bad = 0, n_good = 0, bad_suite = 0;
  gchar *old_name = test_run_name;
  GSList *slist, *reversed;
  g_return_val_if_fail (suite != NULL, -1);
  test_run_name = suite->name[0] == 0 ? g_strdup (test_run_name) : g_strconcat (old_name, "/", suite->name, NULL);
  reversed = g_slist_reverse (g_slist_copy (suite->cases));
  for (slist = reversed; slist; slist = slist->next)
    {
      GTestCase *tc = slist->data;
      n_good++;
      n_bad += test_case_run (tc) != 0;
    }
  g_slist_free (reversed);
  reversed = g_slist_reverse (g_slist_copy (suite->suites));
  for (slist = reversed; slist; slist = slist->next)
    {
      GTestSuite *ts = slist->data;
      bad_suite += g_test_run_suite_internal (ts) != 0;
    }
  g_slist_free (reversed);
  g_free (test_run_name);
  test_run_name = old_name;
  return n_bad || bad_suite;
}

int
g_test_run_suite (GTestSuite *suite)
{
  g_return_val_if_fail (g_test_initialized == TRUE, -1);
  g_return_val_if_fail (g_test_run_once == TRUE, -1);
  g_test_run_once = FALSE;
  return g_test_run_suite_internal (suite);
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
