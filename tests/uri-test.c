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
  { "/etc", "localhost", "file://localhost/etc"},
  { "/etc", "otherhost", "file://otherhost/etc"},
#ifdef G_OS_WIN32
  { "c:\\windows", NULL, "file:///c:\\windows"}, /* these 3 tests almost certainly fail */
  { "c:\\windows", "localhost", "file://localhost/c:\\windows"},
  { "c:\\windows", "otherhost", "file://otherhost/c:\\windows"},
#else
  { "c:\\windows", NULL, NULL, G_CONVERT_ERROR_NOT_ABSOLUTE_PATH}, /* it's important to get this error on Unix */
  { "c:\\windows", "localhost", NULL, G_CONVERT_ERROR_NOT_ABSOLUTE_PATH},
  { "c:\\windows", "otherhost", NULL, G_CONVERT_ERROR_NOT_ABSOLUTE_PATH},
#endif
  { "etc", "localhost", NULL, G_CONVERT_ERROR_NOT_ABSOLUTE_PATH},
  { "/etc/öäå", NULL, NULL, G_CONVERT_ERROR_ILLEGAL_SEQUENCE},
  { "/etc/Ã¶Ã¤Ã¥", NULL, "file:///etc/%C3%B6%C3%A4%C3%A5"},
  { "/etc", "Ã¶Ã¤Ã¥", "file://%C3%B6%C3%A4%C3%A5/etc"},
  { "/etc", "åäö", NULL, G_CONVERT_ERROR_ILLEGAL_SEQUENCE},
  { "/etc/file with #%", NULL, "file:///etc/file%20with%20%23%25"},
  { "", NULL, NULL, G_CONVERT_ERROR_NOT_ABSOLUTE_PATH},
  { "", "", NULL, G_CONVERT_ERROR_NOT_ABSOLUTE_PATH},
  { "", "localhost", NULL, G_CONVERT_ERROR_NOT_ABSOLUTE_PATH},
  { "", "otherhost", NULL, G_CONVERT_ERROR_NOT_ABSOLUTE_PATH},
  { "/0123456789", NULL, "file:///0123456789"},
  { "/ABCDEFGHIJKLMNOPQRSTUVWXYZ", NULL, "file:///ABCDEFGHIJKLMNOPQRSTUVWXYZ"},
  { "/abcdefghijklmnopqrstuvwxyz", NULL, "file:///abcdefghijklmnopqrstuvwxyz"},
  { "/-_.!~*'()", NULL, "file:///-_.!~*'()"},
  { "/\"#%<>[\\]^`{|}\x7F", NULL, "file:///%22%23%25%3C%3E%5B%5C%5D%5E%60%7B%7C%7D%7F"},
  { "/;@+$,", NULL, "file:///%3B%40%2B%24%2C"},
  { "/:", NULL, "file:///:"}, /* not escaped even though reserved as side effect of DOS support -- is that really what we want on Unix? */
  { "/?&=", NULL, "file:///?&="}, /* these are not escaped and other reserved characters are -- is that really what we want? */
  { "/", "0123456789", "file://0123456789/"},
  { "/", "ABCDEFGHIJKLMNOPQRSTUVWXYZ", "file://ABCDEFGHIJKLMNOPQRSTUVWXYZ/"},
  { "/", "abcdefghijklmnopqrstuvwxyz", "file://abcdefghijklmnopqrstuvwxyz/"},
  { "/", "-_.!~*'()", "file://-_.!~*'()/"},
  { "/", "\"#%<>[\\]^`{|}\x7F", "file://%22%23%25%3C%3E%5B%5C%5D%5E%60%7B%7C%7D%7F/"},
  { "/", ";?&=+$,", "file://%3B%3F%26%3D%2B%24%2C/"},
  { "/", "/", "file:////"}, /* should be "file://%2F/" or an error */
  { "/", "@:", "file://@:/"}, /* these are not escaped and other reserved characters are -- is that really what we want? */
  { "/", "\x80\xFF", NULL, G_CONVERT_ERROR_ILLEGAL_SEQUENCE},
  { "/", "\xC3\x80\xC3\xBF", "file://%C3%80%C3%BF/"},
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
  { "file://localhost/etc", "/etc", "localhost"},
  { "file://localhost/etc/%23%25%20file", "/etc/#% file", "localhost"},
  { "file://otherhost/etc", "/etc", "otherhost"},
  { "file://otherhost/etc/%23%25%20file", "/etc/#% file", "otherhost"},
  { "file://%C3%B6%C3%A4%C3%A5/etc", "/etc", "Ã¶Ã¤Ã¥"},
  { "file:////etc/%C3%B6%C3%C3%C3%A5", NULL, NULL, G_CONVERT_ERROR_INVALID_URI},
  { "file://localhost/åäö", NULL, NULL, G_CONVERT_ERROR_INVALID_URI},
  { "file://åäö/etc", NULL, NULL, G_CONVERT_ERROR_INVALID_URI},
  { "file:///some/file#bad", NULL, NULL, G_CONVERT_ERROR_INVALID_URI},
  { "file://some", NULL, NULL, G_CONVERT_ERROR_INVALID_URI},
  { "", NULL, NULL, G_CONVERT_ERROR_NOT_ABSOLUTE_FILE_URI}, /* should be G_CONVERT_ERROR_INVALID_URI */
  { "file:test", NULL, NULL, G_CONVERT_ERROR_NOT_ABSOLUTE_FILE_URI},
  { "http://www.yahoo.com/", NULL, NULL, G_CONVERT_ERROR_NOT_ABSOLUTE_FILE_URI},
  { "file:////etc", "/etc"}, /* should be "//etc" -- mistake in code for DOS results in dropped slash */
  { "file://///etc", "//etc"}, /* should be "///etc" -- mistake in code for DOS results in dropped slash */
  { "file:///c:\\foo", "/c:\\foo"}, /* should be "c:\\foo" on DOS perhaps, but that would be bad for Unix */
  { "file:///c:/foo", "/c:/foo"}, /* should be "c:/foo" on DOS perhaps, but that would be bad for Unix */
  { "file:////c:/foo", "/c:/foo"}, /* should be "//c:/foo" on Unix */
  { "file://0123456789/", "/", "0123456789"},
  { "file://ABCDEFGHIJKLMNOPQRSTUVWXYZ/", "/", "ABCDEFGHIJKLMNOPQRSTUVWXYZ"},
  { "file://abcdefghijklmnopqrstuvwxyz/", "/", "abcdefghijklmnopqrstuvwxyz"},
  { "file://-_.!~*'()/", "/", "-_.!~*'()"},
  { "file://\"<>[\\]^`{|}\x7F/", "/", "\"<>[\\]^`{|}\x7F"},
  { "file://;?&=+$,/", "/", ";?&=+$,"},
  { "file://%C3%80%C3%BF/", "/", "\xC3\x80\xC3\xBF"},
  { "file://@/", "/", "@"},
  { "file://:/", "/", ":"},
  { "file://#/", NULL, NULL, G_CONVERT_ERROR_INVALID_URI},
  { "file://%23/", "/", "#"}, /* is it dangerous to return a hostname with a "#" character in it? */
  { "file://%2F/", "/", "/"}, /* is it dangerous to return a hostname with a "/" character in it? */
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
      
      /* Give some output */
      g_print (".");
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
      
      /* Give some output */
      g_print (".");
    }
  g_print ("\n");
}

int
main (int   argc,
      char *argv[])
{
  run_to_uri_tests ();
  run_from_uri_tests ();

  return any_failed ? 1 : 0;
}
