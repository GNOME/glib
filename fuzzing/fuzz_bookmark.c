#include "fuzz.h"

int
LLVMFuzzerTestOneInput (const unsigned char *data, size_t size)
{
  g_autoptr (GBookmarkFile) bookmark = NULL;

  fuzz_set_logging_func ();

  bookmark = g_bookmark_file_new ();
  g_bookmark_file_load_from_data (bookmark, (const gchar*) data, size, NULL);
  return 0;
}
