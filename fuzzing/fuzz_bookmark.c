#include "fuzz.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  fuzz_set_logging_func ();
  g_autoptr (GBookmarkFile) bookmark = g_bookmark_file_new ();
  g_bookmark_file_load_from_data (bookmark, (const gchar*) data, size, NULL);
  return 0;
}
