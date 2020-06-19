#include "fuzz.h"

int
LLVMFuzzerTestOneInput (const unsigned char *data, size_t size)
{
  GBytes *unescaped_bytes = NULL;
  gchar *escaped_string = NULL;

  fuzz_set_logging_func ();

  if (size > G_MAXSSIZE)
    return 0;

  unescaped_bytes = g_uri_unescape_bytes ((const gchar *) data, (gssize) size);
  if (unescaped_bytes == NULL)
    return 0;

  escaped_string = g_uri_escape_bytes (g_bytes_get_data (unescaped_bytes, NULL),
                                       g_bytes_get_size (unescaped_bytes),
                                       NULL);
  g_bytes_unref (unescaped_bytes);

  if (escaped_string == NULL)
    return 0;

  g_free (escaped_string);

  return 0;
}
