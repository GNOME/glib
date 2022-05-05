#include "gio/gio.h"
#include "glib/glib.h"

int LLVMFuzzerTestOneInput (const unsigned char *data, size_t size);

#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
static GLogWriterOutput
empty_logging_func (GLogLevelFlags log_level, const GLogField *fields,
                    gsize n_fields, gpointer user_data)
{
  return G_LOG_WRITER_HANDLED;
}
#endif

/* Disables logging for oss-fuzz. Must be used with each target. */
static void
fuzz_set_logging_func (void)
{
#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
  static gboolean writer_set = FALSE;

  if (!writer_set)
    {
      g_log_set_writer_func (empty_logging_func, NULL, NULL);
      writer_set = TRUE;
    }
#endif
}
