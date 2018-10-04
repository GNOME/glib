#include "fuzz.h"

static GDBusCapabilityFlags flags = G_DBUS_CAPABILITY_FLAGS_UNIX_FD_PASSING;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  fuzz_set_logging_func ();
  gssize bytes = g_dbus_message_bytes_needed ((guchar*) data, size, NULL);
  if (bytes <= 0)
    return 0;

  g_autoptr (GDBusMessage) msg = g_dbus_message_new_from_blob ((guchar*) data,
                                                               size, flags,
                                                               NULL);
  if (msg == NULL)
    return 0;

  gsize msg_size;
  g_autofree guchar *tmp = g_dbus_message_to_blob (msg, &msg_size, flags, NULL);
  return 0;
}
