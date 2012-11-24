#include <glib.h>

/* We test deprecated functionality here */
G_GNUC_BEGIN_IGNORE_DEPRECATIONS

static void
test_slice_config_child (void)
{
  g_slice_set_config (G_SLICE_CONFIG_ALWAYS_MALLOC, TRUE);
}

static void
test_slice_config (void)
{
  g_test_trap_subprocess ("/slice/config:child",
                          1000000, G_TEST_TRAP_SILENCE_STDERR);
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
  g_test_add_func ("/slice/config:child", test_slice_config_child);

  return g_test_run ();
}
