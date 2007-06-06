/* GObject - GLib Type, Object, Parameter and Signal Library
 * Copyright (C) 2000 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#include	"gtypeplugin.h"
#include	"gobjectalias.h"



/* --- functions --- */
GType
g_type_plugin_get_type (void)
{
  static GType type_plugin_type = 0;
  
  if (!type_plugin_type)
    {
      static const GTypeInfo type_plugin_info = {
	sizeof (GTypePluginClass),
	NULL,           /* base_init */
	NULL,           /* base_finalize */
      };
      
      type_plugin_type = g_type_register_static (G_TYPE_INTERFACE, g_intern_static_string ("GTypePlugin"), &type_plugin_info, 0);
    }
  
  return type_plugin_type;
}

void
g_type_plugin_use (GTypePlugin *plugin)
{
  GTypePluginClass *iface;
  
  g_return_if_fail (G_IS_TYPE_PLUGIN (plugin));
  
  iface = G_TYPE_PLUGIN_GET_CLASS (plugin);
  iface->use_plugin (plugin);
}

void
g_type_plugin_unuse (GTypePlugin *plugin)
{
  GTypePluginClass *iface;
  
  g_return_if_fail (G_IS_TYPE_PLUGIN (plugin));
  
  iface = G_TYPE_PLUGIN_GET_CLASS (plugin);
  iface->unuse_plugin (plugin);
}

void
g_type_plugin_complete_type_info (GTypePlugin     *plugin,
				  GType            g_type,
				  GTypeInfo       *info,
				  GTypeValueTable *value_table)
{
  GTypePluginClass *iface;
  
  g_return_if_fail (G_IS_TYPE_PLUGIN (plugin));
  g_return_if_fail (info != NULL);
  g_return_if_fail (value_table != NULL);
  
  iface = G_TYPE_PLUGIN_GET_CLASS (plugin);
  iface->complete_type_info (plugin,
			     g_type,
			     info,
			     value_table);
}

void
g_type_plugin_complete_interface_info (GTypePlugin    *plugin,
				       GType           instance_type,
				       GType           interface_type,
				       GInterfaceInfo *info)
{
  GTypePluginClass *iface;
  
  g_return_if_fail (G_IS_TYPE_PLUGIN (plugin));
  g_return_if_fail (info != NULL);
  
  iface = G_TYPE_PLUGIN_GET_CLASS (plugin);
  iface->complete_interface_info (plugin,
				  instance_type,
				  interface_type,
				  info);
}

#define __G_TYPE_PLUGIN_C__
#include "gobjectaliasdef.c"
