#include "fuzz.h"

int
LLVMFuzzerTestOneInput (const unsigned char *data, size_t size)
{
  GHashTable *parsed_params = NULL;

  fuzz_set_logging_func ();

  if (size > G_MAXSSIZE)
    return 0;

  parsed_params = g_uri_parse_params ((const gchar *) data, (gssize) size, "&", G_URI_PARAMS_NONE);
  if (parsed_params == NULL)
    return 0;

  g_hash_table_unref (parsed_params);

  return 0;
}
