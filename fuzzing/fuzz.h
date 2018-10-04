#include "gio/gio.h"
#include "glib/glib.h"

#include <stdint.h>

static GLogWriterOutput empty_logging_func (GLogLevelFlags log_level,
                                            const GLogField *fields,
                                            gsize n_fields,
                                            gpointer user_data)
{
  return G_LOG_WRITER_HANDLED;
}

void fuzz_set_logging_func ()
{
#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
  g_log_set_writer_func (empty_logging_func, NULL, NULL);
#endif
}
