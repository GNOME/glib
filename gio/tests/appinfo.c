
#include <locale.h>

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

static void
test_locale (const char *locale)
{
  GAppInfo *appinfo;
  const gchar *orig;

  orig = setlocale (LC_ALL, NULL);
  g_setenv ("LANGUAGE", locale, TRUE);
  setlocale (LC_ALL, "");

  appinfo = (GAppInfo*)g_desktop_app_info_new_from_filename (SRCDIR "/appinfo-test.desktop");

  if (g_strcmp0 (locale, "C") == 0)
    {
      g_assert_cmpstr (g_app_info_get_name (appinfo), ==, "appinfo-test");
      g_assert_cmpstr (g_app_info_get_description (appinfo), ==, "GAppInfo example");
      g_assert_cmpstr (g_app_info_get_display_name (appinfo), ==, "example");
    }
  else if (g_str_has_prefix (locale, "en"))
    {
      g_assert_cmpstr (g_app_info_get_name (appinfo), ==, "appinfo-test");
      g_assert_cmpstr (g_app_info_get_description (appinfo), ==, "GAppInfo example");
      g_assert_cmpstr (g_app_info_get_display_name (appinfo), ==, "example");
    }
  else if (g_str_has_prefix (locale, "de"))
    {
      g_assert_cmpstr (g_app_info_get_name (appinfo), ==, "appinfo-test-de");
      g_assert_cmpstr (g_app_info_get_description (appinfo), ==, "GAppInfo Beispiel");
      g_assert_cmpstr (g_app_info_get_display_name (appinfo), ==, "Beispiel");
    }

  g_object_unref (appinfo);

  g_setenv ("LANGUAGE", orig, TRUE);
  setlocale (LC_ALL, "");
}

static void
test_text (void)
{
  test_locale ("C");
  test_locale ("en_US");
  test_locale ("de");
  test_locale ("de_DE.UTF-8");
}

static void
test_basic (void)
{
  GAppInfo *appinfo;
  GIcon *icon, *icon2;

  appinfo = (GAppInfo*)g_desktop_app_info_new_from_filename (SRCDIR "/appinfo-test.desktop");

  g_assert (g_app_info_get_id (appinfo) == NULL);
  g_assert_cmpstr (g_app_info_get_executable (appinfo), ==, "./appinfo-test");
  g_assert_cmpstr (g_app_info_get_commandline (appinfo), ==, "./appinfo-test --option");

  icon = g_app_info_get_icon (appinfo);
  g_assert (G_IS_THEMED_ICON (icon));
  icon2 = g_themed_icon_new ("testicon");
  g_assert (g_icon_equal (icon, icon2));
  g_object_unref (icon2);

  g_object_unref (appinfo);
}

static void
test_show_in (void)
{
  GAppInfo *appinfo;

  g_desktop_app_info_set_desktop_env ("GNOME");

  appinfo = (GAppInfo*)g_desktop_app_info_new_from_filename (SRCDIR "/appinfo-test.desktop");
  g_assert (g_app_info_should_show (appinfo));
  g_object_unref (appinfo);

  appinfo = (GAppInfo*)g_desktop_app_info_new_from_filename (SRCDIR "/appinfo-test-gnome.desktop");
  g_assert (g_app_info_should_show (appinfo));
  g_object_unref (appinfo);

  appinfo = (GAppInfo*)g_desktop_app_info_new_from_filename (SRCDIR "/appinfo-test-notgnome.desktop");
  g_assert (!g_app_info_should_show (appinfo));
  g_object_unref (appinfo);
}

static void
test_commandline (void)
{
  GAppInfo *appinfo;
  GError *error;

  error = NULL;
  appinfo = g_app_info_create_from_commandline ("./appinfo-test --option",
                                                "cmdline-app-test",
                                                G_APP_INFO_CREATE_SUPPORTS_URIS,
                                                &error);
  g_assert (appinfo != NULL);
  g_assert_no_error (error);
  g_assert_cmpstr (g_app_info_get_name (appinfo), ==, "cmdline-app-test");
  g_assert_cmpstr (g_app_info_get_commandline (appinfo), ==, "./appinfo-test --option %u");
  g_assert (g_app_info_supports_uris (appinfo));
  g_assert (!g_app_info_supports_files (appinfo));

  g_object_unref (appinfo);

  error = NULL;
  appinfo = g_app_info_create_from_commandline ("./appinfo-test --option",
                                                "cmdline-app-test",
                                                G_APP_INFO_CREATE_NONE,
                                                &error);
  g_assert (appinfo != NULL);
  g_assert_no_error (error);
  g_assert_cmpstr (g_app_info_get_name (appinfo), ==, "cmdline-app-test");
  g_assert_cmpstr (g_app_info_get_commandline (appinfo), ==, "./appinfo-test --option %f");
  g_assert (!g_app_info_supports_uris (appinfo));
  g_assert (g_app_info_supports_files (appinfo));

  g_object_unref (appinfo);
}

int
main (int argc, char *argv[])
{
  g_type_init ();
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/appinfo/basic", test_basic);
  g_test_add_func ("/appinfo/text", test_text);
  g_test_add_func ("/appinfo/launch", test_launch);
  g_test_add_func ("/appinfo/show-in", test_show_in);
  g_test_add_func ("/appinfo/commandline", test_commandline);

  return g_test_run ();
}

