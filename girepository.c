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

static GStaticMutex globals_lock = G_STATIC_MUTEX_INIT;
static GIRepository *default_repository = NULL;
static GSList *search_path = NULL;

struct _GIRepositoryPrivate 
{
  GHashTable *typelibs; /* (string) namespace -> GTypelib */
  GHashTable *lazy_typelibs; /* (string) namespace -> GTypelib */
};

G_DEFINE_TYPE (GIRepository, g_irepository, G_TYPE_OBJECT);

static void 
g_irepository_init (GIRepository *repository)
{
  repository->priv = G_TYPE_INSTANCE_GET_PRIVATE (repository, G_TYPE_IREPOSITORY,
						  GIRepositoryPrivate);
  repository->priv->typelibs 
    = g_hash_table_new_full (g_str_hash, g_str_equal,
			     (GDestroyNotify) NULL,
			     (GDestroyNotify) g_typelib_free);
  repository->priv->lazy_typelibs 
    = g_hash_table_new (g_str_hash, g_str_equal);
}

static void
g_irepository_finalize (GObject *object)
{
  GIRepository *repository = G_IREPOSITORY (object);

  g_hash_table_destroy (repository->priv->typelibs);
  
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

static void
init_globals ()
{
  g_static_mutex_lock (&globals_lock);

  if (default_repository == NULL) 
    { 
      default_repository = g_object_new (G_TYPE_IREPOSITORY, NULL);
    }

  if (search_path == NULL)
    {
      const gchar *const *datadirs;
      const gchar *const *dir;
      
      datadirs = g_get_system_data_dirs ();
      
      search_path = NULL;
      for (dir = datadirs; *dir; dir++) {
	char *path = g_build_filename (*dir, "girepository", NULL);
	search_path = g_slist_prepend (search_path, path);
      }
      search_path = g_slist_reverse (search_path);
    }

  g_static_mutex_unlock (&globals_lock);
}

void
g_irepository_prepend_search_path (const char *directory)
{
  init_globals ();
  search_path = g_slist_prepend (search_path, g_strdup (directory));
}

static char *
build_typelib_key (const char *name, const char *source)
{
  GString *str = g_string_new (name);
  g_string_append_c (str, '\0');
  g_string_append (str, source);
  return g_string_free (str, FALSE);
}

static char **
get_typelib_dependencies (GTypelib *typelib)
{
  Header *header;
  const char *dependencies_glob;

  header = (Header *)typelib->data;

  if (header->dependencies == 0)
    return NULL;

  dependencies_glob = g_typelib_get_string (typelib, header->dependencies);
  return g_strsplit (dependencies_glob, "|", 0);
}

static GIRepository *
get_repository (GIRepository *repository)
{
  if (repository != NULL)
    return repository;
  else 
    {
      init_globals ();
      return default_repository;
    }
}

static GTypelib *
get_registered_status (GIRepository *repository,
		       const char   *namespace,
		       gboolean      allow_lazy,
		       gboolean     *lazy_status)
{
  GTypelib *typelib;
  repository = get_repository (repository);
  if (lazy_status)
    *lazy_status = FALSE;
  typelib = g_hash_table_lookup (repository->priv->typelibs, namespace);
  if (typelib)
    return typelib;
  typelib = g_hash_table_lookup (repository->priv->lazy_typelibs, namespace);
  if (!typelib)
    return NULL;
  if (lazy_status)
    *lazy_status = TRUE;
  if (!allow_lazy)
    return NULL;
  return typelib;
}

static GTypelib *
get_registered (GIRepository *repository,
		const char   *namespace)
{
  return get_registered_status (repository, namespace, TRUE, NULL);
}

static gboolean
load_dependencies_recurse (GIRepository *repository,
			   GTypelib     *typelib,
			   GError      **error)
{
  char **dependencies;

  dependencies = get_typelib_dependencies (typelib);

  if (dependencies != NULL)
    {
      int i;
	  
      for (i = 0; dependencies[i]; i++)
	{
	  char *dependency = dependencies[i];
	      
	  if (!g_irepository_require (repository, dependency, 
				      0, error))
	    {
	      g_strfreev (dependencies);
	      return FALSE;
	    }
	}
      g_strfreev (dependencies);
    }
  return TRUE;
}
			   
static const char *
register_internal (GIRepository *repository,
		   const char   *source,
		   gboolean      lazy,
		   GTypelib     *typelib,
		   GError      **error)
{
  Header *header;
  const gchar *namespace;
  gboolean was_loaded;
  gboolean currently_lazy;

  g_return_val_if_fail (typelib != NULL, FALSE);
  
  header = (Header *)typelib->data;

  g_return_val_if_fail (header != NULL, FALSE);

  namespace = g_typelib_get_string (typelib, header->namespace);

  if (lazy)
    {
      g_assert (!g_hash_table_lookup (repository->priv->lazy_typelibs, 
				      namespace));
      g_hash_table_insert (repository->priv->lazy_typelibs, 
			   build_typelib_key (namespace, source), (void *)typelib);
    }
  else
    {
      gpointer value;
      char *key;

      /* First, try loading all the dependencies */
      if (!load_dependencies_recurse (repository, typelib, error))
	return NULL;
      
      /* Check if we are transitioning from lazily loaded state */
      if (g_hash_table_lookup_extended (repository->priv->lazy_typelibs, 
					namespace,
					(gpointer)&key, &value))
	g_hash_table_remove (repository->priv->lazy_typelibs, key);
      else
	key = build_typelib_key (namespace, source);

      g_hash_table_insert (repository->priv->typelibs, key, (void *)typelib);
    }

  return namespace;
}

char **
g_irepository_get_dependencies (GIRepository *repository,
				const char *namespace)
{
  GTypelib *typelib;

  g_return_val_if_fail (namespace != NULL, NULL);

  repository = get_repository (repository);

  typelib = get_registered (repository, namespace);
  g_return_val_if_fail (typelib != NULL, NULL);

  return get_typelib_dependencies (typelib);
}

const char *
g_irepository_load_typelib (GIRepository *repository,
			    GTypelib     *typelib,
			    GIRepositoryLoadFlags flags,
			    GError      **error)
{
  Header *header;
  const char *namespace;
  gboolean allow_lazy = flags & G_IREPOSITORY_LOAD_FLAG_LAZY;
  gboolean is_lazy;

  repository = get_repository (repository);

  header = (Header *) typelib->data;
  namespace = g_typelib_get_string (typelib, header->namespace);

  if (get_registered_status (repository, namespace, allow_lazy, &is_lazy))
    return namespace;
  return register_internal (repository, "<builtin>", 
			    allow_lazy, typelib, error);
}

gboolean
g_irepository_is_registered (GIRepository *repository, 
			     const gchar *namespace)
{
  repository = get_repository (repository);
  return get_registered (repository, namespace) != NULL;
}

GIRepository * 
g_irepository_get_default (void)
{
  return get_repository (NULL);
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

  repository = get_repository (repository);
  
  if (namespace)
    {
      GTypelib *typelib;

      typelib = get_registered (repository, namespace);

      if (typelib)
	n_interfaces = ((Header *)typelib->data)->n_local_entries;
    }
  else
    {
      g_hash_table_foreach (repository->priv->typelibs, 
			    count_interfaces, &n_interfaces);
      g_hash_table_foreach (repository->priv->lazy_typelibs, 
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

  repository = get_repository (repository);

  data.name = NULL;
  data.type = NULL;
  data.index = index + 1;
  data.iface = NULL;

  if (namespace)
    {
      GTypelib *typelib;
      
      typelib = get_registered (repository, namespace);
      
      if (typelib)
	find_interface ((void *)namespace, typelib, &data);
    }
  else
    {
      g_hash_table_foreach (repository->priv->typelibs, find_interface, &data);
      g_hash_table_foreach (repository->priv->lazy_typelibs, find_interface, &data);
    }

  return data.iface;  
}

GIBaseInfo * 
g_irepository_find_by_gtype (GIRepository *repository,
			     GType         type)
{
  IfaceData data;

  repository = get_repository (repository);

  data.name = NULL;
  data.type = g_type_name (type);
  data.index = -1;
  data.iface = NULL;

  g_hash_table_foreach (repository->priv->typelibs, find_interface, &data);
  g_hash_table_foreach (repository->priv->lazy_typelibs, find_interface, &data);

  return data.iface;
}

/**
 * g_irepository_find_by_name
 * @repository: A #GIRepository, may be %NULL for the default
 * @namespace: Namespace to search in, may be %NULL for all
 * @name: Name to find
 *
 * Searches for a particular name in one or all namespaces.
 * See #g_irepository_require to load metadata for namespaces.

 * Returns: #GIBaseInfo representing metadata about @name, or %NULL
 */
GIBaseInfo * 
g_irepository_find_by_name (GIRepository *repository,
			    const gchar  *namespace,
			    const gchar  *name)
{
  IfaceData data;

  repository = get_repository (repository);

  data.name = name;
  data.type = NULL;
  data.index = -1;
  data.iface = NULL;

  if (namespace)
    {
      GTypelib *typelib;
      
      typelib = get_registered (repository, namespace);
      
      if (typelib)
	find_interface ((void *)namespace, typelib, &data);
    }
  else
    {
      g_hash_table_foreach (repository->priv->typelibs, find_interface, &data);
      g_hash_table_foreach (repository->priv->lazy_typelibs, find_interface, &data);
    }

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

/**
 * g_irepository_get_namespaces
 * @repository: A #GIRepository, may be %NULL for the default
 *
 * Return the list of currently known namespaces.  Normally
 * if you want a particular namespace, you should call 
 * #g_irepository_require to load it in.

 * Returns: List of namespaces
 */
gchar ** 
g_irepository_get_namespaces (GIRepository *repository)
{
  GList *l, *list = NULL;
  gchar **names;
  gint i;

  repository = get_repository (repository);

  g_hash_table_foreach (repository->priv->typelibs, collect_namespaces, &list);
  g_hash_table_foreach (repository->priv->lazy_typelibs, collect_namespaces, &list);

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

  repository = get_repository (repository);

  typelib = get_registered (repository, namespace);
  if (!typelib)
    return NULL;
  header = (Header *) typelib->data;
  if (header->shared_library)
    return g_typelib_get_string (typelib, header->shared_library);
  else
    return NULL;
}

/**
 * g_irepository_get_typelib_path
 * @repository: Repository, may be %NULL for the default
 * @namespace: GI namespace to use, e.g. "Gtk"
 *
 * If namespace @namespace is loaded, return the full path to the
 * .typelib file it was loaded from.  If the typelib for 
 * namespace @namespace was included in a shared library, return
 * the special string "<builtin>".
 *
 * Returns: Filesystem path (or <builtin>) if successful, %NULL otherwise
 */

const gchar * 
g_irepository_get_typelib_path (GIRepository *repository,
				const gchar  *namespace)
{
  gpointer orig_key, value;

  repository = get_repository (repository);

  if (!g_hash_table_lookup_extended (repository->priv->typelibs, namespace,
				     &orig_key, &value))
    {
      if (!g_hash_table_lookup_extended (repository->priv->lazy_typelibs, namespace,
					 &orig_key, &value))
	
	return NULL;
    }
  return ((char*)orig_key) + strlen ((char *) orig_key) + 1;
}

/**
 * g_irepository_require
 * @repository: Repository, may be %NULL for the default
 * @namespace: GI namespace to use, e.g. "Gtk"
 * @flags: Set of %GIRepositoryLoadFlags, may be %0
 * @error: a #GError.
 *
 * Force the namespace @namespace to be loaded if it isn't
 * already.  If @namespace is not loaded, this function will
 * search for a ".typelib" file using the repository search 
 * path.
 *
 * Returns: %TRUE if successful, %NULL otherwise
 */
gboolean
g_irepository_require (GIRepository  *repository,
		       const gchar   *namespace,
		       GIRepositoryLoadFlags flags,
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
  gboolean allow_lazy = flags & G_IREPOSITORY_LOAD_FLAG_LAZY;
  gboolean is_lazy;

  repository = get_repository (repository);

  if (get_registered_status (repository, namespace, allow_lazy, &is_lazy))
    return TRUE;

  fname = g_strconcat (namespace, ".typelib", NULL);

  for (ldir = search_path; ldir; ldir = ldir->next)
    {
      Header *header;
      
      full_path = g_build_filename (ldir->data, fname, NULL);
      mfile = g_mapped_file_new (full_path, FALSE, &error1);
      if (error1)
	{
	  g_clear_error (&error1);
	  continue;
	}

      typelib = g_typelib_new_from_mapped_file (mfile);
      header = (Header *) typelib->data;
      typelib_namespace = g_typelib_get_string (typelib, header->namespace);

      if (strcmp (typelib_namespace, namespace) != 0)
	{
	  g_set_error (error, G_IREPOSITORY_ERROR,
		       G_IREPOSITORY_ERROR_NAMESPACE_MISMATCH,
		       "Typelib file %s for namespace '%s' contains "
		       "namespace '%s' which doesn't match the file name",
		       full_path, namespace, typelib_namespace);
	  g_free (full_path);
	  return FALSE; 
	}
      break;
  }

  if (typelib == NULL)
    {
      g_set_error (error, G_IREPOSITORY_ERROR,
		   G_IREPOSITORY_ERROR_TYPELIB_NOT_FOUND,
		   "Typelib file for namespace '%s' was not found in search"
		   " path or could not be openened", namespace);
      g_free (full_path);
      return FALSE;
    }

  g_free (fname);
  if (!register_internal (repository, full_path, allow_lazy, 
			  typelib, error))
    {
      g_typelib_free (typelib);
      g_free (full_path);
      return FALSE;
    }
  g_free (full_path);
  return TRUE; 
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
    case GI_TYPE_TAG_TIME_T:
      return "time_t";
    case GI_TYPE_TAG_GTYPE:
      return "GType";
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
