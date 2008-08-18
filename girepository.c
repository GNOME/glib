/* -*- Mode: C; c-file-style: "gnu"; -*- */
/* GObject introspection: Repository implementation
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

#include <glib.h>
#include <glib/gprintf.h>
#include <gmodule.h>
#include "girepository.h"
#include "gtypelib.h"

static GIRepository *default_repository = NULL;
static GHashTable *default_typelib = NULL;
static GSList *search_path = NULL;

struct _GIRepositoryPrivate 
{
  GHashTable *typelib; /* (string) namespace -> GTypelib */
};

G_DEFINE_TYPE (GIRepository, g_irepository, G_TYPE_OBJECT);

static void 
g_irepository_init (GIRepository *repository)
{
  repository->priv = G_TYPE_INSTANCE_GET_PRIVATE (repository, G_TYPE_IREPOSITORY,
						  GIRepositoryPrivate);
}

static void
g_irepository_finalize (GObject *object)
{
  GIRepository *repository = G_IREPOSITORY (object);

  g_hash_table_destroy (repository->priv->typelib);
  
  (* G_OBJECT_CLASS (g_irepository_parent_class)->finalize) (G_OBJECT (repository));
}

static void
g_irepository_class_init (GIRepositoryClass *class)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (class);

  gobject_class->finalize = g_irepository_finalize;

  g_type_class_add_private (class, sizeof (GIRepositoryPrivate)); 
}

const gchar *
g_irepository_register (GIRepository *repository,
                        GTypelib    *typelib)
{
  Header *header;
  const gchar *name;
  GHashTable *table;
  GError *error = NULL;
  
  g_return_val_if_fail (typelib != NULL, NULL);
  
  header = (Header *)typelib->data;

  g_return_val_if_fail (header != NULL, NULL);

  if (repository != NULL)
    {
      if (repository->priv->typelib == NULL)
	repository->priv->typelib = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                            (GDestroyNotify) NULL,
                                                            (GDestroyNotify) g_typelib_free);
      table = repository->priv->typelib;
    }
  else 
    {
      if (default_typelib == NULL)
	default_typelib = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                  (GDestroyNotify) NULL,
                                                  (GDestroyNotify) g_typelib_free);
      table = default_typelib;
    }

  name = g_typelib_get_string (typelib, header->namespace);

  if (g_hash_table_lookup (table, name))
    {
      g_printerr ("typelib (%p) for '%s' already registered\n",
		 typelib, name);

      return NULL;
    }
  g_hash_table_insert (table, g_strdup(name), (void *)typelib);

  if (typelib->module == NULL)
      typelib->module = g_module_open (NULL, 0); 

  if (g_getenv ("G_IREPOSITORY_VERBOSE"))
    {
      g_printerr ("Loaded typelib %s\n", name);
    }

  return name;
}


void
g_irepository_unregister (GIRepository *repository,
                          const gchar  *namespace)
{
  GHashTable *table;

  if (repository != NULL)
    table = repository->priv->typelib;
  else
    table = default_typelib;

  if (!g_hash_table_remove (table, namespace))
    {
      g_printerr ("namespace '%s' not registered\n", namespace);
    }
}

gboolean
g_irepository_is_registered (GIRepository *repository, 
			     const gchar *namespace)
{
  GHashTable *table;

  if (repository != NULL)
    table = repository->priv->typelib;
  else
    table = default_typelib;

  return g_hash_table_lookup (table, namespace) != NULL;
}

GIRepository * 
g_irepository_get_default (void)
{
  if (default_repository == NULL) 
    { 
      default_repository = g_object_new (G_TYPE_IREPOSITORY, NULL);
      if (default_typelib == NULL)
	default_typelib = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                  (GDestroyNotify) NULL,
                                                  (GDestroyNotify) g_typelib_free);
      default_repository->priv->typelib = default_typelib;
    }

  return default_repository; 
}

static void 
count_interfaces (gpointer key,
		  gpointer value,
		  gpointer data)
{
  guchar *typelib = ((GTypelib *) value)->data;
  gint *n_interfaces = (gint *)data;
  
  *n_interfaces += ((Header *)typelib)->n_local_entries;
}

gint                   
g_irepository_get_n_infos (GIRepository *repository,
			   const gchar  *namespace)
{
  gint n_interfaces = 0;
  
  if (namespace)
    {
      GTypelib *typelib;

      typelib = g_hash_table_lookup (repository->priv->typelib, namespace);

      if (typelib)
	n_interfaces = ((Header *)typelib->data)->n_local_entries;
    }
  else
    {
      g_hash_table_foreach (repository->priv->typelib, 
			    count_interfaces, &n_interfaces);
    }

  return n_interfaces;
}

typedef struct
{
  gint index;
  const gchar *name;
  const gchar *type;
  GIBaseInfo *iface;
} IfaceData;

static void
find_interface (gpointer key,
		gpointer value,
		gpointer data)
{
  gint i;
  GTypelib *typelib = (GTypelib *)value;
  IfaceData *iface_data = (IfaceData *)data;
  gint index;
  gint n_entries;
  guint32 offset;
  const gchar *name;
  const gchar *type;
  DirEntry *entry;    

  index = 0;
  n_entries = ((Header *)typelib->data)->n_local_entries;

  if (iface_data->name)
    {
      for (i = 1; i <= n_entries; i++)
	{
	  entry = g_typelib_get_dir_entry (typelib, i);
	  name = g_typelib_get_string (typelib, entry->name);
	  if (strcmp (name, iface_data->name) == 0)
	    {
	      index = i;
	      break;
	    }
	}
    }
  else if (iface_data->type)
    {
      for (i = 1; i <= n_entries; i++)
	{
	  entry = g_typelib_get_dir_entry (typelib, i);
	  if (entry->blob_type < 4)
	    continue;
	  
	  offset = *(guint32*)&typelib->data[entry->offset + 8];
	  type = g_typelib_get_string (typelib, offset);
	  if (strcmp (type, iface_data->type) == 0)
	    {
	      index = i;
	      break;
	    }
	}
    }
  else if (iface_data->index > n_entries)
    iface_data->index -= n_entries;
  else if (iface_data->index > 0)
    {
      index = iface_data->index;
      iface_data->index = 0;
    }

  if (index != 0)
    {
      entry = g_typelib_get_dir_entry (typelib, index);
      iface_data->iface = g_info_new (entry->blob_type, NULL,
				      typelib, entry->offset);
    }
}

GIBaseInfo * 
g_irepository_get_info (GIRepository *repository,
			const gchar  *namespace,
			gint          index)
{
  IfaceData data;

  data.name = NULL;
  data.type = NULL;
  data.index = index + 1;
  data.iface = NULL;

  if (namespace)
    {
      GTypelib *typelib;
      
      typelib = g_hash_table_lookup (repository->priv->typelib, namespace);
      
      if (typelib)
	find_interface ((void *)namespace, typelib, &data);
    }
  else
    g_hash_table_foreach (repository->priv->typelib, find_interface, &data);

  return data.iface;  
}

GIBaseInfo * 
g_irepository_find_by_gtype (GIRepository *repository,
			     GType         type)
{
  IfaceData data;

  data.name = NULL;
  data.type = g_type_name (type);
  data.index = -1;
  data.iface = NULL;

  g_hash_table_foreach (repository->priv->typelib, find_interface, &data);

  return data.iface;
}

GIBaseInfo * 
g_irepository_find_by_name (GIRepository *repository,
			    const gchar  *namespace,
			    const gchar  *name)
{
  IfaceData data;

  data.name = name;
  data.type = NULL;
  data.index = -1;
  data.iface = NULL;

  if (namespace)
    {
      GTypelib *typelib;
      
      typelib = g_hash_table_lookup (repository->priv->typelib, namespace);
      
      if (typelib)
	find_interface ((void *)namespace, typelib, &data);
    }
  else
    g_hash_table_foreach (repository->priv->typelib, find_interface, &data);

  return data.iface;
}

static void
collect_namespaces (gpointer key,
		    gpointer value,
		    gpointer data)
{
  GList **list = data;

  *list = g_list_append (*list, key);
}

gchar ** 
g_irepository_get_namespaces (GIRepository *repository)
{
  GList *l, *list = NULL;
  gchar **names;
  gint i;

  g_hash_table_foreach (repository->priv->typelib, collect_namespaces, &list);

  names = g_malloc0 (sizeof (gchar *) * (g_list_length (list) + 1));
  i = 0;
  for (l = list; l; l = l->next)
    names[i++] = g_strdup (l->data); 
  g_list_free (list);

  return names;
}

const gchar *
g_irepository_get_shared_library (GIRepository *repository,
                                   const gchar  *namespace)
{
  GTypelib *typelib;
  Header *header;

  typelib = g_hash_table_lookup (repository->priv->typelib, namespace);
  if (!typelib)
    return NULL;
  header = (Header *) typelib->data;
  if (header->shared_library)
    return g_typelib_get_string (typelib, header->shared_library);
  else
    return NULL;
}

static inline void
g_irepository_build_search_path (void)
{
  gchar **dir;
  gchar **tokens;

  if (g_getenv ("GIREPOPATH")) {
    gchar *path;
    path = g_strconcat (g_getenv ("GIREPOPATH"), ":", GIREPO_DEFAULT_SEARCH_PATH, NULL);
    tokens = g_strsplit (path, ":", 0);
    g_free (path);
  } else
    tokens = g_strsplit (GIREPO_DEFAULT_SEARCH_PATH, ":", 0);

  search_path = g_slist_prepend (search_path, ".");
  for (dir = tokens; *dir; ++dir)
    search_path = g_slist_prepend (search_path, *dir);
  search_path = g_slist_reverse (search_path);
  g_free (tokens);
}

const gchar *
g_irepository_register_file (GIRepository  *repository,
			     const gchar   *namespace,
			     GError       **error)
{
  GSList *ldir;
  const char *dir;
  gchar *fname, *full_path;
  GMappedFile *mfile;
  GError *error1 = NULL;
  GTypelib *typelib = NULL;
  const gchar *typelib_namespace, *shlib_fname;
  GModule *module;
  guint32 shlib;
  GHashTable *table;

  if (repository != NULL)
    table = repository->priv->typelib;
  else
    table = default_typelib;

  /* don't bother loading a namespace if already registered */
  if (g_hash_table_lookup (table, namespace))
    return NULL;

  if (search_path == NULL)
    g_irepository_build_search_path ();

  fname = g_strconcat (namespace, ".repo", NULL);

  for (ldir = search_path; ldir; ldir = ldir->next) {
    dir = ldir->data;
    full_path = g_build_filename (dir, fname, NULL);
    mfile = g_mapped_file_new (full_path, FALSE, &error1);
    if (error1) {
      g_debug ("Failed to mmap \"%s\": %s", full_path, error1->message);
      g_clear_error (&error1);
      g_free (full_path);
      continue;
    }
    g_free (full_path);
    typelib = g_typelib_new_from_mapped_file (mfile);
    typelib_namespace = g_typelib_get_string (typelib, ((Header *) typelib->data)->namespace);
    if (strcmp (typelib_namespace, namespace) != 0) {
      g_set_error (error, G_IREPOSITORY_ERROR,
                   G_IREPOSITORY_ERROR_NAMESPACE_MISMATCH,
                   "Typelib file %s for namespace '%s' contains namespace '%s'"
                   " which doesn't match the file name",
                   full_path, namespace, typelib_namespace);
      return NULL; 
    }
    break;
  }
  g_free (fname);
  if (typelib == NULL) {
    g_set_error (error, G_IREPOSITORY_ERROR,
                 G_IREPOSITORY_ERROR_TYPELIB_NOT_FOUND,
                 "Typelib file for namespace '%s' was not found in search"
                 " path or could not be openened", namespace);
    return NULL;
  }
  /* optionally load shared library and attach it to the typelib */
  shlib = ((Header *) typelib->data)->shared_library;
  if (shlib) {
    shlib_fname = g_typelib_get_string (typelib, shlib);
    module = g_module_open (shlib_fname, G_MODULE_BIND_LAZY|G_MODULE_BIND_LOCAL);
    if (module == NULL) {
      g_set_error (error, G_IREPOSITORY_ERROR,
                   G_IREPOSITORY_ERROR_TYPELIB_NOT_FOUND,
                   "Typelib for namespace '%s' references shared library %s,"
                   " but it could not be openened (%s)",
                   namespace, shlib_fname, g_module_error ());
      return NULL;
    }
  }

  g_hash_table_remove (table, namespace);
  return g_irepository_register (repository, typelib);
}


GQuark
g_irepository_error_quark (void)
{
  static GQuark quark = 0;
  if (quark == 0)
    quark = g_quark_from_static_string ("g-irepository-error-quark");
  return quark;
}

const gchar*
g_type_tag_to_string (GITypeTag type)
{
  switch (type)
    {
    case GI_TYPE_TAG_VOID:
      return "void";
    case GI_TYPE_TAG_BOOLEAN:
      return "boolean";
    case GI_TYPE_TAG_INT8:
      return "int8";
    case GI_TYPE_TAG_UINT8:
      return "uint8";
    case GI_TYPE_TAG_INT16:
      return "int16";
    case GI_TYPE_TAG_UINT16:
      return "uint16";
    case GI_TYPE_TAG_INT32:
      return "int32";
    case GI_TYPE_TAG_UINT32:
      return "uint32";
    case GI_TYPE_TAG_INT64:
      return "int64";
    case GI_TYPE_TAG_UINT64:
      return "uint64";
    case GI_TYPE_TAG_INT:
      return "int";
    case GI_TYPE_TAG_UINT:
      return "uint";
    case GI_TYPE_TAG_LONG:
      return "long";
    case GI_TYPE_TAG_ULONG:
      return "ulong";
    case GI_TYPE_TAG_SSIZE:
      return "ssize";
    case GI_TYPE_TAG_SIZE:
      return "size";
    case GI_TYPE_TAG_FLOAT:
      return "float";
    case GI_TYPE_TAG_DOUBLE:
      return "double";
    case GI_TYPE_TAG_UTF8:
      return "utf8";
    case GI_TYPE_TAG_FILENAME:
      return "filename";
    case GI_TYPE_TAG_ARRAY:
      return "array";
    case GI_TYPE_TAG_INTERFACE:
      return "interface";
    case GI_TYPE_TAG_GLIST:
      return "glist";
    case GI_TYPE_TAG_GSLIST:
      return "gslist";
    case GI_TYPE_TAG_GHASH:
      return "ghash";
    case GI_TYPE_TAG_ERROR:
      return "error";
    default:
      return "unknown";
    }
}
