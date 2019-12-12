/* GLib testing framework examples and tests
 * Copyright (C) 2019 Руслан Ижбулатов <lrn1986@gmail.com>
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
#include <malloc.h>

#include "../giowin32-private.c"

static int
g_utf16_cmp0 (const gunichar2 *str1,
              const gunichar2 *str2)
{
  if (!str1)
    return -(str1 != str2);
  if (!str2)
    return str1 != str2;

  while (TRUE)
    {
      if (str1[0] > str2[0])
        return 1;
      else if (str1[0] < str2[0])
        return -1;
      else if (str1[0] == 0 && str2[0] == 0)
        return 0;

      str1++;
      str2++;
    }
}

#define g_assert_cmputf16(s1, cmp, s2, s1u8, s2u8) \
G_STMT_START { \
  const gunichar2 *__s1 = (s1), *__s2 = (s2); \
  if (g_utf16_cmp0 (__s1, __s2) cmp 0) ; else \
    g_assertion_message_cmpstr (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
                                #s1u8 " " #cmp " " #s2u8, s1u8, #cmp, s2u8); \
} G_STMT_END

struct {
  gsize len;
  const gunichar2 utf16[10];
  const gchar *utf8;
  const gchar *utf8_folded;
} string_cases[] = {
  {
    1,
    { 0x0020, 0x0000 },
    " ",
    " ",
  },
  {
    2,
    { 0x0020, 0xd800, 0x0000 },
    NULL,
    NULL,
  },
};

static void
test_utf16_strfuncs (void)
{
  gsize i;

  for (i = 0; i < G_N_ELEMENTS (string_cases); i++)
    {
      gsize len;
      gunichar2 *str;
      gboolean success;
      gchar *utf8;
      gchar *utf8_folded;

      len = g_utf16_len (string_cases[i].utf16);
      g_assert_cmpuint (len, ==, string_cases[i].len);

      str = g_wcsdup (string_cases[i].utf16, string_cases[i].len);
      g_assert_cmpmem (string_cases[i].utf16, len, str, len);
      g_free (str);

      str = g_wcsdup (string_cases[i].utf16, -1);
      g_assert_cmpmem (string_cases[i].utf16, len, str, len);
      g_free (str);

      success = g_utf16_to_utf8_and_fold (string_cases[i].utf16, -1, NULL, NULL);

      if (string_cases[i].utf8 == NULL)
        g_assert_false (success);
      else
        g_assert_true (success);

      utf8 = NULL;
      success = g_utf16_to_utf8_and_fold (string_cases[i].utf16, -1, &utf8, NULL);

      if (string_cases[i].utf8 != NULL)
        {
          g_assert_true (success);
          g_assert_cmpstr (string_cases[i].utf8, ==, utf8);
        }

      g_free (utf8);

      utf8 = NULL;
      utf8_folded = NULL;
      success = g_utf16_to_utf8_and_fold (string_cases[i].utf16, -1, &utf8, &utf8_folded);

      if (string_cases[i].utf8 != NULL)
        {
          g_assert_true (success);
          g_assert_cmpstr (string_cases[i].utf8_folded, ==, utf8_folded);
        }

      g_free (utf8);
      g_free (utf8_folded);
    }
}

struct {
  const char *orig;
  const char *executable;
  const char *executable_basename;
  gboolean    is_rundll32;
  const char *fixed;
} rundll32_commandlines[] = {
  {
    "%SystemRoot%\\System32\\rundll32.exe \"%ProgramFiles%\\Windows Photo Viewer\\PhotoViewer.dll\", ImageView_Fullscreen %1",
    "%SystemRoot%\\System32\\rundll32.exe",
    "rundll32.exe",
    TRUE,
    "%SystemRoot%\\System32\\rundll32.exe \"%ProgramFiles%\\Windows Photo Viewer\\PhotoViewer.dll\"  ImageView_Fullscreen %1",
  },
  {
    "%SystemRoot%\\System32/rundll32.exe \"%ProgramFiles%\\Windows Photo Viewer\\PhotoViewer.dll\", ImageView_Fullscreen %1",
    "%SystemRoot%\\System32/rundll32.exe",
    "rundll32.exe",
    TRUE,
    "%SystemRoot%\\System32/rundll32.exe \"%ProgramFiles%\\Windows Photo Viewer\\PhotoViewer.dll\"  ImageView_Fullscreen %1",
  },
  {
    "\"some path with spaces\\rundll32.exe\" \"%ProgramFiles%\\Windows Photo Viewer\\PhotoViewer.dll\", ImageView_Fullscreen %1",
    "some path with spaces\\rundll32.exe",
    "rundll32.exe",
    TRUE,
    "\"some path with spaces\\rundll32.exe\" \"%ProgramFiles%\\Windows Photo Viewer\\PhotoViewer.dll\"  ImageView_Fullscreen %1",
  },
  {
    "    \"some path with spaces\\rundll32.exe\"\"%ProgramFiles%\\Windows Photo Viewer\\PhotoViewer.dll\",ImageView_Fullscreen %1",
    "some path with spaces\\rundll32.exe",
    "rundll32.exe",
    TRUE,
    "    \"some path with spaces\\rundll32.exe\"\"%ProgramFiles%\\Windows Photo Viewer\\PhotoViewer.dll\" ImageView_Fullscreen %1",
  },
  {
    "rundll32.exe foo.bar,baz",
    "rundll32.exe",
    "rundll32.exe",
    TRUE,
    "rundll32.exe foo.bar baz",
  },
  {
    "  rundll32.exe foo.bar,baz",
    "rundll32.exe",
    "rundll32.exe",
    TRUE,
    "  rundll32.exe foo.bar baz",
  },
  {
    "rundll32.exe",
    "rundll32.exe",
    "rundll32.exe",
    FALSE,
    NULL,
  },
  {
    "rundll32.exe ,foobar",
    "rundll32.exe",
    "rundll32.exe",
    FALSE,
    NULL,
  },
  {
    "rundll32.exe   ,foobar",
    "rundll32.exe",
    "rundll32.exe",
    FALSE,
    NULL,
  },
  {
    "rundll32.exe foo.dll",
    "rundll32.exe",
    "rundll32.exe",
    FALSE,
    NULL,
  },
  {
    "rundll32.exe \"foo bar\",baz",
    "rundll32.exe",
    "rundll32.exe",
    TRUE,
    "rundll32.exe \"foo bar\" baz",
  },
  {
    "\"rundll32.exe\" \"foo bar\",baz",
    "rundll32.exe",
    "rundll32.exe",
    TRUE,
    "\"rundll32.exe\" \"foo bar\" baz",
  },
  {
    "\"rundll32.exe\" \"foo bar\",, , ,,, , ,,baz",
    "rundll32.exe",
    "rundll32.exe",
    TRUE,
    "\"rundll32.exe\" \"foo bar\" , , ,,, , ,,baz",
  },
  {
    "\"rundll32.exe\" foo.bar,,,,,,,,,baz",
    "rundll32.exe",
    "rundll32.exe",
    TRUE,
    "\"rundll32.exe\" foo.bar ,,,,,,,,baz",
  },
  {
    "\"rundll32.exe\" foo.bar baz",
    "rundll32.exe",
    "rundll32.exe",
    TRUE,
    "\"rundll32.exe\" foo.bar baz",
  },
  {
    "\"RuNdlL32.exe\" foo.bar baz",
    "RuNdlL32.exe",
    "RuNdlL32.exe",
    TRUE,
    "\"RuNdlL32.exe\" foo.bar baz",
  },
  {
    "%SystemRoot%\\System32\\rundll32.exe \"%ProgramFiles%\\Windows Photo Viewer\\PhotoViewer.dll,\" ImageView_Fullscreen %1",
    "%SystemRoot%\\System32\\rundll32.exe",
    "rundll32.exe",
    TRUE,
    "%SystemRoot%\\System32\\rundll32.exe \"%ProgramFiles%\\Windows Photo Viewer\\PhotoViewer.dll,\" ImageView_Fullscreen %1",
  },
  {
    "\"rundll32.exe\" \"foo bar,\"baz",
    "rundll32.exe",
    "rundll32.exe",
    TRUE,
    "\"rundll32.exe\" \"foo bar,\"baz",
  },
  {
    "\"rundll32.exe\" some,thing",
    "rundll32.exe",
    "rundll32.exe",
    TRUE,
    "\"rundll32.exe\" some thing",
  },
  {
    "\"rundll32.exe\" some, thing",
    "rundll32.exe",
    "rundll32.exe",
    TRUE,
    "\"rundll32.exe\" some  thing",
  },
  /* Commands with "rundll32" (without the .exe suffix) do exist,
   * but GLib currently does not recognize them, so there's no point
   * in testing these.
   */
};

static void
test_win32_rundll32_fixup (void)
{
  gsize i;

  for (i = 0; i < G_N_ELEMENTS (rundll32_commandlines); i++)
    {
      gunichar2 *argument;
      gunichar2 *expected;

      if (!rundll32_commandlines[i].is_rundll32)
        continue;

      argument = g_utf8_to_utf16 (rundll32_commandlines[i].orig, -1, NULL, NULL, NULL);
      expected = g_utf8_to_utf16 (rundll32_commandlines[i].fixed, -1, NULL, NULL, NULL);

      g_assert_nonnull (argument);
      g_assert_nonnull (expected);
      _g_win32_fixup_broken_microsoft_rundll_commandline (argument);

      g_assert_cmputf16 (argument, ==, expected, rundll32_commandlines[i].orig, rundll32_commandlines[i].fixed);

      g_free (argument);
      g_free (expected);
    }
}

static void
test_win32_extract_executable (void)
{
  gsize i;

  for (i = 0; i < G_N_ELEMENTS (rundll32_commandlines); i++)
    {
      gunichar2 *argument;
      gchar *dll_function;
      gchar *executable;
      gchar *executable_basename;
      gchar *executable_folded;
      gchar *executable_folded_basename;

      argument = g_utf8_to_utf16 (rundll32_commandlines[i].orig, -1, NULL, NULL, NULL);

      _g_win32_extract_executable (argument, NULL, NULL, NULL, NULL, &dll_function);

      if (rundll32_commandlines[i].is_rundll32)
        g_assert_nonnull (dll_function);
      else
        g_assert_null (dll_function);

      g_free (dll_function);

      executable = NULL;
      executable_basename = NULL;
      executable_folded = NULL;
      executable_folded_basename = NULL;
      _g_win32_extract_executable (argument, &executable, &executable_basename, &executable_folded, &executable_folded_basename, NULL);

      g_assert_cmpstr (rundll32_commandlines[i].executable, ==, executable);
      g_assert_cmpstr (rundll32_commandlines[i].executable_basename, ==, executable_basename);

      g_free (executable);
      g_free (executable_folded);
      g_free (argument);
    }
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/appinfo/utf16-strfuncs", test_utf16_strfuncs);
  g_test_add_func ("/appinfo/win32-extract-executable", test_win32_extract_executable);
  g_test_add_func ("/appinfo/win32-rundll32-fixup", test_win32_rundll32_fixup);

  return g_test_run ();
}

