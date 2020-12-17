#include "fuzz.h"

static void
test_parse (const gchar   *data,
            size_t         size,
            GKeyFileFlags  flags)
{
  GKeyFile *key = NULL;

  key = g_key_file_new ();
  g_key_file_load_from_data (key, (const gchar*) data, size, G_KEY_FILE_NONE,
                             NULL);

  g_key_file_free (key);
}

int
LLVMFuzzerTestOneInput (const unsigned char *data, size_t size)
{
  fuzz_set_logging_func ();

  test_parse ((const gchar *) data, size, G_KEY_FILE_NONE);
  test_parse ((const gchar *) data, size, G_KEY_FILE_KEEP_COMMENTS);
  test_parse ((const gchar *) data, size, G_KEY_FILE_KEEP_TRANSLATIONS);

  return 0;
}
