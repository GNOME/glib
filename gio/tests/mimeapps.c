#include <glib/gstdio.h>
#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>

static gboolean
strv_equal (gchar **strv, ...)
{
  gint count;
  va_list list;
  const gchar *str;
  gboolean res;

  res = TRUE;
  count = 0;
  va_start (list, strv);
  while (1)
    {
      str = va_arg (list, const gchar *);
      if (str == NULL)
        break;
      if (g_strcmp0 (str, strv[count]) != 0)
        {
          res = FALSE;
          break;
        }
      count++;
    }
  va_end (list);

  if (res)
    res = g_strv_length (strv) == count;

  return res;
}


const gchar *myapp_data =
  "[Desktop Entry]\n"
  "Encoding=UTF-8\n"
  "Version=1.0\n"
  "Type=Application\n"
  "Exec=my_app %f\n"
  "Name=my app\n";

const gchar *myapp2_data =
  "[Desktop Entry]\n"
  "Encoding=UTF-8\n"
  "Version=1.0\n"
  "Type=Application\n"
  "Exec=my_app2 %f\n"
  "Name=my app 2\n";

/* Test that we handle mimeapps.list as expected.
 * These tests are different from the ones in appinfo.c
 * in that we directly parse mimeapps.list here
 * to verify the results.
 *
 * We need to keep this test in a separate binary, since
 * g_get_user_data_dir() doesn't get updated at runtime.
 */
static void
test_mimeapps (void)
{
  gchar *dir;
  gchar *xdgdir;
  gchar *appdir;
  gchar *mimeapps;
  gchar *name;
  gchar **assoc;
  GAppInfo *appinfo;
  GAppInfo *appinfo2;
  GError *error = NULL;
  GKeyFile *keyfile;
  gchar *str;
  gboolean res;
  GAppInfo *def;
  GList *list;

  dir = g_get_current_dir ();
  xdgdir = g_build_filename (dir, "xdgdatahome", NULL);
  g_test_message ("setting XDG_DATA_HOME to '%s'\n", xdgdir);
  g_setenv ("XDG_DATA_HOME", xdgdir, TRUE);
  g_setenv ("XDG_DATA_DIRS", " ", TRUE);

  appdir = g_build_filename (xdgdir, "applications", NULL);
  g_test_message ("creating '%s'\n", appdir);
  res = g_mkdir_with_parents (appdir, 0700);
  g_assert (res == 0);

  name = g_build_filename (appdir, "myapp.desktop", NULL);
  g_test_message ("creating '%s'\n", name);
  g_file_set_contents (name, myapp_data, -1, &error);
  g_assert_no_error (error);
  g_free (name);

  name = g_build_filename (appdir, "myapp2.desktop", NULL);
  g_test_message ("creating '%s'\n", name);
  g_file_set_contents (name, myapp2_data, -1, &error);
  g_assert_no_error (error);
  g_free (name);

  mimeapps = g_build_filename (appdir, "mimeapps.list", NULL);
  g_test_message ("removing '%s'\n", mimeapps);
  g_remove (mimeapps);

  /* 1. add a non-default association */
  appinfo = (GAppInfo*)g_desktop_app_info_new ("myapp.desktop");
  g_app_info_add_supports_type (appinfo, "application/pdf", &error);
  g_assert_no_error (error);

  /* check api results */
  def = g_app_info_get_default_for_type ("application/pdf", FALSE);
  list = g_app_info_get_recommended_for_type ("application/pdf");
  g_assert (g_app_info_equal (def, appinfo));
  g_assert_cmpint (g_list_length (list), ==, 1);
  g_assert (g_app_info_equal ((GAppInfo*)list->data, appinfo));
  g_object_unref (def);
  g_list_free_full (list, g_object_unref);

  /* check mimeapps.list */
  keyfile = g_key_file_new ();
  g_key_file_load_from_file (keyfile, mimeapps, G_KEY_FILE_NONE, &error);
  g_assert_no_error (error);

  assoc = g_key_file_get_string_list (keyfile, "Added Associations", "application/pdf", NULL, &error);
  g_assert_no_error (error);
  g_assert (strv_equal (assoc, "myapp.desktop", NULL));
  g_strfreev (assoc);

  /* we've unset XDG_DATA_DIRS so there should be no default */
  assoc = g_key_file_get_string_list (keyfile, "Default Applications", "application/pdf", NULL, &error);
  g_assert_error (error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_GROUP_NOT_FOUND);
  g_clear_error (&error);

  g_key_file_free (keyfile);

  /* 2. add another non-default association */
  appinfo2 = (GAppInfo*)g_desktop_app_info_new ("myapp2.desktop");
  g_app_info_add_supports_type (appinfo2, "application/pdf", &error);
  g_assert_no_error (error);

  /* check api results */
  def = g_app_info_get_default_for_type ("application/pdf", FALSE);
  list = g_app_info_get_recommended_for_type ("application/pdf");
  g_assert (g_app_info_equal (def, appinfo));
  g_assert_cmpint (g_list_length (list), ==, 2);
  g_assert (g_app_info_equal ((GAppInfo*)list->data, appinfo));
  g_assert (g_app_info_equal ((GAppInfo*)list->next->data, appinfo2));
  g_object_unref (def);
  g_list_free_full (list, g_object_unref);

  /* check mimeapps.list */
  keyfile = g_key_file_new ();
  g_key_file_load_from_file (keyfile, mimeapps, G_KEY_FILE_NONE, &error);
  g_assert_no_error (error);

  assoc = g_key_file_get_string_list (keyfile, "Added Associations", "application/pdf", NULL, &error);
  g_assert_no_error (error);
  g_assert (strv_equal (assoc, "myapp.desktop", "myapp2.desktop", NULL));
  g_strfreev (assoc);

  assoc = g_key_file_get_string_list (keyfile, "Default Applications", "application/pdf", NULL, &error);
  g_assert_error (error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_GROUP_NOT_FOUND);
  g_clear_error (&error);

  g_key_file_free (keyfile);

  /* 3. make the first app the default */
  g_app_info_set_as_default_for_type (appinfo, "application/pdf", &error);
  g_assert_no_error (error);

  /* check api results */
  def = g_app_info_get_default_for_type ("application/pdf", FALSE);
  list = g_app_info_get_recommended_for_type ("application/pdf");
  g_assert (g_app_info_equal (def, appinfo));
  g_assert_cmpint (g_list_length (list), ==, 2);
  g_assert (g_app_info_equal ((GAppInfo*)list->data, appinfo));
  g_assert (g_app_info_equal ((GAppInfo*)list->next->data, appinfo2));
  g_object_unref (def);
  g_list_free_full (list, g_object_unref);

  /* check mimeapps.list */
  keyfile = g_key_file_new ();
  g_key_file_load_from_file (keyfile, mimeapps, G_KEY_FILE_NONE, &error);
  g_assert_no_error (error);

  assoc = g_key_file_get_string_list (keyfile, "Added Associations", "application/pdf", NULL, &error);
  g_assert_no_error (error);
  g_assert (strv_equal (assoc, "myapp.desktop", "myapp2.desktop", NULL));
  g_strfreev (assoc);

  str = g_key_file_get_string (keyfile, "Default Applications", "application/pdf", &error);
  g_assert_no_error (error);
  g_assert_cmpstr (str, ==, "myapp.desktop");

  g_key_file_free (keyfile);

  /* 4. make the second app the last used one */
  g_app_info_set_as_last_used_for_type (appinfo2, "application/pdf", &error);
  g_assert_no_error (error);

  /* check api results */
  def = g_app_info_get_default_for_type ("application/pdf", FALSE);
  list = g_app_info_get_recommended_for_type ("application/pdf");
  g_assert (g_app_info_equal (def, appinfo));
  g_assert_cmpint (g_list_length (list), ==, 2);
  g_assert (g_app_info_equal ((GAppInfo*)list->data, appinfo2));
  g_assert (g_app_info_equal ((GAppInfo*)list->next->data, appinfo));
  g_object_unref (def);
  g_list_free_full (list, g_object_unref);

  /* check mimeapps.list */
  keyfile = g_key_file_new ();
  g_key_file_load_from_file (keyfile, mimeapps, G_KEY_FILE_NONE, &error);
  g_assert_no_error (error);

  assoc = g_key_file_get_string_list (keyfile, "Added Associations", "application/pdf", NULL, &error);
  g_assert_no_error (error);
  g_assert (strv_equal (assoc, "myapp2.desktop", "myapp.desktop", NULL));
  g_strfreev (assoc);

  g_key_file_free (keyfile);

  /* 5. reset everything */
  g_app_info_reset_type_associations ("application/pdf");

  /* check api results */
  def = g_app_info_get_default_for_type ("application/pdf", FALSE);
  list = g_app_info_get_recommended_for_type ("application/pdf");
  g_assert (def == NULL);
  g_assert (list == NULL);

  /* check mimeapps.list */
  keyfile = g_key_file_new ();
  g_key_file_load_from_file (keyfile, mimeapps, G_KEY_FILE_NONE, &error);
  g_assert_no_error (error);

  res = g_key_file_has_key (keyfile, "Added Associations", "application/pdf", NULL);
  g_assert (!res);

  res = g_key_file_has_key (keyfile, "Default Applications", "application/pdf", NULL);
  g_assert (!res);

  g_key_file_free (keyfile);

  g_object_unref (appinfo);
  g_object_unref (appinfo2);

  g_free (dir);
  g_free (xdgdir);
  g_free (mimeapps);
  g_free (appdir);
}

int
main (int argc, char *argv[])
{
  g_type_init ();
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/appinfo/mimeapps", test_mimeapps);

  return g_test_run ();
}
