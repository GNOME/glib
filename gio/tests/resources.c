/* GLib testing framework examples and tests
 *
 * Copyright (C) 2011 Red Hat, Inc.
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
 */

#include <gio/gio.h>
#include "gconstructor.h"
#include "test_resources2.h"

static void
test_resource (GResource *resource)
{
  GError *error = NULL;
  gboolean found, success;
  gsize size;
  guint32 flags;
  GBytes *data;
  char **children;
  GInputStream *in;
  char buffer[128];

  found = g_resource_get_info (resource,
			       "/not/there",
			       G_RESOURCE_LOOKUP_FLAGS_NONE,
			       &size, &flags, &error);
  g_assert (!found);
  g_assert_error (error, G_RESOURCE_ERROR, G_RESOURCE_ERROR_NOT_FOUND);
  g_clear_error (&error);

  found = g_resource_get_info (resource,
			       "/test1.txt",
			       G_RESOURCE_LOOKUP_FLAGS_NONE,
			       &size, &flags, &error);
  g_assert (found);
  g_assert_no_error (error);
  g_assert_cmpint (size, ==, 6);
  g_assert_cmpuint (flags, ==, G_RESOURCE_FLAGS_COMPRESSED);

  found = g_resource_get_info (resource,
			       "/a_prefix/test2.txt",
			       G_RESOURCE_LOOKUP_FLAGS_NONE,
			       &size, &flags, &error);
  g_assert (found);
  g_assert_no_error (error);
  g_assert_cmpint (size, ==, 6);
  g_assert_cmpuint (flags, ==, 0);

  found = g_resource_get_info (resource,
			       "/a_prefix/test2-alias.txt",
			       G_RESOURCE_LOOKUP_FLAGS_NONE,
			       &size, &flags, &error);
  g_assert (found);
  g_assert_no_error (error);
  g_assert_cmpint (size, ==, 6);
  g_assert_cmpuint (flags, ==, 0);

  data = g_resource_lookup_data (resource,
				 "/test1.txt",
				 G_RESOURCE_LOOKUP_FLAGS_NONE,
				 &error);
  g_assert_cmpstr (g_bytes_get_data (data, NULL), ==, "test1\n");
  g_assert_no_error (error);
  g_bytes_unref (data);

  in = g_resource_open_stream (resource,
			       "/test1.txt",
			       G_RESOURCE_LOOKUP_FLAGS_NONE,
			       &error);
  g_assert (in != NULL);
  g_assert_no_error (error);

  success = g_input_stream_read_all (in, buffer, sizeof (buffer) - 1,
				     &size,
				     NULL, &error);
  g_assert (success);
  g_assert_no_error (error);
  g_assert_cmpint (size, ==, 6);
  buffer[size] = 0;
  g_assert_cmpstr (buffer, ==, "test1\n");

  g_input_stream_close (in, NULL, &error);
  g_assert_no_error (error);
  g_clear_object (&in);

  data = g_resource_lookup_data (resource,
				 "/a_prefix/test2.txt",
				 G_RESOURCE_LOOKUP_FLAGS_NONE,
				 &error);
  g_assert (data != NULL);
  g_assert_no_error (error);
  size = g_bytes_get_size (data);
  g_assert_cmpint (size, ==, 6);
  g_assert_cmpstr (g_bytes_get_data (data, NULL), ==, "test2\n");
  g_bytes_unref (data);

  data = g_resource_lookup_data (resource,
				 "/a_prefix/test2-alias.txt",
				 G_RESOURCE_LOOKUP_FLAGS_NONE,
				 &error);
  g_assert (data != NULL);
  g_assert_no_error (error);
  size = g_bytes_get_size (data);
  g_assert_cmpint (size, ==, 6);
  g_assert_cmpstr (g_bytes_get_data (data, NULL), ==, "test2\n");
  g_bytes_unref (data);

  children = g_resource_enumerate_children  (resource,
					     "/not/here",
					     G_RESOURCE_LOOKUP_FLAGS_NONE,
					     &error);
  g_assert (children == NULL);
  g_assert_error (error, G_RESOURCE_ERROR, G_RESOURCE_ERROR_NOT_FOUND);
  g_clear_error (&error);

  children = g_resource_enumerate_children  (resource,
					     "/a_prefix",
					     G_RESOURCE_LOOKUP_FLAGS_NONE,
					     &error);
  g_assert (children != NULL);
  g_assert_no_error (error);
  g_assert_cmpint (g_strv_length (children), ==, 2);
  g_strfreev (children);
}

static void
test_resource_file (void)
{
  GResource *resource;
  GError *error = NULL;

  resource = g_resource_load ("not-there", &error);
  g_assert (resource == NULL);
  g_assert_error (error, G_FILE_ERROR, G_FILE_ERROR_NOENT);
  g_clear_error (&error);

  resource = g_resource_load ("test.gresource", &error);
  g_assert (resource != NULL);
  g_assert_no_error (error);

  test_resource (resource);
  g_resource_unref (resource);
}

static void
test_resource_data (void)
{
  GResource *resource;
  GError *error = NULL;
  gboolean loaded_file;
  char *content;
  gsize content_size;
  GBytes *data;

  loaded_file = g_file_get_contents ("test.gresource", &content, &content_size,
				     NULL);
  g_assert (loaded_file);

  data = g_bytes_new_take (content, content_size);
  resource = g_resource_new_from_data (data, &error);
  g_bytes_unref (data);
  g_assert (resource != NULL);
  g_assert_no_error (error);

  test_resource (resource);

  g_resource_unref (resource);
}

static void
test_resource_registred (void)
{
  GResource *resource;
  GError *error = NULL;
  gboolean found, success;
  gsize size;
  guint32 flags;
  GBytes *data;
  char **children;
  GInputStream *in;
  char buffer[128];

  resource = g_resource_load ("test.gresource", &error);
  g_assert (resource != NULL);
  g_assert_no_error (error);

  found = g_resources_get_info ("/test1.txt",
				G_RESOURCE_LOOKUP_FLAGS_NONE,
				&size, &flags, &error);
  g_assert (!found);
  g_assert_error (error, G_RESOURCE_ERROR, G_RESOURCE_ERROR_NOT_FOUND);
  g_clear_error (&error);

  g_resources_register (resource);

  found = g_resources_get_info ("/test1.txt",
				G_RESOURCE_LOOKUP_FLAGS_NONE,
				&size, &flags, &error);
  g_assert (found);
  g_assert_no_error (error);
  g_assert_cmpint (size, ==, 6);
  g_assert (flags == (G_RESOURCE_FLAGS_COMPRESSED));

  found = g_resources_get_info ("/a_prefix/test2.txt",
				G_RESOURCE_LOOKUP_FLAGS_NONE,
				&size, &flags, &error);
  g_assert (found);
  g_assert_no_error (error);
  g_assert_cmpint (size, ==, 6);
  g_assert_cmpint (flags, ==, 0);

  found = g_resources_get_info ("/a_prefix/test2-alias.txt",
				G_RESOURCE_LOOKUP_FLAGS_NONE,
				&size, &flags, &error);
  g_assert (found);
  g_assert_no_error (error);
  g_assert_cmpint (size, ==, 6);
  g_assert_cmpuint (flags, ==, 0);

  data = g_resources_lookup_data ("/test1.txt",
				  G_RESOURCE_LOOKUP_FLAGS_NONE,
				  &error);
  g_assert_cmpstr (g_bytes_get_data (data, NULL), ==, "test1\n");
  g_assert_no_error (error);
  g_bytes_unref (data);

  in = g_resources_open_stream ("/test1.txt",
				G_RESOURCE_LOOKUP_FLAGS_NONE,
				&error);
  g_assert (in != NULL);
  g_assert_no_error (error);

  success = g_input_stream_read_all (in, buffer, sizeof (buffer) - 1,
				     &size,
				     NULL, &error);
  g_assert (success);
  g_assert_no_error (error);
  g_assert_cmpint (size, ==, 6);
  buffer[size] = 0;
  g_assert_cmpstr (buffer, ==, "test1\n");

  g_input_stream_close (in, NULL, &error);
  g_assert_no_error (error);
  g_clear_object (&in);

  data = g_resources_lookup_data ("/a_prefix/test2.txt",
				  G_RESOURCE_LOOKUP_FLAGS_NONE,
				  &error);
  g_assert (data != NULL);
  g_assert_no_error (error);
  size = g_bytes_get_size (data);
  g_assert_cmpint (size, ==, 6);
  g_assert_cmpstr (g_bytes_get_data (data, NULL), ==, "test2\n");
  g_bytes_unref (data);

  data = g_resources_lookup_data ("/a_prefix/test2-alias.txt",
				  G_RESOURCE_LOOKUP_FLAGS_NONE,
				  &error);
  g_assert (data != NULL);
  g_assert_no_error (error);
  size = g_bytes_get_size (data);
  g_assert_cmpint (size, ==, 6);
  g_assert_cmpstr (g_bytes_get_data (data, NULL), ==, "test2\n");
  g_bytes_unref (data);

  children = g_resources_enumerate_children ("/not/here",
					     G_RESOURCE_LOOKUP_FLAGS_NONE,
					     &error);
  g_assert (children == NULL);
  g_assert_error (error, G_RESOURCE_ERROR, G_RESOURCE_ERROR_NOT_FOUND);
  g_clear_error (&error);

  children = g_resources_enumerate_children ("/a_prefix",
					     G_RESOURCE_LOOKUP_FLAGS_NONE,
					     &error);
  g_assert (children != NULL);
  g_assert_no_error (error);
  g_assert_cmpint (g_strv_length (children), ==, 2);
  g_strfreev (children);

  g_resources_unregister (resource);
  g_resource_unref (resource);

  found = g_resources_get_info ("/test1.txt",
				G_RESOURCE_LOOKUP_FLAGS_NONE,
				&size, &flags, &error);
  g_assert (!found);
  g_assert_error (error, G_RESOURCE_ERROR, G_RESOURCE_ERROR_NOT_FOUND);
  g_clear_error (&error);
}

static void
test_resource_automatic (void)
{
  GError *error = NULL;
  gboolean found;
  gsize size;
  guint32 flags;
  GBytes *data;

  found = g_resources_get_info ("/auto_loaded/test1.txt",
				G_RESOURCE_LOOKUP_FLAGS_NONE,
				&size, &flags, &error);
  g_assert (found);
  g_assert_no_error (error);
  g_assert_cmpint (size, ==, 6);
  g_assert_cmpint (flags, ==, 0);

  data = g_resources_lookup_data ("/auto_loaded/test1.txt",
				  G_RESOURCE_LOOKUP_FLAGS_NONE,
				  &error);
  g_assert (data != NULL);
  g_assert_no_error (error);
  size = g_bytes_get_size (data);
  g_assert_cmpint (size, ==, 6);
  g_assert_cmpstr (g_bytes_get_data (data, NULL), ==, "test1\n");
  g_bytes_unref (data);
}

static void
test_resource_manual (void)
{
  GError *error = NULL;
  gboolean found;
  gsize size;
  guint32 flags;
  GBytes *data;

  found = g_resources_get_info ("/manual_loaded/test1.txt",
				G_RESOURCE_LOOKUP_FLAGS_NONE,
				&size, &flags, &error);
  g_assert (found);
  g_assert_no_error (error);
  g_assert_cmpint (size, ==, 6);
  g_assert_cmpuint (flags, ==, 0);

  data = g_resources_lookup_data ("/manual_loaded/test1.txt",
				  G_RESOURCE_LOOKUP_FLAGS_NONE,
				  &error);
  g_assert (data != NULL);
  g_assert_no_error (error);
  size = g_bytes_get_size (data);
  g_assert_cmpint (size, ==, 6);
  g_assert_cmpstr (g_bytes_get_data (data, NULL), ==, "test1\n");
  g_bytes_unref (data);
}

static void
test_resource_module (void)
{
  GIOModule *module;
  gboolean found;
  gsize size;
  guint32 flags;
  GBytes *data;
  GError *error;

  if (g_module_supported ())
    {
      char *dir, *path;

      dir = g_get_current_dir ();

      path = g_strconcat (dir, G_DIR_SEPARATOR_S "libresourceplugin",  NULL);
      module = g_io_module_new (path);
      g_free (path);
      g_free (dir);

      error = NULL;

      found = g_resources_get_info ("/resourceplugin/test1.txt",
				    G_RESOURCE_LOOKUP_FLAGS_NONE,
				    &size, &flags, &error);
      g_assert (!found);
      g_assert_error (error, G_RESOURCE_ERROR, G_RESOURCE_ERROR_NOT_FOUND);
      g_clear_error (&error);

      g_type_module_use (G_TYPE_MODULE (module));

      found = g_resources_get_info ("/resourceplugin/test1.txt",
				    G_RESOURCE_LOOKUP_FLAGS_NONE,
				    &size, &flags, &error);
      g_assert (found);
      g_assert_no_error (error);
      g_assert_cmpint (size, ==, 6);
      g_assert_cmpuint (flags, ==, 0);

      data = g_resources_lookup_data ("/resourceplugin/test1.txt",
				      G_RESOURCE_LOOKUP_FLAGS_NONE,
				      &error);
      g_assert (data != NULL);
      g_assert_no_error (error);
      size = g_bytes_get_size (data);
      g_assert_cmpint (size, ==, 6);
      g_assert_cmpstr (g_bytes_get_data (data, NULL), ==, "test1\n");
      g_bytes_unref (data);

      g_type_module_unuse (G_TYPE_MODULE (module));

      found = g_resources_get_info ("/resourceplugin/test1.txt",
				    G_RESOURCE_LOOKUP_FLAGS_NONE,
				    &size, &flags, &error);
      g_assert (!found);
      g_assert_error (error, G_RESOURCE_ERROR, G_RESOURCE_ERROR_NOT_FOUND);
      g_clear_error (&error);
    }
}

static void
test_uri_query_info (void)
{
  GResource *resource;
  GError *error = NULL;
  gboolean loaded_file;
  char *content;
  gsize content_size;
  GBytes *data;
  GFile *file;
  GFileInfo *info;
  const char *content_type;

  loaded_file = g_file_get_contents ("test.gresource", &content, &content_size,
                                     NULL);
  g_assert (loaded_file);

  data = g_bytes_new_take (content, content_size);
  resource = g_resource_new_from_data (data, &error);
  g_bytes_unref (data);
  g_assert (resource != NULL);
  g_assert_no_error (error);

  g_resources_register (resource);

  file = g_file_new_for_uri ("resource://" "/a_prefix/test2-alias.txt");

  info = g_file_query_info (file, "*", 0, NULL, &error);
  g_assert_no_error (error);
  g_object_unref (file);

  content_type = g_file_info_get_content_type (info);
  g_assert (content_type);
  g_assert_cmpstr (content_type, ==, "text/plain");

  g_object_unref (info);

  g_resources_unregister (resource);
  g_resource_unref (resource);
}

int
main (int   argc,
      char *argv[])
{
  g_type_init ();
  g_test_init (&argc, &argv, NULL);

  _g_test2_register_resource ();

  g_test_add_func ("/resource/file", test_resource_file);
  g_test_add_func ("/resource/data", test_resource_data);
  g_test_add_func ("/resource/registred", test_resource_registred);
  g_test_add_func ("/resource/manual", test_resource_manual);
#ifdef G_HAS_CONSTRUCTORS
  g_test_add_func ("/resource/automatic", test_resource_automatic);
  /* This only uses automatic resources too, so it tests the constructors and destructors */
  g_test_add_func ("/resource/module", test_resource_module);
#endif
  g_test_add_func ("/resource/uri/query-info", test_uri_query_info);

  return g_test_run();
}
