/* Test for CVE-2026-58013: memcmp() off the end of the buffer with long terminators
 *
 * Copyright © 2026 Philip Withnall
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <glib.h>
#include <glib/gstdio.h>
#include <string.h>

static void
test_read_line_long_terminator (void)
{
  guint8 *test_data = NULL;
  gsize test_data_len = 0;
  gint fd;
  gchar *filename = NULL;
  GIOChannel *channel = NULL;
  GError *local_error = NULL;
  gchar *line = NULL;
  gsize line_length, terminator_pos;
  const gchar *line_term;
  gint line_term_length;
  GIOStatus status;

  /* Write out a temporary file containing 2047 bytes. This is enough to make it
   * near the length of the GString buffer when read back in. */
  fd = g_file_open_tmp ("glib-test-io-channel-XXXXXX", &filename, &local_error);
  g_assert_no_error (local_error);
  g_close (fd, NULL);
  fd = -1;

  test_data_len = 2047;
  test_data = g_malloc (test_data_len);
  memset (test_data, 'M', test_data_len);
  g_file_set_contents (filename, (const gchar *) test_data, test_data_len, &local_error);
  g_assert_no_error (local_error);

  /* Create the channel. */
  channel = g_io_channel_new_file (filename, "r", &local_error);
  g_assert_no_error (local_error);

  /* Use a long line terminator so it could potentially over-read the end of the buffer. */
  g_io_channel_set_line_term (channel, "DEADBEEF", 8);

  line_term = g_io_channel_get_line_term (channel, &line_term_length);
  g_assert_cmpstr (line_term, ==, "DEADBEEF");
  g_assert_cmpint (line_term_length, ==, 8);

  g_io_channel_set_encoding (channel, "UTF-8", &local_error);
  g_assert_no_error (local_error);

  status = g_io_channel_read_line (channel, &line, &line_length,
                                   &terminator_pos, &local_error);
  g_assert_no_error (local_error);
  g_assert_cmpint (status, ==, G_IO_STATUS_NORMAL);
  g_assert_cmpuint (line_length, ==, 2047);
  g_assert_cmpuint (terminator_pos, ==, 2047);
  g_assert_cmpmem (line, line_length, test_data, test_data_len);

  g_free (line);
  g_io_channel_unref (channel);
  g_free (test_data);
  g_unlink (filename);
  g_free (filename);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/io-channel/read-line/long-terminator", test_read_line_long_terminator);

  return g_test_run ();
}
