/* 
 * Copyright (C) 2008 Red Hat, Inc
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 * 
 * Author: Matthias Clasen
 */

#include <glib/glib.h>
#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>
#include <stdlib.h>
#include <string.h>

static char *basedir;

static GAppInfo * 
create_app_info (const char *name)
{
  GError *error;
  GAppInfo *info;

  error = NULL;
  info = g_app_info_create_from_commandline ("/usr/bin/true blah",
                                             name,
                                             G_APP_INFO_CREATE_NONE,
                                             &error);
  g_assert (error == NULL);

  /* this is necessary to ensure that the info is saved */
  g_app_info_set_as_default_for_type (info, "application/x-blah", &error);
  g_assert (error == NULL);
  g_app_info_remove_supports_type (info, "application/x-blah", &error);
  g_assert (error == NULL);
  g_app_info_reset_type_associations ("application/x-blah");
  
  return info;
}

static void
test_delete (void)
{
  GAppInfo *info;

  const char *id;
  char *filename;
  gboolean res;

  info = create_app_info ("Blah");
 
  id = g_app_info_get_id (info);
  g_assert (id != NULL);

  filename = g_build_filename (basedir, "applications", id, NULL);

  res = g_file_test (filename, G_FILE_TEST_EXISTS);
  g_assert (res);

  res = g_app_info_can_delete (info);
  g_assert (res);

  res = g_app_info_delete (info);
  g_assert (res);

  res = g_file_test (filename, G_FILE_TEST_EXISTS);
  g_assert (!res);

  g_object_unref (info);

  if (g_file_test ("/usr/share/applications/gedit.desktop", G_FILE_TEST_EXISTS))
    {
      info = (GAppInfo*)g_desktop_app_info_new_from_filename ("/usr/share/applications/gedit.desktop");
      g_assert (info);
     
      res = g_app_info_can_delete (info);
      g_assert (!res);
 
      res = g_app_info_delete (info);
      g_assert (!res);
    }
}

static void
test_default (void)
{
  GAppInfo *info, *info1, *info2, *info3;
  GList *list;
  GError *error = NULL;  

  info1 = create_app_info ("Blah1");
  info2 = create_app_info ("Blah2");
  info3 = create_app_info ("Blah3");

  g_app_info_set_as_default_for_type (info1, "application/x-test", &error);
  g_assert (error == NULL);

  g_app_info_set_as_default_for_type (info2, "application/x-test", &error);
  g_assert (error == NULL);

  list = g_app_info_get_all_for_type ("application/x-test");
  g_assert (g_list_length (list) == 2);
  
  /* check that both are in the list, info2 before info1 */
  info = (GAppInfo *)list->data;
  g_assert (g_strcmp0 (g_app_info_get_id (info), g_app_info_get_id (info2)) == 0);

  info = (GAppInfo *)list->next->data;
  g_assert (g_strcmp0 (g_app_info_get_id (info), g_app_info_get_id (info1)) == 0);

  g_list_free_full (list, g_object_unref);

  /* now try adding something at the end */
  g_app_info_add_supports_type (info3, "application/x-test", &error);
  g_assert (error == NULL);

  list = g_app_info_get_all_for_type ("application/x-test");
  g_assert (g_list_length (list) == 3);
  
  /* check that all are in the list, info2, info1, info3 */
  info = (GAppInfo *)list->data;
  g_assert (g_strcmp0 (g_app_info_get_id (info), g_app_info_get_id (info2)) == 0);

  info = (GAppInfo *)list->next->data;
  g_assert (g_strcmp0 (g_app_info_get_id (info), g_app_info_get_id (info1)) == 0);

  info = (GAppInfo *)list->next->next->data;
  g_assert (g_strcmp0 (g_app_info_get_id (info), g_app_info_get_id (info3)) == 0);

  g_list_free_full (list, g_object_unref);

  /* now remove info1 again */
  g_app_info_remove_supports_type (info1, "application/x-test", &error);
  g_assert (error == NULL);

  list = g_app_info_get_all_for_type ("application/x-test");
  g_assert (g_list_length (list) == 2);
  
  /* check that both are in the list, info2 before info3 */
  info = (GAppInfo *)list->data;
  g_assert (g_strcmp0 (g_app_info_get_id (info), g_app_info_get_id (info2)) == 0);

  info = (GAppInfo *)list->next->data;
  g_assert (g_strcmp0 (g_app_info_get_id (info), g_app_info_get_id (info3)) == 0);

  g_list_free_full (list, g_object_unref);

  /* now clean it all up */
  g_app_info_reset_type_associations ("application/x-test");

  list = g_app_info_get_all_for_type ("application/x-test");
  g_assert (list == NULL);
  
  g_app_info_delete (info1);
  g_app_info_delete (info2);
  g_app_info_delete (info3);

  g_object_unref (info1);
  g_object_unref (info2);
}

static void
test_fallback (void)
{
  GAppInfo *info1, *info2, *app;
  GList *apps, *recomm, *fallback, *list, *l, *m;
  GError *error = NULL;
  gint old_length;

  info1 = create_app_info ("Test1");
  info2 = create_app_info ("Test2");

  g_assert (g_content_type_is_a ("text/x-python", "text/plain"));

  apps = g_app_info_get_all_for_type ("text/x-python");
  old_length = g_list_length (apps);
  g_list_free_full (apps, g_object_unref);

  g_app_info_set_as_default_for_type (info1, "text/x-python", &error);
  g_assert (error == NULL);

  g_app_info_set_as_default_for_type (info2, "text/plain", &error);
  g_assert (error == NULL);

  /* check that both apps are registered */
  apps = g_app_info_get_all_for_type ("text/x-python");
  g_assert (g_list_length (apps) == old_length + 2);

  /* check the ordering */
  app = g_list_nth_data (apps, 0);
  g_assert (g_app_info_equal (info1, app));

  /* check that Test1 is the first recommended app */
  recomm = g_app_info_get_recommended_for_type ("text/x-python");
  g_assert (recomm != NULL);
  app = g_list_nth_data (recomm, 0);
  g_assert (g_app_info_equal (info1, app));

  /* and that Test2 is the first fallback */
  fallback = g_app_info_get_fallback_for_type ("text/x-python");
  g_assert (fallback != NULL);
  app = g_list_nth_data (fallback, 0);
  g_assert (g_app_info_equal (info2, app));

  /* check that recomm + fallback = all applications */
  list = g_list_concat (g_list_copy (recomm), g_list_copy (fallback));
  g_assert (g_list_length (list) == g_list_length (apps));

  for (l = list, m = apps; l != NULL && m != NULL; l = l->next, m = m->next)
    {
      g_assert (g_app_info_equal (l->data, m->data));
    }

  g_list_free (list);

  g_list_free_full (apps, g_object_unref);
  g_list_free_full (recomm, g_object_unref);
  g_list_free_full (fallback, g_object_unref);

  g_app_info_reset_type_associations ("text/x-python");
  g_app_info_reset_type_associations ("text/plain");

  g_app_info_delete (info1);
  g_app_info_delete (info2);

  g_object_unref (info1);
  g_object_unref (info2);
}

static void
test_last_used (void)
{
  GList *applications;
  GAppInfo *info1, *info2, *default_app;
  GError *error = NULL;

  info1 = create_app_info ("Test1");
  info2 = create_app_info ("Test2");

  g_app_info_set_as_default_for_type (info1, "application/x-test", &error);
  g_assert (error == NULL);

  g_app_info_add_supports_type (info2, "application/x-test", &error);
  g_assert (error == NULL);

  applications = g_app_info_get_recommended_for_type ("application/x-test");
  g_assert (g_list_length (applications) == 2);

  /* the first should be the default app now */
  g_assert (g_app_info_equal (g_list_nth_data (applications, 0), info1));
  g_assert (g_app_info_equal (g_list_nth_data (applications, 1), info2));

  g_list_free_full (applications, g_object_unref);

  g_app_info_set_as_last_used_for_type (info2, "application/x-test", &error);
  g_assert (error == NULL);

  applications = g_app_info_get_recommended_for_type ("application/x-test");
  g_assert (g_list_length (applications) == 2);

  default_app = g_app_info_get_default_for_type ("application/x-test", FALSE);
  g_assert (g_app_info_equal (default_app, info1));

  /* the first should be the other app now */
  g_assert (g_app_info_equal (g_list_nth_data (applications, 0), info2));
  g_assert (g_app_info_equal (g_list_nth_data (applications, 1), info1));

  g_list_free_full (applications, g_object_unref);

  g_app_info_reset_type_associations ("application/x-test");

  g_app_info_delete (info1);
  g_app_info_delete (info2);

  g_object_unref (info1);
  g_object_unref (info2);
  g_object_unref (default_app);
}

static void
cleanup_dir_recurse (GFile *parent, GFile *root)
{
  gboolean res;
  GError *error;
  GFileEnumerator *enumerator;
  GFileInfo *info;
  GFile *descend;
  char *relative_path;

  g_assert (root != NULL);

  error = NULL;
  enumerator =
    g_file_enumerate_children (parent, "*",
                               G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL,
                               &error);
  if (! enumerator)
          return;
  error = NULL;
  info = g_file_enumerator_next_file (enumerator, NULL, &error);
  while ((info) && (!error))
    {
      descend = g_file_get_child (parent, g_file_info_get_name (info));
      g_assert (descend != NULL);
      relative_path = g_file_get_relative_path (root, descend);
      g_assert (relative_path != NULL);

      if (g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY)
          cleanup_dir_recurse (descend, root);

      error = NULL;
      res = g_file_delete (descend, NULL, &error);
      g_assert_cmpint (res, ==, TRUE);

      g_object_unref (descend);
      error = NULL;
      info = g_file_enumerator_next_file (enumerator, NULL, &error);
    }
  g_assert (error == NULL);

  error = NULL;
  res = g_file_enumerator_close (enumerator, NULL, &error);
  g_assert_cmpint (res, ==, TRUE);
  g_assert (error == NULL);
}

static void
cleanup_subdirs (const char *base_dir)
{
  GFile *base, *file;
 
  base = g_file_new_for_path (base_dir);  
  file = g_file_get_child (base, "applications");
  cleanup_dir_recurse (file, file);
  g_object_unref (file);
  file = g_file_get_child (base, "mime");
  cleanup_dir_recurse (file, file);
  g_object_unref (file);
}

int
main (int   argc,
      char *argv[])
{
  gint result;

  g_type_init ();
  g_test_init (&argc, &argv, NULL);
  
  basedir = g_get_current_dir ();
  g_setenv ("XDG_DATA_HOME", basedir, TRUE);
  cleanup_subdirs (basedir);
  
  g_test_add_func ("/desktop-app-info/delete", test_delete);
  g_test_add_func ("/desktop-app-info/default", test_default);
  g_test_add_func ("/desktop-app-info/fallback", test_fallback);
  g_test_add_func ("/desktop-app-info/lastused", test_last_used);

  result = g_test_run ();

  cleanup_subdirs (basedir);

  return result;
}
