/* gstdio.c - wrappers for C library functions
 *
 * Copyright 2004 Tor Lillqvist
 *
 * GLib is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * GLib is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with GLib; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"
#include "galias.h"

#include "glib.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef G_OS_WIN32
#include <errno.h>
#include <wchar.h>
#include <direct.h>
#include <io.h>
#endif

#include "gstdio.h"

#if !defined (G_OS_UNIX) && !defined (G_OS_WIN32)
#error Please port this to your operating system
#endif


/**
 * g_open:
 * @filename: a pathname in the GLib file name encoding
 * @flags: as in open()
 * @mode: as in open()
 *
 * A wrapper for the POSIX open() function. The open() function is
 * used to convert a pathname into a file descriptor. Note that on
 * POSIX systems file descriptors are implemented by the operating
 * system. On Windows, it's the C library that implements open() and
 * file descriptors. The actual Windows API for opening files is
 * something different.
 *
 * See the C library manual for more details about open().
 *
 * Returns: a new file descriptor, or -1 if an error occurred. The
 * return value can be used exactly like the return value from open().
 * 
 * Since: 2.6
 */
int
g_open (const gchar *filename,
	int          flags,
	int          mode)
{
#ifdef G_OS_WIN32
  if (G_WIN32_HAVE_WIDECHAR_API ())
    {
      wchar_t *wfilename = g_utf8_to_utf16 (filename, -1, NULL, NULL, NULL);
      int retval;
      int save_errno;
      
      if (wfilename == NULL)
	{
	  errno = EINVAL;
	  return -1;
	}

      retval = _wopen (wfilename, flags, mode);
      save_errno = errno;

      g_free (wfilename);

      errno = save_errno;
      return retval;
    }
  else
    {    
      gchar *cp_filename = g_locale_from_utf8 (filename, -1, NULL, NULL, NULL);
      int retval;
      int save_errno;

      if (cp_filename == NULL)
	{
	  errno = EINVAL;
	  return -1;
	}

      retval = open (cp_filename, flags, mode);
      save_errno = errno;

      g_free (cp_filename);

      errno = save_errno;
      return retval;
    }
#else
  return open (filename, flags, mode);
#endif
}

/**
 * g_rename:
 * @oldfilename: a pathname in the GLib file name encoding
 * @newfilename: a pathname in the GLib file name encoding
 *
 * A wrapper for the POSIX rename() function. The rename() function 
 * renames a file, moving it between directories if required.
 * 
 * See your C library manual for more details about how rename() works
 * on your system. Note in particular that on Windows, it is in
 * general not possible to rename a file if a file with the new name
 * already exists. Also it is not possible in general to rename an
 * open file.
 *
 * Returns: 0 if the renaming succeeded, -1 if an error occurred
 * 
 * Since: 2.6
 */
int
g_rename (const gchar *oldfilename,
	  const gchar *newfilename)
{
#ifdef G_OS_WIN32
  if (G_WIN32_HAVE_WIDECHAR_API ())
    {
      wchar_t *woldfilename = g_utf8_to_utf16 (oldfilename, -1, NULL, NULL, NULL);
      wchar_t *wnewfilename;
      int retval;
      int save_errno;

      if (woldfilename == NULL)
	{
	  errno = EINVAL;
	  return -1;
	}

      wnewfilename = g_utf8_to_utf16 (newfilename, -1, NULL, NULL, NULL);

      if (wnewfilename == NULL)
	{
	  g_free (woldfilename);
	  errno = EINVAL;
	  return -1;
	}

      retval = _wrename (woldfilename, wnewfilename);
      save_errno = errno;

      g_free (woldfilename);
      g_free (wnewfilename);
      
      errno = save_errno;
      return retval;
    }
  else
    {
      gchar *cp_oldfilename = g_locale_from_utf8 (oldfilename, -1, NULL, NULL, NULL);
      gchar *cp_newfilename;
      int retval;
      int save_errno;

      if (cp_oldfilename == NULL)
	{
	  errno = EINVAL;
	  return -1;
	}

      cp_newfilename = g_locale_from_utf8 (newfilename, -1, NULL, NULL, NULL);

      if (cp_newfilename == NULL)
	{
	  g_free (cp_oldfilename);
	  errno = EINVAL;
	  return -1;
	}
	
      retval = rename (cp_oldfilename, cp_newfilename);
      save_errno = errno;

      g_free (cp_oldfilename);
      g_free (cp_newfilename);

      errno = save_errno;
      return retval;
    }
#else
  return rename (oldfilename, newfilename);
#endif
}

/**
 * g_mkdir: 
 * @filename: a pathname in the GLib file name encoding
 * @mode: permissions to use for the newly created directory
 *
 * A wrapper for the POSIX mkdir() function. The mkdir() function 
 * attempts to create a directory with the given name and permissions.
 * 
 * See the C library manual for more details about mkdir().
 *
 * Returns: 0 if the directory was successfully created, -1 if an error 
 *    occurred
 * 
 * Since: 2.6
 */
int
g_mkdir (const gchar *filename,
	 int          mode)
{
#ifdef G_OS_WIN32
  if (G_WIN32_HAVE_WIDECHAR_API ())
    {
      wchar_t *wfilename = g_utf8_to_utf16 (filename, -1, NULL, NULL, NULL);
      int retval;
      int save_errno;

      if (wfilename == NULL)
	{
	  errno = EINVAL;
	  return -1;
	}

      retval = _wmkdir (wfilename);
      save_errno = errno;

      g_free (wfilename);
      
      errno = save_errno;
      return retval;
    }
  else
    {
      gchar *cp_filename = g_locale_from_utf8 (filename, -1, NULL, NULL, NULL);
      int retval;
      int save_errno;

      if (cp_filename == NULL)
	{
	  errno = EINVAL;
	  return -1;
	}

      retval = mkdir (cp_filename);
      save_errno = errno;

      g_free (cp_filename);

      errno = save_errno;
      return retval;
    }
#else
  return mkdir (filename, mode);
#endif
}

/**
 * g_stat: 
 * @filename: a pathname in the GLib file name encoding
 * @buf: a pointer to a <structname>stat</structname> struct, which
 *    will be filled with the file information
 *
 * A wrapper for the POSIX stat() function. The stat() function 
 * returns information about a file.
 * 
 * See the C library manual for more details about stat().
 *
 * Returns: 0 if the information was successfully retrieved, -1 if an error 
 *    occurred
 * 
 * Since: 2.6
 */
int
g_stat (const gchar *filename,
	struct stat *buf)
{
#ifdef G_OS_WIN32
  if (G_WIN32_HAVE_WIDECHAR_API ())
    {
      wchar_t *wfilename = g_utf8_to_utf16 (filename, -1, NULL, NULL, NULL);
      int retval;
      int save_errno;

      if (wfilename == NULL)
	{
	  errno = EINVAL;
	  return -1;
	}

      retval = _wstat (wfilename, (struct _stat *) buf);
      save_errno = errno;

      g_free (wfilename);

      errno = save_errno;
      return retval;
    }
  else
    {
      gchar *cp_filename = g_locale_from_utf8 (filename, -1, NULL, NULL, NULL);
      int retval;
      int save_errno;

      if (cp_filename == NULL)
	{
	  errno = EINVAL;
	  return -1;
	}

      retval = stat (cp_filename, buf);
      save_errno = errno;

      g_free (cp_filename);

      errno = save_errno;
      return retval;
    }
#else
  return stat (filename, buf);
#endif
}

/**
 * g_lstat: 
 * @filename: a pathname in the GLib file name encoding
 * @buf: a pointer to a <structname>stat</structname> struct, which
 *    will be filled with the file information
 *
 * A wrapper for the POSIX lstat() function. The lstat() function is
 * like stat() except that in the case of symbolic links, it returns
 * information about the symbolic link itself and not the file that it
 * refers to. If the system does not support symbolic links g_lstat()
 * is identical to g_stat().
 * 
 * See the C library manual for more details about lstat().
 *
 * Returns: 0 if the information was successfully retrieved, -1 if an error 
 *    occurred
 * 
 * Since: 2.6
 */
int
g_lstat (const gchar *filename,
	 struct stat *buf)
{
#ifdef HAVE_LSTAT
  /* This can't be Win32, so don't do the widechar dance. */
  return lstat (filename, buf);
#else
  return g_stat (filename, buf);
#endif
}

/**
 * g_unlink:
 * @filename: a pathname in the GLib file name encoding
 *
 * A wrapper for the POSIX unlink() function. The unlink() function 
 * deletes a name from the filesystem. If this was the last link to the 
 * file and no processes have it opened, the diskspace occupied by the
 * file is freed.
 * 
 * See your C library manual for more details about unlink(). Note
 * that on Windows, it is in general not possible to delete files that
 * are open to some process, or mapped into memory.
 *
 * Returns: 0 if the name was successfully deleted, -1 if an error 
 *    occurred
 * 
 * Since: 2.6
 */
int
g_unlink (const gchar *filename)
{
#ifdef G_OS_WIN32
  if (G_WIN32_HAVE_WIDECHAR_API ())
    {
      wchar_t *wfilename = g_utf8_to_utf16 (filename, -1, NULL, NULL, NULL);
      int retval;
      int save_errno;

      if (wfilename == NULL)
	{
	  errno = EINVAL;
	  return -1;
	}

      retval = _wunlink (wfilename);
      save_errno = errno;

      g_free (wfilename);

      errno = save_errno;
      return retval;
    }
  else
    {
      gchar *cp_filename = g_locale_from_utf8 (filename, -1, NULL, NULL, NULL);
      int retval;
      int save_errno;

      if (cp_filename == NULL)
	{
	  errno = EINVAL;
	  return -1;
	}

      retval = unlink (cp_filename);
      save_errno = errno;

      g_free (cp_filename);

      errno = save_errno;
      return retval;
    }
#else
  return unlink (filename);
#endif
}

/**
 * g_remove:
 * @filename: a pathname in the GLib file name encoding
 *
 * A wrapper for the POSIX remove() function. The remove() function
 * deletes a name from the filesystem.
 * 
 * See your C library manual for more details about how remove() works
 * on your system. On Unix, remove() removes also directories, as it
 * calls unlink() for files and rmdir() for directories. On Windows,
 * although remove() in the C library only works for files, this
 * function tries first remove() and then if that fails rmdir(), and
 * thus works for both files and directories. Note however, that on
 * Windows, it is in general not possible to remove a file that is
 * open to some process, or mapped into memory.
 *
 * Returns: 0 if the file was successfully removed, -1 if an error 
 *    occurred
 * 
 * Since: 2.6
 */
int
g_remove (const gchar *filename)
{
#ifdef G_OS_WIN32
  if (G_WIN32_HAVE_WIDECHAR_API ())
    {
      wchar_t *wfilename = g_utf8_to_utf16 (filename, -1, NULL, NULL, NULL);
      int retval;
      int save_errno;

      if (wfilename == NULL)
	{
	  errno = EINVAL;
	  return -1;
	}

      retval = _wremove (wfilename);
      if (retval == -1)
	retval = _wrmdir (wfilename);
      save_errno = errno;

      g_free (wfilename);

      errno = save_errno;
      return retval;
    }
  else
    {
      gchar *cp_filename = g_locale_from_utf8 (filename, -1, NULL, NULL, NULL);
      int retval;
      int save_errno;
      
      if (cp_filename == NULL)
	{
	  errno = EINVAL;
	  return -1;
	}

      retval = remove (cp_filename);
      if (retval == -1)
	retval = rmdir (cp_filename);
      save_errno = errno;

      g_free (cp_filename);

      errno = save_errno;
      return retval;
    }
#else
  return remove (filename);
#endif
}

/**
 * g_rmdir:
 * @filename: a pathname in the GLib file name encoding
 *
 * A wrapper for the POSIX rmdir() function. The rmdir() function
 * deletes a directory from the filesystem.
 * 
 * See your C library manual for more details about how rmdir() works
 * on your system.
 *
 * Returns: 0 if the directory was successfully removed, -1 if an error 
 *    occurred
 * 
 * Since: 2.6
 */
int
g_rmdir (const gchar *filename)
{
#ifdef G_OS_WIN32
  if (G_WIN32_HAVE_WIDECHAR_API ())
    {
      wchar_t *wfilename = g_utf8_to_utf16 (filename, -1, NULL, NULL, NULL);
      int retval;
      int save_errno;

      if (wfilename == NULL)
	{
	  errno = EINVAL;
	  return -1;
	}
      
      retval = _wrmdir (wfilename);
      save_errno = errno;

      g_free (wfilename);

      errno = save_errno;
      return retval;
    }
  else
    {
      gchar *cp_filename = g_locale_from_utf8 (filename, -1, NULL, NULL, NULL);
      int retval;
      int save_errno;

      if (cp_filename == NULL)
	{
	  errno = EINVAL;
	  return -1;
	}

      retval = rmdir (cp_filename);
      save_errno = errno;

      g_free (cp_filename);

      errno = save_errno;
      return retval;
    }
#else
  return rmdir (filename);
#endif
}

/**
 * g_fopen:
 * @filename: a pathname in the GLib file name encoding
 * @mode: a string describing the mode in which the file should be 
 *   opened
 *
 * A wrapper for the POSIX fopen() function. The fopen() function opens
 * a file and associates a new stream with it. 
 * 
 * See the C library manual for more details about fopen().
 *
 * Returns: A <typename>FILE</typename> pointer if the file was successfully
 *    opened, or %NULL if an error occurred
 * 
 * Since: 2.6
 */
FILE *
g_fopen (const gchar *filename,
	 const gchar *mode)
{
#ifdef G_OS_WIN32
  if (G_WIN32_HAVE_WIDECHAR_API ())
    {
      wchar_t *wfilename = g_utf8_to_utf16 (filename, -1, NULL, NULL, NULL);
      wchar_t *wmode;
      FILE *retval;
      int save_errno;

      if (wfilename == NULL)
	{
	  errno = EINVAL;
	  return NULL;
	}

      wmode = g_utf8_to_utf16 (mode, -1, NULL, NULL, NULL);

      if (wmode == NULL)
	{
	  g_free (wfilename);
	  errno = EINVAL;
	  return NULL;
	}
	
      retval = _wfopen (wfilename, wmode);
      save_errno = errno;

      g_free (wfilename);
      g_free (wmode);

      errno = save_errno;
      return retval;
    }
  else
    {
      gchar *cp_filename = g_locale_from_utf8 (filename, -1, NULL, NULL, NULL);
      FILE *retval;
      int save_errno;

      if (cp_filename == NULL)
	{
	  errno = EINVAL;
	  return NULL;
	}

      retval = fopen (cp_filename, mode);
      save_errno = errno;

      g_free (cp_filename);

      errno = save_errno;
      return retval;
    }
#else
  return fopen (filename, mode);
#endif
}

/**
 * g_freopen:
 * @filename: a pathname in the GLib file name encoding
 * @mode: a string describing the mode in which the file should be 
 *   opened
 * @stream: an existing stream which will be reused, or %NULL
 *
 * A wrapper for the POSIX freopen() function. The freopen() function
 * opens a file and associates it with an existing stream.
 * 
 * See the C library manual for more details about freopen().
 *
 * Returns: A <typename>FILE</typename> pointer if the file was successfully
 *    opened, or %NULL if an error occurred.
 * 
 * Since: 2.6
 */
FILE *
g_freopen (const gchar *filename,
	   const gchar *mode,
	   FILE        *stream)
{
#ifdef G_OS_WIN32
  if (G_WIN32_HAVE_WIDECHAR_API ())
    {
      wchar_t *wfilename = g_utf8_to_utf16 (filename, -1, NULL, NULL, NULL);
      wchar_t *wmode;
      FILE *retval;
      int save_errno;

      if (wfilename == NULL)
	{
	  errno = EINVAL;
	  return NULL;
	}
      
      wmode = g_utf8_to_utf16 (mode, -1, NULL, NULL, NULL);

      if (wmode == NULL)
	{
	  g_free (wfilename);
	  errno = EINVAL;
	  return NULL;
	}
      
      retval = _wfreopen (wfilename, wmode, stream);
      save_errno = errno;

      g_free (wfilename);
      g_free (wmode);

      errno = save_errno;
      return retval;
    }
  else
    {
      gchar *cp_filename = g_locale_from_utf8 (filename, -1, NULL, NULL, NULL);
      FILE *retval;
      int save_errno;

      if (cp_filename == NULL)
	{
	  errno = EINVAL;
	  return NULL;
	}

      retval = freopen (cp_filename, mode, stream);
      save_errno = errno;

      g_free (cp_filename);

      errno = save_errno;
      return retval;
    }
#else
  return freopen (filename, mode, stream);
#endif
}
