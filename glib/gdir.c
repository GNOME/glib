/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * gdir.c: Simplified wrapper around the DIRENT functions.
 *
 * Copyright 2001 Hans Breuer
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

#include <errno.h>
#include <string.h> /* strcmp */

#ifdef HAVE_DIRENT_H
#include <sys/types.h>
#include <dirent.h>
#endif

#include "glib.h"
#include "gdir.h"

#include "glibintl.h"

struct _GDir
{
  DIR *dir;
};

/**
 * g_dir_open:
 * @path: the path to the directory you are interested in
 * @flags: Currently must be set to 0. Reserved for future use.
 * @error: return location for a #GError, or %NULL.
 *         If non-%NULL, an error will be set if and only if
 *         g_dir_open_fails.
 *
 * Opens a directory for reading. The names of the files
 * in the directory can then be retrieved using
 * g_dir_read_name().
 *
 * Return value: a newly allocated #GDir on success, %NULL on failure.
 *   If non-%NULL, you must free the result with g_dir_close()
 *   when you are finished with it.
 **/
GDir *
g_dir_open (const gchar  *path,
            guint         flags,
            GError      **error)
{
  GDir *dir;
  gchar *utf8_path;

  g_return_val_if_fail (path != NULL, NULL);

  dir = g_new (GDir, 1);

  dir->dir = opendir (path);

  if (dir->dir)
    return dir;

  /* error case */
  utf8_path = g_filename_to_utf8 (path, -1,
				  NULL, NULL, NULL);
  g_set_error (error,
               G_FILE_ERROR,
               g_file_error_from_errno (errno),
               _("Error opening directory '%s': %s"),
	       utf8_path, g_strerror (errno));

  g_free (utf8_path);
  g_free (dir);

  return NULL;
}

/**
 * g_dir_read_name:
 * @dir: a #GDir* created by g_dir_open()
 *
 * Retrieves the name of the next entry in the directory.
 * The '.' and '..' entries are omitted. The returned name is in 
 * the encoding used for filenames. Use g_filename_to_utf8() to 
 * convert it to UTF-8.
 *
 * Return value: The entries name or %NULL if there are no 
 *   more entries. The return value is owned by GLib and
 *   must not be modified or freed.
 **/
G_CONST_RETURN gchar*
g_dir_read_name (GDir *dir)
{
  struct dirent *entry;

  g_return_val_if_fail (dir != NULL, NULL);

  entry = readdir (dir->dir);
  while (entry 
         && (0 == strcmp (entry->d_name, ".") ||
             0 == strcmp (entry->d_name, "..")))
    entry = readdir (dir->dir);

  if (entry)
    return entry->d_name;
  else
    return NULL;
}

/**
 * g_dir_rewind:
 * @dir: a #GDir* created by g_dir_open()
 *
 * Resets the given directory. The next call to g_dir_read_name()
 * will return the first entry again.
 **/
void
g_dir_rewind (GDir *dir)
{
  g_return_if_fail (dir != NULL);
  
  rewinddir (dir->dir);
}

/**
 * g_dir_close:
 * @dir: a #GDir* created by g_dir_open()
 *
 * Closes the directory and deallocates all related resources.
 **/
void
g_dir_close (GDir *dir)
{
  g_return_if_fail (dir != NULL);

  closedir (dir->dir);
  g_free (dir);
}
