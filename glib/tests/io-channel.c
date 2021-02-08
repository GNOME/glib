/* GLib testing framework examples and tests
 *
 * Copyright © 2020 Endless Mobile, Inc.
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
 * Author: Philip Withnall <withnall@endlessm.com>
 */

#include <glib.h>
#include <glib/gstdio.h>

static void
test_read_line_embedded_nuls (void)
{
  const guint8 test_data[] = { 'H', 'i', '!', '\0', 'y', 'o', 'u', '\n', ':', ')', '\n' };
  gint fd;
  gchar *filename = NULL;
  GIOChannel *channel = NULL;
  GError *local_error = NULL;
  gchar *line = NULL;
  gsize line_length, terminator_pos;
  GIOStatus status;

  g_test_summary ("Test that reading a line containing embedded nuls works "
                  "when using non-standard line terminators.");

  /* Write out a temporary file. */
  fd = g_file_open_tmp ("glib-test-io-channel-XXXXXX", &filename, &local_error);
  g_assert_no_error (local_error);
  g_close (fd, NULL);
  fd = -1;

  g_file_set_contents (filename, (const gchar *) test_data, sizeof (test_data), &local_error);
  g_assert_no_error (local_error);

  /* Create the channel. */
  channel = g_io_channel_new_file (filename, "r", &local_error);
  g_assert_no_error (local_error);

  /* Only break on newline characters, not nuls.
   * Use length -1 here to exercise glib#2323; the case where length > 0
   * is covered in glib/tests/protocol.c. */
  g_io_channel_set_line_term (channel, "\n", -1);
  g_io_channel_set_encoding (channel, NULL, &local_error);
  g_assert_no_error (local_error);

  status = g_io_channel_read_line (channel, &line, &line_length,
                                   &terminator_pos, &local_error);
  g_assert_no_error (local_error);
  g_assert_cmpint (status, ==, G_IO_STATUS_NORMAL);
  g_assert_cmpuint (line_length, ==, 8);
  g_assert_cmpuint (terminator_pos, ==, 7);
  g_assert_cmpmem (line, line_length, test_data, 8);

  g_free (line);
  g_io_channel_unref (channel);
  g_free (filename);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/io-channel/read-line/embedded-nuls", test_read_line_embedded_nuls);

  return g_test_run ();
}
