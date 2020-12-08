#include "fuzz.h"

int
LLVMFuzzerTestOneInput (const unsigned char *data, size_t size)
{
  unsigned char *nul_terminated_data = NULL;
  GSocketConnectable *connectable = NULL;

  fuzz_set_logging_func ();

  /* ignore @size (g_network_address_parse() doesn’t support it); ensure @data is nul-terminated */
  nul_terminated_data = (unsigned char *) g_strndup ((const gchar *) data, size);
  connectable = g_network_address_parse ((const gchar *) nul_terminated_data, 1, NULL);
  g_free (nul_terminated_data);

  if (connectable != NULL)
    {
      gchar *text = g_socket_connectable_to_string (connectable);
      g_free (text);
    }

  g_clear_object (&connectable);

  return 0;
}
