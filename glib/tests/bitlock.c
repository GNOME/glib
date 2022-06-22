#include <glib.h>

static void
test_bitlocks (void)
{
  guint64 start = g_get_monotonic_time ();
  gint lock = 0;
  guint i;
  guint n_iterations;

  n_iterations = g_test_perf () ? 100000000 : 1;

  for (i = 0; i < n_iterations; i++)
    {
      g_bit_lock (&lock, 0);
      g_bit_unlock (&lock, 0);
    }

  {
    gdouble elapsed;
    gdouble rate;

    elapsed = g_get_monotonic_time () - start;
    elapsed /= 1000000;
    rate = n_iterations / elapsed;

    g_test_maximized_result (rate, "iterations per second");
  }
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/bitlock/performance/uncontended", test_bitlocks);

  return g_test_run ();
}
