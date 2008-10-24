/* GObject introspection: Parsed IDL
 *
 * Copyright (C) 2005 Matthias Clasen
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
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __G_IR_MODULE_H__
#define __G_IR_MODULE_H__

#include <glib.h>
#include "gtypelib.h"

G_BEGIN_DECLS


typedef struct _GIrModule GIrModule;

struct _GIrModule
{ 
  gchar *name;
  gchar *version;
  gchar *shared_library;
  GList *dependencies;
  GList *entries;
};

GIrModule *g_ir_module_new            (const gchar *name,
				       const gchar *nsversion,
				       const gchar *module_filename);
void       g_ir_module_free           (GIrModule  *module);

GTypelib * g_ir_module_build_typelib  (GIrModule  *module,
				       GList       *modules);

void _g_irnode_init_stats (void);
void _g_irnode_dump_stats (void);

G_END_DECLS

#endif  /* __G_IR_MODULE_H__ */
