/*
 * Copyright © 2025 Luca Bacci
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
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Luca Bacci <luca.bacci@outlook.com>
 */

#include "glib.h"
#include "gprintf.h"

#include <glib/gstdio.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#endif

#ifdef _WIN32

/* Creates a console screen buffer and wraps it in a FILE stream. Closing the
 * stream closes the handle too. The console output codepage is set to a
 * limited (non-UTF-8) one as the very last step: it is console-wide and
 * outlives the process, so the caller must restore it (see
 * read_console_cells()) before running any assertion that can abort. */
static FILE *
open_console_stream (HANDLE *out_handle,
                     UINT   *out_previous_codepage)
{
  const DWORD access_rights = GENERIC_READ | GENERIC_WRITE;
  const DWORD share_mode = FILE_SHARE_READ | FILE_SHARE_WRITE;
  const DWORD flags = CONSOLE_TEXTMODE_BUFFER;
  HANDLE handle;
  int fd;
  FILE *stream;

  /* A process can be attached to at most one console. If the calling process is
   * already attached to a console, the error code returned is ERROR_ACCESS_DENIED
   */
  g_assert_true (AllocConsole () || GetLastError () == ERROR_ACCESS_DENIED);

  g_assert_true (IsValidCodePage (1252));

  handle = CreateConsoleScreenBuffer (access_rights, share_mode, NULL, flags, NULL);
  g_assert_true (handle != INVALID_HANDLE_VALUE);

  fd = _open_osfhandle ((intptr_t)handle, _O_BINARY);
  g_assert_cmpint (fd, >=, 0);

  stream = _fdopen (fd, "w");
  g_assert_nonnull (stream);

  /* Set a limited codepage */
  *out_previous_codepage = GetConsoleOutputCP ();
  SetConsoleOutputCP (1252);

  *out_handle = handle;
  return stream;
}

/* Reads back the console cells and returns them as a NUL-terminated
 * UTF-16 string, or NULL on failure. Free with g_free().
 * Restores the console output codepage right away, so that a failing
 * assertion in the caller does not leave the console in a modified state. */
static wchar_t *
read_console_cells (HANDLE handle,
                    DWORD  length_cells,
                    UINT   restore_codepage,
                    DWORD *out_read)
{
  wchar_t *aux = g_new (wchar_t, length_cells + 1);
  BOOL ret;

  ret = ReadConsoleOutputCharacter (handle,
                                    aux,
                                    length_cells,
                                    (COORD){0, 0},
                                    out_read);

  SetConsoleOutputCP (restore_codepage);

  if (!ret)
    {
      g_free (aux);
      return NULL;
    }

  aux[*out_read] = L'\0';
  return aux;
}

#endif

static void
test_print_console (void)
{
#ifdef _WIN32
  UINT previous_codepage = CP_UTF8;
  HANDLE handle;
  FILE *stream;
  const char *utf8;
  wchar_t *utf16;
  DWORD length_cells;
  wchar_t *aux;
  DWORD aux_read;
  gboolean write_ok;

  /* Use only characters in the Basic Multilingual Plane (BMP).
   * We can write any UTF-8 string, but the read-back API used
   * in this test only supports UCS2 */
  utf8 = "\u00E0"    /* Latin Small Letter A with Grave */
         "\u03B1"    /* Greek Small Letter Alpha */
         "\u3041";   /* Hiragana Letter Small A */

  /* Do everything that can abort before touching the console codepage */
  utf16 = g_utf8_to_utf16 (utf8, -1, NULL, NULL, NULL);
  g_assert_nonnull (utf16);

  stream = open_console_stream (&handle, &previous_codepage);

  g_fprintf (stream, "%s", utf8);

  write_ok = (fflush (stream) == 0 && !ferror (stream));

  /* CJK characters usually are "wide", they take the space of
   * two cells (see wcwidth()). Probably newer terminals use
   * one cell per grapheme cluster, but let's just multiply
   * by two to account for wide characters */
  length_cells = (DWORD)wcslen (utf16) * 2 + 1;
  aux = read_console_cells (handle, length_cells, previous_codepage, &aux_read);

  g_assert_true (write_ok);
  g_assert_nonnull (aux);

  if (wcsncmp (aux, utf16, wcslen (utf16)) != 0)
    {
      char *aux_utf8 = g_utf16_to_utf8 (aux, aux_read, NULL, NULL, NULL);

      g_test_fail_printf ("string `%s' was written as `%s'", utf8, aux_utf8);
      g_free (aux_utf8);
    }

  g_clear_pointer (&aux, g_free);
  g_clear_pointer (&utf16, g_free);

  /* Closes all of stream, fd, and handle */
  g_assert_cmpint (fclose (stream), !=, EOF);
#else
  g_test_skip ("Testing console output only supported on Windows");
#endif
}

/* Writes a string with an embedded NUL byte and non-ASCII characters:
 * "a", alpha (2 bytes), NUL, hiragana a (3 bytes), "b" */
static gboolean
print_embedded_nul (FILE *stream)
{
  int ret;

  ret = g_fprintf (stream, "%s%c%s", "a" "\u03B1", 0, "\u3041" "b");

  /* 1 + 2 bytes, NUL, 3 + 1 bytes */
  return ret == 8 && fflush (stream) == 0 && !ferror (stream);
}

static void
test_print_console_embedded_nul (void)
{
#ifdef _WIN32
  UINT previous_codepage = CP_UTF8;
  HANDLE handle;
  FILE *stream;
  /* Expected cells, ignoring the cell for the embedded NUL */
  const wchar_t expected[] = L"a" L"\u03B1" L"\u3041" L"b";
  wchar_t *aux;
  DWORD aux_read;
  gboolean write_ok;
  size_t matched = 0;
  DWORD i;

  g_test_bug ("https://gitlab.gnome.org/GNOME/glib/-/issues/3761");
  g_test_summary ("Test that console output does not stop at embedded NUL bytes");

  stream = open_console_stream (&handle, &previous_codepage);

  write_ok = print_embedded_nul (stream);

  aux = read_console_cells (handle, 16, previous_codepage, &aux_read);

  g_assert_true (write_ok);
  g_assert_nonnull (aux);

  /* The data after the NUL must have been written. The console may render
   * the NUL itself as a blank cell, store it as a NUL cell or drop it, and
   * wide characters may take two cells, so just check that the expected
   * characters appear in order, with anything in between. */
  g_assert_cmpuint (aux_read, >=, 2);
  g_assert_cmpuint (aux[0], ==, L'a');
  g_assert_cmpuint (aux[1], ==, 0x3B1);
  matched = 2;

  for (i = 2; i < aux_read && matched < G_N_ELEMENTS (expected) - 1; i++)
    {
      if (aux[i] == expected[matched])
        matched++;
    }
  g_assert_cmpuint (matched, ==, G_N_ELEMENTS (expected) - 1);

  g_free (aux);

  /* Closes all of stream, fd, and handle */
  g_assert_cmpint (fclose (stream), !=, EOF);
#else
  g_test_skip ("Testing console output only supported on Windows");
#endif
}

static void
test_fprintf_embedded_nul (void)
{
  GError *error = NULL;
  char *filename = NULL;
  char *contents = NULL;
  gsize length = 0;
  FILE *stream;
  int fd;
  const char expected[] = {'a', (char) 0xCE, (char) 0xB1, 0,
                           (char) 0xE3, (char) 0x81, (char) 0x81, 'b'};

  g_test_bug ("https://gitlab.gnome.org/GNOME/glib/-/issues/3761");
  g_test_summary ("Test that g_fprintf() does not truncate output at embedded NUL bytes");

  fd = g_file_open_tmp ("print-nul-XXXXXX", &filename, &error);
  g_assert_no_error (error);
  g_assert_cmpint (fd, >=, 0);
  g_close (fd, &error);
  g_assert_no_error (error);

  stream = g_fopen (filename, "wb");
  g_assert_nonnull (stream);

  g_assert_true (print_embedded_nul (stream));
  g_assert_cmpint (fclose (stream), ==, 0);

  g_file_get_contents (filename, &contents, &length, &error);
  g_assert_no_error (error);
  g_assert_cmpuint (length, ==, sizeof (expected));
  g_assert_cmpmem (contents, length, expected, sizeof (expected));

  g_free (contents);
  g_remove (filename);
  g_free (filename);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/print-embedded-nul", test_fprintf_embedded_nul);

  g_test_add_func ("/print-console", test_print_console);
  g_test_add_func ("/print-console-embedded-nul", test_print_console_embedded_nul);

  return g_test_run ();
}
