#include "fuzz.h"

static void
test_with_flags (const gchar *data,
                 GUriFlags    flags)
{
  GUri *uri = NULL;
  gchar *uri_string = NULL;

  uri = g_uri_parse (data, flags, NULL);

  if (uri == NULL)
    return;

  uri_string = g_uri_to_string (uri);
  g_uri_unref (uri);

  if (uri_string == NULL)
    return;

  g_free (uri_string);
}

int
LLVMFuzzerTestOneInput (const unsigned char *data, size_t size)
{
  unsigned char *nul_terminated_data = NULL;

  fuzz_set_logging_func ();

  /* ignore @size (g_uri_parse() doesn’t support it); ensure @data is nul-terminated */
  nul_terminated_data = (unsigned char *) g_strndup ((const gchar *) data, size);
  test_with_flags ((const gchar *) nul_terminated_data, G_URI_FLAGS_NONE);
  test_with_flags ((const gchar *) nul_terminated_data, G_URI_FLAGS_PARSE_STRICT);
  g_free (nul_terminated_data);

  return 0;
}
