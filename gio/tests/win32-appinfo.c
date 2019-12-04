/* GLib testing framework examples and tests
 * Copyright (C) 2008 Red Hat, Inc
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

static void
test_win32_rundll32_fixup (void)
{
  guint i;

  struct {
    const char *orig;
    const char *fixed;
  } cases[] = {
    {
      "%SystemRoot%\\System32\\rundll32.exe \"%ProgramFiles%\\Windows Photo Viewer\\PhotoViewer.dll\", ImageView_Fullscreen %1",
      "%SystemRoot%\\System32\\rundll32.exe \"%ProgramFiles%\\Windows Photo Viewer\\PhotoViewer.dll\"  ImageView_Fullscreen %1",
    },
    {
      "rundll32.exe foo.bar,baz",
      "rundll32.exe foo.bar baz",
    },
    {
      "rundll32.exe \"foo bar\",baz",
      "rundll32.exe \"foo bar\" baz",
    },
    {
      "\"rundll32.exe\" \"foo bar\",baz",
      "\"rundll32.exe\" \"foo bar\" baz",
    },
    {
      "\"rundll32.exe\" \"foo bar\",, , ,,, , ,,baz",
      "\"rundll32.exe\" \"foo bar\" , , ,,, , ,,baz",
    },
    {
      "\"rundll32.exe\" foo.bar,,,,,,,,,baz",
      "\"rundll32.exe\" foo.bar ,,,,,,,,baz",
    },
    {
      "\"rundll32.exe\" foo.bar baz",
      "\"rundll32.exe\" foo.bar baz",
    },
    {
      "\"RuNdlL32.exe\" foo.bar baz",
      "\"RuNdlL32.exe\" foo.bar baz",
    },
    {
      "%SystemRoot%\\System32\\rundll32.exe \"%ProgramFiles%\\Windows Photo Viewer\\PhotoViewer.dll,\" ImageView_Fullscreen %1",
      "%SystemRoot%\\System32\\rundll32.exe \"%ProgramFiles%\\Windows Photo Viewer\\PhotoViewer.dll,\" ImageView_Fullscreen %1",
    },
    {
      "\"rundll32.exe\" \"foo bar,\"baz",
      "\"rundll32.exe\" \"foo bar,\"baz",
    },
    {
      "\"rundll32.exe\" some,thing",
      "\"rundll32.exe\" some thing",
    },
    {
      "\"rundll32.exe\" some, thing",
      "\"rundll32.exe\" some  thing",
    },
    /* Commands with "rundll32" (without the .exe suffix) do exist,
     * but GLib currently does not recognize them, so there's no point
     * in testing these.
     */
  };

  for (i = 0; i < G_N_ELEMENTS (cases); i++)
    {
      gunichar2 *argument = g_utf8_to_utf16 (cases[i].orig, -1, NULL, NULL, NULL);
      gunichar2 *expected = g_utf8_to_utf16 (cases[i].fixed, -1, NULL, NULL, NULL);

      g_assert_nonnull (argument);
      g_assert_nonnull (expected);
      _g_win32_fixup_broken_microsoft_rundll_commandline (argument);

      g_assert_cmputf16 (argument, ==, expected, cases[i].orig, cases[i].fixed);

      g_free (argument);
      g_free (expected);
    }
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/appinfo/win32-rundll32-fixup", test_win32_rundll32_fixup);

  return g_test_run();
}

