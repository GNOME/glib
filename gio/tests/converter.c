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

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/converter/bytes", test_convert_bytes);

  return g_test_run();
}
