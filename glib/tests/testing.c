/* GLib testing framework examples and tests
 * Copyright (C) 2007 Imendio AB
 * Authors: Tim Janik
 *
 * This work is provided "as is"; redistribution and modification
 * in whole or in part, in any medium, physical or electronic is
 * permitted without restriction.
 *
 * This work is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * In no event shall the authors or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even
 * if advised of the possibility of such damage.
 */

#include <glib.h>

#include <stdlib.h>

/* test assertion variants */
static void
test_assertions_bad_cmpstr (void)
{
  g_assert_cmpstr ("fzz", !=, "fzz");
  exit (0);
}

static void
test_assertions_bad_cmpint (void)
{
  g_assert_cmpint (4, !=, 4);
  exit (0);
}

static void
test_assertions (void)
{
  gchar *fuu;
  g_assert_cmpint (1, >, 0);
  g_assert_cmphex (2, ==, 2);
  g_assert_cmpfloat (3.3, !=, 7);
  g_assert_cmpfloat (7, <=, 3 + 4);
  g_assert (TRUE);
  g_assert_cmpstr ("foo", !=, "faa");
  fuu = g_strdup_printf ("f%s", "uu");
  g_test_queue_free (fuu);
  g_assert_cmpstr ("foo", !=, fuu);
  g_assert_cmpstr ("fuu", ==, fuu);
  g_assert_cmpstr (NULL, <, "");
  g_assert_cmpstr (NULL, ==, NULL);
  g_assert_cmpstr ("", >, NULL);
  g_assert_cmpstr ("foo", <, "fzz");
  g_assert_cmpstr ("fzz", >, "faa");
  g_assert_cmpstr ("fzz", ==, "fzz");

  g_test_trap_subprocess ("/misc/assertions:bad_cmpstr",
                          0, G_TEST_TRAP_SILENCE_STDERR);
  g_test_trap_assert_failed ();
  g_test_trap_assert_stderr ("*assertion failed*");

  g_test_trap_subprocess ("/misc/assertions:bad_cmpint",
                          0, G_TEST_TRAP_SILENCE_STDERR);
  g_test_trap_assert_failed ();
  g_test_trap_assert_stderr ("*assertion failed*");
}

/* test g_test_timer* API */
static void
test_timer (void)
{
  double ttime;
  g_test_timer_start();
  g_assert_cmpfloat (g_test_timer_last(), ==, 0);
  g_usleep (25 * 1000);
  ttime = g_test_timer_elapsed();
  g_assert_cmpfloat (ttime, >, 0);
  g_assert_cmpfloat (g_test_timer_last(), ==, ttime);
  g_test_minimized_result (ttime, "timer-test-time: %fsec", ttime);
  g_test_maximized_result (5, "bogus-quantity: %ddummies", 5); /* simple API test */
}

#ifdef G_OS_UNIX
G_GNUC_BEGIN_IGNORE_DEPRECATIONS

/* fork out for a failing test */
static void
test_fork_fail (void)
{
  if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR))
    {
      g_assert_not_reached();
    }
  g_test_trap_assert_failed();
  g_test_trap_assert_stderr ("*ERROR*test_fork_fail*should not be reached*");
}

/* fork out to assert stdout and stderr patterns */
static void
test_fork_patterns (void)
{
  if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDOUT | G_TEST_TRAP_SILENCE_STDERR))
    {
      g_print ("some stdout text: somagic17\n");
      g_printerr ("some stderr text: semagic43\n");
      exit (0);
    }
  g_test_trap_assert_passed();
  g_test_trap_assert_stdout ("*somagic17*");
  g_test_trap_assert_stderr ("*semagic43*");
}

/* fork out for a timeout test */
static void
test_fork_timeout (void)
{
  /* allow child to run for only a fraction of a second */
  if (g_test_trap_fork (0.11 * 1000000, 0))
    {
      /* loop and sleep forever */
      while (TRUE)
        g_usleep (1000 * 1000);
    }
  g_test_trap_assert_failed();
  g_assert (g_test_trap_reached_timeout());
}

G_GNUC_END_IGNORE_DEPRECATIONS
#endif /* G_OS_UNIX */

static void
test_subprocess_fail_child (void)
{
  g_assert_not_reached ();
}

static void
test_subprocess_fail (void)
{
  g_test_trap_subprocess ("/subprocess/fail:child",
                          0, G_TEST_TRAP_SILENCE_STDERR);
  g_test_trap_assert_failed ();
  g_test_trap_assert_stderr ("*ERROR*test_subprocess_fail_child*should not be reached*");
}

static void
test_subprocess_no_such_test_child (void)
{
  g_test_trap_subprocess ("/subprocess/this-test-does-not-exist",
                          0, G_TEST_TRAP_SILENCE_STDERR);
  g_assert_not_reached ();
}

static void
test_subprocess_no_such_test (void)
{
  g_test_trap_subprocess ("/subprocess/no-such-test:child",
                          0, G_TEST_TRAP_SILENCE_STDERR);
  g_test_trap_assert_failed ();
  g_test_trap_assert_stderr ("*assertion failed*g_test_case_exists*");
  g_test_trap_assert_stderr_unmatched ("*should not be reached*");
}

static void
test_subprocess_patterns_child (void)
{
  g_print ("some stdout text: somagic17\n");
  g_printerr ("some stderr text: semagic43\n");
  exit (0);
}

static void
test_subprocess_patterns (void)
{
  g_test_trap_subprocess ("/subprocess/patterns:child",
                          0, G_TEST_TRAP_SILENCE_STDOUT | G_TEST_TRAP_SILENCE_STDERR);
  g_test_trap_assert_passed ();
  g_test_trap_assert_stdout ("*somagic17*");
  g_test_trap_assert_stderr ("*semagic43*");
}

static void
test_subprocess_timeout_child (void)
{
  /* loop and sleep forever */
  while (TRUE)
    g_usleep (1000 * 1000);
}

static void
test_subprocess_timeout (void)
{
  /* allow child to run for only a fraction of a second */
  g_test_trap_subprocess ("/subprocess/timeout:child",
                          0.11 * 1000000, 0);
  g_test_trap_assert_failed ();
  g_assert (g_test_trap_reached_timeout ());
}

/* run a test with fixture setup and teardown */
typedef struct {
  guint  seed;
  guint  prime;
  gchar *msg;
} Fixturetest;
static void
fixturetest_setup (Fixturetest  *fix,
                   gconstpointer test_data)
{
  g_assert (test_data == (void*) 0xc0cac01a);
  fix->seed = 18;
  fix->prime = 19;
  fix->msg = g_strdup_printf ("%d", fix->prime);
}
static void
fixturetest_test (Fixturetest  *fix,
                  gconstpointer test_data)
{
  guint prime = g_spaced_primes_closest (fix->seed);
  g_assert_cmpint (prime, ==, fix->prime);
  prime = g_ascii_strtoull (fix->msg, NULL, 0);
  g_assert_cmpint (prime, ==, fix->prime);
  g_assert (test_data == (void*) 0xc0cac01a);
}
static void
fixturetest_teardown (Fixturetest  *fix,
                      gconstpointer test_data)
{
  g_assert (test_data == (void*) 0xc0cac01a);
  g_free (fix->msg);
}

static struct {
  int bit, vint1, vint2, irange;
  long double vdouble, drange;
} shared_rand_state;

static void
test_rand1 (void)
{
  shared_rand_state.bit = g_test_rand_bit();
  shared_rand_state.vint1 = g_test_rand_int();
  shared_rand_state.vint2 = g_test_rand_int();
  g_assert_cmpint (shared_rand_state.vint1, !=, shared_rand_state.vint2);
  shared_rand_state.irange = g_test_rand_int_range (17, 35);
  g_assert_cmpint (shared_rand_state.irange, >=, 17);
  g_assert_cmpint (shared_rand_state.irange, <=, 35);
  shared_rand_state.vdouble = g_test_rand_double();
  shared_rand_state.drange = g_test_rand_double_range (-999, +17);
  g_assert_cmpfloat (shared_rand_state.drange, >=, -999);
  g_assert_cmpfloat (shared_rand_state.drange, <=, +17);
}

static void
test_rand2 (void)
{
  /* this test only works if run after test1.
   * we do this to check that random number generators
   * are reseeded upon fixture setup.
   */
  g_assert_cmpint (shared_rand_state.bit, ==, g_test_rand_bit());
  g_assert_cmpint (shared_rand_state.vint1, ==, g_test_rand_int());
  g_assert_cmpint (shared_rand_state.vint2, ==, g_test_rand_int());
  g_assert_cmpint (shared_rand_state.irange, ==, g_test_rand_int_range (17, 35));
  g_assert_cmpfloat (shared_rand_state.vdouble, ==, g_test_rand_double());
  g_assert_cmpfloat (shared_rand_state.drange, ==, g_test_rand_double_range (-999, +17));
}

static void
test_data_test (gconstpointer test_data)
{
  g_assert (test_data == (void*) 0xc0c0baba);
}

static void
test_random_conversions (void)
{
  /* very simple conversion test using random numbers */
  int vint = g_test_rand_int();
  char *err, *str = g_strdup_printf ("%d", vint);
  gint64 vint64 = g_ascii_strtoll (str, &err, 10);
  g_assert_cmphex (vint, ==, vint64);
  g_assert (!err || *err == 0);
  g_free (str);
}

static gboolean
fatal_handler (const gchar    *log_domain,
               GLogLevelFlags  log_level,
               const gchar    *message,
               gpointer        user_data)
{
  return FALSE;
}

static void
test_fatal_log_handler_critical_pass (void)
{
  g_test_log_set_fatal_handler (fatal_handler, NULL);
  g_str_has_prefix (NULL, "file://");
  g_critical ("Test passing");
  exit (0);
}

static void
test_fatal_log_handler_error_fail (void)
{
  g_error ("Test failing");
  exit (0);
}

static void
test_fatal_log_handler_critical_fail (void)
{
  g_str_has_prefix (NULL, "file://");
  g_critical ("Test passing");
  exit (0);
}

static void
test_fatal_log_handler (void)
{
  g_test_trap_subprocess ("/misc/fatal-log-handler:critical-pass",
                          0, G_TEST_TRAP_SILENCE_STDERR);
  g_test_trap_assert_passed ();
  g_test_trap_assert_stderr ("*CRITICAL*g_str_has_prefix*");
  g_test_trap_assert_stderr ("*CRITICAL*Test passing*");

  g_test_trap_subprocess ("/misc/fatal-log-handler:error-fail",
                          0, G_TEST_TRAP_SILENCE_STDERR);
  g_test_trap_assert_failed ();
  g_test_trap_assert_stderr ("*ERROR*Test failing*");

  g_test_trap_subprocess ("/misc/fatal-log-handler:critical-fail",
                          0, G_TEST_TRAP_SILENCE_STDERR);
  g_test_trap_assert_failed ();
  g_test_trap_assert_stderr ("*CRITICAL*g_str_has_prefix*");
  g_test_trap_assert_stderr_unmatched ("*CRITICAL*Test passing*");
}

static void
test_expected_messages_warning (void)
{
  g_warning ("This is a %d warning", g_random_int ());
  g_return_if_reached ();
}

static void
test_expected_messages_expect_warning (void)
{
  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
                         "This is a * warning");
  test_expected_messages_warning ();
}

static void
test_expected_messages_wrong_warning (void)
{
  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL,
                         "*should not be *");
  test_expected_messages_warning ();
}

static void
test_expected_messages_expected (void)
{
  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
                         "This is a * warning");
  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL,
                         "*should not be reached");

  test_expected_messages_warning ();

  g_test_assert_expected_messages ();
  exit (0);
}

static void
test_expected_messages_extra_warning (void)
{
  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
                         "This is a * warning");
  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL,
                         "*should not be reached");
  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL,
                         "nope");

  test_expected_messages_warning ();

  /* If we don't assert, it won't notice the missing message */
  exit (0);
}

static void
test_expected_messages_unexpected_extra_warning (void)
{
  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
                         "This is a * warning");
  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL,
                         "*should not be reached");
  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL,
                         "nope");

  test_expected_messages_warning ();

  g_test_assert_expected_messages ();
  exit (0);
}

static void
test_expected_messages (void)
{
  g_test_trap_subprocess ("/misc/expected-messages:warning",
                          0, G_TEST_TRAP_SILENCE_STDERR);
  g_test_trap_assert_failed ();
  g_test_trap_assert_stderr ("*This is a * warning*");
  g_test_trap_assert_stderr_unmatched ("*should not be reached*");

  g_test_trap_subprocess ("/misc/expected-messages:expect-warning",
                          0, G_TEST_TRAP_SILENCE_STDERR);
  g_test_trap_assert_failed ();
  g_test_trap_assert_stderr_unmatched ("*This is a * warning*");
  g_test_trap_assert_stderr ("*should not be reached*");

  g_test_trap_subprocess ("/misc/expected-messages:wrong-warning",
                          0, G_TEST_TRAP_SILENCE_STDERR);
  g_test_trap_assert_failed ();
  g_test_trap_assert_stderr_unmatched ("*should not be reached*");
  g_test_trap_assert_stderr ("*Did not see expected message CRITICAL*should not be *WARNING*This is a * warning*");

  g_test_trap_subprocess ("/misc/expected-messages:expected",
                          0, G_TEST_TRAP_SILENCE_STDERR);
  g_test_trap_assert_passed ();
  g_test_trap_assert_stderr ("");

  g_test_trap_subprocess ("/misc/expected-messages:extra-warning",
                          0, G_TEST_TRAP_SILENCE_STDERR);
  g_test_trap_assert_passed ();
  g_test_trap_assert_stderr ("");

  g_test_trap_subprocess ("/misc/expected-messages:unexpected-extra-warning",
                          0, G_TEST_TRAP_SILENCE_STDERR);
  g_test_trap_assert_failed ();
  g_test_trap_assert_stderr ("*Did not see expected message CRITICAL*nope*");
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/random-generator/rand-1", test_rand1);
  g_test_add_func ("/random-generator/rand-2", test_rand2);
  g_test_add_func ("/random-generator/random-conversions", test_random_conversions);
  g_test_add_func ("/misc/assertions", test_assertions);
  g_test_add_func ("/misc/assertions:bad_cmpstr", test_assertions_bad_cmpstr);
  g_test_add_func ("/misc/assertions:bad_cmpint", test_assertions_bad_cmpint);
  g_test_add_data_func ("/misc/test-data", (void*) 0xc0c0baba, test_data_test);
  g_test_add ("/misc/primetoul", Fixturetest, (void*) 0xc0cac01a, fixturetest_setup, fixturetest_test, fixturetest_teardown);
  if (g_test_perf())
    g_test_add_func ("/misc/timer", test_timer);

#ifdef G_OS_UNIX
  g_test_add_func ("/forking/fail assertion", test_fork_fail);
  g_test_add_func ("/forking/patterns", test_fork_patterns);
  if (g_test_slow())
    g_test_add_func ("/forking/timeout", test_fork_timeout);
#endif

  g_test_add_func ("/subprocess/fail", test_subprocess_fail);
  g_test_add_func ("/subprocess/fail:child", test_subprocess_fail_child);
  g_test_add_func ("/subprocess/no-such-test", test_subprocess_no_such_test);
  g_test_add_func ("/subprocess/no-such-test:child", test_subprocess_no_such_test_child);
  if (g_test_slow ())
    {
      g_test_add_func ("/subprocess/timeout", test_subprocess_timeout);
      g_test_add_func ("/subprocess/timeout:child", test_subprocess_timeout_child);
    }
  g_test_add_func ("/subprocess/patterns", test_subprocess_patterns);
  g_test_add_func ("/subprocess/patterns:child", test_subprocess_patterns_child);

  g_test_add_func ("/misc/fatal-log-handler", test_fatal_log_handler);
  g_test_add_func ("/misc/fatal-log-handler:critical-pass", test_fatal_log_handler_critical_pass);
  g_test_add_func ("/misc/fatal-log-handler:error-fail", test_fatal_log_handler_error_fail);
  g_test_add_func ("/misc/fatal-log-handler:critical-fail", test_fatal_log_handler_critical_fail);

  g_test_add_func ("/misc/expected-messages", test_expected_messages);
  g_test_add_func ("/misc/expected-messages:warning", test_expected_messages_warning);
  g_test_add_func ("/misc/expected-messages:expect-warning", test_expected_messages_expect_warning);
  g_test_add_func ("/misc/expected-messages:wrong-warning", test_expected_messages_wrong_warning);
  g_test_add_func ("/misc/expected-messages:expected", test_expected_messages_expected);
  g_test_add_func ("/misc/expected-messages:extra-warning", test_expected_messages_extra_warning);
  g_test_add_func ("/misc/expected-messages:unexpected-extra-warning", test_expected_messages_unexpected_extra_warning);

  return g_test_run();
}
