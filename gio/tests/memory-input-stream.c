/* GLib testing framework examples and tests
 * Copyright (C) 2007 Imendio AB
 * Authors: Tim Janik
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
test_read_chunks (void)
{
  const char *data1 = "abcdefghijklmnopqrstuvwxyz";
  const char *data2 = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  const char *result = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
  char buffer[128];
  gsize bytes_read, pos, len, chunk_size;
  GError *error = NULL;
  GInputStream *stream;
  gboolean res;

  stream = g_memory_input_stream_new ();

  g_memory_input_stream_add_data (G_MEMORY_INPUT_STREAM (stream),
                                  data1, -1, NULL);  
  g_memory_input_stream_add_data (G_MEMORY_INPUT_STREAM (stream),
                                  data2, -1, NULL);  
  len = strlen (data1) + strlen (data2);

  for (chunk_size = 1; chunk_size < len - 1; chunk_size++)
    {
      pos = 0;
      while (pos < len) 
        {
          bytes_read = g_input_stream_read (stream, buffer, chunk_size, NULL, &error);
          g_assert_no_error (error);
          g_assert_cmpint (bytes_read, ==, MIN (chunk_size, len - pos));
          g_assert (strncmp (buffer, result + pos, bytes_read) == 0);

          pos += bytes_read;
        }
      
      g_assert_cmpint (pos, ==, len);
      res = g_seekable_seek (G_SEEKABLE (stream), 0, G_SEEK_SET, NULL, &error);
      g_assert_cmpint (res, ==, TRUE);
      g_assert_no_error (error);
    }
}

int
main (int   argc,
      char *argv[])
{
  g_type_init ();
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/memory-input-stream/read-chunks", test_read_chunks);

  return g_test_run();
}
