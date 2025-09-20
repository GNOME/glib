/* Unit tests for gdocumentportal
 * GIO - GLib Input, Output and Streaming Library
 *
 * Copyright (C) 2025 Marco Trevisan
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Marco Trevisan <marco.trevisan@canonical.com>
 */

#include <gio/gio.h>

#include "gdbus-sessionbus.h"
#include "fake-document-portal.h"
#include "gdocumentportal.h"

static void
test_document_portal_add_uri (void)
{
  GFakeDocumentPortalThread *thread = NULL;
  GFile *file;
  GList *uris = NULL;  /* (element-type utf8) */
  GList *portal_uris = NULL;  /* (element-type utf8) */
  GFileIOStream *iostream = NULL;
  GError *error = NULL;
  const char *app_id;
  char *basename;
  char *expected_name;

  /* Run a fake-document-portal */
  app_id = "org.gnome.glib.gio";
  thread = g_fake_document_portal_thread_new (session_bus_get_address (), app_id);
  g_fake_document_portal_thread_run (thread);

  file = g_file_new_tmp ("test_document_portal_add_uri_XXXXXX",
                         &iostream, NULL);
  g_assert_no_error (error);
  g_io_stream_close ((GIOStream *) iostream, NULL, &error);
  g_assert_no_error (error);
  g_object_unref (iostream);

  uris = g_list_append (uris, g_file_get_uri (file));
  portal_uris = g_document_portal_add_documents (uris, app_id, &error);
  g_assert_no_error (error);

  basename = g_file_get_basename (file);
  expected_name = g_strdup_printf ("file:/document-portal/document-id-0/%s", basename);

  g_assert_cmpuint (g_list_length (portal_uris), ==, 1);
  g_assert_cmpstr (g_list_nth_data (portal_uris, 0), ==, expected_name);

  g_fake_document_portal_thread_stop (thread);
  g_clear_object (&thread);
  g_clear_object (&file);
  g_clear_pointer (&expected_name, g_free);
  g_clear_pointer (&basename, g_free);
  g_clear_list (&uris, g_free);
  g_clear_list (&portal_uris, g_free);
}

static void
test_document_portal_add_not_existent_uri (void)
{
  GFakeDocumentPortalThread *thread = NULL;
  GList *uris = NULL;  /* (element-type const char*) */
  GList *portal_uris = NULL;  /* (element-type utf8) */
  GError *error = NULL;
  const char *app_id;
  const char *uri;

  /* Run a fake-document-portal */
  app_id = "org.gnome.glib.gio.not-existent-uri";
  thread = g_fake_document_portal_thread_new (session_bus_get_address (), app_id);
  g_fake_document_portal_thread_run (thread);

  uri = "file:/no-existent-path-really!";
  uris = g_list_append (uris, (char *) uri);
  portal_uris = g_document_portal_add_documents (uris, app_id, &error);
  g_assert_no_error (error);

  g_assert_cmpuint (g_list_length (portal_uris), ==, 1);
  g_assert_cmpstr (g_list_nth_data (portal_uris, 0), ==, uri);

  g_fake_document_portal_thread_stop (thread);
  g_clear_object (&thread);
  g_clear_list (&uris, NULL);
  g_clear_list (&portal_uris, g_free);
}

static void
test_document_portal_add_existent_and_not_existent_uris (void)
{
  GFakeDocumentPortalThread *thread = NULL;
  GFile *file;
  GList *uris = NULL;  /* (element-type utf8) */
  GList *portal_uris = NULL;  /* (element-type utf8) */
  GFileIOStream *iostream = NULL;
  GError *error = NULL;
  const char *app_id;
  const char *invalid_uri;
  char *basename;
  char *expected_name0;
  char *expected_name1;

  /* Run a fake-document-portal */
  app_id = "org.gnome.glib.gio.mixed-uris";
  thread = g_fake_document_portal_thread_new (session_bus_get_address (), app_id);
  g_fake_document_portal_thread_run (thread);

  file = g_file_new_tmp ("test_document_portal_add_existent_and_not_existent_uris_XXXXXX",
                         &iostream, NULL);
  g_assert_no_error (error);
  g_io_stream_close ((GIOStream *) iostream, NULL, &error);
  g_assert_no_error (error);
  g_object_unref (iostream);

  invalid_uri = "file:/no-existent-path-really!";

  uris = g_list_append (uris, g_file_get_uri (file));
  uris = g_list_append (uris, g_strdup (invalid_uri));
  uris = g_list_append (uris, g_file_get_uri (file));
  uris = g_list_append (uris, g_strdup (invalid_uri));

  portal_uris = g_document_portal_add_documents (uris, app_id, &error);
  g_assert_no_error (error);

  basename = g_file_get_basename (file);
  expected_name0 = g_strdup_printf ("file:/document-portal/document-id-0/%s", basename);
  expected_name1 = g_strdup_printf ("file:/document-portal/document-id-1/%s", basename);

  g_assert_cmpuint (g_list_length (portal_uris), ==, 4);
  g_assert_cmpstr (g_list_nth_data (portal_uris, 0), ==, expected_name0);
  g_assert_cmpstr (g_list_nth_data (portal_uris, 1), ==, invalid_uri);
  g_assert_cmpstr (g_list_nth_data (portal_uris, 2), ==, expected_name1);
  g_assert_cmpstr (g_list_nth_data (portal_uris, 3), ==, invalid_uri);

  g_fake_document_portal_thread_stop (thread);
  g_clear_object (&thread);
  g_clear_object (&file);
  g_clear_pointer (&expected_name1, g_free);
  g_clear_pointer (&expected_name0, g_free);
  g_clear_pointer (&basename, g_free);
  g_clear_list (&uris, g_free);
  g_clear_list (&portal_uris, g_free);
}

static void
test_document_portal_add_symlink_uri (void)
{
  GFakeDocumentPortalThread *thread = NULL;
  GFile *target;
  GFile *link1;
  GFile *link2;
  GFile *parent_dir;
  GList *uris = NULL;  /* (element-type utf8) */
  GList *portal_uris = NULL;  /* (element-type utf8) */
  GFileIOStream *iostream = NULL;
  GError *error = NULL;
  const char *app_id;
  char *tmpdir_path;
  char *basename;
  char *expected_name;

  /* Run a fake-document-portal */
  app_id = "org.gnome.glib.gio.symlinks";
  thread = g_fake_document_portal_thread_new (session_bus_get_address (), app_id);
  g_fake_document_portal_thread_run (thread);

  target = g_file_new_tmp ("test_document_portal_add_symlink_uri_XXXXXX",
                           &iostream, NULL);
  g_assert_no_error (error);
  g_io_stream_close ((GIOStream *) iostream, NULL, &error);
  g_assert_no_error (error);
  g_object_unref (iostream);

  tmpdir_path = g_dir_make_tmp ("g_file_symlink_XXXXXX", &error);
  g_assert_no_error (error);

  parent_dir = g_file_new_for_path (tmpdir_path);
  g_assert_true (g_file_query_exists (parent_dir, NULL));

  link1 = g_file_get_child (parent_dir, "symlink");
  g_assert_false (g_file_query_exists (link1, NULL));

  g_file_make_symbolic_link (link1, g_file_peek_path (target), NULL, &error);
  g_assert_no_error (error);
  g_assert_true (g_file_query_exists (link1, NULL));

  link2 = g_file_get_child (parent_dir, "symlink-of-symlink");
  g_assert_false (g_file_query_exists (link2, NULL));

  basename = g_file_get_basename (link1);
  g_file_make_symbolic_link (link2, basename, NULL, &error);
  g_clear_pointer (&basename, g_free);
  g_assert_true (g_file_query_exists (link2, NULL));
  g_assert_no_error (error);

  uris = g_list_append (uris, g_file_get_uri (link1));
  uris = g_list_append (uris, g_file_get_uri (link2));

  portal_uris = g_document_portal_add_documents (uris, app_id, &error);
  g_assert_no_error (error);

  basename = g_file_get_basename (target);
  g_assert_cmpuint (g_list_length (portal_uris), ==, 2);

  expected_name = g_strdup_printf ("file:/document-portal/document-id-0/%s", basename);
  g_assert_cmpstr (g_list_nth_data (portal_uris, 0), ==, expected_name);
  g_clear_pointer (&expected_name, g_free);

  expected_name = g_strdup_printf ("file:/document-portal/document-id-1/%s", basename);
  g_assert_cmpstr (g_list_nth_data (portal_uris, 1), ==, expected_name);
  g_clear_pointer (&expected_name, g_free);

  g_fake_document_portal_thread_stop (thread);
  g_clear_object (&thread);
  g_clear_object (&target);
  g_clear_object (&link1);
  g_clear_object (&link2);
  g_clear_object (&parent_dir);
  g_clear_pointer (&expected_name, g_free);
  g_clear_pointer (&basename, g_free);
  g_clear_pointer (&tmpdir_path, g_free);
  g_clear_list (&uris, g_free);
  g_clear_list (&portal_uris, g_free);
}

int
main (int   argc,
      char *argv[])
{
  g_setenv ("LC_ALL", "C", TRUE);
  g_test_init (&argc, &argv, G_TEST_OPTION_ISOLATE_DIRS, NULL);

  g_test_add_func ("/document-portal/add-uri", test_document_portal_add_uri);
  g_test_add_func ("/document-portal/add-not-existent-uri", test_document_portal_add_not_existent_uri);
  g_test_add_func ("/document-portal/add-existent-and-not-existent-uri", test_document_portal_add_existent_and_not_existent_uris);
  g_test_add_func ("/document-portal/add-symlink-uri", test_document_portal_add_symlink_uri);

  return session_bus_run ();
}
