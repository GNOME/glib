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


int
main (int argc, char *argv[])
{
  setlocale (LC_ALL, "");

  g_type_init ();
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/gschema/no-default", test_no_default);

  return g_test_run ();
}
