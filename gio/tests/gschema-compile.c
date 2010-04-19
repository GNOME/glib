#include <stdlib.h>
#include <unistd.h>
#include <locale.h>
#include <libintl.h>
#include <gio.h>
#include <gstdio.h>

static void
test_no_default (void)
{
  g_remove ("gschemas.compiled");

  if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR))
    {
      gchar *argv[] = {
        "../gschema-compile",
        SRCDIR "/schema-tests/no-default/",
        "--targetdir=.",
        NULL
      };
      gchar *envp[] = { NULL };
      execve (argv[0], argv, envp);
    }
  g_test_trap_assert_failed ();
  g_test_trap_assert_stderr ("*<default> is required in <key>*");
}

static void
test_missing_quotes (void)
{
  g_remove ("gschemas.compiled");

  if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR))
    {
      gchar *argv[] = {
        "../gschema-compile",
        SRCDIR "/schema-tests/missing-quotes/",
        "--targetdir=.",
        NULL
      };
      gchar *envp[] = { NULL };
      execve (argv[0], argv, envp);
    }
  g_test_trap_assert_failed ();
  g_test_trap_assert_stderr ("*unknown keyword*");
}

static void
test_incomplete_list (void)
{
  g_remove ("gschemas.compiled");

  if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR))
    {
      gchar *argv[] = {
        "../gschema-compile",
        SRCDIR "/schema-tests/incomplete-list/",
        "--targetdir=.",
        NULL
      };
      gchar *envp[] = { NULL };
      execve (argv[0], argv, envp);
    }
  g_test_trap_assert_failed ();
  g_test_trap_assert_stderr ("*to follow array element*");
}

static void
test_wrong_category (void)
{
  g_remove ("gschemas.compiled");

  if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR))
    {
      gchar *argv[] = {
        "../gschema-compile",
        SRCDIR "/schema-tests/wrong-category/",
        "--targetdir=.",
        NULL
      };
      gchar *envp[] = { NULL };
      execve (argv[0], argv, envp);
    }
  g_test_trap_assert_failed ();
  g_test_trap_assert_stderr ("*attribute 'l10n' invalid*");
}

static void
test_bad_type (void)
{
  g_remove ("gschemas.compiled");

  if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR))
    {
      gchar *argv[] = {
        "../gschema-compile",
        SRCDIR "/schema-tests/bad-type/",
        "--targetdir=.",
        NULL
      };
      gchar *envp[] = { NULL };
      execve (argv[0], argv, envp);
    }
  g_test_trap_assert_failed ();
  g_test_trap_assert_stderr ("*invalid GVariant type string*");
}

static void
test_overflow (void)
{
  g_remove ("gschemas.compiled");

  if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR))
    {
      gchar *argv[] = {
        "../gschema-compile",
        SRCDIR "/schema-tests/overflow/",
        "--targetdir=.",
        NULL
      };
      gchar *envp[] = { NULL };
      execve (argv[0], argv, envp);
    }
  g_test_trap_assert_failed ();
  g_test_trap_assert_stderr ("*out of range*");
}

int
main (int argc, char *argv[])
{
  setlocale (LC_ALL, "");

  g_type_init ();
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/gschema/no-default", test_no_default);
  g_test_add_func ("/gschema/missing-quotes", test_missing_quotes);
  g_test_add_func ("/gschema/incomplete-list", test_incomplete_list);
  g_test_add_func ("/gschema/wrong-category", test_wrong_category);
  g_test_add_func ("/gschema/bad-type", test_bad_type);
  g_test_add_func ("/gschema/overflow", test_overflow);

  return g_test_run ();
}
