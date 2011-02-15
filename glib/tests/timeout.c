#include <glib.h>

static GMainLoop *loop;

static gboolean
stop_waiting (gpointer data)
{
  g_main_loop_quit (loop);

  return FALSE;
}

static gboolean
function (gpointer data)
{
  g_assert_not_reached ();
}

static void
test_seconds (void)
{
  /* Bug 642052 mentions that g_timeout_add_seconds(21475) schedules a
   * job that runs once per second.
   *
   * Test that that isn't true anymore by scheduling two jobs:
   *   - one, as above
   *   - another that runs in 2100ms
   *
   * If everything is working properly, the 2100ms one should run first
   * (and exit the mainloop).  If we ever see the 21475 second job run
   * then we have trouble (since it ran in less than 2 seconds).
   *
   * We need a timeout of at least 2 seconds because
   * g_timeout_add_second can add as much as an additional second of
   * latency.
   */
  loop = g_main_loop_new (NULL, FALSE);

  g_timeout_add (2100, stop_waiting, NULL);
  g_timeout_add_seconds (21475, function, NULL);

  g_main_loop_run (loop);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/timeout/seconds", test_seconds);

  return g_test_run ();
}
