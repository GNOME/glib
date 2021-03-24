#include "fuzz.h"

int
LLVMFuzzerTestOneInput (const unsigned char *data, size_t size)
{
  unsigned char *nul_terminated_data = NULL;
  gchar *canonicalized = NULL;

  fuzz_set_logging_func ();

  /* ignore @size (g_canonicalize_filename() doesn’t support it); ensure @data is nul-terminated */
  nul_terminated_data = (unsigned char *) g_strndup ((const gchar *) data, size);
  canonicalized = g_canonicalize_filename ((const gchar *) nul_terminated_data, "/");
  g_free (nul_terminated_data);

  g_free (canonicalized);

  return 0;
}
