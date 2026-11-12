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
 */

#include "config.h"

#include <glib.h>
#include <glib/gunicodeprivate.h>

static void
test_utf8_to_utf16_make_valid (void)
{
  #define MAKE_ENTRY(string, string_is_valid_utf8) \
    {string, string_is_valid_utf8, NULL, 0}
  #define MAKE_VALID_UTF8_ENTRY(string) \
    MAKE_ENTRY(string, TRUE)
  #define MAKE_INVALID_UTF8_ENTRY(string) \
    MAKE_ENTRY(string, FALSE)
  struct {
    const char *utf8;
    gboolean    utf8_is_valid;
    gunichar2  *utf16;
    glong       utf16_length;
  } strings[] = {
    MAKE_VALID_UTF8_ENTRY ("hello"),
    MAKE_INVALID_UTF8_ENTRY ("\xf4\xf4\xf4"),
    MAKE_VALID_UTF8_ENTRY ("world"),
    MAKE_INVALID_UTF8_ENTRY ("\xf4\xf4\xf4"),
    MAKE_VALID_UTF8_ENTRY ("絵文字"),
    MAKE_VALID_UTF8_ENTRY (" 🚀"),
  };
  #undef MAKE_INVALID_UTF8_ENTRY
  #undef MAKE_VALID_UTF8_ENTRY
  #undef MAKE_ENTRY

  /* Set up structures */

  GString *utf8 = g_string_sized_new (100);

  for (size_t i = 0; i < G_N_ELEMENTS (strings); i++)
    g_string_append (utf8, strings[i].utf8);

  /* Could use UTF-16 string literals beginning with C23 */
  for (size_t i = 0; i < G_N_ELEMENTS (strings); i++)
    {
      if (strings[i].utf8_is_valid)
        {
          strings[i].utf16 = g_utf8_to_utf16 (strings[i].utf8, -1, NULL,
                                              &strings[i].utf16_length, NULL);
        }
    }

  /* Actual test */

{
  gunichar2 buffer[500];
  gunichar2 *utf16;
  size_t utf16_length = 0;
  const gunichar2 *iter = buffer;

  g_utf8_to_utf16_make_valid (utf8->str, -1,
                              buffer, G_N_ELEMENTS (buffer),
                              &utf16, &utf16_length);

  /* The result fits in the passed staging buffer */
  g_assert_true (utf16 == buffer);

  for (size_t i = 0; i < G_N_ELEMENTS (strings); i++)
    {
      if (strings[i].utf8_is_valid)
        {
          size_t size = strings[i].utf16_length * sizeof (gunichar2);
          g_assert_true (memcmp (iter, strings[i].utf16, size) == 0);

          iter += strings[i].utf16_length;
        }
      else
        {
          g_assert_cmpint (*iter, ==, 0xFFFD);

          while (*iter == 0xFFFD)
            iter++;
        }
    }
  g_assert_cmpint (*iter, ==, (gunichar2) 0);

  /* Test small buffer */
  g_utf8_to_utf16_make_valid (utf8->str, -1,
                              buffer, 10,
                              &utf16, &utf16_length);

  g_assert_true (utf16 != buffer);
  g_assert_cmpint (buffer[utf16_length], == , (gunichar2) 0);
  g_assert_cmpint (utf16[utf16_length], == , (gunichar2) 0);
  g_assert_cmpmem (utf16, utf16_length, buffer, utf16_length);

  g_free (utf16);
}

  /* Clean up structures */

  g_string_free (utf8, TRUE);

  for (size_t i = 0; i < G_N_ELEMENTS (strings); i++)
    {
      g_free (strings[i].utf16);
    }
}

static void
test_utf8_to_utf16_make_valid_backtrack (void)
{
  const char *utf8 =
    "a" /* 1 byte => 1 gunichar2 */
    "α" /* 2 bytes => 1 gunichar2 */
    "⍺" /* 3 bytes => 1 gunichar2 */
    "𝐀" /* 4 bytes => 2 gunichar2 */
    "\xf4\xf4"; /* invalid sequence of 2 bytes => 2 gunichar2 */
  gunichar2 buffer[50];
  gunichar2 *utf16;
  size_t utf8_offset = 0;
  size_t utf16_offset = 0;
  size_t i;

  g_utf8_to_utf16_make_valid (utf8, -1, buffer, G_N_ELEMENTS (buffer), &utf16, NULL);

  for (i = 0; i <= 3; i++)
    {
      utf8_offset = g_utf8_to_utf16_make_valid_backtrack (utf8, -1, utf16_offset++);
      g_assert_cmpint (utf8_offset, ==, i * (i + 1) / 2);
    }

  for (i = 0; i < 2; i++)
    {
      utf8_offset = g_utf8_to_utf16_make_valid_backtrack (utf8, -1, utf16_offset++);
      g_assert_cmpint (utf8_offset, ==, 6 + 4);
    }

  for (i = 0; i < 2; i++)
    {
      utf8_offset = g_utf8_to_utf16_make_valid_backtrack (utf8, -1, utf16_offset++);
      g_assert_cmpint (utf8_offset, ==, 6 + 4 + i + 1);
    }

  /* Passing overlong offsets is safe */
  utf8_offset = g_utf8_to_utf16_make_valid_backtrack (utf8, -1, 1000);
  g_assert_cmpint (utf8_offset, ==, strlen (utf8));

  /* Edge case */
  g_assert_cmpint (g_utf8_to_utf16_make_valid_backtrack ("", -1, 0), ==, 0);

  /* No need to call g_free */
  g_assert_true (buffer == utf16);
}

static void
test_utf8_to_utf16_make_valid_length (void)
{
  /* Embedded NULs are valid characters when the length is explicit */
  const char with_nul[] = "a" "\0" "\xce\xb1" "\0" "b";
  /* Truncated sequence at the end of the explicit length, followed by
   * a valid continuation byte that must not be read */
  const char truncated[] = "a\xe2\x82\xac";
  gunichar2 buffer[50];
  gunichar2 *utf16;
  size_t utf16_len;

  g_utf8_to_utf16_make_valid (with_nul, sizeof (with_nul) - 1,
                              buffer, G_N_ELEMENTS (buffer),
                              &utf16, &utf16_len);
  g_assert_true (utf16 == buffer);
  g_assert_cmpuint (utf16_len, ==, 5);
  g_assert_cmpuint (utf16[0], ==, 'a');
  g_assert_cmpuint (utf16[1], ==, 0);
  g_assert_cmpuint (utf16[2], ==, 0x3B1);
  g_assert_cmpuint (utf16[3], ==, 0);
  g_assert_cmpuint (utf16[4], ==, 'b');
  g_assert_cmpuint (utf16[5], ==, 0);

  /* The length bounds the conversion: only the first byte of the euro sign
   * is included, so it is an incomplete sequence and becomes a single
   * replacement character */
  g_utf8_to_utf16_make_valid (truncated, 2, buffer, G_N_ELEMENTS (buffer),
                              &utf16, &utf16_len);
  g_assert_cmpuint (utf16_len, ==, 2);
  g_assert_cmpuint (utf16[0], ==, 'a');
  g_assert_cmpuint (utf16[1], ==, 0xFFFD);
  g_assert_cmpuint (utf16[2], ==, 0);

  g_utf8_to_utf16_make_valid (truncated, 0, buffer, G_N_ELEMENTS (buffer),
                              &utf16, &utf16_len);
  g_assert_cmpuint (utf16_len, ==, 0);
  g_assert_cmpuint (utf16[0], ==, 0);

  /* Backtrack across embedded NULs: 'a', NUL, alpha, NUL, 'b' */
  g_assert_cmpuint (g_utf8_to_utf16_make_valid_backtrack (with_nul, sizeof (with_nul) - 1, 1), ==, 1);
  g_assert_cmpuint (g_utf8_to_utf16_make_valid_backtrack (with_nul, sizeof (with_nul) - 1, 2), ==, 2);
  g_assert_cmpuint (g_utf8_to_utf16_make_valid_backtrack (with_nul, sizeof (with_nul) - 1, 3), ==, 4);
  g_assert_cmpuint (g_utf8_to_utf16_make_valid_backtrack (with_nul, sizeof (with_nul) - 1, 4), ==, 5);
  g_assert_cmpuint (g_utf8_to_utf16_make_valid_backtrack (with_nul, sizeof (with_nul) - 1, 100), ==, sizeof (with_nul) - 1);
}

static void
test_utf8_to_utf16_make_valid_truncated (void)
{
  /* Every case ends with an incomplete or stray sequence. The input is an
   * exact-size heap copy without a trailing NUL, so that a read past the
   * given length is caught by ASan or valgrind. */
  static const struct
  {
    const char *utf8;
    size_t len;
  } tests[] = {
    { "x" "\xc3", 2 },                     /* 2-byte sequence cut after 1 */
    { "x" "\xe2", 2 },                     /* 3-byte sequence cut after 1 */
    { "x" "\xe2\x82", 3 },                 /* 3-byte sequence cut after 2 */
    { "x" "\xf0", 2 },                     /* 4-byte sequence cut after 1 */
    { "x" "\xf0\x9f", 3 },                 /* 4-byte sequence cut after 2 */
    { "x" "\xf0\x9f\x9a", 4 },             /* 4-byte sequence cut after 3 */
    { "x" "\x80", 2 },                     /* stray continuation byte */
  };
  gunichar2 buffer[50];
  gunichar2 reference[50];
  gunichar2 *utf16;
  size_t utf16_len, reference_len;
  size_t i;
  char *copy;

  for (i = 0; i < G_N_ELEMENTS (tests); i++)
    {
      /* The NUL-terminated conversion of the same bytes is the reference */
      g_utf8_to_utf16_make_valid (tests[i].utf8, -1,
                                  reference, G_N_ELEMENTS (reference),
                                  &utf16, &reference_len);
      g_assert_true (utf16 == reference);

      copy = g_memdup2 (tests[i].utf8, tests[i].len);
      g_utf8_to_utf16_make_valid (copy, (gssize) tests[i].len,
                                  buffer, G_N_ELEMENTS (buffer),
                                  &utf16, &utf16_len);
      g_assert_cmpuint (utf16_len, ==, reference_len);
      g_assert_cmpmem (utf16, utf16_len * sizeof (gunichar2),
                       reference, reference_len * sizeof (gunichar2));
      g_assert_cmpuint (utf16[0], ==, 'x');
      g_assert_cmpuint (utf16[1], ==, 0xFFFD);
      g_assert_cmpuint (utf16[utf16_len], ==, 0);
      if (utf16 != buffer)
        g_free (utf16);
      g_free (copy);
    }

  /* Several consecutive embedded NULs */
  copy = g_memdup2 ("a" "\0\0\0" "b", 5);
  g_utf8_to_utf16_make_valid (copy, 5, buffer, G_N_ELEMENTS (buffer),
                              &utf16, &utf16_len);
  g_assert_cmpuint (utf16_len, ==, 5);
  g_assert_cmpuint (utf16[0], ==, 'a');
  g_assert_cmpuint (utf16[1], ==, 0);
  g_assert_cmpuint (utf16[2], ==, 0);
  g_assert_cmpuint (utf16[3], ==, 0);
  g_assert_cmpuint (utf16[4], ==, 'b');
  g_free (copy);
}

static void
test_utf8_to_utf16_make_valid_supplementary (void)
{
  /* An invalid byte followed by a valid character outside the BMP: the
   * conversion has to resume at the valid character instead of treating
   * the rest of the string as invalid. */
  const char *utf8 =
    "x"
    "\xf4"  /* invalid byte => U+FFFD */
    "🚀"  /* U+1F680 ROCKET => 2 gunichar2 */
    "y";
  gunichar2 buffer[50];
  gunichar2 *utf16;
  size_t utf16_len;

  g_utf8_to_utf16_make_valid (utf8, -1, buffer, G_N_ELEMENTS (buffer), &utf16, &utf16_len);

  g_assert_true (utf16 == buffer);
  g_assert_cmpuint (utf16_len, ==, 5);
  g_assert_cmpuint (utf16[0], ==, 'x');
  g_assert_cmpuint (utf16[1], ==, 0xFFFD);
  g_assert_cmpuint (utf16[2], ==, 0xD83D);
  g_assert_cmpuint (utf16[3], ==, 0xDE80);
  g_assert_cmpuint (utf16[4], ==, 'y');
  g_assert_cmpuint (utf16[5], ==, 0);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/utf8/utf8-to-utf16-make-valid", test_utf8_to_utf16_make_valid);
  g_test_add_func ("/utf8/utf8-to-utf16-make-valid-backtrack", test_utf8_to_utf16_make_valid_backtrack);
  g_test_add_func ("/utf8/utf8-to-utf16-make-valid-supplementary", test_utf8_to_utf16_make_valid_supplementary);
  g_test_add_func ("/utf8/utf8-to-utf16-make-valid-length", test_utf8_to_utf16_make_valid_length);
  g_test_add_func ("/utf8/utf8-to-utf16-make-valid-truncated", test_utf8_to_utf16_make_valid_truncated);

  return g_test_run();
}
