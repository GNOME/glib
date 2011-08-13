#include <glib.h>
#include <unistd.h>

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

static gint64 last_time;
static gint count;

static gboolean
test_func (gpointer data)
{
  gint64 current_time;

  current_time = g_get_monotonic_time ();

  /* We accept 2 on the first iteration because _add_seconds() can
   * have an initial latency of 1 second, see its documentation.
   */
  if (count == 0)
    g_assert (current_time / 1000000 - last_time / 1000000 <= 2);
  else
    g_assert (current_time / 1000000 - last_time / 1000000 == 1);

  last_time = current_time;
  count++;

  /* Make the timeout take up to 0.1 seconds.
   * We should still get scheduled for the next second.
   */
  g_usleep (count * 10000);

  if (count < 10)
    return TRUE;

  g_main_loop_quit (loop);

  return FALSE;
}

static void
test_rounding (void)
{
  loop = g_main_loop_new (NULL, FALSE);

  last_time = g_get_monotonic_time ();
  g_timeout_add_seconds (1, test_func, NULL);

  g_main_loop_run (loop);
}

static gboolean
on_test_date_time_watch_timeout (gpointer user_data)
{
  *((gboolean*)user_data) = TRUE;

  g_main_loop_quit (loop);

  return TRUE;
}

/* This test isn't very useful; it's hard to actually test much of the
 * functionality of g_date_time_source_new() without a means to set
 * the system clock (which typically requires system-specific
 * interfaces as well as elevated privileges).
 *
 * But at least we're running the code and ensuring the timer fires.
 */
static void
test_date_time_create_watch (gboolean cancel_on_set)
{
  GSource *source;
  GDateTime *now, *expiry;
  gboolean fired = FALSE;
  gint64 orig_time_monotonic, end_time_monotonic;
  gint64 elapsed_monotonic_seconds;
  
  loop = g_main_loop_new (NULL, FALSE);

  orig_time_monotonic = g_get_monotonic_time ();

  now = g_date_time_new_now_local ();
  expiry = g_date_time_add_seconds (now, 7);
  g_date_time_unref (now);

  source = g_date_time_source_new (expiry, cancel_on_set);
  g_source_set_callback (source, on_test_date_time_watch_timeout, &fired, NULL);
  g_source_attach (source, NULL);
  g_source_unref (source);

  g_main_loop_run (loop);

  g_assert (fired);
  if (!cancel_on_set)
    {
      end_time_monotonic = g_get_monotonic_time ();
      
      elapsed_monotonic_seconds = 1 + (end_time_monotonic - orig_time_monotonic) / G_TIME_SPAN_SECOND;
      
      g_assert_cmpint (elapsed_monotonic_seconds, >=, 7);
    }
  else
    {
      /* We can't really assert much about the cancel_on_set case */
    }
}

static void
test_date_time_create_watch_nocancel_on_set (void)
{
  test_date_time_create_watch (FALSE);
}

static void
test_date_time_create_watch_cancel_on_set (void)
{
  test_date_time_create_watch (TRUE);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/timeout/seconds", test_seconds);
  g_test_add_func ("/timeout/rounding", test_rounding);
  g_test_add_func ("/timeout/datetime_watch_nocancel_on_set", test_date_time_create_watch_nocancel_on_set);
  g_test_add_func ("/timeout/datetime_watch_cancel_on_set", test_date_time_create_watch_cancel_on_set);

  return g_test_run ();
}
