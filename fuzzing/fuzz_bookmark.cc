#include "glib/glib.h"
#include <stdint.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  g_autoptr (GBookmarkFile) bookmark = g_bookmark_file_new ();
  g_bookmark_file_load_from_data (bookmark, (const gchar*) data, size, NULL);
  return 0;
}
