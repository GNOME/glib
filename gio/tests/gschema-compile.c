#include <stdlib.h>
#include <unistd.h>
#include <locale.h>
#include <libintl.h>
#include <gio.h>
#include <gstdio.h>

typedef struct {
  const gchar *name;
  const gchar *opt;
  const gchar *err;
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
        "../glib-compile-schemas",
        "--dry-run",
        "--schema-files", path,
        (gchar *)test->opt,
        NULL
      };
      gchar *envp[] = { NULL };
      execve (argv[0], argv, envp);
      g_free (filename);
      g_free (path);
    }
  if (test->err)
    {
      g_test_trap_assert_failed ();
      g_test_trap_assert_stderr (test->err);
    }
  else
    g_test_trap_assert_passed();
}

static const SchemaTest tests[] = {
  { "no-default",                   NULL, "*<default> is required in <key>*"                    },
  { "missing-quotes",               NULL, "*unknown keyword*"                                   },
  { "incomplete-list",              NULL, "*to follow array element*"                           },
  { "wrong-category",               NULL, "*attribute 'l10n' invalid*"                          },
  { "bad-type",                     NULL, "*invalid GVariant type string*"                      },
  { "overflow",                     NULL, "*out of range*"                                      },
  { "range-wrong-type",             NULL, "*<range> not allowed for keys of type*"              },
  { "range-missing-min",            NULL, "*element 'range' requires attribute 'min'*"          },
  { "range-missing-max",            NULL, "*element 'range' requires attribute 'max'*"          },
  { "default-out-of-range",         NULL, "*<default> is not contained in the specified range*" },
  { "choices-wrong-type",           NULL, "*<choices> not allowed for keys of type*"            },
  { "choice-missing-value",         NULL, "*element 'choice' requires attribute 'value'*"       },
  { "default-not-in-choices",       NULL, "*<default> contains string not in <choices>*"        },
  { "array-default-not-in-choices", NULL, "*<default> contains string not in <choices>*"        },
  { "bad-key",                      NULL, "*invalid name*"                                      },
  { "bad-key",                      "--allow-any-name", NULL                                    },
  { "bad-key2",                     NULL, "*invalid name*"                                      },
  { "bad-key2",                     "--allow-any-name", NULL                                    },
  { "bad-key3",                     NULL, "*invalid name*"                                      },
  { "bad-key3",                     "--allow-any-name", NULL                                    },
  { "bad-key4",                     NULL, "*invalid name*"                                      },
  { "bad-key4",                     "--allow-any-name", NULL                                    },
  { "empty-key",                    NULL, "*empty names*"                                       },
  { "empty-key",                    "--allow-any-name", "*empty names*"                         }
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
      gchar *name = g_strdup_printf ("/gschema/%s%s", tests[i].name, tests[i].opt ? "/opt" : "");
      g_test_add_data_func (name, &tests[i], (gpointer) test_schema);
      g_free (name);
    }

  return g_test_run ();
}
