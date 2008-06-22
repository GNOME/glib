/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
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
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GLib Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GLib Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GLib at ftp://ftp.gtk.org/pub/gtk/. 
 */

#undef G_DISABLE_ASSERT
#undef G_LOG_DOMAIN

#include "config.h"

#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

typedef struct
{
  char *filename;
  char *hostname;
  char *expected_result;
  GConvertError expected_error; /* If failed */
}  ToUriTest;

ToUriTest
to_uri_tests[] = {
  { "/etc", NULL, "file:///etc"},
  { "/etc", "", "file:///etc"},
  { "/etc", "otherhost", "file://otherhost/etc"},
#ifdef G_OS_WIN32
  { "/etc", "localhost", "file:///etc"},
  { "c:\\windows", NULL, "file:///c:/windows"},
  { "c:\\windows", "localhost", "file:///c:/windows"},
  { "c:\\windows", "otherhost", "file://otherhost/c:/windows"},
  { "\\\\server\\share\\dir", NULL, "file:////server/share/dir"},
  { "\\\\server\\share\\dir", "localhost", "file:////server/share/dir"},
#else
  { "/etc", "localhost", "file://localhost/etc"},
  { "c:\\windows", NULL, NULL, G_CONVERT_ERROR_NOT_ABSOLUTE_PATH}, /* it's important to get this error on Unix */
  { "c:\\windows", "localhost", NULL, G_CONVERT_ERROR_NOT_ABSOLUTE_PATH},
  { "c:\\windows", "otherhost", NULL, G_CONVERT_ERROR_NOT_ABSOLUTE_PATH},
#endif
  { "etc", "localhost", NULL, G_CONVERT_ERROR_NOT_ABSOLUTE_PATH},
#ifndef G_PLATFORM_WIN32
  { "/etc/\xE5\xE4\xF6", NULL, "file:///etc/%E5%E4%F6" },
  { "/etc/\xC3\xB6\xC3\xA4\xC3\xA5", NULL, "file:///etc/%C3%B6%C3%A4%C3%A5"},
#endif
  { "/etc", "\xC3\xB6\xC3\xA4\xC3\xA5", NULL, G_CONVERT_ERROR_ILLEGAL_SEQUENCE},
  { "/etc", "\xE5\xE4\xF6", NULL, G_CONVERT_ERROR_ILLEGAL_SEQUENCE},
  { "/etc/file with #%", NULL, "file:///etc/file%20with%20%23%25"},
  { "", NULL, NULL, G_CONVERT_ERROR_NOT_ABSOLUTE_PATH},
  { "", "", NULL, G_CONVERT_ERROR_NOT_ABSOLUTE_PATH},
  { "", "localhost", NULL, G_CONVERT_ERROR_NOT_ABSOLUTE_PATH},
  { "", "otherhost", NULL, G_CONVERT_ERROR_NOT_ABSOLUTE_PATH},
  { "/0123456789", NULL, "file:///0123456789"},
  { "/ABCDEFGHIJKLMNOPQRSTUVWXYZ", NULL, "file:///ABCDEFGHIJKLMNOPQRSTUVWXYZ"},
  { "/abcdefghijklmnopqrstuvwxyz", NULL, "file:///abcdefghijklmnopqrstuvwxyz"},
  { "/-_.!~*'()", NULL, "file:///-_.!~*'()"},
#ifdef G_OS_WIN32
  /* As '\\' is a path separator on Win32, it gets turned into '/' in the URI */
  { "/\"#%<>[\\]^`{|}\x7F", NULL, "file:///%22%23%25%3C%3E%5B/%5D%5E%60%7B%7C%7D%7F"},
#else
  /* On Unix, '\\' is a normal character in the file name */
  { "/\"#%<>[\\]^`{|}\x7F", NULL, "file:///%22%23%25%3C%3E%5B%5C%5D%5E%60%7B%7C%7D%7F"},
#endif
  { "/;@+$,", NULL, "file:///%3B@+$,"},
  /* This and some of the following are of course as such illegal file names on Windows,
   * and would not occur in real life.
   */
  { "/:", NULL, "file:///:"},
  { "/?&=", NULL, "file:///%3F&="}, 
  { "/", "0123456789-", NULL, G_CONVERT_ERROR_ILLEGAL_SEQUENCE},
  { "/", "ABCDEFGHIJKLMNOPQRSTUVWXYZ", "file://ABCDEFGHIJKLMNOPQRSTUVWXYZ/"},
  { "/", "abcdefghijklmnopqrstuvwxyz", "file://abcdefghijklmnopqrstuvwxyz/"},
  { "/", "_.!~*'()", NULL, G_CONVERT_ERROR_ILLEGAL_SEQUENCE},
  { "/", "\"#%<>[\\]^`{|}\x7F", NULL, G_CONVERT_ERROR_ILLEGAL_SEQUENCE},
  { "/", ";?&=+$,", NULL, G_CONVERT_ERROR_ILLEGAL_SEQUENCE},
  { "/", "/", NULL, G_CONVERT_ERROR_ILLEGAL_SEQUENCE},
  { "/", "@:", NULL, G_CONVERT_ERROR_ILLEGAL_SEQUENCE},
  { "/", "\x80\xFF", NULL, G_CONVERT_ERROR_ILLEGAL_SEQUENCE},
  { "/", "\xC3\x80\xC3\xBF", NULL, G_CONVERT_ERROR_ILLEGAL_SEQUENCE},
};


typedef struct
{
  char *uri;
  char *expected_filename;
  char *expected_hostname;
  GConvertError expected_error; /* If failed */
}  FromUriTest;

FromUriTest
from_uri_tests[] = {
  { "file:///etc", "/etc"},
  { "file:/etc", "/etc"},
#ifdef G_OS_WIN32
  /* On Win32 we don't return "localhost" hostames, just in case
   * it isn't recognized anyway.
   */
  { "file://localhost/etc", "/etc", NULL},
  { "file://localhost/etc/%23%25%20file", "/etc/#% file", NULL},
  { "file://localhost/\xE5\xE4\xF6", "/\xe5\xe4\xf6", NULL},
  { "file://localhost/%E5%E4%F6", "/\xe5\xe4\xf6", NULL},
#else
  { "file://localhost/etc", "/etc", "localhost"},
  { "file://localhost/etc/%23%25%20file", "/etc/#% file", "localhost"},
  { "file://localhost/\xE5\xE4\xF6", "/\xe5\xe4\xf6", "localhost"},
  { "file://localhost/%E5%E4%F6", "/\xe5\xe4\xf6", "localhost"},
#endif
  { "file://otherhost/etc", "/etc", "otherhost"},
  { "file://otherhost/etc/%23%25%20file", "/etc/#% file", "otherhost"},
  { "file://%C3%B6%C3%A4%C3%A5/etc", NULL, NULL, G_CONVERT_ERROR_BAD_URI},
  { "file:////etc/%C3%B6%C3%C3%C3%A5", "//etc/\xc3\xb6\xc3\xc3\xc3\xa5", NULL},
  { "file://\xE5\xE4\xF6/etc", NULL, NULL, G_CONVERT_ERROR_BAD_URI},
  { "file://%E5%E4%F6/etc", NULL, NULL, G_CONVERT_ERROR_BAD_URI},
  { "file:///some/file#bad", NULL, NULL, G_CONVERT_ERROR_BAD_URI},
  { "file://some", NULL, NULL, G_CONVERT_ERROR_BAD_URI},
  { "", NULL, NULL, G_CONVERT_ERROR_BAD_URI},
  { "file:test", NULL, NULL, G_CONVERT_ERROR_BAD_URI},
  { "http://www.yahoo.com/", NULL, NULL, G_CONVERT_ERROR_BAD_URI},
  { "file:////etc", "//etc"},
  { "file://///etc", "///etc"},
#ifdef G_OS_WIN32
  /* URIs with backslashes come from some nonstandard application, but accept them anyhow */
  { "file:///c:\\foo", "c:\\foo"},
  { "file:///c:/foo\\bar", "c:\\foo\\bar"},
  /* Accept also the old Netscape drive-letter-and-vertical bar convention */
  { "file:///c|/foo", "c:\\foo"},
  { "file:////server/share/dir", "\\\\server\\share\\dir"},
  { "file://localhost//server/share/foo", "\\\\server\\share\\foo"},
  { "file://otherhost//server/share/foo", "\\\\server\\share\\foo", "otherhost"},
#else
  { "file:///c:\\foo", "/c:\\foo"},
  { "file:///c:/foo", "/c:/foo"},
  { "file:////c:/foo", "//c:/foo"},
#endif
  { "file://0123456789/", NULL, NULL, G_CONVERT_ERROR_BAD_URI},
  { "file://ABCDEFGHIJKLMNOPQRSTUVWXYZ/", "/", "ABCDEFGHIJKLMNOPQRSTUVWXYZ"},
  { "file://abcdefghijklmnopqrstuvwxyz/", "/", "abcdefghijklmnopqrstuvwxyz"},
  { "file://-_.!~*'()/", NULL, NULL, G_CONVERT_ERROR_BAD_URI},
  { "file://\"<>[\\]^`{|}\x7F/", NULL, NULL, G_CONVERT_ERROR_BAD_URI},
  { "file://;?&=+$,/", NULL, NULL, G_CONVERT_ERROR_BAD_URI},
  { "file://%C3%80%C3%BF/", NULL, NULL, G_CONVERT_ERROR_BAD_URI},
  { "file://@/", NULL, NULL, G_CONVERT_ERROR_BAD_URI},
  { "file://:/", NULL, NULL, G_CONVERT_ERROR_BAD_URI},
  { "file://#/", NULL, NULL, G_CONVERT_ERROR_BAD_URI},
  { "file://%23/", NULL, NULL, G_CONVERT_ERROR_BAD_URI},
  { "file://%2F/", NULL, NULL, G_CONVERT_ERROR_BAD_URI},
};


static gboolean any_failed = FALSE;

static void
run_to_uri_tests (void)
{
  int i;
  gchar *res;
  GError *error;
  
  for (i = 0; i < G_N_ELEMENTS (to_uri_tests); i++)
    {
      error = NULL;
      res = g_filename_to_uri (to_uri_tests[i].filename,
			       to_uri_tests[i].hostname,
			       &error);

      if (to_uri_tests[i].expected_result == NULL)
	{
	  if (res != NULL)
	    {
	      g_print ("\ng_filename_to_uri() test %d failed, expected to return NULL, actual result: %s\n", i, res);
	      any_failed = TRUE;
	    }
	  else
	    {
	      if (error == NULL)
		{
		  g_print ("\ng_filename_to_uri() test %d failed, returned NULL, but didn't set error\n", i);
		  any_failed = TRUE;
		}
	      else if (error->domain != G_CONVERT_ERROR)
		{
		  g_print ("\ng_filename_to_uri() test %d failed, returned NULL, set non G_CONVERT_ERROR error\n", i);
		  any_failed = TRUE;
		}
	      else if (error->code != to_uri_tests[i].expected_error)
		{
		  g_print ("\ng_filename_to_uri() test %d failed as expected, but set wrong errorcode %d instead of expected %d \n",
			   i, error->code, to_uri_tests[i].expected_error);
		  any_failed = TRUE;
		}
	    }
	}
      else if (res == NULL || strcmp (res, to_uri_tests[i].expected_result) != 0)
	{
	  g_print ("\ng_filename_to_uri() test %d failed, expected result: %s, actual result: %s\n",
		   i, to_uri_tests[i].expected_result, (res) ? res : "NULL");
	  if (error)
	    g_print ("Error message: %s\n", error->message);
	  any_failed = TRUE;
	}
      g_free (res);
    }
}

static void
run_from_uri_tests (void)
{
  int i;
  gchar *res;
  gchar *hostname;
  GError *error;
  
  for (i = 0; i < G_N_ELEMENTS (from_uri_tests); i++)
    {
      error = NULL;
      res = g_filename_from_uri (from_uri_tests[i].uri,
				 &hostname,
				 &error);

      if (from_uri_tests[i].expected_filename == NULL)
	{
	  if (res != NULL)
	    {
	      g_print ("\ng_filename_from_uri() test %d failed, expected to return NULL, actual result: %s\n", i, res);
	      any_failed = TRUE;
	    }
	  else
	    {
	      if (error == NULL)
		{
		  g_print ("\ng_filename_from_uri() test %d failed, returned NULL, but didn't set error\n", i);
		  any_failed = TRUE;
		}
	      else if (error->domain != G_CONVERT_ERROR)
		{
		  g_print ("\ng_filename_from_uri() test %d failed, returned NULL, set non G_CONVERT_ERROR error\n", i);
		  any_failed = TRUE;
		}
	      else if (error->code != from_uri_tests[i].expected_error)
		{
		  g_print ("\ng_filename_from_uri() test %d failed as expected, but set wrong errorcode %d instead of expected %d \n",
			   i, error->code, from_uri_tests[i].expected_error);
		  any_failed = TRUE;
		}
	    }
	}
      else
	{
#ifdef G_OS_WIN32
	  gchar *slash, *p;

	  p = from_uri_tests[i].expected_filename = g_strdup (from_uri_tests[i].expected_filename);
	  while ((slash = strchr (p, '/')) != NULL)
	    {
	      *slash = '\\';
	      p = slash + 1;
	    }
#endif
	  if (res == NULL || strcmp (res, from_uri_tests[i].expected_filename) != 0)
	    {
	      g_print ("\ng_filename_from_uri() test %d failed, expected result: %s, actual result: %s\n",
		       i, from_uri_tests[i].expected_filename, (res) ? res : "NULL");
	      any_failed = TRUE;
	    }

	  if (from_uri_tests[i].expected_hostname == NULL)
	    {
	      if (hostname != NULL)
		{
		  g_print ("\ng_filename_from_uri() test %d failed, expected no hostname, got: %s\n",
			   i, hostname);
		  any_failed = TRUE;
		}
	    }
	  else if (hostname == NULL ||
		   strcmp (hostname, from_uri_tests[i].expected_hostname) != 0)
	    {
	      g_print ("\ng_filename_from_uri() test %d failed, expected hostname: %s, actual result: %s\n",
		       i, from_uri_tests[i].expected_hostname, (hostname) ? hostname : "NULL");
	      any_failed = TRUE;
	    }
	}
    }
}

static gint
safe_strcmp (const gchar *a, const gchar *b)
{
  return strcmp (a ? a : "", b ? b : "");
}

static gint
safe_strcmp_filename (const gchar *a, const gchar *b)
{
#ifndef G_OS_WIN32
  return safe_strcmp (a, b);
#else
  if (!a || !b)
    return safe_strcmp (a, b);
  else
    {
      while (*a && *b)
	{
	  if ((G_IS_DIR_SEPARATOR (*a) && G_IS_DIR_SEPARATOR (*b)) ||
	      *a == *b)
	    a++, b++;
	  else
	    return (*a - *b);
	}
      return (*a - *b);
    }
#endif
}

static gint
safe_strcmp_hostname (const gchar *a, const gchar *b)
{
#ifndef G_OS_WIN32
  return safe_strcmp (a, b);
#else
  if (safe_strcmp (a, "localhost") == 0 && b == NULL)
    return 0;
  else
    return safe_strcmp (a, b);
#endif
}

static void
run_roundtrip_tests (void)
{
  int i;
  gchar *uri, *hostname, *res;
  GError *error;
  
  for (i = 0; i < G_N_ELEMENTS (to_uri_tests); i++)
    {
      if (to_uri_tests[i].expected_error != 0)
	continue;

      error = NULL;
      uri = g_filename_to_uri (to_uri_tests[i].filename,
			       to_uri_tests[i].hostname,
			       &error);
      
      if (error != NULL)
	{
	  g_print ("g_filename_to_uri failed unexpectedly: %s\n", 
		   error->message);
	  any_failed = TRUE;
	  continue;
	}
      
      error = NULL;
      res = g_filename_from_uri (uri, &hostname, &error);
      if (error != NULL)
	{
	  g_print ("g_filename_from_uri failed unexpectedly: %s\n", 
		   error->message);
	  any_failed = TRUE;
	  continue;
	}

      if (safe_strcmp_filename (to_uri_tests[i].filename, res))
	{
	  g_print ("roundtrip test %d failed, filename modified: "
		   " expected \"%s\", but got \"%s\"\n",
		   i, to_uri_tests[i].filename, res);
	  any_failed = TRUE;
	}

      if (safe_strcmp_hostname (to_uri_tests[i].hostname, hostname))
	{
	  g_print ("roundtrip test %d failed, hostname modified: "
		     " expected \"%s\", but got \"%s\"\n",
		   i, to_uri_tests[i].hostname, hostname);
	  any_failed = TRUE;
	}
    }
}

static void
run_uri_list_tests (void)
{
  /* straight from the RFC */
  gchar *list =
    "# urn:isbn:0-201-08372-8\r\n"
    "http://www.huh.org/books/foo.html\r\n"
    "http://www.huh.org/books/foo.pdf   \r\n"
    "   ftp://ftp.foo.org/books/foo.txt\r\n";
  gchar *expected_uris[] = {
    "http://www.huh.org/books/foo.html",
    "http://www.huh.org/books/foo.pdf",
    "ftp://ftp.foo.org/books/foo.txt"
  };

  gchar **uris;
  gint j;

  uris = g_uri_list_extract_uris (list);
  
  if (g_strv_length (uris) != 3)
    {
      g_print ("uri list test failed: "
	       " expected %d uris, but got %d\n",
	       3, g_strv_length (uris));
      any_failed = TRUE;
    }
  
  for (j = 0; j < 3; j++)
    {
      if (safe_strcmp (uris[j], expected_uris[j])) 
	{
	  g_print ("uri list test failed: "
		   " expected \"%s\", but got \"%s\"\n",
		   expected_uris[j], uris[j]);
	  any_failed = TRUE;
	}
    }

  g_strfreev (uris);

  uris = g_uri_list_extract_uris ("# just hot air\r\n# more hot air");
  if (g_strv_length (uris) != 0)
    {
      g_print ("uri list test 2 failed: "
	       " expected %d uris, but got %d (first is \"%s\")\n",
	       0, g_strv_length (uris), uris[0]);
      any_failed = TRUE;
    }
  
}

int
main (int   argc,
      char *argv[])
{
#ifdef G_OS_UNIX
#  ifdef HAVE_UNSETENV  
  unsetenv ("G_BROKEN_FILENAMES");
#  else
  /* putenv with no = isn't standard, but works to unset the variable
   * on some systems
   */
  putenv ("G_BROKEN_FILENAMES");
#  endif  
#endif

  run_to_uri_tests ();
  run_from_uri_tests ();
  run_roundtrip_tests ();
  run_uri_list_tests ();

  return any_failed ? 1 : 0;
}
