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

#undef G_LOG_DOMAIN

#include <stdio.h>
#include <string.h>
#include "glib.h"
#include <stdarg.h>

static gboolean any_failed = FALSE;
static gboolean failed = FALSE;

#define	TEST(m,cond)	G_STMT_START { failed = !(cond); \
if (failed) \
  { if (!m) \
      g_print ("\n(%s:%d) failed for: %s\n", __FILE__, __LINE__, ( # cond )); \
    else \
      g_print ("\n(%s:%d) failed for: %s: (%s)\n", __FILE__, __LINE__, ( # cond ), (gchar*)m); \
      any_failed = TRUE; \
  } \
else \
  g_print ("."); fflush (stdout); \
} G_STMT_END

#define GLIB_TEST_STRING "el dorado "

static gboolean
strv_check (gchar **strv, ...)
{
  gboolean ok = TRUE;
  gint i = 0;
  va_list list;

  va_start (list, strv);
  while (ok)
    {
      const gchar *str = va_arg (list, const char *);
      if (strv[i] == NULL)
	{
	  ok = str == NULL;
	  break;
	}
      if (str == NULL)
	ok = FALSE;
      else if (strcmp (strv[i], str) != 0)
	ok = FALSE;
      i++;
    }
  va_end (list);

  g_strfreev (strv);

  return ok;
}

int
main (int   argc,
      char *argv[])
{
  gchar *string;
  gchar *vec[] = { "Foo", "Bar", NULL };
  gchar **copy;
  
  TEST (NULL, g_ascii_strcasecmp ("FroboZZ", "frobozz") == 0);
  TEST (NULL, g_ascii_strcasecmp ("frobozz", "frobozz") == 0);
  TEST (NULL, g_ascii_strcasecmp ("frobozz", "FROBOZZ") == 0);
  TEST (NULL, g_ascii_strcasecmp ("FROBOZZ", "froboz") != 0);
  TEST (NULL, g_ascii_strcasecmp ("", "") == 0);
  TEST (NULL, g_ascii_strcasecmp ("!#%&/()", "!#%&/()") == 0);
  TEST (NULL, g_ascii_strcasecmp ("a", "b") < 0);
  TEST (NULL, g_ascii_strcasecmp ("a", "B") < 0);
  TEST (NULL, g_ascii_strcasecmp ("A", "b") < 0);
  TEST (NULL, g_ascii_strcasecmp ("A", "B") < 0);
  TEST (NULL, g_ascii_strcasecmp ("b", "a") > 0);
  TEST (NULL, g_ascii_strcasecmp ("b", "A") > 0);
  TEST (NULL, g_ascii_strcasecmp ("B", "a") > 0);
  TEST (NULL, g_ascii_strcasecmp ("B", "A") > 0);

  TEST (NULL, g_strdup (NULL) == NULL);
  string = g_strdup (GLIB_TEST_STRING);
  TEST (NULL, string != NULL);
  TEST (NULL, strcmp (string, GLIB_TEST_STRING) == 0);
  g_free(string);
  
  string = g_strconcat (GLIB_TEST_STRING, NULL);
  TEST (NULL, string != NULL);
  TEST (NULL, strcmp (string, GLIB_TEST_STRING) == 0);
  g_free(string);
  
  string = g_strconcat (GLIB_TEST_STRING, GLIB_TEST_STRING, 
			GLIB_TEST_STRING, NULL);
  TEST (NULL, string != NULL);
  TEST (NULL, strcmp (string, GLIB_TEST_STRING GLIB_TEST_STRING
		      GLIB_TEST_STRING) == 0);
  g_free(string);
  
  string = g_strdup_printf ("%05d %-5s", 21, "test");
  TEST (NULL, string != NULL);
  TEST (NULL, strcmp(string, "00021 test ") == 0);
  g_free (string);
  
  TEST (NULL, strcmp
	(g_strcompress ("abc\\\\\\\"\\b\\f\\n\\r\\t\\003\\177\\234\\313\\12345z"),
	 "abc\\\"\b\f\n\r\t\003\177\234\313\12345z") == 0);
  TEST (NULL, strcmp (g_strescape("abc\\\"\b\f\n\r\t\003\177\234\313", NULL),
		      "abc\\\\\\\"\\b\\f\\n\\r\\t\\003\\177\\234\\313") == 0);
  TEST (NULL, strcmp(g_strescape("abc\\\"\b\f\n\r\t\003\177\234\313",
				 "\b\f\001\002\003\004"),
		     "abc\\\\\\\"\b\f\\n\\r\\t\003\\177\\234\\313") == 0);

  copy = g_strdupv (vec);
  TEST (NULL, strcmp (copy[0], "Foo") == 0);
  TEST (NULL, strcmp (copy[1], "Bar") == 0);
  TEST (NULL, copy[2] == NULL);
  g_strfreev (copy);
  
  TEST (NULL, strcmp (g_strstr_len ("FooBarFooBarFoo", 6, "Bar"),
		      "BarFooBarFoo") == 0);
  TEST (NULL, strcmp (g_strrstr ("FooBarFooBarFoo", "Bar"),
		      "BarFoo") == 0);
  TEST (NULL, strcmp (g_strrstr_len ("FooBarFooBarFoo", 14, "BarFoo"),
		      "BarFooBarFoo") == 0);

  TEST (NULL, strv_check (g_strsplit ("", ",", 0), "", NULL));
  TEST (NULL, strv_check (g_strsplit ("x", ",", 0), "x", NULL));
  TEST (NULL, strv_check (g_strsplit ("x,y", ",", 0), "x", "y", NULL));
  TEST (NULL, strv_check (g_strsplit ("x,y,", ",", 0), "x", "y", "", NULL));
  TEST (NULL, strv_check (g_strsplit (",x,y", ",", 0), "", "x", "y", NULL));
  TEST (NULL, strv_check (g_strsplit (",x,y,", ",", 0), "", "x", "y", "", NULL));
  TEST (NULL, strv_check (g_strsplit ("x,y,z", ",", 0), "x", "y", "z", NULL));
  TEST (NULL, strv_check (g_strsplit ("x,y,z,", ",", 0), "x", "y", "z", "", NULL));
  TEST (NULL, strv_check (g_strsplit (",x,y,z", ",", 0), "", "x", "y", "z", NULL));
  TEST (NULL, strv_check (g_strsplit (",x,y,z,", ",", 0), "", "x", "y", "z", "", NULL));
  TEST (NULL, strv_check (g_strsplit (",,x,,y,,z,,", ",", 0), "", "", "x", "", "y", "", "z", "", "", NULL));
  TEST (NULL, strv_check (g_strsplit (",,x,,y,,z,,", ",,", 0), "", "x", "y", "z", "", NULL));

  TEST (NULL, strv_check (g_strsplit ("", ",", 2), "", NULL));
  TEST (NULL, strv_check (g_strsplit ("x", ",", 2), "x", NULL));
  TEST (NULL, strv_check (g_strsplit ("x,y", ",", 2), "x", "y", NULL));
  TEST (NULL, strv_check (g_strsplit ("x,y,", ",", 2), "x", "y", "", NULL));
  TEST (NULL, strv_check (g_strsplit (",x,y", ",", 2), "", "x", "y", NULL));
  TEST (NULL, strv_check (g_strsplit (",x,y,", ",", 2), "", "x", "y,", NULL));
  TEST (NULL, strv_check (g_strsplit ("x,y,z", ",", 2), "x", "y", "z", NULL));
  TEST (NULL, strv_check (g_strsplit ("x,y,z,", ",", 2), "x", "y", "z,", NULL));
  TEST (NULL, strv_check (g_strsplit (",x,y,z", ",", 2), "", "x", "y,z", NULL));
  TEST (NULL, strv_check (g_strsplit (",x,y,z,", ",", 2), "", "x", "y,z,", NULL));
  TEST (NULL, strv_check (g_strsplit (",,x,,y,,z,,", ",", 2), "", "", "x,,y,,z,,", NULL));
  TEST (NULL, strv_check (g_strsplit (",,x,,y,,z,,", ",,", 2), "", "x", "y,,z,,", NULL));

  g_print ("\n");

  return any_failed;
}
