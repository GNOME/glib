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
test_peek (void)
{
  GInputStream *base;
  GInputStream *in;
  gssize npeek;
  char *buffer;

  base = g_memory_input_stream_new_from_data ("abcdefghijk", -1, NULL);
  in = g_buffered_input_stream_new_sized (base, 64);

  g_buffered_input_stream_fill (G_BUFFERED_INPUT_STREAM (in), strlen ("abcdefghijk"), NULL, NULL);

  buffer = g_new0 (char, 64);
  npeek = g_buffered_input_stream_peek (G_BUFFERED_INPUT_STREAM (in), buffer, 2, 3);
  g_assert_cmpint (npeek, ==, 3);
  g_assert_cmpstr ("cde", ==, buffer);
  g_free (buffer);

  buffer = g_new0 (char, 64);
  npeek = g_buffered_input_stream_peek (G_BUFFERED_INPUT_STREAM (in), buffer, 9, 5);
  g_assert_cmpint (npeek, ==, 2);
  g_assert_cmpstr ("jk", ==, buffer);
  g_free (buffer);

  buffer = g_new0 (char, 64);
  npeek = g_buffered_input_stream_peek (G_BUFFERED_INPUT_STREAM (in), buffer, 75, 3);
  g_assert_cmpint (npeek, ==, 0);
  g_free (buffer);

  g_object_unref (in);
}

static void
test_peek_buffer (void)
{
  GInputStream *base;
  GInputStream *in;
  gssize nfill;
  gsize bufsize;
  char *buffer;

  base = g_memory_input_stream_new_from_data ("abcdefghijk", -1, NULL);
  in = g_buffered_input_stream_new (base);

  nfill = g_buffered_input_stream_fill (G_BUFFERED_INPUT_STREAM (in), strlen ("abcdefghijk"), NULL, NULL);
  buffer = (char *) g_buffered_input_stream_peek_buffer (G_BUFFERED_INPUT_STREAM (in), &bufsize);
  g_assert_cmpint (nfill, ==, bufsize);
  g_assert (0 == strncmp ("abcdefghijk", buffer, bufsize));

  g_object_unref (in);
}

static void
test_set_buffer_size (void)
{
  GInputStream *base;
  GInputStream *in;
  gsize size, bufsize;

  base = g_memory_input_stream_new_from_data ("abcdefghijk", -1, NULL);
  in = g_buffered_input_stream_new (base);

  g_buffered_input_stream_set_buffer_size (G_BUFFERED_INPUT_STREAM (in), 64);
  size = g_buffered_input_stream_get_buffer_size (G_BUFFERED_INPUT_STREAM (in));
  g_assert_cmpint (size, ==, 64);

  /* size cannot shrink below current content len */
  g_buffered_input_stream_fill (G_BUFFERED_INPUT_STREAM (in), strlen ("abcdefghijk"), NULL, NULL);
  g_buffered_input_stream_peek_buffer (G_BUFFERED_INPUT_STREAM (in), &bufsize);
  g_buffered_input_stream_set_buffer_size (G_BUFFERED_INPUT_STREAM (in), 2);
  size = g_buffered_input_stream_get_buffer_size (G_BUFFERED_INPUT_STREAM (in));
  g_assert_cmpint (size, ==, bufsize);

  g_object_unref (in);
}

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

  g_object_unref (in);
}

int
main (int   argc,
      char *argv[])
{
  g_type_init ();
  g_test_init (&argc, &argv, NULL);
  g_test_bug_base ("http://bugzilla.gnome.org/");

  g_test_add_func ("/buffered-input-stream/peek", test_peek);
  g_test_add_func ("/buffered-input-stream/peek-buffer", test_peek_buffer);
  g_test_add_func ("/buffered-input-stream/set-buffer-size", test_set_buffer_size);
  g_test_add_func ("/buffered-input-stream/read-byte", test_read_byte);

  return g_test_run();
}
