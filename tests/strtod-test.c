#undef G_DISABLE_ASSERT
#undef G_LOG_DOMAIN

#include <glib.h>
#include <locale.h>
#include <string.h>
#include <math.h>

void
test_string (char *number, double res)
{
  gdouble d;
  char *locales[] = {"sv_SE", "en_US", "fa_IR", "C"};
  int l;
  char *end;
  char *dummy;
  
  /* we try a copy of number, with some free space for malloc before that. 
   * This is supposed to smash the some wrong pointer calculations. */

  dummy = g_malloc (100000);
  number = g_strdup (number);
  g_free (dummy);

  for (l = 0; l < G_N_ELEMENTS (locales); l++)
    {
      setlocale (LC_NUMERIC, locales[l]);
      d = g_ascii_strtod (number, &end);
      if (d != res)
	g_print ("g_ascii_strtod for locale %s failed\n", locales[l]);
      if (end - number != strlen(number))
	g_print ("g_ascii_strtod for locale %s endptr was wrong\n", locales[l]);
    }
  
  g_free (number);
}


int 
main ()
{
  gdouble d;
  char buffer[G_ASCII_DTOSTR_BUF_SIZE];

  test_string ("123.123", 123.123);
  test_string ("123.123e2", 123.123e2);
  test_string ("123.123e-2", 123.123e-2);
  test_string ("-123.123", -123.123);
  test_string ("-123.123e2", -123.123e2);
  test_string ("-123.123e-2", -123.123e-2);
  
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
