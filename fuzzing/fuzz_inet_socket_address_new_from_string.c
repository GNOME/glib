#include "fuzz.h"

int
LLVMFuzzerTestOneInput (const unsigned char *data, size_t size)
{
  unsigned char *nul_terminated_data = NULL;
  GSocketAddress *addr = NULL;

  fuzz_set_logging_func ();

  /* ignore @size (the function doesn’t support it); ensure @data is nul-terminated */
  nul_terminated_data = (unsigned char *) g_strndup ((const gchar *) data, size);
  addr = g_inet_socket_address_new_from_string ((const gchar *) nul_terminated_data, 1);
  g_free (nul_terminated_data);

  if (addr != NULL)
    {
      gchar *text = g_socket_connectable_to_string (G_SOCKET_CONNECTABLE (addr));
      g_free (text);
    }

  g_clear_object (&addr);

  return 0;
}
