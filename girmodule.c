/* GObject introspection: Typelib creation 
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

#include <stdio.h>
#include <string.h>

#include "girmodule.h"
#include "girnode.h"

#define ALIGN_VALUE(this, boundary) \
  (( ((unsigned long)(this)) + (((unsigned long)(boundary)) -1)) & (~(((unsigned long)(boundary))-1)))


GIrModule *
g_ir_module_new (const gchar *name, const gchar *shared_library)
{
  GIrModule *module;
  
  module = g_new (GIrModule, 1);

  module->name = g_strdup (name);
  if (shared_library)
      module->shared_library = g_strdup (shared_library);
  else
      module->shared_library = NULL;
  module->entries = NULL;

  return module;
}

void
g_ir_module_free (GIrModule *module)
{
  GList *e;

  g_free (module->name);

  for (e = module->entries; e; e = e->next)
    g_ir_node_free ((GIrNode *)e->data);

  g_list_free (module->entries);

  g_free (module);
}

GTypelib *
g_ir_module_build_typelib (GIrModule  *module,
			     GList       *modules)
{
  GTypelib *typelib;
  gsize length;
  gint i;
  GList *e;
  Header *header;
  DirEntry *entry;
  guint32 header_size;
  guint32 dir_size;
  guint32 n_entries;
  guint32 n_local_entries;
  guint32 size, offset, offset2, old_offset;
  GHashTable *strings;
  GHashTable *types;
  guchar *data;

  header_size = ALIGN_VALUE (sizeof (Header), 4);
  n_local_entries = g_list_length (module->entries);

 restart:
  init_stats ();
  strings = g_hash_table_new (g_str_hash, g_str_equal);
  types = g_hash_table_new (g_str_hash, g_str_equal);
  n_entries = g_list_length (module->entries);

  g_message ("%d entries (%d local)\n", n_entries, n_local_entries);
  
  dir_size = n_entries * 12;  
  size = header_size + dir_size;

  size += ALIGN_VALUE (strlen (module->name) + 1, 4);

  for (e = module->entries; e; e = e->next)
    {
      GIrNode *node = e->data;
      
      size += g_ir_node_get_full_size (node);
    }

  /* Adjust size for strings allocated in header below specially */
  size += strlen (module->name);
  if (module->shared_library) 
    size += strlen (module->shared_library);

  g_message ("allocating %d bytes (%d header, %d directory, %d entries)\n", 
	  size, header_size, dir_size, size - header_size - dir_size);

  data = g_malloc0 (size);

  /* fill in header */
  header = (Header *)data;
  memcpy (header, G_IR_MAGIC, 16);
  header->major_version = 1;
  header->minor_version = 0;
  header->reserved = 0;
  header->n_entries = n_entries;
  header->n_local_entries = n_local_entries;
  header->n_annotations = 0;
  header->annotations = 0; /* filled in later */
  header->size = 0; /* filled in later */
  header->namespace = write_string (module->name, strings, data, &header_size);
  header->shared_library = (module->shared_library?
                             write_string (module->shared_library, strings, data, &header_size)
                             : 0);
  header->directory = ALIGN_VALUE (header_size, 4);
  header->entry_blob_size = 12;
  header->function_blob_size = 16;
  header->callback_blob_size = 12;
  header->signal_blob_size = 12;
  header->vfunc_blob_size = 16;
  header->arg_blob_size = 12;
  header->property_blob_size = 12;
  header->field_blob_size = 12;
  header->value_blob_size = 12;
  header->constant_blob_size = 20;
  header->error_domain_blob_size = 16;
  header->annotation_blob_size = 12;
  header->signature_blob_size = 8;
  header->enum_blob_size = 20;
  header->struct_blob_size = 20;
  header->object_blob_size = 32;
  header->interface_blob_size = 28;
  header->union_blob_size = 28;

  /* fill in directory and content */
  entry = (DirEntry *)&data[header->directory];

  offset2 = header->directory + dir_size;

  for (e = module->entries, i = 0; e; e = e->next, i++)
    {
      GIrNode *node = e->data;

      if (strchr (node->name, '.'))
        {
	  g_error ("Names may not contain '.'");
	}

      /* we picked up implicit xref nodes, start over */
      if (i == n_entries)
	{
	  g_message ("Found implicit cross references, starting over");

	  g_hash_table_destroy (strings);
	  g_hash_table_destroy (types);
	  strings = NULL;

	  g_free (data);
	  data = NULL;

	  goto restart;
	}
	
      offset = offset2;

      if (node->type == G_IR_NODE_XREF)
	{
	  entry->blob_type = 0;
	  entry->local = FALSE;
	  entry->offset = write_string (((GIrNodeXRef*)node)->namespace, strings, data, &offset2);
	  entry->name = write_string (node->name, strings, data, &offset2);
	}
      else
	{
	  old_offset = offset;
	  offset2 = offset + g_ir_node_get_size (node);

	  entry->blob_type = node->type;
	  entry->local = TRUE;
	  entry->offset = offset;
	  entry->name = write_string (node->name, strings, data, &offset2);

	  g_ir_node_build_typelib (node, module, modules, 
				     strings, types, data, &offset, &offset2);

	  if (offset2 > old_offset + g_ir_node_get_full_size (node))
	    g_error ("left a hole of %d bytes\n", offset2 - old_offset - g_ir_node_get_full_size (node));
	}

      entry++;
    }

  dump_stats ();

  header->annotations = offset2;
  
  g_message ("reallocating to %d bytes", offset2);

  data = g_realloc (data, offset2);
  header = (Header*) data;
  length = header->size = offset2;
  typelib = g_typelib_new_from_memory (data, length);

  g_hash_table_destroy (strings);
  g_hash_table_destroy (types);

  return typelib;
}

