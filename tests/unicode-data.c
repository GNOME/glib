/* GLib
 * unicode-data.c: Test Unicode character data
 *
 * Copyright (C) 2019 Red Hat, Inc
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"
#include <glib.h>
#include <string.h>
#include <locale.h>

#ifndef G_OS_WIN32
#include <unistd.h>
#endif

#include "test-common.h"

static const char *
char_type (GUnicodeType t)
{
  const char *names[] = {
    "Cc", "Cf", "Cn", "Co", "Cs", "Ll", "Lm", "Lo", "Lt",
    "Lu", "Mc", "Me", "Mn", "Nd", "Nl", "No", "Pc", "Pd",
    "Pe", "Pf", "Pi", "Po", "Ps", "Sc", "Sk", "Sm", "So",
    "Zl", "Zp", "Zs"
  };
  return names[t];
}

static const char *
break_type (GUnicodeBreakType t)
{
  const char *names[] = {
    "BK", "CR", "LF", "CM", "SG", "ZW", "IN", "GL", "CB",
    "SP", "BA", "BB", "B2", "HY", "NS", "OP", "CL", "QU",
    "EX", "ID", "NU", "IS", "SY", "AL", "PR", "PO", "SA",
    "AI", "XX", "NL", "WJ", "JL", "JV", "JT", "H2", "H3",
    "CP", "CJ", "HL", "RI", "EB", "EM", "ZWJ"
  };
  return names[t];
}

static const char *
script_name (GUnicodeScript s)
{
  const char *names[] = {
    "Zyyy", "Zinh", "Arab", "Armn", "Beng", "Bopo", "Cher",
    "Copt", "Cyrl", "Dsrt", "Deva", "Ethi", "Geor", "Goth",
    "Grek", "Gujr", "Guru", "Hani", "Hang", "Hebr", "Hira",
    "Knda", "Kana", "Khmr", "Laoo", "Latn", "Mlym", "Mong",
    "Mymr", "Ogam", "Ital", "Orya", "Runr", "Sinh", "Syrc",
    "Taml", "Telu", "Thaa", "Thai", "Tibt", "Cans", "Yiii",
    "Tglg", "Hano", "Buhd", "Tagb", "Brai", "Cprt", "Limb",
    "Osma", "Shaw", "Linb", "Tale", "Ugar", "Talu", "Bugi",
    "Glag", "Tfng", "Sylo", "Xpeo", "Khar", "Zzzz", "Bali",
    "Xsux", "Phnx", "Phag", "Nkoo", "Kali", "Lepc", "Rjng",
    "Sund", "Saur", "Cham", "Olck", "Vaii", "Cari", "Lyci",
    "Lydi", "Avst", "Bamu", "Egyp", "Armi", "Phli", "Prti",
    "Java", "Kthi", "Lisu", "Mtei", "Sarb", "Orkh", "Samr",
    "Lana", "Tavt", "Batk", "Brah", "Mand", "Cakm", "Merc",
    "Mero", "Plrd", "Shrd", "Sora", "Takr", "Bass", "Aghb",
    "Dupl", "Elba", "Gran", "Khoj", "Sind", "Lina", "Mahj",
    "Mani", "Mend", "Modi", "Mroo", "Nbat", "Narb", "Perm",
    "Hmng", "Palm", "Pauc", "Phlp", "Sidd", "Tirh", "Wara",
    "Ahom", "Hluw", "Hatr", "Mult", "Hung", "Sgnw", "Adlm",
    "Bhks", "Marc", "Newa", "Osge", "Tang", "Gonm", "Nshu",
    "Soyo", "Zanb", "Dogr", "Gong", "Rohg", "Maka", "Medf",
    "Sogo", "Sogd", "Elym", "Nand", "Rohg", "Wcho"
  };
  return names[s];
}

static void
test_file (const char *filename, GString *string)
{
  char *contents;
  gsize length;
  GError *error = NULL;
  char *p;
  GString *s1, *s2, *s3;
  GUnicodeScript prev_script = -1;
  int m;

  if (!g_file_get_contents (filename, &contents, &length, &error))
    {
      g_error ("%s", error->message);
      g_error_free (error);
      return;
    }

  g_string_append (string, "Text: ");
  s1 = g_string_new ("Char type: ");
  s2 = g_string_new ("Break type: ");
  s3 = g_string_new ("Script: ");

  m = MAX (MAX (s1->len, s2->len), s3->len);

  g_string_append_printf (s1, "%*s", (int)(m - s1->len), "");
  g_string_append_printf (s2, "%*s", (int)(m - s2->len), "");
  g_string_append_printf (s3, "%*s", (int)(m - s3->len), "");
  g_string_append_printf (string, "%*s", (int)(m - strlen ("Text: ")), "");

  for (p = contents; *p; p = g_utf8_next_char (p))
    {
      gunichar ch = g_utf8_get_char (p);
      const char *ctype = char_type (g_unichar_type (ch));
      const char *btype = break_type (g_unichar_break_type (ch));
      GUnicodeScript script = g_unichar_get_script (ch);
      int c = strlen (ctype);
      int b = strlen (btype);
      int s = 0;
      int t = 0;

      g_string_append_printf (s1, "%s", ctype);
      g_string_append_printf (s2, "%s", btype);

      if (prev_script != script)
        {
          const char *str = script_name (script);
          prev_script = script;
          g_string_append (s3, str);
          s = strlen (str);
        }

      if (ch == 0x20)
        {
          g_string_append (string, "[ ]");
          t = 3;
        }
      else if (g_unichar_isgraph (ch) &&
               (ch != 0x2028) &&
               (ch != 0x2029))
        {
          g_string_append_unichar (string, ch);
          t = 1;
        }
      else
        {
          char *str = g_strdup_printf ("[%#04x]", ch);
          g_string_append (string, str); 
          t = strlen (str);
          g_free (str);
        }

      m = MAX (t, MAX (MAX (c + 1, b + 1), s + 1));

      g_string_append_printf (string, "%*s", m - t, "");
      g_string_append_printf (s1, "%*s", m - c, "");
      g_string_append_printf (s2, "%*s", m - b, "");
      g_string_append_printf (s3, "%*s", m - s, "");
    }

  g_string_append (string, "\n");
  g_string_append_len (string, s1->str, s1->len);
  g_string_append (string, "\n");
  g_string_append_len (string, s2->str, s2->len);
  g_string_append (string, "\n");
  g_string_append_len (string, s3->str, s3->len);
  g_string_append (string, "\n");

  g_string_free (s1, TRUE);
  g_string_free (s2, TRUE);
  g_string_free (s3, TRUE);

  g_free (contents);
}

static gchar *
get_expected_filename (const gchar *filename)
{
  gchar *f, *p, *expected;

  f = g_strdup (filename);
  p = strstr (f, ".chars");
  if (p)
    *p = 0;
  expected = g_strconcat (f, ".expected", NULL);

  g_free (f);

  return expected;
}

static void
test_break (gconstpointer d)
{
  const char *filename = d;
  char *expected_file;
  GError *error = NULL;
  GString *dump;
  char *diff = NULL;

  expected_file = get_expected_filename (filename);

  dump = g_string_sized_new (0);

  test_file (filename, dump);

  diff = diff_with_file (expected_file, dump->str, dump->len, &error);
  g_assert_no_error (error);

  if (diff && diff[0])
    {
      g_printerr ("Contents don't match expected contents:\n%s", diff);
      g_test_fail ();
      g_free (diff);
    }

  g_string_free (dump, TRUE);
  g_free (expected_file);
}

int
main (int argc, char *argv[])
{
  GDir *dir;
  GError *error = NULL;
  const gchar *name;
  gchar *path;

  g_setenv ("LC_ALL", "en_US.UTF-8", TRUE);
  setlocale (LC_ALL, "");

  g_test_init (&argc, &argv, NULL);

  /* allow to easily generate expected output for new test cases */
  if (argc > 1)
    {
      GString *string;

      string = g_string_sized_new (0);
      test_file (argv[1], string);
      g_print ("%s", string->str);

      return 0;
    }

  path = g_test_build_filename (G_TEST_DIST, "chars", NULL);
  dir = g_dir_open (path, 0, &error);
  g_free (path);
  g_assert_no_error (error);
  while ((name = g_dir_read_name (dir)) != NULL)
    {
      if (!strstr (name, "chars"))
        continue;

      path = g_strdup_printf ("/chars/%s", name);
      g_test_add_data_func_full (path, g_test_build_filename (G_TEST_DIST, "chars", name, NULL),
                                 test_break, g_free);
      g_free (path);
    }
  g_dir_close (dir);

  return g_test_run ();
}
