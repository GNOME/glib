#include <glib.h>

static void
test_slice_config (void)
{
  if (!g_test_undefined ())
    return;

  if (g_test_trap_fork (1000000, G_TEST_TRAP_SILENCE_STDERR))
    g_slice_set_config (G_SLICE_CONFIG_ALWAYS_MALLOC, TRUE);

  g_test_trap_assert_failed ();
}

int
main (int argc, char **argv)
{
  /* have to do this before using gtester since it uses gslice */
  gboolean was;

  was = g_slice_get_config (G_SLICE_CONFIG_ALWAYS_MALLOC);
  g_slice_set_config (G_SLICE_CONFIG_ALWAYS_MALLOC, !was);
  g_assert_cmpint (g_slice_get_config (G_SLICE_CONFIG_ALWAYS_MALLOC), !=, was);
  g_slice_set_config (G_SLICE_CONFIG_ALWAYS_MALLOC, was);

  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/slice/config", test_slice_config);

  return g_test_run ();
}
