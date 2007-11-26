/* GIO - GLib Input, Output and Streaming Library
 * 
 * Copyright (C) 2006-2007 Red Hat, Inc.
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
 *
 * Author: Alexander Larsson <alexl@redhat.com>
 */

#ifndef __G_IO_MODULE_H__
#define __G_IO_MODULE_H__

#include <glib-object.h>
#include <gmodule.h>

G_BEGIN_DECLS

#define G_IO_TYPE_MODULE         (g_io_module_get_type ())
#define G_IO_MODULE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), G_IO_TYPE_MODULE, GIOModule))
#define G_IO_MODULE_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), G_IO_TYPE_MODULE, GIOModuleClass))
#define G_IO_IS_MODULE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), G_IO_TYPE_MODULE))
#define G_IO_IS_MODULE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), G_IO_TYPE_MODULE))
#define G_IO_MODULE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), G_IO_TYPE_MODULE, GIOModuleClass))

typedef struct _GIOModule GIOModule;
typedef struct _GIOModuleClass GIOModuleClass;

GType      g_io_module_get_type (void) G_GNUC_CONST;
GIOModule *g_io_module_new      (const gchar *filename);

void       g_io_modules_ensure_loaded (const char *directory);

/* API for the modules to implement */
void        g_io_module_load     (GIOModule   *module);
void        g_io_module_unload   (GIOModule   *module);

G_END_DECLS

#endif /* __G_IO_MODULE_H__ */
