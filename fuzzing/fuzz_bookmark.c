#include "glib/glib.h"

int
LLVMFuzzerTestOneInput (const unsigned char *data, size_t size)
{
  GBookmarkFile *bookmark = NULL;

  bookmark = g_bookmark_file_new ();
  g_bookmark_file_load_from_data (bookmark, (const gchar*) data, size, NULL);

  g_bookmark_file_free (bookmark);
  return 0;
}
