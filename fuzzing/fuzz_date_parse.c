#include "fuzz.h"

int
LLVMFuzzerTestOneInput (const unsigned char *data, size_t size)
{
  unsigned char *nul_terminated_data = NULL;
  GDate *date = g_date_new ();

  fuzz_set_logging_func ();

  /* ignore @size (g_date_set_parse() doesn’t support it); ensure @data is nul-terminated */
  nul_terminated_data = (unsigned char *) g_strndup ((const gchar *) data, size);
  g_date_set_parse (date, (const gchar *) nul_terminated_data);
  g_free (nul_terminated_data);

  g_date_free (date);

  return 0;
}
