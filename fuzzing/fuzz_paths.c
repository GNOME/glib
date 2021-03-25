#include "fuzz.h"

int
LLVMFuzzerTestOneInput (const unsigned char *data, size_t size)
{
  unsigned char *nul_terminated_data = NULL;
  const gchar *skipped_root;
  gchar *basename = NULL, *dirname = NULL;

  fuzz_set_logging_func ();

  /* ignore @size (none of the functions support it); ensure @data is nul-terminated */
  nul_terminated_data = (unsigned char *) g_strndup ((const gchar *) data, size);

  g_path_is_absolute ((const gchar *) nul_terminated_data);

  skipped_root = g_path_skip_root ((const gchar *) nul_terminated_data);
  g_assert (skipped_root == NULL || skipped_root >= (const gchar *) nul_terminated_data);
  g_assert (skipped_root == NULL || skipped_root <= (const gchar *) nul_terminated_data + size);

  basename = g_path_get_basename ((const gchar *) nul_terminated_data);
  g_assert (strcmp (basename, ".") == 0 || strlen (basename) <= size);

  dirname = g_path_get_dirname ((const gchar *) nul_terminated_data);
  g_assert (strcmp (dirname, ".") == 0 || strlen (dirname) <= size);

  g_free (nul_terminated_data);
  g_free (dirname);
  g_free (basename);

  return 0;
}
