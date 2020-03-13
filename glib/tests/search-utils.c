#include "config.h"

#include <locale.h>
#include <glib.h>

typedef struct
{
  const gchar *string;
  const gchar *prefix;
  const gchar *locale;
  gboolean should_match;
} SearchTest;

static void
test_search (void)
{
  SearchTest tests[] = {
    /* Test word separators and case */
    { "Hello World", "he", "C", TRUE },
    { "Hello World", "wo", "C", TRUE },
    { "Hello World", "lo", "C", FALSE },
    { "Hello World", "ld", "C", FALSE },
    { "Hello-World", "wo", "C", TRUE },
    { "HelloWorld", "wo", "C", FALSE },

    /* Test composed chars (accentued letters) */
    { "Jörgen", "jor", "sv_SE.UTF-8", TRUE },
    { "Gaëtan", "gaetan", "fr_FR.UTF-8", TRUE },
    { "élève", "ele", "fr_FR.UTF-8", TRUE },
    { "Azais", "AzaÏs", "fr_FR.UTF-8", FALSE },
    { "AzaÏs", "Azais", "fr_FR.UTF-8", TRUE },

    /* Test decomposed chars, they looks the same, but are actually
     * composed of multiple unicodes */
    { "Jorgen", "Jör", "sv_SE.UTF-8", FALSE },
    { "Jörgen", "jor", "sv_SE.UTF-8", TRUE },

    /* Turkish special case */
    { "İstanbul", "ist", "tr_TR.UTF-8", TRUE },
    { "Diyarbakır", "diyarbakir", "tr_TR.UTF-8", TRUE },

    /* Test unicode chars when no locale is available */
    { "Jörgen", "jor", "C", TRUE },
    { "Jorgen", "Jör", "C", FALSE },
    { "Jörgen", "jor", "C", TRUE },

    /* Multi words */
    { "Xavier Claessens", "Xav Cla", "C", TRUE },
    { "Xavier Claessens", "Cla Xav", "C", TRUE },
    { "Foo Bar Baz", "   b  ", "C", TRUE },
    { "Foo Bar Baz", "bar bazz", "C", FALSE },

    { NULL, NULL, NULL, FALSE }
  };
  guint i;
  gchar *user_locale;

  g_debug ("Started");

  setlocale (LC_ALL, "");
  user_locale = setlocale (LC_ALL, NULL);
  g_debug ("Current user locale: %s", user_locale);

  for (i = 0; tests[i].string != NULL; i ++)
    {
      gboolean match;
      gboolean ok;
      gboolean skipped;

      if (setlocale (LC_ALL, tests[i].locale))
        {
          skipped = FALSE;
          match = g_str_match_string (tests[i].prefix, tests[i].string, TRUE);
          ok = (match == tests[i].should_match);
        }
      else
        {
          skipped = TRUE;
          g_info ("Locale '%s' is unavailable", tests[i].locale);
        }

      g_debug ("'%s' - '%s' %s: %s", tests[i].prefix, tests[i].string,
          tests[i].should_match ? "should match" : "should NOT match",
          skipped ? "SKIPPED" : ok ? "OK" : "FAILED");

      g_assert (skipped || ok);
    }
}

int
main (int argc,
      char **argv)
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/search", test_search);

  return g_test_run ();
}
