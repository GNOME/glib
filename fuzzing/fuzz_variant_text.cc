#include "fuzz.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  fuzz_set_logging_func ();
  const gchar *gdata = (const gchar*) data;
  g_autoptr (GVariant) v = g_variant_parse (NULL, gdata, gdata + size, NULL,
                                            NULL);
  if (v == NULL)
    return 0;
  g_autofree gchar *tmp = g_variant_print (v, TRUE);
  return 0;
}
