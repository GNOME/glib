#include <gio.h>

int
main (void)
{
  GSettings *settings;
  gchar *str = NULL;

  g_type_init ();

  settings = g_settings_new ("org.gtk.test");

  g_settings_get (settings, "greeting", "s", &str);
  g_print ("it was '%s'\n", str);

  g_settings_set (settings, "greeting", "s", "goodbye world");

  g_settings_get (settings, "greeting", "s", &str);
  g_print ("it is now '%s'\n", str);

  g_settings_set (settings, "greeting", "i", 555);

  g_settings_get (settings, "greeting", "s", &str);
  g_print ("finally, it is '%s'\n", str);
}
