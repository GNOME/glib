#include "modules/symbol-visibility.h"
#define _GLIB_EXTERN GLIB_TEST_EXPORT_SYMBOL

#include <gio/gio.h>

void
g_io_module_load (GIOModule *module)
{
}

void
g_io_module_unload (GIOModule   *module)
{
}

char **
g_io_module_query (void)
{
  return NULL;
}

