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

#include <string.h> /* strerror, strcmp */
#ifdef HAVE_DIRENT_H
#include <dirent.h>
#endif

#include "glib.h"
#include "gdir.h"

#include "glibintl.h"

struct _GDir
{
  /*< private >*/
  DIR *dir;
};

/**
 * g_dir_open:
 * @path: the path to the directory you are interested in
 * @flags: for future binary compatible extensions 
 * @error: return location for a #GError
 *
 * Return value: a #GDir* on success, NULL if error is set
 **/
GDir *
g_dir_open (const gchar  *path,
            guint         flags,
            GError      **error)
{
  GDir *dir = g_new (GDir, 1);

  dir->dir = opendir (path);

  if (dir->dir)
    return dir;

  /* error case */
  g_set_error (error,
               G_FILE_ERROR,
               g_file_error_from_errno (errno),
               _("Error opening dir '%s': %s"),
                 path, strerror (errno));

  g_free (dir);
  return NULL;
}

/**
 * g_dir_read_name:
 * @dir: a #GDir* created by g_dir_open()
 *
 * Iterator which delivers the next directory entries name
 * with each call. The '.' and '..' entries are omitted.
 *
 * BTW: using these functions will simplify porting of
 * your app, at least to Windows.
 *
 * Return value: The entries name or NULL if there are no 
 * more entries. Don't free this value!
 **/
G_CONST_RETURN gchar*
g_dir_read_name (GDir    *dir)
{
  struct dirent *entry;

  g_return_val_if_fail (dir != NULL, NULL);

  entry = readdir (dir->dir);
  while (entry 
         && (   0 == strcmp (entry->d_name, ".") 
             || 0 == strcmp (entry->d_name, "..")))
    entry = readdir (dir->dir);

  return entry->d_name;
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
 *
 * Return value: TRUE on success, FALSE otherwise.
 **/
gboolean
g_dir_close (GDir *dir)
{
  int ret = 0;

  g_return_val_if_fail (dir != NULL, FALSE);

  ret = closedir (dir->dir);
  g_free (dir);

  return !ret; 
}
