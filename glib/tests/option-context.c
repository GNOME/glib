/* Unit tests for GOptionContext
 * Copyright (C) 2007 Openismus GmbH
 * Authors: Mathias Hasselmann
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
#include <glib/gtestutils.h>

#include <stdlib.h>

static void
group_captions (void)
{
  gchar *help_variants[] = { "--help", "--help-all", "--help-test" };

  GOptionEntry main_entries[] = {
    { "main-switch", 0,
      G_OPTION_FLAG_NO_ARG,
      G_OPTION_ARG_NONE, NULL,
      "A switch that is in the main group", NULL },
    { NULL }
  };

  GOptionEntry group_entries[] = {
    { "test-switch", 0,
      G_OPTION_FLAG_NO_ARG,
      G_OPTION_ARG_NONE, NULL,
      "A switch that is in the test group", NULL },
    { NULL }
  };

  gint i, j;

  g_test_bug ("504142");

  for (i = 0; i < 4; ++i)
    {
      gboolean have_main_entries = (0 != (i & 1));
      gboolean have_test_entries = (0 != (i & 2));

      GOptionContext *options;
      GOptionGroup   *group = NULL;

      options = g_option_context_new (NULL);

      if (have_main_entries)
        g_option_context_add_main_entries (options, main_entries, NULL);
      if (have_test_entries)
        {
          group = g_option_group_new ("test", "Test Options",
                                      "Show all test options",
                                      NULL, NULL);
          g_option_context_add_group (options, group);
          g_option_group_add_entries (group, group_entries);
        }

      for (j = 0; j < G_N_ELEMENTS (help_variants); ++j)
        {
          gchar *args[3];

          args[0] = __FILE__;
          args[1] = help_variants[j];
          args[2] = NULL;

          g_test_message ("test setup: args='%s', main-entries=%d, test-entries=%d",
                          args[1], have_main_entries, have_test_entries);

          if (g_test_trap_fork (0, G_TEST_TRAP_SILENCE_STDOUT |
                                   G_TEST_TRAP_SILENCE_STDERR))
            {
              gchar **argv = args;
              gint    argc = 2;
              GError *error = NULL;

              g_option_context_parse (options, &argc, &argv, &error);
              exit(0);
            }
          else
            {
              gboolean expect_main_description = FALSE;
              gboolean expect_main_switch      = FALSE;

              gboolean expect_test_description = FALSE;
              gboolean expect_test_switch      = FALSE;
              gboolean expect_test_group       = FALSE;

              g_test_trap_assert_passed ();
              g_test_trap_assert_stderr ("");

              switch (j)
                {
                  case 0:
                    g_assert_cmpstr ("--help", ==, args[1]);
                    expect_main_switch = have_main_entries;
                    expect_test_group  = have_test_entries;
                    break;

                  case 1:
                    g_assert_cmpstr ("--help-all", ==, args[1]);
                    expect_main_switch = have_main_entries;
                    expect_test_switch = have_test_entries;
                    expect_test_group  = have_test_entries;
                    break;

                  case 2:
                    g_assert_cmpstr ("--help-test", ==, args[1]);
                    expect_test_switch = have_test_entries;
                    break;

                  default:
                    g_assert_not_reached ();
                    break;
                }

              expect_main_description |= expect_main_switch;
              expect_test_description |= expect_test_switch;

              if (expect_main_description)
                g_test_trap_assert_stdout           ("*Application Options*");
              else
                g_test_trap_assert_stdout_unmatched ("*Application Options*");
              if (expect_main_switch)
                g_test_trap_assert_stdout           ("*--main-switch*");
              else
                g_test_trap_assert_stdout_unmatched ("*--main-switch*");

              if (expect_test_description)
                g_test_trap_assert_stdout           ("*Test Options*");
              else
                g_test_trap_assert_stdout_unmatched ("*Test Options*");
              if (expect_test_switch)
                g_test_trap_assert_stdout           ("*--test-switch*");
              else
                g_test_trap_assert_stdout_unmatched ("*--test-switch*");

              if (expect_test_group)
                g_test_trap_assert_stdout           ("*--help-test*");
              else
                g_test_trap_assert_stdout_unmatched ("*--help-test*");
            }
        }
    }
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_bug_base ("http://bugzilla.gnome.org/");
  g_test_add_func ("/group/captions", group_captions);

  return g_test_run();
}
