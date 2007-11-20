/* GLib testing framework examples and tests
 * Copyright (C) 2007 Tim Janik
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
#include <glib/gtestframework.h>
#include <stdlib.h>

/* test assertion variants */
static void
test_assertions (void)
{
  g_assert_cmpint (1, >, 0);
  g_assert_cmphex (2, ==, 2);
  g_assert_cmpfloat (3.3, !=, 7);
  g_assert_cmpfloat (7, <=, 3 + 4);
  g_assert (TRUE);
  g_assert_cmpstr ("foo", !=, "faa");
  gchar *fuu = g_strdup_printf ("f%s", "uu");
  g_test_queue_free (fuu);
  g_assert_cmpstr ("foo", !=, fuu);
  g_assert_cmpstr ("fuu", ==, fuu);
  g_assert_cmpstr (NULL, <, "");
  g_assert_cmpstr (NULL, ==, NULL);
  g_assert_cmpstr ("", >, NULL);
  g_assert_cmpstr ("foo", <, "fzz");
  g_assert_cmpstr ("fzz", >, "faa");
  g_assert_cmpstr ("fzz", ==, "fzz");
}

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

/* run a test with fixture setup and teardown */
typedef struct {
  guint  seed;
  guint  prime;
  gchar *msg;
} Fixturetest;
static void
fixturetest_setup (Fixturetest *fix)
{
  fix->seed = 18;
  fix->prime = 19;
  fix->msg = g_strdup_printf ("%d", fix->prime);
}
static void
fixturetest_test (Fixturetest *fix)
{
  guint prime = g_spaced_primes_closest (fix->seed);
  g_assert_cmpint (prime, ==, fix->prime);
  prime = g_ascii_strtoull (fix->msg, NULL, 0);
  g_assert_cmpint (prime, ==, fix->prime);
}
static void
fixturetest_teardown (Fixturetest *fix)
{
  g_free (fix->msg);
}

int
main (int   argc,
      char *argv[])
{
  GTestCase *tc;
  g_test_init (&argc, &argv, NULL);
  GTestSuite *rootsuite = g_test_create_suite ("top");
  GTestSuite *miscsuite = g_test_create_suite ("misc");
  g_test_suite_add_suite (rootsuite, miscsuite);
  GTestSuite *forksuite = g_test_create_suite ("fork");
  g_test_suite_add_suite (rootsuite, forksuite);

  tc = g_test_create_case ("fail assertion", 0, NULL, test_fork_fail, NULL);
  g_test_suite_add (forksuite, tc);
  tc = g_test_create_case ("patterns", 0, NULL, test_fork_patterns, NULL);
  g_test_suite_add (forksuite, tc);
  tc = g_test_create_case ("timeout", 0, NULL, test_fork_timeout, NULL);
  g_test_suite_add (forksuite, tc);

  tc = g_test_create_case ("assertions", 0, NULL, test_assertions, NULL);
  g_test_suite_add (miscsuite, tc);
  tc = g_test_create_case ("primetoul", sizeof (Fixturetest),
                           (void(*)(void)) fixturetest_setup,
                           (void(*)(void)) fixturetest_test,
                           (void(*)(void)) fixturetest_teardown);
  g_test_suite_add (miscsuite, tc);

  return g_test_run_suite (rootsuite);
}
