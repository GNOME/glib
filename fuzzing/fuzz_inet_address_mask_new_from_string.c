#include "fuzz.h"

int
LLVMFuzzerTestOneInput (const unsigned char *data, size_t size)
{
  unsigned char *nul_terminated_data = NULL;
  GInetAddressMask *mask = NULL;

  fuzz_set_logging_func ();

  /* ignore @size (the function doesn’t support it); ensure @data is nul-terminated */
  nul_terminated_data = (unsigned char *) g_strndup ((const gchar *) data, size);
  mask = g_inet_address_mask_new_from_string ((const gchar *) nul_terminated_data, NULL);
  g_free (nul_terminated_data);

  if (mask != NULL)
    {
      gchar *text = g_inet_address_mask_to_string (mask);
      g_free (text);
    }

  g_clear_object (&mask);

  return 0;
}
