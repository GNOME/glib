#include "fuzz.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  fuzz_set_logging_func ();
  g_autoptr (GKeyFile) key = g_key_file_new ();
  g_key_file_load_from_data (key, (const gchar*) data, size, G_KEY_FILE_NONE,
                             NULL);
  return 0;
}
