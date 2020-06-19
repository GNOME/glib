#include "fuzz.h"

int
LLVMFuzzerTestOneInput (const unsigned char *data, size_t size)
{
  GUri *uri = NULL;
  gchar *uri_string = NULL;
  const GUriFlags flags = G_URI_FLAGS_NONE;

  fuzz_set_logging_func ();

  /* ignore @size */
  uri = g_uri_parse ((const gchar *) data, flags, NULL);
  if (uri == NULL)
    return 0;

  uri_string = g_uri_to_string (uri);
  g_uri_unref (uri);

  if (uri_string == NULL)
    return 0;

  g_free (uri_string);

  return 0;
}
