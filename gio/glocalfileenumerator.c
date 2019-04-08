/* GIO - GLib Input, Output and Streaming Library
 * 
 * Copyright (C) 2006-2007 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Alexander Larsson <alexl@redhat.com>
 */

#include "config.h"

#include <fcntl.h>
#include <glib.h>
#include <glocalfileenumerator.h>
#include <glocalfileinfo.h>
#include <glocalfile.h>
#include <gioerror.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "glibintl.h"
#include "gtask.h"


#define CHUNK_SIZE 1000

#ifdef G_OS_WIN32
#define USE_GDIR
#endif

#ifndef USE_GDIR

#include <sys/types.h>
#include <dirent.h>
#include <errno.h>

typedef struct {
  char *name;
  long inode;
  GFileType type;
} DirEntry;

#endif

struct _GLocalFileEnumerator
{
  GFileEnumerator parent;

  GFileAttributeMatcher *matcher;
  GFileAttributeMatcher *reduced_matcher;
  char *filename;
  char *attributes;
  GFileQueryInfoFlags flags;

  gboolean got_parent_info;
  GLocalParentFileInfo parent_info;
  
#ifdef USE_GDIR
  GDir *dir;
#else
  DIR *dir;
  DirEntry *entries;
  int entries_pos;
  gboolean at_end;
#endif
  
  gboolean follow_symlinks;
};

#define g_local_file_enumerator_get_type _g_local_file_enumerator_get_type
G_DEFINE_TYPE (GLocalFileEnumerator, g_local_file_enumerator, G_TYPE_FILE_ENUMERATOR)

static GFileInfo *g_local_file_enumerator_next_file (GFileEnumerator  *enumerator,
						     GCancellable     *cancellable,
						     GError          **error);
static gboolean   g_local_file_enumerator_close     (GFileEnumerator  *enumerator,
						     GCancellable     *cancellable,
						     GError          **error);
static GFileEnumerator *g_local_file_enumerator_enumerate_children        (GFileEnumerator      *enumerator,
                                                                           const gchar          *child_name,
                                                                           const gchar          *attributes,
                                                                           GFileQueryInfoFlags   flags,
                                                                           GCancellable         *cancellable,
                                                                           GError              **error);
static void             g_local_file_enumerator_enumerate_children_async  (GFileEnumerator      *enumerator,
                                                                           const gchar          *child_name,
                                                                           const gchar          *attributes,
                                                                           GFileQueryInfoFlags   flags,
                                                                           gint                  io_priority,
                                                                           GCancellable         *cancellable,
                                                                           GAsyncReadyCallback   callback,
                                                                           gpointer              user_data);
static GFileEnumerator *g_local_file_enumerator_enumerate_children_finish (GFileEnumerator      *enumerator,
                                                                           GAsyncResult         *result,
                                                                           GError              **error);

static void
free_entries (GLocalFileEnumerator *local)
{
#ifndef USE_GDIR
  int i;

  if (local->entries != NULL)
    {
      for (i = 0; local->entries[i].name != NULL; i++)
	g_free (local->entries[i].name);
      
      g_free (local->entries);
    }
#endif
}

static void
g_local_file_enumerator_finalize (GObject *object)
{
  GLocalFileEnumerator *local;

  local = G_LOCAL_FILE_ENUMERATOR (object);

  if (local->got_parent_info)
    _g_local_file_info_free_parent_info (&local->parent_info);
  g_free (local->filename);
  g_file_attribute_matcher_unref (local->matcher);
  g_file_attribute_matcher_unref (local->reduced_matcher);
  if (local->dir)
    {
#ifdef USE_GDIR
      g_dir_close (local->dir);
#else
      closedir (local->dir);
#endif      
      local->dir = NULL;
    }

  free_entries (local);

  G_OBJECT_CLASS (g_local_file_enumerator_parent_class)->finalize (object);
}


static void
g_local_file_enumerator_class_init (GLocalFileEnumeratorClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GFileEnumeratorClass *enumerator_class = G_FILE_ENUMERATOR_CLASS (klass);
  
  gobject_class->finalize = g_local_file_enumerator_finalize;

  enumerator_class->next_file = g_local_file_enumerator_next_file;
  enumerator_class->close_fn = g_local_file_enumerator_close;
  enumerator_class->enumerate_children = g_local_file_enumerator_enumerate_children;
  enumerator_class->enumerate_children_async = g_local_file_enumerator_enumerate_children_async;
  enumerator_class->enumerate_children_finish = g_local_file_enumerator_enumerate_children_finish;
}

static void
g_local_file_enumerator_init (GLocalFileEnumerator *local)
{
}

#ifdef USE_GDIR
static void
convert_file_to_io_error (GError **error,
			  GError  *file_error)
{
  int new_code;

  if (file_error == NULL)
    return;
  
  new_code = G_IO_ERROR_FAILED;
  
  if (file_error->domain == G_FILE_ERROR) 
    {
      switch (file_error->code) 
        {
        case G_FILE_ERROR_NOENT:
          new_code = G_IO_ERROR_NOT_FOUND;
          break;
        case G_FILE_ERROR_ACCES:
          new_code = G_IO_ERROR_PERMISSION_DENIED;
          break;
        case G_FILE_ERROR_NOTDIR:
          new_code = G_IO_ERROR_NOT_DIRECTORY;
          break;
        case G_FILE_ERROR_MFILE:
          new_code = G_IO_ERROR_TOO_MANY_OPEN_FILES;
          break;
        default:
          break;
        }
    }
  
  g_set_error_literal (error, G_IO_ERROR,
                       new_code,
                       file_error->message);
}
#else
static GFileAttributeMatcher *
g_file_attribute_matcher_subtract_attributes (GFileAttributeMatcher *matcher,
                                              const char *           attributes)
{
  GFileAttributeMatcher *result, *tmp;

  tmp = g_file_attribute_matcher_new (attributes);
  result = g_file_attribute_matcher_subtract (matcher, tmp);
  g_file_attribute_matcher_unref (tmp);

  return result;
}
#endif

static GFileEnumerator *
_g_local_file_enumerator_new_with_dir (GLocalFile           *file,
#ifdef USE_GDIR
				       GDir                 *dir,
#else
				       DIR                  *dir,
#endif
				       const char           *attributes,
				       GFileQueryInfoFlags   flags,
				       GCancellable         *cancellable,
				       GError              **error)
{
  GLocalFileEnumerator *local;
  char *filename = g_file_get_path (G_FILE (file));

  local = g_object_new (G_TYPE_LOCAL_FILE_ENUMERATOR,
                        "container", file,
                        NULL);

  local->dir = dir;
  local->filename = filename;
  local->matcher = g_file_attribute_matcher_new (attributes);
#ifndef USE_GDIR
  local->reduced_matcher = g_file_attribute_matcher_subtract_attributes (local->matcher,
                                                                         G_LOCAL_FILE_INFO_NOSTAT_ATTRIBUTES","
                                                                         "standard::type");
#endif
  local->flags = flags;

  return G_FILE_ENUMERATOR (local);
}

GFileEnumerator *
_g_local_file_enumerator_new (GLocalFile           *file,
			      const char           *attributes,
			      GFileQueryInfoFlags   flags,
			      GCancellable         *cancellable,
			      GError              **error)
{
  char *filename = g_file_get_path (G_FILE (file));

#ifdef USE_GDIR
  GError *dir_error;
  GDir *dir;

  dir_error = NULL;
  dir = g_dir_open (filename, 0, error != NULL ? &dir_error : NULL);
  if (dir == NULL)
    {
      if (error != NULL)
	{
	  convert_file_to_io_error (error, dir_error);
	  g_error_free (dir_error);
	}
      g_free (filename);
      return NULL;
    }
#else
  DIR *dir;
  int errsv;

  dir = opendir (filename);
  if (dir == NULL)
    {
      gchar *utf8_filename;
      errsv = errno;

      utf8_filename = g_filename_to_utf8 (filename, -1, NULL, NULL, NULL);
      g_set_error (error, G_IO_ERROR,
                   g_io_error_from_errno (errsv),
                   "Error opening directory '%s': %s",
                   utf8_filename, g_strerror (errsv));
      g_free (utf8_filename);
      g_free (filename);
      return NULL;
    }

#endif

  return _g_local_file_enumerator_new_with_dir (file, dir, attributes, flags, cancellable, error);
}

#ifndef USE_GDIR
static int
sort_by_inode (const void *_a, const void *_b)
{
  const DirEntry *a, *b;

  a = _a;
  b = _b;
  return a->inode - b->inode;
}

#ifdef HAVE_STRUCT_DIRENT_D_TYPE
static GFileType
file_type_from_dirent (char d_type)
{
  switch (d_type)
    {
    case DT_BLK:
    case DT_CHR:
    case DT_FIFO:
    case DT_SOCK:
      return G_FILE_TYPE_SPECIAL;
    case DT_DIR:
      return G_FILE_TYPE_DIRECTORY;
    case DT_LNK:
      return G_FILE_TYPE_SYMBOLIC_LINK;
    case DT_REG:
      return G_FILE_TYPE_REGULAR;
    case DT_UNKNOWN:
    default:
      return G_FILE_TYPE_UNKNOWN;
    }
}
#endif

static const char *
next_file_helper (GLocalFileEnumerator *local, GFileType *file_type)
{
  struct dirent *entry;
  const char *filename;
  int i;

  if (local->at_end)
    return NULL;
  
  if (local->entries == NULL ||
      (local->entries[local->entries_pos].name == NULL))
    {
      if (local->entries == NULL)
	local->entries = g_new (DirEntry, CHUNK_SIZE + 1);
      else
	{
	  /* Restart by clearing old names */
	  for (i = 0; local->entries[i].name != NULL; i++)
	    g_free (local->entries[i].name);
	}
      
      for (i = 0; i < CHUNK_SIZE; i++)
	{
	  entry = readdir (local->dir);
	  while (entry 
		 && (0 == strcmp (entry->d_name, ".") ||
		     0 == strcmp (entry->d_name, "..")))
	    entry = readdir (local->dir);

	  if (entry)
	    {
	      local->entries[i].name = g_strdup (entry->d_name);
	      local->entries[i].inode = entry->d_ino;
#if HAVE_STRUCT_DIRENT_D_TYPE
              local->entries[i].type = file_type_from_dirent (entry->d_type);
#else
              local->entries[i].type = G_FILE_TYPE_UNKNOWN;
#endif
	    }
	  else
	    break;
	}
      local->entries[i].name = NULL;
      local->entries_pos = 0;
      
      qsort (local->entries, i, sizeof (DirEntry), sort_by_inode);
    }

  filename = local->entries[local->entries_pos].name;
  if (filename == NULL)
    local->at_end = TRUE;
    
  *file_type = local->entries[local->entries_pos].type;

  local->entries_pos++;

  return filename;
}

#endif

static GFileInfo *
g_local_file_enumerator_next_file (GFileEnumerator  *enumerator,
				   GCancellable     *cancellable,
				   GError          **error)
{
  GLocalFileEnumerator *local = G_LOCAL_FILE_ENUMERATOR (enumerator);
  const char *filename;
  char *path;
  GFileInfo *info;
  GError *my_error;
  GFileType file_type;

  if (!local->got_parent_info)
    {
      _g_local_file_info_get_parent_info (local->filename, local->matcher, &local->parent_info);
      local->got_parent_info = TRUE;
    }

 next_file:

#ifdef USE_GDIR
  filename = g_dir_read_name (local->dir);
  file_type = G_FILE_TYPE_UNKNOWN;
#else
  filename = next_file_helper (local, &file_type);
#endif

  if (filename == NULL)
    return NULL;

  my_error = NULL;
  path = g_build_filename (local->filename, filename, NULL);
  if (file_type == G_FILE_TYPE_UNKNOWN ||
      (file_type == G_FILE_TYPE_SYMBOLIC_LINK && !(local->flags & G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS)))
    {
      info = _g_local_file_info_get (filename, path,
                                     local->matcher,
                                     local->flags,
                                     &local->parent_info,
                                     &my_error); 
    }
  else
    {
      info = _g_local_file_info_get (filename, path,
                                     local->reduced_matcher,
                                     local->flags,
                                     &local->parent_info,
                                     &my_error); 
      if (info)
        {
          _g_local_file_info_get_nostat (info, filename, path, local->matcher);
          g_file_info_set_file_type (info, file_type);
          if (file_type == G_FILE_TYPE_SYMBOLIC_LINK)
            g_file_info_set_is_symlink (info, TRUE);
        }
    }
  g_free (path);

  if (info == NULL)
    {
      /* Failed to get info */
      /* If the file does not exist there might have been a race where
       * the file was removed between the readdir and the stat, so we
       * ignore the file. */
      if (g_error_matches (my_error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
	{
	  g_error_free (my_error);
	  goto next_file;
	}
      else
	g_propagate_error (error, my_error);
    }

  return info;
}

static gboolean
g_local_file_enumerator_close (GFileEnumerator  *enumerator,
			       GCancellable     *cancellable,
			       GError          **error)
{
  GLocalFileEnumerator *local = G_LOCAL_FILE_ENUMERATOR (enumerator);

  if (local->dir)
    {
#ifdef USE_GDIR
      g_dir_close (local->dir);
#else
      closedir (local->dir);
#endif
      local->dir = NULL;
    }

  return TRUE;
}

static GFileEnumerator *
g_local_file_enumerator_enumerate_children (GFileEnumerator      *enumerator,
                                            const gchar          *child_name,
                                            const gchar          *attributes,
                                            GFileQueryInfoFlags   flags,
                                            GCancellable         *cancellable,
                                            GError              **error)
{
#ifdef USE_GDIR
  return G_FILE_ENUMERATOR_CLASS (g_local_file_enumerator_parent_class)->
    enumerate_children (enumerator, child_name, attributes, flags, cancellable, error);
#else
  GLocalFileEnumerator *local = G_LOCAL_FILE_ENUMERATOR (enumerator);
  GFile *child;
  DIR *dir;
  int fd;
  int childfd;
  GFileEnumerator *ret;

  fd = dirfd (local->dir);
  if (fd == -1)
    goto handle_errno;

  childfd = openat (fd, child_name, O_RDONLY|O_DIRECTORY);
  if (childfd == -1)
    goto handle_errno;

  child = g_file_get_child (g_file_enumerator_get_container (enumerator), child_name);
  dir = fdopendir (childfd);
  ret = _g_local_file_enumerator_new_with_dir (G_LOCAL_FILE (child), dir, attributes, flags, cancellable, error);
  g_clear_object (&child);

  return g_steal_pointer (&ret);

handle_errno:
  {
    int errsv = errno;

    g_set_error (error, G_IO_ERROR,
		 g_io_error_from_errno (errsv),
		 "Error enumerating child '%s': %s",
		 child_name, g_strerror (errsv));

    return NULL;
  }
#endif
}

#ifndef USE_GDIR
typedef struct
{
  GFile *child;
  gchar *attributes;
  GFileQueryInfoFlags flags;
  gint fd;
} EnumerateChildren;

static void
enumerate_children_free (gpointer data)
{
  EnumerateChildren *state = data;

  if (state->fd != -1)
    close (state->fd);
  g_clear_pointer (&state->attributes, g_free);
  g_clear_object (&state->child);
  g_slice_free (EnumerateChildren, state);
}

static void
enumerate_children_worker (GTask        *task,
                           gpointer      source_object,
                           gpointer      task_data,
                           GCancellable *cancellable)
{
  EnumerateChildren *state = task_data;
  GFileEnumerator *ret;
  GError *error = NULL;
  DIR *dir;

  g_assert (G_IS_TASK (task));
  g_assert (state != NULL);
  g_assert (state->fd >= 0);
  g_assert (G_IS_FILE (state->child));

  dir = fdopendir (state->fd);
  ret = _g_local_file_enumerator_new_with_dir (G_LOCAL_FILE (state->child),
					       dir,
					       state->attributes,
					       state->flags,
					       cancellable,
					       &error);

  if (error != NULL)
    g_task_return_error (task, g_steal_pointer (&error));
  else
    g_task_return_pointer (task, g_steal_pointer (&ret), g_object_unref);
}
#endif

static void
g_local_file_enumerator_enumerate_children_async (GFileEnumerator     *enumerator,
                                                  const gchar         *child_name,
                                                  const gchar         *attributes,
                                                  GFileQueryInfoFlags  flags,
                                                  gint                 io_priority,
                                                  GCancellable        *cancellable,
                                                  GAsyncReadyCallback  callback,
                                                  gpointer             user_data)
{
#ifdef USE_GDIR
  G_FILE_ENUMERATOR_CLASS (g_local_file_enumerator_parent_class)->
    enumerate_children_async (enumerator, child_name, attributes, flags, cancellable, callback, user_data);
#else
  GLocalFileEnumerator *self = G_LOCAL_FILE_ENUMERATOR (enumerator);
  EnumerateChildren *state;
  GTask *task;
  int errsv;
  int fd;

  task = g_task_new (enumerator, cancellable, callback, user_data);
  g_task_set_source_tag (task, g_local_file_enumerator_enumerate_children_async);
  g_task_set_priority (task, io_priority);

  fd = dirfd (self->dir);
  if (fd == -1)
    goto handle_errno;

  fd = dup (fd);
  if (fd == -1)
    goto handle_errno;

  state = g_slice_new0 (EnumerateChildren);
  state->child = g_file_get_child (g_file_enumerator_get_container (enumerator), child_name);
  state->fd = fd;
  state->attributes = g_strdup (attributes);
  state->flags = flags;

  g_task_set_task_data (task, state, enumerate_children_free);
  g_task_run_in_thread (task, enumerate_children_worker);

  goto cleanup;

handle_errno:
  errsv = errno;
  g_task_return_new_error (task,
                           G_IO_ERROR,
                           g_io_error_from_errno (errsv),
                           "Error enumerating child '%s': %s",
                           child_name, g_strerror (errsv));

cleanup:
  g_clear_object (&task);
#endif
}

static GFileEnumerator *
g_local_file_enumerator_enumerate_children_finish (GFileEnumerator  *enumerator,
                                                   GAsyncResult     *result,
                                                   GError          **error)
{
#ifdef USE_GDIR
  return G_FILE_ENUMERATOR_CLASS (g_local_file_enumerator_parent_class)->
    enumerate_children_finish (enumerator, result, error);
#else
  g_assert (G_IS_LOCAL_FILE_ENUMERATOR (enumerator));
  g_assert (G_IS_TASK (result));

  return g_task_propagate_pointer (G_TASK (result), error);
#endif
}
