/* gfileutils.c - File utility functions
 *
 *  Copyright 2000 Red Hat, Inc.
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
 *   Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include "glib.h"

#include <sys/stat.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifdef G_OS_WIN32
#include <io.h>
#ifndef F_OK
#define	F_OK 0
#define	X_OK 1
#define	W_OK 2
#define	R_OK 4
#endif /* !F_OK */

#ifndef S_ISREG
#define S_ISREG(mode) ((mode)&_S_IFREG)
#endif

#ifndef S_ISDIR
#define S_ISDIR(mode) ((mode)&_S_IFDIR)
#endif

#endif /* G_OS_WIN32 */

#ifndef S_ISLNK
#define S_ISLNK(x) 0
#endif

#ifndef O_BINARY
#define O_BINARY 0
#endif

#define _(x) x

/**
 * g_file_test:
 * @filename: a filename to test
 * @test: bitfield of #GFileTest flags
 * 
 * Returns TRUE if any of the tests in the bitfield @test are
 * TRUE. For example, (G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)
 * will return TRUE if the file exists; the check whether it's
 * a directory doesn't matter since the existence test is TRUE.
 * With the current set of available tests, there's no point
 * passing in more than one test at a time.
 *
 * Return value: whether a test was TRUE
 **/
gboolean
g_file_test (const gchar *filename,
             GFileTest    test)
{
  if (test & G_FILE_TEST_EXISTS)
    return (access (filename, F_OK) == 0);
  else if (test & G_FILE_TEST_IS_EXECUTABLE)
    return (access (filename, X_OK) == 0);
  else
    {
      struct stat s;
      
      if (stat (filename, &s) < 0)
        return FALSE;

      if ((test & G_FILE_TEST_IS_REGULAR) &&
          S_ISREG (s.st_mode))
        return TRUE;
      else if ((test & G_FILE_TEST_IS_DIR) &&
               S_ISDIR (s.st_mode))
        return TRUE;
      else if ((test & G_FILE_TEST_IS_SYMLINK) &&
               S_ISLNK (s.st_mode))
        return TRUE;
      else
        return FALSE;
    }
}

GQuark
g_file_error_quark (void)
{
  static GQuark q = 0;
  if (q == 0)
    q = g_quark_from_static_string ("g-file-error-quark");

  return q;
}

GFileError
g_file_error_from_errno (gint en)
{
  switch (en)
    {
#ifdef EEXIST
    case EEXIST:
      return G_FILE_ERROR_EXIST;
      break;
#endif

#ifdef EISDIR
    case EISDIR:
      return G_FILE_ERROR_ISDIR;
      break;
#endif

#ifdef EACCES
    case EACCES:
      return G_FILE_ERROR_ACCES;
      break;
#endif

#ifdef ENAMETOOLONG
    case ENAMETOOLONG:
      return G_FILE_ERROR_NAMETOOLONG;
      break;
#endif

#ifdef ENOENT
    case ENOENT:
      return G_FILE_ERROR_NOENT;
      break;
#endif

#ifdef ENOTDIR
    case ENOTDIR:
      return G_FILE_ERROR_NOTDIR;
      break;
#endif

#ifdef ENXIO
    case ENXIO:
      return G_FILE_ERROR_NXIO;
      break;
#endif

#ifdef ENODEV
    case ENODEV:
      return G_FILE_ERROR_NODEV;
      break;
#endif

#ifdef EROFS
    case EROFS:
      return G_FILE_ERROR_ROFS;
      break;
#endif

#ifdef ETXTBSY
    case ETXTBSY:
      return G_FILE_ERROR_TXTBSY;
      break;
#endif

#ifdef EFAULT
    case EFAULT:
      return G_FILE_ERROR_FAULT;
      break;
#endif

#ifdef ELOOP
    case ELOOP:
      return G_FILE_ERROR_LOOP;
      break;
#endif

#ifdef ENOSPC
    case ENOSPC:
      return G_FILE_ERROR_NOSPC;
      break;
#endif

#ifdef ENOMEM
    case ENOMEM:
      return G_FILE_ERROR_NOMEM;
      break;
#endif

#ifdef EMFILE
    case EMFILE:
      return G_FILE_ERROR_MFILE;
      break;
#endif

#ifdef ENFILE
    case ENFILE:
      return G_FILE_ERROR_NFILE;
      break;
#endif

#ifdef EBADF
    case EBADF:
      return G_FILE_ERROR_BADF;
      break;
#endif

#ifdef EINVAL
    case EINVAL:
      return G_FILE_ERROR_INVAL;
      break;
#endif

#ifdef EPIPE
    case EPIPE:
      return G_FILE_ERROR_PIPE;
      break;
#endif

#ifdef EAGAIN
    case EAGAIN:
      return G_FILE_ERROR_AGAIN;
      break;
#endif

#ifdef EINTR
    case EINTR:
      return G_FILE_ERROR_INTR;
      break;
#endif

#ifdef EIO
    case EIO:
      return G_FILE_ERROR_IO;
      break;
#endif

#ifdef EPERM
    case EPERM:
      return G_FILE_ERROR_PERM;
      break;
#endif
      
    default:
      return G_FILE_ERROR_FAILED;
      break;
    }
}

static gboolean
get_contents_stdio (const gchar *filename,
                    FILE        *f,
                    gchar      **contents,
                    guint       *length,
                    GError     **error)
{
  gchar buf[2048];
  size_t bytes;
  GString *str;

  g_assert (f != NULL);
  
  str = g_string_new ("");
  
  while (!feof (f))
    {
      bytes = fread (buf, 1, 2048, f);
      
      if (ferror (f))
        {
          g_set_error (error,
                       G_FILE_ERROR,
                       g_file_error_from_errno (errno),
                       _("Error reading file '%s': %s"),
                       filename, strerror (errno));

          g_string_free (str, TRUE);
          
          return FALSE;
        }

      g_string_append_len (str, buf, bytes);
    }

  fclose (f);

  if (length)
    *length = str->len;
  
  *contents = g_string_free (str, FALSE);

  return TRUE;  
}

#ifndef G_OS_WIN32

static gboolean
get_contents_regfile (const gchar *filename,
                      struct stat *stat_buf,
                      gint         fd,
                      gchar      **contents,
                      guint       *length,
                      GError     **error)
{
  gchar *buf;
  size_t bytes_read;
  size_t size;
      
  size = stat_buf->st_size;

  buf = g_new (gchar, size + 1);
      
  bytes_read = 0;
  while (bytes_read < size)
    {
      gint rc;
          
      rc = read (fd, buf + bytes_read, size - bytes_read);

      if (rc < 0)
        {
          if (errno != EINTR) 
            {
              close (fd);

              g_free (buf);
                  
              g_set_error (error,
                           G_FILE_ERROR,
                           g_file_error_from_errno (errno),
                           _("Failed to read from file '%s': %s"),
                           filename, strerror (errno));

              return FALSE;
            }
        }
      else if (rc == 0)
        break;
      else
        bytes_read += rc;
    }
      
  buf[bytes_read] = '\0';

  if (length)
    *length = bytes_read;
  
  *contents = buf;

  return TRUE;
}

static gboolean
get_contents_posix (const gchar *filename,
                    gchar      **contents,
                    guint       *length,
                    GError     **error)
{
  struct stat stat_buf;
  gint fd;
  
  fd = open (filename, O_RDONLY);

  if (fd < 0)
    {
      g_set_error (error,
                   G_FILE_ERROR,
                   g_file_error_from_errno (errno),
                   _("Failed to open file '%s': %s"),
                   filename, strerror (errno));

      return FALSE;
    }

  /* I don't think this will ever fail, aside from ENOMEM, but. */
  if (fstat (fd, &stat_buf) < 0)
    {
      close (fd);
      
      g_set_error (error,
                   G_FILE_ERROR,
                   g_file_error_from_errno (errno),
                   _("Failed to get attributes of file '%s': fstat() failed: %s"),
                   filename, strerror (errno));

      return FALSE;
    }

  if (stat_buf.st_size > 0 && S_ISREG (stat_buf.st_mode))
    {
      return get_contents_regfile (filename,
                                   &stat_buf,
                                   fd,
                                   contents,
                                   length,
                                   error);
    }
  else
    {
      FILE *f;

      f = fdopen (fd, "r");
      
      if (f == NULL)
        {
          g_set_error (error,
                       G_FILE_ERROR,
                       g_file_error_from_errno (errno),
                       _("Failed to open file '%s': fdopen() failed: %s"),
                       filename, strerror (errno));
          
          return FALSE;
        }
  
      return get_contents_stdio (filename, f, contents, length, error);
    }
}

#else  /* G_OS_WIN32 */

static gboolean
get_contents_win32 (const gchar *filename,
                    gchar      **contents,
                    guint       *length,
                    GError     **error)
{
  FILE *f;

  /* I guess you want binary mode; maybe you want text sometimes? */
  f = fopen (filename, "rb");

  if (f == NULL)
    {
      g_set_error (error,
                   G_FILE_ERROR,
                   g_file_error_from_errno (errno),
                   _("Failed to open file '%s': %s"),
                   filename, strerror (errno));
      
      return FALSE;
    }
  
  return get_contents_stdio (filename, f, contents, length, error);
}

#endif

/**
 * g_file_get_contents:
 * @filename: a file to read contents from
 * @contents: location to store an allocated string
 * @length: location to store length in bytes of the contents
 * @error: return location for a #GError
 * 
 * Reads an entire file into allocated memory, with good error
 * checking. If @error is set, FALSE is returned, and @contents is set
 * to NULL. If TRUE is returned, @error will not be set, and @contents
 * will be set to the file contents.  The string stored in @contents
 * will be nul-terminated, so for text files you can pass NULL for the
 * @length argument.  The error domain is #G_FILE_ERROR. Possible
 * error codes are those in the #GFileError enumeration.
 *
 * FIXME currently crashes if the file is too big to fit in memory;
 * should probably use g_try_malloc() when we have that function.
 * 
 * Return value: TRUE on success, FALSE if error is set
 **/
gboolean
g_file_get_contents (const gchar *filename,
                     gchar      **contents,
                     guint       *length,
                     GError     **error)
{  
  g_return_val_if_fail (filename != NULL, FALSE);
  g_return_val_if_fail (contents != NULL, FALSE);

  *contents = NULL;
  if (length)
    *length = 0;

#ifdef G_OS_WIN32
  return get_contents_win32 (filename, contents, length, error);
#else
  return get_contents_posix (filename, contents, length, error);
#endif
}

/*
 * mkstemp() implementation is from the GNU C library.
 * Copyright (C) 1991,92,93,94,95,96,97,98,99 Free Software Foundation, Inc.
 */
/**
 * g_mkstemp:
 * @tmpl: template filename
 *
 * Open a temporary file. See "man mkstemp" on most UNIX-like systems.
 * This is a portability wrapper, which simply calls mkstemp() on systems
 * that have it, and implements it in GLib otherwise.
 *
 * The parameter is a string that should match the rules for mktemp, i.e.
 * end in "XXXXXX". The X string will be modified to form the name
 * of a file that didn't exist.
 *
 * Return value: A file handle (as from open()) to the file file
 * opened for reading and writing. The file is opened in binary mode
 * on platforms where there is a difference. The file handle should be
 * closed with close(). In case of errors, -1 is returned.
 *
 */
int
g_mkstemp (char *tmpl)
{
#ifdef HAVE_MKSTEMP
  return mkstemp (tmpl);
#else
  int len;
  char *XXXXXX;
  int count, fd;
  static const char letters[] =
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  glong value;
  GTimeVal tv;

  len = strlen (tmpl);
  if (len < 6 || strcmp (&tmpl[len - 6], "XXXXXX"))
    return -1;

  /* This is where the Xs start.  */
  XXXXXX = &tmpl[len - 6];

  /* Get some more or less random data.  */
  g_get_current_time (&tv);
  value = tv.tv_usec ^ tv.tv_sec;

  for (count = 0; count < 100; value += 7777, ++count)
    {
      glong v = value;

      /* Fill in the random bits.  */
      XXXXXX[0] = letters[v % 62];
      v /= 62;
      XXXXXX[1] = letters[v % 62];
      v /= 62;
      XXXXXX[2] = letters[v % 62];
      v /= 62;
      XXXXXX[3] = letters[v % 62];
      v /= 62;
      XXXXXX[4] = letters[v % 62];
      v /= 62;
      XXXXXX[5] = letters[v % 62];

      fd = open (tmpl, O_RDWR | O_CREAT | O_EXCL | O_BINARY, 0600);

      if (fd >= 0)
	return fd;
      else if (errno != EEXIST)
	/* Any other error will apply also to other names we might
	 *  try, and there are 2^32 or so of them, so give up now.
	 */
	return -1;
    }

  /* We got out of the loop because we ran out of combinations to try.  */
  return -1;
#endif
}
