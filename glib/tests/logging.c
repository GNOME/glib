#include <stdlib.h>
#include <glib.h>

/* Test g_warn macros */
static void
test_warnings (void)
{
  if (!g_test_undefined ())
    return;

  if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR))
    {
      g_warn_if_reached ();
    }
  g_test_trap_assert_failed();
  g_test_trap_assert_stderr ("*WARNING*test_warnings*should not be reached*");

  if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR))
    {
      g_warn_if_fail (FALSE);
    }
  g_test_trap_assert_failed();
  g_test_trap_assert_stderr ("*WARNING*test_warnings*runtime check failed*");
}

static guint log_count = 0;

static void
log_handler (const gchar    *log_domain,
             GLogLevelFlags  log_level,
             const gchar    *message,
             gpointer        user_data)
{
  g_assert_cmpstr (log_domain, ==, "bu");
  g_assert_cmpint (log_level, ==, G_LOG_LEVEL_INFO);

  log_count++;
}

/* test that custom log handlers only get called for
 * their domain and level
 */
static void
test_set_handler (void)
{
  guint id;

  id = g_log_set_handler ("bu", G_LOG_LEVEL_INFO, log_handler, NULL);

  if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDOUT))
    {
      g_log ("bu", G_LOG_LEVEL_DEBUG, "message");
      g_log ("ba", G_LOG_LEVEL_DEBUG, "message");
      g_log ("bu", G_LOG_LEVEL_INFO, "message");
      g_log ("ba", G_LOG_LEVEL_INFO, "message");

      g_assert_cmpint (log_count, ==, 1);
      exit (0);
    }
  g_test_trap_assert_passed ();

  g_log_remove_handler ("bu", id);
}

static void
test_default_handler (void)
{
  if (g_test_undefined ())
    {
      if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR))
        {
          g_error ("message1");
          exit (0);
        }
      g_test_trap_assert_failed ();
      g_test_trap_assert_stderr ("*ERROR*message1*");

      if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR))
        {
          g_critical ("message2");
          exit (0);
        }
      g_test_trap_assert_failed ();
      g_test_trap_assert_stderr ("*CRITICAL*message2*");

      if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR))
        {
          g_warning ("message3");
          exit (0);
        }
      g_test_trap_assert_failed ();
      g_test_trap_assert_stderr ("*WARNING*message3*");
    }

  if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR))
    {
      g_message ("message4");
      exit (0);
    }
  g_test_trap_assert_passed ();
  g_test_trap_assert_stderr ("*Message*message4*");

  if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDOUT))
    {
      g_log (G_LOG_DOMAIN, G_LOG_LEVEL_INFO, "message5");
      exit (0);
    }
  g_test_trap_assert_passed ();
  g_test_trap_assert_stdout_unmatched ("*INFO*message5*");

  g_setenv ("G_MESSAGES_DEBUG", "foo bar baz", TRUE);
  if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDOUT))
    {
      g_log ("bar", G_LOG_LEVEL_INFO, "message5");
      exit (0);
    }
  g_test_trap_assert_passed ();
  g_test_trap_assert_stdout ("*INFO*message5*");

  if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDOUT))
    {
      g_log ("baz", G_LOG_LEVEL_DEBUG, "message6");
      exit (0);
    }
  g_test_trap_assert_passed ();
  g_test_trap_assert_stdout ("*DEBUG*message6*");

  g_setenv ("G_MESSAGES_DEBUG", "all", TRUE);
  if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDOUT))
    {
      g_log ("foo", G_LOG_LEVEL_DEBUG, "6");
      g_log ("bar", G_LOG_LEVEL_DEBUG, "6");
      g_log ("baz", G_LOG_LEVEL_DEBUG, "6");
      exit (0);
    }
  g_test_trap_assert_passed ();
  g_test_trap_assert_stdout ("*DEBUG*6*6*6*");

  g_unsetenv ("G_MESSAGES_DEBUG");
  if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDOUT))
    {
      g_log (G_LOG_DOMAIN, 1<<10, "message7");
      exit (0);
    }
  g_test_trap_assert_passed ();
  g_test_trap_assert_stdout ("*LOG-0x400*message7*");
}

/* test that setting levels as fatal works */
static void
test_fatal_log_mask (void)
{
  GLogLevelFlags flags;

  if (!g_test_undefined ())
    return;

  flags = g_log_set_fatal_mask ("bu", G_LOG_LEVEL_INFO);
  if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDOUT))
    g_log ("bu", G_LOG_LEVEL_INFO, "fatal");
  g_test_trap_assert_failed ();
  g_log_set_fatal_mask ("bu", flags);
}

static gint my_print_count = 0;
static void
my_print_handler (const gchar *text)
{
  my_print_count++;
}

static void
test_print_handler (void)
{
  GPrintFunc old_print_handler;

  old_print_handler = g_set_print_handler (my_print_handler);
  g_assert (old_print_handler == NULL);

  my_print_count = 0;
  g_print ("bu ba");
  g_assert_cmpint (my_print_count, ==, 1);

  g_set_print_handler (NULL);
}

static void
test_printerr_handler (void)
{
  GPrintFunc old_printerr_handler;

  old_printerr_handler = g_set_printerr_handler (my_print_handler);
  g_assert (old_printerr_handler == NULL);

  my_print_count = 0;
  g_printerr ("bu ba");
  g_assert_cmpint (my_print_count, ==, 1);

  g_set_printerr_handler (NULL);
}

static char *fail_str = "foo";
static char *log_str = "bar";

static gboolean
good_failure_handler (const gchar    *log_domain,
                      GLogLevelFlags  log_level,
                      const gchar    *msg,
                      gpointer        user_data)
{
  g_test_message ("The Good Fail Message Handler\n");
  g_assert ((char *)user_data != log_str);
  g_assert ((char *)user_data == fail_str);

  return FALSE;
}

static gboolean
bad_failure_handler (const gchar    *log_domain,
                     GLogLevelFlags  log_level,
                     const gchar    *msg,
                     gpointer        user_data)
{
  g_test_message ("The Bad Fail Message Handler\n");
  g_assert ((char *)user_data == log_str);
  g_assert ((char *)user_data != fail_str);

  return FALSE;
}

static void
test_handler (const gchar    *log_domain,
              GLogLevelFlags  log_level,
              const gchar    *msg,
              gpointer        user_data)
{
  g_test_message ("The Log Message Handler\n");
  g_assert ((char *)user_data != fail_str);
  g_assert ((char *)user_data == log_str);
}

static void
bug653052 (void)
{
  if (!g_test_undefined ())
    return;

  g_test_bug ("653052");

  g_test_log_set_fatal_handler (good_failure_handler, fail_str);
  g_log_set_default_handler (test_handler, log_str);

  g_return_if_fail (0);

  g_test_log_set_fatal_handler (bad_failure_handler, fail_str);
  g_log_set_default_handler (test_handler, log_str);

  g_return_if_fail (0);
}

int
main (int argc, char *argv[])
{
  g_unsetenv ("G_MESSAGES_DEBUG");

  g_test_init (&argc, &argv, NULL);
  g_test_bug_base ("http://bugzilla.gnome.org/");

  g_test_add_func ("/logging/default-handler", test_default_handler);
  g_test_add_func ("/logging/warnings", test_warnings);
  g_test_add_func ("/logging/fatal-log-mask", test_fatal_log_mask);
  g_test_add_func ("/logging/set-handler", test_set_handler);
  g_test_add_func ("/logging/print-handler", test_print_handler);
  g_test_add_func ("/logging/printerr-handler", test_printerr_handler);
  g_test_add_func ("/logging/653052", bug653052);

  return g_test_run ();
}

