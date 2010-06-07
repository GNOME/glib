#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>

static void
test_launch (void)
{
  GAppInfo *appinfo;

  appinfo = (GAppInfo*)g_desktop_app_info_new_from_filename (SRCDIR "/appinfo-test.desktop");
  g_assert (appinfo != NULL);

  g_assert (g_app_info_launch (appinfo, NULL, NULL, NULL));
}

int
main (int argc, char *argv[])
{
  g_type_init ();
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/appinfo/launch", test_launch);

  return g_test_run ();
}

