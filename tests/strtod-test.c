#undef G_DISABLE_ASSERT
#undef G_LOG_DOMAIN

/* for NAN and INFINITY */
#define _ISOC99_SOURCE

#include <glib.h>
#include <locale.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

void
test_string (char *number, double res, gboolean check_end, int correct_len)
{
  double d;
  char *locales[] = {"sv_SE", "en_US", "fa_IR", "C", "ru_RU"};
  int l;
  char *dummy;
  
  /* we try a copy of number, with some free space for malloc before that. 
   * This is supposed to smash the some wrong pointer calculations. */

  dummy = g_malloc (100000);
  number = g_strdup (number);
  g_free (dummy);

  for (l = 0; l < G_N_ELEMENTS (locales); l++)
    {
      gboolean ok;
      char *end = "(unset)";

      setlocale (LC_NUMERIC, locales[l]);
      d = g_ascii_strtod (number, &end);
      ok = isnan (res) ? isnan (d) : (d == res);
      if (!ok)
	{
	  g_print ("g_ascii_strtod on \"%s\" for locale %s failed\n", number, locales[l]);
	  g_print ("expected %f (nan %d) actual %f (nan %d)\n", 
		   res, isnan (res),
		   d, isnan (d));
	}

      ok = (end - number) == (check_end ? correct_len : strlen (number));
      if (!ok) {
	if (end == NULL)
	  g_print ("g_ascii_strtod on \"%s\" for locale %s endptr was NULL\n",
		   number, locales[l]);
	else if (end >= number && end <= number + strlen (number))
	  g_print ("g_ascii_strtod on \"%s\" for locale %s endptr was wrong, leftover: \"%s\"\n",
		   number, locales[l], end);
	else
	  g_print ("g_ascii_strtod on \"%s\" for locale %s endptr was REALLY wrong (number=%p, end=%p)\n",
		   number, locales[l], number, end);
      }
    }

  g_free (number);
}


int 
main ()
{
  gdouble d, our_nan, our_inf;
  char buffer[G_ASCII_DTOSTR_BUF_SIZE];

#ifdef NAN
  our_nan = NAN;
#else
  /* Do this before any call to setlocale.  */
  our_nan = atof ("NaN");
#endif
  g_assert (isnan (our_nan));

#ifdef INFINITY
  our_inf = INFINITY;
#else
  our_inf = atof ("Infinity");
#endif
  g_assert (our_inf > 1 && our_inf == our_inf / 2);

  test_string ("123.123", 123.123, FALSE, 0);
  test_string ("123.123e2", 123.123e2, FALSE, 0);
  test_string ("123.123e-2", 123.123e-2, FALSE, 0);
  test_string ("-123.123", -123.123, FALSE, 0);
  test_string ("-123.123e2", -123.123e2, FALSE, 0);
  test_string ("-123.123e-2", -123.123e-2, FALSE, 0);
  test_string ("5.4", 5.4, TRUE, 3);
  test_string ("5.4,5.5", 5.4, TRUE, 3);
  test_string ("5,4", 5.0, TRUE, 1);
  /* the following are for #156421 */
  test_string ("1e1", 1e1, FALSE, 0); 
  test_string ("NAN", our_nan, FALSE, 0);
  test_string ("-nan", -our_nan, FALSE, 0);
  test_string ("INF", our_inf, FALSE, 0);
  test_string ("-infinity", -our_inf, FALSE, 0);
  test_string ("-.75,0", -0.75, TRUE, 4);
  
  d = 179769313486231570814527423731704356798070567525844996598917476803157260780028538760589558632766878171540458953514382464234321326889464182768467546703537516986049910576551282076245490090389328944075868508455133942304583236903222948165808559332123348274797826204144723168738177180919299881250404026184124858368.0;
  g_assert (d == g_ascii_strtod (g_ascii_dtostr (buffer, sizeof (buffer), d), NULL));

  d = -179769313486231570814527423731704356798070567525844996598917476803157260780028538760589558632766878171540458953514382464234321326889464182768467546703537516986049910576551282076245490090389328944075868508455133942304583236903222948165808559332123348274797826204144723168738177180919299881250404026184124858368.0;
  g_assert (d == g_ascii_strtod (g_ascii_dtostr (buffer, sizeof (buffer), d), NULL));
  
  d = pow (2.0, -1024.1);
  g_assert (d == g_ascii_strtod (g_ascii_dtostr (buffer, sizeof (buffer), d), NULL));
  
  d = -pow (2.0, -1024.1);
  g_assert (d == g_ascii_strtod (g_ascii_dtostr (buffer, sizeof (buffer), d), NULL));
  

  return 0;
}
