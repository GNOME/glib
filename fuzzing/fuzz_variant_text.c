#include "fuzz.h"

int LLVMFuzzerTestOneInput(const unsigned char *data, size_t size) {
  const gchar *gdata = (const gchar*) data;
  g_autoptr (GVariant) variant = NULL;
  g_autofree gchar *text = NULL;

  fuzz_set_logging_func ();

  variant = g_variant_parse (NULL, gdata, gdata + size, NULL, NULL);
  if (variant == NULL)
    return 0;

  text = g_variant_print (variant, TRUE);
  return 0;
}
