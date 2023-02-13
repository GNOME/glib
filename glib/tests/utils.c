/* Unit tests for utilities
 * Copyright (C) 2010 Red Hat, Inc.
 *
 * SPDX-License-Identifier: LicenseRef-old-glib-tests
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
 *
 * Author: Matthias Clasen
 */

#ifndef GLIB_DISABLE_DEPRECATION_WARNINGS
#define GLIB_DISABLE_DEPRECATION_WARNINGS
#endif

#include "glib.h"
#include "glib-private.h"
#include "gutilsprivate.h"
#include "glib/gstdio.h"

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#ifdef G_OS_UNIX
#include <sys/utsname.h>
#endif
#ifdef G_OS_WIN32
#include <windows.h>
#endif

static gboolean
strv_check (const gchar * const *strv, ...)
{
  va_list args;
  gchar *s;
  gint i;

  va_start (args, strv);
  for (i = 0; strv[i]; i++)
    {
      s = va_arg (args, gchar*);
      if (g_strcmp0 (strv[i], s) != 0)
        {
          va_end (args);
          return FALSE;
        }
    }

  va_end (args);

  return TRUE;
}

static void
test_language_names (void)
{
  const gchar * const *names;

  g_setenv ("LANGUAGE", "de:en_US", TRUE);
  names = g_get_language_names ();
  g_assert (strv_check (names, "de", "en_US", "en", "C", NULL));

  g_setenv ("LANGUAGE", "tt_RU.UTF-8@iqtelif", TRUE);
  names = g_get_language_names ();
  g_assert (strv_check (names,
                        "tt_RU.UTF-8@iqtelif",
                        "tt_RU@iqtelif",
                        "tt.UTF-8@iqtelif",
                        "tt@iqtelif",
                        "tt_RU.UTF-8",
                        "tt_RU",
                        "tt.UTF-8",
                        "tt",
                        "C",
                        NULL));
}

static void
test_locale_variants (void)
{
  char **v;

  v = g_get_locale_variants ("fr_BE");
  g_assert (strv_check ((const gchar * const *) v, "fr_BE", "fr", NULL));
  g_strfreev (v);

  v = g_get_locale_variants ("sr_SR@latin");
  g_assert (strv_check ((const gchar * const *) v, "sr_SR@latin", "sr@latin", "sr_SR", "sr", NULL));
  g_strfreev (v);
}

static void
test_version (void)
{
  if (g_test_verbose ())
    g_printerr ("(header %d.%d.%d library %d.%d.%d) ",
              GLIB_MAJOR_VERSION, GLIB_MINOR_VERSION, GLIB_MICRO_VERSION,
              glib_major_version, glib_minor_version, glib_micro_version);

  g_assert (glib_check_version (GLIB_MAJOR_VERSION,
                                GLIB_MINOR_VERSION,
                                GLIB_MICRO_VERSION) == NULL);
  g_assert (glib_check_version (GLIB_MAJOR_VERSION,
                                GLIB_MINOR_VERSION,
                                0) == NULL);
  g_assert (glib_check_version (GLIB_MAJOR_VERSION - 1,
                                0,
                                0) != NULL);
  g_assert (glib_check_version (GLIB_MAJOR_VERSION + 1,
                                0,
                                0) != NULL);
  g_assert (glib_check_version (GLIB_MAJOR_VERSION,
                                GLIB_MINOR_VERSION + 1,
                                0) != NULL);
  /* don't use + 1 here, since a +/-1 difference can
   * happen due to post-release version bumps in git
   */
  g_assert (glib_check_version (GLIB_MAJOR_VERSION,
                                GLIB_MINOR_VERSION,
                                GLIB_MICRO_VERSION + 3) != NULL);
}

static const gchar *argv0;

static void
test_appname (void)
{
  const gchar *prgname;
  const gchar *appname;

  prgname = g_get_prgname ();
  appname = g_get_application_name ();
  g_assert_cmpstr (prgname, ==, argv0);
  g_assert_cmpstr (appname, ==, prgname);

  g_set_prgname ("prgname");

  prgname = g_get_prgname ();
  appname = g_get_application_name ();
  g_assert_cmpstr (prgname, ==, "prgname");
  g_assert_cmpstr (appname, ==, "prgname");

  g_set_application_name ("appname");

  prgname = g_get_prgname ();
  appname = g_get_application_name ();
  g_assert_cmpstr (prgname, ==, "prgname");
  g_assert_cmpstr (appname, ==, "appname");
}

static gpointer
thread_prgname_check (gpointer data)
{
  gint *n_threads_got_prgname = (gint *) data;
  const gchar *old_prgname;

  old_prgname = g_get_prgname ();
  g_assert_cmpstr (old_prgname, ==, "prgname");

  g_atomic_int_inc (n_threads_got_prgname);

  while (g_strcmp0 (g_get_prgname (), "prgname2") != 0);

  return NULL;
}

static void
test_prgname_thread_safety (void)
{
  gsize i;
  gint n_threads_got_prgname;
  GThread *threads[4];

  g_test_bug ("https://gitlab.gnome.org/GNOME/glib/-/issues/847");
  g_test_summary ("Test that threads racing to get and set the program name "
                  "always receive a valid program name.");

  g_set_prgname ("prgname");
  g_atomic_int_set (&n_threads_got_prgname, 0);

  for (i = 0; i < G_N_ELEMENTS (threads); i++)
    threads[i] = g_thread_new (NULL, thread_prgname_check, &n_threads_got_prgname);

  while (g_atomic_int_get (&n_threads_got_prgname) != G_N_ELEMENTS (threads))
    g_usleep (50);

  g_set_prgname ("prgname2");

  /* Wait for all the workers to exit. */
  for (i = 0; i < G_N_ELEMENTS (threads); i++)
    g_thread_join (threads[i]);

  /* reset prgname */
  g_set_prgname ("prgname");
}

static void
test_tmpdir (void)
{
  g_test_bug ("https://bugzilla.gnome.org/show_bug.cgi?id=627969");
  g_assert_cmpstr (g_get_tmp_dir (), !=, "");
}

#if defined(__GNUC__) && (__GNUC__ >= 4)
#define TEST_BUILTINS 1
#else
#define TEST_BUILTINS 0
#endif

#if TEST_BUILTINS
static gint
builtin_bit_nth_lsf1 (gulong mask, gint nth_bit)
{
  if (nth_bit >= 0)
    {
      if (G_LIKELY (nth_bit < GLIB_SIZEOF_LONG * 8 - 1))
        mask &= -(1UL << (nth_bit + 1));
      else
        mask = 0;
    }
  return __builtin_ffsl (mask) - 1;
}

static gint
builtin_bit_nth_lsf2 (gulong mask, gint nth_bit)
{
  if (nth_bit >= 0)
    {
      if (G_LIKELY (nth_bit < GLIB_SIZEOF_LONG * 8 - 1))
        mask &= -(1UL << (nth_bit + 1));
      else
        mask = 0;
    }
  return mask ? __builtin_ctzl (mask) : -1;
}

static gint
builtin_bit_nth_msf (gulong mask, gint nth_bit)
{
  if (nth_bit >= 0 && nth_bit < GLIB_SIZEOF_LONG * 8)
    mask &= (1UL << nth_bit) - 1;
  return mask ? GLIB_SIZEOF_LONG * 8 - 1 - __builtin_clzl (mask) : -1;
}

static guint
builtin_bit_storage (gulong number)
{
  return number ? GLIB_SIZEOF_LONG * 8 - __builtin_clzl (number) : 1;
}
#endif

static gint
naive_bit_nth_lsf (gulong mask, gint nth_bit)
{
  if (G_UNLIKELY (nth_bit < -1))
    nth_bit = -1;
  while (nth_bit < ((GLIB_SIZEOF_LONG * 8) - 1))
    {
      nth_bit++;
      if (mask & (1UL << nth_bit))
        return nth_bit;
    }
  return -1;
}

static gint
naive_bit_nth_msf (gulong mask, gint nth_bit)
{
  if (nth_bit < 0 || G_UNLIKELY (nth_bit > GLIB_SIZEOF_LONG * 8))
    nth_bit = GLIB_SIZEOF_LONG * 8;
  while (nth_bit > 0)
    {
      nth_bit--;
      if (mask & (1UL << nth_bit))
        return nth_bit;
    }
  return -1;
}

static guint
naive_bit_storage (gulong number)
{
  guint n_bits = 0;

  do
    {
      n_bits++;
      number >>= 1;
    }
  while (number);
  return n_bits;
}

static void
test_basic_bits (void)
{
  gulong i;
  gint nth_bit;

  /* we loop like this: 0, -1, 1, -2, 2, -3, 3, ... */
  for (i = 0; (glong) i < 1500; i = -(i + ((glong) i >= 0)))
    {
      guint naive_bit_storage_i = naive_bit_storage (i);

      /* Test the g_bit_*() implementations against the compiler builtins (if
       * available), and against a slow-but-correct ‘naive’ implementation.
       * They should all agree.
       *
       * The macro and function versions of the g_bit_*() functions are tested,
       * hence one call with the function name in brackets (to avoid it being
       * expanded as a macro). */
#if TEST_BUILTINS
      g_assert_cmpint (naive_bit_storage_i, ==, builtin_bit_storage (i));
#endif
      g_assert_cmpint (naive_bit_storage_i, ==, g_bit_storage (i));
      g_assert_cmpint (naive_bit_storage_i, ==, (g_bit_storage) (i));

      for (nth_bit = -3; nth_bit <= 2 + GLIB_SIZEOF_LONG * 8; nth_bit++)
        {
          gint naive_bit_nth_lsf_i_nth_bit = naive_bit_nth_lsf (i, nth_bit);
          gint naive_bit_nth_msf_i_nth_bit = naive_bit_nth_msf (i, nth_bit);

#if TEST_BUILTINS
          g_assert_cmpint (naive_bit_nth_lsf_i_nth_bit, ==,
                           builtin_bit_nth_lsf1 (i, nth_bit));
          g_assert_cmpint (naive_bit_nth_lsf_i_nth_bit, ==,
                           builtin_bit_nth_lsf2 (i, nth_bit));
#endif
          g_assert_cmpint (naive_bit_nth_lsf_i_nth_bit, ==,
                           g_bit_nth_lsf (i, nth_bit));
          g_assert_cmpint (naive_bit_nth_lsf_i_nth_bit, ==,
                           (g_bit_nth_lsf) (i, nth_bit));

#if TEST_BUILTINS
          g_assert_cmpint (naive_bit_nth_msf_i_nth_bit, ==,
                           builtin_bit_nth_msf (i, nth_bit));
#endif
          g_assert_cmpint (naive_bit_nth_msf_i_nth_bit, ==,
                           g_bit_nth_msf (i, nth_bit));
          g_assert_cmpint (naive_bit_nth_msf_i_nth_bit, ==,
                           (g_bit_nth_msf) (i, nth_bit));
        }
    }
}

static void
test_bits (void)
{
  gulong mask;
  gint max_bit;
  gint i, pos;

  pos = g_bit_nth_lsf (0, -1);
  g_assert_cmpint (pos, ==, -1);

  max_bit = sizeof (gulong) * 8;
  for (i = 0; i < max_bit; i++)
    {
      mask = 1UL << i;

      pos = g_bit_nth_lsf (mask, -1);
      g_assert_cmpint (pos, ==, i);

      pos = g_bit_nth_lsf (mask, i - 3);
      g_assert_cmpint (pos , ==, i);

      pos = g_bit_nth_lsf (mask, i);
      g_assert_cmpint (pos , ==, -1);

      pos = g_bit_nth_lsf (mask, i + 1);
      g_assert_cmpint (pos , ==, -1);
    }

  pos = g_bit_nth_msf (0, -1);
  g_assert_cmpint (pos, ==, -1);

  for (i = 0; i < max_bit; i++)
    {
      mask = 1UL << i;

      pos = g_bit_nth_msf (mask, -1);
      g_assert_cmpint (pos, ==, i);

      pos = g_bit_nth_msf (mask, i + 3);
      g_assert_cmpint (pos , ==, i);

      pos = g_bit_nth_msf (mask, i);
      g_assert_cmpint (pos , ==, -1);

      if (i > 0)
        {
          pos = g_bit_nth_msf (mask, i - 1);
          g_assert_cmpint (pos , ==, -1);
        }
    }
}

static void
test_swap (void)
{
  guint16 a16, b16;
  guint32 a32, b32;
  guint64 a64, b64;

  a16 = 0xaabb;
  b16 = 0xbbaa;

  g_assert_cmpint (GUINT16_SWAP_LE_BE (a16), ==, b16);

  a32 = 0xaaaabbbb;
  b32 = 0xbbbbaaaa;

  g_assert_cmpint (GUINT32_SWAP_LE_BE (a32), ==, b32);

  a64 = G_GUINT64_CONSTANT(0xaaaaaaaabbbbbbbb);
  b64 = G_GUINT64_CONSTANT(0xbbbbbbbbaaaaaaaa);

  g_assert_cmpint (GUINT64_SWAP_LE_BE (a64), ==, b64);
}

static void
test_find_program (void)
{
  gchar *res;

#ifdef G_OS_UNIX
  gchar *relative_path;
  gchar *absolute_path;
  gchar *cwd;
  gsize i;

  res = g_find_program_in_path ("sh");
  g_assert (res != NULL);
  g_free (res);

  res = g_find_program_in_path ("/bin/sh");
  g_assert (res != NULL);
  g_free (res);

  cwd = g_get_current_dir ();
  absolute_path = g_find_program_in_path ("sh");
  relative_path = g_strdup (absolute_path);
  for (i = 0; cwd[i] != '\0'; i++)
    {
      if (cwd[i] == '/' && cwd[i + 1] != '\0')
        {
          gchar *relative_path_2 = g_strconcat ("../", relative_path, NULL);
          g_free (relative_path);
          relative_path = relative_path_2;
        }
    }
  res = g_find_program_in_path (relative_path);
  g_assert_nonnull (res);
  g_assert_true (g_path_is_absolute (res));
  g_free (cwd);
  g_free (absolute_path);
  g_free (relative_path);
  g_free (res);

#else
  /* There's not a lot we can search for that would reliably work both
   * on real Windows and mingw.
   */
#endif

  res = g_find_program_in_path ("this_program_does_not_exit");
  g_assert (res == NULL);

  res = g_find_program_in_path ("/bin");
  g_assert (res == NULL);

  res = g_find_program_in_path ("/etc/passwd");
  g_assert (res == NULL);
}

static char *
find_program_for_path (const char *program,
                       const char *path,
                       const char *working_dir)
{
  return GLIB_PRIVATE_CALL(g_find_program_for_path) (program, path, working_dir);
}

static void
test_find_program_for_path (void)
{
  GError *error = NULL;
  /* Using .cmd extension to make windows to consider it an executable */
  const char *command_to_find = "just-an-exe-file.cmd";
  char *path;
  char *exe_path;
  char *found_path;
  char *old_cwd;
  char *tmp;

  tmp = g_dir_make_tmp ("find_program_for_path_XXXXXXX", &error);
  g_assert_no_error (error);

  path = g_build_filename (tmp, "sub-path", NULL);
  g_mkdir (path, 0700);
  g_assert_true (g_file_test (path, G_FILE_TEST_IS_DIR));

  exe_path = g_build_filename (path, command_to_find, NULL);
  g_file_set_contents (exe_path, "", -1, &error);
  g_assert_no_error (error);
  g_assert_true (g_file_test (exe_path, G_FILE_TEST_EXISTS));

#ifdef G_OS_UNIX
  g_assert_no_errno (g_chmod (exe_path, 0500));
#endif
  g_assert_true (g_file_test (exe_path, G_FILE_TEST_IS_EXECUTABLE));

  g_assert_null (g_find_program_in_path (command_to_find));
  g_assert_null (find_program_for_path (command_to_find, NULL, NULL));

  found_path = find_program_for_path (command_to_find, path, NULL);
#ifdef __APPLE__
  g_assert_nonnull (found_path);
  g_assert_true (g_str_has_suffix (found_path, exe_path));
#else
  g_assert_cmpstr (exe_path, ==, found_path);
#endif
  g_clear_pointer (&found_path, g_free);

  found_path = find_program_for_path (command_to_find, path, path);
#ifdef __APPLE__
  g_assert_nonnull (found_path);
  g_assert_true (g_str_has_suffix (found_path, exe_path));
#else
  g_assert_cmpstr (exe_path, ==, found_path);
#endif
  g_clear_pointer (&found_path, g_free);

  found_path = find_program_for_path (command_to_find, NULL, path);
#ifdef __APPLE__
  g_assert_nonnull (found_path);
  g_assert_true (g_str_has_suffix (found_path, exe_path));
#else
  g_assert_cmpstr (exe_path, ==, found_path);
#endif
  g_clear_pointer (&found_path, g_free);

  found_path = find_program_for_path (command_to_find, "::", path);
#ifdef __APPLE__
  g_assert_nonnull (found_path);
  g_assert_true (g_str_has_suffix (found_path, exe_path));
#else
  g_assert_cmpstr (exe_path, ==, found_path);
#endif
  g_clear_pointer (&found_path, g_free);

  old_cwd = g_get_current_dir ();
  g_chdir (path);
  found_path =
    find_program_for_path (command_to_find,
      G_SEARCHPATH_SEPARATOR_S G_SEARCHPATH_SEPARATOR_S, NULL);
  g_chdir (old_cwd);
  g_clear_pointer (&old_cwd, g_free);
#ifdef __APPLE__
  g_assert_nonnull (found_path);
  g_assert_true (g_str_has_suffix (found_path, exe_path));
#else
  g_assert_cmpstr (exe_path, ==, found_path);
#endif
  g_clear_pointer (&found_path, g_free);

  old_cwd = g_get_current_dir ();
  g_chdir (tmp);
  found_path =
    find_program_for_path (command_to_find,
      G_SEARCHPATH_SEPARATOR_S G_SEARCHPATH_SEPARATOR_S, "sub-path");
  g_chdir (old_cwd);
  g_clear_pointer (&old_cwd, g_free);
#ifdef __APPLE__
  g_assert_nonnull (found_path);
  g_assert_true (g_str_has_suffix (found_path, exe_path));
#else
  g_assert_cmpstr (exe_path, ==, found_path);
#endif
  g_clear_pointer (&found_path, g_free);

  g_assert_null (
    find_program_for_path (command_to_find,
      G_SEARCHPATH_SEPARATOR_S G_SEARCHPATH_SEPARATOR_S, "other-sub-path"));

  found_path = find_program_for_path (command_to_find,
    G_SEARCHPATH_SEPARATOR_S "sub-path" G_SEARCHPATH_SEPARATOR_S, tmp);
#ifdef __APPLE__
  g_assert_nonnull (found_path);
  g_assert_true (g_str_has_suffix (found_path, exe_path));
#else
  g_assert_cmpstr (exe_path, ==, found_path);
#endif
  g_clear_pointer (&found_path, g_free);

  g_assert_null (find_program_for_path (command_to_find,
    G_SEARCHPATH_SEPARATOR_S "other-sub-path" G_SEARCHPATH_SEPARATOR_S, tmp));

#ifdef G_OS_UNIX
  found_path = find_program_for_path ("sh", NULL, tmp);
  g_assert_nonnull (found_path);
  g_clear_pointer (&found_path, g_free);

  old_cwd = g_get_current_dir ();
  g_chdir ("/");
  found_path = find_program_for_path ("sh", "sbin:bin:usr/bin:usr/sbin", NULL);
  g_chdir (old_cwd);
  g_clear_pointer (&old_cwd, g_free);
  g_assert_nonnull (found_path);
  g_clear_pointer (&found_path, g_free);

  found_path = find_program_for_path ("sh", "sbin:bin:usr/bin:usr/sbin", "/");
  g_assert_nonnull (found_path);
  g_clear_pointer (&found_path, g_free);
#endif /* G_OS_UNIX */

  g_clear_pointer (&exe_path, g_free);
  g_clear_pointer (&path, g_free);
  g_clear_pointer (&tmp, g_free);
  g_clear_error (&error);
}

static void
test_debug (void)
{
  GDebugKey keys[] = {
    { "key1", 1 },
    { "key2", 2 },
    { "key3", 4 },
  };
  guint res;

  res = g_parse_debug_string (NULL, keys, G_N_ELEMENTS (keys));
  g_assert_cmpint (res, ==, 0);

  res = g_parse_debug_string ("foobabla;#!%!$%112 223", keys, G_N_ELEMENTS (keys));
  g_assert_cmpint (res, ==, 0);

  res = g_parse_debug_string ("key1:key2", keys, G_N_ELEMENTS (keys));
  g_assert_cmpint (res, ==, 3);

  res = g_parse_debug_string ("key1;key2", keys, G_N_ELEMENTS (keys));
  g_assert_cmpint (res, ==, 3);

  res = g_parse_debug_string ("key1,key2", keys, G_N_ELEMENTS (keys));
  g_assert_cmpint (res, ==, 3);

  res = g_parse_debug_string ("key1   key2", keys, G_N_ELEMENTS (keys));
  g_assert_cmpint (res, ==, 3);

  res = g_parse_debug_string ("key1\tkey2", keys, G_N_ELEMENTS (keys));
  g_assert_cmpint (res, ==, 3);

  res = g_parse_debug_string ("all", keys, G_N_ELEMENTS (keys));
  g_assert_cmpint (res, ==, 7);

  if (g_test_subprocess ())
    {
      res = g_parse_debug_string ("help", keys, G_N_ELEMENTS (keys));
      g_assert_cmpint (res, ==, 0);
      return;
    }
  g_test_trap_subprocess (NULL, 0, G_TEST_SUBPROCESS_DEFAULT);
  g_test_trap_assert_passed ();
  g_test_trap_assert_stderr ("*Supported debug values: key1 key2 key3 all help*");
}

static void
test_codeset (void)
{
  gchar *c;
  const gchar *c2;

  c = g_get_codeset ();
  g_get_charset (&c2);

  g_assert_cmpstr (c, ==, c2);

  g_free (c);
}

static void
test_codeset2 (void)
{
  if (g_test_subprocess ())
    {
      const gchar *c;
      g_setenv ("CHARSET", "UTF-8", TRUE);
      g_get_charset (&c);
      g_assert_cmpstr (c, ==, "UTF-8");
      return;
    }
  g_test_trap_subprocess (NULL, 0, G_TEST_SUBPROCESS_DEFAULT);
  g_test_trap_assert_passed ();
}

static void
test_console_charset (void)
{
  const gchar *c1;
  const gchar *c2;

#ifdef G_OS_WIN32
  /* store current environment and unset $LANG to make sure it does not interfere */
  const unsigned int initial_cp = GetConsoleOutputCP ();
  gchar *initial_lang = g_strdup (g_getenv ("LANG"));
  g_unsetenv ("LANG");

  /* set console output codepage to something specific (ISO-8859-1 aka CP28591) and query it */
  SetConsoleOutputCP (28591);
  g_get_console_charset (&c1);
  g_assert_cmpstr (c1, ==, "ISO-8859-1");

  /* set $LANG to something specific (should override the console output codepage) and query it */
  g_setenv ("LANG", "de_DE.ISO-8859-15@euro", TRUE);
  g_get_console_charset (&c2);
  g_assert_cmpstr (c2, ==, "ISO-8859-15");

  /* reset environment */
  if (initial_cp)
    SetConsoleOutputCP (initial_cp);
  if (initial_lang)
    g_setenv ("LANG", initial_lang, TRUE);
  g_free (initial_lang);
#else
  g_get_charset (&c1);
  g_get_console_charset (&c2);

  g_assert_cmpstr (c1, ==, c2);
#endif
}

extern const gchar *glib_pgettext (const gchar *msgidctxt, gsize msgidoffset);

static void
test_gettext (void)
{
  const gchar *am0, *am1, *am2, *am3;

  am0 = glib_pgettext ("GDateTime\004AM", strlen ("GDateTime") + 1);
  am1 = g_dpgettext ("glib20", "GDateTime\004AM", strlen ("GDateTime") + 1);
  am2 = g_dpgettext ("glib20", "GDateTime|AM", 0);
  am3 = g_dpgettext2 ("glib20", "GDateTime", "AM");

  g_assert_cmpstr (am0, ==, am1);
  g_assert_cmpstr (am1, ==, am2);
  g_assert_cmpstr (am2, ==, am3);
}

static void
test_username (void)
{
  const gchar *name;

  name = g_get_user_name ();

  g_assert (name != NULL);
}

static void
test_realname (void)
{
  const gchar *name;

  name = g_get_real_name ();

  g_assert (name != NULL);
}

static void
test_hostname (void)
{
  const gchar *name;

  name = g_get_host_name ();

  g_assert (name != NULL);
  g_assert_true (g_utf8_validate (name, -1, NULL));
}

#ifdef G_OS_UNIX
static void
test_xdg_dirs (void)
{
  gchar *xdg;
  const gchar *dir;
  const gchar * const *dirs;
  gchar *s;

  xdg = g_strdup (g_getenv ("XDG_CONFIG_HOME"));
  if (!xdg)
    xdg = g_build_filename (g_get_home_dir (), ".config", NULL);

  dir = g_get_user_config_dir ();

  g_assert_cmpstr (dir, ==, xdg);
  g_free (xdg);

  xdg = g_strdup (g_getenv ("XDG_DATA_HOME"));
  if (!xdg)
    xdg = g_build_filename (g_get_home_dir (), ".local", "share", NULL);

  dir = g_get_user_data_dir ();

  g_assert_cmpstr (dir, ==, xdg);
  g_free (xdg);

  xdg = g_strdup (g_getenv ("XDG_CACHE_HOME"));
  if (!xdg)
    xdg = g_build_filename (g_get_home_dir (), ".cache", NULL);

  dir = g_get_user_cache_dir ();

  g_assert_cmpstr (dir, ==, xdg);
  g_free (xdg);

  xdg = g_strdup (g_getenv ("XDG_STATE_HOME"));
  if (!xdg)
    xdg = g_build_filename (g_get_home_dir (), ".local/state", NULL);

  dir = g_get_user_state_dir ();

  g_assert_cmpstr (dir, ==, xdg);
  g_free (xdg);

  xdg = g_strdup (g_getenv ("XDG_RUNTIME_DIR"));
  if (!xdg)
    xdg = g_strdup (g_get_user_cache_dir ());

  dir = g_get_user_runtime_dir ();

  g_assert_cmpstr (dir, ==, xdg);
  g_free (xdg);

  xdg = (gchar *)g_getenv ("XDG_CONFIG_DIRS");
  if (!xdg)
    xdg = "/etc/xdg";

  dirs = g_get_system_config_dirs ();

  s = g_strjoinv (":", (gchar **)dirs);

  g_assert_cmpstr (s, ==, xdg);

  g_free (s);
}
#endif

static void
test_special_dir (void)
{
  const gchar *dir, *dir2;

  dir = g_get_user_special_dir (G_USER_DIRECTORY_DESKTOP);
  g_reload_user_special_dirs_cache ();
  dir2 = g_get_user_special_dir (G_USER_DIRECTORY_DESKTOP);

  g_assert_cmpstr (dir, ==, dir2);
}

static void
test_desktop_special_dir (void)
{
  const gchar *dir, *dir2;

  dir = g_get_user_special_dir (G_USER_DIRECTORY_DESKTOP);
  g_assert (dir != NULL);

  g_reload_user_special_dirs_cache ();
  dir2 = g_get_user_special_dir (G_USER_DIRECTORY_DESKTOP);
  g_assert (dir2 != NULL);
}

static void
test_os_info (void)
{
  gchar *name;
  gchar *contents = NULL;
#if defined (G_OS_UNIX) && !(defined (G_OS_WIN32) || defined (__APPLE__))
  struct utsname info;
#endif

  /* Whether this is implemented or not, it must not crash */
  name = g_get_os_info (G_OS_INFO_KEY_NAME);
  g_test_message ("%s: %s",
                  G_OS_INFO_KEY_NAME,
                  name == NULL ? "(null)" : name);

#if defined (G_OS_WIN32) || defined (__APPLE__)
  /* These OSs have a special case so NAME should always succeed */
  g_assert_nonnull (name);
#elif defined (G_OS_UNIX)
  if (g_file_get_contents ("/etc/os-release", &contents, NULL, NULL) ||
      g_file_get_contents ("/usr/lib/os-release", &contents, NULL, NULL) ||
      uname (&info) == 0)
    g_assert_nonnull (name);
  else
    g_test_skip ("os-release(5) API not implemented on this platform");
#else
  g_test_skip ("g_get_os_info() not supported on this platform");
#endif

  g_free (name);
  g_free (contents);
}

static void
source_test (gpointer data)
{
  g_assert_not_reached ();
}

static void
test_clear_source (void)
{
  guint id;

  id = g_idle_add_once (source_test, NULL);
  g_assert_cmpuint (id, >, 0);

  g_clear_handle_id (&id, g_source_remove);
  g_assert_cmpuint (id, ==, 0);

  id = g_timeout_add_once (100, source_test, NULL);
  g_assert_cmpuint (id, >, 0);

  g_clear_handle_id (&id, g_source_remove);
  g_assert_cmpuint (id, ==, 0);
}

static void
test_clear_pointer (void)
{
  gpointer a;

  a = g_malloc (5);
  g_clear_pointer (&a, g_free);
  g_assert (a == NULL);

  a = g_malloc (5);
  (g_clear_pointer) (&a, g_free);
  g_assert (a == NULL);
}

/* Test that g_clear_pointer() works with a GDestroyNotify which contains a cast.
 * See https://gitlab.gnome.org/GNOME/glib/issues/1425 */
static void
test_clear_pointer_cast (void)
{
  GHashTable *hash_table = NULL;

  hash_table = g_hash_table_new (g_str_hash, g_str_equal);

  g_assert_nonnull (hash_table);

  g_clear_pointer (&hash_table, (void (*) (GHashTable *)) g_hash_table_destroy);

  g_assert_null (hash_table);
}

/* Test that the macro version of g_clear_pointer() only evaluates its argument
 * once, just like the function version would. */
static void
test_clear_pointer_side_effects (void)
{
  gchar **my_string_array, **i;

  my_string_array = g_new0 (gchar*, 3);
  my_string_array[0] = g_strdup ("hello");
  my_string_array[1] = g_strdup ("there");
  my_string_array[2] = NULL;

  i = my_string_array;

  g_clear_pointer (i++, g_free);

  g_assert_true (i == &my_string_array[1]);
  g_assert_null (my_string_array[0]);
  g_assert_nonnull (my_string_array[1]);
  g_assert_null (my_string_array[2]);

  g_free (my_string_array[1]);
  g_free (my_string_array[2]);
  g_free (my_string_array);
}

static int obj_count;

static void
get_obj (gpointer *obj_out)
{
  gpointer obj = g_malloc (5);
  obj_count++;

  if (obj_out)
    *obj_out = g_steal_pointer (&obj);

  if (obj)
    {
      g_free (obj);
      obj_count--;
    }
}

static void
test_take_pointer (void)
{
  gpointer a;
  gpointer b;

  get_obj (NULL);

  get_obj (&a);
  g_assert (a);

  /* ensure that it works to skip the macro */
  b = (g_steal_pointer) (&a);
  g_assert (!a);
  obj_count--;
  g_free (b);

  g_assert (!obj_count);
}

static void
test_misc_mem (void)
{
  gpointer a;

  a = g_try_malloc (0);
  g_assert (a == NULL);

  a = g_try_malloc0 (0);
  g_assert (a == NULL);

  a = g_malloc (16);
  a = g_try_realloc (a, 20);
  a = g_try_realloc (a, 0);

  g_assert (a == NULL);
}

static void
aligned_alloc_nz (void)
{
  gpointer a;

  /* Test an alignment that’s zero */
  a = g_aligned_alloc (16, sizeof(char), 0);
  g_aligned_free (a);
  exit (0);
}

static void
aligned_alloc_npot (void)
{
  gpointer a;

  /* Test an alignment that’s not a power of two */
  a = g_aligned_alloc (16, sizeof(char), 15);
  g_aligned_free (a);
  exit (0);
}

static void
aligned_alloc_nmov (void)
{
  gpointer a;

  /* Test an alignment that’s not a multiple of sizeof(void*) */
  a = g_aligned_alloc (16, sizeof(char), sizeof(void *) / 2);
  g_aligned_free (a);
  exit (0);
}

static void
test_aligned_mem (void)
{
  gpointer a;

  g_test_summary ("Aligned memory allocator");

  a = g_aligned_alloc (0, sizeof (int), MAX (sizeof (void *), 8));
  g_assert_null (a);

  a = g_aligned_alloc0 (0, sizeof (int), MAX (sizeof (void *), 8));
  g_assert_null (a);

  a = g_aligned_alloc (16, 0, MAX (sizeof (void *), 8));
  g_assert_null (a);

#define CHECK_SUBPROCESS_FAIL(name,msg) do { \
      if (g_test_undefined ()) \
        { \
          g_test_message (msg); \
          g_test_trap_subprocess ("/utils/aligned-mem/subprocess/" #name, 0, \
                                  G_TEST_SUBPROCESS_DEFAULT); \
          g_test_trap_assert_failed (); \
        } \
    } while (0)

  CHECK_SUBPROCESS_FAIL (aligned_alloc_nz, "Alignment must not be zero");
  CHECK_SUBPROCESS_FAIL (aligned_alloc_npot, "Alignment must be a power of two");
  CHECK_SUBPROCESS_FAIL (aligned_alloc_nmov, "Alignment must be a multiple of sizeof(void*)");
}

static void
test_aligned_mem_alignment (void)
{
  gchar *p;

  g_test_summary ("Check that g_aligned_alloc() returns a correctly aligned pointer");

  p = g_aligned_alloc (5, sizeof (*p), 256);
  g_assert_nonnull (p);
  g_assert_cmpuint (((guintptr) p) % 256, ==, 0);

  g_aligned_free (p);
}

static void
test_aligned_mem_zeroed (void)
{
  gsize n_blocks = 10;
  guint *p;
  gsize i;

  g_test_summary ("Check that g_aligned_alloc0() zeroes out its allocation");

  p = g_aligned_alloc0 (n_blocks, sizeof (*p), 16);
  g_assert_nonnull (p);

  for (i = 0; i < n_blocks; i++)
    g_assert_cmpuint (p[i], ==, 0);

  g_aligned_free (p);
}

static void
test_aligned_mem_free_sized (void)
{
  gsize n_blocks = 10;
  guint *p;

  g_test_summary ("Check that g_aligned_free_sized() works");

  p = g_aligned_alloc (n_blocks, sizeof (*p), 16);
  g_assert_nonnull (p);

  g_aligned_free_sized (p, sizeof (*p), n_blocks * 16);

  /* NULL should be ignored */
  g_aligned_free_sized (NULL, sizeof (*p), n_blocks * 16);
}

static void
test_free_sized (void)
{
  gpointer p;

  g_test_summary ("Check that g_free_sized() works");

  p = g_malloc (123);
  g_assert_nonnull (p);

  g_free_sized (p, 123);

  /* NULL should be ignored */
  g_free_sized (NULL, 123);
}

static void
test_nullify (void)
{
  gpointer p = &test_nullify;

  g_assert (p != NULL);

  g_nullify_pointer (&p);

  g_assert (p == NULL);
}

static void
atexit_func (void)
{
  g_print ("atexit called");
}

static void
test_atexit (void)
{
  if (g_test_subprocess ())
    {
      g_atexit (atexit_func);
      return;
    }
  g_test_trap_subprocess (NULL, 0, G_TEST_SUBPROCESS_DEFAULT);
  g_test_trap_assert_passed ();
  g_test_trap_assert_stdout ("*atexit called*");
}

static void
test_check_setuid (void)
{
  gboolean res;

  res = GLIB_PRIVATE_CALL(g_check_setuid) ();
  g_assert (!res);
}

/* Test the defined integer limits are correct, as some compilers have had
 * problems with signed/unsigned conversion in the past. These limits should not
 * vary between platforms, compilers or architectures.
 *
 * Use string comparisons to avoid the same systematic problems with unary minus
 * application in C++. See https://gitlab.gnome.org/GNOME/glib/issues/1663. */
static void
test_int_limits (void)
{
  gchar *str = NULL;

  g_test_bug ("https://gitlab.gnome.org/GNOME/glib/issues/1663");

  str = g_strdup_printf ("%d %d %u\n"
                         "%" G_GINT16_FORMAT " %" G_GINT16_FORMAT " %" G_GUINT16_FORMAT "\n"
                         "%" G_GINT32_FORMAT " %" G_GINT32_FORMAT " %" G_GUINT32_FORMAT "\n"
                         "%" G_GINT64_FORMAT " %" G_GINT64_FORMAT " %" G_GUINT64_FORMAT "\n",
                         G_MININT8, G_MAXINT8, G_MAXUINT8,
                         G_MININT16, G_MAXINT16, G_MAXUINT16,
                         G_MININT32, G_MAXINT32, G_MAXUINT32,
                         G_MININT64, G_MAXINT64, G_MAXUINT64);

  g_assert_cmpstr (str, ==,
                   "-128 127 255\n"
                   "-32768 32767 65535\n"
                   "-2147483648 2147483647 4294967295\n"
                   "-9223372036854775808 9223372036854775807 18446744073709551615\n");
  g_free (str);
}

static void
test_clear_list (void)
{
    GList *list = NULL;

    g_clear_list (&list, NULL);
    g_assert_null (list);

    list = g_list_prepend (list, "test");
    g_assert_nonnull (list);

    g_clear_list (&list, NULL);
    g_assert_null (list);

    g_clear_list (&list, g_free);
    g_assert_null (list);

    list = g_list_prepend (list, g_malloc (16));
    g_assert_nonnull (list);

    g_clear_list (&list, g_free);
    g_assert_null (list);
}

static void
test_clear_slist (void)
{
    GSList *slist = NULL;

    g_clear_slist (&slist, NULL);
    g_assert_null (slist);

    slist = g_slist_prepend (slist, "test");
    g_assert_nonnull (slist);

    g_clear_slist (&slist, NULL);
    g_assert_null (slist);

    g_clear_slist (&slist, g_free);
    g_assert_null (slist);

    slist = g_slist_prepend (slist, g_malloc (16));
    g_assert_nonnull (slist);

    g_clear_slist (&slist, g_free);
    g_assert_null (slist);
}

int
main (int   argc,
      char *argv[])
{
  argv0 = argv[0];

  /* for tmpdir test, need to do this early before g_get_any_init */
  g_setenv ("TMPDIR", "", TRUE);
  g_unsetenv ("TMP");
  g_unsetenv ("TEMP");

  /* g_test_init() only calls g_set_prgname() if g_get_prgname()
   * returns %NULL, but g_get_prgname() on Windows never returns NULL.
   * So we need to do this by hand to make test_appname() work on
   * Windows.
   */
  g_set_prgname (argv[0]);

  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/utils/language-names", test_language_names);
  g_test_add_func ("/utils/locale-variants", test_locale_variants);
  g_test_add_func ("/utils/version", test_version);
  g_test_add_func ("/utils/appname", test_appname);
  g_test_add_func ("/utils/prgname-thread-safety", test_prgname_thread_safety);
  g_test_add_func ("/utils/tmpdir", test_tmpdir);
  g_test_add_func ("/utils/basic_bits", test_basic_bits);
  g_test_add_func ("/utils/bits", test_bits);
  g_test_add_func ("/utils/swap", test_swap);
  g_test_add_func ("/utils/find-program", test_find_program);
  g_test_add_func ("/utils/find-program-for-path", test_find_program_for_path);
  g_test_add_func ("/utils/debug", test_debug);
  g_test_add_func ("/utils/codeset", test_codeset);
  g_test_add_func ("/utils/codeset2", test_codeset2);
  g_test_add_func ("/utils/console-charset", test_console_charset);
  g_test_add_func ("/utils/gettext", test_gettext);
  g_test_add_func ("/utils/username", test_username);
  g_test_add_func ("/utils/realname", test_realname);
  g_test_add_func ("/utils/hostname", test_hostname);
#ifdef G_OS_UNIX
  g_test_add_func ("/utils/xdgdirs", test_xdg_dirs);
#endif
  g_test_add_func ("/utils/specialdir", test_special_dir);
  g_test_add_func ("/utils/specialdir/desktop", test_desktop_special_dir);
  g_test_add_func ("/utils/os-info", test_os_info);
  g_test_add_func ("/utils/clear-pointer", test_clear_pointer);
  g_test_add_func ("/utils/clear-pointer-cast", test_clear_pointer_cast);
  g_test_add_func ("/utils/clear-pointer/side-effects", test_clear_pointer_side_effects);
  g_test_add_func ("/utils/take-pointer", test_take_pointer);
  g_test_add_func ("/utils/clear-source", test_clear_source);
  g_test_add_func ("/utils/misc-mem", test_misc_mem);
  g_test_add_func ("/utils/aligned-mem", test_aligned_mem);
  g_test_add_func ("/utils/aligned-mem/subprocess/aligned_alloc_nz", aligned_alloc_nz);
  g_test_add_func ("/utils/aligned-mem/subprocess/aligned_alloc_npot", aligned_alloc_npot);
  g_test_add_func ("/utils/aligned-mem/subprocess/aligned_alloc_nmov", aligned_alloc_nmov);
  g_test_add_func ("/utils/aligned-mem/alignment", test_aligned_mem_alignment);
  g_test_add_func ("/utils/aligned-mem/zeroed", test_aligned_mem_zeroed);
  g_test_add_func ("/utils/aligned-mem/free-sized", test_aligned_mem_free_sized);
  g_test_add_func ("/utils/free-sized", test_free_sized);
  g_test_add_func ("/utils/nullify", test_nullify);
  g_test_add_func ("/utils/atexit", test_atexit);
  g_test_add_func ("/utils/check-setuid", test_check_setuid);
  g_test_add_func ("/utils/int-limits", test_int_limits);
  g_test_add_func ("/utils/clear-list", test_clear_list);
  g_test_add_func ("/utils/clear-slist", test_clear_slist);

  return g_test_run ();
}
