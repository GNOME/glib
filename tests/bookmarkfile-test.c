#undef G_DISABLE_ASSERT

#include <glib.h>
#include <time.h>
#include <locale.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define TEST_URI_0 	"file:///abc/defgh/ijklmnopqrstuvwxyz"
#define TEST_URI_1 	"file:///test/uri/1"
#define TEST_URI_2 	"file:///test/uri/2"

#define TEST_MIME 	"text/plain"

#define TEST_APP_NAME 	"bookmarkfile-test"
#define TEST_APP_EXEC 	"bookmarkfile-test %f"

static gboolean
test_load (GBookmarkFile *bookmark,
           const gchar   *filename)
{
  GError *error = NULL;
  gboolean res;
  
  res = g_bookmark_file_load_from_file (bookmark, filename, &error);
  if (error)
    {
      g_print ("Load error: %s\n", error->message);
      g_error_free (error);
    }

  return res;
}

static gboolean
test_query (GBookmarkFile *bookmark)
{
  gint size;
  gchar **uris;
  gsize uris_len, i;
  gboolean res = TRUE;

  size = g_bookmark_file_get_size (bookmark);
  uris = g_bookmark_file_get_uris (bookmark, &uris_len);
 
  if (uris_len != size)
    {
      g_print ("URI/size mismatch: URI count is %d (should be %d)\n", uris_len, size);

      res = FALSE;
    }

  for (i = 0; i < uris_len; i++)
    if (!g_bookmark_file_has_item (bookmark, uris[i]))
      {
        g_print ("URI/bookmark mismatch: bookmark for '%s' does not exist\n", uris[i]);

	res = FALSE;
      }

  g_strfreev (uris);
  
  return res;
}

static gboolean
test_modify (GBookmarkFile *bookmark)
{
  gchar *text;
  guint count;
  time_t stamp;
  GError *error = NULL;
  
  g_print ("\t=> check global title/description...");
  g_bookmark_file_set_title (bookmark, NULL, "a file");
  g_bookmark_file_set_description (bookmark, NULL, "a bookmark file");
  
  text = g_bookmark_file_get_title (bookmark, NULL, &error);
  g_assert_no_error (error);
  g_assert_cmpstr (text, ==, "a file");
  g_free (text);

  text = g_bookmark_file_get_description (bookmark, NULL, &error);
  g_assert_no_error (error);
  g_assert_cmpstr (text, ==, "a bookmark file");
  g_free (text);
  g_print ("ok\n");

  g_print ("\t=> check bookmark title/description...");
  g_bookmark_file_set_title (bookmark, TEST_URI_0, "a title");
  g_bookmark_file_set_description (bookmark, TEST_URI_0, "a description");

  text = g_bookmark_file_get_title (bookmark, TEST_URI_0, &error);
  g_assert_no_error (error);
  g_assert_cmpstr (text, ==, "a title");
  g_free (text);
  g_print ("ok\n");

  g_print ("\t=> check non existing bookmark...");
  g_bookmark_file_get_description (bookmark, TEST_URI_1, &error);
  g_assert_error (error, G_BOOKMARK_FILE_ERROR, G_BOOKMARK_FILE_ERROR_URI_NOT_FOUND);
  g_clear_error (&error);
  g_print ("ok\n");
  
  g_print ("\t=> check application...");
  g_bookmark_file_set_mime_type (bookmark, TEST_URI_0, TEST_MIME);
  g_bookmark_file_add_application (bookmark, TEST_URI_0,
				   TEST_APP_NAME,
				   TEST_APP_EXEC);
  g_assert (g_bookmark_file_has_application (bookmark, TEST_URI_0, TEST_APP_NAME, NULL) == TRUE);
  g_bookmark_file_get_app_info (bookmark, TEST_URI_0, TEST_APP_NAME,
		  		&text,
				&count,
				&stamp,
				&error);
  g_assert_no_error (error);
  g_assert (count == 1);
  g_assert (stamp == g_bookmark_file_get_modified (bookmark, TEST_URI_0, NULL));
  g_free (text);
  
  g_bookmark_file_get_app_info (bookmark, TEST_URI_0, "fail",
		  		&text,
				&count,
				&stamp,
				&error);
  g_assert_error (error, G_BOOKMARK_FILE_ERROR, G_BOOKMARK_FILE_ERROR_APP_NOT_REGISTERED);
  g_clear_error (&error);
  g_print ("ok\n"); 

  g_print ("\t=> check groups...");
  g_bookmark_file_add_group (bookmark, TEST_URI_1, "Test");
  g_assert (g_bookmark_file_has_group (bookmark, TEST_URI_1, "Test", NULL) == TRUE);
  g_assert (g_bookmark_file_has_group (bookmark, TEST_URI_1, "Fail", NULL) == FALSE);
  g_print ("ok\n");

  g_print ("\t=> check remove...");
  g_assert (g_bookmark_file_remove_item (bookmark, TEST_URI_1, &error) == TRUE);
  g_assert_no_error (error);
  g_assert (g_bookmark_file_remove_item (bookmark, TEST_URI_1, &error) == FALSE);
  g_assert_error (error, G_BOOKMARK_FILE_ERROR, G_BOOKMARK_FILE_ERROR_URI_NOT_FOUND);
  g_clear_error (&error);
  g_print ("ok\n");
  
  return TRUE;
}

static gint
test_file (const gchar *filename)
{
  GBookmarkFile *bookmark_file;
  gboolean success;

  g_return_val_if_fail (filename != NULL, 1);

  g_print ("checking GBookmarkFile...\n");

  bookmark_file = g_bookmark_file_new ();
  g_assert (bookmark_file != NULL);

  success = test_load (bookmark_file, filename);
  
  if (success)
    {
      success = test_query (bookmark_file);
      success = test_modify (bookmark_file);
    }

  g_bookmark_file_free (bookmark_file);

  g_print ("ok\n");

  return (success == TRUE ? 0 : 1);
}

int
main (int   argc,
      char *argv[])
{
  if (argc > 1)
    return test_file (argv[1]);
  else
    {
      fprintf (stderr, "Usage: bookmarkfile-test <bookmarkfile>\n");

      return 1;
    }
}
