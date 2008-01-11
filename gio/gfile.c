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

#include <config.h>
#include <string.h>
#include <sys/types.h>
#ifdef HAVE_PWD_H
#include <pwd.h>
#endif
#include "gfile.h"
#include "gvfs.h"
#include "gioscheduler.h"
#include <glocalfile.h>
#include "gsimpleasyncresult.h"
#include "gfileattribute-priv.h"
#include "gpollfilemonitor.h"
#include "glibintl.h"

#include "gioalias.h"

/**
 * SECTION:gfile
 * @short_description: File and Directory Handling
 * @include: gio.h
 * @see_also: #GFileInfo, #GFileEnumerator
 * 
 * #GFile is a high level abstraction for manipulating files on a 
 * virtual file system. #GFile<!-- -->s are lightweight, immutable 
 * objects that do no I/O upon creation. It is necessary to understand that
 * #GFile objects do not represent files, merely a handle to a file. All
 * file I/O is implemented as streaming operations (see #GInputStream and 
 * #GOutputStream).
 * 
 * To construct a #GFile, you can use: 
 * g_file_new_for_path() if you have a path.
 * g_file_new_for_uri() if you have a URI.
 * g_file_new_for_commandline_arg() for a command line argument.
 * 
 * You can move through the filesystem with #GFile handles with
 * g_file_get_parent() to get a handle to the parent directory.
 * g_file_get_child() to get a handle to a child within a directory.
 * g_file_resolve_relative_path() to resolve a relative path between
 * two #GFile<!-- -->s.
 * 
 * Many #GFile operations have both synchronous and asynchronous versions 
 * to suit your application. Asynchronous versions of synchronous functions 
 * simply have _async() appended to their function names. The asynchronous 
 * I/O functions call a #GAsyncReadyCallback which is then used to finalize 
 * the operation, producing a GAsyncResult which is then passed to the 
 * function's matching _finish() 
 * operation. 
 *
 * Some #GFile operations do not have synchronous analogs, as they may
 * take a very long time to finish, and blocking may leave an application
 * unusable. Notable cases include:
 * g_file_mount_mountable() to mount a mountable file.
 * g_file_unmount_mountable() to unmount a mountable file.
 * g_file_eject_mountable() to eject a mountable file.
 * 
 * <para id="gfile-etag"><indexterm><primary>entity tag</primary></indexterm>
 * One notable feature of #GFile<!-- -->s are entity tags, or "etags" for 
 * short. Entity tags are somewhat like a more abstract version of the 
 * traditional mtime, and can be used to quickly determine if the file has
 * been modified from the version on the file system. See the HTTP 1.1 
 * <ulink url="http://www.w3.org/Protocols/rfc2616/rfc2616-sec14.html">specification</ulink>
 * for HTTP Etag headers, which are a very similar concept.
 * </para>
 **/

static void g_file_base_init (gpointer g_class);
static void g_file_class_init (gpointer g_class,
			       gpointer class_data);

static void               g_file_real_query_info_async          (GFile                *file,
								 const char           *attributes,
								 GFileQueryInfoFlags   flags,
								 int                   io_priority,
								 GCancellable         *cancellable,
								 GAsyncReadyCallback   callback,
								 gpointer              user_data);
static GFileInfo *        g_file_real_query_info_finish         (GFile                *file,
								 GAsyncResult         *res,
								 GError              **error);
static void               g_file_real_enumerate_children_async  (GFile                *file,
								 const char           *attributes,
								 GFileQueryInfoFlags   flags,
								 int                   io_priority,
								 GCancellable         *cancellable,
								 GAsyncReadyCallback   callback,
								 gpointer              user_data);
static GFileEnumerator *  g_file_real_enumerate_children_finish (GFile                *file,
								 GAsyncResult         *res,
								 GError              **error);
static void               g_file_real_read_async                (GFile                *file,
								 int                   io_priority,
								 GCancellable         *cancellable,
								 GAsyncReadyCallback   callback,
								 gpointer              user_data);
static GFileInputStream * g_file_real_read_finish               (GFile                *file,
								 GAsyncResult         *res,
								 GError              **error);
static void               g_file_real_append_to_async           (GFile                *file,
								 GFileCreateFlags      flags,
								 int                   io_priority,
								 GCancellable         *cancellable,
								 GAsyncReadyCallback   callback,
								 gpointer              user_data);
static GFileOutputStream *g_file_real_append_to_finish          (GFile                *file,
								 GAsyncResult         *res,
								 GError              **error);
static void               g_file_real_create_async              (GFile                *file,
								 GFileCreateFlags      flags,
								 int                   io_priority,
								 GCancellable         *cancellable,
								 GAsyncReadyCallback   callback,
								 gpointer              user_data);
static GFileOutputStream *g_file_real_create_finish             (GFile                *file,
								 GAsyncResult         *res,
								 GError              **error);
static void               g_file_real_replace_async             (GFile                *file,
								 const char           *etag,
								 gboolean              make_backup,
								 GFileCreateFlags      flags,
								 int                   io_priority,
								 GCancellable         *cancellable,
								 GAsyncReadyCallback   callback,
								 gpointer              user_data);
static GFileOutputStream *g_file_real_replace_finish            (GFile                *file,
								 GAsyncResult         *res,
								 GError              **error);
static gboolean           g_file_real_set_attributes_from_info  (GFile                *file,
								 GFileInfo            *info,
								 GFileQueryInfoFlags   flags,
								 GCancellable         *cancellable,
								 GError              **error);
static void               g_file_real_set_display_name_async    (GFile                *file,
								 const char           *display_name,
								 int                   io_priority,
								 GCancellable         *cancellable,
								 GAsyncReadyCallback   callback,
								 gpointer              user_data);
static GFile *            g_file_real_set_display_name_finish   (GFile                *file,
								 GAsyncResult         *res,
								 GError              **error);
static void               g_file_real_set_attributes_async      (GFile                *file,
								 GFileInfo            *info,
								 GFileQueryInfoFlags   flags,
								 int                   io_priority,
								 GCancellable         *cancellable,
								 GAsyncReadyCallback   callback,
								 gpointer              user_data);
static gboolean           g_file_real_set_attributes_finish     (GFile                *file,
								 GAsyncResult         *res,
								 GFileInfo           **info,
								 GError              **error);

GType
g_file_get_type (void)
{
  static GType file_type = 0;

  if (! file_type)
    {
      static const GTypeInfo file_info =
      {
        sizeof (GFileIface), /* class_size */
	g_file_base_init,   /* base_init */
	NULL,		/* base_finalize */
	g_file_class_init,
	NULL,		/* class_finalize */
	NULL,		/* class_data */
	0,
	0,              /* n_preallocs */
	NULL
      };

      file_type =
	g_type_register_static (G_TYPE_INTERFACE, I_("GFile"),
				&file_info, 0);

      g_type_interface_add_prerequisite (file_type, G_TYPE_OBJECT);
    }

  return file_type;
}

static void
g_file_class_init (gpointer g_class,
		   gpointer class_data)
{
  GFileIface *iface = g_class;

  iface->enumerate_children_async = g_file_real_enumerate_children_async;
  iface->enumerate_children_finish = g_file_real_enumerate_children_finish;
  iface->set_display_name_async = g_file_real_set_display_name_async;
  iface->set_display_name_finish = g_file_real_set_display_name_finish;
  iface->query_info_async = g_file_real_query_info_async;
  iface->query_info_finish = g_file_real_query_info_finish;
  iface->set_attributes_async = g_file_real_set_attributes_async;
  iface->set_attributes_finish = g_file_real_set_attributes_finish;
  iface->read_async = g_file_real_read_async;
  iface->read_finish = g_file_real_read_finish;
  iface->append_to_async = g_file_real_append_to_async;
  iface->append_to_finish = g_file_real_append_to_finish;
  iface->create_async = g_file_real_create_async;
  iface->create_finish = g_file_real_create_finish;
  iface->replace_async = g_file_real_replace_async;
  iface->replace_finish = g_file_real_replace_finish;
  iface->set_attributes_from_info = g_file_real_set_attributes_from_info;
}

static void
g_file_base_init (gpointer g_class)
{
}


/**
 * g_file_is_native:
 * @file: input #GFile.
 *
 * Checks to see if a file is native to the platform.
 *
 * A native file s one expressed in the platform-native filename format,
 * e.g. "C:\Windows" or "/usr/bin/". This does not mean the file is local,
 * as it might be on a locally mounted remote filesystem.
 *
 * On some systems non-native files may be available using
 * the native filesystem via a userspace filesystem (FUSE), in
 * these cases this call will return %FALSE, but g_file_get_path()
 * will still return a native path.
 *
 * This call does no blocking i/o.
 * 
 * Returns: %TRUE if file is native. 
 **/
gboolean
g_file_is_native (GFile *file)
{
  GFileIface *iface;

  g_return_val_if_fail (G_IS_FILE (file), FALSE);

  iface = G_FILE_GET_IFACE (file);

  return (* iface->is_native) (file);
}


/**
 * g_file_has_uri_scheme: 
 * @file: input #GFile.
 * @uri_scheme: a string containing a URI scheme.
 *
 * Checks to see if a #GFile has a given URI scheme.
 *
 * This call does no blocking i/o.
 * 
 * Returns: %TRUE if #GFile's backend supports the
 *     given URI scheme, %FALSE if URI scheme is %NULL,
 *     not supported, or #GFile is invalid.
 **/
gboolean
g_file_has_uri_scheme (GFile      *file,
		       const char *uri_scheme)
{
  GFileIface *iface;
  
  g_return_val_if_fail (G_IS_FILE (file), FALSE);
  g_return_val_if_fail (uri_scheme != NULL, FALSE);

  iface = G_FILE_GET_IFACE (file);

  return (* iface->has_uri_scheme) (file, uri_scheme);
}


/**
 * g_file_get_uri_scheme:
 * @file: input #GFile.
 *
 * Gets the URI scheme for a #GFile.
 * RFC 3986 decodes the scheme as:
 * <programlisting>
 * URI = scheme ":" hier-part [ "?" query ] [ "#" fragment ] 
 * </programlisting>
 * Common schemes include "file", "http", "ftp", etc. 
 *
 * This call does no blocking i/o.
 * 
 * Returns: a string containing the URI scheme for the given 
 *     #GFile. The returned string should be freed with g_free() 
 *     when no longer needed.
 **/
char *
g_file_get_uri_scheme (GFile *file)
{
  GFileIface *iface;

  g_return_val_if_fail (G_IS_FILE (file), NULL);

  iface = G_FILE_GET_IFACE (file);

  return (* iface->get_uri_scheme) (file);
}


/**
 * g_file_get_basename:
 * @file: input #GFile.
 *
 * Gets the base name (the last component of the path) for a given #GFile.
 *
 * If called for the top level of a system (such as the filesystem root
 * or a uri like sftp://host/ it will return a single directory separator
 * (and on Windows, possibly a drive letter).
 *
 * This call does no blocking i/o.
 * 
 * Returns: string containing the #GFile's base name, or %NULL 
 *     if given #GFile is invalid. The returned string should be 
 *     freed with g_free() when no longer needed.
 **/
char *
g_file_get_basename (GFile *file)
{
  GFileIface *iface;
  
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  iface = G_FILE_GET_IFACE (file);

  return (* iface->get_basename) (file);
}

/**
 * g_file_get_path:
 * @file: input #GFile.
 *
 * Gets the local pathname for #GFile, if one exists. 
 *
 * This call does no blocking i/o.
 * 
 * Returns: string containing the #GFile's path, or %NULL if 
 *     no such path exists. The returned string should be 
 *     freed with g_free() when no longer needed.
 **/
char *
g_file_get_path (GFile *file)
{
  GFileIface *iface;

  g_return_val_if_fail (G_IS_FILE (file), NULL);

  iface = G_FILE_GET_IFACE (file);

  return (* iface->get_path) (file);
}

/**
 * g_file_get_uri:
 * @file: input #GFile.
 *
 * Gets the URI for the @file.
 *
 * This call does no blocking i/o.
 * 
 * Returns: a string containing the #GFile's URI.
 *     The returned string should be freed with g_free() when no longer needed.
 **/
char *
g_file_get_uri (GFile *file)
{
  GFileIface *iface;
  
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  iface = G_FILE_GET_IFACE (file);

  return (* iface->get_uri) (file);
}

/**
 * g_file_get_parse_name:
 * @file: input #GFile.
 *
 * Gets the parse name of the @file.
 * A parse name is a UTF-8 string that describes the
 * file such that one can get the #GFile back using
 * g_file_parse_name().
 *
 * This is generally used to show the #GFile as a nice
 * string in a user interface, like in a location entry.
 *
 * For local files with names that can safely be converted
 * to UTF8 the pathname is used, otherwise the IRI is used
 * (a form of URI that allows UTF8 characters unescaped).
 *
 * This call does no blocking i/o.
 * 
 * Returns: a string containing the #GFile's parse name. The returned 
 *     string should be freed with g_free() when no longer needed.
 **/
char *
g_file_get_parse_name (GFile *file)
{
  GFileIface *iface;
  
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  iface = G_FILE_GET_IFACE (file);

  return (* iface->get_parse_name) (file);
}

/**
 * g_file_dup:
 * @file: input #GFile.
 *
 * Duplicates a #GFile handle. This operation does not duplicate 
 * the actual file or directory represented by the #GFile; see 
 * g_file_copy() if attempting to copy a file. 
 *
 * This call does no blocking i/o.
 * 
 * Returns: #GFile that is a duplicate of the given #GFile. 
 **/
GFile *
g_file_dup (GFile *file)
{
  GFileIface *iface;
  
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  iface = G_FILE_GET_IFACE (file);

  return (* iface->dup) (file);
}

/**
 * g_file_hash:
 * @file: #gconstpointer to a #GFile.
 *
 * Creates a hash value for a #GFile.
 *
 * This call does no blocking i/o.
 * 
 * Returns: 0 if @file is not a valid #GFile, otherwise an 
 *     integer that can be used as hash value for the #GFile. 
 *     This function is intended for easily hashing a #GFile to 
 *     add to a #GHashTable or similar data structure.
 **/
guint
g_file_hash (gconstpointer file)
{
  GFileIface *iface;
  
  g_return_val_if_fail (G_IS_FILE (file), 0);

  iface = G_FILE_GET_IFACE (file);

  return (* iface->hash) ((GFile *)file);
}

/**
 * g_file_equal:
 * @file1: the first #GFile.
 * @file2: the second #GFile.
 *
 * Checks equality of two given #GFile<!-- -->s
 *
 * This call does no blocking i/o.
 * 
 * Returns: %TRUE if @file1 and @file2 are equal.
 *     %FALSE if either is not a #GFile.
 **/
gboolean
g_file_equal (GFile *file1,
	      GFile *file2)
{
  GFileIface *iface;
  
  g_return_val_if_fail (G_IS_FILE (file1), FALSE);
  g_return_val_if_fail (G_IS_FILE (file2), FALSE);
  
  if (G_TYPE_FROM_INSTANCE (file1) != G_TYPE_FROM_INSTANCE (file2))
    return FALSE;

  iface = G_FILE_GET_IFACE (file1);
  
  return (* iface->equal) (file1, file2);
}


/**
 * g_file_get_parent:
 * @file: input #GFile.
 *
 * Gets the parent directory for the @file. 
 * If the @file represents the root directory of the 
 * file system, then %NULL will be returned.
 *
 * This call does no blocking i/o.
 * 
 * Returns: a #GFile structure to the parent of the given
 *     #GFile or %NULL if there is no parent. 
 **/
GFile *
g_file_get_parent (GFile *file)
{
  GFileIface *iface;
  
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  iface = G_FILE_GET_IFACE (file);

  return (* iface->get_parent) (file);
}

/**
 * g_file_get_child:
 * @file: input #GFile.
 * @name: string containing the child's name.
 *
 * Gets a specific child of @file with name equal to @name.
 *
 * Note that the file with that specific name might not exist, but
 * you can still have a #GFile that points to it. You can use this
 * for instance to create that file.
 *
 * This call does no blocking i/o.
 * 
 * Returns: a #GFile to a child specified by @name.
 **/
GFile *
g_file_get_child (GFile      *file,
		  const char *name)
{
  g_return_val_if_fail (G_IS_FILE (file), NULL);
  g_return_val_if_fail (name != NULL, NULL);

  return g_file_resolve_relative_path (file, name);
}

/**
 * g_file_get_child_for_display_name:
 * @file: input #GFile.
 * @display_name: string to a possible child.
 * @error: #GError.
 *
 * Gets the child of @file for a given @display_name (i.e. a UTF8
 * version of the name). If this function fails, it returns %NULL and @error will be 
 * set. This is very useful when constructing a GFile for a new file
 * and the user entered the filename in the user interface, for instance
 * when you select a directory and type a filename in the file selector.
 * 
 * This call does no blocking i/o.
 * 
 * Returns: a #GFile to the specified child, or 
 *     %NULL if the display name couldn't be converted.  
 **/
GFile *
g_file_get_child_for_display_name (GFile      *file,
				   const char *display_name,
				   GError **error)
{
  GFileIface *iface;
  
  g_return_val_if_fail (G_IS_FILE (file), NULL);
  g_return_val_if_fail (display_name != NULL, NULL);

  iface = G_FILE_GET_IFACE (file);

  return (* iface->get_child_for_display_name) (file, display_name, error);
}

/**
 * g_file_contains_file:
 * @parent: input #GFile.
 * @descendant: input #GFile.
 * 
 * Checks whether @parent (recursively) contains the specified @descendant.
 * 
 * This call does no blocking i/o.
 * 
 * Returns:  %TRUE if the @descendant's parent, grandparent, etc is @parent. %FALSE otherwise.
 **/
gboolean
g_file_contains_file (GFile *parent,
		      GFile *descendant)
{
  GFileIface *iface;
  
  g_return_val_if_fail (G_IS_FILE (parent), FALSE);
  g_return_val_if_fail (G_IS_FILE (descendant), FALSE);

  if (G_TYPE_FROM_INSTANCE (parent) != G_TYPE_FROM_INSTANCE (descendant))
    return FALSE;
  
  iface = G_FILE_GET_IFACE (parent);

  return (* iface->contains_file) (parent, descendant);
}

/**
 * g_file_get_relative_path:
 * @parent: input #GFile.
 * @descendant: input #GFile.
 *
 * Gets the path for @descendant relative to @parent. 
 *
 * This call does no blocking i/o.
 * 
 * Returns: string with the relative path from @descendant 
 *     to @parent, or %NULL if @descendant is not a descendant of @parent. The returned string should be freed with 
 *     g_free() when no longer needed.
 **/
char *
g_file_get_relative_path (GFile *parent,
			  GFile *descendant)
{
  GFileIface *iface;
  
  g_return_val_if_fail (G_IS_FILE (parent), NULL);
  g_return_val_if_fail (G_IS_FILE (descendant), NULL);

  if (G_TYPE_FROM_INSTANCE (parent) != G_TYPE_FROM_INSTANCE (descendant))
    return NULL;
  
  iface = G_FILE_GET_IFACE (parent);

  return (* iface->get_relative_path) (parent, descendant);
}

/**
 * g_file_resolve_relative_path:
 * @file: input #GFile.
 * @relative_path: a given relative path string.
 *
 * Resolves a relative path for @file to an absolute path.
 *
 * This call does no blocking i/o.
 * 
 * Returns: #GFile to the resolved path. %NULL if @relative_path 
 *     is %NULL or if @file is invalid.
 **/
GFile *
g_file_resolve_relative_path (GFile      *file,
			      const char *relative_path)
{
  GFileIface *iface;

  g_return_val_if_fail (G_IS_FILE (file), NULL);
  g_return_val_if_fail (relative_path != NULL, NULL);

  iface = G_FILE_GET_IFACE (file);

  return (* iface->resolve_relative_path) (file, relative_path);
}

/**
 * g_file_enumerate_children:
 * @file: input #GFile.
 * @attributes: an attribute query string.
 * @flags: a set of #GFileQueryInfoFlags.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @error: #GError for error reporting.
 *
 * Gets the requested information about the files in a directory. The result
 * is a #GFileEnumerator object that will give out #GFileInfo objects for
 * all the files in the directory.
 *
 * The @attribute value is a string that specifies the file attributes that
 * should be gathered. It is not an error if its not possible to read a particular
 * requested attribute from a file, it just won't be set. @attribute should
 * be a comma-separated list of attribute or attribute wildcards. The wildcard "*"
 * means all attributes, and a wildcard like "standard::*" means all attributes in the standard
 * namespace. An example attribute query be "standard::*,owner::user".
 * The standard attributes are available as defines, like #G_FILE_ATTRIBUTE_STANDARD_NAME.
 *
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. 
 * 
 * If the file does not exist, the G_IO_ERROR_NOT_FOUND error will be returned.
 * If the file is not a directory, the G_FILE_ERROR_NOTDIR error will be returned.
 * Other errors are possible too.
 *
 * Returns: A #GFileEnumerator if successful, %NULL on error. 
 **/
GFileEnumerator *
g_file_enumerate_children (GFile                *file,
			   const char           *attributes,
			   GFileQueryInfoFlags   flags,
			   GCancellable         *cancellable,
			   GError              **error)
			   
{
  GFileIface *iface;
  
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return NULL;
  
  iface = G_FILE_GET_IFACE (file);

  if (iface->enumerate_children == NULL)
    {
      g_set_error (error, G_IO_ERROR,
		   G_IO_ERROR_NOT_SUPPORTED,
		   _("Operation not supported"));
      return NULL;
    }

  return (* iface->enumerate_children) (file, attributes, flags,
					cancellable, error);
}

/**
 * g_file_enumerate_children_async:
 * @file: input #GFile.
 * @attributes: an attribute query string.
 * @flags: a set of #GFileQueryInfoFlags.
 * @io_priority: the <link linkend="io-priority">I/O priority</link> 
 *     of the request.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @callback: a #GAsyncReadyCallback to call when the request is satisfied
 * @user_data: the data to pass to callback function
 *
 * Asynchronously gets the requested information about the files in a directory. The result
 * is a #GFileEnumerator object that will give out #GFileInfo objects for
 * all the files in the directory.
 *
 * For more details, see g_file_enumerate_children() which is
 * the synchronous version of this call.
 *
 * When the operation is finished, @callback will be called. You can then call
 * g_file_enumerate_children_finish() to get the result of the operation.
 **/
void
g_file_enumerate_children_async (GFile               *file,
				 const char          *attributes,
				 GFileQueryInfoFlags  flags,
				 int                  io_priority,
				 GCancellable        *cancellable,
				 GAsyncReadyCallback  callback,
				 gpointer             user_data)
{
  GFileIface *iface;

  g_return_if_fail (G_IS_FILE (file));

  iface = G_FILE_GET_IFACE (file);
  (* iface->enumerate_children_async) (file,
				       attributes,
				       flags,
				       io_priority,
				       cancellable,
				       callback,
				       user_data);
}

/**
 * g_file_enumerate_children_finish:
 * @file: input #GFile.
 * @res: a #GAsyncResult.
 * @error: a #GError.
 * 
 * Finishes an async enumerate children operation.
 * See g_file_enumerate_children_async().
 *
 * Returns: a #GFileEnumerator or %NULL if an error occurred.
 **/
GFileEnumerator *
g_file_enumerate_children_finish (GFile         *file,
				  GAsyncResult  *res,
				  GError       **error)
{
  GFileIface *iface;
  
  g_return_val_if_fail (G_IS_FILE (file), NULL);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (res), NULL);

  if (G_IS_SIMPLE_ASYNC_RESULT (res))
    {
      GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (res);
      if (g_simple_async_result_propagate_error (simple, error))
	return NULL;
    }
  
  iface = G_FILE_GET_IFACE (file);
  return (* iface->enumerate_children_finish) (file, res, error);
}


/**
 * g_file_query_info:
 * @file: input #GFile.
 * @attributes: an attribute query string.
 * @flags: a set of #GFileQueryInfoFlags.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @error: a #GError.
 *
 * Gets the requested information about specified @file. The result
 * is a #GFileInfo objects that contains key-value attributes (like type or size
 * for the file.
 *
 * The @attribute value is a string that specifies the file attributes that
 * should be gathered. It is not an error if its not possible to read a particular
 * requested attribute from a file, it just won't be set. @attribute should
 * be a comma-separated list of attribute or attribute wildcards. The wildcard "*"
 * means all attributes, and a wildcard like "standard::*" means all attributes in the standard
 * namespace. An example attribute query be "standard::*,owner::user".
 * The standard attributes are available as defines, like #G_FILE_ATTRIBUTE_STANDARD_NAME.
 * 
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. 
 *
 * For symlinks, normally the information about the target of the
 * symlink is returned, rather than information about the symlink itself.
 * However if you pass #G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS in @flags the
 * information about the symlink itself will be returned. Also, for symlinks
 * that points to non-existing files the information about the symlink itself
 * will be returned.
 *
 * If the file does not exist, the G_IO_ERROR_NOT_FOUND error will be returned.
 * Other errors are possible too, and depend on what kind of filesystem the file is on.
 *
 * Returns: a #GFileInfo for the given @file, or %NULL on error.
 **/
GFileInfo *
g_file_query_info (GFile                *file,
		   const char           *attributes,
		   GFileQueryInfoFlags   flags,
		   GCancellable         *cancellable,
		   GError              **error)
{
  GFileIface *iface;
  
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return NULL;
  
  iface = G_FILE_GET_IFACE (file);

  if (iface->query_info == NULL)
    {
      g_set_error (error, G_IO_ERROR,
		   G_IO_ERROR_NOT_SUPPORTED,
		   _("Operation not supported"));
      return NULL;
    }
  
  return (* iface->query_info) (file, attributes, flags, cancellable, error);
}

/**
 * g_file_query_info_async:
 * @file: input #GFile.
 * @attributes: an attribute query string.
 * @flags: a set of #GFileQueryInfoFlags.
 * @io_priority: the <link linkend="io-priority">I/O priority</link> 
 *     of the request.
 * @cancellable: optional #GCancellable object, %NULL to ignore. 
 * @callback: a #GAsyncReadyCallback to call when the request is satisfied
 * @user_data: the data to pass to callback function
 * 
 * Asynchronously gets the requested information about specified @file. The result
 * is a #GFileInfo objects that contains key-value attributes (such as type or size
 * for the file).
 * 
 * For more details, see g_file_query_info() which is
 * the synchronous version of this call.
 *
 * When the operation is finished, @callback will be called. You can then call
 * g_file_query_info_finish() to get the result of the operation.
 **/
void
g_file_query_info_async (GFile               *file,
			 const char          *attributes,
			 GFileQueryInfoFlags  flags,
			 int                  io_priority,
			 GCancellable        *cancellable,
			 GAsyncReadyCallback  callback,
			 gpointer             user_data)
{
  GFileIface *iface;
  
  g_return_if_fail (G_IS_FILE (file));

  iface = G_FILE_GET_IFACE (file);
  (* iface->query_info_async) (file,
			       attributes,
			       flags,
			       io_priority,
			       cancellable,
			       callback,
			       user_data);
}

/**
 * g_file_query_info_finish:
 * @file: input #GFile.
 * @res: a #GAsyncResult. 
 * @error: a #GError. 
 * 
 * Finishes an asynchronous file info query. 
 * See g_file_query_info_async().
 * 
 * Returns: #GFileInfo for given @file or %NULL on error.
 **/
GFileInfo *
g_file_query_info_finish (GFile         *file,
			  GAsyncResult  *res,
			  GError       **error)
{
  GFileIface *iface;

  g_return_val_if_fail (G_IS_FILE (file), NULL);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (res), NULL);

  if (G_IS_SIMPLE_ASYNC_RESULT (res))
    {
      GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (res);
      if (g_simple_async_result_propagate_error (simple, error))
	return NULL;
    }
  
  iface = G_FILE_GET_IFACE (file);
  return (* iface->query_info_finish) (file, res, error);
}

/**
 * g_file_query_filesystem_info:
 * @file: input #GFile.
 * @attributes:  an attribute query string.
 * @cancellable: optional #GCancellable object, %NULL to ignore. 
 * @error: a #GError. 
 * 
 * Similar to g_file_query_info(), but obtains information
 * about the filesystem the @file is on, rather than the file itself.
 * For instance the amount of space available and the type of
 * the filesystem.
 *
 * The @attribute value is a string that specifies the file attributes that
 * should be gathered. It is not an error if its not possible to read a particular
 * requested attribute from a file, it just won't be set. @attribute should
 * be a comma-separated list of attribute or attribute wildcards. The wildcard "*"
 * means all attributes, and a wildcard like "fs:*" means all attributes in the fs
 * namespace. The standard namespace for filesystem attributes is "fs".
 * Common attributes of interest are #G_FILE_ATTRIBUTE_FILESYSTEM_SIZE
 * (the total size of the filesystem in bytes), #G_FILE_ATTRIBUTE_FILESYSTEM_FREE (number of
 * bytes available), and #G_FILE_ATTRIBUTE_FILESYSTEM_TYPE (type of the filesystem).
 * 
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. 
 *
 * If the file does not exist, the G_IO_ERROR_NOT_FOUND error will be returned.
 * Other errors are possible too, and depend on what kind of filesystem the file is on.
 *
 * Returns: a #GFileInfo or %NULL if there was an error.
 **/
GFileInfo *
g_file_query_filesystem_info (GFile         *file,
			      const char    *attributes,
			      GCancellable  *cancellable,
			      GError       **error)
{
  GFileIface *iface;
  
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return NULL;
  
  iface = G_FILE_GET_IFACE (file);

  if (iface->query_filesystem_info == NULL)
    {
      g_set_error (error, G_IO_ERROR,
		   G_IO_ERROR_NOT_SUPPORTED,
		   _("Operation not supported"));
      return NULL;
    }
  
  return (* iface->query_filesystem_info) (file, attributes, cancellable, error);
}

/**
 * g_file_find_enclosing_mount:
 * @file: input #GFile.
 * @cancellable: optional #GCancellable object, %NULL to ignore. 
 * @error: a #GError. 
 *
 * Gets a #GMount for the #GFile. 
 *
 * If the #GFileIface for @file does not have a mount (e.g. possibly a 
 * remote share), @error will be set to %G_IO_ERROR_NOT_FOUND and %NULL
 * will be returned.
 * 
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. 
 * 
 * Returns: a #GMount where the @file is located or %NULL on error.
 **/
GMount *
g_file_find_enclosing_mount (GFile         *file,
			     GCancellable  *cancellable,
			     GError       **error)
{
  GFileIface *iface;

  g_return_val_if_fail (G_IS_FILE (file), NULL);
  
  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return NULL;

  iface = G_FILE_GET_IFACE (file);
  if (iface->find_enclosing_mount == NULL)
    {
      g_set_error (error, G_IO_ERROR,
		   G_IO_ERROR_NOT_FOUND,
		   _("Containing mount does not exist"));
      return NULL;
    }
  
  return (* iface->find_enclosing_mount) (file, cancellable, error);
}

/**
 * g_file_read:
 * @file: #GFile to read.
 * @cancellable: a #GCancellable
 * @error: a #GError, or %NULL
 *
 * Opens a file for reading. The result is a #GFileInputStream that
 * can be used to read the contents of the file.
 *
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. 
 * 
 * If the file does not exist, the G_IO_ERROR_NOT_FOUND error will be returned.
 * If the file is a directory, the G_IO_ERROR_IS_DIRECTORY error will be returned.
 * Other errors are possible too, and depend on what kind of filesystem the file is on.
 *
 * Returns: #GFileInputStream or %NULL on error.
 **/
GFileInputStream *
g_file_read (GFile         *file,
	     GCancellable  *cancellable,
	     GError       **error)
{
  GFileIface *iface;
  
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return NULL;

  iface = G_FILE_GET_IFACE (file);

  if (iface->read_fn == NULL)
    {
      g_set_error (error, G_IO_ERROR,
		   G_IO_ERROR_NOT_SUPPORTED,
		   _("Operation not supported"));
      return NULL;
    }
  
  return (* iface->read_fn) (file, cancellable, error);
}

/**
 * g_file_append_to:
 * @file: input #GFile.
 * @flags: a set of #GFileCreateFlags.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @error: a #GError, or %NULL
 *
 * Gets an output stream for appending data to the file. If
 * the file doesn't already exist it is created.
 *
 * By default files created are generally readable by everyone,
 * but if you pass #G_FILE_CREATE_PRIVATE in @flags the file
 * will be made readable only to the current user, to the level that
 * is supported on the target filesystem.
 *
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. 
 *
 * Some file systems don't allow all file names, and may
 * return an G_IO_ERROR_INVALID_FILENAME error.
 * If the file is a directory the G_IO_ERROR_IS_DIRECTORY error will be
 * returned. Other errors are possible too, and depend on what kind of
 * filesystem the file is on.
 * 
 * Returns: a #GFileOutputStream.
 **/
GFileOutputStream *
g_file_append_to (GFile             *file,
		  GFileCreateFlags   flags,
		  GCancellable      *cancellable,
		  GError           **error)
{
  GFileIface *iface;

  g_return_val_if_fail (G_IS_FILE (file), NULL);

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return NULL;
  
  iface = G_FILE_GET_IFACE (file);

  if (iface->append_to == NULL)
    {
      g_set_error (error, G_IO_ERROR,
		   G_IO_ERROR_NOT_SUPPORTED,
		   _("Operation not supported"));
      return NULL;
    }
  
  return (* iface->append_to) (file, flags, cancellable, error);
}

/**
 * g_file_create:
 * @file: input #GFile.
 * @flags: a set of #GFileCreateFlags.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @error: a #GError, or %NULL
 *
 * Creates a new file and returns an output stream for writing to it.
 * The file must not already exists.
 *
 * By default files created are generally readable by everyone,
 * but if you pass #G_FILE_CREATE_PRIVATE in @flags the file
 * will be made readable only to the current user, to the level that
 * is supported on the target filesystem.
 *
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. 
 *
 * If a file with this name already exists the G_IO_ERROR_EXISTS error
 * will be returned. If the file is a directory the G_IO_ERROR_IS_DIRECTORY
 * error will be returned.
 * Some file systems don't allow all file names, and may
 * return an G_IO_ERROR_INVALID_FILENAME error, and if the name
 * is to long G_IO_ERROR_FILENAME_TOO_LONG will be returned.
 * Other errors are possible too, and depend on what kind of
 * filesystem the file is on.
 * 
 * Returns: a #GFileOutputStream for the newly created file, or 
 * %NULL on error.
 **/
GFileOutputStream *
g_file_create (GFile             *file,
	       GFileCreateFlags   flags,
	       GCancellable      *cancellable,
	       GError           **error)
{
  GFileIface *iface;
  
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return NULL;
  
  iface = G_FILE_GET_IFACE (file);

  if (iface->create == NULL)
    {
      g_set_error (error, G_IO_ERROR,
		   G_IO_ERROR_NOT_SUPPORTED,
		   _("Operation not supported"));
      return NULL;
    }
  
  return (* iface->create) (file, flags, cancellable, error);
}

/**
 * g_file_replace:
 * @file: input #GFile.
 * @etag: an optional <link linkend="gfile-etag">entity tag</link> for the 
 *     current #GFile, or #NULL to ignore.
 * @make_backup: %TRUE if a backup should be created.
 * @flags: a set of #GFileCreateFlags.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @error: a #GError, or %NULL
 *
 * Returns an output stream for overwriting the file, possibly
 * creating a backup copy of the file first.
 *
 * This will try to replace the file in the safest way possible so
 * that any errors during the writing will not affect an already
 * existing copy of the file. For instance, for local files it
 * may write to a temporary file and then atomically rename over
 * the destination when the stream is closed.
 * 
 * By default files created are generally readable by everyone,
 * but if you pass #G_FILE_CREATE_PRIVATE in @flags the file
 * will be made readable only to the current user, to the level that
 * is supported on the target filesystem.
 *
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. 
 * 
 * If you pass in a non-#NULL @etag value, then this value is
 * compared to the current entity tag of the file, and if they differ
 * an G_IO_ERROR_WRONG_ETAG error is returned. This generally means
 * that the file has been changed since you last read it. You can get
 * the new etag from g_file_output_stream_get_etag() after you've
 * finished writing and closed the #GFileOutputStream. When you load
 * a new file you can use g_file_input_stream_query_info() to get
 * the etag of the file.
 * 
 * If @make_backup is %TRUE, this function will attempt to make a backup
 * of the current file before overwriting it. If this fails a G_IO_ERROR_CANT_CREATE_BACKUP
 * error will be returned. If you want to replace anyway, try again with
 * @make_backup set to %FALSE.
 *
 * If the file is a directory the G_IO_ERROR_IS_DIRECTORY error will be returned,
 * and if the file is some other form of non-regular file then a
 * G_IO_ERROR_NOT_REGULAR_FILE error will be returned.
 * Some file systems don't allow all file names, and may
 * return an G_IO_ERROR_INVALID_FILENAME error, and if the name
 * is to long G_IO_ERROR_FILENAME_TOO_LONG will be returned.
 * Other errors are possible too, and depend on what kind of
 * filesystem the file is on.
 *
 * Returns: a #GFileOutputStream or %NULL on error. 
 **/
GFileOutputStream *
g_file_replace (GFile             *file,
		const char        *etag,
		gboolean           make_backup,
		GFileCreateFlags   flags,
		GCancellable      *cancellable,
		GError           **error)
{
  GFileIface *iface;

  g_return_val_if_fail (G_IS_FILE (file), NULL);

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return NULL;
  
  iface = G_FILE_GET_IFACE (file);

  if (iface->replace == NULL)
    {
      g_set_error (error, G_IO_ERROR,
		   G_IO_ERROR_NOT_SUPPORTED,
		   _("Operation not supported"));
      return NULL;
    }
  
  
  /* Handle empty tag string as NULL in consistent way. */
  if (etag && *etag == 0)
    etag = NULL;
  
  return (* iface->replace) (file, etag, make_backup, flags, cancellable, error);
}

/**
 * g_file_read_async:
 * @file: input #GFile.
 * @io_priority: the <link linkend="io-priority">I/O priority</link> 
 *     of the request. 
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @callback: a #GAsyncReadyCallback to call when the request is satisfied
 * @user_data: the data to pass to callback function
 *
 * Asynchronously opens @file for reading.
 *
 * For more details, see g_file_read() which is
 * the synchronous version of this call.
 *
 * When the operation is finished, @callback will be called. You can then call
 * g_file_read_finish() to get the result of the operation.
 **/
void
g_file_read_async (GFile               *file,
		   int                  io_priority,
		   GCancellable        *cancellable,
		   GAsyncReadyCallback  callback,
		   gpointer             user_data)
{
  GFileIface *iface;
  
  g_return_if_fail (G_IS_FILE (file));

  iface = G_FILE_GET_IFACE (file);
  (* iface->read_async) (file,
			 io_priority,
			 cancellable,
			 callback,
			 user_data);
}

/**
 * g_file_read_finish:
 * @file: input #GFile.
 * @res: a #GAsyncResult. 
 * @error: a #GError, or %NULL
 *
 * Finishes an asynchronous file read operation started with 
 * g_file_read_async(). 
 *  
 * Returns: a #GFileInputStream or %NULL on error.
 **/
GFileInputStream *
g_file_read_finish (GFile         *file,
		    GAsyncResult  *res,
		    GError       **error)
{
  GFileIface *iface;
  
  g_return_val_if_fail (G_IS_FILE (file), NULL);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (res), NULL);

  if (G_IS_SIMPLE_ASYNC_RESULT (res))
    {
      GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (res);
      if (g_simple_async_result_propagate_error (simple, error))
	return NULL;
    }
  
  iface = G_FILE_GET_IFACE (file);
  return (* iface->read_finish) (file, res, error);
}

/**
 * g_file_append_to_async:
 * @file: input #GFile.
 * @flags: a set of #GFileCreateFlags.
 * @io_priority: the <link linkend="io-priority">I/O priority</link> 
 *     of the request. 
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @callback: a #GAsyncReadyCallback to call when the request is satisfied
 * @user_data: the data to pass to callback function
 * 
 * Asynchronously opens @file for appending.
 *
 * For more details, see g_file_append_to() which is
 * the synchronous version of this call.
 *
 * When the operation is finished, @callback will be called. You can then call
 * g_file_append_to_finish() to get the result of the operation.
 **/
void
g_file_append_to_async (GFile               *file,
			GFileCreateFlags     flags,
			int                  io_priority,
			GCancellable        *cancellable,
			GAsyncReadyCallback  callback,
			gpointer             user_data)
{
  GFileIface *iface;
  
  g_return_if_fail (G_IS_FILE (file));

  iface = G_FILE_GET_IFACE (file);
  (* iface->append_to_async) (file,
			      flags,
			      io_priority,
			      cancellable,
			      callback,
			      user_data);
}

/**
 * g_file_append_to_finish:
 * @file: input #GFile.
 * @res: #GAsyncResult
 * @error: a #GError, or %NULL
 * 
 * Finishes an asynchronous file append operation started with 
 * g_file_append_to_async(). 
 * 
 * Returns: a valid #GFileOutputStream or %NULL on error.
 **/
GFileOutputStream *
g_file_append_to_finish (GFile         *file,
			 GAsyncResult  *res,
			 GError       **error)
{
  GFileIface *iface;
  
  g_return_val_if_fail (G_IS_FILE (file), NULL);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (res), NULL);

  if (G_IS_SIMPLE_ASYNC_RESULT (res))
    {
      GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (res);
      if (g_simple_async_result_propagate_error (simple, error))
	return NULL;
    }
  
  iface = G_FILE_GET_IFACE (file);
  return (* iface->append_to_finish) (file, res, error);
}

/**
 * g_file_create_async:
 * @file: input #GFile.
 * @flags: a set of #GFileCreateFlags.
 * @io_priority: the <link linkend="io-priority">I/O priority</link> 
 *     of the request.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @callback: a #GAsyncReadyCallback to call when the request is satisfied
 * @user_data: the data to pass to callback function
 * 
 * Asynchronously creates a new file and returns an output stream for writing to it.
 * The file must not already exists.
 *
 * For more details, see g_file_create() which is
 * the synchronous version of this call.
 *
 * When the operation is finished, @callback will be called. You can then call
 * g_file_create_finish() to get the result of the operation.
 **/
void
g_file_create_async (GFile               *file,
		     GFileCreateFlags     flags,
		     int                  io_priority,
		     GCancellable        *cancellable,
		     GAsyncReadyCallback  callback,
		     gpointer             user_data)
{
  GFileIface *iface;
  
  g_return_if_fail (G_IS_FILE (file));

  iface = G_FILE_GET_IFACE (file);
  (* iface->create_async) (file,
			   flags,
			   io_priority,
			   cancellable,
			   callback,
			   user_data);
}

/**
 * g_file_create_finish:
 * @file: input #GFile.
 * @res: a #GAsyncResult. 
 * @error: a #GError, or %NULL
 * 
 * Finishes an asynchronous file create operation started with 
 * g_file_create_async(). 
 * 
 * Returns: a #GFileOutputStream or %NULL on error.
 **/
GFileOutputStream *
g_file_create_finish (GFile         *file,
		      GAsyncResult  *res,
		      GError       **error)
{
  GFileIface *iface;
  
  g_return_val_if_fail (G_IS_FILE (file), NULL);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (res), NULL);

  if (G_IS_SIMPLE_ASYNC_RESULT (res))
    {
      GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (res);
      if (g_simple_async_result_propagate_error (simple, error))
	return NULL;
    }
  
  iface = G_FILE_GET_IFACE (file);
  return (* iface->create_finish) (file, res, error);
}

/**
 * g_file_replace_async:
 * @file: input #GFile.
 * @etag: an <link linkend="gfile-etag">entity tag</link> for the 
 *     current #GFile, or NULL to ignore.
 * @make_backup: %TRUE if a backup should be created.
 * @flags: a set of #GFileCreateFlags.
 * @io_priority: the <link linkend="io-priority">I/O priority</link> 
 *     of the request.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @callback: a #GAsyncReadyCallback to call when the request is satisfied
 * @user_data: the data to pass to callback function
 *
 * Asynchronously overwrites the file, replacing the contents, possibly
 * creating a backup copy of the file first.
 *
 * For more details, see g_file_replace() which is
 * the synchronous version of this call.
 *
 * When the operation is finished, @callback will be called. You can then call
 * g_file_replace_finish() to get the result of the operation.
 **/
void
g_file_replace_async (GFile               *file,
		      const char          *etag,
		      gboolean             make_backup,
		      GFileCreateFlags     flags,
		      int                  io_priority,
		      GCancellable        *cancellable,
		      GAsyncReadyCallback  callback,
		      gpointer             user_data)
{
  GFileIface *iface;
  
  g_return_if_fail (G_IS_FILE (file));

  iface = G_FILE_GET_IFACE (file);
  (* iface->replace_async) (file,
			    etag,
			    make_backup,
			    flags,
			    io_priority,
			    cancellable,
			    callback,
			    user_data);
}

/**
 * g_file_replace_finish:
 * @file: input #GFile.
 * @res: a #GAsyncResult. 
 * @error: a #GError, or %NULL
 * 
 * Finishes an asynchronous file replace operation started with 
 * g_file_replace_async(). 
 * 
 * Returns: a #GFileOutputStream, or %NULL on error.
 **/
GFileOutputStream *
g_file_replace_finish (GFile         *file,
		       GAsyncResult  *res,
		       GError       **error)
{
  GFileIface *iface;
  
  g_return_val_if_fail (G_IS_FILE (file), NULL);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (res), NULL);

  if (G_IS_SIMPLE_ASYNC_RESULT (res))
    {
      GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (res);
      if (g_simple_async_result_propagate_error (simple, error))
	return NULL;
    }
  
  iface = G_FILE_GET_IFACE (file);
  return (* iface->replace_finish) (file, res, error);
}

static gboolean
copy_symlink (GFile           *destination,
	      GFileCopyFlags   flags,
	      GCancellable    *cancellable,
	      const char      *target,
	      GError         **error)
{
  GError *my_error;
  gboolean tried_delete;
  GFileInfo *info;
  GFileType file_type;

  tried_delete = FALSE;

 retry:
  my_error = NULL;
  if (!g_file_make_symbolic_link (destination, target, cancellable, &my_error))
    {
      /* Maybe it already existed, and we want to overwrite? */
      if (!tried_delete && (flags & G_FILE_COPY_OVERWRITE) && 
	  my_error->domain == G_IO_ERROR && my_error->code == G_IO_ERROR_EXISTS)
	{
	  g_error_free (my_error);


	  /* Don't overwrite if the destination is a directory */
	  info = g_file_query_info (destination, G_FILE_ATTRIBUTE_STANDARD_TYPE,
				    G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
				    cancellable, &my_error);
	  if (info != NULL)
	    {
	      file_type = g_file_info_get_file_type (info);
	      g_object_unref (info);
	      
	      if (file_type == G_FILE_TYPE_DIRECTORY)
		{
		  g_set_error (error, G_IO_ERROR, G_IO_ERROR_IS_DIRECTORY,
			       _("Can't copy over directory"));
		  return FALSE;
		}
	    }
	  
	  if (!g_file_delete (destination, cancellable, error))
	    return FALSE;
	  
	  tried_delete = TRUE;
	  goto retry;
	}
            /* Nah, fail */
      g_propagate_error (error, my_error);
      return FALSE;
    }

  return TRUE;
}

static GInputStream *
open_source_for_copy (GFile           *source,
		      GFile           *destination,
		      GFileCopyFlags   flags,
		      GCancellable    *cancellable,
		      GError         **error)
{
  GError *my_error;
  GInputStream *in;
  GFileInfo *info;
  GFileType file_type;
  
  my_error = NULL;
  in = (GInputStream *)g_file_read (source, cancellable, &my_error);
  if (in != NULL)
    return in;

  /* There was an error opening the source, try to set a good error for it: */

  if (my_error->domain == G_IO_ERROR && my_error->code == G_IO_ERROR_IS_DIRECTORY)
    {
      /* The source is a directory, don't fail with WOULD_RECURSE immediately, 
       * as that is less useful to the app. Better check for errors on the 
       * target instead. 
       */
      g_error_free (my_error);
      my_error = NULL;
      
      info = g_file_query_info (destination, G_FILE_ATTRIBUTE_STANDARD_TYPE,
				G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
				cancellable, &my_error);
      if (info != NULL)
	{
	  file_type = g_file_info_get_file_type (info);
	  g_object_unref (info);
	  
	  if (flags & G_FILE_COPY_OVERWRITE)
	    {
	      if (file_type == G_FILE_TYPE_DIRECTORY)
		{
		  g_set_error (error, G_IO_ERROR, G_IO_ERROR_WOULD_MERGE,
			       _("Can't copy directory over directory"));
		  return NULL;
		}
	      /* continue to would_recurse error */
	    }
	  else
	    {
	      g_set_error (error, G_IO_ERROR, G_IO_ERROR_EXISTS,
			   _("Target file exists"));
	      return NULL;
	    }
	}
      else
	{
	  /* Error getting info from target, return that error 
           * (except for NOT_FOUND, which is no error here) 
           */
	  if (my_error->domain != G_IO_ERROR && my_error->code != G_IO_ERROR_NOT_FOUND)
	    {
	      g_propagate_error (error, my_error);
	      return NULL;
	    }
	  g_error_free (my_error);
	}
      
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_WOULD_RECURSE,
		   _("Can't recursively copy directory"));
      return NULL;
    }

  g_propagate_error (error, my_error);
  return NULL;
}

static gboolean
should_copy (GFileAttributeInfo *info, 
             gboolean            as_move)
{
  if (as_move)
    return info->flags & G_FILE_ATTRIBUTE_INFO_COPY_WHEN_MOVED;
  return info->flags & G_FILE_ATTRIBUTE_INFO_COPY_WITH_FILE;
}

static char *
build_attribute_list_for_copy (GFileAttributeInfoList *attributes,
			       GFileAttributeInfoList *namespaces,
			       gboolean                as_move)
{
  GString *s;
  gboolean first;
  int i;
  
  first = TRUE;
  s = g_string_new ("");

  if (attributes)
    {
      for (i = 0; i < attributes->n_infos; i++)
	{
	  if (should_copy (&attributes->infos[i], as_move))
	    {
	      if (first)
		first = FALSE;
	      else
		g_string_append_c (s, ',');
		
	      g_string_append (s, attributes->infos[i].name);
	    }
	}
    }

  if (namespaces)
    {
      for (i = 0; i < namespaces->n_infos; i++)
	{
	  if (should_copy (&namespaces->infos[i], as_move))
	    {
	      if (first)
		first = FALSE;
	      else
		g_string_append_c (s, ',');
		
	      g_string_append (s, namespaces->infos[i].name);
	      g_string_append (s, ":*");
	    }
	}
    }

  return g_string_free (s, FALSE);
}

/**
 * g_file_copy_attributes:
 * @source: a #GFile with attributes.
 * @destination: a #GFile to copy attributes to.
 * @flags: a set of #GFileCopyFlags.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @error: a #GError, %NULL to ignore.
 *
 * Copies the file attributes from @source to @destination. 
 *
 * Normally only a subset of the file attributes are copied,
 * those that are copies in a normal file copy operation
 * (which for instance does not include e.g. mtime). However
 * if #G_FILE_COPY_ALL_METADATA is specified in @flags, then
 * all the metadata that is possible to copy is copied.
 *
 * Returns: %TRUE if the attributes were copied successfully, %FALSE otherwise.
 **/
gboolean
g_file_copy_attributes (GFile           *source,
			GFile           *destination,
			GFileCopyFlags   flags,
			GCancellable    *cancellable,
			GError         **error)
{
  GFileAttributeInfoList *attributes, *namespaces;
  char *attrs_to_read;
  gboolean res;
  GFileInfo *info;
  gboolean as_move;
  gboolean source_nofollow_symlinks;

  as_move = flags & G_FILE_COPY_ALL_METADATA;
  source_nofollow_symlinks = flags & G_FILE_COPY_NOFOLLOW_SYMLINKS;

  /* Ignore errors here, if the target supports no attributes there is nothing to copy */
  attributes = g_file_query_settable_attributes (destination, cancellable, NULL);
  namespaces = g_file_query_writable_namespaces (destination, cancellable, NULL);

  if (attributes == NULL && namespaces == NULL)
    return TRUE;

  attrs_to_read = build_attribute_list_for_copy (attributes, namespaces, as_move);

  /* Ignore errors here, if we can't read some info (e.g. if it doesn't exist)
   * we just don't copy it. 
   */
  info = g_file_query_info (source, attrs_to_read,
			    source_nofollow_symlinks ? G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS:0,
			    cancellable,
			    NULL);

  g_free (attrs_to_read);
  
  res = TRUE;
  if  (info)
    {
      res = g_file_set_attributes_from_info (destination,
					     info, 0,
					     cancellable,
					     error);
      g_object_unref (info);
    }
  
  g_file_attribute_info_list_unref (attributes);
  g_file_attribute_info_list_unref (namespaces);
  
  return res;
}

/* Closes the streams */
static gboolean
copy_stream_with_progress (GInputStream           *in,
			   GOutputStream          *out,
			   GCancellable           *cancellable,
			   GFileProgressCallback   progress_callback,
			   gpointer                progress_callback_data,
			   GError                **error)
{
  gssize n_read, n_written;
  goffset current_size;
  char buffer[8192], *p;
  gboolean res;
  goffset total_size;
  GFileInfo *info;

  total_size = 0;
  info = g_file_input_stream_query_info (G_FILE_INPUT_STREAM (in),
					 G_FILE_ATTRIBUTE_STANDARD_SIZE,
					 cancellable, NULL);
  if (info)
    {
      total_size = g_file_info_get_size (info);
      g_object_unref (info);
    }
  
  current_size = 0;
  res = TRUE;
  while (TRUE)
    {
      n_read = g_input_stream_read (in, buffer, sizeof (buffer), cancellable, error);
      if (n_read == -1)
	{
	  res = FALSE;
	  break;
	}
	
      if (n_read == 0)
	break;

      current_size += n_read;

      p = buffer;
      while (n_read > 0)
	{
	  n_written = g_output_stream_write (out, p, n_read, cancellable, error);
	  if (n_written == -1)
	    {
	      res = FALSE;
	      break;
	    }

	  p += n_written;
	  n_read -= n_written;
	}

      if (!res)
        break;
      
      if (progress_callback)
	progress_callback (current_size, total_size, progress_callback_data);
    }

  if (!res)
    error = NULL; /* Ignore further errors */

  /* Make sure we send full copied size */
  if (progress_callback)
    progress_callback (current_size, total_size, progress_callback_data);

  
  /* Don't care about errors in source here */
  g_input_stream_close (in, cancellable, NULL);

  /* But write errors on close are bad! */
  if (!g_output_stream_close (out, cancellable, error))
    res = FALSE;

  g_object_unref (in);
  g_object_unref (out);
      
  return res;
}

static gboolean
file_copy_fallback (GFile                  *source,
		    GFile                  *destination,
		    GFileCopyFlags          flags,
		    GCancellable           *cancellable,
		    GFileProgressCallback   progress_callback,
		    gpointer                progress_callback_data,
		    GError                **error)
{
  GInputStream *in;
  GOutputStream *out;
  GFileInfo *info;
  const char *target;

  /* Maybe copy the symlink? */
  if (flags & G_FILE_COPY_NOFOLLOW_SYMLINKS)
    {
      info = g_file_query_info (source,
				G_FILE_ATTRIBUTE_STANDARD_TYPE "," G_FILE_ATTRIBUTE_STANDARD_SYMLINK_TARGET,
				G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
				cancellable,
				error);
      if (info == NULL)
	return FALSE;

      if (g_file_info_get_file_type (info) == G_FILE_TYPE_SYMBOLIC_LINK &&
	  (target = g_file_info_get_symlink_target (info)) != NULL)
	{
	  if (!copy_symlink (destination, flags, cancellable, target, error))
	    {
	      g_object_unref (info);
	      return FALSE;
	    }
	  
	  g_object_unref (info);
	  goto copied_file;
	}
      
      g_object_unref (info);
    }
  
  in = open_source_for_copy (source, destination, flags, cancellable, error);
  if (in == NULL)
    return FALSE;
  
  if (flags & G_FILE_COPY_OVERWRITE)
    {
      out = (GOutputStream *)g_file_replace (destination,
					     NULL, 0,
					     flags & G_FILE_COPY_BACKUP,
					     cancellable, error);
    }
  else
    {
      out = (GOutputStream *)g_file_create (destination, 0, cancellable, error);
    }

  if (out == NULL)
    {
      g_object_unref (in);
      return FALSE;
    }

  if (!copy_stream_with_progress (in, out, cancellable,
				  progress_callback, progress_callback_data,
				  error))
    return FALSE;

 copied_file:

  /* Ignore errors here. Failure to copy metadata is not a hard error */
  g_file_copy_attributes (source, destination,
			  flags, cancellable, NULL);
  
  return TRUE;
}

/**
 * g_file_copy:
 * @source: input #GFile.
 * @destination: destination #GFile
 * @flags: set of #GFileCopyFlags
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @progress_callback: function to callback with progress information
 * @progress_callback_data: user data to pass to @progress_callback
 * @error: #GError to set on error, or %NULL
 *
 * Copies the file @source to the location specified by @destination.
 * Can not handle recursive copies of directories.
 *
 * If the flag #G_FILE_COPY_OVERWRITE is specified an already
 * existing @destination file is overwritten.
 *
 * If the flag #G_FILE_COPY_NOFOLLOW_SYMLINKS is specified then symlinks
 * will be copied as symlinks, otherwise the target of the
 * @source symlink will be copied.
 *
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. 
 * 
 * If @progress_callback is not %NULL, then the operation can be monitored by
 * setting this to a #GFileProgressCallback function. @progress_callback_data
 * will be passed to this function. It is guaranteed that this callback will
 * be called after all data has been transferred with the total number of bytes
 * copied during the operation.
 * 
 * If the @source file does not exist then the G_IO_ERROR_NOT_FOUND
 * error is returned, independent on the status of the @destination.
 *
 * If #G_FILE_COPY_OVERWRITE is not specified and the target exists, then the
 * error G_IO_ERROR_EXISTS is returned.
 *
 * If trying to overwrite a file over a directory the G_IO_ERROR_IS_DIRECTORY
 * error is returned. If trying to overwrite a directory with a directory the
 * G_IO_ERROR_WOULD_MERGE error is returned.
 *
 * If the source is a directory and the target does not exist, or #G_FILE_COPY_OVERWRITE is
 * specified and the target is a file, then the G_IO_ERROR_WOULD_RECURSE error
 * is returned.
 *
 * If you are interested in copying the #GFile object itself (not the on-disk
 * file), see g_file_dup().
 *
 * Returns: %TRUE on success, %FALSE otherwise.
 **/
gboolean
g_file_copy (GFile                  *source,
	     GFile                  *destination,
	     GFileCopyFlags          flags,
	     GCancellable           *cancellable,
	     GFileProgressCallback   progress_callback,
	     gpointer                progress_callback_data,
	     GError                **error)
{
  GFileIface *iface;
  GError *my_error;
  gboolean res;

  g_return_val_if_fail (G_IS_FILE (source), FALSE);
  g_return_val_if_fail (G_IS_FILE (destination), FALSE);

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return FALSE;
  
  if (G_OBJECT_TYPE (source) == G_OBJECT_TYPE (destination))
    {
      iface = G_FILE_GET_IFACE (source);

      if (iface->copy)
	{
	  my_error = NULL;
	  res = (* iface->copy) (source, destination, flags, cancellable, progress_callback, progress_callback_data, &my_error);
	  
	  if (res)
	    return TRUE;
	  
	  if (my_error->domain != G_IO_ERROR || my_error->code != G_IO_ERROR_NOT_SUPPORTED)
	    {
	      g_propagate_error (error, my_error);
	      return FALSE;
	    }
	}
    }

  return file_copy_fallback (source, destination, flags, cancellable,
			     progress_callback, progress_callback_data,
			     error);
}


/**
 * g_file_move:
 * @source: #GFile pointing to the source location.
 * @destination: #GFile pointing to the destination location.
 * @flags: set of #GFileCopyFlags.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @progress_callback: #GFileProgressCallback function for updates.
 * @progress_callback_data: gpointer to user data for the callback function.
 * @error: #GError for returning error conditions, or %NULL
 *
 *
 * Tries to move the file or directory @source to the location specified by @destination.
 * If native move operations is supported then this is used, otherwise a copy + delete
 * fallback is used. The native implementation may support moving directories (for instance
 * on moves inside the same filesystem), but the fallback code does not.
 * 
 * If the flag #G_FILE_COPY_OVERWRITE is specified an already
 * existing @destination file is overwritten.
 *
 * If the flag #G_FILE_COPY_NOFOLLOW_SYMLINKS is specified then symlinks
 * will be copied as symlinks, otherwise the target of the
 * @source symlink will be copied.
 *
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. 
 * 
 * If @progress_callback is not %NULL, then the operation can be monitored by
 * setting this to a #GFileProgressCallback function. @progress_callback_data
 * will be passed to this function. It is guaranteed that this callback will
 * be called after all data has been transferred with the total number of bytes
 * copied during the operation.
 * 
 * If the @source file does not exist then the G_IO_ERROR_NOT_FOUND
 * error is returned, independent on the status of the @destination.
 *
 * If #G_FILE_COPY_OVERWRITE is not specified and the target exists, then the
 * error G_IO_ERROR_EXISTS is returned.
 *
 * If trying to overwrite a file over a directory the G_IO_ERROR_IS_DIRECTORY
 * error is returned. If trying to overwrite a directory with a directory the
 * G_IO_ERROR_WOULD_MERGE error is returned.
 *
 * If the source is a directory and the target does not exist, or #G_FILE_COPY_OVERWRITE is
 * specified and the target is a file, then the G_IO_ERROR_WOULD_RECURSE error
 * may be returned (if the native move operation isn't available).
 *
 * Returns: %TRUE on successful move, %FALSE otherwise.
 **/
gboolean
g_file_move (GFile                  *source,
	     GFile                  *destination,
	     GFileCopyFlags          flags,
	     GCancellable           *cancellable,
	     GFileProgressCallback   progress_callback,
	     gpointer                progress_callback_data,
	     GError                **error)
{
  GFileIface *iface;
  GError *my_error;
  gboolean res;

  g_return_val_if_fail (G_IS_FILE (source), FALSE);
  g_return_val_if_fail (G_IS_FILE (destination), FALSE);

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return FALSE;
  
  if (G_OBJECT_TYPE (source) == G_OBJECT_TYPE (destination))
    {
      iface = G_FILE_GET_IFACE (source);

      if (iface->move)
	{
	  my_error = NULL;
	  res = (* iface->move) (source, destination, flags, cancellable, progress_callback, progress_callback_data, &my_error);
	  
	  if (res)
	    return TRUE;
	  
	  if (my_error->domain != G_IO_ERROR || my_error->code != G_IO_ERROR_NOT_SUPPORTED)
	    {
	      g_propagate_error (error, my_error);
	      return FALSE;
	    }
	}
    }

  if (flags & G_FILE_COPY_NO_FALLBACK_FOR_MOVE)
    {  
      g_set_error (error, G_IO_ERROR,
		   G_IO_ERROR_NOT_SUPPORTED,
		   _("Operation not supported"));
      return FALSE;
    }
  
  flags |= G_FILE_COPY_ALL_METADATA;
  if (!g_file_copy (source, destination, flags, cancellable,
		    progress_callback, progress_callback_data,
		    error))
    return FALSE;

  return g_file_delete (source, cancellable, error);
}

/**
 * g_file_make_directory
 * @file: input #GFile.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @error: a #GError, or %NULL 
 *
 * Creates a directory.
 * 
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. 
 * 
 * Returns: %TRUE on successful creation, %FALSE otherwise.
 **/
gboolean
g_file_make_directory (GFile         *file,
		       GCancellable  *cancellable,
		       GError       **error)
{
  GFileIface *iface;

  g_return_val_if_fail (G_IS_FILE (file), FALSE);

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return FALSE;
  
  iface = G_FILE_GET_IFACE (file);

  if (iface->make_directory == NULL)
    {
      g_set_error (error, G_IO_ERROR,
		   G_IO_ERROR_NOT_SUPPORTED,
		   _("Operation not supported"));
      return FALSE;
    }
  
  return (* iface->make_directory) (file, cancellable, error);
}

/**
 * g_file_make_symbolic_link:
 * @file: input #GFile.
 * @symlink_value: a string with the value of the new symlink.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @error: a #GError. 
 * 
 * Creates a symbolic link.
 *
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. 
 * 
 * Returns: %TRUE on the creation of a new symlink, %FALSE otherwise.
 **/
gboolean
g_file_make_symbolic_link (GFile         *file,
			   const char    *symlink_value,
			   GCancellable  *cancellable,
			   GError       **error)
{
  GFileIface *iface;

  g_return_val_if_fail (G_IS_FILE (file), FALSE);
  g_return_val_if_fail (symlink_value != NULL, FALSE);

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return FALSE;

  if (*symlink_value == '\0')
    {
      g_set_error (error, G_IO_ERROR,
                   G_IO_ERROR_INVALID_ARGUMENT,
                   _("Invalid symlink value given"));
      return FALSE;
    }
  
  iface = G_FILE_GET_IFACE (file);

  if (iface->make_symbolic_link == NULL)
    {
      g_set_error (error, G_IO_ERROR,
		   G_IO_ERROR_NOT_SUPPORTED,
		   _("Operation not supported"));
      return FALSE;
    }
  
  return (* iface->make_symbolic_link) (file, symlink_value, cancellable, error);
}

/**
 * g_file_delete:
 * @file: input #GFile.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @error: a #GError, or %NULL 
 * 
 * Deletes a file.
 * 
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. 
 * 
 * Returns: %TRUE if the file was deleted. %FALSE otherwise.
 **/
gboolean
g_file_delete (GFile         *file,
	       GCancellable  *cancellable,
	       GError       **error)
{
  GFileIface *iface;
  
  g_return_val_if_fail (G_IS_FILE (file), FALSE);

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return FALSE;
  
  iface = G_FILE_GET_IFACE (file);

  if (iface->delete_file == NULL)
    {
      g_set_error (error, G_IO_ERROR,
		   G_IO_ERROR_NOT_SUPPORTED,
		   _("Operation not supported"));
      return FALSE;
    }
  
  return (* iface->delete_file) (file, cancellable, error);
}

/**
 * g_file_trash:
 * @file: #GFile to send to trash.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @error: a #GError, or %NULL
 *
 * Sends @file to the "Trashcan", if possible. This is similar to
 * deleting it, but the user can recover it before emptying the trashcan.
 * Not all file systems support trashing, so this call can return the
 * %G_IO_ERROR_NOT_SUPPORTED error.
 *
 *
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. 
 * 
 * Returns: %TRUE on successful trash, %FALSE otherwise.
 **/
gboolean
g_file_trash (GFile         *file,
	      GCancellable  *cancellable,
	      GError       **error)
{
  GFileIface *iface;
  
  g_return_val_if_fail (G_IS_FILE (file), FALSE);

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return FALSE;
  
  iface = G_FILE_GET_IFACE (file);

  if (iface->trash == NULL)
    {
      g_set_error (error,
		   G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
		   _("Trash not supported"));
      return FALSE;
    }
  
  return (* iface->trash) (file, cancellable, error);
}

/**
 * g_file_set_display_name:
 * @file: input #GFile.
 * @display_name: a string.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @error: a #GError, or %NULL
 * 
 * Renames @file to the specified display name.
 *
 * The display name is converted from UTF8 to the correct encoding for the target
 * filesystem if possible and the @file is renamed to this.
 * 
 * If you want to implement a rename operation in the user interface the edit name
 * (#G_FILE_ATTRIBUTE_STANDARD_EDIT_NAME) should be used as the initial value in the rename
 * widget, and then the result after editing should be passed to g_file_set_display_name().
 *
 * On success the resulting converted filename is returned.
 * 
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. 
 * 
 * Returns: a #GFile specifying what @file was renamed to, or %NULL if there was an error.
 **/
GFile *
g_file_set_display_name (GFile         *file,
			 const char    *display_name,
			 GCancellable  *cancellable,
			 GError       **error)
{
  GFileIface *iface;
  
  g_return_val_if_fail (G_IS_FILE (file), NULL);
  g_return_val_if_fail (display_name != NULL, NULL);

  if (strchr (display_name, G_DIR_SEPARATOR) != NULL)
    {
      g_set_error (error,
		   G_IO_ERROR,
		   G_IO_ERROR_INVALID_ARGUMENT,
		   _("File names cannot contain '%c'"), G_DIR_SEPARATOR);
      return NULL;
    }
  
  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return NULL;
  
  iface = G_FILE_GET_IFACE (file);

  return (* iface->set_display_name) (file, display_name, cancellable, error);
}

/**
 * g_file_set_display_name_async:
 * @file: input #GFile.
 * @display_name: a string.
 * @io_priority: the <link linkend="io-priority">I/O priority</link> 
 *     of the request. 
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @callback: a #GAsyncReadyCallback to call when the request is satisfied
 * @user_data: the data to pass to callback function
 * 
 * Asynchronously sets the display name for a given #GFile.
 * 
 * For more details, see g_set_display_name() which is
 * the synchronous version of this call.
 *
 * When the operation is finished, @callback will be called. You can then call
 * g_file_set_display_name_finish() to get the result of the operation.
 **/
void
g_file_set_display_name_async (GFile               *file,
			       const char          *display_name,
			       int                  io_priority,
			       GCancellable        *cancellable,
			       GAsyncReadyCallback  callback,
			       gpointer             user_data)
{
  GFileIface *iface;
  
  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (display_name != NULL);

  iface = G_FILE_GET_IFACE (file);
  (* iface->set_display_name_async) (file,
				     display_name,
				     io_priority,
				     cancellable,
				     callback,
				     user_data);
}

/**
 * g_file_set_display_name_finish:
 * @file: input #GFile.
 * @res: a #GAsyncResult. 
 * @error: a #GError, or %NULL
 * 
 * Finishes setting a display name started with 
 * g_file_set_display_name_async().
 * 
 * Returns: a #GFile or %NULL on error.
 **/
GFile *
g_file_set_display_name_finish (GFile         *file,
				GAsyncResult  *res,
				GError       **error)
{
  GFileIface *iface;
  
  g_return_val_if_fail (G_IS_FILE (file), NULL);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (res), NULL);

  if (G_IS_SIMPLE_ASYNC_RESULT (res))
    {
      GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (res);
      if (g_simple_async_result_propagate_error (simple, error))
	return NULL;
    }
  
  iface = G_FILE_GET_IFACE (file);
  return (* iface->set_display_name_finish) (file, res, error);
}

/**
 * g_file_query_settable_attributes:
 * @file: input #GFile.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @error: a #GError, or %NULL
 * 
 * Obtain the list of settable attributes for the file.
 *
 * Returns the type and full attribute name of all the attributes 
 * that can be set on this file. This doesn't mean setting it will always 
 * succeed though, you might get an access failure, or some specific 
 * file may not support a specific attribute.
 *
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. 
 * 
 * Returns: a #GFileAttributeInfoList describing the settable attributes.
 * When you are done with it, release it with g_file_attribute_info_list_unref()
 **/
GFileAttributeInfoList *
g_file_query_settable_attributes (GFile         *file,
				  GCancellable  *cancellable,
				  GError       **error)
{
  GFileIface *iface;
  GError *my_error;
  GFileAttributeInfoList *list;

  g_return_val_if_fail (G_IS_FILE (file), NULL);

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return NULL;
  
  iface = G_FILE_GET_IFACE (file);

  if (iface->query_settable_attributes == NULL)
    return g_file_attribute_info_list_new ();

  my_error = NULL;
  list = (* iface->query_settable_attributes) (file, cancellable, &my_error);
  
  if (list == NULL)
    {
      if (my_error->domain == G_IO_ERROR && my_error->code == G_IO_ERROR_NOT_SUPPORTED)
	{
	  list = g_file_attribute_info_list_new ();
	  g_error_free (my_error);
	}
      else
	g_propagate_error (error, my_error);
    }
  
  return list;
}

/**
 * g_file_query_writable_namespaces:
 * @file: input #GFile.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @error: a #GError, or %NULL
 * 
 * Obtain the list of attribute namespaces where new attributes 
 * can be created by a user. An example of this is extended
 * attributes (in the "xattr" namespace).
 *
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. 
 * 
 * Returns: a #GFileAttributeInfoList describing the writable namespaces.
 * When you are done with it, release it with g_file_attribute_info_list_unref()
 **/
GFileAttributeInfoList *
g_file_query_writable_namespaces (GFile         *file,
				  GCancellable  *cancellable,
				  GError       **error)
{
  GFileIface *iface;
  GError *my_error;
  GFileAttributeInfoList *list;
  
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return NULL;
  
  iface = G_FILE_GET_IFACE (file);

  if (iface->query_writable_namespaces == NULL)
    return g_file_attribute_info_list_new ();

  my_error = NULL;
  list = (* iface->query_writable_namespaces) (file, cancellable, &my_error);
  
  if (list == NULL)
    {
      if (my_error->domain == G_IO_ERROR && my_error->code == G_IO_ERROR_NOT_SUPPORTED)
	{
	  list = g_file_attribute_info_list_new ();
	  g_error_free (my_error);
	}
      else
	g_propagate_error (error, my_error);
    }
  
  return list;
}

/**
 * g_file_set_attribute:
 * @file: input #GFile.
 * @attribute: a string containing the attribute's name.
 * @type: The type of the attribute
 * @value_p: a pointer to the value (or the pointer itself if the type is a pointer type)
 * @flags: a set of #GFileQueryInfoFlags.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @error: a #GError, or %NULL
 * 
 * Sets an attribute in the file with attribute name @attribute to @value.
 * 
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. 
 * 
 * Returns: %TRUE if the attribute was set, %FALSE otherwise.
 **/
gboolean
g_file_set_attribute (GFile                      *file,
		      const char                 *attribute,
		      GFileAttributeType          type,
		      gpointer                    value_p,
		      GFileQueryInfoFlags         flags,
		      GCancellable               *cancellable,
		      GError                    **error)
{
  GFileIface *iface;
  
  g_return_val_if_fail (G_IS_FILE (file), FALSE);
  g_return_val_if_fail (attribute != NULL && *attribute != '\0', FALSE);

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return FALSE;
  
  iface = G_FILE_GET_IFACE (file);

  if (iface->set_attribute == NULL)
    {
      g_set_error (error, G_IO_ERROR,
		   G_IO_ERROR_NOT_SUPPORTED,
		   _("Operation not supported"));
      return FALSE;
    }

  return (* iface->set_attribute) (file, attribute, type, value_p, flags, cancellable, error);
}

/**
 * g_file_set_attributes_from_info:
 * @file: input #GFile.
 * @info: a #GFileInfo.
 * @flags: #GFileQueryInfoFlags
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @error: a #GError, or %NULL 
 * 
 * Tries to set all attributes in the #GFileInfo on the target values, 
 * not stopping on the first error.
 * 
 * If there is any error during this operation then @error will be set to
 * the first error. Error on particular fields are flagged by setting 
 * the "status" field in the attribute value to 
 * %G_FILE_ATTRIBUTE_STATUS_ERROR_SETTING, which means you can also detect
 * further errors.
 *
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. 
 * 
 * Returns: %TRUE if there was any error, %FALSE otherwise.
 **/
gboolean
g_file_set_attributes_from_info (GFile                *file,
				 GFileInfo            *info,
				 GFileQueryInfoFlags   flags,
				 GCancellable         *cancellable,
				 GError              **error)
{
  GFileIface *iface;

  g_return_val_if_fail (G_IS_FILE (file), FALSE);
  g_return_val_if_fail (G_IS_FILE_INFO (info), FALSE);

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return FALSE;
  
  g_file_info_clear_status (info);
  
  iface = G_FILE_GET_IFACE (file);

  return (* iface->set_attributes_from_info) (file, 
                                              info, 
                                              flags, 
                                              cancellable, 
                                              error);
}


static gboolean
g_file_real_set_attributes_from_info (GFile                *file,
				      GFileInfo            *info,
				      GFileQueryInfoFlags   flags,
				      GCancellable         *cancellable,
				      GError              **error)
{
  char **attributes;
  int i;
  gboolean res;
  GFileAttributeValue *value;
  
  res = TRUE;
  
  attributes = g_file_info_list_attributes (info, NULL);

  for (i = 0; attributes[i] != NULL; i++)
    {
      value = _g_file_info_get_attribute_value (info, attributes[i]);

      if (value->status != G_FILE_ATTRIBUTE_STATUS_UNSET)
	continue;

      if (!g_file_set_attribute (file, attributes[i],
				 value->type, _g_file_attribute_value_peek_as_pointer (value),
				 flags, cancellable, error))
	{
	  value->status = G_FILE_ATTRIBUTE_STATUS_ERROR_SETTING;
	  res = FALSE;
	  /* Don't set error multiple times */
	  error = NULL;
	}
      else
	value->status = G_FILE_ATTRIBUTE_STATUS_SET;
    }
  
  g_strfreev (attributes);
  
  return res;
}

/**
 * g_file_set_attributes_async:
 * @file: input #GFile.
 * @info: a #GFileInfo.
 * @flags: a #GFileQueryInfoFlags.
 * @io_priority: the <link linkend="io-priority">I/O priority</link> 
 *     of the request. 
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @callback: a #GAsyncReadyCallback. 
 * @user_data: a #gpointer.
 *
 * Asynchronously sets the attributes of @file with @info.
 * 
 * For more details, see g_file_set_attributes_from_info() which is
 * the synchronous version of this call.
 *
 * When the operation is finished, @callback will be called. You can then call
 * g_file_set_attributes_finish() to get the result of the operation.
 **/
void
g_file_set_attributes_async (GFile               *file,
			     GFileInfo           *info,
			     GFileQueryInfoFlags  flags,
			     int                  io_priority,
			     GCancellable        *cancellable,
			     GAsyncReadyCallback  callback,
			     gpointer             user_data)
{
  GFileIface *iface;
  
  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (G_IS_FILE_INFO (info));

  iface = G_FILE_GET_IFACE (file);
  (* iface->set_attributes_async) (file, 
                                   info, 
                                   flags, 
                                   io_priority, 
                                   cancellable, 
                                   callback, 
                                   user_data);
}

/**
 * g_file_set_attributes_finish:
 * @file: input #GFile.
 * @result: a #GAsyncResult.
 * @info: a #GFileInfo.
 * @error: a #GError, or %NULL
 * 
 * Finishes setting an attribute started in g_file_set_attributes_async().
 * 
 * Returns: %TRUE if the attributes were set correctly, %FALSE otherwise.
 **/
gboolean
g_file_set_attributes_finish (GFile         *file,
			      GAsyncResult  *result,
			      GFileInfo    **info,
			      GError       **error)
{
  GFileIface *iface;
  
  g_return_val_if_fail (G_IS_FILE (file), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  /* No standard handling of errors here, as we must set info even
   * on errors 
   */
  iface = G_FILE_GET_IFACE (file);
  return (* iface->set_attributes_finish) (file, result, info, error);
}

/**
 * g_file_set_attribute_string:
 * @file: input #GFile.
 * @attribute: a string containing the attribute's name.
 * @value: a string containing the attribute's value.
 * @flags: #GFileQueryInfoFlags.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @error: a #GError, or %NULL
 * 
 * Sets @attribute of type %G_FILE_ATTRIBUTE_TYPE_STRING to @value. 
 * If @attribute is of a different type, this operation will fail.
 * 
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. 
 * 
 * Returns: %TRUE if the @attribute was successfully set, %FALSE otherwise.
 **/
gboolean
g_file_set_attribute_string (GFile                *file,
			     const char           *attribute,
			     const char           *value,
			     GFileQueryInfoFlags   flags,
			     GCancellable         *cancellable,
			     GError              **error)
{
  return g_file_set_attribute (file, attribute,
			       G_FILE_ATTRIBUTE_TYPE_STRING, (gpointer)value,
			       flags, cancellable, error);
}

/**
 * g_file_set_attribute_byte_string:
 * @file: input #GFile.
 * @attribute: a string containing the attribute's name.
 * @value: a string containing the attribute's new value.
 * @flags: a #GFileQueryInfoFlags.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @error: a #GError, or %NULL
 * 
 * Sets @attribute of type %G_FILE_ATTRIBUTE_TYPE_BYTE_STRING to @value. 
 * If @attribute is of a different type, this operation will fail, 
 * returning %FALSE. 
 * 
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. 
 * 
 * Returns: %TRUE if the @attribute was successfully set to @value 
 * in the @file, %FALSE otherwise.
 **/
gboolean
g_file_set_attribute_byte_string  (GFile                *file,
				   const char           *attribute,
				   const char           *value,
				   GFileQueryInfoFlags   flags,
				   GCancellable         *cancellable,
				   GError              **error)
{
  return g_file_set_attribute (file, attribute,
			       G_FILE_ATTRIBUTE_TYPE_BYTE_STRING, (gpointer)value,
			       flags, cancellable, error);
}

/**
 * g_file_set_attribute_uint32:
 * @file: input #GFile.
 * @attribute: a string containing the attribute's name.
 * @value: a #guint32 containing the attribute's new value.
 * @flags: a #GFileQueryInfoFlags.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @error: a #GError, or %NULL
 * 
 * Sets @attribute of type %G_FILE_ATTRIBUTE_TYPE_UINT32 to @value. 
 * If @attribute is of a different type, this operation will fail.
 * 
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. 
 * 
 * Returns: %TRUE if the @attribute was successfully set to @value 
 * in the @file, %FALSE otherwise.
 **/
gboolean
g_file_set_attribute_uint32 (GFile                *file,
			     const char           *attribute,
			     guint32               value,
			     GFileQueryInfoFlags   flags,
			     GCancellable         *cancellable,
			     GError              **error)
{
  return g_file_set_attribute (file, attribute,
			       G_FILE_ATTRIBUTE_TYPE_UINT32, &value,
			       flags, cancellable, error);
}

/**
 * g_file_set_attribute_int32:
 * @file: input #GFile.
 * @attribute: a string containing the attribute's name.
 * @value: a #gint32 containing the attribute's new value.
 * @flags: a #GFileQueryInfoFlags.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @error: a #GError, or %NULL
 * 
 * Sets @attribute of type %G_FILE_ATTRIBUTE_TYPE_INT32 to @value. 
 * If @attribute is of a different type, this operation will fail.
 * 
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. 
 * 
 * Returns: %TRUE if the @attribute was successfully set to @value 
 * in the @file, %FALSE otherwise. 
 **/
gboolean
g_file_set_attribute_int32 (GFile                *file,
			    const char           *attribute,
			    gint32                value,
			    GFileQueryInfoFlags   flags,
			    GCancellable         *cancellable,
			    GError              **error)
{
  return g_file_set_attribute (file, attribute,
			       G_FILE_ATTRIBUTE_TYPE_INT32, &value,
			       flags, cancellable, error);
}

/**
 * g_file_set_attribute_uint64:
 * @file: input #GFile. 
 * @attribute: a string containing the attribute's name.
 * @value: a #guint64 containing the attribute's new value.
 * @flags: a #GFileQueryInfoFlags.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @error: a #GError, or %NULL
 * 
 * Sets @attribute of type %G_FILE_ATTRIBUTE_TYPE_UINT64 to @value. 
 * If @attribute is of a different type, this operation will fail.
 * 
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. 
 * 
 * Returns: %TRUE if the @attribute was successfully set to @value 
 * in the @file, %FALSE otherwise.
 **/
gboolean
g_file_set_attribute_uint64 (GFile                *file,
			     const char           *attribute,
			     guint64               value,
			     GFileQueryInfoFlags   flags,
			     GCancellable         *cancellable,
			     GError              **error)
 {
  return g_file_set_attribute (file, attribute,
			       G_FILE_ATTRIBUTE_TYPE_UINT64, &value,
			       flags, cancellable, error);
}

/**
 * g_file_set_attribute_int64:
 * @file: input #GFile.
 * @attribute: a string containing the attribute's name.
 * @value: a #guint64 containing the attribute's new value.
 * @flags: a #GFileQueryInfoFlags.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @error: a #GError, or %NULL
 * 
 * Sets @attribute of type %G_FILE_ATTRIBUTE_TYPE_INT64 to @value. 
 * If @attribute is of a different type, this operation will fail.
 * 
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. 
 * 
 * Returns: %TRUE if the @attribute was successfully set, %FALSE otherwise.
 **/
gboolean
g_file_set_attribute_int64 (GFile                *file,
			    const char           *attribute,
			    gint64                value,
			    GFileQueryInfoFlags   flags,
			    GCancellable         *cancellable,
			    GError              **error)
{
  return g_file_set_attribute (file, attribute,
			       G_FILE_ATTRIBUTE_TYPE_INT64, &value,
			       flags, cancellable, error);
}

/**
 * g_file_mount_mountable:
 * @file: input #GFile.
 * @mount_operation: a #GMountOperation, or %NULL to avoid user interaction.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @callback: a #GAsyncReadyCallback to call when the request is satisfied
 * @user_data: the data to pass to callback function
 * 
 * Mounts a file of type G_FILE_TYPE_MOUNTABLE.
 * Using @mount_operation, you can request callbacks when, for instance, 
 * passwords are needed during authentication.
 *
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned.
 *
 * When the operation is finished, @callback will be called. You can then call
 * g_file_mount_mountable_finish() to get the result of the operation.
 **/
void
g_file_mount_mountable (GFile               *file,
			GMountOperation     *mount_operation,
			GCancellable        *cancellable,
			GAsyncReadyCallback  callback,
			gpointer             user_data)
{
  GFileIface *iface;

  g_return_if_fail (G_IS_FILE (file));

  iface = G_FILE_GET_IFACE (file);

  if (iface->mount_mountable == NULL)
    g_simple_async_report_error_in_idle (G_OBJECT (file),
				         callback,
					 user_data,
					 G_IO_ERROR,
					 G_IO_ERROR_NOT_SUPPORTED,
					 _("Operation not supported"));
  
  (* iface->mount_mountable) (file,
			      mount_operation,
			      cancellable,
			      callback,
			      user_data);
}

/**
 * g_file_mount_mountable_finish:
 * @file: input #GFile.
 * @result: a #GAsyncResult.
 * @error: a #GError, or %NULL
 *
 * Finishes a mount operation. See g_file_mount_mountable() for details.
 * 
 * Finish an asynchronous mount operation that was started 
 * with g_file_mount_mountable().
 *
 * Returns: a #GFile or %NULL on error.
 **/
GFile *
g_file_mount_mountable_finish (GFile         *file,
			       GAsyncResult  *result,
			       GError       **error)
{
  GFileIface *iface;

  g_return_val_if_fail (G_IS_FILE (file), NULL);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), NULL);

  if (G_IS_SIMPLE_ASYNC_RESULT (result))
    {
      GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (result);
      if (g_simple_async_result_propagate_error (simple, error))
	return NULL;
    }
  
  iface = G_FILE_GET_IFACE (file);
  return (* iface->mount_mountable_finish) (file, result, error);
}

/**
 * g_file_unmount_mountable:
 * @file: input #GFile.
 * @flags: flags affecting the operation
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @callback: a #GAsyncReadyCallback to call when the request is satisfied
 * @user_data: the data to pass to callback function
 *
 * Unmounts a file of type G_FILE_TYPE_MOUNTABLE.
 *
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned.
 *
 * When the operation is finished, @callback will be called. You can then call
 * g_file_unmount_mountable_finish() to get the result of the operation.
 **/
void
g_file_unmount_mountable (GFile               *file,
			  GMountUnmountFlags   flags,
			  GCancellable        *cancellable,
			  GAsyncReadyCallback  callback,
			  gpointer             user_data)
{
  GFileIface *iface;
  
  g_return_if_fail (G_IS_FILE (file));

  iface = G_FILE_GET_IFACE (file);
  
  if (iface->unmount_mountable == NULL)
    g_simple_async_report_error_in_idle (G_OBJECT (file),
					 callback,
					 user_data,
					 G_IO_ERROR,
					 G_IO_ERROR_NOT_SUPPORTED,
					 _("Operation not supported"));
  
  (* iface->unmount_mountable) (file,
				flags,
				cancellable,
				callback,
				user_data);
}

/**
 * g_file_unmount_mountable_finish:
 * @file: input #GFile.
 * @result: a #GAsyncResult.
 * @error: a #GError, or %NULL
 *
 * Finishes an unmount operation, see g_file_unmount_mountable() for details.
 * 
 * Finish an asynchronous unmount operation that was started 
 * with g_file_unmount_mountable().
 *
 * Returns: %TRUE if the operation finished successfully. %FALSE
 * otherwise.
 **/
gboolean
g_file_unmount_mountable_finish (GFile         *file,
				 GAsyncResult  *result,
				 GError       **error)
{
  GFileIface *iface;
  
  g_return_val_if_fail (G_IS_FILE (file), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  if (G_IS_SIMPLE_ASYNC_RESULT (result))
    {
      GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (result);
      if (g_simple_async_result_propagate_error (simple, error))
	return FALSE;
    }
  
  iface = G_FILE_GET_IFACE (file);
  return (* iface->unmount_mountable_finish) (file, result, error);
}

/**
 * g_file_eject_mountable:
 * @file: input #GFile.
 * @flags: flags affecting the operation
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @callback: a #GAsyncReadyCallback to call when the request is satisfied
 * @user_data: the data to pass to callback function
 * 
 * Starts an asynchronous eject on a mountable.  
 * When this operation has completed, @callback will be called with
 * @user_user data, and the operation can be finalized with 
 * g_file_eject_mountable_finish().
 * 
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. 
 **/
void
g_file_eject_mountable (GFile               *file,
			GMountUnmountFlags   flags,
			GCancellable        *cancellable,
			GAsyncReadyCallback  callback,
			gpointer             user_data)
{
  GFileIface *iface;

  g_return_if_fail (G_IS_FILE (file));

  iface = G_FILE_GET_IFACE (file);
  
  if (iface->eject_mountable == NULL)
    g_simple_async_report_error_in_idle (G_OBJECT (file),
					 callback,
					 user_data,
					 G_IO_ERROR,
					 G_IO_ERROR_NOT_SUPPORTED,
					 _("Operation not supported"));
  
  (* iface->eject_mountable) (file,
			      flags,
			      cancellable,
			      callback,
			      user_data);
}

/**
 * g_file_eject_mountable_finish:
 * @file: input #GFile.
 * @result: a #GAsyncResult.
 * @error: a #GError, or %NULL
 * 
 * Finishes an asynchronous eject operation started by 
 * g_file_eject_mountable().
 * 
 * Returns: %TRUE if the @file was ejected successfully. %FALSE 
 * otherwise.
 **/
gboolean
g_file_eject_mountable_finish (GFile         *file,
			       GAsyncResult  *result,
			       GError       **error)
{
  GFileIface *iface;
  
  g_return_val_if_fail (G_IS_FILE (file), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  if (G_IS_SIMPLE_ASYNC_RESULT (result))
    {
      GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (result);
      if (g_simple_async_result_propagate_error (simple, error))
	return FALSE;
    }
  
  iface = G_FILE_GET_IFACE (file);
  return (* iface->eject_mountable_finish) (file, result, error);
}

/**
 * g_file_monitor_directory:
 * @file: input #GFile.
 * @flags: a set of #GFileMonitorFlags.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * 
 * Obtains a directory monitor for the given file.
 * This may fail if directory monitoring is not supported.
 *
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. 
 * 
 * Returns: a #GFileMonitor for the given @file, 
 * or %NULL on error.
 **/
GFileMonitor*
g_file_monitor_directory (GFile             *file,
			  GFileMonitorFlags  flags,
			  GCancellable      *cancellable)
{
  GFileIface *iface;

  g_return_val_if_fail (G_IS_FILE (file), NULL);

  iface = G_FILE_GET_IFACE (file);

  if (iface->monitor_dir == NULL)
    return NULL;

  return (* iface->monitor_dir) (file, flags, cancellable);
}

/**
 * g_file_monitor_file:
 * @file: input #GFile.
 * @flags: a set of #GFileMonitorFlags.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * 
 * Obtains a file monitor for the given file. If no file notification
 * mechanism exists, then regular polling of the file is used.
 *
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. 
 * 
 * Returns: a #GFileMonitor for the given @file.
 **/
GFileMonitor*
g_file_monitor_file (GFile             *file,
		     GFileMonitorFlags  flags,
		     GCancellable      *cancellable)
{
  GFileIface *iface;
  GFileMonitor *monitor;
  
  g_return_val_if_fail (G_IS_FILE (file), NULL);

  iface = G_FILE_GET_IFACE (file);

  monitor = NULL;
  
  if (iface->monitor_file)
    monitor = (* iface->monitor_file) (file, flags, cancellable);

/* Fallback to polling */
  if (monitor == NULL)
    monitor = _g_poll_file_monitor_new (file);

  return monitor;
}

/********************************************
 *   Default implementation of async ops    *
 ********************************************/

typedef struct {
  char *attributes;
  GFileQueryInfoFlags flags;
  GFileInfo *info;
} QueryInfoAsyncData;

static void
query_info_data_free (QueryInfoAsyncData *data)
{
  if (data->info)
    g_object_unref (data->info);
  g_free (data->attributes);
  g_free (data);
}

static void
query_info_async_thread (GSimpleAsyncResult *res,
			 GObject            *object,
			 GCancellable       *cancellable)
{
  GError *error = NULL;
  QueryInfoAsyncData *data;
  GFileInfo *info;
  
  data = g_simple_async_result_get_op_res_gpointer (res);
  
  info = g_file_query_info (G_FILE (object), data->attributes, data->flags, cancellable, &error);

  if (info == NULL)
    {
      g_simple_async_result_set_from_error (res, error);
      g_error_free (error);
    }
  else
    data->info = info;
}

static void
g_file_real_query_info_async (GFile               *file,
			      const char          *attributes,
			      GFileQueryInfoFlags  flags,
			      int                  io_priority,
			      GCancellable        *cancellable,
			      GAsyncReadyCallback  callback,
			      gpointer             user_data)
{
  GSimpleAsyncResult *res;
  QueryInfoAsyncData *data;

  data = g_new0 (QueryInfoAsyncData, 1);
  data->attributes = g_strdup (attributes);
  data->flags = flags;
  
  res = g_simple_async_result_new (G_OBJECT (file), callback, user_data, g_file_real_query_info_async);
  g_simple_async_result_set_op_res_gpointer (res, data, (GDestroyNotify)query_info_data_free);
  
  g_simple_async_result_run_in_thread (res, query_info_async_thread, io_priority, cancellable);
  g_object_unref (res);
}

static GFileInfo *
g_file_real_query_info_finish (GFile         *file,
			       GAsyncResult  *res,
			       GError       **error)
{
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (res);
  QueryInfoAsyncData *data;

  g_warn_if_fail (g_simple_async_result_get_source_tag (simple) == g_file_real_query_info_async);

  data = g_simple_async_result_get_op_res_gpointer (simple);
  if (data->info)
    return g_object_ref (data->info);
  
  return NULL;
}

typedef struct {
  char *attributes;
  GFileQueryInfoFlags flags;
  GFileEnumerator *enumerator;
} EnumerateChildrenAsyncData;

static void
enumerate_children_data_free (EnumerateChildrenAsyncData *data)
{
  if (data->enumerator)
    g_object_unref (data->enumerator);
  g_free (data->attributes);
  g_free (data);
}

static void
enumerate_children_async_thread (GSimpleAsyncResult *res,
				 GObject            *object,
				 GCancellable       *cancellable)
{
  GError *error = NULL;
  EnumerateChildrenAsyncData *data;
  GFileEnumerator *enumerator;
  
  data = g_simple_async_result_get_op_res_gpointer (res);
  
  enumerator = g_file_enumerate_children (G_FILE (object), data->attributes, data->flags, cancellable, &error);

  if (enumerator == NULL)
    {
      g_simple_async_result_set_from_error (res, error);
      g_error_free (error);
    }
  else
    data->enumerator = enumerator;
}

static void
g_file_real_enumerate_children_async (GFile               *file,
				      const char          *attributes,
				      GFileQueryInfoFlags  flags,
				      int                  io_priority,
				      GCancellable        *cancellable,
				      GAsyncReadyCallback  callback,
				      gpointer             user_data)
{
  GSimpleAsyncResult *res;
  EnumerateChildrenAsyncData *data;

  data = g_new0 (EnumerateChildrenAsyncData, 1);
  data->attributes = g_strdup (attributes);
  data->flags = flags;
  
  res = g_simple_async_result_new (G_OBJECT (file), callback, user_data, g_file_real_enumerate_children_async);
  g_simple_async_result_set_op_res_gpointer (res, data, (GDestroyNotify)enumerate_children_data_free);
  
  g_simple_async_result_run_in_thread (res, enumerate_children_async_thread, io_priority, cancellable);
  g_object_unref (res);
}

static GFileEnumerator *
g_file_real_enumerate_children_finish (GFile         *file,
				       GAsyncResult  *res,
				       GError       **error)
{
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (res);
  EnumerateChildrenAsyncData *data;

  g_warn_if_fail (g_simple_async_result_get_source_tag (simple) == g_file_real_enumerate_children_async);

  data = g_simple_async_result_get_op_res_gpointer (simple);
  if (data->enumerator)
    return g_object_ref (data->enumerator);
  
  return NULL;
}

static void
open_read_async_thread (GSimpleAsyncResult *res,
			GObject            *object,
			GCancellable       *cancellable)
{
  GFileIface *iface;
  GFileInputStream *stream;
  GError *error = NULL;

  iface = G_FILE_GET_IFACE (object);

  stream = iface->read_fn (G_FILE (object), cancellable, &error);

  if (stream == NULL)
    {
      g_simple_async_result_set_from_error (res, error);
      g_error_free (error);
    }
  else
    g_simple_async_result_set_op_res_gpointer (res, stream, g_object_unref);
}

static void
g_file_real_read_async (GFile               *file,
			int                  io_priority,
			GCancellable        *cancellable,
			GAsyncReadyCallback  callback,
			gpointer             user_data)
{
  GSimpleAsyncResult *res;
  
  res = g_simple_async_result_new (G_OBJECT (file), callback, user_data, g_file_real_read_async);
  
  g_simple_async_result_run_in_thread (res, open_read_async_thread, io_priority, cancellable);
  g_object_unref (res);
}

static GFileInputStream *
g_file_real_read_finish (GFile         *file,
			 GAsyncResult  *res,
			 GError       **error)
{
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (res);
  gpointer op;

  g_warn_if_fail (g_simple_async_result_get_source_tag (simple) == g_file_real_read_async);

  op = g_simple_async_result_get_op_res_gpointer (simple);
  if (op)
    return g_object_ref (op);
  
  return NULL;
}

static void
append_to_async_thread (GSimpleAsyncResult *res,
			GObject            *object,
			GCancellable       *cancellable)
{
  GFileIface *iface;
  GFileCreateFlags *data;
  GFileOutputStream *stream;
  GError *error = NULL;

  iface = G_FILE_GET_IFACE (object);

  data = g_simple_async_result_get_op_res_gpointer (res);

  stream = iface->append_to (G_FILE (object), *data, cancellable, &error);

  if (stream == NULL)
    {
      g_simple_async_result_set_from_error (res, error);
      g_error_free (error);
    }
  else
    g_simple_async_result_set_op_res_gpointer (res, stream, g_object_unref);
}

static void
g_file_real_append_to_async (GFile               *file,
			     GFileCreateFlags     flags,
			     int                  io_priority,
			     GCancellable        *cancellable,
			     GAsyncReadyCallback  callback,
			     gpointer             user_data)
{
  GFileCreateFlags *data;
  GSimpleAsyncResult *res;

  data = g_new0 (GFileCreateFlags, 1);
  *data = flags;

  res = g_simple_async_result_new (G_OBJECT (file), callback, user_data, g_file_real_append_to_async);
  g_simple_async_result_set_op_res_gpointer (res, data, (GDestroyNotify)g_free);

  g_simple_async_result_run_in_thread (res, append_to_async_thread, io_priority, cancellable);
  g_object_unref (res);
}

static GFileOutputStream *
g_file_real_append_to_finish (GFile         *file,
			      GAsyncResult  *res,
			      GError       **error)
{
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (res);
  gpointer op;

  g_warn_if_fail (g_simple_async_result_get_source_tag (simple) == g_file_real_append_to_async);

  op = g_simple_async_result_get_op_res_gpointer (simple);
  if (op)
    return g_object_ref (op);
  
  return NULL;
}

static void
create_async_thread (GSimpleAsyncResult *res,
		     GObject            *object,
		     GCancellable       *cancellable)
{
  GFileIface *iface;
  GFileCreateFlags *data;
  GFileOutputStream *stream;
  GError *error = NULL;

  iface = G_FILE_GET_IFACE (object);

  data = g_simple_async_result_get_op_res_gpointer (res);

  stream = iface->create (G_FILE (object), *data, cancellable, &error);

  if (stream == NULL)
    {
      g_simple_async_result_set_from_error (res, error);
      g_error_free (error);
    }
  else
    g_simple_async_result_set_op_res_gpointer (res, stream, g_object_unref);
}

static void
g_file_real_create_async (GFile               *file,
			  GFileCreateFlags     flags,
			  int                  io_priority,
			  GCancellable        *cancellable,
			  GAsyncReadyCallback  callback,
			  gpointer             user_data)
{
  GFileCreateFlags *data;
  GSimpleAsyncResult *res;

  data = g_new0 (GFileCreateFlags, 1);
  *data = flags;

  res = g_simple_async_result_new (G_OBJECT (file), callback, user_data, g_file_real_create_async);
  g_simple_async_result_set_op_res_gpointer (res, data, (GDestroyNotify)g_free);

  g_simple_async_result_run_in_thread (res, create_async_thread, io_priority, cancellable);
  g_object_unref (res);
}

static GFileOutputStream *
g_file_real_create_finish (GFile         *file,
			   GAsyncResult  *res,
			   GError       **error)
{
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (res);
  gpointer op;

  g_warn_if_fail (g_simple_async_result_get_source_tag (simple) == g_file_real_create_async);

  op = g_simple_async_result_get_op_res_gpointer (simple);
  if (op)
    return g_object_ref (op);
  
  return NULL;
}

typedef struct {
  GFileOutputStream *stream;
  char *etag;
  gboolean make_backup;
  GFileCreateFlags flags;
} ReplaceAsyncData;

static void
replace_async_data_free (ReplaceAsyncData *data)
{
  if (data->stream)
    g_object_unref (data->stream);
  g_free (data->etag);
  g_free (data);
}

static void
replace_async_thread (GSimpleAsyncResult *res,
		      GObject            *object,
		      GCancellable       *cancellable)
{
  GFileIface *iface;
  GFileOutputStream *stream;
  GError *error = NULL;
  ReplaceAsyncData *data;

  iface = G_FILE_GET_IFACE (object);
  
  data = g_simple_async_result_get_op_res_gpointer (res);

  stream = iface->replace (G_FILE (object),
			   data->etag,
			   data->make_backup,
			   data->flags,
			   cancellable,
			   &error);

  if (stream == NULL)
    {
      g_simple_async_result_set_from_error (res, error);
      g_error_free (error);
    }
  else
    data->stream = stream;
}

static void
g_file_real_replace_async (GFile               *file,
			   const char          *etag,
			   gboolean             make_backup,
			   GFileCreateFlags     flags,
			   int                  io_priority,
			   GCancellable        *cancellable,
			   GAsyncReadyCallback  callback,
			   gpointer             user_data)
{
  GSimpleAsyncResult *res;
  ReplaceAsyncData *data;

  data = g_new0 (ReplaceAsyncData, 1);
  data->etag = g_strdup (etag);
  data->make_backup = make_backup;
  data->flags = flags;

  res = g_simple_async_result_new (G_OBJECT (file), callback, user_data, g_file_real_replace_async);
  g_simple_async_result_set_op_res_gpointer (res, data, (GDestroyNotify)replace_async_data_free);

  g_simple_async_result_run_in_thread (res, replace_async_thread, io_priority, cancellable);
  g_object_unref (res);
}

static GFileOutputStream *
g_file_real_replace_finish (GFile         *file,
			    GAsyncResult  *res,
			    GError       **error)
{
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (res);
  ReplaceAsyncData *data;

  g_warn_if_fail (g_simple_async_result_get_source_tag (simple) == g_file_real_replace_async);

  data = g_simple_async_result_get_op_res_gpointer (simple);
  if (data->stream)
    return g_object_ref (data->stream);
  
  return NULL;
}

typedef struct {
  char *name;
  GFile *file;
} SetDisplayNameAsyncData;

static void
set_display_name_data_free (SetDisplayNameAsyncData *data)
{
  g_free (data->name);
  if (data->file)
    g_object_unref (data->file);
  g_free (data);
}

static void
set_display_name_async_thread (GSimpleAsyncResult *res,
			       GObject            *object,
			       GCancellable       *cancellable)
{
  GError *error = NULL;
  SetDisplayNameAsyncData *data;
  GFile *file;
  
  data = g_simple_async_result_get_op_res_gpointer (res);
  
  file = g_file_set_display_name (G_FILE (object), data->name, cancellable, &error);

  if (file == NULL)
    {
      g_simple_async_result_set_from_error (res, error);
      g_error_free (error);
    }
  else
    data->file = file;
}

static void
g_file_real_set_display_name_async (GFile               *file,	
				    const char          *display_name,
				    int                  io_priority,
				    GCancellable        *cancellable,
				    GAsyncReadyCallback  callback,
				    gpointer             user_data)
{
  GSimpleAsyncResult *res;
  SetDisplayNameAsyncData *data;

  data = g_new0 (SetDisplayNameAsyncData, 1);
  data->name = g_strdup (display_name);
  
  res = g_simple_async_result_new (G_OBJECT (file), callback, user_data, g_file_real_set_display_name_async);
  g_simple_async_result_set_op_res_gpointer (res, data, (GDestroyNotify)set_display_name_data_free);
  
  g_simple_async_result_run_in_thread (res, set_display_name_async_thread, io_priority, cancellable);
  g_object_unref (res);
}

static GFile *
g_file_real_set_display_name_finish (GFile         *file,
				     GAsyncResult  *res,
				     GError       **error)
{
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (res);
  SetDisplayNameAsyncData *data;

  g_warn_if_fail (g_simple_async_result_get_source_tag (simple) == g_file_real_set_display_name_async);

  data = g_simple_async_result_get_op_res_gpointer (simple);
  if (data->file)
    return g_object_ref (data->file);
  
  return NULL;
}

typedef struct {
  GFileQueryInfoFlags flags;
  GFileInfo *info;
  gboolean res;
  GError *error;
} SetInfoAsyncData;

static void
set_info_data_free (SetInfoAsyncData *data)
{
  if (data->info)
    g_object_unref (data->info);
  if (data->error)
    g_error_free (data->error);
  g_free (data);
}

static void
set_info_async_thread (GSimpleAsyncResult *res,
		       GObject            *object,
		       GCancellable       *cancellable)
{
  SetInfoAsyncData *data;
  
  data = g_simple_async_result_get_op_res_gpointer (res);
  
  data->error = NULL;
  data->res = g_file_set_attributes_from_info (G_FILE (object),
					       data->info,
					       data->flags,
					       cancellable,
					       &data->error);
}

static void
g_file_real_set_attributes_async (GFile               *file,
				  GFileInfo           *info,
				  GFileQueryInfoFlags  flags,
				  int                  io_priority,
				  GCancellable        *cancellable,
				  GAsyncReadyCallback  callback,
				  gpointer             user_data)
{
  GSimpleAsyncResult *res;
  SetInfoAsyncData *data;

  data = g_new0 (SetInfoAsyncData, 1);
  data->info = g_file_info_dup (info);
  data->flags = flags;
  
  res = g_simple_async_result_new (G_OBJECT (file), callback, user_data, g_file_real_set_attributes_async);
  g_simple_async_result_set_op_res_gpointer (res, data, (GDestroyNotify)set_info_data_free);
  
  g_simple_async_result_run_in_thread (res, set_info_async_thread, io_priority, cancellable);
  g_object_unref (res);
}

static gboolean
g_file_real_set_attributes_finish (GFile         *file,
				   GAsyncResult  *res,
				   GFileInfo    **info,
				   GError       **error)
{
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (res);
  SetInfoAsyncData *data;
  
  g_warn_if_fail (g_simple_async_result_get_source_tag (simple) == g_file_real_set_attributes_async);

  data = g_simple_async_result_get_op_res_gpointer (simple);

  if (info) 
    *info = g_object_ref (data->info);

  if (error != NULL && data->error) 
    *error = g_error_copy (data->error);
  
  return data->res;
}

/********************************************
 *   Default VFS operations                 *
 ********************************************/

/**
 * g_file_new_for_path:
 * @path: a string containing a relative or absolute path.
 * 
 * Constructs a #GFile for a given path. This operation never
 * fails, but the returned object might not support any I/O
 * operation if @path is malformed.
 * 
 * Returns: a new #GFile for the given @path. 
 **/
GFile *
g_file_new_for_path (const char *path)
{
  g_return_val_if_fail (path != NULL, NULL);

  return g_vfs_get_file_for_path (g_vfs_get_default (), path);
}
 
/**
 * g_file_new_for_uri:
 * @uri: a string containing a URI.
 * 
 * Constructs a #GFile for a given URI. This operation never 
 * fails, but the returned object might not support any I/O 
 * operation if @uri is malformed or if the uri type is 
 * not supported.
 * 
 * Returns: a #GFile for the given @uri.
 **/ 
GFile *
g_file_new_for_uri (const char *uri)
{
  g_return_val_if_fail (uri != NULL, NULL);

  return g_vfs_get_file_for_uri (g_vfs_get_default (), uri);
}
  
/**
 * g_file_parse_name:
 * @parse_name: a file name or path to be parsed.
 * 
 * Constructs a #GFile with the given @parse_name (i.e. something given by g_file_get_parse_name()).
 * This operation never fails, but the returned object might not support any I/O
 * operation if the @parse_name cannot be parsed.
 * 
 * Returns: a new #GFile.
 **/
GFile *
g_file_parse_name (const char *parse_name)
{
  g_return_val_if_fail (parse_name != NULL, NULL);

  return g_vfs_parse_name (g_vfs_get_default (), parse_name);
}

static gboolean
is_valid_scheme_character (char c)
{
  return g_ascii_isalnum (c) || c == '+' || c == '-' || c == '.';
}

static gboolean
has_valid_scheme (const char *uri)
{
  const char *p;
  
  p = uri;
  
  if (!is_valid_scheme_character (*p))
    return FALSE;

  do {
    p++;
  } while (is_valid_scheme_character (*p));

  return *p == ':';
}

/**
 * g_file_new_for_commandline_arg:
 * @arg: a command line string.
 * 
 * Creates a #GFile with the given argument from the command line. The value of
 * @arg can be either a URI, an absolute path or a relative path resolved
 * relative to the current working directory.
 * This operation never fails, but the returned object might not support any
 * I/O operation if @arg points to a malformed path.
 *
 * Returns: a new #GFile. 
 **/
GFile *
g_file_new_for_commandline_arg (const char *arg)
{
  GFile *file;
  char *filename;
  char *current_dir;
  
  g_return_val_if_fail (arg != NULL, NULL);
  
  if (g_path_is_absolute (arg))
    return g_file_new_for_path (arg);

  if (has_valid_scheme (arg))
    return g_file_new_for_uri (arg);
    
  current_dir = g_get_current_dir ();
  filename = g_build_filename (current_dir, arg, NULL);
  g_free (current_dir);
  
  file = g_file_new_for_path (filename);
  g_free (filename);
  
  return file;
}

/**
 * g_file_mount_enclosing_volume:
 * @location: input #GFile.
 * @mount_operation: a #GMountOperation or %NULL to avoid user interaction.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @callback: a #GAsyncReadyCallback to call when the request is satisfied
 * @user_data: the data to pass to callback function
 * 
 * Starts a @mount_operation, mounting the volume that contains the file @location. 
 * 
 * When this operation has completed, @callback will be called with
 * @user_user data, and the operation can be finalized with 
 * g_file_mount_enclosing_volume_finish().
 * 
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. 
 **/
void
g_file_mount_enclosing_volume (GFile               *location,
			       GMountOperation     *mount_operation,
			       GCancellable        *cancellable,
			       GAsyncReadyCallback  callback,
			       gpointer             user_data)
{
  GFileIface *iface;

  g_return_if_fail (G_IS_FILE (location));

  iface = G_FILE_GET_IFACE (location);

  if (iface->mount_enclosing_volume == NULL)
    {
      g_simple_async_report_error_in_idle (G_OBJECT (location),
					   callback, user_data,
					   G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
					   _("volume doesn't implement mount"));
      
      return;
    }
  
  (* iface->mount_enclosing_volume) (location, mount_operation, cancellable, callback, user_data);

}

/**
 * g_file_mount_enclosing_volume_finish:
 * @location: input #GFile.
 * @result: a #GAsyncResult.
 * @error: a #GError, or %NULL
 * 
 * Finishes a mount operation started by g_file_mount_enclosing_volume().
 * 
 * Returns: %TRUE if successful. If an error
 * has occurred, this function will return %FALSE and set @error
 * appropriately if present.
 **/
gboolean
g_file_mount_enclosing_volume_finish (GFile         *location,
				      GAsyncResult  *result,
				      GError       **error)
{
  GFileIface *iface;

  g_return_val_if_fail (G_IS_FILE (location), FALSE);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

  if (G_IS_SIMPLE_ASYNC_RESULT (result))
    {
      GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (result);
      if (g_simple_async_result_propagate_error (simple, error))
	return FALSE;
    }
  
  iface = G_FILE_GET_IFACE (location);

  return (* iface->mount_enclosing_volume_finish) (location, result, error);
}

/********************************************
 *   Utility functions                      *
 ********************************************/

#define GET_CONTENT_BLOCK_SIZE 8192

/**
 * g_file_load_contents:
 * @file: input #GFile.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @contents: a location to place the contents of the file.
 * @length: a location to place the length of the contents of the file.
 * @etag_out: a location to place the current entity tag for the file.
 * @error: a #GError, or %NULL
 *
 * Loads the content of the file into memory, returning the size of
 * the data. The data is always zero terminated, but this is not
 * included in the resultant @length.
 * 
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. 
 * 
 * Returns: %TRUE if the @file's contents were successfully loaded.
 * %FALSE if there were errors..
 **/
gboolean
g_file_load_contents (GFile         *file,
		      GCancellable  *cancellable,
		      char         **contents,
		      gsize         *length,
		      char         **etag_out,
		      GError       **error)
{
  GFileInputStream *in;
  GByteArray *content;
  gsize pos;
  gssize res;
  GFileInfo *info;

  g_return_val_if_fail (G_IS_FILE (file), FALSE);
  g_return_val_if_fail (contents != NULL, FALSE);

  in = g_file_read (file, cancellable, error);
  if (in == NULL)
    return FALSE;

  content = g_byte_array_new ();
  pos = 0;
  
  g_byte_array_set_size (content, pos + GET_CONTENT_BLOCK_SIZE + 1);
  while ((res = g_input_stream_read (G_INPUT_STREAM (in),
				     content->data + pos,
				     GET_CONTENT_BLOCK_SIZE,
				     cancellable, error)) > 0)
    {
      pos += res;
      g_byte_array_set_size (content, pos + GET_CONTENT_BLOCK_SIZE + 1);
    }

  if (etag_out)
    {
      *etag_out = NULL;
      
      info = g_file_input_stream_query_info (in,
					     G_FILE_ATTRIBUTE_ETAG_VALUE,
					     cancellable,
					     NULL);
      if (info)
	{
	  *etag_out = g_strdup (g_file_info_get_etag (info));
	  g_object_unref (info);
	}
    } 

  /* Ignore errors on close */
  g_input_stream_close (G_INPUT_STREAM (in), cancellable, NULL);
  g_object_unref (in);

  if (res < 0)
    {
      /* error is set already */
      g_byte_array_free (content, TRUE);
      return FALSE;
    }

  if (length)
    *length = pos;

  /* Zero terminate (we got an extra byte allocated for this */
  content->data[pos] = 0;
  
  *contents = (char *)g_byte_array_free (content, FALSE);
  
  return TRUE;
}

typedef struct {
  GFile *file;
  GError *error;
  GCancellable *cancellable;
  GFileReadMoreCallback read_more_callback;
  GAsyncReadyCallback callback;
  gpointer user_data;
  GByteArray *content;
  gsize pos;
  char *etag;
} LoadContentsData;


static void
load_contents_data_free (LoadContentsData *data)
{
  if (data->error)
    g_error_free (data->error);
  if (data->cancellable)
    g_object_unref (data->cancellable);
  if (data->content)
    g_byte_array_free (data->content, TRUE);
  g_free (data->etag);
  g_object_unref (data->file);
  g_free (data);
}

static void
load_contents_close_callback (GObject      *obj,
			      GAsyncResult *close_res,
			      gpointer      user_data)
{
  GInputStream *stream = G_INPUT_STREAM (obj);
  LoadContentsData *data = user_data;
  GSimpleAsyncResult *res;

  /* Ignore errors here, we're only reading anyway */
  g_input_stream_close_finish (stream, close_res, NULL);
  g_object_unref (stream);

  res = g_simple_async_result_new (G_OBJECT (data->file),
				   data->callback,
				   data->user_data,
				   g_file_load_contents_async);
  g_simple_async_result_set_op_res_gpointer (res, data, (GDestroyNotify)load_contents_data_free);
  g_simple_async_result_complete (res);
  g_object_unref (res);
}

static void
load_contents_fstat_callback (GObject      *obj,
			      GAsyncResult *stat_res,
			      gpointer      user_data)
{
  GInputStream *stream = G_INPUT_STREAM (obj);
  LoadContentsData *data = user_data;
  GFileInfo *info;

  info = g_file_input_stream_query_info_finish (G_FILE_INPUT_STREAM (stream),
						   stat_res, NULL);
  if (info)
    {
      data->etag = g_strdup (g_file_info_get_etag (info));
      g_object_unref (info);
    }

  g_input_stream_close_async (stream, 0,
			      data->cancellable,
			      load_contents_close_callback, data);
}

static void
load_contents_read_callback (GObject      *obj,
			     GAsyncResult *read_res,
			     gpointer      user_data)
{
  GInputStream *stream = G_INPUT_STREAM (obj);
  LoadContentsData *data = user_data;
  GError *error = NULL;
  gssize read_size;

  read_size = g_input_stream_read_finish (stream, read_res, &error);

  if (read_size < 0) 
    {
      /* Error or EOF, close the file */
      data->error = error;
      g_input_stream_close_async (stream, 0,
				  data->cancellable,
				  load_contents_close_callback, data);
    }
  else if (read_size == 0)
    {
      g_file_input_stream_query_info_async (G_FILE_INPUT_STREAM (stream),
					    G_FILE_ATTRIBUTE_ETAG_VALUE,
					    0,
					    data->cancellable,
					    load_contents_fstat_callback,
					    data);
    }
  else if (read_size > 0)
    {
      data->pos += read_size;
      
      g_byte_array_set_size (data->content,
			     data->pos + GET_CONTENT_BLOCK_SIZE);


      if (data->read_more_callback &&
	  !data->read_more_callback ((char *)data->content->data, data->pos, data->user_data))
	g_file_input_stream_query_info_async (G_FILE_INPUT_STREAM (stream),
					      G_FILE_ATTRIBUTE_ETAG_VALUE,
					      0,
					      data->cancellable,
					      load_contents_fstat_callback,
					      data);
      else 
	g_input_stream_read_async (stream,
				   data->content->data + data->pos,
				   GET_CONTENT_BLOCK_SIZE,
				   0,
				   data->cancellable,
				   load_contents_read_callback,
				   data);
    }
}

static void
load_contents_open_callback (GObject      *obj,
			     GAsyncResult *open_res,
			     gpointer      user_data)
{
  GFile *file = G_FILE (obj);
  GFileInputStream *stream;
  LoadContentsData *data = user_data;
  GError *error = NULL;
  GSimpleAsyncResult *res;

  stream = g_file_read_finish (file, open_res, &error);

  if (stream)
    {
      g_byte_array_set_size (data->content,
			     data->pos + GET_CONTENT_BLOCK_SIZE);
      g_input_stream_read_async (G_INPUT_STREAM (stream),
				 data->content->data + data->pos,
				 GET_CONTENT_BLOCK_SIZE,
				 0,
				 data->cancellable,
				 load_contents_read_callback,
				 data);
      
    }
  else
    {
      res = g_simple_async_result_new_from_error (G_OBJECT (data->file),
						  data->callback,
						  data->user_data,
						  error);
      g_simple_async_result_complete (res);
      g_error_free (error);
      load_contents_data_free (data);
      g_object_unref (res);
    }
}

/**
 * g_file_load_partial_contents_async:
 * @file: input #GFile.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @read_more_callback: a #GFileReadMoreCallback to receive partial data and to specify whether further data should be read.
 * @callback: a #GAsyncReadyCallback to call when the request is satisfied
 * @user_data: the data to pass to the callback functions.
 *
 * Reads the partial contents of a file. A #GFileReadMoreCallback should be 
 * used to stop reading from the file when appropriate, else this function
 * will behave exactly as g_file_load_contents_async(). This operation 
 * can be finished by g_file_load_partial_contents_finish().
 *
 * Users of this function should be aware that @user_data is passed to 
 * both the @read_more_callback and the @callback.
 *
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. 
 **/
void
g_file_load_partial_contents_async (GFile                 *file,
				    GCancellable          *cancellable,
				    GFileReadMoreCallback  read_more_callback,
				    GAsyncReadyCallback    callback,
				    gpointer               user_data)
{
  LoadContentsData *data;

  g_return_if_fail (G_IS_FILE (file));

  data = g_new0 (LoadContentsData, 1);

  if (cancellable)
    data->cancellable = g_object_ref (cancellable);
  data->read_more_callback = read_more_callback;
  data->callback = callback;
  data->user_data = user_data;
  data->content = g_byte_array_new ();
  data->file = g_object_ref (file);

  g_file_read_async (file,
		     0,
		     cancellable,
		     load_contents_open_callback,
		     data);
}

/**
 * g_file_load_partial_contents_finish:
 * @file: input #GFile.
 * @res: a #GAsyncResult. 
 * @contents: a location to place the contents of the file.
 * @length: a location to place the length of the contents of the file.
 * @etag_out: a location to place the current entity tag for the file.
 * @error: a #GError, or %NULL
 * 
 * Finishes an asynchronous partial load operation that was started
 * with g_file_load_partial_contents_async().
 *
 * Returns: %TRUE if the load was successful. If %FALSE and @error is 
 * present, it will be set appropriately. 
 **/
gboolean
g_file_load_partial_contents_finish (GFile         *file,
				     GAsyncResult  *res,
				     char         **contents,
				     gsize         *length,
				     char         **etag_out,
				     GError       **error)
{
  GSimpleAsyncResult *simple;
  LoadContentsData *data;

  g_return_val_if_fail (G_IS_FILE (file), FALSE);
  g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (res), FALSE);
  g_return_val_if_fail (contents != NULL, FALSE);

  simple = G_SIMPLE_ASYNC_RESULT (res);

  if (g_simple_async_result_propagate_error (simple, error))
    return FALSE;
  
  g_warn_if_fail (g_simple_async_result_get_source_tag (simple) == g_file_load_contents_async);
  
  data = g_simple_async_result_get_op_res_gpointer (simple);

  if (data->error)
    {
      g_propagate_error (error, data->error);
      data->error = NULL;
      *contents = NULL;
      if (length)
	*length = 0;
      return FALSE;
    }

  if (length)
    *length = data->pos;

  if (etag_out)
    {
      *etag_out = data->etag;
      data->etag = NULL;
    }

  /* Zero terminate */
  g_byte_array_set_size (data->content, data->pos + 1);
  data->content->data[data->pos] = 0;
  
  *contents = (char *)g_byte_array_free (data->content, FALSE);
  data->content = NULL;

  return TRUE;
}

/**
 * g_file_load_contents_async:
 * @file: input #GFile.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @callback: a #GAsyncReadyCallback to call when the request is satisfied
 * @user_data: the data to pass to callback function
 * 
 * Starts an asynchronous load of the @file's contents.
 *
 * For more details, see g_file_load_contents() which is
 * the synchronous version of this call.
 *
 * When the load operation has completed, @callback will be called 
 * with @user data. To finish the operation, call 
 * g_file_load_contents_finish() with the #GAsyncResult returned by 
 * the @callback.
 * 
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. 
 **/
void
g_file_load_contents_async (GFile               *file,
			   GCancellable        *cancellable,
			   GAsyncReadyCallback  callback,
			   gpointer             user_data)
{
  g_file_load_partial_contents_async (file,
				      cancellable,
				      NULL,
				      callback, user_data);
}

/**
 * g_file_load_contents_finish:
 * @file: input #GFile.
 * @res: a #GAsyncResult. 
 * @contents: a location to place the contents of the file.
 * @length: a location to place the length of the contents of the file.
 * @etag_out: a location to place the current entity tag for the file.
 * @error: a #GError, or %NULL
 * 
 * Finishes an asynchronous load of the @file's contents. 
 * The contents are placed in @contents, and @length is set to the 
 * size of the @contents string. If @etag_out is present, it will be 
 * set to the new entity tag for the @file.
 * 
 * Returns: %TRUE if the load was successful. If %FALSE and @error is 
 * present, it will be set appropriately. 
 **/
gboolean
g_file_load_contents_finish (GFile         *file,
			     GAsyncResult  *res,
			     char         **contents,
			     gsize         *length,
			     char         **etag_out,
			     GError       **error)
{
  return g_file_load_partial_contents_finish (file,
					      res,
					      contents,
					      length,
					      etag_out,
					      error);
}
  
/**
 * g_file_replace_contents:
 * @file: input #GFile.
 * @contents: a string containing the new contents for @file.
 * @length: the length of @contents in bytes.
 * @etag: the old <link linkend="gfile-etag">entity tag</link> 
 *     for the document.
 * @make_backup: %TRUE if a backup should be created.
 * @flags: a set of #GFileCreateFlags.
 * @new_etag: a location to a new <link linkend="gfile-etag">entity tag</link>
 *      for the document.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @error: a #GError, or %NULL
 *
 * Replaces the contents of @file with @contents of @length bytes.
 
 * If @etag is specified (not %NULL) any existing file must have that etag, or
 * the error %G_IO_ERROR_WRONG_ETAG will be returned.
 *
 * If @make_backup is %TRUE, this function will attempt to make a backup of @file.
 * 
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. 
 *
 * The returned @new_etag can be used to verify that the file hasn't changed the
 * next time it is saved over.
 * 
 * Returns: %TRUE if successful. If an error
 * has occurred, this function will return %FALSE and set @error
 * appropriately if present.
 **/
gboolean
g_file_replace_contents (GFile             *file,
			 const char        *contents,
			 gsize              length,
			 const char        *etag,
			 gboolean           make_backup,
			 GFileCreateFlags   flags,
			 char             **new_etag,
			 GCancellable      *cancellable,
			 GError           **error)
{
  GFileOutputStream *out;
  gsize pos, remainder;
  gssize res;

  g_return_val_if_fail (G_IS_FILE (file), FALSE);
  g_return_val_if_fail (contents != NULL, FALSE);

  out = g_file_replace (file, etag, make_backup, flags, cancellable, error);
  if (out == NULL)
    return FALSE;

  pos = 0;
  remainder = length;
  while (remainder > 0 &&
	 (res = g_output_stream_write (G_OUTPUT_STREAM (out),
				       contents + pos,
				       MIN (remainder, GET_CONTENT_BLOCK_SIZE),
				       cancellable,
				       error)) > 0)
    {
      pos += res;
      remainder -= res;
    }
  
  if (remainder > 0 && res < 0)
    {
      /* Ignore errors on close */
      g_output_stream_close (G_OUTPUT_STREAM (out), cancellable, NULL);
      
      /* error is set already */
      return FALSE;
    }
  
  if (!g_output_stream_close (G_OUTPUT_STREAM (out), cancellable, error))
    return FALSE;

  if (new_etag)
    *new_etag = g_file_output_stream_get_etag (out);
  
  return TRUE;
}

typedef struct {
  GFile *file;
  GError *error;
  GCancellable *cancellable;
  GAsyncReadyCallback callback;
  gpointer user_data;
  const char *content;
  gsize length;
  gsize pos;
  char *etag;
} ReplaceContentsData;

static void
replace_contents_data_free (ReplaceContentsData *data)
{
  if (data->error)
    g_error_free (data->error);
  if (data->cancellable)
    g_object_unref (data->cancellable);
  g_object_unref (data->file);
  g_free (data->etag);
  g_free (data);
}

static void
replace_contents_close_callback (GObject      *obj,
				 GAsyncResult *close_res,
				 gpointer      user_data)
{
  GOutputStream *stream = G_OUTPUT_STREAM (obj);
  ReplaceContentsData *data = user_data;
  GSimpleAsyncResult *res;

  /* Ignore errors here, we're only reading anyway */
  g_output_stream_close_finish (stream, close_res, NULL);
  g_object_unref (stream);

  data->etag = g_file_output_stream_get_etag (G_FILE_OUTPUT_STREAM (stream));
  
  res = g_simple_async_result_new (G_OBJECT (data->file),
				   data->callback,
				   data->user_data,
				   g_file_replace_contents_async);
  g_simple_async_result_set_op_res_gpointer (res, data, (GDestroyNotify)replace_contents_data_free);
  g_simple_async_result_complete (res);
  g_object_unref (res);
}

static void
replace_contents_write_callback (GObject      *obj,
				 GAsyncResult *read_res,
				 gpointer      user_data)
{
  GOutputStream *stream = G_OUTPUT_STREAM (obj);
  ReplaceContentsData *data = user_data;
  GError *error = NULL;
  gssize write_size;
  
  write_size = g_output_stream_write_finish (stream, read_res, &error);

  if (write_size <= 0) 
    {
      /* Error or EOF, close the file */
      if (write_size < 0)
	data->error = error;
      g_output_stream_close_async (stream, 0,
				   data->cancellable,
				   replace_contents_close_callback, data);
    }
  else if (write_size > 0)
    {
      data->pos += write_size;

      if (data->pos >= data->length)
	g_output_stream_close_async (stream, 0,
				     data->cancellable,
				     replace_contents_close_callback, data);
      else
	g_output_stream_write_async (stream,
				     data->content + data->pos,
				     data->length - data->pos,
				     0,
				     data->cancellable,
				     replace_contents_write_callback,
				     data);
    }
}

static void
replace_contents_open_callback (GObject      *obj,
				GAsyncResult *open_res,
				gpointer      user_data)
{
  GFile *file = G_FILE (obj);
  GFileOutputStream *stream;
  ReplaceContentsData *data = user_data;
  GError *error = NULL;
  GSimpleAsyncResult *res;

  stream = g_file_replace_finish (file, open_res, &error);

  if (stream)
    {
      g_output_stream_write_async (G_OUTPUT_STREAM (stream),
				   data->content + data->pos,
				   data->length - data->pos,
				   0,
				   data->cancellable,
				   replace_contents_write_callback,
				   data);
      
    }
  else
    {
      res = g_simple_async_result_new_from_error (G_OBJECT (data->file),
						  data->callback,
						  data->user_data,
						  error);
      g_simple_async_result_complete (res);
      g_error_free (error);
      replace_contents_data_free (data);
      g_object_unref (res);
    }
}

/**
 * g_file_replace_contents_async:
 * @file: input #GFile.
 * @contents: string of contents to replace the file with.
 * @length: the length of @contents in bytes.
 * @etag: a new <link linkend="gfile-etag">entity tag</link> for the @file.
 * @make_backup: %TRUE if a backup should be created.
 * @flags: a set of #GFileCreateFlags.
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @callback: a #GAsyncReadyCallback to call when the request is satisfied
 * @user_data: the data to pass to callback function
 * 
 * Starts an asynchronous replacement of @file with the given 
 * @contents of @length bytes. @etag will replace the document's 
 * current entity tag.
 * 
 * When this operation has completed, @callback will be called with
 * @user_user data, and the operation can be finalized with 
 * g_file_replace_contents_finish().
 * 
 * If @cancellable is not %NULL, then the operation can be cancelled by
 * triggering the cancellable object from another thread. If the operation
 * was cancelled, the error %G_IO_ERROR_CANCELLED will be returned. 
 * 
 * If @make_backup is %TRUE, this function will attempt to 
 * make a backup of @file.
 **/
void
g_file_replace_contents_async  (GFile               *file,
				const char          *contents,
				gsize                length,
				const char          *etag,
				gboolean             make_backup,
				GFileCreateFlags     flags,
				GCancellable        *cancellable,
				GAsyncReadyCallback  callback,
				gpointer             user_data)
{
  ReplaceContentsData *data;

  g_return_if_fail (G_IS_FILE (file));
  g_return_if_fail (contents != NULL);

  data = g_new0 (ReplaceContentsData, 1);

  if (cancellable)
    data->cancellable = g_object_ref (cancellable);
  data->callback = callback;
  data->user_data = user_data;
  data->content = contents;
  data->length = length;
  data->pos = 0;
  data->file = g_object_ref (file);

  g_file_replace_async (file,
			etag,
			make_backup,
			flags,
			0,
			cancellable,
			replace_contents_open_callback,
			data);
}
  
/**
 * g_file_replace_contents_finish:
 * @file: input #GFile.
 * @res: a #GAsyncResult. 
 * @new_etag: a location of a new <link linkend="gfile-etag">entity tag</link> 
 *     for the document.
 * @error: a #GError, or %NULL 
 * 
 * Finishes an asynchronous replace of the given @file. See
 * g_file_replace_contents_async(). Sets @new_etag to the new entity 
 * tag for the document, if present.
 * 
 * Returns: %TRUE on success, %FALSE on failure.
 **/
gboolean
g_file_replace_contents_finish (GFile         *file,
				GAsyncResult  *res,
				char         **new_etag,
				GError       **error)
{
  GSimpleAsyncResult *simple;
  ReplaceContentsData *data;

  g_return_val_if_fail (G_IS_FILE (file), FALSE);
  g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (res), FALSE);

  simple = G_SIMPLE_ASYNC_RESULT (res);

  if (g_simple_async_result_propagate_error (simple, error))
    return FALSE;
  
  g_warn_if_fail (g_simple_async_result_get_source_tag (simple) == g_file_replace_contents_async);
  
  data = g_simple_async_result_get_op_res_gpointer (simple);

  if (data->error)
    {
      g_propagate_error (error, data->error);
      data->error = NULL;
      return FALSE;
    }


  if (new_etag)
    {
      *new_etag = data->etag;
      data->etag = NULL; /* Take ownership */
    }
  
  return TRUE;
}

#define __G_FILE_C__
#include "gioaliasdef.c"
