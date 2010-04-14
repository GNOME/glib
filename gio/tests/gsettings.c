#include <gio.h>

static void
test_basic (void)
{
  gchar *str = NULL;
  GSettings *settings;

  settings = g_settings_new ("org.gtk.test");

  g_settings_get (settings, "greeting", "s", &str);
  g_assert_cmpstr (str, ==, "Hello, earthlings");

  g_settings_set (settings, "greeting", "s", "goodbye world");
  g_settings_get (settings, "greeting", "s", &str);
  g_assert_cmpstr (str, ==, "goodbye world");

  g_settings_set (settings, "greeting", "i", 555);

  g_settings_get (settings, "greeting", "s", &str);
#if 0
  /* FIXME: currently, the integer write is not failing, so we get hte schema default here */
  g_assert_cmpstr (str, ==, "goodbye world");
#endif
}

int
main (int argc, char *argv[])
{
  g_type_init ();
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/gsettings/basic", test_basic);

  return g_test_run ();
}
