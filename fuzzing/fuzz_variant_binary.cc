#include "glib/glib.h"
#include <stdint.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  g_autoptr (GVariant) v = g_variant_new_from_data (G_VARIANT_TYPE_VARIANT,
                                                    data, size, FALSE, NULL,
                                                    NULL);
  if (v == NULL)
    return 0;
  g_variant_get_normal_form(v);
  g_variant_get_data(v);
  return 0;
}
