#include "gio/gio.h"
#include "glib/glib.h"

#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION // oss-fuzz
static GLogWriterOutput
empty_logging_func (GLogLevelFlags log_level, const GLogField *fields,
                    gsize n_fields, gpointer user_data)
{
  return G_LOG_WRITER_HANDLED;
}

static void
fuzz_set_logging_func (void)
{
  g_log_set_writer_func (empty_logging_func, NULL, NULL);
}
#else
int LLVMFuzzerTestOneInput(const unsigned char *data, size_t size);
static void fuzz_set_logging_func (void) {};
#endif
