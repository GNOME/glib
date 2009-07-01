/* GLib testing framework examples and tests
 * Copyright (C) 2008 Red Hat, Inc.
 * Authors: Matthias Clasen <mclasen@redhat.com>
 *
 * This work is provided "as is"; redistribution and modification
 * in whole or in part, in any medium, physical or electronic is
 * permitted without restriction.
 *
 * This work is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * In no event shall the authors or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even
 * if advised of the possibility of such damage.
 */

#include <glib/glib.h>
#include <gio/gio.h>
#include <stdlib.h>
#include <string.h>

static void
test_read_byte (void)
{
  GInputStream *base;
  GInputStream *in;

  g_test_bug ("562393");

  base = g_memory_input_stream_new_from_data ("abcdefghijk", -1, NULL);
  in = g_buffered_input_stream_new (base);

  g_assert_cmpint (g_buffered_input_stream_read_byte (G_BUFFERED_INPUT_STREAM (in), NULL, NULL), ==, 'a');
  g_assert_cmpint (g_buffered_input_stream_read_byte (G_BUFFERED_INPUT_STREAM (in), NULL, NULL), ==, 'b');
  g_assert_cmpint (g_buffered_input_stream_read_byte (G_BUFFERED_INPUT_STREAM (in), NULL, NULL), ==, 'c');

  g_assert_cmpint (g_input_stream_skip (in, 3, NULL, NULL), ==, 3);
  
  g_assert_cmpint (g_buffered_input_stream_read_byte (G_BUFFERED_INPUT_STREAM (in), NULL, NULL), ==, 'g');
}


int
main (int   argc,
      char *argv[])
{
  g_type_init ();
  g_test_init (&argc, &argv, NULL);
  g_test_bug_base ("http://bugzilla.gnome.org/");

  g_test_add_func ("/buffered-input-stream/read-byte", test_read_byte);

  return g_test_run();
}
