/* 
 * Copyright (C) 2011 Red Hat, Inc.
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
 * Authors: Colin Walters <walters@verbum.org>
 */

#include "config.h"
#include <glib.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static void
test_platform_argv0 (void)
{
  GOptionContext *context;
  gboolean arg;
  GOptionEntry entries [] =
    { { "test", 't', 0, G_OPTION_ARG_STRING, &arg, NULL, NULL },
      { NULL } };
  gboolean retval;
  gboolean fatal_errors = TRUE;

#if !defined HAVE_PROC_SELF_CMDLINE && !defined __OpenBSD__
  fatal_errors = FALSE;
#endif

  context = g_option_context_new (NULL);
  g_option_context_add_main_entries (context, entries, NULL);
  
  retval = g_option_context_parse (context, NULL, NULL, NULL);
  if (fatal_errors)
    {
      g_assert (retval == TRUE);
      g_assert (g_strcmp0 (g_get_prgname(), "option-argv0") == 0
		|| g_strcmp0 (g_get_prgname(), "lt-option-argv0") == 0
#ifdef G_OS_WIN32
                || g_strcmp0 (g_get_prgname(), "option-argv0.exe") == 0
#endif
               );
    }
  else
    {
      gboolean failed = FALSE;

      if (!retval)
        {
          g_print ("g_option_context_parse() failed\n");
          failed = TRUE;
        }
      else if (g_strcmp0 (g_get_prgname(), "option-argv0") != 0 &&
               g_strcmp0 (g_get_prgname(), "lt-option-argv0") != 0 &&
#ifdef G_OS_WIN32
               g_strcmp0 (g_get_prgname(), "option-argv0.exe") != 0
#else
               TRUE
#endif
              )
        {
          g_print ("program name `%s' is neither `option-argv0', nor `lt-option-argv0'\n", g_get_prgname());
          failed = TRUE;
        }
      else
        g_print ("The test unexpectedly passed\n");
      if (failed)
        g_test_skip ("platform_get_argv0() is not implemented [correctly?] on this platform");
    }

  g_option_context_free (context);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, "no_g_set_prgname", NULL);

  g_test_add_func ("/option/argv0", test_platform_argv0);

  return g_test_run ();
}
