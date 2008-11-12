/* -*- Mode: C; c-file-style: "gnu"; -*- */
/* GObject introspection: Repository implementation
 *
 * Copyright (C) 2005 Matthias Clasen
 * Copyright (C) 2008 Colin Walters <walters@verbum.org>
 * Copyright (C) 2008 Red Hat, Inc.
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
#include <stdlib.h>

#include <glib.h>
#include <glib/gprintf.h>
#include <gmodule.h>
#include "girepository.h"
#include "gtypelib.h"

#include "config.h"

static GStaticMutex globals_lock = G_STATIC_MUTEX_INIT;
static GIRepository *default_repository = NULL;
static GSList *search_path = NULL;

struct _GIRepositoryPrivate 
{
  GHashTable *typelibs; /* (string) namespace -> GTypelib */
  GHashTable *lazy_typelibs; /* (string) namespace-version -> GTypelib */
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
  g_hash_table_destroy (repository->priv->lazy_typelibs);
  
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
      const char *libdir;
      char *typelib_dir;
      const gchar *type_lib_path_env;

      type_lib_path_env = g_getenv ("GI_TYPELIB_PATH");

      search_path = NULL;
      if (type_lib_path_env)
        {
          gchar **custom_dirs;
          gchar **d;

          custom_dirs = g_strsplit (type_lib_path_env, G_SEARCHPATH_SEPARATOR_S, 0);

          d = custom_dirs;
          while (*d)
            {
              search_path = g_slist_prepend (search_path, *d);
              d++;
            }

          /* ownership of the array content was passed to the list */
          g_free (custom_dirs);
        }

      libdir = GOBJECT_INTROSPECTION_LIBDIR;

      typelib_dir = g_build_filename (libdir, "girepository", NULL);

      search_path = g_slist_prepend (search_path, typelib_dir);

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

/**
 * g_irepository_get_search_path:
 *
 * Returns the search path the GIRepository will use when looking for typelibs.
 * The string is internal to GIRespository and should not be freed, nor should
 * the elements.
 *
 * Return value: (element-type filename) (transfer none): list of strings
 */
GSList *
g_irepository_get_search_path (void)
{
  return search_path;
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
check_version_conflict (GTypelib *typelib, 
			const gchar *namespace,
			const gchar *expected_version,
			char       **version_conflict)
{
  Header *header;
  const char *loaded_version;

  if (expected_version == NULL)
    {
      if (version_conflict)
	*version_conflict = NULL;
      return typelib;
    }
  
  header = (Header*)typelib->data;
  loaded_version = g_typelib_get_string (typelib, header->nsversion);
  g_assert (loaded_version != NULL);
  
  if (strcmp (expected_version, loaded_version) != 0)
    {
      if (version_conflict)
	*version_conflict = (char*)loaded_version;
      return NULL;
    }
  if (version_conflict)
    *version_conflict = NULL;
  return typelib;
}

static GTypelib *
get_registered_status (GIRepository *repository,
		       const char   *namespace,
		       const char   *version,
		       gboolean      allow_lazy,
		       gboolean     *lazy_status,
		       char        **version_conflict)
{
  GTypelib *typelib;
  repository = get_repository (repository);
  if (lazy_status)
    *lazy_status = FALSE;
  typelib = g_hash_table_lookup (repository->priv->typelibs, namespace);
  if (typelib) 
    return check_version_conflict (typelib, namespace, version, version_conflict);
  typelib = g_hash_table_lookup (repository->priv->lazy_typelibs, namespace);
  if (!typelib)
    return NULL;
  if (lazy_status)
    *lazy_status = TRUE;
  if (!allow_lazy)
    return NULL;
  return check_version_conflict (typelib, namespace, version, version_conflict);
}

static GTypelib *
get_registered (GIRepository *repository,
		const char   *namespace,
		const char   *version)
{
  return get_registered_status (repository, namespace, version, TRUE, NULL, NULL);
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
	  const char *last_dash;
	  char *dependency_namespace;
	  const char *dependency_version;

	  last_dash = strrchr (dependency, '-');
	  dependency_namespace = g_strndup (dependency, last_dash - dependency);
	  dependency_version = last_dash+1;
	      
	  if (!g_irepository_require (repository, dependency_namespace, dependency_version,
				      0, error))
	    {
	      g_free (dependency_namespace);
	      g_strfreev (dependencies);
	      return FALSE;
	    }
	  g_free (dependency_namespace);
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
  const gchar *version;

  g_return_val_if_fail (typelib != NULL, FALSE);
  
  header = (Header *)typelib->data;

  g_return_val_if_fail (header != NULL, FALSE);

  namespace = g_typelib_get_string (typelib, header->namespace);
  version = g_typelib_get_string (typelib, header->nsversion);

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

  if (typelib->modules == NULL)
    typelib->modules = g_list_append(typelib->modules, g_module_open (NULL, 0));

  return namespace;
}

/**
 * g_irepository_get_dependencies
 * @repository: A #GIRepository, may be %NULL for the default
 * @namespace: Namespace of interest
 *
 * Return an array of all (transitive) dependencies for namespace
 * @namespace, including version.  The returned strings are of the
 * form <code>namespace-version</code>.
 *
 * Note: The namespace must have already been loaded using a function
 * such as #g_irepository_require before calling this function.
 *
 * Returns: Zero-terminated string array of versioned dependencies
 */
char **
g_irepository_get_dependencies (GIRepository *repository,
				const char *namespace)
{
  GTypelib *typelib;

  g_return_val_if_fail (namespace != NULL, NULL);

  repository = get_repository (repository);

  typelib = get_registered (repository, namespace, NULL);
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
  const char *nsversion;
  gboolean allow_lazy = flags & G_IREPOSITORY_LOAD_FLAG_LAZY;
  gboolean is_lazy;
  char *version_conflict;

  repository = get_repository (repository);

  header = (Header *) typelib->data;
  namespace = g_typelib_get_string (typelib, header->namespace);
  nsversion = g_typelib_get_string (typelib, header->nsversion);

  if (get_registered_status (repository, namespace, nsversion, allow_lazy, 
			     &is_lazy, &version_conflict))
    {
      if (version_conflict != NULL)
	{
	  g_set_error (error, G_IREPOSITORY_ERROR,
		       G_IREPOSITORY_ERROR_NAMESPACE_VERSION_CONFLICT,
		       "Attempting to load namespace '%s', version '%s', but '%s' is already loaded",
		       namespace, nsversion, version_conflict);
	  return NULL;
	}
      return namespace;
    }
  return register_internal (repository, "<builtin>", 
			    allow_lazy, typelib, error);
}

/**
 * g_irepository_is_registered
 * @repository: A #GIRepository, may be %NULL for the default
 * @namespace: Namespace of interest
 * @version: <allow-none>: Required version, may be %NULL for latest
 *
 * Check whether a particular namespace (and optionally, a specific
 * version thereof) is currently loaded.  This function is likely to
 * only be useful in unusual circumstances; in order to act upon
 * metadata in the namespace, you should call #g_irepository_require
 * instead which will ensure the namespace is loaded, and return as
 * quickly as this function will if it has already been loaded.
 * 
 * Returns: %TRUE if namespace-version is loaded, %FALSE otherwise
 */
gboolean
g_irepository_is_registered (GIRepository *repository, 
			     const gchar *namespace,
			     const gchar *version)
{
  repository = get_repository (repository);
  return get_registered (repository, namespace, version) != NULL;
}

/**
 * g_irepository_get_default
 *
 * Returns the singleton process-global default #GIRepository.  It is
 * not currently supported to have multiple repositories in a
 * particular process, but this function is provided in the unlikely
 * eventuality that it would become possible, and as a convenience for
 * higher level language bindings to conform to the GObject method
 * call conventions.

 * All methods on #GIRepository also accept %NULL as an instance
 * parameter to mean this default repository, which is usually more
 * convenient for C.
 * 
 * Returns: The global singleton #GIRepository 
 */
GIRepository * 
g_irepository_get_default (void)
{
  return get_repository (NULL);
}

/**
 * g_irepository_get_n_infos
 * @repository: A #GIRepository, may be %NULL for the default
 * @namespace: Namespace to inspect
 *
 * This function returns the number of metadata entries in
 * given namespace @namespace.  The namespace must have
 * already been loaded before calling this function.
 *
 * Returns: number of metadata entries
 */
gint                   
g_irepository_get_n_infos (GIRepository *repository,
			   const gchar  *namespace)
{
  GTypelib *typelib;
  gint n_interfaces = 0;

  g_return_val_if_fail (namespace != NULL, -1);

  repository = get_repository (repository);
  
  typelib = get_registered (repository, namespace, NULL);

  g_return_val_if_fail (typelib != NULL, -1);

  n_interfaces = ((Header *)typelib->data)->n_local_entries;

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
	  RegisteredTypeBlob *blob;

	  entry = g_typelib_get_dir_entry (typelib, i);
	  if (!BLOB_IS_REGISTERED_TYPE (entry))
	    continue;

	  blob = (RegisteredTypeBlob *)(&typelib->data[entry->offset]);
	  if (!blob->gtype_name)
	    continue;

	  type = g_typelib_get_string (typelib, blob->gtype_name);
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

/**
 * g_irepository_get_info
 * @repository: A #GIRepository, may be %NULL for the default
 * @namespace: Namespace to inspect
 * @index: Offset into namespace metadata for entry
 *
 * This function returns a particular metadata entry in the
 * given namespace @namespace.  The namespace must have
 * already been loaded before calling this function.
 *
 * Returns: #GIBaseInfo containing metadata
 */
GIBaseInfo * 
g_irepository_get_info (GIRepository *repository,
			const gchar  *namespace,
			gint          index)
{
  IfaceData data;
  GTypelib *typelib;

  g_return_val_if_fail (namespace != NULL, NULL);

  repository = get_repository (repository);

  data.name = NULL;
  data.type = NULL;
  data.index = index + 1;
  data.iface = NULL;

  typelib = get_registered (repository, namespace, NULL);
  
  g_return_val_if_fail (typelib != NULL, NULL);

  find_interface ((void *)namespace, typelib, &data);

  return data.iface;  
}

/**
 * g_irepository_find_by_gtype
 * @repository: A #GIRepository, may be %NULL for the default
 * @type: GType to search for
 *
 * Searches all loaded namespaces for a particular #GType.  Note that
 * in order to locate the metadata, the namespace corresponding to
 * the type must first have been loaded.  There is currently no
 * mechanism for determining the namespace which corresponds to an
 * arbitrary GType - thus, this function will function most reliably
 * when you have expect the GType to be from a known namespace.
 *
 * Returns: #GIBaseInfo representing metadata about @type, or %NULL
 */
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
 * @namespace: Namespace which will be searched
 * @name: Entry name to find
 *
 * Searches for a particular entry in a namespace.  Before calling
 * this function for a particular namespace, you must call
 * #g_irepository_require once to load the namespace, or otherwise
 * ensure the namespace has already been loaded.
 *
 * Returns: #GIBaseInfo representing metadata about @name, or %NULL
 */
GIBaseInfo * 
g_irepository_find_by_name (GIRepository *repository,
			    const gchar  *namespace,
			    const gchar  *name)
{
  IfaceData data;
  GTypelib *typelib;

  g_return_val_if_fail (namespace != NULL, NULL);

  repository = get_repository (repository);

  data.name = name;
  data.type = NULL;
  data.index = -1;
  data.iface = NULL;

  typelib = get_registered (repository, namespace, NULL);
  
  g_return_val_if_fail (typelib != NULL, NULL);

  find_interface ((void *)namespace, typelib, &data);

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
 * Return the list of currently loaded namespaces.
 *
 * Returns: <utf8,transfer>: List of namespaces
 */
gchar ** 
g_irepository_get_loaded_namespaces (GIRepository *repository)
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

/**
 * g_irepository_get_version
 * @repository: A #GIRepository, may be %NULL for the default
 * @namespace: Namespace to inspect
 *
 * This function returns the loaded version associated with the given
 * namespace @namespace.
 *
 * Note: The namespace must have already been loaded using a function
 * such as #g_irepository_require before calling this function.
 *
 * Returns: Loaded version
 */
const gchar *
g_irepository_get_version (GIRepository *repository,
			   const gchar  *namespace)
{
  GTypelib *typelib;
  Header *header;

  g_return_val_if_fail (namespace != NULL, NULL);

  repository = get_repository (repository);

  typelib = get_registered (repository, namespace, NULL);

  g_return_val_if_fail (typelib != NULL, NULL);

  header = (Header *) typelib->data;
  return g_typelib_get_string (typelib, header->nsversion);
}

/**
 * g_irepository_get_shared_library
 * @repository: A #GIRepository, may be %NULL for the default
 * @namespace: Namespace to inspect
 *
 * This function returns the full path to the shared C library
 * associated with the given namespace @namespace. There may be no
 * shared library path associated, in which case this function will
 * return %NULL.
 *
 * Note: The namespace must have already been loaded using a function
 * such as #g_irepository_require before calling this function.
 *
 * Returns: Full path to shared library, or %NULL if none associated
 */
const gchar *
g_irepository_get_shared_library (GIRepository *repository,
				  const gchar  *namespace)
{
  GTypelib *typelib;
  Header *header;

  g_return_val_if_fail (namespace != NULL, NULL);

  repository = get_repository (repository);

  typelib = get_registered (repository, namespace, NULL);

  g_return_val_if_fail (typelib != NULL, NULL);

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
 * @version: <allow-none>: Version of namespace to use, e.g. "0.8", may be %NULL
 *
 * If namespace @namespace is loaded, return the full path to the
 * .typelib file it was loaded from.  If the typelib for 
 * namespace @namespace was included in a shared library, return
 * the special string "<builtin>".
 *
 * Returns: Filesystem path (or <builtin>) if successful, %NULL if namespace is not loaded
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

/* This simple search function looks for a specified namespace-version;
   it's faster than the full directory listing required for latest version. */
static GMappedFile *
find_namespace_version (const gchar  *namespace,
			const gchar  *version,
			gchar       **path_ret)
{
  GSList *ldir;
  GError *error = NULL;
  GMappedFile *mfile = NULL;
  char *fname;
 
  fname = g_strdup_printf ("%s-%s.typelib", namespace, version);

  for (ldir = search_path; ldir; ldir = ldir->next)
    {
      char *path = g_build_filename (ldir->data, fname, NULL);
      
      mfile = g_mapped_file_new (path, FALSE, &error);
      if (error)
	{
	  g_free (path);
	  g_clear_error (&error);
	  continue;
	}
      *path_ret = path;
      break;
    }
  g_free (fname);
  return mfile;
}

static gboolean
parse_version (const char *version,
	       int *major,
	       int *minor)
{
  const char *dot;
  char *end;

  *major = strtol (version, &end, 10);
  dot = strchr (version, '.');
  if (dot == NULL)
    {
      *minor = 0;
      return TRUE;
    }
  if (dot != end)
    return FALSE;
  *minor = strtol (dot+1, &end, 10);
  if (end != (version + strlen (version)))
    return FALSE;
  return TRUE;
}

static int
compare_version (const char *v1,
		 const char *v2)
{
  gboolean success;
  int v1_major, v1_minor;
  int v2_major, v2_minor;

  success = parse_version (v1, &v1_major, &v1_minor);
  g_assert (success);

  success = parse_version (v2, &v2_major, &v2_minor);
  g_assert (success);

  if (v1_major > v2_major)
    return 1;
  else if (v2_major > v1_major)
    return -1;
  else if (v1_minor > v2_minor)
    return 1;
  else if (v2_minor > v1_minor)
    return -1;
  return 0;
}

struct NamespaceVersionCandidadate
{
  GMappedFile *mfile;
  int path_index;
  char *path;
  char *version;
};

static int
compare_candidate_reverse (struct NamespaceVersionCandidadate *c1,
			   struct NamespaceVersionCandidadate *c2)
{
  int result = compare_version (c1->version, c2->version);
  /* First, check the version */
  if (result > 0)
    return -1;
  else if (result < 0)
    return 1;
  else 
    {
      /* Now check the path index, which says how early in the search path
       * we found it.  This ensures that of equal version targets, we
       * pick the earlier one.
       */
      if (c1->path_index == c2->path_index)
	return 0;
      else if (c1->path_index > c2->path_index)
	return 1;
      else
	return -1;
    }
}

static void
free_candidate (struct NamespaceVersionCandidadate *candidate)
{
  g_mapped_file_free (candidate->mfile);
  g_free (candidate->path);
  g_free (candidate->version);
  g_free (candidate);
}

static GMappedFile *
find_namespace_latest (const gchar  *namespace,
		       gchar       **version_ret,
		       gchar       **path_ret)
{
  GSList *ldir;
  GError *error = NULL;
  char *namespace_dash;
  char *namespace_typelib;
  GSList *candidates = NULL;
  GMappedFile *result = NULL;
  int index;

  *version_ret = NULL;
  *path_ret = NULL;

  namespace_dash = g_strdup_printf ("%s-", namespace);
  namespace_typelib = g_strdup_printf ("%s.typelib", namespace);

  index = 0;
  for (ldir = search_path; ldir; ldir = ldir->next)
    {
      GDir *dir;
      const char *dirname;
      const char *entry;

      dirname = (const char*)ldir->data;
      dir = g_dir_open (dirname, 0, NULL);
      if (dir == NULL)
	continue;
      while ((entry = g_dir_read_name (dir)) != NULL) 
	{
	  GMappedFile *mfile;
	  char *path, *version;
	  struct NamespaceVersionCandidadate *candidate;

	  if (!g_str_has_suffix (entry, ".typelib"))
	    continue;
	  
	  if (g_str_has_prefix (entry, namespace_dash))
	    {
	      const char *last_dash;
	      const char *name_end;
	      int major, minor;

	      name_end = strrchr (entry, '.');
	      last_dash = strrchr (entry, '-');
	      version = g_strndup (last_dash+1, name_end-(last_dash+1));
	      if (!parse_version (version, &major, &minor))
		continue;
	    }
	  else
	    continue;

	  path = g_build_filename (dirname, entry, NULL);
	  mfile = g_mapped_file_new (path, FALSE, &error);
	  if (mfile == NULL)
	    {
	      g_free (path);
	      g_free (version);
	      g_clear_error (&error);
	      continue;
	    }
	  candidate = g_new0 (struct NamespaceVersionCandidadate, 1);
	  candidate->mfile = mfile;
	  candidate->path_index = index;
	  candidate->path = path;
	  candidate->version = version;
	  candidates = g_slist_prepend (candidates, candidate);
	}
      g_dir_close (dir);
      index++;
    }

  if (candidates != NULL)
    {
      struct NamespaceVersionCandidadate *elected;
      candidates = g_slist_sort (candidates, (GCompareFunc) compare_candidate_reverse);
      
      elected = (struct NamespaceVersionCandidadate *) candidates->data;
      /* Remove the elected one so we don't try to free its contents */
      candidates = g_slist_delete_link (candidates, candidates);
      
      result = elected->mfile;
      *path_ret = elected->path;
      *version_ret = elected->version;
      g_free (elected); /* just free the container */
      g_slist_foreach (candidates, (GFunc) free_candidate, NULL);
      g_slist_free (candidates);
    }  

  g_free (namespace_dash);
  g_free (namespace_typelib);
  return result;
}

/**
 * g_irepository_require
 * @repository: <allow-none>: Repository, may be %NULL for the default
 * @namespace: GI namespace to use, e.g. "Gtk"
 * @version: <allow-none>: Version of namespace, may be %NULL for latest
 * @flags: Set of %GIRepositoryLoadFlags, may be %0
 * @error: a #GError.
 *
 * Force the namespace @namespace to be loaded if it isn't already.
 * If @namespace is not loaded, this function will search for a
 * ".typelib" file using the repository search path.  In addition, a
 * version @version of namespace may be specified.  If @version is
 * not specified, the latest will be used.
 *
 * Returns: a pointer to the #GTypelib if successful, %NULL otherwise
 */
GTypelib *
g_irepository_require (GIRepository  *repository,
		       const gchar   *namespace,
		       const gchar   *version,
		       GIRepositoryLoadFlags flags,
		       GError       **error)
{
  GMappedFile *mfile;
  GTypelib *ret = NULL;
  Header *header;
  GTypelib *typelib = NULL;
  const gchar *typelib_namespace, *typelib_version;
  gboolean allow_lazy = (flags & G_IREPOSITORY_LOAD_FLAG_LAZY) > 0;
  gboolean is_lazy;
  char *version_conflict = NULL;
  char *path = NULL;
  char *tmp_version = NULL;

  g_return_val_if_fail (namespace != NULL, FALSE);

  repository = get_repository (repository);

  typelib = get_registered_status (repository, namespace, version, allow_lazy, 
                                   &is_lazy, &version_conflict);
  if (typelib)
    return typelib;

  if (version_conflict != NULL)
    {
      g_set_error (error, G_IREPOSITORY_ERROR,
		   G_IREPOSITORY_ERROR_NAMESPACE_VERSION_CONFLICT,
		   "Requiring namespace '%s' version '%s', but '%s' is already loaded",
		   namespace, version, version_conflict);
      return NULL;
    }

  if (version != NULL)
    {
      mfile = find_namespace_version (namespace, version, &path);
      tmp_version = g_strdup (version);
    }
  else
    {
      mfile = find_namespace_latest (namespace, &tmp_version, &path);
    }
  
  if (mfile == NULL)
    {
      if (version != NULL)
	g_set_error (error, G_IREPOSITORY_ERROR,
		     G_IREPOSITORY_ERROR_TYPELIB_NOT_FOUND,
		     "Typelib file for namespace '%s', version '%s' not found",
		     namespace, version);
      else
	g_set_error (error, G_IREPOSITORY_ERROR,
		     G_IREPOSITORY_ERROR_TYPELIB_NOT_FOUND,
		     "Typelib file for namespace '%s' (any version) not found",
		     namespace);
      goto out;
    }

  typelib = g_typelib_new_from_mapped_file (mfile);
  header = (Header *) typelib->data;
  typelib_namespace = g_typelib_get_string (typelib, header->namespace);
  typelib_version = g_typelib_get_string (typelib, header->nsversion);
  
  if (strcmp (typelib_namespace, namespace) != 0)
    {
      g_set_error (error, G_IREPOSITORY_ERROR,
		   G_IREPOSITORY_ERROR_NAMESPACE_MISMATCH,
		   "Typelib file %s for namespace '%s' contains "
		   "namespace '%s' which doesn't match the file name",
		   path, namespace, typelib_namespace);
      goto out;
    }
  if (version != NULL && strcmp (typelib_version, version) != 0)
    {
      g_set_error (error, G_IREPOSITORY_ERROR,
		   G_IREPOSITORY_ERROR_NAMESPACE_MISMATCH,
		   "Typelib file %s for namespace '%s' contains "
		   "version '%s' which doesn't match the expected version '%s'",
		   path, namespace, typelib_version, version);
      goto out;
    }

  if (!register_internal (repository, path, allow_lazy, 
			  typelib, error))
    {
      g_typelib_free (typelib);
      goto out;
    }
  ret = typelib;
 out:
  g_free (tmp_version);
  g_free (path);
  return ret; 
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
