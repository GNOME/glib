/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright (C) 2009 Paolo Borelli
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
 * Author: Paolo Borelli <pborelli@gnome.org>
 */

#include <glib/glib.h>
#include <gio/gio.h>
#include <string.h>

static void
do_test_read (const char *str, gssize expected_nread, glong expected_nchar)
{
  GInputStream *base;
  GInputStream *in;
  gssize len, n;
  char *buf;
  GError *err;

  len = strlen (str);

  base = g_memory_input_stream_new_from_data (str, -1, NULL);
  in = g_utf8_input_stream_new (base);
  g_object_unref (base);

  buf = g_new0 (char, strlen(str));
  err = NULL;
  n = g_input_stream_read (in, buf, len, NULL, &err);
  g_assert_cmpint (n, ==, expected_nread);
  if (expected_nread < 0)
    {
      g_assert_error (err, G_IO_ERROR, G_IO_ERROR_INVALID_DATA);
    }
  else
    {
      g_assert_cmpstr (str, ==, buf);
      g_assert_cmpint (g_utf8_strlen (buf, -1), ==, expected_nchar);
      g_assert (err == NULL);
    }
  g_free (buf);

  g_object_unref (in);
}

static void
do_test_read_partial (const char *str,
                      gssize chunk_len,
                      gssize expected_nread1,
                      gssize expected_nread2,
                      glong expected_nchar)
{
  GInputStream *base;
  GInputStream *in;
  gssize len, n1, n2;
  char *buf;
  GError *err;

  len = strlen (str);

  base = g_memory_input_stream_new_from_data (str, -1, NULL);
  in = g_utf8_input_stream_new (base);
  g_object_unref (base);

  buf = g_new0 (char, strlen(str));
  err = NULL;
  n1 = g_input_stream_read (in, buf, chunk_len, NULL, &err);
  g_assert_cmpint (n1, ==, expected_nread1);
  g_assert (err == NULL);

  n2 = g_input_stream_read (in, buf + n1, len - n1, NULL, &err);
  g_assert_cmpint (n2, ==, expected_nread2);
  if (expected_nread2 < 0)
    {
      g_assert_error (err, G_IO_ERROR, G_IO_ERROR_INVALID_DATA);
    }
  else
    {
      g_assert_cmpstr (str, ==, buf);
      g_assert_cmpint (g_utf8_strlen (buf, -1), ==, expected_nchar);
      g_assert (err == NULL);
    }
  g_free (buf);

  g_object_unref (in);
}

static void
test_read_ascii (void)
{
  do_test_read ("foobar", 6, 6);
}

static void
test_read_utf8 (void)
{
  do_test_read ("foobar\xc3\xa8\xc3\xa8\xc3\xa8zzzzzz", 18, 15);
}

static void
test_read_utf8_partial (void)
{
  do_test_read_partial ("foobar\xc3\xa8\xc3\xa8\xc3\xa8zzzzzz", 7, 6, 12, 15);
}

static void
test_read_invalid_start (void)
{
  do_test_read ("\xef\xbf\xbezzzzzz", -1, -1);
}

static void
test_read_invalid_middle (void)
{
  do_test_read ("foobar\xef\xbf\xbezzzzzz", -1, -1);
}

static void
test_read_invalid_end (void)
{
  do_test_read ("foobar\xef\xbf\xbe", -1, -1);
}

static void
test_read_invalid_partial (void)
{
  do_test_read_partial ("foobar\xef\xbf\xbezzzzzz", 7, 6, -1, -1);
}

static void
test_read_small_valid (void)
{
  GInputStream *base;
  GInputStream *in;
  gssize len, n;
  char *buf;
  GError *err;

  base = g_memory_input_stream_new_from_data ("\xc3\xa8\xc3\xa8", -1, NULL);
  in = g_utf8_input_stream_new (base);
  g_object_unref (base);

  len = strlen("\xc3\xa8\xc3\xa8");
  buf = g_new0 (char, len);
  err = NULL;

  /* read a single byte */
  n = g_input_stream_read (in, buf, 1, NULL, &err);
  g_assert_cmpint (n, ==, 1);
  g_assert_cmpstr ("\xc3", ==, buf);
  g_assert (err == NULL);

  /* read the rest */
  n = g_input_stream_read (in, buf + n, len - n, NULL, &err);
  g_assert_cmpint (n, ==, len - 1);
  g_assert_cmpstr ("\xc3\xa8\xc3\xa8", ==, buf);
  g_assert (err == NULL);

  g_object_unref (in);
}

static void
test_read_small_invalid (void)
{
  GInputStream *base;
  GInputStream *in;
  gssize n;
  char *buf;
  GError *err;

  base = g_memory_input_stream_new_from_data ("\xbf\xbe", -1, NULL);
  in = g_utf8_input_stream_new (base);
  g_object_unref (base);

  buf = g_new0 (char, 2);
  err = NULL;
  n = g_input_stream_read (in, buf, 1, NULL, &err);
  g_assert_cmpint (n, ==, -1);
  g_assert_error (err, G_IO_ERROR, G_IO_ERROR_INVALID_DATA);

  g_object_unref (in);
}

static void
test_read_small_consecutive (void)
{
  GInputStream *base;
  GInputStream *in;
  gssize len, n;
  char *buf;
  GError *err;

  base = g_memory_input_stream_new_from_data ("\xc3\xa8\xc3\xa8", -1, NULL);
  in = g_utf8_input_stream_new (base);
  g_object_unref (base);

  len = strlen("\xc3\xa8\xc3\xa8");
  buf = g_new0 (char, len);
  err = NULL;
  n = 0;

  /* read a single byte at a time */
  while (n < len)
  {
    gssize r;

    r = g_input_stream_read (in, buf + n, 1, NULL, &err);
    g_assert_cmpint (r, ==, 1);
    g_assert (err == NULL);

    n += r;
  }

  g_assert_cmpstr ("\xc3\xa8\xc3\xa8", ==, buf);

  g_object_unref (in);
}

int
main (int   argc,
      char *argv[])
{
  g_type_init ();
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/utf8-input-stream/read-ascii", test_read_ascii);
  g_test_add_func ("/utf8-input-stream/read-utf8", test_read_utf8);
  g_test_add_func ("/utf8-input-stream/read-utf8-partial", test_read_utf8_partial);
  g_test_add_func ("/utf8-input-stream/read-invalid-start", test_read_invalid_start);
  g_test_add_func ("/utf8-input-stream/read-invalid-middle", test_read_invalid_middle);
  g_test_add_func ("/utf8-input-stream/read-invalid-end", test_read_invalid_end);
  g_test_add_func ("/utf8-input-stream/read-invalid-partial", test_read_invalid_partial);
  g_test_add_func ("/utf8-input-stream/read-small-valid", test_read_small_valid);
  g_test_add_func ("/utf8-input-stream/read-small-invalid", test_read_small_invalid);
  g_test_add_func ("/utf8-input-stream/read-small-consecutive", test_read_small_consecutive);

  return g_test_run();
}
