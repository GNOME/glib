#include <stdlib.h>
#include <unistd.h>
#include <locale.h>
#include <libintl.h>
#include <gio.h>
#include <gstdio.h>

typedef struct {
  const gchar *name;
  const gchar *stderr;
} SchemaTest;

static void
test_schema (gpointer data)
{
  SchemaTest *test = (SchemaTest *) data;

  if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDERR))
    {
      gchar *filename = g_strconcat (test->name, ".gschema.xml", NULL);
      gchar *path = g_build_filename (SRCDIR, "schema-tests", filename, NULL);
      gchar *argv[] = {
        "../gschema-compile",
        "--dry-run",
        "--one-schema-file", path,
        NULL
      };
      gchar *envp[] = { NULL };
      execve (argv[0], argv, envp);
      g_free (filename);
      g_free (path);
    }
  g_test_trap_assert_failed ();
  g_test_trap_assert_stderr (test->stderr);
}

static const SchemaTest tests[] = {
  { "no-default",       "*<default> is required in <key>*"  },
  { "missing-quotes",   "*unknown keyword*"                 },
  { "incomplete-list",  "*to follow array element*"         },
  { "wrong-category",   "*attribute 'l10n' invalid*"        },
  { "bad-type",         "*invalid GVariant type string*"    },
  { "overflow",         "*out of range*"                    },
  { "bad-key",          "*invalid name*"                    },
  { "bad-key2",         "*invalid name*"                    },
  { "bad-key3",         "*invalid name*"                    },
  { "bad-key4",         "*invalid name*"                    },
  { "empty-key",        "*empty names*"                     },
};

int
main (int argc, char *argv[])
{
  guint i;

  setlocale (LC_ALL, "");

  g_type_init ();
  g_test_init (&argc, &argv, NULL);

  for (i = 0; i < G_N_ELEMENTS (tests); ++i)
    {
      gchar *name = g_strdup_printf ("/gschema/%s", tests[i].name);
      g_test_add_data_func (name, &tests[i], (gpointer) test_schema);
      g_free (name);
    }

  return g_test_run ();
}
