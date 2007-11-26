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

#ifndef __G_FILE_H__
#define __G_FILE_H__

#include <glib-object.h>
#include <gio/gfileinfo.h>
#include <gio/gfileenumerator.h>
#include <gio/gfileinputstream.h>
#include <gio/gfileoutputstream.h>
#include <gio/gmountoperation.h>

G_BEGIN_DECLS

#define G_TYPE_FILE            (g_file_get_type ())
#define G_FILE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), G_TYPE_FILE, GFile))
#define G_IS_FILE(obj)	       (G_TYPE_CHECK_INSTANCE_TYPE ((obj), G_TYPE_FILE))
#define G_FILE_GET_IFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), G_TYPE_FILE, GFileIface))

typedef enum {
  G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS = (1<<0)
} GFileQueryInfoFlags;

typedef enum  {
  G_FILE_CREATE_FLAGS_NONE = 0,
  G_FILE_CREATE_FLAGS_PRIVATE = (1<<0)
} GFileCreateFlags;

typedef enum {
  G_FILE_COPY_OVERWRITE = (1<<0),
  G_FILE_COPY_BACKUP = (1<<1),
  G_FILE_COPY_NOFOLLOW_SYMLINKS = (1<<2),
  G_FILE_COPY_ALL_METADATA = (1<<3)
} GFileCopyFlags;

typedef enum  {
  G_FILE_MONITOR_FLAGS_NONE = 0,
  G_FILE_MONITOR_FLAGS_MONITOR_MOUNTS = (1<<0)
} GFileMonitorFlags;

typedef struct _GFile         		GFile; /* Dummy typedef */
typedef struct _GFileIface    		GFileIface;
typedef struct _GDirectoryMonitor       GDirectoryMonitor;
typedef struct _GFileMonitor            GFileMonitor;
typedef struct _GVolume         GVolume; /* Dummy typedef */

typedef void (*GFileProgressCallback) (goffset current_num_bytes,
				       goffset total_num_bytes,
				       gpointer user_data);
typedef gboolean (* GFileReadMoreCallback) (const char *file_contents,
					    goffset file_size,
					    gpointer callback_data);


struct _GFileIface
{
  GTypeInterface g_iface;

  /* Virtual Table */

  GFile *             (*dup)                        (GFile         *file);
  guint               (*hash)                       (GFile         *file);
  gboolean            (*equal)                      (GFile         *file1,
						     GFile         *file2);
  gboolean            (*is_native)                  (GFile         *file);
  gboolean            (*has_uri_scheme)             (GFile         *file,
						     const char    *uri_scheme);
  char *              (*get_uri_scheme)             (GFile         *file);
  char *              (*get_basename)               (GFile         *file);
  char *              (*get_path)                   (GFile         *file);
  char *              (*get_uri)                    (GFile         *file);
  char *              (*get_parse_name)             (GFile         *file);
  GFile *             (*get_parent)                 (GFile         *file);
  gboolean            (*contains_file)              (GFile         *parent,
						     GFile         *descendant);
  char *              (*get_relative_path)          (GFile         *parent,
						     GFile         *descendant);
  GFile *             (*resolve_relative_path)      (GFile        *file,
						     const char   *relative_path);
  GFile *             (*get_child_for_display_name) (GFile        *file,
						     const char   *display_name,
						     GError      **error);
  
  GFileEnumerator *   (*enumerate_children)        (GFile                *file,
						    const char           *attributes,
						    GFileQueryInfoFlags   flags,
						    GCancellable         *cancellable,
						    GError              **error);
  void                (*enumerate_children_async)  (GFile                      *file,
						    const char                 *attributes,
						    GFileQueryInfoFlags         flags,
						    int                         io_priority,
						    GCancellable               *cancellable,
						    GAsyncReadyCallback         callback,
						    gpointer                    user_data);
  GFileEnumerator *   (*enumerate_children_finish) (GFile                      *file,
						    GAsyncResult               *res,
						    GError                    **error);
  
  GFileInfo *         (*query_info)         (GFile                *file,
					     const char           *attributes,
					     GFileQueryInfoFlags   flags,
					     GCancellable         *cancellable,
					     GError              **error);
  void                (*query_info_async)   (GFile                *file,
					     const char           *attributes,
					     GFileQueryInfoFlags   flags,
					     int                   io_priority,
					     GCancellable         *cancellable,
					     GAsyncReadyCallback   callback,
					     gpointer              user_data);
  GFileInfo *         (*query_info_finish)  (GFile                *file,
					     GAsyncResult         *res,
					     GError              **error);
  
  GFileInfo *         (*query_filesystem_info)(GFile                *file,
					     const char           *attributes,
					     GCancellable         *cancellable,
					     GError              **error);
  void                (*_query_filesystem_info_async) (void);
  void                (*_query_filesystem_info_finish) (void);
  
  GVolume *           (*find_enclosing_volume)(GFile              *file,
					       GCancellable       *cancellable,
					       GError            **error);
  void                (*find_enclosing_volume_async)(GFile              *file,
						     int                   io_priority,
						     GCancellable         *cancellable,
						     GAsyncReadyCallback   callback,
						     gpointer              user_data);
  GVolume *           (*find_enclosing_volume_finish)(GFile              *file,
						      GAsyncResult         *res,
						      GError            **error);
  
  GFile *             (*set_display_name)         (GFile                *file,
						   const char           *display_name,
						   GCancellable         *cancellable,
						   GError              **error);
  void                (*set_display_name_async)   (GFile                      *file,
						   const char                 *display_name,
						   int                         io_priority,
						   GCancellable               *cancellable,
						   GAsyncReadyCallback         callback,
						   gpointer                    user_data);
  GFile *              (*set_display_name_finish) (GFile                      *file,
						   GAsyncResult               *res,
						   GError                    **error);
  
  GFileAttributeInfoList * (*query_settable_attributes) (GFile        *file,
							 GCancellable *cancellable,
							 GError      **error);
  void                (*_query_settable_attributes_async) (void);
  void                (*_query_settable_attributes_finish) (void);
  
  GFileAttributeInfoList * (*query_writable_namespaces) (GFile        *file,
							 GCancellable *cancellable,
							 GError      **error);
  void                (*_query_writable_namespaces_async) (void);
  void                (*_query_writable_namespaces_finish) (void);
  
  gboolean            (*set_attribute)            (GFile                *file,
						   const char           *attribute,
						   const GFileAttributeValue *value,
						   GFileQueryInfoFlags   flags,
						   GCancellable         *cancellable,
						   GError              **error);
  gboolean            (*set_attributes_from_info) (GFile          *file,
						   GFileInfo            *info,
						   GFileQueryInfoFlags   flags,
						   GCancellable         *cancellable,
						   GError              **error);
  void                (*set_attributes_async)     (GFile                      *file,
						   GFileInfo                  *info,
						   GFileQueryInfoFlags        flags,
						   int                         io_priority,
						   GCancellable               *cancellable,
						   GAsyncReadyCallback         callback,
						   gpointer                    user_data);
  gboolean            (*set_attributes_finish)    (GFile                      *file,
						   GAsyncResult               *result,
						   GFileInfo                 **info,
						   GError                    **error);
  
  GFileInputStream *  (*read)               (GFile                *file,
					     GCancellable         *cancellable,
					     GError              **error);
  void                (*read_async)         (GFile                *file,
					     int                   io_priority,
					     GCancellable         *cancellable,
					     GAsyncReadyCallback   callback,
					     gpointer              user_data);
  GFileInputStream *  (*read_finish)        (GFile                *file,
					     GAsyncResult         *res,
					     GError              **error);
  
  GFileOutputStream * (*append_to)          (GFile                *file,
					     GFileCreateFlags      flags,
					     GCancellable         *cancellable,
					     GError               **error);
  void                 (*append_to_async)   (GFile                      *file,
					     GFileCreateFlags            flags,
					     int                         io_priority,
					     GCancellable               *cancellable,
					     GAsyncReadyCallback         callback,
					     gpointer                    user_data);
  GFileOutputStream *  (*append_to_finish)  (GFile                      *file,
					     GAsyncResult               *res,
					     GError                    **error);
  
  GFileOutputStream * (*create)             (GFile                *file,
					     GFileCreateFlags      flags,
					     GCancellable         *cancellable,
					     GError               **error);
  void                 (*create_async)      (GFile                      *file,
					     GFileCreateFlags            flags,
					     int                         io_priority,
					     GCancellable               *cancellable,
					     GAsyncReadyCallback         callback,
					     gpointer                    user_data);
  GFileOutputStream *  (*create_finish)     (GFile                      *file,
					     GAsyncResult               *res,
					     GError                    **error);
  
  GFileOutputStream *  (*replace)           (GFile                *file,
					     const char           *etag,
					     gboolean              make_backup,
					     GFileCreateFlags      flags,
					     GCancellable         *cancellable,
					     GError              **error);
  void                 (*replace_async)     (GFile                      *file,
					     const char                 *etag,
					     gboolean                    make_backup,
					     GFileCreateFlags            flags,
					     int                         io_priority,
					     GCancellable               *cancellable,
					     GAsyncReadyCallback         callback,
					     gpointer                    user_data);
  GFileOutputStream *  (*replace_finish)    (GFile                      *file,
					     GAsyncResult               *res,
					     GError                    **error);
  
  gboolean            (*delete_file)        (GFile                *file,
					     GCancellable         *cancellable,
					     GError              **error);
  void                (*_delete_file_async) (void);
  void                (*_delete_file_finish) (void);
  
  gboolean            (*trash)              (GFile                *file,
					     GCancellable         *cancellable,
					     GError              **error);
  void                (*_trash_async) (void);
  void                (*_trash_finish) (void);
  
  gboolean            (*make_directory)     (GFile                *file,
					     GCancellable         *cancellable,
					     GError              **error);
  void                (*_make_directory_async) (void);
  void                (*_make_directory_finish) (void);
  
  gboolean            (*make_symbolic_link) (GFile                *file,
					     const char           *symlink_value,
					     GCancellable         *cancellable,
					     GError              **error);
  void                (*_make_symbolic_link_async) (void);
  void                (*_make_symbolic_link_finish) (void);
  
  gboolean            (*copy)               (GFile                *source,
					     GFile                *destination,
					     GFileCopyFlags        flags,
					     GCancellable         *cancellable,
					     GFileProgressCallback progress_callback,
					     gpointer              progress_callback_data,
					     GError              **error);
  void                (*_copy_async) (void);
  void                (*_copy_finish) (void);
  
  gboolean            (*move)               (GFile                *source,
					     GFile                *destination,
					     GFileCopyFlags        flags,
					     GCancellable         *cancellable,
					     GFileProgressCallback progress_callback,
					     gpointer              progress_callback_data,
					     GError              **error);

  void                (*_move_async) (void);
  void                (*_move_finish) (void);


  void                (*mount_mountable)           (GFile               *file,
						    GMountOperation     *mount_operation,
						    GCancellable         *cancellable,
						    GAsyncReadyCallback  callback,
						    gpointer             user_data);
  GFile *             (*mount_mountable_finish)    (GFile               *file,
						    GAsyncResult        *result,
						    GError             **error);
  void                (*unmount_mountable)         (GFile               *file,
						    GCancellable         *cancellable,
						    GAsyncReadyCallback  callback,
						    gpointer             user_data);
  gboolean            (*unmount_mountable_finish)  (GFile               *file,
						    GAsyncResult        *result,
						    GError             **error);
  void                (*eject_mountable)           (GFile               *file,
						    GCancellable        *cancellable,
						    GAsyncReadyCallback  callback,
						    gpointer             user_data);
  gboolean            (*eject_mountable_finish)    (GFile               *file,
						    GAsyncResult        *result,
						    GError             **error);


  void     (*mount_for_location)        (GFile *location,
					 GMountOperation *mount_operation,
					 GCancellable *cancellable,
					 GAsyncReadyCallback callback,
					 gpointer user_data);
  gboolean (*mount_for_location_finish) (GFile *location,
					 GAsyncResult *result,
					 GError **error);
  
  GDirectoryMonitor* (*monitor_dir)         (GFile                  *file,
					     GFileMonitorFlags       flags,
					     GCancellable           *cancellable);

  GFileMonitor*      (*monitor_file)        (GFile                  *file,
					     GFileMonitorFlags       flags,
					     GCancellable           *cancellable);
};

GType g_file_get_type (void) G_GNUC_CONST;

GFile *                 g_file_new_for_path               (const char                 *path);
GFile *                 g_file_new_for_uri                (const char                 *uri);
GFile *                 g_file_new_for_commandline_arg    (const char                 *arg);
GFile *                 g_file_parse_name                 (const char                 *parse_name);
GFile *                 g_file_dup                        (GFile                      *file);
guint                   g_file_hash                       (gconstpointer               file);
gboolean                g_file_equal                      (GFile                      *file1,
							   GFile                      *file2);
char *                  g_file_get_basename               (GFile                      *file);
char *                  g_file_get_path                   (GFile                      *file);
char *                  g_file_get_uri                    (GFile                      *file);
char *                  g_file_get_parse_name             (GFile                      *file);
GFile *                 g_file_get_parent                 (GFile                      *file);
GFile *                 g_file_get_child                  (GFile                      *file,
							   const char                 *name);
GFile *                 g_file_get_child_for_display_name (GFile                      *file,
							   const char                 *display_name,
							   GError                    **error);
gboolean                g_file_contains_file              (GFile                      *parent,
							   GFile                      *descendant);
char *                  g_file_get_relative_path          (GFile                      *parent,
							   GFile                      *descendant);
GFile *                 g_file_resolve_relative_path      (GFile                      *file,
							   const char                 *relative_path);
gboolean                g_file_is_native                  (GFile                      *file);
gboolean                g_file_has_uri_scheme             (GFile                      *file,
							   const char                 *uri_scheme);
char *                  g_file_get_uri_scheme             (GFile                      *file);
GFileInputStream *      g_file_read                       (GFile                      *file,
							   GCancellable               *cancellable,
							   GError                    **error);
void                    g_file_read_async                 (GFile                      *file,
							   int                         io_priority,
							   GCancellable               *cancellable,
							   GAsyncReadyCallback         callback,
							   gpointer                    user_data);
GFileInputStream *      g_file_read_finish                (GFile                      *file,
							   GAsyncResult               *res,
							   GError                    **error);
GFileOutputStream *     g_file_append_to                  (GFile                      *file,
							   GFileCreateFlags             flags,
							   GCancellable               *cancellable,
							   GError                    **error);
GFileOutputStream *     g_file_create                     (GFile                      *file,
							   GFileCreateFlags             flags,
							   GCancellable               *cancellable,
							   GError                    **error);
GFileOutputStream *     g_file_replace                    (GFile                      *file,
							   const char                 *etag,
							   gboolean                    make_backup,
							   GFileCreateFlags            flags,
							   GCancellable               *cancellable,
							   GError                    **error);
void                    g_file_append_to_async            (GFile                      *file,
							   GFileCreateFlags            flags,
							   int                         io_priority,
							   GCancellable               *cancellable,
							   GAsyncReadyCallback         callback,
							   gpointer                    user_data);
GFileOutputStream *     g_file_append_to_finish           (GFile                      *file,
							   GAsyncResult               *res,
							   GError                    **error);
void                    g_file_create_async               (GFile                      *file,
							   GFileCreateFlags            flags,
							   int                         io_priority,
							   GCancellable               *cancellable,
							   GAsyncReadyCallback         callback,
							   gpointer                    user_data);
GFileOutputStream *     g_file_create_finish              (GFile                      *file,
							   GAsyncResult               *res,
							   GError                    **error);
void                    g_file_replace_async              (GFile                      *file,
							   const char                 *etag,
							   gboolean                    make_backup,
							   GFileCreateFlags            flags,
							   int                         io_priority,
							   GCancellable               *cancellable,
							   GAsyncReadyCallback         callback,
							   gpointer                    user_data);
GFileOutputStream *     g_file_replace_finish             (GFile                      *file,
							   GAsyncResult               *res,
							   GError                    **error);
GFileInfo *             g_file_query_info                 (GFile                      *file,
							   const char                 *attributes,
							   GFileQueryInfoFlags         flags,
							   GCancellable               *cancellable,
							   GError                    **error);
void                    g_file_query_info_async           (GFile                      *file,
							   const char                 *attributes,
							   GFileQueryInfoFlags         flags,
							   int                         io_priority,
							   GCancellable               *cancellable,
							   GAsyncReadyCallback         callback,
							   gpointer                    user_data);
GFileInfo *             g_file_query_info_finish          (GFile                      *file,
							   GAsyncResult               *res,
							   GError                    **error);
GFileInfo *             g_file_query_filesystem_info      (GFile                      *file,
							   const char                 *attributes,
							   GCancellable               *cancellable,
							   GError                    **error);
GVolume *               g_file_find_enclosing_volume      (GFile                      *file,
							   GCancellable               *cancellable,
							   GError                    **error);
GFileEnumerator *       g_file_enumerate_children         (GFile                      *file,
							   const char                 *attributes,
							   GFileQueryInfoFlags         flags,
							   GCancellable               *cancellable,
							   GError                    **error);
void                    g_file_enumerate_children_async   (GFile                      *file,
							   const char                 *attributes,
							   GFileQueryInfoFlags         flags,
							   int                         io_priority,
							   GCancellable               *cancellable,
							   GAsyncReadyCallback         callback,
							   gpointer                    user_data);
GFileEnumerator *       g_file_enumerate_children_finish  (GFile                      *file,
							   GAsyncResult               *res,
							   GError                    **error);
GFile *                 g_file_set_display_name           (GFile                      *file,
							   const char                 *display_name,
							   GCancellable               *cancellable,
							   GError                    **error);
void                    g_file_set_display_name_async     (GFile                      *file,
							   const char                 *display_name,
							   int                         io_priority,
							   GCancellable               *cancellable,
							   GAsyncReadyCallback         callback,
							   gpointer                    user_data);
GFile *                 g_file_set_display_name_finish    (GFile                      *file,
							   GAsyncResult               *res,
							   GError                    **error);
gboolean                g_file_delete                     (GFile                      *file,
							   GCancellable               *cancellable,
							   GError                    **error);
gboolean                g_file_trash                      (GFile                      *file,
							   GCancellable               *cancellable,
							   GError                    **error);
gboolean                g_file_copy                       (GFile                      *source,
							   GFile                      *destination,
							   GFileCopyFlags              flags,
							   GCancellable               *cancellable,
							   GFileProgressCallback       progress_callback,
							   gpointer                    progress_callback_data,
							   GError                    **error);
gboolean                g_file_move                       (GFile                      *source,
							   GFile                      *destination,
							   GFileCopyFlags              flags,
							   GCancellable               *cancellable,
							   GFileProgressCallback       progress_callback,
							   gpointer                    progress_callback_data,
							   GError                    **error);
gboolean                g_file_make_directory             (GFile                      *file,
							   GCancellable               *cancellable,
							   GError                    **error);
gboolean                g_file_make_symbolic_link         (GFile                      *file,
							   const char                 *symlink_value,
							   GCancellable               *cancellable,
							   GError                    **error);
GFileAttributeInfoList *g_file_query_settable_attributes  (GFile                      *file,
							   GCancellable               *cancellable,
							   GError                    **error);
GFileAttributeInfoList *g_file_query_writable_namespaces  (GFile                      *file,
							   GCancellable               *cancellable,
							   GError                    **error);
gboolean                g_file_set_attribute              (GFile                      *file,
							   const char                 *attribute,
							   const GFileAttributeValue  *value,
							   GFileQueryInfoFlags         flags,
							   GCancellable               *cancellable,
							   GError                    **error);
gboolean                g_file_set_attributes_from_info   (GFile                      *file,
							   GFileInfo                  *info,
							   GFileQueryInfoFlags         flags,
							   GCancellable               *cancellable,
							   GError                    **error);
void                    g_file_set_attributes_async       (GFile                      *file,
							   GFileInfo                  *info,
							   GFileQueryInfoFlags         flags,
							   int                         io_priority,
							   GCancellable               *cancellable,
							   GAsyncReadyCallback         callback,
							   gpointer                    user_data);
gboolean                g_file_set_attributes_finish      (GFile                      *file,
							   GAsyncResult               *result,
							   GFileInfo                 **info,
							   GError                    **error);
gboolean                g_file_set_attribute_string       (GFile                      *file,
							   const char                 *attribute,
							   const char                 *value,
							   GFileQueryInfoFlags         flags,
							   GCancellable               *cancellable,
							   GError                    **error);
gboolean                g_file_set_attribute_byte_string  (GFile                      *file,
							   const char                 *attribute,
							   const char                 *value,
							   GFileQueryInfoFlags         flags,
							   GCancellable               *cancellable,
							   GError                    **error);
gboolean                g_file_set_attribute_uint32       (GFile                      *file,
							   const char                 *attribute,
							   guint32                     value,
							   GFileQueryInfoFlags         flags,
							   GCancellable               *cancellable,
							   GError                    **error);
gboolean                g_file_set_attribute_int32        (GFile                      *file,
							   const char                 *attribute,
							   gint32                      value,
							   GFileQueryInfoFlags         flags,
							   GCancellable               *cancellable,
							   GError                    **error);
gboolean                g_file_set_attribute_uint64       (GFile                      *file,
							   const char                 *attribute,
							   guint64                     value,
							   GFileQueryInfoFlags         flags,
							   GCancellable               *cancellable,
							   GError                    **error);
gboolean                g_file_set_attribute_int64        (GFile                      *file,
							   const char                 *attribute,
							   gint64                      value,
							   GFileQueryInfoFlags         flags,
							   GCancellable               *cancellable,
							   GError                    **error);
void                    g_mount_for_location              (GFile                      *location,
							   GMountOperation            *mount_operation,
							   GCancellable               *cancellable,
							   GAsyncReadyCallback         callback,
							   gpointer                    user_data);
gboolean                g_mount_for_location_finish       (GFile                      *location,
							   GAsyncResult               *result,
							   GError                    **error);
void                    g_file_mount_mountable            (GFile                      *file,
							   GMountOperation            *mount_operation,
							   GCancellable               *cancellable,
							   GAsyncReadyCallback         callback,
							   gpointer                    user_data);
GFile *                 g_file_mount_mountable_finish     (GFile                      *file,
							   GAsyncResult               *result,
							   GError                    **error);
void                    g_file_unmount_mountable          (GFile                      *file,
							   GCancellable               *cancellable,
							   GAsyncReadyCallback         callback,
							   gpointer                    user_data);
gboolean                g_file_unmount_mountable_finish   (GFile                      *file,
							   GAsyncResult               *result,
							   GError                    **error);
void                    g_file_eject_mountable            (GFile                      *file,
							   GCancellable               *cancellable,
							   GAsyncReadyCallback         callback,
							   gpointer                    user_data);
gboolean                g_file_eject_mountable_finish     (GFile                      *file,
							   GAsyncResult               *result,
							   GError                    **error);

gboolean                g_file_copy_attributes            (GFile                      *source,
							   GFile                      *destination,
							   GFileCopyFlags              flags,
							   GCancellable               *cancellable,
							   GError                    **error);


GDirectoryMonitor* g_file_monitor_directory          (GFile                  *file,
						      GFileMonitorFlags       flags,
						      GCancellable           *cancellable);
GFileMonitor*      g_file_monitor_file               (GFile                  *file,
						      GFileMonitorFlags       flags,
						      GCancellable           *cancellable);


/* Utilities */

gboolean g_file_load_contents                (GFile                  *file,
					      GCancellable           *cancellable,
					      char                  **contents,
					      gsize                  *length,
					      char                  **etag_out,
					      GError                **error);
void     g_file_load_contents_async          (GFile                  *file,
					      GCancellable           *cancellable,
					      GAsyncReadyCallback     callback,
					      gpointer                user_data);
gboolean g_file_load_contents_finish         (GFile                  *file,
					      GAsyncResult           *res,
					      char                  **contents,
					      gsize                  *length,
					      char                  **etag_out,
					      GError                **error);
void     g_file_load_partial_contents_async  (GFile                  *file,
					      GCancellable           *cancellable,
					      GFileReadMoreCallback   read_more_callback,
					      GAsyncReadyCallback     callback,
					      gpointer                user_data);
gboolean g_file_load_partial_contents_finish (GFile                  *file,
					      GAsyncResult           *res,
					      char                  **contents,
					      gsize                  *length,
					      char                  **etag_out,
					      GError                **error);
gboolean g_file_replace_contents             (GFile                  *file,
					      const char             *contents,
					      gsize                   length,
					      const char             *etag,
					      gboolean                make_backup,
					      GFileCreateFlags        flags,
					      char                  **new_etag,
					      GCancellable           *cancellable,
					      GError                **error);
void     g_file_replace_contents_async       (GFile                  *file,
					      const char             *contents,
					      gsize                   length,
					      const char             *etag,
					      gboolean                make_backup,
					      GFileCreateFlags        flags,
					      GCancellable           *cancellable,
					      GAsyncReadyCallback     callback,
					      gpointer                user_data);
gboolean g_file_replace_contents_finish      (GFile                  *file,
					      GAsyncResult           *res,
					      char                  **new_etag,
					      GError                **error);

G_END_DECLS

#endif /* __G_FILE_H__ */
