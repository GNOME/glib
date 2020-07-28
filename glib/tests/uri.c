/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
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
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GLib Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GLib Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GLib at ftp://ftp.gtk.org/pub/gtk/.
 */

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
} FileToUriTest;

FileToUriTest
file_to_uri_tests[] = {
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
} FileFromUriTest;

FileFromUriTest
file_from_uri_tests[] = {
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

static void
run_file_to_uri_tests (void)
{
  int i;
  gchar *res;
  GError *error;

  for (i = 0; i < G_N_ELEMENTS (file_to_uri_tests); i++)
    {
      error = NULL;
      res = g_filename_to_uri (file_to_uri_tests[i].filename,
                               file_to_uri_tests[i].hostname,
                               &error);

      if (res)
        g_assert_cmpstr (res, ==, file_to_uri_tests[i].expected_result);
      else
        g_assert_error (error, G_CONVERT_ERROR, file_to_uri_tests[i].expected_error);

      g_free (res);
      g_clear_error (&error);
    }
}

static void
run_file_from_uri_tests (void)
{
  int i;
  gchar *res;
  gchar *hostname;
  GError *error;

  for (i = 0; i < G_N_ELEMENTS (file_from_uri_tests); i++)
    {
      error = NULL;
      res = g_filename_from_uri (file_from_uri_tests[i].uri,
                                 &hostname,
                                 &error);

#ifdef G_OS_WIN32
      if (file_from_uri_tests[i].expected_filename)
        {
          gchar *p, *slash;
          p = file_from_uri_tests[i].expected_filename =
            g_strdup (file_from_uri_tests[i].expected_filename);
          while ((slash = strchr (p, '/')) != NULL)
            {
              *slash = '\\';
              p = slash + 1;
            }
        }
#endif
      if (res)
        g_assert_cmpstr (res, ==, file_from_uri_tests[i].expected_filename);
      else
        g_assert_error (error, G_CONVERT_ERROR, file_from_uri_tests[i].expected_error);
      g_assert_cmpstr (hostname, ==, file_from_uri_tests[i].expected_hostname);

      g_free (res);
      g_free (hostname);
      g_clear_error (&error);
    }
}

static gint
safe_strcmp_filename (const gchar *a, const gchar *b)
{
#ifndef G_OS_WIN32
  return g_strcmp0 (a, b);
#else
  if (!a || !b)
    return g_strcmp0 (a, b);
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
  if (a == NULL)
    a = "";
  if (b == NULL)
    b = "";
#ifndef G_OS_WIN32
  return strcmp (a, b);
#else
  if (strcmp (a, "localhost") == 0 && !*b)
    return 0;
  else
    return strcmp (a, b);
#endif
}

static void
run_file_roundtrip_tests (void)
{
  int i;
  gchar *uri, *hostname, *res;
  GError *error;

  for (i = 0; i < G_N_ELEMENTS (file_to_uri_tests); i++)
    {
      if (file_to_uri_tests[i].expected_error != 0)
        continue;

      error = NULL;
      uri = g_filename_to_uri (file_to_uri_tests[i].filename,
                               file_to_uri_tests[i].hostname,
                               &error);
      g_assert_no_error (error);

      hostname = NULL;
      res = g_filename_from_uri (uri, &hostname, &error);
      g_assert_no_error (error);

      g_assert_cmpint (safe_strcmp_filename (file_to_uri_tests[i].filename, res), ==, 0);
      g_assert_cmpint (safe_strcmp_hostname (file_to_uri_tests[i].hostname, hostname), ==, 0);
      g_free (res);
      g_free (uri);
      g_free (hostname);
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
  g_assert_cmpint (g_strv_length (uris), ==, 3);

  for (j = 0; j < 3; j++)
    g_assert_cmpstr (uris[j], ==, expected_uris[j]);

  g_strfreev (uris);

  uris = g_uri_list_extract_uris ("# just hot air\r\n# more hot air");
  g_assert_cmpint (g_strv_length (uris), ==, 0);
  g_strfreev (uris);
}

static void
test_uri_unescape_string (void)
{
  const struct
    {
      /* Inputs */
      const gchar *escaped;  /* (nullable) */
      const gchar *illegal_characters;  /* (nullable) */
      /* Outputs */
      const gchar *expected_unescaped;  /* (nullable) */
    }
  tests[] =
    {
      { "%2Babc %4F", NULL, "+abc O" },
      { "%2Babc %4F", "+", NULL },
      { "%00abc %4F", "+/", NULL },
      { "/cursors/none.png", "/", "/cursors/none.png" },
      { "/cursors%2fbad-subdir/none.png", "/", NULL },
      { "%0", NULL, NULL },
      { "%ra", NULL, NULL },
      { "%2r", NULL, NULL },
      { "Timm B\344der", NULL, "Timm B\344der" },
      { NULL, NULL, NULL },  /* actually a valid test, not a delimiter */
    };
  gsize i;

  for (i = 0; i < G_N_ELEMENTS (tests); i++)
    {
      gchar *s = NULL;

      g_test_message ("Test %" G_GSIZE_FORMAT ": %s", i, tests[i].escaped);

      s = g_uri_unescape_string (tests[i].escaped, tests[i].illegal_characters);
      g_assert_cmpstr (s, ==, tests[i].expected_unescaped);
      g_free (s);
    }
}

static void
test_uri_unescape_bytes (gconstpointer test_data)
{
  GError *error = NULL;
  gboolean use_nul_terminated = GPOINTER_TO_INT (test_data);
  const struct
    {
      /* Inputs */
      const gchar *escaped;  /* (nullable) */
      const gchar *illegal;
      /* Outputs */
      gssize expected_unescaped_len;  /* -1 => error expected */
      const guint8 *expected_unescaped;  /* (nullable) */
    }
  tests[] =
    {
      { "%00%00", NULL, 2, (const guint8 *) "\x00\x00" },
      { "/cursors/none.png", "/", 17, (const guint8 *) "/cursors/none.png" },
      { "/cursors%2fbad-subdir/none.png", "/", -1, NULL },
      { "%%", NULL, -1, NULL },
      { "%", NULL, -1, NULL },
    };
  gsize i;

  for (i = 0; i < G_N_ELEMENTS (tests); i++)
    {
      gssize escaped_len = 0;
      gchar *escaped = NULL;
      GBytes *bytes = NULL;

      g_test_message ("Test %" G_GSIZE_FORMAT ": %s", i, tests[i].escaped);

      /* The tests get run twice: once with the length unspecified, using a
       * nul-terminated string; and once with the length specified and a copy of
       * the string with the trailing nul explicitly removed (to help catch
       * buffer overflows). */
      if (use_nul_terminated)
        {
          escaped_len = -1;
          escaped = g_strdup (tests[i].escaped);
        }
      else
        {
          escaped_len = strlen (tests[i].escaped);  /* no trailing nul */
          escaped = g_memdup (tests[i].escaped, escaped_len);
        }

      bytes = g_uri_unescape_bytes (escaped, escaped_len, tests[i].illegal, &error);

      if (tests[i].expected_unescaped_len < 0)
        {
          g_assert_null (bytes);
          g_assert_error (error, G_URI_ERROR, G_URI_ERROR_MISC);
          g_clear_error (&error);
        }
      else
        {
          g_assert_no_error (error);
          g_assert_cmpmem (g_bytes_get_data (bytes, NULL),
                           g_bytes_get_size (bytes),
                           tests[i].expected_unescaped,
                           tests[i].expected_unescaped_len);
        }

      g_clear_pointer (&bytes, g_bytes_unref);
      g_free (escaped);
    }
}

static void
test_uri_unescape_segment (void)
{
  const gchar *escaped_segment = "%2Babc %4F---";
  gchar *s = NULL;

  s = g_uri_unescape_segment (escaped_segment, escaped_segment + 10, NULL);
  g_assert_cmpstr (s, ==, "+abc O");
  g_free (s);
}

static void
test_uri_escape (void)
{
  gchar *s;

  s = g_uri_escape_string ("abcdefgABCDEFG._~", NULL, FALSE);
  g_assert_cmpstr (s, ==, "abcdefgABCDEFG._~");
  g_free (s);
  s = g_uri_escape_string (":+ \\?#", NULL, FALSE);
  g_assert_cmpstr (s, ==, "%3A%2B%20%5C%3F%23");
  g_free (s);
  s = g_uri_escape_string ("a+b:c", "+", FALSE);
  g_assert_cmpstr (s, ==, "a+b%3Ac");
  g_free (s);
  s = g_uri_escape_string ("a+b:c\303\234", "+", TRUE);
  g_assert_cmpstr (s, ==, "a+b%3Ac\303\234");
  g_free (s);

  s = g_uri_escape_bytes ((guchar*)"\0\0", 2, NULL);
  g_assert_cmpstr (s, ==, "%00%00");
  g_free (s);
}

static void
test_uri_scheme (void)
{
  const gchar *s1, *s2;
  gchar *s;

  s = g_uri_parse_scheme ("ftp://ftp.gtk.org");
  g_assert_cmpstr (s, ==, "ftp");
  g_free (s);

  s = g_uri_parse_scheme ("1bad:");
  g_assert_null (s);
  s = g_uri_parse_scheme ("bad");
  g_assert_null (s);
  s = g_uri_parse_scheme ("99http://host/path");
  g_assert_null (s);
  s = g_uri_parse_scheme (".http://host/path");
  g_assert_null (s);
  s = g_uri_parse_scheme ("+http://host/path");
  g_assert_null (s);

  s1 = g_uri_peek_scheme ("ftp://ftp.gtk.org");
  g_assert_cmpstr (s1, ==, "ftp");
  s2 = g_uri_peek_scheme ("FTP://ftp.gtk.org");
  g_assert_cmpstr (s2, ==, "ftp");
  g_assert_true (s1 == s2);
  s1 = g_uri_peek_scheme ("1bad:");
  g_assert_null (s1);
  s1 = g_uri_peek_scheme ("bad");
  g_assert_null (s1);
}

typedef struct {
  const gchar *scheme;
  const gchar *userinfo;
  const gchar *host;
  gint         port;
  const gchar *path;
  const gchar *query;
  const gchar *fragment;
} UriParts;

typedef struct {
  const gchar *orig;
  const UriParts parts;
} UriAbsoluteTest;

static const UriAbsoluteTest absolute_tests[] = {
  { "foo:",
    { "foo", NULL, NULL, -1, "", NULL, NULL }
  },
  { "file:/dev/null",
    { "file", NULL, NULL, -1, "/dev/null", NULL, NULL }
  },
  { "file:///dev/null",
    { "file", NULL, "", -1, "/dev/null", NULL, NULL }
  },
  { "ftp://user@host/path",
    { "ftp", "user", "host", -1, "/path", NULL, NULL }
  },
  { "ftp://user@host:9999/path",
    { "ftp", "user", "host", 9999, "/path", NULL, NULL }
  },
  { "ftp://user:password@host/path",
    { "ftp", "user:password", "host", -1, "/path", NULL, NULL }
  },
  { "ftp://user:password@host:9999/path",
    { "ftp", "user:password", "host", 9999, "/path", NULL, NULL }
  },
  { "ftp://user:password@host",
    { "ftp", "user:password", "host", -1, "", NULL, NULL }
  },
  { "http://us%65r@host",
    { "http", "user", "host", -1, "", NULL, NULL }
  },
  { "http://us%40r@host",
    { "http", "us@r", "host", -1, "", NULL, NULL }
  },
  { "http://us%3ar@host",
    { "http", "us:r", "host", -1, "", NULL, NULL }
  },
  { "http://us%2fr@host",
    { "http", "us/r", "host", -1, "", NULL, NULL }
  },
  { "http://us%3fr@host",
    { "http", "us?r", "host", -1, "", NULL, NULL }
  },
  { "http://host?query",
    { "http", NULL, "host", -1, "", "query", NULL }
  },
  { "http://host/path?query=http%3A%2F%2Fhost%2Fpath%3Fchildparam%3Dchildvalue&param=value",
    { "http", NULL, "host", -1, "/path", "query=http://host/path?childparam=childvalue&param=value", NULL }
  },
  { "http://control-chars/%01%02%03%04%05%06%07%08%09%0A%0B%0C%0D%0E%0F%10%11%12%13%14%15%16%17%18%19%1A%1B%1C%1D%1E%1F%7F",
    { "http", NULL, "control-chars", -1, "/\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F\x7F", NULL, NULL }
  },
  { "http://space/%20",
    { "http", NULL, "space", -1, "/ ", NULL, NULL }
  },
  { "http://delims/%3C%3E%23%25%22",
    { "http", NULL, "delims", -1, "/<>#%\"", NULL, NULL }
  },
  { "http://unwise-chars/%7B%7D%7C%5C%5E%5B%5D%60",
    { "http", NULL, "unwise-chars", -1, "/{}|\\^[]`", NULL, NULL }
  },

  /* From RFC 2732 */
  { "http://[FEDC:BA98:7654:3210:FEDC:BA98:7654:3210]:80/index.html",
    { "http", NULL, "FEDC:BA98:7654:3210:FEDC:BA98:7654:3210", 80, "/index.html", NULL, NULL }
  },
  { "http://[1080:0:0:0:8:800:200C:417A]/index.html",
    { "http", NULL, "1080:0:0:0:8:800:200C:417A", -1, "/index.html", NULL, NULL }
  },
  { "http://[3ffe:2a00:100:7031::1]",
    { "http", NULL, "3ffe:2a00:100:7031::1", -1, "", NULL, NULL }
  },
  { "http://[1080::8:800:200C:417A]/foo",
    { "http", NULL, "1080::8:800:200C:417A", -1, "/foo", NULL, NULL }
  },
  { "http://[::192.9.5.5]/ipng",
    { "http", NULL, "::192.9.5.5", -1, "/ipng", NULL, NULL }
  },
  { "http://[::FFFF:129.144.52.38]:80/index.html",
    { "http", NULL, "::FFFF:129.144.52.38", 80, "/index.html", NULL, NULL }
  },
  { "http://[2010:836B:4179::836B:4179]",
    { "http", NULL, "2010:836B:4179::836B:4179", -1, "", NULL, NULL }
  },

  /* some problematic URIs that are handled differently in libsoup */
  { "http://host/path with spaces",
    { "http", NULL, "host", -1, "/path with spaces", NULL, NULL }
  },
  { "  http://host/path",
    { "http", NULL, "host", -1, "/path", NULL, NULL }
  },
  { "http://host/path  ",
    { "http", NULL, "host", -1, "/path", NULL, NULL }
  },
  { "http://host  ",
    { "http", NULL, "host", -1, "", NULL, NULL }
  },
  { "http://host:999  ",
    { "http", NULL, "host", 999, "", NULL, NULL }
  },
  { "http://host/pa\nth",
    { "http", NULL, "host", -1, "/path", NULL, NULL }
  },
  { "http:\r\n//host/path",
    { "http", NULL, "host", -1, "/path", NULL, NULL }
  },
  { "http://\thost/path",
    { "http", NULL, "host", -1, "/path", NULL, NULL }
  },

  /* Bug 594405; 0-length is different from not-present */
  { "http://host/path?",
    { "http", NULL, "host", -1, "/path", "", NULL }
  },
  { "http://host/path#",
    { "http", NULL, "host", -1, "/path", NULL, "" },
  },

  /* Bug 590524; ignore bad %-encoding */
  { "http://host/path%",
    { "http", NULL, "host", -1, "/path%", NULL, NULL }
  },
  { "http://h%ost/path",
    { "http", NULL, "h%ost", -1, "/path", NULL, NULL }
  },
  { "http://host/path%%",
    { "http", NULL, "host", -1, "/path%%", NULL, NULL }
  },
  { "http://host/path%%%",
    { "http", NULL, "host", -1, "/path%%%", NULL, NULL }
  },
  { "http://host/path%/x/",
    { "http", NULL, "host", -1, "/path%/x/", NULL, NULL }
  },
  { "http://host/path%0x/",
    { "http", NULL, "host", -1, "/path%0x/", NULL, NULL }
  },
  { "http://host/path%ax",
    { "http", NULL, "host", -1, "/path%ax", NULL, NULL }
  },

  /* GUri doesn't %-encode non-ASCII characters */
  { "http://host/p\xc3\xa4th/",
    { "http", NULL, "host", -1, "/p\xc3\xa4th/", NULL, NULL }
  },

  { "HTTP:////////////////",
    { "http", NULL, "", -1, "//////////////", NULL, NULL }
  },

  { "http://@host",
    { "http", "", "host", -1, "", NULL, NULL }
  },
  { "http://:@host",
    { "http", ":", "host", -1, "", NULL, NULL }
  },

  /* IPv6 scope ID parsing (both correct and incorrect) */
  { "http://[fe80::dead:beef%em1]/",
    { "http", NULL, "fe80::dead:beef%em1", -1, "/", NULL, NULL }
  },
  { "http://[fe80::dead:beef%25em1]/",
    { "http", NULL, "fe80::dead:beef%em1", -1, "/", NULL, NULL }
  },
  { "http://[fe80::dead:beef%10]/",
    { "http", NULL, "fe80::dead:beef%10", -1, "/", NULL, NULL }
  },

  /* ".." past top */
  { "http://example.com/..",
    { "http", NULL, "example.com", -1, "/..", NULL, NULL }
  },

  /* scheme parsing */
  { "foo0://host/path",
    { "foo0", NULL, "host", -1, "/path", NULL, NULL } },
  { "f0.o://host/path",
    { "f0.o", NULL, "host", -1, "/path", NULL, NULL } },
  { "http++://host/path",
    { "http++", NULL, "host", -1, "/path", NULL, NULL } },
  { "http-ish://host/path",
    { "http-ish", NULL, "host", -1, "/path", NULL, NULL } },

  /* IPv6 scope ID parsing (both correct and incorrect) */
  { "http://[fe80::dead:beef%em1]/",
    { "http", NULL, "fe80::dead:beef%em1", -1, "/", NULL, NULL } },
  { "http://[fe80::dead:beef%25em1]/",
    { "http", NULL, "fe80::dead:beef%em1", -1, "/", NULL, NULL } },
  { "http://[fe80::dead:beef%10]/",
    { "http", NULL, "fe80::dead:beef%10", -1, "/", NULL, NULL } },
};
static int num_absolute_tests = G_N_ELEMENTS (absolute_tests);

static void
test_uri_parsing_absolute (void)
{
  int i;

  for (i = 0; i < num_absolute_tests; i++)
    {
      const UriAbsoluteTest *test = &absolute_tests[i];
      GError *error = NULL;
      GUri *uri;

      uri = g_uri_parse (test->orig, G_URI_FLAGS_NONE, &error);
      g_assert_no_error (error);

      g_assert_cmpstr (g_uri_get_scheme (uri),   ==, test->parts.scheme);
      g_assert_cmpstr (g_uri_get_userinfo (uri), ==, test->parts.userinfo);
      g_assert_cmpstr (g_uri_get_host (uri),     ==, test->parts.host);
      g_assert_cmpint (g_uri_get_port (uri),     ==, test->parts.port);
      g_assert_cmpstr (g_uri_get_path (uri),     ==, test->parts.path);
      g_assert_cmpstr (g_uri_get_query (uri),    ==, test->parts.query);
      g_assert_cmpstr (g_uri_get_fragment (uri), ==, test->parts.fragment);

      g_uri_unref (uri);
    }
}

typedef struct {
  const gchar *orig, *resolved;
  UriParts parts;
} UriRelativeTest;

/* This all comes from RFC 3986 */
static const char *relative_test_base = "http://a/b/c/d;p?q";
static const UriRelativeTest relative_tests[] = {
  { "g:h", "g:h",
    { "g", NULL, NULL, -1, "h", NULL, NULL } },
  { "g", "http://a/b/c/g",
    { "http", NULL, "a", -1, "/b/c/g", NULL, NULL } },
  { "./g", "http://a/b/c/g",
    { "http", NULL, "a", -1, "/b/c/g", NULL, NULL } },
  { "g/", "http://a/b/c/g/",
    { "http", NULL, "a", -1, "/b/c/g/", NULL, NULL } },
  { "/g", "http://a/g",
    { "http", NULL, "a", -1, "/g", NULL, NULL } },
  { "//g", "http://g",
    { "http", NULL, "g", -1, "", NULL, NULL } },
  { "?y", "http://a/b/c/d;p?y",
    { "http", NULL, "a", -1, "/b/c/d;p", "y", NULL } },
  { "g?y", "http://a/b/c/g?y",
    { "http", NULL, "a", -1, "/b/c/g", "y", NULL } },
  { "#s", "http://a/b/c/d;p?q#s",
    { "http", NULL, "a", -1, "/b/c/d;p", "q", "s" } },
  { "g#s", "http://a/b/c/g#s",
    { "http", NULL, "a", -1, "/b/c/g", NULL, "s" } },
  { "g?y#s", "http://a/b/c/g?y#s",
    { "http", NULL, "a", -1, "/b/c/g", "y", "s" } },
  { ";x", "http://a/b/c/;x",
    { "http", NULL, "a", -1, "/b/c/;x", NULL, NULL } },
  { "g;x", "http://a/b/c/g;x",
    { "http", NULL, "a", -1, "/b/c/g;x", NULL, NULL } },
  { "g;x?y#s", "http://a/b/c/g;x?y#s",
    { "http", NULL, "a", -1, "/b/c/g;x", "y", "s" } },
  { ".", "http://a/b/c/",
    { "http", NULL, "a", -1, "/b/c/", NULL, NULL } },
  { "./", "http://a/b/c/",
    { "http", NULL, "a", -1, "/b/c/", NULL, NULL } },
  { "..", "http://a/b/",
    { "http", NULL, "a", -1, "/b/", NULL, NULL } },
  { "../", "http://a/b/",
    { "http", NULL, "a", -1, "/b/", NULL, NULL } },
  { "../g", "http://a/b/g",
    { "http", NULL, "a", -1, "/b/g", NULL, NULL } },
  { "../..", "http://a/",
    { "http", NULL, "a", -1, "/", NULL, NULL } },
  { "../../", "http://a/",
    { "http", NULL, "a", -1, "/", NULL, NULL } },
  { "../../g", "http://a/g",
    { "http", NULL, "a", -1, "/g", NULL, NULL } },
  { "", "http://a/b/c/d;p?q",
    { "http", NULL, "a", -1, "/b/c/d;p", "q", NULL } },
  { "../../../g", "http://a/g",
    { "http", NULL, "a", -1, "/g", NULL, NULL } },
  { "../../../../g", "http://a/g",
    { "http", NULL, "a", -1, "/g", NULL, NULL } },
  { "/./g", "http://a/g",
    { "http", NULL, "a", -1, "/g", NULL, NULL } },
  { "/../g", "http://a/g",
    { "http", NULL, "a", -1, "/g", NULL, NULL } },
  { "g.", "http://a/b/c/g.",
    { "http", NULL, "a", -1, "/b/c/g.", NULL, NULL } },
  { ".g", "http://a/b/c/.g",
    { "http", NULL, "a", -1, "/b/c/.g", NULL, NULL } },
  { "g..", "http://a/b/c/g..",
    { "http", NULL, "a", -1, "/b/c/g..", NULL, NULL } },
  { "..g", "http://a/b/c/..g",
    { "http", NULL, "a", -1, "/b/c/..g", NULL, NULL } },
  { "./../g", "http://a/b/g",
    { "http", NULL, "a", -1, "/b/g", NULL, NULL } },
  { "./g/.", "http://a/b/c/g/",
    { "http", NULL, "a", -1, "/b/c/g/", NULL, NULL } },
  { "g/./h", "http://a/b/c/g/h",
    { "http", NULL, "a", -1, "/b/c/g/h", NULL, NULL } },
  { "g/../h", "http://a/b/c/h",
    { "http", NULL, "a", -1, "/b/c/h", NULL, NULL } },
  { "g;x=1/./y", "http://a/b/c/g;x=1/y",
    { "http", NULL, "a", -1, "/b/c/g;x=1/y", NULL, NULL } },
  { "g;x=1/../y", "http://a/b/c/y",
    { "http", NULL, "a", -1, "/b/c/y", NULL, NULL } },
  { "g?y/./x", "http://a/b/c/g?y/./x",
    { "http", NULL, "a", -1, "/b/c/g", "y/./x", NULL } },
  { "g?y/../x", "http://a/b/c/g?y/../x",
    { "http", NULL, "a", -1, "/b/c/g", "y/../x", NULL } },
  { "g#s/./x", "http://a/b/c/g#s/./x",
    { "http", NULL, "a", -1, "/b/c/g", NULL, "s/./x" } },
  { "g#s/../x", "http://a/b/c/g#s/../x",
    { "http", NULL, "a", -1, "/b/c/g", NULL, "s/../x" } },
  { "http:g", "http:g",
    { "http", NULL, NULL, -1, "g", NULL, NULL } },
  { "http://a/../..", "http://a/",
    { "http", NULL, "a", -1, "/", NULL, NULL } }
};
static int num_relative_tests = G_N_ELEMENTS (relative_tests);

static void
test_uri_parsing_relative (void)
{
  int i;
  GUri *base, *uri;
  GError *error = NULL;
  gchar *resolved;

  base = g_uri_parse (relative_test_base, G_URI_FLAGS_NONE, &error);
  g_assert_no_error (error);

  for (i = 0; i < num_relative_tests; i++)
    {
      const UriRelativeTest *test = &relative_tests[i];
      gchar *tostring;

      uri = g_uri_parse_relative (base, test->orig, G_URI_FLAGS_NONE, &error);
      g_assert_no_error (error);

      g_assert_cmpstr (g_uri_get_scheme (uri),   ==, test->parts.scheme);
      g_assert_cmpstr (g_uri_get_userinfo (uri), ==, test->parts.userinfo);
      g_assert_cmpstr (g_uri_get_host (uri),     ==, test->parts.host);
      g_assert_cmpint (g_uri_get_port (uri),     ==, test->parts.port);
      g_assert_cmpstr (g_uri_get_path (uri),     ==, test->parts.path);
      g_assert_cmpstr (g_uri_get_query (uri),    ==, test->parts.query);
      g_assert_cmpstr (g_uri_get_fragment (uri), ==, test->parts.fragment);

      tostring = g_uri_to_string (uri);
      g_assert_cmpstr (tostring, ==, test->resolved);
      g_free (tostring);

      g_uri_unref (uri);

      resolved = g_uri_resolve_relative (relative_test_base, test->orig, G_URI_FLAGS_NONE, &error);
      g_assert_no_error (error);
      g_assert_cmpstr (resolved, ==, test->resolved);
      g_free (resolved);
    }
  uri = g_uri_parse_relative (base, "%%", G_URI_FLAGS_PARSE_STRICT, &error);
  g_assert_null (uri);
  g_assert_error (error, G_URI_ERROR, G_URI_ERROR_BAD_PATH);
  g_clear_error (&error);

  g_uri_unref (base);

  resolved = g_uri_resolve_relative (NULL, "http://a", G_URI_FLAGS_NONE, &error);
  g_assert_no_error (error);
  g_assert_cmpstr (resolved, ==, "http://a");
  g_free (resolved);

  resolved = g_uri_resolve_relative ("http://a", "b", G_URI_FLAGS_NONE, &error);
  g_assert_no_error (error);
  g_assert_cmpstr (resolved, ==, "http://a/b");
  g_free (resolved);

  resolved = g_uri_resolve_relative (NULL, "a", G_URI_FLAGS_NONE, &error);
  g_assert_null (resolved);
  g_assert_error (error, G_URI_ERROR, G_URI_ERROR_MISC);
  g_clear_error (&error);

  resolved = g_uri_resolve_relative ("../b", "a", G_URI_FLAGS_NONE, &error);
  g_assert_null (resolved);
  g_assert_error (error, G_URI_ERROR, G_URI_ERROR_MISC);
  g_clear_error (&error);

  resolved = g_uri_resolve_relative ("%%", "a", G_URI_FLAGS_NONE, &error);
  g_assert_null (resolved);
  g_assert_error (error, G_URI_ERROR, G_URI_ERROR_MISC);
  g_clear_error (&error);
}

static void
test_uri_to_string (void)
{
  GUri *uri;
  gchar *tostring;

  uri = g_uri_build (G_URI_FLAGS_NONE, "scheme", "userinfo", "host", 1234,
                     "/path", "query", "fragment");

  tostring = g_uri_to_string (uri);
  g_assert_cmpstr (tostring, ==, "scheme://userinfo@host:1234/path?query#fragment");
  g_free (tostring);

  g_uri_unref (uri);

  uri = g_uri_build_with_user (G_URI_FLAGS_NONE, "scheme", "user", "pass", "auth", "host", 1234,
                               "/path", "query", "fragment");
  tostring = g_uri_to_string (uri);
  g_assert_cmpstr (tostring, ==, "scheme://user%3Apass%3Bauth@host:1234/path?query#fragment");
  g_free (tostring);
  tostring = g_uri_to_string_partial (uri, G_URI_HIDE_USERINFO);
  g_assert_cmpstr (tostring, ==, "scheme://host:1234/path?query#fragment");
  g_free (tostring);
  tostring = g_uri_to_string_partial (uri, G_URI_HIDE_FRAGMENT);
  g_assert_cmpstr (tostring, ==, "scheme://user%3Apass%3Bauth@host:1234/path?query");
  g_free (tostring);
  g_uri_unref (uri);

  uri = g_uri_build_with_user (G_URI_FLAGS_HAS_PASSWORD|G_URI_FLAGS_HAS_AUTH_PARAMS,
                               "scheme", "user", "pass", "auth", "host", 1234,
                               "/path", "query", "fragment");
  tostring = g_uri_to_string (uri);
  g_assert_cmpstr (tostring, ==, "scheme://user:pass;auth@host:1234/path?query#fragment");
  g_free (tostring);
  tostring = g_uri_to_string_partial (uri, G_URI_HIDE_PASSWORD);
  g_assert_cmpstr (tostring, ==, "scheme://user;auth@host:1234/path?query#fragment");
  g_free (tostring);
  tostring = g_uri_to_string_partial (uri, G_URI_HIDE_AUTH_PARAMS);
  g_assert_cmpstr (tostring, ==, "scheme://user:pass@host:1234/path?query#fragment");
  g_free (tostring);
  g_uri_unref (uri);
}

static void
test_uri_build (void)
{
  GUri *uri;

  uri = g_uri_build (G_URI_FLAGS_NON_DNS, "scheme", "userinfo", "host", 1234,
                     "/path", "query", "fragment");

  /* check ref/unref */
  g_uri_ref (uri);
  g_uri_unref (uri);

  g_assert_cmpint (g_uri_get_flags (uri), ==, G_URI_FLAGS_NON_DNS);
  g_assert_cmpstr (g_uri_get_scheme (uri), ==, "scheme");
  g_assert_cmpstr (g_uri_get_userinfo (uri), ==, "userinfo");
  g_assert_cmpstr (g_uri_get_host (uri), ==, "host");
  g_assert_cmpint (g_uri_get_port (uri), ==, 1234);
  g_assert_cmpstr (g_uri_get_path (uri), ==, "/path");
  g_assert_cmpstr (g_uri_get_query (uri), ==, "query");
  g_assert_cmpstr (g_uri_get_fragment (uri), ==, "fragment");
  g_assert_cmpstr (g_uri_get_user (uri), ==, NULL);
  g_assert_cmpstr (g_uri_get_password (uri), ==, NULL);
  g_uri_unref (uri);

  uri = g_uri_build_with_user (G_URI_FLAGS_NON_DNS, "scheme", "user", "password",
                               "authparams", "host", 1234,
                               "/path", "query", "fragment");

  g_assert_cmpint (g_uri_get_flags (uri), ==, G_URI_FLAGS_NON_DNS);
  g_assert_cmpstr (g_uri_get_scheme (uri), ==, "scheme");
  g_assert_cmpstr (g_uri_get_userinfo (uri), ==, "user:password;authparams");
  g_assert_cmpstr (g_uri_get_host (uri), ==, "host");
  g_assert_cmpint (g_uri_get_port (uri), ==, 1234);
  g_assert_cmpstr (g_uri_get_path (uri), ==, "/path");
  g_assert_cmpstr (g_uri_get_query (uri), ==, "query");
  g_assert_cmpstr (g_uri_get_fragment (uri), ==, "fragment");
  g_assert_cmpstr (g_uri_get_user (uri), ==, "user");
  g_assert_cmpstr (g_uri_get_password (uri), ==, "password");
  g_assert_cmpstr (g_uri_get_auth_params (uri), ==, "authparams");
  g_uri_unref (uri);

  uri = g_uri_build_with_user (G_URI_FLAGS_ENCODED, "scheme", "user%01", "password%02",
                               "authparams%03", "host", 1234,
                               "/path", "query", "fragment");
  g_assert_cmpstr (g_uri_get_userinfo (uri), ==, "user%01:password%02;authparams%03");
  g_uri_unref (uri);

  uri = g_uri_build_with_user (G_URI_FLAGS_ENCODED, "scheme", NULL, NULL,
                               NULL, "host", 1234,
                               "/path", "query", "fragment");
  g_assert_null (g_uri_get_userinfo (uri));
  g_uri_unref (uri);
}

static void
test_uri_split (void)
{
  gchar *scheme = NULL;
  gchar *userinfo = NULL;
  gchar *user = NULL;
  gchar *pass = NULL;
  gchar *authparams = NULL;
  gchar *host = NULL;
  gchar *path = NULL;
  gchar *query = NULL;
  gchar *fragment = NULL;
  GError *error = NULL;
  gint port;

  g_uri_split ("scheme://user%3Apass%3Bauth@host:1234/path?query#fragment",
               G_URI_FLAGS_NONE,
               &scheme,
               &userinfo,
               &host,
               &port,
               &path,
               &query,
               &fragment,
               &error);
  g_assert_no_error (error);
  g_assert_cmpstr (scheme, ==, "scheme");
  g_assert_cmpstr (userinfo, ==, "user:pass;auth");
  g_assert_cmpstr (host, ==, "host");
  g_assert_cmpint (port, ==, 1234);
  g_assert_cmpstr (path, ==, "/path");
  g_assert_cmpstr (query, ==, "query");
  g_assert_cmpstr (fragment, ==, "fragment");
  g_free (scheme);
  g_free (userinfo);
  g_free (host);
  g_free (path);
  g_free (query);
  g_free (fragment);

  g_uri_split ("scheme://user%3Apass%3Bauth@h%01st:1234/path?query#fragment",
               G_URI_FLAGS_ENCODED,
               NULL,
               NULL,
               &host,
               NULL,
               NULL,
               NULL,
               NULL,
               &error);
  g_assert_no_error (error);
  g_assert_cmpstr (host, ==, "h\001st");
  g_free (host);

  g_uri_split ("scheme://@@@host:1234/path?query#fragment",
               G_URI_FLAGS_ENCODED,
               NULL,
               &userinfo,
               NULL,
               NULL,
               NULL,
               NULL,
               NULL,
               &error);
  g_assert_no_error (error);
  g_assert_cmpstr (userinfo, ==, "@@");
  g_free (userinfo);


  g_uri_split ("http://f;oo/",
               G_URI_FLAGS_NONE,
               NULL,
               NULL,
               NULL,
               NULL,
               &path,
               NULL,
               NULL,
               &error);
  g_assert_no_error (error);
  g_assert_cmpstr (path, ==, ";oo/");
  g_free (path);

  g_uri_split ("http://h%01st/path?saisons=%C3%89t%C3%A9%2Bhiver",
               G_URI_FLAGS_NONE,
               NULL,
               NULL,
               &host,
               NULL,
               NULL,
               &query,
               NULL,
               &error);
  g_assert_no_error (error);
  g_assert_cmpstr (host, ==, "h\001st");
  g_assert_cmpstr (query, ==, "saisons=Été+hiver");
  g_free (host);
  g_free (query);

  g_uri_split ("http://h%01st/path?saisons=%C3%89t%C3%A9%2Bhiver",
               G_URI_FLAGS_ENCODED_QUERY,
               NULL,
               NULL,
               &host,
               NULL,
               NULL,
               &query,
               NULL,
               &error);
  g_assert_no_error (error);
  g_assert_cmpstr (host, ==, "h\001st");
  g_assert_cmpstr (query, ==, "saisons=%C3%89t%C3%A9%2Bhiver");
  g_free (host);
  g_free (query);

  g_uri_split_with_user ("scheme://user:pass;auth@host:1234/path?query#fragment",
                         G_URI_FLAGS_HAS_AUTH_PARAMS|G_URI_FLAGS_HAS_PASSWORD,
                         NULL,
                         &user,
                         &pass,
                         &authparams,
                         NULL,
                         NULL,
                         NULL,
                         NULL,
                         NULL,
                         &error);
  g_assert_no_error (error);
  g_assert_cmpstr (user, ==, "user");
  g_assert_cmpstr (pass, ==, "pass");
  g_assert_cmpstr (authparams, ==, "auth");
  g_free (user);
  g_free (pass);
  g_free (authparams);

  g_uri_split_network ("scheme://user:pass;auth@host:1234/path?query#fragment",
                       G_URI_FLAGS_NONE,
                       NULL,
                       NULL,
                       NULL,
                       &error);
  g_assert_no_error (error);

  g_uri_split_network ("scheme://user:pass;auth@host:1234/path?query#fragment",
                       G_URI_FLAGS_NONE,
                       &scheme,
                       &host,
                       &port,
                       &error);
  g_assert_no_error (error);
  g_assert_cmpstr (scheme, ==, "scheme");
  g_assert_cmpstr (host, ==, "host");
  g_assert_cmpint (port, ==, 1234);
  g_free (scheme);
  g_free (host);

  g_uri_split_network ("%00",
                       G_URI_FLAGS_NONE, NULL, NULL, NULL, &error);
  g_assert_error (error, G_URI_ERROR, G_URI_ERROR_BAD_PATH);
  g_clear_error (&error);

  g_uri_split_network ("/a",
                       G_URI_FLAGS_NONE,
                       &scheme,
                       &host,
                       &port,
                       &error);
  g_assert_error (error, G_URI_ERROR, G_URI_ERROR_BAD_SCHEME);
  g_clear_error (&error);

  g_uri_split_network ("schme:#",
                       G_URI_FLAGS_NONE,
                       &scheme,
                       &host,
                       &port,
                       &error);
  g_assert_error (error, G_URI_ERROR, G_URI_ERROR_BAD_HOST);
  g_clear_error (&error);

  g_uri_split_network ("scheme://[]/a",
                       G_URI_FLAGS_NONE, NULL, NULL, NULL, &error);
  g_assert_error (error, G_URI_ERROR, G_URI_ERROR_BAD_HOST);
  g_clear_error (&error);

  g_uri_split_network ("scheme://user%00:pass;auth@host",
                       G_URI_FLAGS_HAS_PASSWORD|G_URI_FLAGS_HAS_AUTH_PARAMS,
                       NULL, NULL, NULL, &error);
  g_assert_error (error, G_URI_ERROR, G_URI_ERROR_BAD_USER);
  g_clear_error (&error);

  g_uri_split_network ("scheme://user:pass%00;auth@host",
                       G_URI_FLAGS_HAS_PASSWORD|G_URI_FLAGS_HAS_AUTH_PARAMS,
                       NULL, NULL, NULL, &error);
  g_assert_error (error, G_URI_ERROR, G_URI_ERROR_BAD_PASSWORD);
  g_clear_error (&error);

  g_uri_split_network ("scheme://user:pass;auth@host:1234/path?quer%00y#fragment",
                       G_URI_FLAGS_NONE,
                       NULL, NULL, NULL, &error);
  g_assert_error (error, G_URI_ERROR, G_URI_ERROR_BAD_QUERY);
  g_clear_error (&error);

  g_uri_split_network ("scheme://use%00r:pass;auth@host:1234/path",
                       G_URI_FLAGS_NONE,
                       NULL, NULL, NULL, &error);
  g_assert_error (error, G_URI_ERROR, G_URI_ERROR_BAD_USER);
  g_clear_error (&error);

  g_uri_split ("scheme://user:pass;auth@host:1234/path?query#fragm%00ent",
               G_URI_FLAGS_NONE,
               &scheme,
               &userinfo,
               &host,
               &port,
               &path,
               &query,
               &fragment,
               &error);
  g_assert_error (error, G_URI_ERROR, G_URI_ERROR_BAD_FRAGMENT);
  g_clear_error (&error);

  g_uri_split_with_user ("scheme://user:pa%x0s;auth@host:1234/path?query#fragment",
                         G_URI_FLAGS_HAS_PASSWORD|G_URI_FLAGS_PARSE_STRICT,
                         &scheme,
                         &user,
                         &pass,
                         &authparams,
                         &host,
                         &port,
                         &path,
                         &query,
                         &fragment,
                         &error);
  g_assert_error (error, G_URI_ERROR, G_URI_ERROR_BAD_PASSWORD);
  g_clear_error (&error);

  g_uri_split_with_user ("scheme://user:pass;auth%00@host",
                         G_URI_FLAGS_HAS_PASSWORD|G_URI_FLAGS_HAS_AUTH_PARAMS,
                         &scheme,
                         &user,
                         &pass,
                         &authparams,
                         &host,
                         &port,
                         &path,
                         &query,
                         &fragment,
                         &error);
  g_assert_error (error, G_URI_ERROR, G_URI_ERROR_BAD_AUTH_PARAMS);
  g_clear_error (&error);

  g_uri_split_network ("scheme://user:pass%00;auth@host",
                       G_URI_FLAGS_HAS_PASSWORD|G_URI_FLAGS_HAS_AUTH_PARAMS,
                       NULL, NULL, NULL, &error);
  g_assert_error (error, G_URI_ERROR, G_URI_ERROR_BAD_PASSWORD);
  g_clear_error (&error);

}

static void
test_uri_is_valid (void)
{
  GError *error = NULL;

  g_assert_true (g_uri_is_valid ("http://[::192.9.5.5]/ipng", G_URI_FLAGS_NONE, NULL));
  g_assert_true (g_uri_is_valid ("http://127.127.127.127/", G_URI_FLAGS_NONE, NULL));
  g_assert_true (g_uri_is_valid ("http://127.127.127.b/", G_URI_FLAGS_NONE, NULL));
  g_assert_true (g_uri_is_valid ("http://\xc3\x89XAMPLE.COM/", G_URI_FLAGS_NONE, NULL));

  g_assert_true (g_uri_is_valid ("  \r http\t://f oo  \t\n ", G_URI_FLAGS_NONE, NULL));
  g_assert_false (g_uri_is_valid ("  \r http\t://f oo  \t\n ", G_URI_FLAGS_PARSE_STRICT, &error));
  g_assert_error (error, G_URI_ERROR, G_URI_ERROR_BAD_SCHEME);
  g_clear_error (&error);

  g_assert_false (g_uri_is_valid ("http://[::192.9.5.5/ipng", G_URI_FLAGS_NONE, &error));
  g_assert_error (error, G_URI_ERROR, G_URI_ERROR_BAD_HOST);
  g_clear_error (&error);

  g_assert_false (g_uri_is_valid ("http://[fe80::dead:beef%wef%]/", G_URI_FLAGS_NONE, &error));
  g_assert_error (error, G_URI_ERROR, G_URI_ERROR_BAD_HOST);
  g_clear_error (&error);

  g_assert_false (g_uri_is_valid ("http://%00/", G_URI_FLAGS_NON_DNS, &error));
  g_assert_error (error, G_URI_ERROR, G_URI_ERROR_BAD_HOST);
  g_clear_error (&error);

  g_assert_true (g_uri_is_valid ("http://foo/", G_URI_FLAGS_NON_DNS, &error));

  g_assert_false (g_uri_is_valid ("http://%00/", G_URI_FLAGS_NONE, &error));
  g_assert_error (error, G_URI_ERROR, G_URI_ERROR_BAD_HOST);
  g_clear_error (&error);

  g_assert_false (g_uri_is_valid ("http://%30.%30.%30.%30/", G_URI_FLAGS_NONE, &error));
  g_assert_error (error, G_URI_ERROR, G_URI_ERROR_BAD_HOST);
  g_clear_error (&error);

  g_assert_false (g_uri_is_valid ("http://host:port", G_URI_FLAGS_NONE, &error));
  g_assert_error (error, G_URI_ERROR, G_URI_ERROR_BAD_PORT);
  g_clear_error (&error);

  g_assert_false (g_uri_is_valid ("http://host:65536", G_URI_FLAGS_NONE, &error));
  g_assert_error (error, G_URI_ERROR, G_URI_ERROR_BAD_PORT);
  g_clear_error (&error);

  g_assert_false (g_uri_is_valid ("http://host:6553l", G_URI_FLAGS_NONE, &error));
  g_assert_error (error, G_URI_ERROR, G_URI_ERROR_BAD_PORT);
  g_clear_error (&error);
}

static void
test_uri_parse_params (gconstpointer test_data)
{
  GError *err = NULL;
  gboolean use_nul_terminated = GPOINTER_TO_INT (test_data);
  const struct
    {
      /* Inputs */
      const gchar *uri;
      gchar *separators;
      GUriParamsFlags flags;
      /* Outputs */
      gssize expected_n_params;  /* -1 => error expected */
      /* key, value, key, value, …, limited to length 2*expected_n_params */
      const gchar *expected_param_key_values[6];
    }
  tests[] =
    {
      { "p1=foo&p2=bar;p3=baz", "&;", G_URI_PARAMS_NONE, 3, { "p1", "foo", "p2", "bar", "p3", "baz" }},
      { "p1=foo&p2=bar", "", G_URI_PARAMS_NONE, 1, { "p1", "foo&p2=bar" }},
      { "p1=foo&&P1=bar", "&", G_URI_PARAMS_NONE, -1, { NULL, }},
      { "%00=foo", "&", G_URI_PARAMS_NONE, -1, { NULL, }},
      { "p1=%00", "&", G_URI_PARAMS_NONE, -1, { NULL, }},
      { "p1=foo&P1=bar", "&", G_URI_PARAMS_CASE_INSENSITIVE, 1, { "p1", "bar", NULL, }},
      { "=%", "&", G_URI_PARAMS_NONE, 1, { "", "%", NULL, }},
      { "=", "&", G_URI_PARAMS_NONE, 1, { "", "", NULL, }},
      { "foo", "&", G_URI_PARAMS_NONE, -1, { NULL, }},
      { "foo=bar+%26+baz&saisons=%C3%89t%C3%A9%2Bhiver", "&", G_URI_PARAMS_WWW_FORM,
        2, { "foo", "bar & baz", "saisons", "Été+hiver", NULL, }},
      { "foo=bar+%26+baz&saisons=%C3%89t%C3%A9%2Bhiver", "&", G_URI_PARAMS_NONE,
        2, { "foo", "bar+&+baz", "saisons", "Été+hiver", NULL, }},
    };
  gsize i;

  for (i = 0; i < G_N_ELEMENTS (tests); i++)
    {
      GHashTable *params;
      gchar *uri = NULL;
      gssize uri_len;

      g_test_message ("URI %" G_GSIZE_FORMAT ": %s", i, tests[i].uri);

      g_assert (tests[i].expected_n_params < 0 ||
                tests[i].expected_n_params <= G_N_ELEMENTS (tests[i].expected_param_key_values) / 2);

      /* The tests get run twice: once with the length unspecified, using a
       * nul-terminated string; and once with the length specified and a copy of
       * the string with the trailing nul explicitly removed (to help catch
       * buffer overflows). */
      if (use_nul_terminated)
        {
          uri_len = -1;
          uri = g_strdup (tests[i].uri);
        }
      else
        {
          uri_len = strlen (tests[i].uri);  /* no trailing nul */
          uri = g_memdup (tests[i].uri, uri_len);
        }

      params = g_uri_parse_params (uri, uri_len, tests[i].separators, tests[i].flags, &err);

      if (tests[i].expected_n_params < 0)
        {
          g_assert_null (params);
          g_assert_error (err, G_URI_ERROR, G_URI_ERROR_MISC);
          g_clear_error (&err);
        }
      else
        {
          gsize j;

          g_assert_no_error (err);
          g_assert_cmpint (g_hash_table_size (params), ==, tests[i].expected_n_params);

          for (j = 0; j < tests[i].expected_n_params; j += 2)
            g_assert_cmpstr (g_hash_table_lookup (params, tests[i].expected_param_key_values[j]), ==,
                             tests[i].expected_param_key_values[j + 1]);
        }

      g_clear_pointer (&params, g_hash_table_unref);
      g_free (uri);
    }
}

static void
test_uri_join (void)
{
  gchar *uri = NULL;

  uri = g_uri_join_with_user (G_URI_FLAGS_NONE, "scheme", "user\001", "pass\002", "authparams\003",
                              "host", 9876, "/path", "query", "fragment");
  g_assert_cmpstr (uri, ==, "scheme://user%01:pass%02;authparams%03@host:9876/path?query#fragment");
  g_free (uri);

  uri = g_uri_join_with_user (G_URI_FLAGS_NONE, "scheme", "user\001", "pass\002", "authparams\003",
                              "::192.9.5.5", 9876, "/path", "query", "fragment");
  g_assert_cmpstr (uri, ==, "scheme://user%01:pass%02;authparams%03@[::192.9.5.5]:9876/path?query#fragment");
  g_free (uri);

  uri = g_uri_join_with_user (G_URI_FLAGS_ENCODED,
                              "scheme", "user%01", "pass%02", "authparams%03",
                              "::192.9.5.5", 9876, "/path", "query", "fragment");
  g_assert_cmpstr (uri, ==,
                   "scheme://user%01:pass%02;authparams%03@[::192.9.5.5]:9876/path?query#fragment");
  g_free (uri);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/uri/file-to-uri", run_file_to_uri_tests);
  g_test_add_func ("/uri/file-from-uri", run_file_from_uri_tests);
  g_test_add_func ("/uri/file-roundtrip", run_file_roundtrip_tests);
  g_test_add_func ("/uri/list", run_uri_list_tests);
  g_test_add_func ("/uri/unescape-string", test_uri_unescape_string);
  g_test_add_data_func ("/uri/unescape-bytes/nul-terminated", GINT_TO_POINTER (TRUE), test_uri_unescape_bytes);
  g_test_add_data_func ("/uri/unescape-bytes/length", GINT_TO_POINTER (FALSE), test_uri_unescape_bytes);
  g_test_add_func ("/uri/unescape-segment", test_uri_unescape_segment);
  g_test_add_func ("/uri/escape", test_uri_escape);
  g_test_add_func ("/uri/scheme", test_uri_scheme);
  g_test_add_func ("/uri/parsing/absolute", test_uri_parsing_absolute);
  g_test_add_func ("/uri/parsing/relative", test_uri_parsing_relative);
  g_test_add_func ("/uri/build", test_uri_build);
  g_test_add_func ("/uri/split", test_uri_split);
  g_test_add_func ("/uri/is_valid", test_uri_is_valid);
  g_test_add_func ("/uri/to-string", test_uri_to_string);
  g_test_add_func ("/uri/join", test_uri_join);
  g_test_add_data_func ("/uri/parse-params/nul-terminated", GINT_TO_POINTER (TRUE), test_uri_parse_params);
  g_test_add_data_func ("/uri/parse-params/length", GINT_TO_POINTER (FALSE), test_uri_parse_params);

  return g_test_run ();
}
