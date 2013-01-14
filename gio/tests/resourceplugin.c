#include "config.h"

#include <gio/gio.h>

_GLIB_EXTERN void
g_io_module_load (GIOModule *module)
{
}

_GLIB_EXTERN void
g_io_module_unload (GIOModule   *module)
{
}

_GLIB_EXTERN char **
g_io_module_query (void)
{
  return NULL;
}

