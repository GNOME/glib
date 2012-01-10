#include <gio/gio.h>

extern GResource *_g_plugin_resource;

void
g_io_module_load (GIOModule *module)
{
  g_resource_set_module (_g_plugin_resource, G_TYPE_MODULE (module));
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
