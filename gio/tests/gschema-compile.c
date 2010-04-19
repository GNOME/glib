#include <stdlib.h>
#include <unistd.h>
#include <locale.h>
#include <libintl.h>
#include <gio.h>
#include <gstdio.h>

static void
test_no_default (void)
{
  g_remove ("schema-tests/no-default/gschemas.compiled");

  if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR))
    {
       gchar *argv[] = { "../gschema-compile", "./schema-tests/no-default/", NULL };
       gchar *envp[] = { NULL };
       execve (argv[0], argv, envp);
    }
  g_test_trap_assert_failed ();
  g_test_trap_assert_stderr ("*<default> is required in <key>*");
}

static void
test_missing_quotes (void)
{
  g_remove ("schema-tests/missing-quotes/gschemas.compiled");

  if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR))
    {
       gchar *argv[] = { "../gschema-compile", "./schema-tests/missing-quotes/", NULL };
       gchar *envp[] = { NULL };
       execve (argv[0], argv, envp);
    }
  g_test_trap_assert_failed ();
  g_test_trap_assert_stderr ("*unknown keyword*");
}

static void
test_incomplete_list (void)
{
  g_remove ("schema-tests/incomplete-list/gschemas.compiled");

  if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR))
    {
       gchar *argv[] = { "../gschema-compile", "./schema-tests/incomplete-list/", NULL };
       gchar *envp[] = { NULL };
       execve (argv[0], argv, envp);
    }
  g_test_trap_assert_failed ();
  g_test_trap_assert_stderr ("*to follow array element*");
}

static void
test_wrong_category (void)
{
  g_remove ("schema-tests/wrong-category/gschemas.compiled");

  if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR))
    {
       gchar *argv[] = { "../gschema-compile", "./schema-tests/wrong-category/", NULL };
       gchar *envp[] = { NULL };
       execve (argv[0], argv, envp);
    }
  g_test_trap_assert_failed ();
  g_test_trap_assert_stderr ("*attribute 'l10n' invalid*");
}

static void
test_bad_type (void)
{
  g_remove ("schema-tests/bad-type/gschemas.compiled");

  if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR))
    {
       gchar *argv[] = { "../gschema-compile", "./schema-tests/bad-type/", NULL };
       gchar *envp[] = { NULL };
       execve (argv[0], argv, envp);
    }
  g_test_trap_assert_failed ();
  g_test_trap_assert_stderr ("*invalid GVariant type string*");
}

static void
test_overflow (void)
{
  g_remove ("schema-tests/overflow/gschemas.compiled");

  if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR))
    {
       gchar *argv[] = { "../gschema-compile", "./schema-tests/overflow/", NULL };
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
