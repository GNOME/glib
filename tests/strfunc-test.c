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

#include <stdio.h>
#include <string.h>
#include "glib.h"
#include <stdarg.h>
#include <ctype.h>

static gboolean any_failed = FALSE;
static gboolean failed = FALSE;

#define	TEST(m,cond)	G_STMT_START { failed = !(cond); \
if (failed) \
  { if (!m) \
      g_print ("(%s:%d) failed for: %s\n", __FILE__, __LINE__, ( # cond )); \
    else \
      g_print ("(%s:%d) failed for: %s: (%s)\n", __FILE__, __LINE__, ( # cond ), m ? (gchar*)m : ""); \
    fflush (stdout); \
    any_failed = TRUE; \
  } \
} G_STMT_END

#define TEST_FAILED(message) \
  G_STMT_START { g_print ("Error: "); g_print message; g_print ("\n"); any_failed = TRUE; } G_STMT_END

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

static gboolean
str_check (gchar *str,
	   gchar *expected)
{
  gboolean ok = (strcmp (str, expected) == 0);

  g_free (str);

  return ok;
}

static gboolean
strchomp_check (gchar *str,
		gchar *expected)
{
  gchar *tmp = strdup (str);
  gboolean ok;

  g_strchomp (tmp);
  ok = (strcmp (tmp, expected) == 0);
  g_free (tmp);

  return ok;
}

#define FOR_ALL_CTYPE(macro)	\
	macro(isalnum)		\
	macro(isalpha)		\
	macro(iscntrl)		\
	macro(isdigit)		\
	macro(isgraph)		\
	macro(islower)		\
	macro(isprint)		\
	macro(ispunct)		\
	macro(isspace)		\
	macro(isupper)		\
	macro(isxdigit)

#define DEFINE_CALL_CTYPE(function)		\
	static int				\
	call_##function (int c)			\
	{					\
		return function (c);		\
	}

#define DEFINE_CALL_G_ASCII_CTYPE(function)	\
	static gboolean				\
	call_g_ascii_##function (gchar c)	\
	{					\
		return g_ascii_##function (c);	\
	}

FOR_ALL_CTYPE (DEFINE_CALL_CTYPE)
FOR_ALL_CTYPE (DEFINE_CALL_G_ASCII_CTYPE)

static void
test_is_function (const char *name,
		  gboolean (* ascii_function) (gchar),
		  int (* c_library_function) (int),
		  gboolean (* unicode_function) (gunichar))
{
  int c;

  for (c = 0; c <= 0x7F; c++)
    {
      gboolean ascii_result = ascii_function ((gchar)c);
      gboolean c_library_result = c_library_function (c) != 0;
      gboolean unicode_result = unicode_function ((gunichar) c);
      if (ascii_result != c_library_result && c != '\v')
	TEST_FAILED (("g_ascii_%s returned %d and %s returned %d for 0x%X",
		      name, ascii_result, name, c_library_result, c));
      if (ascii_result != unicode_result)
	TEST_FAILED (("g_ascii_%s returned %d and g_unichar_%s returned %d for 0x%X",
		      name, ascii_result, name, unicode_result, c));
    }
  for (c = 0x80; c <= 0xFF; c++)
    {
      gboolean ascii_result = ascii_function ((gchar)c);
      if (ascii_result)
	TEST_FAILED (("g_ascii_%s returned TRUE for 0x%X",
		      name, c));
    }
}

static void
test_to_function (const char *name,
		  gchar (* ascii_function) (gchar),
		  int (* c_library_function) (int),
		  gunichar (* unicode_function) (gunichar))
{
  int c;

  for (c = 0; c <= 0x7F; c++)
    {
      int ascii_result = (guchar) ascii_function ((gchar) c);
      int c_library_result = c_library_function (c);
      int unicode_result = unicode_function ((gunichar) c);
      if (ascii_result != c_library_result)
	TEST_FAILED (("g_ascii_%s returned 0x%X and %s returned 0x%X for 0x%X",
		      name, ascii_result, name, c_library_result, c));
      if (ascii_result != unicode_result)
	TEST_FAILED (("g_ascii_%s returned 0x%X and g_unichar_%s returned 0x%X for 0x%X",
		      name, ascii_result, name, unicode_result, c));
    }
  for (c = 0x80; c <= 0xFF; c++)
    {
      int ascii_result = (guchar) ascii_function ((gchar) c);
      if (ascii_result != c)
	TEST_FAILED (("g_ascii_%s returned 0x%X for 0x%X",
		      name, ascii_result, c));
    }
}

static void
test_digit_function (const char *name,
		     int (* ascii_function) (gchar),
		     int (* unicode_function) (gunichar))
{
  int c;

  for (c = 0; c <= 0x7F; c++)
    {
      int ascii_result = ascii_function ((gchar) c);
      int unicode_result = unicode_function ((gunichar) c);
      if (ascii_result != unicode_result)
	TEST_FAILED (("g_ascii_%s_value returned %d and g_unichar_%s_value returned %d for 0x%X",
		      name, ascii_result, name, unicode_result, c));
    }
  for (c = 0x80; c <= 0xFF; c++)
    {
      int ascii_result = ascii_function ((gchar) c);
      if (ascii_result != -1)
	TEST_FAILED (("g_ascii_%s_value returned %d for 0x%X",
		      name, ascii_result, c));
    }
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

  /* Test g_strsplit() */
  TEST (NULL, strv_check (g_strsplit ("", ",", 0), NULL));
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

  TEST (NULL, strv_check (g_strsplit ("", ",", 1), NULL));
  TEST (NULL, strv_check (g_strsplit ("x", ",", 1), "x", NULL));
  TEST (NULL, strv_check (g_strsplit ("x,y", ",", 1), "x,y", NULL));
  TEST (NULL, strv_check (g_strsplit ("x,y,", ",", 1), "x,y,", NULL));
  TEST (NULL, strv_check (g_strsplit (",x,y", ",", 1), ",x,y", NULL));
  TEST (NULL, strv_check (g_strsplit (",x,y,", ",", 1), ",x,y,", NULL));
  TEST (NULL, strv_check (g_strsplit ("x,y,z", ",", 1), "x,y,z", NULL));
  TEST (NULL, strv_check (g_strsplit ("x,y,z,", ",", 1), "x,y,z,", NULL));
  TEST (NULL, strv_check (g_strsplit (",x,y,z", ",", 1), ",x,y,z", NULL));
  TEST (NULL, strv_check (g_strsplit (",x,y,z,", ",", 1), ",x,y,z,", NULL));
  TEST (NULL, strv_check (g_strsplit (",,x,,y,,z,,", ",", 1), ",,x,,y,,z,,", NULL));
  TEST (NULL, strv_check (g_strsplit (",,x,,y,,z,,", ",,", 1), ",,x,,y,,z,,", NULL));

  TEST (NULL, strv_check (g_strsplit ("", ",", 2), NULL));
  TEST (NULL, strv_check (g_strsplit ("x", ",", 2), "x", NULL));
  TEST (NULL, strv_check (g_strsplit ("x,y", ",", 2), "x", "y", NULL));
  TEST (NULL, strv_check (g_strsplit ("x,y,", ",", 2), "x", "y,", NULL));
  TEST (NULL, strv_check (g_strsplit (",x,y", ",", 2), "", "x,y", NULL));
  TEST (NULL, strv_check (g_strsplit (",x,y,", ",", 2), "", "x,y,", NULL));
  TEST (NULL, strv_check (g_strsplit ("x,y,z", ",", 2), "x", "y,z", NULL));
  TEST (NULL, strv_check (g_strsplit ("x,y,z,", ",", 2), "x", "y,z,", NULL));
  TEST (NULL, strv_check (g_strsplit (",x,y,z", ",", 2), "", "x,y,z", NULL));
  TEST (NULL, strv_check (g_strsplit (",x,y,z,", ",", 2), "", "x,y,z,", NULL));
  TEST (NULL, strv_check (g_strsplit (",,x,,y,,z,,", ",", 2), "", ",x,,y,,z,,", NULL));
  TEST (NULL, strv_check (g_strsplit (",,x,,y,,z,,", ",,", 2), "", "x,,y,,z,,", NULL));

  /* Test g_strsplit_set() */
  TEST (NULL, strv_check (g_strsplit_set ("", ",/", 0), NULL));
  TEST (NULL, strv_check (g_strsplit_set (":def/ghi:", ":/", -1), "", "def", "ghi", "", NULL));
  TEST (NULL, strv_check (g_strsplit_set ("abc:def/ghi", ":/", -1), "abc", "def", "ghi", NULL));
  TEST (NULL, strv_check (g_strsplit_set (",;,;,;,;", ",;", -1), "", "", "", "", "", "", "", "", "", NULL));
  TEST (NULL, strv_check (g_strsplit_set (",,abc.def", ".,", -1), "", "", "abc", "def", NULL));

  TEST (NULL, strv_check (g_strsplit_set (",x.y", ",.", 0), "", "x", "y", NULL));
  TEST (NULL, strv_check (g_strsplit_set (".x,y,", ",.", 0), "", "x", "y", "", NULL));
  TEST (NULL, strv_check (g_strsplit_set ("x,y.z", ",.", 0), "x", "y", "z", NULL));
  TEST (NULL, strv_check (g_strsplit_set ("x.y,z,", ",.", 0), "x", "y", "z", "", NULL));
  TEST (NULL, strv_check (g_strsplit_set (",x.y,z", ",.", 0), "", "x", "y", "z", NULL));
  TEST (NULL, strv_check (g_strsplit_set (",x,y,z,", ",.", 0), "", "x", "y", "z", "", NULL));
  TEST (NULL, strv_check (g_strsplit_set (",.x,,y,;z..", ".,;", 0), "", "", "x", "", "y", "", "z", "", "", NULL));
  TEST (NULL, strv_check (g_strsplit_set (",,x,,y,,z,,", ",,", 0), "", "", "x", "", "y", "", "z", "", "", NULL));

  TEST (NULL, strv_check (g_strsplit_set ("x,y.z", ",.", 1), "x,y.z", NULL));
  TEST (NULL, strv_check (g_strsplit_set ("x.y,z,", ",.", 1), "x.y,z,", NULL));
  TEST (NULL, strv_check (g_strsplit_set (",x,y,z", ",.", 1), ",x,y,z", NULL));
  TEST (NULL, strv_check (g_strsplit_set (",x,y.z,", ",.", 1), ",x,y.z,", NULL));
  TEST (NULL, strv_check (g_strsplit_set (",,x,.y,,z,,", ",.", 1), ",,x,.y,,z,,", NULL));
  TEST (NULL, strv_check (g_strsplit_set (",.x,,y,,z,,", ",,..", 1), ",.x,,y,,z,,", NULL));
   
  TEST (NULL, strv_check (g_strsplit_set ("", ",", 0), NULL));
  TEST (NULL, strv_check (g_strsplit_set ("x", ",", 0), "x", NULL));
  TEST (NULL, strv_check (g_strsplit_set ("x,y", ",", 0), "x", "y", NULL));
  TEST (NULL, strv_check (g_strsplit_set ("x,y,", ",", 0), "x", "y", "", NULL));
  TEST (NULL, strv_check (g_strsplit_set (",x,y", ",", 0), "", "x", "y", NULL));
  TEST (NULL, strv_check (g_strsplit_set (",x,y,", ",", 0), "", "x", "y", "", NULL));
  TEST (NULL, strv_check (g_strsplit_set ("x,y,z", ",", 0), "x", "y", "z", NULL));
  TEST (NULL, strv_check (g_strsplit_set ("x,y,z,", ",", 0), "x", "y", "z", "", NULL));
  TEST (NULL, strv_check (g_strsplit_set (",x,y,z", ",", 0), "", "x", "y", "z", NULL));
  TEST (NULL, strv_check (g_strsplit_set (",x,y,z,", ",", 0), "", "x", "y", "z", "", NULL));
  TEST (NULL, strv_check (g_strsplit_set (",,x,,y,,z,,", ",", 0), "", "", "x", "", "y", "", "z", "", "", NULL));

  TEST (NULL, strv_check (g_strsplit_set ("", ",", 1), NULL));
  TEST (NULL, strv_check (g_strsplit_set ("x", ",", 1), "x", NULL));
  TEST (NULL, strv_check (g_strsplit_set ("x,y", ",", 1), "x,y", NULL));
  TEST (NULL, strv_check (g_strsplit_set ("x,y,", ",", 1), "x,y,", NULL));
  TEST (NULL, strv_check (g_strsplit_set (",x,y", ",", 1), ",x,y", NULL));
  TEST (NULL, strv_check (g_strsplit_set (",x,y,", ",", 1), ",x,y,", NULL));
  TEST (NULL, strv_check (g_strsplit_set ("x,y,z", ",", 1), "x,y,z", NULL));
  TEST (NULL, strv_check (g_strsplit_set ("x,y,z,", ",", 1), "x,y,z,", NULL));
  TEST (NULL, strv_check (g_strsplit_set (",x,y,z", ",", 1), ",x,y,z", NULL));
  TEST (NULL, strv_check (g_strsplit_set (",x,y,z,", ",", 1), ",x,y,z,", NULL));
  TEST (NULL, strv_check (g_strsplit_set (",,x,,y,,z,,", ",", 1), ",,x,,y,,z,,", NULL));
  TEST (NULL, strv_check (g_strsplit_set (",,x,,y,,z,,", ",,", 1), ",,x,,y,,z,,", NULL));

  TEST (NULL, strv_check (g_strsplit_set ("", ",", 2), NULL));
  TEST (NULL, strv_check (g_strsplit_set ("x", ",", 2), "x", NULL));
  TEST (NULL, strv_check (g_strsplit_set ("x,y", ",", 2), "x", "y", NULL));
  TEST (NULL, strv_check (g_strsplit_set ("x,y,", ",", 2), "x", "y,", NULL));
  TEST (NULL, strv_check (g_strsplit_set (",x,y", ",", 2), "", "x,y", NULL));
  TEST (NULL, strv_check (g_strsplit_set (",x,y,", ",", 2), "", "x,y,", NULL));
  TEST (NULL, strv_check (g_strsplit_set ("x,y,z", ",", 2), "x", "y,z", NULL));
  TEST (NULL, strv_check (g_strsplit_set ("x,y,z,", ",", 2), "x", "y,z,", NULL));
  TEST (NULL, strv_check (g_strsplit_set (",x,y,z", ",", 2), "", "x,y,z", NULL));
  TEST (NULL, strv_check (g_strsplit_set (",x,y,z,", ",", 2), "", "x,y,z,", NULL));
  TEST (NULL, strv_check (g_strsplit_set (",,x,,y,,z,,", ",", 2), "", ",x,,y,,z,,", NULL));
  
  TEST (NULL, strv_check (g_strsplit_set (",,x,.y,..z,,", ",.", 3), "", "", "x,.y,..z,,", NULL));

  
  
  #define TEST_IS(name) test_is_function (#name, call_g_ascii_##name, call_##name, g_unichar_##name);

  FOR_ALL_CTYPE(TEST_IS)

  #undef TEST_IS

  #define TEST_TO(name) test_to_function (#name, g_ascii_##name, name, g_unichar_##name)

  TEST_TO (tolower);
  TEST_TO (toupper);

  #undef TEST_TO

  #define TEST_DIGIT(name) test_digit_function (#name, g_ascii_##name##_value, g_unichar_##name##_value)

  TEST_DIGIT (digit);
  TEST_DIGIT (xdigit);

  #undef TEST_DIGIT

  /* Tests for strchomp () */
  TEST (NULL, strchomp_check ("", ""));
  TEST (NULL, strchomp_check (" ", ""));
  TEST (NULL, strchomp_check (" \t\r\n", ""));
  TEST (NULL, strchomp_check ("a ", "a"));
  TEST (NULL, strchomp_check ("a  ", "a"));
  TEST (NULL, strchomp_check ("a a", "a a"));
  TEST (NULL, strchomp_check ("a a ", "a a"));

  /* Tests for g_build_path, g_build_filename */

  TEST (NULL, str_check (g_build_path ("", NULL), ""));
  TEST (NULL, str_check (g_build_path ("", "", NULL), ""));
  TEST (NULL, str_check (g_build_path ("", "x", NULL), "x"));
  TEST (NULL, str_check (g_build_path ("", "x", "y",  NULL), "xy"));
  TEST (NULL, str_check (g_build_path ("", "x", "y", "z", NULL), "xyz"));

  TEST (NULL, str_check (g_build_path (":", NULL), ""));
  TEST (NULL, str_check (g_build_path (":", ":", NULL), ":"));
  TEST (NULL, str_check (g_build_path (":", ":x", NULL), ":x"));
  TEST (NULL, str_check (g_build_path (":", "x:", NULL), "x:"));
  TEST (NULL, str_check (g_build_path (":", "", "x", NULL), "x"));
  TEST (NULL, str_check (g_build_path (":", "", ":x", NULL), ":x"));
  TEST (NULL, str_check (g_build_path (":", ":", "x", NULL), ":x"));
  TEST (NULL, str_check (g_build_path (":", "::", "x", NULL), "::x"));
  TEST (NULL, str_check (g_build_path (":", "x", "", NULL), "x"));
  TEST (NULL, str_check (g_build_path (":", "x:", "", NULL), "x:"));
  TEST (NULL, str_check (g_build_path (":", "x", ":", NULL), "x:"));
  TEST (NULL, str_check (g_build_path (":", "x", "::", NULL), "x::"));
  TEST (NULL, str_check (g_build_path (":", "x", "y",  NULL), "x:y"));
  TEST (NULL, str_check (g_build_path (":", ":x", "y", NULL), ":x:y"));
  TEST (NULL, str_check (g_build_path (":", "x", "y:", NULL), "x:y:"));
  TEST (NULL, str_check (g_build_path (":", ":x:", ":y:", NULL), ":x:y:"));
  TEST (NULL, str_check (g_build_path (":", ":x::", "::y:", NULL), ":x:y:"));
  TEST (NULL, str_check (g_build_path (":", "x", "","y",  NULL), "x:y"));
  TEST (NULL, str_check (g_build_path (":", "x", ":", "y",  NULL), "x:y"));
  TEST (NULL, str_check (g_build_path (":", "x", "::", "y",  NULL), "x:y"));
  TEST (NULL, str_check (g_build_path (":", "x", "y", "z", NULL), "x:y:z"));
  TEST (NULL, str_check (g_build_path (":", ":x:", ":y:", ":z:", NULL), ":x:y:z:"));
  TEST (NULL, str_check (g_build_path (":", "::x::", "::y::", "::z::", NULL), "::x:y:z::"));

  TEST (NULL, str_check (g_build_path ("::", NULL), ""));
  TEST (NULL, str_check (g_build_path ("::", "::", NULL), "::"));
  TEST (NULL, str_check (g_build_path ("::", ":::", NULL), ":::"));
  TEST (NULL, str_check (g_build_path ("::", "::x", NULL), "::x"));
  TEST (NULL, str_check (g_build_path ("::", "x::", NULL), "x::"));
  TEST (NULL, str_check (g_build_path ("::", "", "x", NULL), "x"));
  TEST (NULL, str_check (g_build_path ("::", "", "::x", NULL), "::x"));
  TEST (NULL, str_check (g_build_path ("::", "::", "x", NULL), "::x"));
  TEST (NULL, str_check (g_build_path ("::", "::::", "x", NULL), "::::x"));
  TEST (NULL, str_check (g_build_path ("::", "x", "", NULL), "x"));
  TEST (NULL, str_check (g_build_path ("::", "x::", "", NULL), "x::"));
  TEST (NULL, str_check (g_build_path ("::", "x", "::", NULL), "x::"));
  /* This following is weird, but keeps the definition simple */
  TEST (NULL, str_check (g_build_path ("::", "x", ":::", NULL), "x:::::"));
  TEST (NULL, str_check (g_build_path ("::", "x", "::::", NULL), "x::::"));
  TEST (NULL, str_check (g_build_path ("::", "x", "y",  NULL), "x::y"));
  TEST (NULL, str_check (g_build_path ("::", "::x", "y", NULL), "::x::y"));
  TEST (NULL, str_check (g_build_path ("::", "x", "y::", NULL), "x::y::"));
  TEST (NULL, str_check (g_build_path ("::", "::x::", "::y::", NULL), "::x::y::"));
  TEST (NULL, str_check (g_build_path ("::", "::x:::", ":::y::", NULL), "::x::::y::"));
  TEST (NULL, str_check (g_build_path ("::", "::x::::", "::::y::", NULL), "::x::y::"));
  TEST (NULL, str_check (g_build_path ("::", "x", "", "y",  NULL), "x::y"));
  TEST (NULL, str_check (g_build_path ("::", "x", "::", "y",  NULL), "x::y"));
  TEST (NULL, str_check (g_build_path ("::", "x", "::::", "y",  NULL), "x::y"));
  TEST (NULL, str_check (g_build_path ("::", "x", "y", "z", NULL), "x::y::z"));
  TEST (NULL, str_check (g_build_path ("::", "::x::", "::y::", "::z::", NULL), "::x::y::z::"));
  TEST (NULL, str_check (g_build_path ("::", ":::x:::", ":::y:::", ":::z:::", NULL), ":::x::::y::::z:::"));
  TEST (NULL, str_check (g_build_path ("::", "::::x::::", "::::y::::", "::::z::::", NULL), "::::x::y::z::::"));

#define S G_DIR_SEPARATOR_S

  TEST (NULL, str_check (g_build_filename (NULL), ""));
  TEST (NULL, str_check (g_build_filename (S, NULL), S));
  TEST (NULL, str_check (g_build_filename (S"x", NULL), S"x"));
  TEST (NULL, str_check (g_build_filename ("x"S, NULL), "x"S));
  TEST (NULL, str_check (g_build_filename ("", "x", NULL), "x"));
  TEST (NULL, str_check (g_build_filename ("", S"x", NULL), S"x"));
  TEST (NULL, str_check (g_build_filename (S, "x", NULL), S"x"));
  TEST (NULL, str_check (g_build_filename (S S, "x", NULL), S S"x"));
  TEST (NULL, str_check (g_build_filename ("x", "", NULL), "x"));
  TEST (NULL, str_check (g_build_filename ("x"S, "", NULL), "x"S));
  TEST (NULL, str_check (g_build_filename ("x", S, NULL), "x"S));
  TEST (NULL, str_check (g_build_filename ("x", S S, NULL), "x"S S));
  TEST (NULL, str_check (g_build_filename ("x", "y",  NULL), "x"S"y"));
  TEST (NULL, str_check (g_build_filename (S"x", "y", NULL), S"x"S"y"));
  TEST (NULL, str_check (g_build_filename ("x", "y"S, NULL), "x"S"y"S));
  TEST (NULL, str_check (g_build_filename (S"x"S, S"y"S, NULL), S"x"S"y"S));
  TEST (NULL, str_check (g_build_filename (S"x"S S, S S"y"S, NULL), S"x"S"y"S));
  TEST (NULL, str_check (g_build_filename ("x", "", "y",  NULL), "x"S"y"));
  TEST (NULL, str_check (g_build_filename ("x", S, "y",  NULL), "x"S"y"));
  TEST (NULL, str_check (g_build_filename ("x", S S, "y",  NULL), "x"S"y"));
  TEST (NULL, str_check (g_build_filename ("x", "y", "z", NULL), "x"S"y"S"z"));
  TEST (NULL, str_check (g_build_filename (S"x"S, S"y"S, S"z"S, NULL), S"x"S"y"S"z"S));
  TEST (NULL, str_check (g_build_filename (S S"x"S S, S S"y"S S, S S"z"S S, NULL), S S"x"S"y"S"z"S S));

#ifdef G_OS_WIN32

  /* Test also using the slash as file name separator */
#define U "/"
  TEST (NULL, str_check (g_build_filename (NULL), ""));
  TEST (NULL, str_check (g_build_filename (U, NULL), U));
  TEST (NULL, str_check (g_build_filename (U"x", NULL), U"x"));
  TEST (NULL, str_check (g_build_filename ("x"U, NULL), "x"U));
  TEST (NULL, str_check (g_build_filename ("", U"x", NULL), U"x"));
  TEST (NULL, str_check (g_build_filename ("", U"x", NULL), U"x"));
  TEST (NULL, str_check (g_build_filename (U, "x", NULL), U"x"));
  TEST (NULL, str_check (g_build_filename (U U, "x", NULL), U U"x"));
  TEST (NULL, str_check (g_build_filename (U S, "x", NULL), U S"x"));
  TEST (NULL, str_check (g_build_filename ("x"U, "", NULL), "x"U));
  TEST (NULL, str_check (g_build_filename ("x"S"y", "z"U"a", NULL), "x"S"y"S"z"U"a"));
  TEST (NULL, str_check (g_build_filename ("x", U, NULL), "x"U));
  TEST (NULL, str_check (g_build_filename ("x", U U, NULL), "x"U U));
  TEST (NULL, str_check (g_build_filename ("x", S U, NULL), "x"S U));
  TEST (NULL, str_check (g_build_filename (U"x", "y", NULL), U"x"U"y"));
  TEST (NULL, str_check (g_build_filename ("x", "y"U, NULL), "x"U"y"U));
  TEST (NULL, str_check (g_build_filename (U"x"U, U"y"U, NULL), U"x"U"y"U));
  TEST (NULL, str_check (g_build_filename (U"x"U U, U U"y"U, NULL), U"x"U"y"U));
  TEST (NULL, str_check (g_build_filename ("x", U, "y",  NULL), "x"U"y"));
  TEST (NULL, str_check (g_build_filename ("x", U U, "y",  NULL), "x"U"y"));
  TEST (NULL, str_check (g_build_filename ("x", U S, "y",  NULL), "x"S"y"));
  TEST (NULL, str_check (g_build_filename ("x", S U, "y",  NULL), "x"U"y"));
  TEST (NULL, str_check (g_build_filename ("x", U "y", "z", NULL), "x"U"y"U"z"));
  TEST (NULL, str_check (g_build_filename ("x", S "y", "z", NULL), "x"S"y"S"z"));
  TEST (NULL, str_check (g_build_filename ("x", S "y", "z", U, "a", "b", NULL), "x"S"y"S"z"U"a"U"b"));
  TEST (NULL, str_check (g_build_filename (U"x"U, U"y"U, U"z"U, NULL), U"x"U"y"U"z"U));
  TEST (NULL, str_check (g_build_filename (U U"x"U U, U U"y"U U, U U"z"U U, NULL), U U"x"U"y"U"z"U U));
#endif /* G_OS_WIN32 */

#undef S

  {
    gchar buf[5];
    
    TEST (NULL, 3 == g_snprintf (buf, 0, "%s", "abc"));
    TEST (NULL, 3 == g_snprintf (NULL,0, "%s", "abc"));
    TEST (NULL, 3 == g_snprintf (buf, 5, "%s", "abc"));
    TEST (NULL, 4 == g_snprintf (buf, 5, "%s", "abcd"));
    TEST (NULL, 9 == g_snprintf (buf, 5, "%s", "abcdefghi"));
  }

  return any_failed;
}
