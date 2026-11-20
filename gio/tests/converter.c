/* GLib testing framework examples and tests
 * Copyright (C) 2024 Red Hat, Inc.
 * Authors: Matthias Clasen
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
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
 */

#include <glib/glib.h>
#include <gio/gio.h>
#include <stdlib.h>
#include <string.h>

static void
test_charset_unsupported (void)
{
  GCharsetConverter *converter;
  GError *error = NULL;

  converter = g_charset_converter_new ("UTF-8",
                                       "this-charset-does-not-exist",
                                       &error);
  g_assert_null (converter);
  g_assert_nonnull (error);
  g_assert_cmpuint (error->domain, ==, G_IO_ERROR);
  g_clear_error (&error);
}

static void
test_charset_cancellable (void)
{
  GCharsetConverter *converter;
  GCancellable *cancellable;
  GError *error = NULL;

  cancellable = g_cancellable_new ();
  g_cancellable_cancel (cancellable);
  converter = g_initable_new (G_TYPE_CHARSET_CONVERTER,
                              cancellable,
                              &error,
                              "to-charset", "UTF-8",
                              "from-charset", "UTF-8",
                              NULL);
  g_assert_null (converter);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED);

  g_clear_error (&error);
  g_object_unref (cancellable);
}

static void
test_extra_bytes_at_end (void)
{
  char data[1024];
  gsize size;
  GBytes *bytes;
  GConverter *converter;
  GError *error = NULL;
  GBytes *result;

  /* Create some simple data to encode */
  data[0] = 0;
  bytes = g_bytes_new_static (data, 1);

  /* encode the data */
  converter = G_CONVERTER (g_zlib_compressor_new (G_ZLIB_COMPRESSOR_FORMAT_GZIP, 9));
  result = g_converter_convert_bytes (converter, bytes, &error);
  g_assert_no_error (error);
  g_assert_nonnull (result);
  g_bytes_unref (bytes);
  g_clear_object (&converter);

  /* Append a 0 byte to the encoded data */
  size = g_bytes_get_size (result);
  g_assert_cmpuint (size, <, G_N_ELEMENTS (data)); /* just to be very sure */
  memcpy (data, g_bytes_get_data (result, NULL), size);
  data[size] = 0;
  bytes = g_bytes_new_static (data, size + 1);
  g_bytes_unref (result);

  /* Decompress the just compressed bytes with the extra 0 */
  converter = G_CONVERTER (g_zlib_decompressor_new (G_ZLIB_COMPRESSOR_FORMAT_GZIP));
  result = g_converter_convert_bytes (converter, bytes, &error);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_MESSAGE_TOO_LARGE);
  g_assert_null (result);
  g_clear_error (&error);

  g_object_unref (converter);
  g_bytes_unref (bytes);
}

static void
test_convert_bytes (void)
{
  char data[8192];
  GBytes *bytes;
  GConverter *converter;
  GError *error = NULL;
  GBytes *result;

  for (gsize i = 0; i < sizeof (data); i++)
    data[i] = g_test_rand_int_range (0, 256);

  bytes = g_bytes_new_static (data, sizeof (data));

  converter = G_CONVERTER (g_zlib_compressor_new (G_ZLIB_COMPRESSOR_FORMAT_GZIP, 9));
  result = g_converter_convert_bytes (converter, bytes, &error);
  g_assert_no_error (error);
  g_assert_nonnull (result);

  g_bytes_unref (result);
  g_object_unref (converter);

  converter = G_CONVERTER (g_zlib_decompressor_new (G_ZLIB_COMPRESSOR_FORMAT_GZIP));
  result = g_converter_convert_bytes (converter, bytes, &error);
  g_assert_nonnull (error);
  g_error_free (error);
  g_assert_true (result == NULL);

  g_object_unref (converter);
  g_bytes_unref (bytes);
}

static void
test_gzip_os_property (void)
{
  const int os_testvalue = 42;
  const char *data = "Hello World";
  GBytes *bytes;
  GZlibCompressor *compressor;
  GError *error = NULL;
  GBytes *result;
  int os;
  const char *encoded;

  bytes = g_bytes_new_static (data, sizeof (data));

  compressor = g_zlib_compressor_new (G_ZLIB_COMPRESSOR_FORMAT_GZIP, 9);
  g_zlib_compressor_set_os (compressor, os_testvalue);
  g_assert_cmpint (g_zlib_compressor_get_os (compressor), ==, os_testvalue);

  g_object_get (compressor, "os", &os, NULL);
  g_assert_cmpint (os, ==, os_testvalue);

  result = g_converter_convert_bytes (G_CONVERTER (compressor), bytes, &error);
  g_assert_no_error (error);
  g_assert_nonnull (result);

  /* The OS is stored in the 10th byte of the header, see RFC 1952 */
  g_assert_cmpint (g_bytes_get_size (result), >, 10);
  encoded = g_bytes_get_data (result, NULL);
  g_assert_cmpint (encoded[9], ==, os_testvalue);

  g_bytes_unref (result);
  g_object_unref (compressor);
  g_bytes_unref (bytes);
}

static void
test_zlib_compressor_flush_small_buffer (void)
{
  const GZlibCompressorFormat formats[] = {
    G_ZLIB_COMPRESSOR_FORMAT_ZLIB,
    G_ZLIB_COMPRESSOR_FORMAT_GZIP,
    G_ZLIB_COMPRESSOR_FORMAT_RAW,
  };
  const char data[] = "Hello World, Hello World, Hello World";

  g_test_summary ("Test that flushing a GZlibCompressor into an output buffer "
                  "that is too small fails with G_IO_ERROR_NO_SPACE instead of "
                  "never returning G_CONVERTER_FLUSHED");

  for (gsize i = 0; i < G_N_ELEMENTS (formats); i++)
    {
      GZlibCompressor *compressor;
      GConverterResult res;
      GError *error = NULL;
      guint8 out[64];
      gsize bytes_read = 0, bytes_written = 0;
      guint n_calls = 0;

      compressor = g_zlib_compressor_new (formats[i], -1);

      res = g_converter_convert (G_CONVERTER (compressor),
                                 data, sizeof (data), out, sizeof (out),
                                 G_CONVERTER_NO_FLAGS,
                                 &bytes_read, &bytes_written, &error);
      g_assert_no_error (error);
      g_assert_cmpint (res, ==, G_CONVERTER_CONVERTED);
      g_assert_cmpuint (bytes_read, ==, sizeof (data));

      /* zlib needs more than six bytes of output space for a sync flush. */
      res = g_converter_convert (G_CONVERTER (compressor),
                                 NULL, 0, out, 1, G_CONVERTER_FLUSH,
                                 &bytes_read, &bytes_written, &error);
      g_assert_error (error, G_IO_ERROR, G_IO_ERROR_NO_SPACE);
      g_assert_cmpint (res, ==, G_CONVERTER_ERROR);
      g_clear_error (&error);

      /* With a large enough buffer the flush completes. */
      do
        {
          res = g_converter_convert (G_CONVERTER (compressor),
                                     NULL, 0, out, 7, G_CONVERTER_FLUSH,
                                     &bytes_read, &bytes_written, &error);
          g_assert_no_error (error);
          n_calls++;
        }
      while (res == G_CONVERTER_CONVERTED && n_calls < 100);

      g_assert_cmpint (res, ==, G_CONVERTER_FLUSHED);

      g_object_unref (compressor);
    }
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/converter/charset-unsupported", test_charset_unsupported);
  g_test_add_func ("/converter/charset-cancellable", test_charset_cancellable);
  g_test_add_func ("/converter/bytes", test_convert_bytes);
  g_test_add_func ("/converter/extra-bytes-at-end", test_extra_bytes_at_end);
  g_test_add_func ("/converter/gzip-os-property", test_gzip_os_property);
  g_test_add_func ("/converter/zlib-compressor/flush-small-buffer", test_zlib_compressor_flush_small_buffer);

  return g_test_run();
}
