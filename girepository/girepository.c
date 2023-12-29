/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 * GObject introspection: Repository implementation
 *
 * Copyright (C) 2005 Matthias Clasen
 * Copyright (C) 2008 Colin Walters <walters@verbum.org>
 * Copyright (C) 2008 Red Hat, Inc.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
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

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <glib.h>
#include <glib/gprintf.h>
#include <gmodule.h>
#include "girepository.h"
#include <gitypelib/gitypelib-private.h>
#include "girepository-private.h"

/**
 * GIRepository:
 *
 * `GIRepository` is used to manage repositories of namespaces. Namespaces
 * are represented on disk by type libraries (`.typelib` files).
 *
 * ### Discovery of type libraries
 *
 * `GIRepository` will typically look for a `girepository-1.0` directory
 * under the library directory used when compiling gobject-introspection. On a
 * standard Linux system this will end up being `/usr/lib/girepository-1.0`.
 *
 * It is possible to control the search paths programmatically, using
 * [func@GIRepository.Repository.prepend_search_path]. It is also possible to
 * modify the search paths by using the `GI_TYPELIB_PATH` environment variable.
 * The environment variable takes precedence over the default search path
 * and the [func@GIRepository.Repository.prepend_search_path] calls.
 *
 * Since: 2.80
 */

static GIRepository *default_repository = NULL;
static GPtrArray *typelib_search_path = NULL;

typedef struct {
  guint n_interfaces;
  GIBaseInfo *interfaces[];
} GTypeInterfaceCache;

static void
gtype_interface_cache_free (gpointer data)
{
  GTypeInterfaceCache *cache = data;
  guint i;

  for (i = 0; i < cache->n_interfaces; i++)
    gi_base_info_unref ((GIBaseInfo*) cache->interfaces[i]);
  g_free (cache);
}

struct _GIRepositoryPrivate
{
  GHashTable *typelibs; /* (string) namespace -> GITypelib */
  GHashTable *lazy_typelibs; /* (string) namespace-version -> GITypelib */
  GHashTable *info_by_gtype; /* GType -> GIBaseInfo */
  GHashTable *info_by_error_domain; /* GQuark -> GIBaseInfo */
  GHashTable *interfaces_for_gtype; /* GType -> GTypeInterfaceCache */
  GHashTable *unknown_gtypes; /* hashset of GType */
};

G_DEFINE_TYPE_WITH_CODE (GIRepository, gi_repository, G_TYPE_OBJECT, G_ADD_PRIVATE (GIRepository));

#ifdef G_PLATFORM_WIN32

#include <windows.h>

static HMODULE girepository_dll = NULL;

#ifdef DLL_EXPORT

BOOL WINAPI DllMain (HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved);

BOOL WINAPI
DllMain (HINSTANCE hinstDLL,
	 DWORD     fdwReason,
	 LPVOID    lpvReserved)
{
  if (fdwReason == DLL_PROCESS_ATTACH)
      girepository_dll = hinstDLL;

  return TRUE;
}

#endif

#undef GOBJECT_INTROSPECTION_LIBDIR

/* GOBJECT_INTROSPECTION_LIBDIR is used only in code called just once,
 * so no problem leaking this
 */
#define GOBJECT_INTROSPECTION_LIBDIR \
  g_build_filename (g_win32_get_package_installation_directory_of_module (girepository_dll), \
		    "lib", \
		    NULL)

#endif

static void
gi_repository_init (GIRepository *repository)
{
  repository->priv = gi_repository_get_instance_private (repository);
  repository->priv->typelibs
    = g_hash_table_new_full (g_str_hash, g_str_equal,
			     (GDestroyNotify) g_free,
			     (GDestroyNotify) gi_typelib_free);
  repository->priv->lazy_typelibs
    = g_hash_table_new_full (g_str_hash, g_str_equal,
                             (GDestroyNotify) g_free,
                             (GDestroyNotify) NULL);
  repository->priv->info_by_gtype
    = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                             (GDestroyNotify) NULL,
                             (GDestroyNotify) gi_base_info_unref);
  repository->priv->info_by_error_domain
    = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                             (GDestroyNotify) NULL,
                             (GDestroyNotify) gi_base_info_unref);
  repository->priv->interfaces_for_gtype
    = g_hash_table_new_full (g_direct_hash, g_direct_equal,
                             (GDestroyNotify) NULL,
                             (GDestroyNotify) gtype_interface_cache_free);
  repository->priv->unknown_gtypes = g_hash_table_new (NULL, NULL);
}

static void
gi_repository_finalize (GObject *object)
{
  GIRepository *repository = GI_REPOSITORY (object);

  g_hash_table_destroy (repository->priv->typelibs);
  g_hash_table_destroy (repository->priv->lazy_typelibs);
  g_hash_table_destroy (repository->priv->info_by_gtype);
  g_hash_table_destroy (repository->priv->info_by_error_domain);
  g_hash_table_destroy (repository->priv->interfaces_for_gtype);
  g_hash_table_destroy (repository->priv->unknown_gtypes);

  (* G_OBJECT_CLASS (gi_repository_parent_class)->finalize) (G_OBJECT (repository));
}

static void
gi_repository_class_init (GIRepositoryClass *class)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (class);

  gobject_class->finalize = gi_repository_finalize;
}

static void
init_globals (void)
{
  static gsize initialized = 0;

  if (!g_once_init_enter (&initialized))
    return;

  if (default_repository == NULL)
    default_repository = gi_repository_new ();

  if (typelib_search_path == NULL)
    {
      const char *libdir;
      char *typelib_dir;
      const gchar *type_lib_path_env;

      /* This variable is intended to take precedence over both:
       *   - the default search path;
       *   - all gi_repository_prepend_search_path() calls.
       */
      type_lib_path_env = g_getenv ("GI_TYPELIB_PATH");

      if (type_lib_path_env)
        {
          gchar **custom_dirs;

          custom_dirs = g_strsplit (type_lib_path_env, G_SEARCHPATH_SEPARATOR_S, 0);
          typelib_search_path =
            g_ptr_array_new_take_null_terminated ((gpointer) g_steal_pointer (&custom_dirs), g_free);
        }
      else
        {
          typelib_search_path = g_ptr_array_new_null_terminated (1, g_free, TRUE);
        }

      libdir = GOBJECT_INTROSPECTION_LIBDIR;

      typelib_dir = g_build_filename (libdir, "girepository-1.0", NULL);

      g_ptr_array_add (typelib_search_path, g_steal_pointer (&typelib_dir));
    }

  g_once_init_leave (&initialized, 1);
}

/**
 * gi_repository_prepend_search_path:
 * @directory: (type filename): directory name to prepend to the typelib
 *   search path
 *
 * Prepends @directory to the typelib search path.
 *
 * See also: gi_repository_get_search_path().
 *
 * Since: 2.80
 */
void
gi_repository_prepend_search_path (const char *directory)
{
  init_globals ();
  g_ptr_array_insert (typelib_search_path, 0, g_strdup (directory));
}

/**
 * gi_repository_get_search_path:
 * @n_paths_out: (optional) (out): The number of search paths returned.
 *
 * Returns the current search path [class@GIRepository.Repository] will use when
 * loading typelib files.
 *
 * The list is internal to [class@GIRepository.Repository] and should not be
 * freed, nor should its string elements.
 *
 * Returns: (element-type filename) (transfer none) (array length=n_paths_out): list of search paths, most
 *   important first
 * Since: 2.80
 */
const char * const *
gi_repository_get_search_path (size_t *n_paths_out)
{
  if G_UNLIKELY (!typelib_search_path || !typelib_search_path->pdata)
    {
      static const char * const empty_search_path[] = {NULL};

      if (n_paths_out)
        *n_paths_out = 0;

      return empty_search_path;
    }

  if (n_paths_out)
    *n_paths_out = typelib_search_path->len;

  return (const char * const *) typelib_search_path->pdata;
}

static char *
build_typelib_key (const char *name, const char *source)
{
  GString *str = g_string_new (name);
  g_string_append_c (str, '\0');
  g_string_append (str, source);
  return g_string_free (str, FALSE);
}

/* Note: Returns %NULL (not an empty %NULL-terminated array) if there are no
 * dependencies. */
static char **
get_typelib_dependencies (GITypelib *typelib)
{
  Header *header;
  const char *dependencies_glob;

  header = (Header *)typelib->data;

  if (header->dependencies == 0)
    return NULL;

  dependencies_glob = gi_typelib_get_string (typelib, header->dependencies);
  return g_strsplit (dependencies_glob, "|", 0);
}

static GIRepository *
get_repository (GIRepository *repository)
{
  init_globals ();

  if (repository != NULL)
    return repository;
  else
    return default_repository;
}

static GITypelib *
check_version_conflict (GITypelib *typelib,
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
  loaded_version = gi_typelib_get_string (typelib, header->nsversion);
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

static GITypelib *
get_registered_status (GIRepository *repository,
		       const char   *namespace,
		       const char   *version,
		       gboolean      allow_lazy,
		       gboolean     *lazy_status,
		       char        **version_conflict)
{
  GITypelib *typelib;
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

static GITypelib *
get_registered (GIRepository *repository,
		const char   *namespace,
		const char   *version)
{
  return get_registered_status (repository, namespace, version, TRUE, NULL, NULL);
}

static gboolean
load_dependencies_recurse (GIRepository *repository,
			   GITypelib     *typelib,
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

	  if (!gi_repository_require (repository, dependency_namespace, dependency_version,
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
		   GITypelib     *typelib,
		   GError      **error)
{
  Header *header;
  const gchar *namespace;

  g_return_val_if_fail (typelib != NULL, FALSE);

  header = (Header *)typelib->data;

  g_return_val_if_fail (header != NULL, FALSE);

  namespace = gi_typelib_get_string (typelib, header->namespace);

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

      g_hash_table_insert (repository->priv->typelibs,
                           g_steal_pointer (&key),
                           (void *)typelib);
    }

  /* These types might be resolved now, clear the cache */
  g_hash_table_remove_all (repository->priv->unknown_gtypes);

  return namespace;
}

/**
 * gi_repository_get_immediate_dependencies:
 * @repository: (nullable): A #GIRepository, or `NULL` for the singleton
 *   process-global default #GIRepository
 * @namespace_: Namespace of interest
 *
 * Return an array of the immediate versioned dependencies for @namespace_.
 * Returned strings are of the form `namespace-version`.
 *
 * Note: @namespace_ must have already been loaded using a function
 * such as [method@GIRepository.Repository.require] before calling this
 * function.
 *
 * To get the transitive closure of dependencies for @namespace_, use
 * [method@GIRepository.Repository.get_dependencies].
 *
 * Returns: (transfer full) (array zero-terminated=1): `NULL`-terminated string
 *   array of immediate versioned dependencies
 * Since: 2.80
 */
char **
gi_repository_get_immediate_dependencies (GIRepository *repository,
                                          const char   *namespace)
{
  GITypelib *typelib;
  gchar **deps;

  g_return_val_if_fail (namespace != NULL, NULL);

  repository = get_repository (repository);

  typelib = get_registered (repository, namespace, NULL);
  g_return_val_if_fail (typelib != NULL, NULL);

  /* Ensure we always return a non-%NULL vector. */
  deps = get_typelib_dependencies (typelib);
  if (deps == NULL)
      deps = g_strsplit ("", "|", 0);

  return deps;
}

/* Load the transitive closure of dependency namespace-version strings for the
 * given @typelib. @repository must be non-%NULL. @transitive_dependencies must
 * be a pre-existing GHashTable<owned utf8, owned utf8> set for storing the
 * dependencies. */
static void
get_typelib_dependencies_transitive (GIRepository *repository,
                                     GITypelib    *typelib,
                                     GHashTable   *transitive_dependencies)
{
  gchar **immediate_dependencies;
  guint i;

  immediate_dependencies = get_typelib_dependencies (typelib);

  for (i = 0; immediate_dependencies != NULL && immediate_dependencies[i]; i++)
    {
      gchar *dependency;
      const gchar *last_dash;
      gchar *dependency_namespace;

      dependency = immediate_dependencies[i];

      /* Steal from the strv. */
      g_hash_table_add (transitive_dependencies, dependency);
      immediate_dependencies[i] = NULL;

      /* Recurse for this namespace. */
      last_dash = strrchr (dependency, '-');
      dependency_namespace = g_strndup (dependency, last_dash - dependency);

      typelib = get_registered (repository, dependency_namespace, NULL);
      g_return_if_fail (typelib != NULL);
      get_typelib_dependencies_transitive (repository, typelib,
                                           transitive_dependencies);

      g_free (dependency_namespace);
    }

  g_free (immediate_dependencies);
}

/**
 * gi_repository_get_dependencies:
 * @repository: (nullable): A #GIRepository, or `NULL` for the singleton
 *   process-global default #GIRepository
 * @namespace_: Namespace of interest
 *
 * Retrieves all (transitive) versioned dependencies for
 * @namespace_.
 *
 * The returned strings are of the form `namespace-version`.
 *
 * Note: @namespace_ must have already been loaded using a function
 * such as [method@GIRepository.Repository.require] before calling this
 * function.
 *
 * To get only the immediate dependencies for @namespace_, use
 * [method@GIRepository.Repository.get_immediate_dependencies].
 *
 * Returns: (transfer full) (array zero-terminated=1): `NULL`-terminated string
 *   array of all versioned dependencies
 * Since: 2.80
 */
char **
gi_repository_get_dependencies (GIRepository *repository,
				const char *namespace)
{
  GITypelib *typelib;
  GHashTable *transitive_dependencies;  /* set of owned utf8 */
  GHashTableIter iter;
  gchar *dependency;
  GPtrArray *out;  /* owned utf8 elements */

  g_return_val_if_fail (namespace != NULL, NULL);

  repository = get_repository (repository);

  typelib = get_registered (repository, namespace, NULL);
  g_return_val_if_fail (typelib != NULL, NULL);

  /* Load the dependencies. */
  transitive_dependencies = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                   g_free, NULL);
  get_typelib_dependencies_transitive (repository, typelib,
                                       transitive_dependencies);

  /* Convert to a string array. */
  out = g_ptr_array_new_null_terminated (g_hash_table_size (transitive_dependencies),
                                         g_free, TRUE);
  g_hash_table_iter_init (&iter, transitive_dependencies);

  while (g_hash_table_iter_next (&iter, (gpointer) &dependency, NULL))
    {
      g_ptr_array_add (out, dependency);
      g_hash_table_iter_steal (&iter);
    }

  g_hash_table_unref (transitive_dependencies);

  return (gchar **) g_ptr_array_free (out, FALSE);
}

/**
 * gi_repository_load_typelib:
 * @repository: (nullable): A #GIRepository, or `NULL` for the singleton
 *   process-global default #GIRepository
 * @typelib: the typelib to load
 * @flags: flags affecting the loading operation
 * @error: return location for a [type@GLib.Error], or `NULL`
 *
 * Load the given @typelib into the repository.
 *
 * Returns: namespace of the loaded typelib
 * Since: 2.80
 */
const char *
gi_repository_load_typelib (GIRepository *repository,
			    GITypelib     *typelib,
			    GIRepositoryLoadFlags flags,
			    GError      **error)
{
  Header *header;
  const char *namespace;
  const char *nsversion;
  gboolean allow_lazy = flags & GI_REPOSITORY_LOAD_FLAG_LAZY;
  gboolean is_lazy;
  char *version_conflict;

  repository = get_repository (repository);

  header = (Header *) typelib->data;
  namespace = gi_typelib_get_string (typelib, header->namespace);
  nsversion = gi_typelib_get_string (typelib, header->nsversion);

  if (get_registered_status (repository, namespace, nsversion, allow_lazy,
			     &is_lazy, &version_conflict))
    {
      if (version_conflict != NULL)
	{
	  g_set_error (error, GI_REPOSITORY_ERROR,
		       GI_REPOSITORY_ERROR_NAMESPACE_VERSION_CONFLICT,
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
 * gi_repository_is_registered:
 * @repository: (nullable): A #GIRepository, or `NULL` for the singleton
 *   process-global default #GIRepository
 * @namespace_: Namespace of interest
 * @version: (nullable): Required version, may be `NULL` for latest
 *
 * Check whether a particular namespace (and optionally, a specific
 * version thereof) is currently loaded.
 *
 * This function is likely to only be useful in unusual circumstances; in order
 * to act upon metadata in the namespace, you should call
 * [method@GIRepository.Repository.require] instead which will ensure the
 * namespace is loaded, and return as quickly as this function will if it has
 * already been loaded.
 *
 * Returns: `TRUE` if namespace-version is loaded, `FALSE` otherwise
 * Since: 2.80
 */
gboolean
gi_repository_is_registered (GIRepository *repository,
			     const gchar *namespace,
			     const gchar *version)
{
  repository = get_repository (repository);
  return get_registered (repository, namespace, version) != NULL;
}

/**
 * gi_repository_get_default:
 *
 * Returns the singleton process-global default #GIRepository.
 *
 * It is not currently supported to have multiple repositories in a
 * particular process, but this function is provided in the unlikely
 * eventuality that it would become possible, and as a convenience for
 * higher level language bindings to conform to the GObject method
 * call conventions.
 *
 * All methods on #GIRepository also accept `NULL` as an instance
 * parameter to mean this default repository, which is usually more
 * convenient for C.
 *
 * Returns: (transfer none): The global singleton [class@GIRepository.Repository]
 * Since: 2.80
 */
GIRepository *
gi_repository_get_default (void)
{
  return get_repository (NULL);
}

/**
 * gi_repository_new:
 *
 * Create a new (non-singleton) [class@GIRepository.Repository].
 *
 * Most callers should use [func@GIRepository.Repository.get_default] instead,
 * as a singleton repository is more useful in most situations.
 *
 * Returns: (transfer full): a new [class@GIRepository.Repository]
 * Since: 2.80
 */
GIRepository *
gi_repository_new (void)
{
  return g_object_new (GI_TYPE_REPOSITORY, NULL);
}

/**
 * gi_repository_get_n_infos:
 * @repository: (nullable): A #GIRepository, or `NULL` for the singleton
 *   process-global default #GIRepository
 * @namespace_: Namespace to inspect
 *
 * This function returns the number of metadata entries in
 * given namespace @namespace_.
 *
 * The namespace must have already been loaded before calling this function.
 *
 * Returns: number of metadata entries
 * Since: 2.80
 */
guint
gi_repository_get_n_infos (GIRepository *repository,
			   const gchar  *namespace)
{
  GITypelib *typelib;
  guint n_interfaces = 0;

  g_return_val_if_fail (namespace != NULL, -1);

  repository = get_repository (repository);

  typelib = get_registered (repository, namespace, NULL);

  g_return_val_if_fail (typelib != NULL, -1);

  n_interfaces = ((Header *)typelib->data)->n_local_entries;

  return n_interfaces;
}

/**
 * gi_repository_get_info:
 * @repository: (nullable): A #GIRepository, or `NULL` for the singleton
 *   process-global default #GIRepository
 * @namespace_: Namespace to inspect
 * @idx: 0-based offset into namespace metadata for entry
 *
 * This function returns a particular metadata entry in the
 * given namespace @namespace_.
 *
 * The namespace must have already been loaded before calling this function.
 * See [method@GIRepository.Repository.get_n_infos] to find the maximum number
 * of entries.
 *
 * Returns: (transfer full) (nullable): [class@GIRepository.BaseInfo] containing
 *   metadata, or `NULL` if @idx was too high
 * Since: 2.80
 */
GIBaseInfo *
gi_repository_get_info (GIRepository *repository,
			const gchar  *namespace,
			guint         idx)
{
  GITypelib *typelib;
  DirEntry *entry;

  g_return_val_if_fail (namespace != NULL, NULL);

  repository = get_repository (repository);

  typelib = get_registered (repository, namespace, NULL);

  g_return_val_if_fail (typelib != NULL, NULL);

  entry = gi_typelib_get_dir_entry (typelib, idx + 1);
  if (entry == NULL)
    return NULL;
  return gi_info_new_full (entry->blob_type,
                           repository,
                           NULL, typelib, entry->offset);
}

typedef struct {
  const gchar *gtype_name;
  GITypelib *result_typelib;
} FindByGTypeData;

static DirEntry *
find_by_gtype (GHashTable *table, FindByGTypeData *data, gboolean check_prefix)
{
  GHashTableIter iter;
  gpointer key, value;
  DirEntry *ret;

  g_hash_table_iter_init (&iter, table);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      GITypelib *typelib = (GITypelib*)value;
      if (check_prefix)
        {
          if (!gi_typelib_matches_gtype_name_prefix (typelib, data->gtype_name))
            continue;
        }

      ret = gi_typelib_get_dir_entry_by_gtype_name (typelib, data->gtype_name);
      if (ret)
        {
          data->result_typelib = typelib;
          return ret;
        }
    }

  return NULL;
}

/**
 * gi_repository_find_by_gtype:
 * @repository: (nullable): A #GIRepository, or `NULL` for the singleton
 *   process-global default #GIRepository
 * @gtype: [type@GObject.Type] to search for
 *
 * Searches all loaded namespaces for a particular [type@GObject.Type].
 *
 * Note that in order to locate the metadata, the namespace corresponding to
 * the type must first have been loaded.  There is currently no
 * mechanism for determining the namespace which corresponds to an
 * arbitrary [type@GObject.Type] — thus, this function will operate most
 * reliably when you know the [type@GObject.Type] is from a loaded namespace.
 *
 * Returns: (transfer full) (nullable): [class@GIRepository.BaseInfo]
 *   representing metadata about @type, or `NULL` if none found
 * Since: 2.80
 */
GIBaseInfo *
gi_repository_find_by_gtype (GIRepository *repository,
                             GType         gtype)
{
  FindByGTypeData data;
  GIBaseInfo *cached;
  DirEntry *entry;

  g_return_val_if_fail (gtype != G_TYPE_INVALID, NULL);

  repository = get_repository (repository);

  cached = g_hash_table_lookup (repository->priv->info_by_gtype,
				(gpointer)gtype);

  if (cached != NULL)
    return gi_base_info_ref (cached);

  if (g_hash_table_contains (repository->priv->unknown_gtypes, (gpointer)gtype))
    return NULL;

  data.gtype_name = g_type_name (gtype);
  data.result_typelib = NULL;

  /* Inside each typelib, we include the "C prefix" which acts as
   * a namespace mechanism.  For GtkTreeView, the C prefix is Gtk.
   * Given the assumption that GTypes for a library also use the
   * C prefix, we know we can skip examining a typelib if our
   * target type does not have this typelib's C prefix. Use this
   * assumption as our first attempt at locating the DirEntry.
   */
  entry = find_by_gtype (repository->priv->typelibs, &data, TRUE);
  if (entry == NULL)
    entry = find_by_gtype (repository->priv->lazy_typelibs, &data, TRUE);

  /* Not ever class library necessarily specifies a correct c_prefix,
   * so take a second pass. This time we will try a global lookup,
   * ignoring prefixes.
   * See http://bugzilla.gnome.org/show_bug.cgi?id=564016
   */
  if (entry == NULL)
    entry = find_by_gtype (repository->priv->typelibs, &data, FALSE);
  if (entry == NULL)
    entry = find_by_gtype (repository->priv->lazy_typelibs, &data, FALSE);

  if (entry != NULL)
    {
      cached = gi_info_new_full (entry->blob_type,
                                 repository,
                                 NULL, data.result_typelib, entry->offset);

      g_hash_table_insert (repository->priv->info_by_gtype,
			   (gpointer) gtype,
			   gi_base_info_ref (cached));
      return cached;
    }
  else
    {
      g_hash_table_add (repository->priv->unknown_gtypes, (gpointer) gtype);
      return NULL;
    }
}

/**
 * gi_repository_find_by_name:
 * @repository: (nullable): A #GIRepository, or `NULL` for the singleton
 *   process-global default #GIRepository
 * @namespace_: Namespace which will be searched
 * @name: Entry name to find
 *
 * Searches for a particular entry in a namespace.
 *
 * Before calling this function for a particular namespace, you must call
 * [method@GIRepository.Repository.require] to load the namespace, or otherwise
 * ensure the namespace has already been loaded.
 *
 * Returns: (transfer full) (nullable): [class@GIRepository.BaseInfo]
 *   representing metadata about @name, or `NULL` if none found
 * Since: 2.80
 */
GIBaseInfo *
gi_repository_find_by_name (GIRepository *repository,
			    const gchar  *namespace,
			    const gchar  *name)
{
  GITypelib *typelib;
  DirEntry *entry;

  g_return_val_if_fail (namespace != NULL, NULL);

  repository = get_repository (repository);
  typelib = get_registered (repository, namespace, NULL);
  g_return_val_if_fail (typelib != NULL, NULL);

  entry = gi_typelib_get_dir_entry_by_name (typelib, name);
  if (entry == NULL)
    return NULL;
  return gi_info_new_full (entry->blob_type,
                           repository,
                           NULL, typelib, entry->offset);
}

typedef struct {
  GIRepository *repository;
  GQuark domain;

  GITypelib *result_typelib;
  DirEntry *result;
} FindByErrorDomainData;

static void
find_by_error_domain_foreach (gpointer key,
			      gpointer value,
			      gpointer datap)
{
  GITypelib *typelib = (GITypelib*)value;
  FindByErrorDomainData *data = datap;

  if (data->result != NULL)
    return;

  data->result = gi_typelib_get_dir_entry_by_error_domain (typelib, data->domain);
  if (data->result)
    data->result_typelib = typelib;
}

/**
 * gi_repository_find_by_error_domain:
 * @repository: (nullable): A #GIRepository, or `NULL` for the singleton
 *   process-global default #GIRepository
 * @domain: a [type@GLib.Error] domain
 *
 * Searches for the enum type corresponding to the given [type@GLib.Error]
 * domain.
 *
 * Before calling this function for a particular namespace, you must call
 * [method@GIRepository.Repository.require] to load the namespace, or otherwise
 * ensure the namespace has already been loaded.
 *
 * Returns: (transfer full) (nullable): [class@GIRepository.EnumInfo]
 *   representing metadata about @domain’s enum type, or `NULL` if none found
 * Since: 2.80
 */
GIEnumInfo *
gi_repository_find_by_error_domain (GIRepository *repository,
				    GQuark        domain)
{
  FindByErrorDomainData data;
  GIEnumInfo *cached;

  repository = get_repository (repository);

  cached = g_hash_table_lookup (repository->priv->info_by_error_domain,
				GUINT_TO_POINTER (domain));

  if (cached != NULL)
    return (GIEnumInfo *) gi_base_info_ref ((GIBaseInfo *)cached);

  data.repository = repository;
  data.domain = domain;
  data.result_typelib = NULL;
  data.result = NULL;

  g_hash_table_foreach (repository->priv->typelibs, find_by_error_domain_foreach, &data);
  if (data.result == NULL)
    g_hash_table_foreach (repository->priv->lazy_typelibs, find_by_error_domain_foreach, &data);

  if (data.result != NULL)
    {
      cached = (GIEnumInfo *) gi_info_new_full (data.result->blob_type,
                                                repository,
                                                NULL, data.result_typelib, data.result->offset);

      g_hash_table_insert (repository->priv->info_by_error_domain,
			   GUINT_TO_POINTER (domain),
			   gi_base_info_ref ((GIBaseInfo *) cached));
      return cached;
    }
  return NULL;
}

/**
 * gi_repository_get_object_gtype_interfaces:
 * @repository: (nullable): a #GIRepository, or `NULL` for the default repository
 * @gtype: a [type@GObject.Type] whose fundamental type is `G_TYPE_OBJECT`
 * @n_interfaces_out: (out): Number of interfaces
 * @interfaces_out: (out) (transfer none) (array length=n_interfaces_out): Interfaces for @gtype
 *
 * Look up the implemented interfaces for @gtype.
 *
 * This function cannot fail per se; but for a totally ‘unknown’
 * [type@GObject.Type], it may return 0 implemented interfaces.
 *
 * The semantics of this function are designed for a dynamic binding,
 * where in certain cases (such as a function which returns an
 * interface which may have ‘hidden’ implementation classes), not all
 * data may be statically known, and will have to be determined from
 * the [type@GObject.Type] of the object.  An example is
 * [func@Gio.File.new_for_path] returning a concrete class of
 * `GLocalFile`, which is a [type@GObject.Type] we see at runtime, but
 * not statically.
 *
 * Since: 2.80
 */
void
gi_repository_get_object_gtype_interfaces (GIRepository      *repository,
                                           GType              gtype,
                                           gsize             *n_interfaces_out,
                                           GIInterfaceInfo ***interfaces_out)
{
  GTypeInterfaceCache *cache;

  g_return_if_fail (g_type_fundamental (gtype) == G_TYPE_OBJECT);

  repository = get_repository (repository);

  cache = g_hash_table_lookup (repository->priv->interfaces_for_gtype,
                               (gpointer) gtype);
  if (cache == NULL)
    {
      GType *interfaces;
      guint n_interfaces;
      guint i;
      GList *interface_infos = NULL, *iter;

      interfaces = g_type_interfaces (gtype, &n_interfaces);
      for (i = 0; i < n_interfaces; i++)
        {
          GIBaseInfo *base_info;

          base_info = gi_repository_find_by_gtype (repository, interfaces[i]);
          if (base_info == NULL)
            continue;

          if (gi_base_info_get_info_type (base_info) != GI_INFO_TYPE_INTERFACE)
            {
              /* FIXME - could this really happen? */
              gi_base_info_unref (base_info);
              continue;
            }

          if (!g_list_find (interface_infos, base_info))
            interface_infos = g_list_prepend (interface_infos, base_info);
        }

      cache = g_malloc (sizeof (GTypeInterfaceCache)
                        + sizeof (GIBaseInfo*) * g_list_length (interface_infos));
      cache->n_interfaces = g_list_length (interface_infos);
      for (iter = interface_infos, i = 0; iter; iter = iter->next, i++)
        cache->interfaces[i] = iter->data;
      g_list_free (interface_infos);

      g_hash_table_insert (repository->priv->interfaces_for_gtype, (gpointer) gtype,
                           cache);

      g_free (interfaces);
    }

  *n_interfaces_out = cache->n_interfaces;
  *interfaces_out = (GIInterfaceInfo**)&cache->interfaces[0];
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
 * gi_repository_get_loaded_namespaces:
 * @repository: (nullable): A #GIRepository, or `NULL` for the singleton
 *   process-global default #GIRepository
 *
 * Return the list of currently loaded namespaces.
 *
 * Returns: (element-type utf8) (transfer full) (array zero-terminated=1): `NULL`-terminated
 *   list of namespaces
 * Since: 2.80
 */
gchar **
gi_repository_get_loaded_namespaces (GIRepository *repository)
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
 * gi_repository_get_version:
 * @repository: (nullable): A #GIRepository, or `NULL` for the singleton
 *   process-global default #GIRepository
 * @namespace_: Namespace to inspect
 *
 * This function returns the loaded version associated with the given
 * namespace @namespace_.
 *
 * Note: The namespace must have already been loaded using a function
 * such as [method@GIRepository.Repository.require] before calling this
 * function.
 *
 * Returns: Loaded version
 * Since: 2.80
 */
const gchar *
gi_repository_get_version (GIRepository *repository,
			   const gchar  *namespace)
{
  GITypelib *typelib;
  Header *header;

  g_return_val_if_fail (namespace != NULL, NULL);

  repository = get_repository (repository);

  typelib = get_registered (repository, namespace, NULL);

  g_return_val_if_fail (typelib != NULL, NULL);

  header = (Header *) typelib->data;
  return gi_typelib_get_string (typelib, header->nsversion);
}

/**
 * gi_repository_get_shared_library:
 * @repository: (nullable): A #GIRepository, or `NULL` for the singleton
 *   process-global default #GIRepository
 * @namespace_: Namespace to inspect
 *
 * This function returns a comma-separated list of paths to the
 * shared C libraries associated with the given namespace @namespace_.
 *
 * There may be no shared library path associated, in which case this
 * function will return `NULL`.
 *
 * Note: The namespace must have already been loaded using a function
 * such as [method@GIRepository.Repository.require] before calling this
 * function.
 *
 * Returns: (nullable): Comma-separated list of paths to shared libraries,
 *   or `NULL` if none are associated
 * Since: 2.80
 */
const gchar *
gi_repository_get_shared_library (GIRepository *repository,
				  const gchar  *namespace)
{
  GITypelib *typelib;
  Header *header;

  g_return_val_if_fail (namespace != NULL, NULL);

  repository = get_repository (repository);

  typelib = get_registered (repository, namespace, NULL);

  g_return_val_if_fail (typelib != NULL, NULL);

  header = (Header *) typelib->data;
  if (header->shared_library)
    return gi_typelib_get_string (typelib, header->shared_library);
  else
    return NULL;
}

/**
 * gi_repository_get_c_prefix:
 * @repository: (nullable): A #GIRepository, or `NULL` for the singleton
 *   process-global default #GIRepository
 * @namespace_: Namespace to inspect
 *
 * This function returns the ‘C prefix’, or the C level namespace
 * associated with the given introspection namespace.
 *
 * Each C symbol starts with this prefix, as well each [type@GObject.Type] in
 * the library.
 *
 * Note: The namespace must have already been loaded using a function
 * such as [method@GIRepository.Repository.require] before calling this
 * function.
 *
 * Returns: (nullable): C namespace prefix, or `NULL` if none associated
 * Since: 2.80
 */
const gchar *
gi_repository_get_c_prefix (GIRepository *repository,
                            const gchar  *namespace_)
{
  GITypelib *typelib;
  Header *header;

  g_return_val_if_fail (namespace_ != NULL, NULL);

  repository = get_repository (repository);

  typelib = get_registered (repository, namespace_, NULL);

  g_return_val_if_fail (typelib != NULL, NULL);

  header = (Header *) typelib->data;
  if (header->c_prefix)
    return gi_typelib_get_string (typelib, header->c_prefix);
  else
    return NULL;
}

/**
 * gi_repository_get_typelib_path:
 * @repository: (nullable): A #GIRepository, or `NULL` for the singleton
 *   process-global default #GIRepository
 * @namespace_: GI namespace to use, e.g. `Gtk`
 *
 * If namespace @namespace_ is loaded, return the full path to the
 * .typelib file it was loaded from.
 *
 * If the typelib for namespace @namespace_ was included in a shared library,
 * return the special string `<builtin>`.
 *
 * Returns: (type filename) (nullable): Filesystem path (or `<builtin>`) if
 *   successful, `NULL` if namespace is not loaded
 * Since: 2.80
 */
const gchar *
gi_repository_get_typelib_path (GIRepository *repository,
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
find_namespace_version (const char          *namespace,
                        const char          *version,
                        const char * const  *search_paths,
                        size_t               n_search_paths,
                        char               **path_ret)
{
  GError *error = NULL;
  GMappedFile *mfile = NULL;
  char *fname;

  fname = g_strdup_printf ("%s-%s.typelib", namespace, version);

  for (size_t i = 0; i < n_search_paths; ++i)
    {
      char *path = g_build_filename (search_paths[i], fname, NULL);

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

  /* Avoid a compiler warning about `success` being unused with G_DISABLE_ASSERT */
  (void) success;

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
  g_mapped_file_unref (candidate->mfile);
  g_free (candidate->path);
  g_free (candidate->version);
  g_slice_free (struct NamespaceVersionCandidadate, candidate);
}

static GSList *
enumerate_namespace_versions (const char         *namespace,
                              const char * const *search_paths,
                              size_t              n_search_paths)
{
  GSList *candidates = NULL;
  GHashTable *found_versions = g_hash_table_new (g_str_hash, g_str_equal);
  char *namespace_dash;
  char *namespace_typelib;
  GError *error = NULL;
  int index;

  namespace_dash = g_strdup_printf ("%s-", namespace);
  namespace_typelib = g_strdup_printf ("%s.typelib", namespace);

  index = 0;
  for (size_t i = 0; i < n_search_paths; ++i)
    {
      GDir *dir;
      const char *dirname;
      const char *entry;

      dirname = search_paths[i];
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
		{
		  g_free (version);
		  continue;
		}
	    }
	  else
	    continue;

	  if (g_hash_table_lookup (found_versions, version) != NULL)
	    {
	      g_free (version);
	      continue;
	    }

	  path = g_build_filename (dirname, entry, NULL);
	  mfile = g_mapped_file_new (path, FALSE, &error);
	  if (mfile == NULL)
	    {
	      g_free (path);
	      g_free (version);
	      g_clear_error (&error);
	      continue;
	    }
	  candidate = g_slice_new0 (struct NamespaceVersionCandidadate);
	  candidate->mfile = mfile;
	  candidate->path_index = index;
	  candidate->path = path;
	  candidate->version = version;
	  candidates = g_slist_prepend (candidates, candidate);
	  g_hash_table_add (found_versions, version);
	}
      g_dir_close (dir);
      index++;
    }

  g_free (namespace_dash);
  g_free (namespace_typelib);
  g_hash_table_destroy (found_versions);

  return candidates;
}

static GMappedFile *
find_namespace_latest (const char          *namespace,
                       const char * const  *search_paths,
                       size_t               n_search_paths,
                       char               **version_ret,
                       char               **path_ret)
{
  GSList *candidates;
  GMappedFile *result = NULL;

  *version_ret = NULL;
  *path_ret = NULL;

  candidates = enumerate_namespace_versions (namespace, search_paths, n_search_paths);

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
      g_slice_free (struct NamespaceVersionCandidadate, elected); /* just free the container */
      g_slist_foreach (candidates, (GFunc) (void *) free_candidate, NULL);
      g_slist_free (candidates);
    }
  return result;
}

/**
 * gi_repository_enumerate_versions:
 * @repository: (nullable): A #GIRepository, or `NULL` for the singleton
 *   process-global default #GIRepository
 * @namespace_: GI namespace, e.g. `Gtk`
 * @n_versions_out: (optional) (out): The number of versions returned.
 *
 * Obtain an unordered list of versions (either currently loaded or
 * available) for @namespace_ in this @repository.
 *
 * Returns: (element-type utf8) (transfer full) (array length=n_versions_out): the array of versions.
 * Since: 2.80
 */
char **
gi_repository_enumerate_versions (GIRepository *repository,
                                  const gchar  *namespace_,
                                  size_t       *n_versions_out)
{
  GPtrArray *versions;
  GSList *candidates, *link;
  const gchar *loaded_version;
  char **ret;

  init_globals ();
  candidates = enumerate_namespace_versions (namespace_,
                                             (const char * const *) typelib_search_path->pdata,
                                             typelib_search_path->len);

  if (!candidates)
    {
      if (n_versions_out)
        *n_versions_out = 0;
      return g_strdupv ((char *[]) {NULL});
    }

  versions = g_ptr_array_new_null_terminated (1, g_free, TRUE);
  for (link = candidates; link; link = link->next)
    {
      struct NamespaceVersionCandidadate *candidate = link->data;
      g_ptr_array_add (versions, g_steal_pointer (&candidate->version));
      free_candidate (candidate);
    }
  g_slist_free (candidates);

  /* The currently loaded version of a namespace is also part of the
   * available versions, as it could have been loaded using
   * require_private().
   */
  if (gi_repository_is_registered (repository, namespace_, NULL))
    {
      loaded_version = gi_repository_get_version (repository, namespace_);
      if (loaded_version &&
          !g_ptr_array_find_with_equal_func (versions, loaded_version, g_str_equal, NULL))
        g_ptr_array_add (versions, g_strdup (loaded_version));
    }

  ret = (char **) g_ptr_array_steal (versions, n_versions_out);
  g_ptr_array_unref (g_steal_pointer (&versions));

  return ret;
}

static GITypelib *
require_internal (GIRepository           *repository,
                  const char             *namespace,
                  const char             *version,
                  GIRepositoryLoadFlags   flags,
                  const char * const     *search_paths,
                  size_t                  n_search_paths,
                  GError                **error)
{
  GMappedFile *mfile;
  GITypelib *ret = NULL;
  Header *header;
  GITypelib *typelib = NULL;
  const gchar *typelib_namespace, *typelib_version;
  gboolean allow_lazy = (flags & GI_REPOSITORY_LOAD_FLAG_LAZY) > 0;
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
      g_set_error (error, GI_REPOSITORY_ERROR,
		   GI_REPOSITORY_ERROR_NAMESPACE_VERSION_CONFLICT,
		   "Requiring namespace '%s' version '%s', but '%s' is already loaded",
		   namespace, version, version_conflict);
      return NULL;
    }

  if (version != NULL)
    {
      mfile = find_namespace_version (namespace, version, search_paths,
                                      n_search_paths, &path);
      tmp_version = g_strdup (version);
    }
  else
    {
      mfile = find_namespace_latest (namespace, search_paths, n_search_paths,
                                     &tmp_version, &path);
    }

  if (mfile == NULL)
    {
      if (version != NULL)
	g_set_error (error, GI_REPOSITORY_ERROR,
		     GI_REPOSITORY_ERROR_TYPELIB_NOT_FOUND,
		     "Typelib file for namespace '%s', version '%s' not found",
		     namespace, version);
      else
	g_set_error (error, GI_REPOSITORY_ERROR,
		     GI_REPOSITORY_ERROR_TYPELIB_NOT_FOUND,
		     "Typelib file for namespace '%s' (any version) not found",
		     namespace);
      goto out;
    }

  {
    GError *temp_error = NULL;
    typelib = gi_typelib_new_from_mapped_file (mfile, &temp_error);
    if (!typelib)
      {
	g_set_error (error, GI_REPOSITORY_ERROR,
		     GI_REPOSITORY_ERROR_TYPELIB_NOT_FOUND,
		     "Failed to load typelib file '%s' for namespace '%s': %s",
		     path, namespace, temp_error->message);
	g_clear_error (&temp_error);
	goto out;
      }
  }
  header = (Header *) typelib->data;
  typelib_namespace = gi_typelib_get_string (typelib, header->namespace);
  typelib_version = gi_typelib_get_string (typelib, header->nsversion);

  if (strcmp (typelib_namespace, namespace) != 0)
    {
      g_set_error (error, GI_REPOSITORY_ERROR,
		   GI_REPOSITORY_ERROR_NAMESPACE_MISMATCH,
		   "Typelib file %s for namespace '%s' contains "
		   "namespace '%s' which doesn't match the file name",
		   path, namespace, typelib_namespace);
      gi_typelib_free (typelib);
      goto out;
    }
  if (version != NULL && strcmp (typelib_version, version) != 0)
    {
      g_set_error (error, GI_REPOSITORY_ERROR,
		   GI_REPOSITORY_ERROR_NAMESPACE_MISMATCH,
		   "Typelib file %s for namespace '%s' contains "
		   "version '%s' which doesn't match the expected version '%s'",
		   path, namespace, typelib_version, version);
      gi_typelib_free (typelib);
      goto out;
    }

  if (!register_internal (repository, path, allow_lazy,
			  typelib, error))
    {
      gi_typelib_free (typelib);
      goto out;
    }
  ret = typelib;
 out:
  g_free (tmp_version);
  g_free (path);
  return ret;
}

/**
 * gi_repository_require:
 * @repository: (nullable): A #GIRepository, or `NULL` for the singleton
 *   process-global default #GIRepository
 * @namespace_: GI namespace to use, e.g. `Gtk`
 * @version: (nullable): Version of namespace, may be `NULL` for latest
 * @flags: Set of [flags@GIRepository.RepositoryLoadFlags], may be 0
 * @error: a [type@GLib.Error].
 *
 * Force the namespace @namespace_ to be loaded if it isn’t already.
 *
 * If @namespace_ is not loaded, this function will search for a
 * `.typelib` file using the repository search path.  In addition, a
 * version @version of namespace may be specified.  If @version is
 * not specified, the latest will be used.
 *
 * Returns: (transfer none): a pointer to the [type@GITypelib.Typelib] if
 *   successful, `NULL` otherwise
 * Since: 2.80
 */
GITypelib *
gi_repository_require (GIRepository  *repository,
		       const gchar   *namespace,
		       const gchar   *version,
		       GIRepositoryLoadFlags flags,
		       GError       **error)
{
  GITypelib *typelib;

  init_globals ();
  typelib = require_internal (repository, namespace, version, flags,
                              (const char * const *) typelib_search_path->pdata,
                              typelib_search_path->len, error);

  return typelib;
}

/**
 * gi_repository_require_private:
 * @repository: (nullable): A #GIRepository, or `NULL` for the singleton
 *   process-global default #GIRepository
 * @typelib_dir: (type filename): Private directory where to find the requested
 *   typelib
 * @namespace_: GI namespace to use, e.g. `Gtk`
 * @version: (nullable): Version of namespace, may be `NULL` for latest
 * @flags: Set of [flags@GIRepository.RepositoryLoadFlags], may be 0
 * @error: a [type@GLib.Error].
 *
 * Force the namespace @namespace_ to be loaded if it isn’t already.
 *
 * If @namespace_ is not loaded, this function will search for a
 * `.typelib` file within the private directory only. In addition, a
 * version @version of namespace should be specified.  If @version is
 * not specified, the latest will be used.
 *
 * Returns: (transfer none): a pointer to the [type@GITypelib.Typelib] if
 *   successful, `NULL` otherwise
 * Since: 2.80
 */
GITypelib *
gi_repository_require_private (GIRepository  *repository,
			       const gchar   *typelib_dir,
			       const gchar   *namespace,
			       const gchar   *version,
			       GIRepositoryLoadFlags flags,
			       GError       **error)
{
  const char * const search_path[] = { typelib_dir, NULL };

  return require_internal (repository, namespace, version, flags,
                           search_path, 1, error);
}

static gboolean
gi_repository_introspect_cb (const char *option_name,
			     const char *value,
			     gpointer data,
			     GError **error)
{
  GError *tmp_error = NULL;
  char **args;

  args = g_strsplit (value, ",", 2);

  if (!gi_repository_dump (args[0], args[1], &tmp_error))
    {
      g_error ("Failed to extract GType data: %s",
	       tmp_error->message);
      exit (1);
    }
  exit (0);
}

static const GOptionEntry introspection_args[] = {
  { "introspect-dump", 0, G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_CALLBACK,
    gi_repository_introspect_cb, "Dump introspection information",
    "infile.txt,outfile.xml" },
  G_OPTION_ENTRY_NULL
};

/**
 * gi_repository_get_option_group:
 *
 * Obtain the option group for girepository.
 *
 * It’s used by the dumper and for programs that want to provide introspection
 * information
 *
 * Returns: (transfer full): the option group
 * Since: 2.80
 */
GOptionGroup *
gi_repository_get_option_group (void)
{
  GOptionGroup *group;
  group = g_option_group_new ("girepository", "Introspection Options", "Show Introspection Options", NULL, NULL);

  g_option_group_add_entries (group, introspection_args);
  return group;
}

GQuark
gi_repository_error_quark (void)
{
  static GQuark quark = 0;
  if (quark == 0)
    quark = g_quark_from_static_string ("g-irepository-error-quark");
  return quark;
}

/**
 * gi_type_tag_to_string:
 * @type: the type_tag
 *
 * Obtain a string representation of @type
 *
 * Returns: the string
 * Since: 2.80
 */
const gchar*
gi_type_tag_to_string (GITypeTag type)
{
  switch (type)
    {
    case GI_TYPE_TAG_VOID:
      return "void";
    case GI_TYPE_TAG_BOOLEAN:
      return "gboolean";
    case GI_TYPE_TAG_INT8:
      return "gint8";
    case GI_TYPE_TAG_UINT8:
      return "guint8";
    case GI_TYPE_TAG_INT16:
      return "gint16";
    case GI_TYPE_TAG_UINT16:
      return "guint16";
    case GI_TYPE_TAG_INT32:
      return "gint32";
    case GI_TYPE_TAG_UINT32:
      return "guint32";
    case GI_TYPE_TAG_INT64:
      return "gint64";
    case GI_TYPE_TAG_UINT64:
      return "guint64";
    case GI_TYPE_TAG_FLOAT:
      return "gfloat";
    case GI_TYPE_TAG_DOUBLE:
      return "gdouble";
    case GI_TYPE_TAG_UNICHAR:
      return "gunichar";
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

/**
 * gi_info_type_to_string:
 * @type: the info type
 *
 * Obtain a string representation of @type
 *
 * Returns: the string
 * Since: 2.80
 */
const gchar*
gi_info_type_to_string (GIInfoType type)
{
  switch (type)
    {
    case GI_INFO_TYPE_INVALID:
      return "invalid";
    case GI_INFO_TYPE_FUNCTION:
      return "function";
    case GI_INFO_TYPE_CALLBACK:
      return "callback";
    case GI_INFO_TYPE_STRUCT:
      return "struct";
    case GI_INFO_TYPE_BOXED:
      return "boxed";
    case GI_INFO_TYPE_ENUM:
      return "enum";
    case GI_INFO_TYPE_FLAGS:
      return "flags";
    case GI_INFO_TYPE_OBJECT:
      return "object";
    case GI_INFO_TYPE_INTERFACE:
      return "interface";
    case GI_INFO_TYPE_CONSTANT:
      return "constant";
    case GI_INFO_TYPE_UNION:
      return "union";
    case GI_INFO_TYPE_VALUE:
      return "value";
    case GI_INFO_TYPE_SIGNAL:
      return "signal";
    case GI_INFO_TYPE_VFUNC:
      return "vfunc";
    case GI_INFO_TYPE_PROPERTY:
      return "property";
    case GI_INFO_TYPE_FIELD:
      return "field";
    case GI_INFO_TYPE_ARG:
      return "arg";
    case GI_INFO_TYPE_TYPE:
      return "type";
    case GI_INFO_TYPE_UNRESOLVED:
      return "unresolved";
    default:
      return "unknown";
  }
}


/**
 * gi_repository_prepend_library_path:
 * @directory: (type filename): a single directory to scan for shared libraries
 *
 * Prepends @directory to the search path that is used to
 * search shared libraries referenced by imported namespaces.
 *
 * Multiple calls to this function all contribute to the final
 * list of paths.
 *
 * The list of paths is unique and shared for all
 * [class@GIRepository.Repository] instances across the process, but it doesn’t
 * affect namespaces imported before the call.
 *
 * If the library is not found in the directories configured
 * in this way, loading will fall back to the system library
 * path (i.e. `LD_LIBRARY_PATH` and `DT_RPATH` in ELF systems).
 * See the documentation of your dynamic linker for full details.
 *
 * Since: 2.80
 */
void
gi_repository_prepend_library_path (const char *directory)
{
  gi_typelib_prepend_library_path(directory);
}

