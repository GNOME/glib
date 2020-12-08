#include "fuzz.h"

int
LLVMFuzzerTestOneInput (const unsigned char *data, size_t size)
{
  unsigned char *nul_terminated_data = NULL;
  GDateTime *dt = NULL;

  fuzz_set_logging_func ();

  /* ignore @size (the function doesn’t support it); ensure @data is nul-terminated */
  nul_terminated_data = (unsigned char *) g_strndup ((const gchar *) data, size);
  dt = g_date_time_new_from_iso8601 ((const gchar *) nul_terminated_data, NULL);
  g_free (nul_terminated_data);

  if (dt != NULL)
    {
      gchar *text = g_date_time_format_iso8601 (dt);
      g_free (text);
    }

  g_clear_pointer (&dt, g_date_time_unref);

  return 0;
}
