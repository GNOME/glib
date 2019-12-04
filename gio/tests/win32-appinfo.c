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
    const gunichar2 *orig;
    const gunichar2 *fixed;
  } cases[] = {
    {
      L"%SystemRoot%\\System32\\rundll32.exe \"%ProgramFiles%\\Windows Photo Viewer\\PhotoViewer.dll\", ImageView_Fullscreen %1",
      L"%SystemRoot%\\System32\\rundll32.exe \"%ProgramFiles%\\Windows Photo Viewer\\PhotoViewer.dll\"  ImageView_Fullscreen %1",
    },
    {
      L"rundll32.exe foo.bar,baz",
      L"rundll32.exe foo.bar baz",
    },
    {
      L"rundll32.exe \"foo bar\",baz",
      L"rundll32.exe \"foo bar\" baz",
    },
    {
      L"\"rundll32.exe\" \"foo bar\",baz",
      L"\"rundll32.exe\" \"foo bar\" baz",
    },
    {
      L"\"rundll32.exe\" \"foo bar\",, , ,,, , ,,baz",
      L"\"rundll32.exe\" \"foo bar\" , , ,,, , ,,baz",
    },
    {
      L"\"rundll32.exe\" foo.bar,,,,,,,,,baz",
      L"\"rundll32.exe\" foo.bar ,,,,,,,,baz",
    },
    {
      L"\"rundll32.exe\" foo.bar baz",
      L"\"rundll32.exe\" foo.bar baz",
    },
    {
      L"\"RuNdlL32.exe\" foo.bar baz",
      L"\"RuNdlL32.exe\" foo.bar baz",
    },
    {
      L"%SystemRoot%\\System32\\rundll32.exe \"%ProgramFiles%\\Windows Photo Viewer\\PhotoViewer.dll,\" ImageView_Fullscreen %1",
      L"%SystemRoot%\\System32\\rundll32.exe \"%ProgramFiles%\\Windows Photo Viewer\\PhotoViewer.dll,\" ImageView_Fullscreen %1",
    },
    {
      L"\"rundll32.exe\" \"foo bar,\"baz",
      L"\"rundll32.exe\" \"foo bar,\"baz",
    },
    {
      L"\"rundll32.exe\" some,thing",
      L"\"rundll32.exe\" some thing",
    },
    {
      L"\"rundll32.exe\" some, thing",
      L"\"rundll32.exe\" some  thing",
    },
    /* Commands with "rundll32" (without the .exe suffix) do exist,
     * but GLib currently does not recognize them, so there's no point
     * in testing these.
     */
  };

  for (i = 0; i < G_N_ELEMENTS (cases); i++)
    {
      gunichar2 *argument = wcsdup (cases[i].orig);
      gchar *in_u8;
      gchar *out_u8;

      _g_win32_fixup_broken_microsoft_rundll_commandline (argument);

      in_u8 = g_utf16_to_utf8 (argument, -1, NULL, NULL, NULL);
      g_assert_nonnull (in_u8);
      out_u8 = g_utf16_to_utf8 (cases[i].fixed, -1, NULL, NULL, NULL);
      g_assert_nonnull (out_u8);

      g_assert_cmputf16 (argument, ==, cases[i].fixed, in_u8, out_u8);

      g_free (out_u8);
      g_free (in_u8);
      free (argument);
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

