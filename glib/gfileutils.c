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
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>

#ifdef G_OS_WIN32
#include <io.h>
#ifndef F_OK
#define	F_OK 0
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

#include "glibintl.h"

#ifdef G_OS_WIN32
#define G_IS_DIR_SEPARATOR(c) (c == G_DIR_SEPARATOR || c == '/')
#else
#define G_IS_DIR_SEPARATOR(c) (c == G_DIR_SEPARATOR)
#endif

/**
 * g_file_test:
 * @filename: a filename to test
 * @test: bitfield of #GFileTest flags
 * 
 * Returns %TRUE if any of the tests in the bitfield @test are
 * %TRUE. For example, <literal>(G_FILE_TEST_EXISTS | 
 * G_FILE_TEST_IS_DIR)</literal> will return %TRUE if the file exists; 
 * the check whether it's a directory doesn't matter since the existence 
 * test is %TRUE. With the current set of available tests, there's no point
 * passing in more than one test at a time.
 * 
 * Apart from %G_FILE_TEST_IS_SYMLINK all tests follow symbolic links,
 * so for a symbolic link to a regular file g_file_test() will return
 * %TRUE for both %G_FILE_TEST_IS_SYMLINK and %G_FILE_TEST_IS_REGULAR.
 *
 * Note, that for a dangling symbolic link g_file_test() will return
 * %TRUE for %G_FILE_TEST_IS_SYMLINK and %FALSE for all other flags.
 *
 * You should never use g_file_test() to test whether it is safe
 * to perform an operaton, because there is always the possibility
 * of the condition changing before you actually perform the operation.
 * For example, you might think you could use %G_FILE_TEST_IS_SYMLINK
 * to know whether it is is safe to write to a file without being
 * tricked into writing into a different location. It doesn't work!
 *
 * <informalexample><programlisting>
 * /&ast; DON'T DO THIS &ast;/
 *  if (!g_file_test (filename, G_FILE_TEST_IS_SYMLINK)) {
 *    fd = open (filename, O_WRONLY);
 *    /&ast; write to fd &ast;/
 *  }
 * </programlisting></informalexample>
 *
 * Another thing to note is that %G_FILE_TEST_EXISTS and
 * %G_FILE_TEST_IS_EXECUTABLE are implemented using the access()
 * system call. This usually doesn't matter, but if your program
 * is setuid or setgid it means that these tests will give you
 * the answer for the real user ID and group ID , rather than the
 * effective user ID and group ID.
 *
 * Return value: whether a test was %TRUE
 **/
gboolean
g_file_test (const gchar *filename,
             GFileTest    test)
{
  if ((test & G_FILE_TEST_EXISTS) && (access (filename, F_OK) == 0))
    return TRUE;
  
#ifndef G_OS_WIN32
  if ((test & G_FILE_TEST_IS_EXECUTABLE) && (access (filename, X_OK) == 0))
    {
      if (getuid () != 0)
	return TRUE;

      /* For root, on some POSIX systems, access (filename, X_OK)
       * will succeed even if no executable bits are set on the
       * file. We fall through to a stat test to avoid that.
       */
    }
  else
    test &= ~G_FILE_TEST_IS_EXECUTABLE;
#endif	

  if (test & G_FILE_TEST_IS_SYMLINK)
    {
#ifdef G_OS_WIN32
      /* no sym links on win32, no lstat in msvcrt */
#else
      struct stat s;

      if ((lstat (filename, &s) == 0) && S_ISLNK (s.st_mode))
        return TRUE;
#endif
    }
  
  if (test & (G_FILE_TEST_IS_REGULAR |
	      G_FILE_TEST_IS_DIR |
	      G_FILE_TEST_IS_EXECUTABLE))
    {
      struct stat s;
      
      if (stat (filename, &s) == 0)
	{
	  if ((test & G_FILE_TEST_IS_REGULAR) && S_ISREG (s.st_mode))
	    return TRUE;
	  
	  if ((test & G_FILE_TEST_IS_DIR) && S_ISDIR (s.st_mode))
	    return TRUE;

#ifndef G_OS_WIN32
	  /* The extra test for root when access (file, X_OK) succeeds.
	   * Probably only makes sense on Unix.
	   */
	  if ((test & G_FILE_TEST_IS_EXECUTABLE) &&
	      ((s.st_mode & S_IXOTH) ||
	       (s.st_mode & S_IXUSR) ||
	       (s.st_mode & S_IXGRP)))
	    return TRUE;
#else
	  if ((test & G_FILE_TEST_IS_EXECUTABLE) &&
	      (s.st_mode & _S_IEXEC))
	    return TRUE;
#endif
	}
    }

  return FALSE;
}

GQuark
g_file_error_quark (void)
{
  static GQuark q = 0;
  if (q == 0)
    q = g_quark_from_static_string ("g-file-error-quark");

  return q;
}

/**
 * g_file_error_from_errno:
 * @err_no: an "errno" value
 * 
 * Gets a #GFileError constant based on the passed-in @errno.
 * For example, if you pass in %EEXIST this function returns
 * #G_FILE_ERROR_EXIST. Unlike @errno values, you can portably
 * assume that all #GFileError values will exist.
 *
 * Normally a #GFileError value goes into a #GError returned
 * from a function that manipulates files. So you would use
 * g_file_error_from_errno() when constructing a #GError.
 * 
 * Return value: #GFileError corresponding to the given @errno
 **/
GFileError
g_file_error_from_errno (gint err_no)
{
  switch (err_no)
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
                    gsize       *length, 
                    GError     **error)
{
  gchar buf[2048];
  size_t bytes;
  char *str;
  size_t total_bytes;
  size_t total_allocated;
  
  g_assert (f != NULL);

#define STARTING_ALLOC 64
  
  total_bytes = 0;
  total_allocated = STARTING_ALLOC;
  str = g_malloc (STARTING_ALLOC);
  
  while (!feof (f))
    {
      bytes = fread (buf, 1, 2048, f);

      while ((total_bytes + bytes + 1) > total_allocated)
        {
          total_allocated *= 2;
          str = g_try_realloc (str, total_allocated);

          if (str == NULL)
            {
              g_set_error (error,
                           G_FILE_ERROR,
                           G_FILE_ERROR_NOMEM,
                           _("Could not allocate %lu bytes to read file \"%s\""),
                           (gulong) total_allocated, filename);
              goto error;
            }
        }
      
      if (ferror (f))
        {
          g_set_error (error,
                       G_FILE_ERROR,
                       g_file_error_from_errno (errno),
                       _("Error reading file '%s': %s"),
                       filename, g_strerror (errno));

          goto error;
        }

      memcpy (str + total_bytes, buf, bytes);
      total_bytes += bytes;
    }

  fclose (f);

  str[total_bytes] = '\0';
  
  if (length)
    *length = total_bytes;
  
  *contents = str;
  
  return TRUE;

 error:

  g_free (str);
  fclose (f);
  
  return FALSE;  
}

#ifndef G_OS_WIN32

static gboolean
get_contents_regfile (const gchar *filename,
                      struct stat *stat_buf,
                      gint         fd,
                      gchar      **contents,
                      gsize       *length,
                      GError     **error)
{
  gchar *buf;
  size_t bytes_read;
  size_t size;
  size_t alloc_size;
  
  size = stat_buf->st_size;

  alloc_size = size + 1;
  buf = g_try_malloc (alloc_size);

  if (buf == NULL)
    {
      g_set_error (error,
                   G_FILE_ERROR,
                   G_FILE_ERROR_NOMEM,
                   _("Could not allocate %lu bytes to read file \"%s\""),
                   (gulong) alloc_size, filename);

      goto error;
    }
  
  bytes_read = 0;
  while (bytes_read < size)
    {
      gssize rc;
          
      rc = read (fd, buf + bytes_read, size - bytes_read);

      if (rc < 0)
        {
          if (errno != EINTR) 
            {
              g_free (buf);
                  
              g_set_error (error,
                           G_FILE_ERROR,
                           g_file_error_from_errno (errno),
                           _("Failed to read from file '%s': %s"),
                           filename, g_strerror (errno));

	      goto error;
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

  close (fd);

  return TRUE;

 error:

  close (fd);
  
  return FALSE;
}

static gboolean
get_contents_posix (const gchar *filename,
                    gchar      **contents,
                    gsize       *length,
                    GError     **error)
{
  struct stat stat_buf;
  gint fd;
  
  /* O_BINARY useful on Cygwin */
  fd = open (filename, O_RDONLY|O_BINARY);

  if (fd < 0)
    {
      g_set_error (error,
                   G_FILE_ERROR,
                   g_file_error_from_errno (errno),
                   _("Failed to open file '%s': %s"),
                   filename, g_strerror (errno));

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
                   filename, g_strerror (errno));

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
                       filename, g_strerror (errno));
          
          return FALSE;
        }
  
      return get_contents_stdio (filename, f, contents, length, error);
    }
}

#else  /* G_OS_WIN32 */

static gboolean
get_contents_win32 (const gchar *filename,
                    gchar      **contents,
                    gsize       *length,
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
                   filename, g_strerror (errno));
      
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
 * checking. If @error is set, %FALSE is returned, and @contents is set
 * to %NULL. If %TRUE is returned, @error will not be set, and @contents
 * will be set to the file contents.  The string stored in @contents
 * will be nul-terminated, so for text files you can pass %NULL for the
 * @length argument.  The error domain is #G_FILE_ERROR. Possible
 * error codes are those in the #GFileError enumeration.
 *
 * Return value: %TRUE on success, %FALSE if error is set
 **/
gboolean
g_file_get_contents (const gchar *filename,
                     gchar      **contents,
                     gsize       *length,
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
 * Opens a temporary file. See the mkstemp() documentation
 * on most UNIX-like systems. This is a portability wrapper, which simply calls 
 * mkstemp() on systems that have it, and implements 
 * it in GLib otherwise.
 *
 * The parameter is a string that should match the rules for
 * mkstemp(), i.e. end in "XXXXXX". The X string will 
 * be modified to form the name of a file that didn't exist.
 *
 * Return value: A file handle (as from open()) to the file
 * opened for reading and writing. The file is opened in binary mode
 * on platforms where there is a difference. The file handle should be
 * closed with close(). In case of errors, -1 is returned.
 */
gint
g_mkstemp (gchar *tmpl)
{
#ifdef HAVE_MKSTEMP
  return mkstemp (tmpl);
#else
  int len;
  char *XXXXXX;
  int count, fd;
  static const char letters[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  static const int NLETTERS = sizeof (letters) - 1;
  glong value;
  GTimeVal tv;
  static int counter = 0;

  len = strlen (tmpl);
  if (len < 6 || strcmp (&tmpl[len - 6], "XXXXXX"))
    return -1;

  /* This is where the Xs start.  */
  XXXXXX = &tmpl[len - 6];

  /* Get some more or less random data.  */
  g_get_current_time (&tv);
  value = (tv.tv_usec ^ tv.tv_sec) + counter++;

  for (count = 0; count < 100; value += 7777, ++count)
    {
      glong v = value;

      /* Fill in the random bits.  */
      XXXXXX[0] = letters[v % NLETTERS];
      v /= NLETTERS;
      XXXXXX[1] = letters[v % NLETTERS];
      v /= NLETTERS;
      XXXXXX[2] = letters[v % NLETTERS];
      v /= NLETTERS;
      XXXXXX[3] = letters[v % NLETTERS];
      v /= NLETTERS;
      XXXXXX[4] = letters[v % NLETTERS];
      v /= NLETTERS;
      XXXXXX[5] = letters[v % NLETTERS];

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

/**
 * g_file_open_tmp:
 * @tmpl: Template for file name, as in g_mkstemp(), basename only
 * @name_used: location to store actual name used
 * @error: return location for a #GError
 *
 * Opens a file for writing in the preferred directory for temporary
 * files (as returned by g_get_tmp_dir()). 
 *
 * @tmpl should be a string ending with six 'X' characters, as the
 * parameter to g_mkstemp() (or mkstemp()). 
 * However, unlike these functions, the template should only be a 
 * basename, no directory components are allowed. If template is %NULL, 
 * a default template is used.
 *
 * Note that in contrast to g_mkstemp() (and mkstemp()) 
 * @tmpl is not modified, and might thus be a read-only literal string.
 *
 * The actual name used is returned in @name_used if non-%NULL. This
 * string should be freed with g_free() when not needed any longer.
 *
 * Return value: A file handle (as from open()) to 
 * the file opened for reading and writing. The file is opened in binary 
 * mode on platforms where there is a difference. The file handle should be
 * closed with close(). In case of errors, -1 is returned 
 * and @error will be set.
 **/
gint
g_file_open_tmp (const gchar *tmpl,
		 gchar      **name_used,
		 GError     **error)
{
  int retval;
  const char *tmpdir;
  char *sep;
  char *fulltemplate;
  const char *slash;

  if (tmpl == NULL)
    tmpl = ".XXXXXX";

  if ((slash = strchr (tmpl, G_DIR_SEPARATOR)) != NULL
#ifdef G_OS_WIN32
      || (strchr (tmpl, '/') != NULL && (slash = "/"))
#endif
      )
    {
      char c[2];
      c[0] = *slash;
      c[1] = '\0';

      g_set_error (error,
		   G_FILE_ERROR,
		   G_FILE_ERROR_FAILED,
		   _("Template '%s' invalid, should not contain a '%s'"),
		   tmpl, c);

      return -1;
    }
  
  if (strlen (tmpl) < 6 ||
      strcmp (tmpl + strlen (tmpl) - 6, "XXXXXX") != 0)
    {
      g_set_error (error,
		   G_FILE_ERROR,
		   G_FILE_ERROR_FAILED,
		   _("Template '%s' doesn't end with XXXXXX"),
		   tmpl);
      return -1;
    }

  tmpdir = g_get_tmp_dir ();

  if (G_IS_DIR_SEPARATOR (tmpdir [strlen (tmpdir) - 1]))
    sep = "";
  else
    sep = G_DIR_SEPARATOR_S;

  fulltemplate = g_strconcat (tmpdir, sep, tmpl, NULL);

  retval = g_mkstemp (fulltemplate);

  if (retval == -1)
    {
      g_set_error (error,
		   G_FILE_ERROR,
		   g_file_error_from_errno (errno),
		   _("Failed to create file '%s': %s"),
		   fulltemplate, g_strerror (errno));
      g_free (fulltemplate);
      return -1;
    }

  if (name_used)
    *name_used = fulltemplate;
  else
    g_free (fulltemplate);

  return retval;
}

static gchar *
g_build_pathv (const gchar *separator,
	       const gchar *first_element,
	       va_list      args)
{
  GString *result;
  gint separator_len = strlen (separator);
  gboolean is_first = TRUE;
  gboolean have_leading = FALSE;
  const gchar *single_element = NULL;
  const gchar *next_element;
  const gchar *last_trailing = NULL;

  result = g_string_new (NULL);

  next_element = first_element;

  while (TRUE)
    {
      const gchar *element;
      const gchar *start;
      const gchar *end;

      if (next_element)
	{
	  element = next_element;
	  next_element = va_arg (args, gchar *);
	}
      else
	break;

      /* Ignore empty elements */
      if (!*element)
	continue;
      
      start = element;

      if (separator_len)
	{
	  while (start &&
		 strncmp (start, separator, separator_len) == 0)
	    start += separator_len;
      	}

      end = start + strlen (start);
      
      if (separator_len)
	{
	  while (end >= start + separator_len &&
		 strncmp (end - separator_len, separator, separator_len) == 0)
	    end -= separator_len;
	  
	  last_trailing = end;
	  while (last_trailing >= element + separator_len &&
		 strncmp (last_trailing - separator_len, separator, separator_len) == 0)
	    last_trailing -= separator_len;

	  if (!have_leading)
	    {
	      /* If the leading and trailing separator strings are in the
	       * same element and overlap, the result is exactly that element
	       */
	      if (last_trailing <= start)
		single_element = element;
		  
	      g_string_append_len (result, element, start - element);
	      have_leading = TRUE;
	    }
	  else
	    single_element = NULL;
	}

      if (end == start)
	continue;

      if (!is_first)
	g_string_append (result, separator);
      
      g_string_append_len (result, start, end - start);
      is_first = FALSE;
    }

  if (single_element)
    {
      g_string_free (result, TRUE);
      return g_strdup (single_element);
    }
  else
    {
      if (last_trailing)
	g_string_append (result, last_trailing);
  
      return g_string_free (result, FALSE);
    }
}

/**
 * g_build_path:
 * @separator: a string used to separator the elements of the path.
 * @first_element: the first element in the path
 * @Varargs: remaining elements in path, terminated by %NULL
 * 
 * Creates a path from a series of elements using @separator as the
 * separator between elements. At the boundary between two elements,
 * any trailing occurrences of separator in the first element, or
 * leading occurrences of separator in the second element are removed
 * and exactly one copy of the separator is inserted.
 *
 * Empty elements are ignored.
 *
 * The number of leading copies of the separator on the result is
 * the same as the number of leading copies of the separator on
 * the first non-empty element.
 *
 * The number of trailing copies of the separator on the result is
 * the same as the number of trailing copies of the separator on
 * the last non-empty element. (Determination of the number of
 * trailing copies is done without stripping leading copies, so
 * if the separator is <literal>ABA</literal>, <literal>ABABA</literal>
 * has 1 trailing copy.)
 *
 * However, if there is only a single non-empty element, and there
 * are no characters in that element not part of the leading or
 * trailing separators, then the result is exactly the original value
 * of that element.
 *
 * Other than for determination of the number of leading and trailing
 * copies of the separator, elements consisting only of copies
 * of the separator are ignored.
 * 
 * Return value: a newly-allocated string that must be freed with g_free().
 **/
gchar *
g_build_path (const gchar *separator,
	      const gchar *first_element,
	      ...)
{
  gchar *str;
  va_list args;

  g_return_val_if_fail (separator != NULL, NULL);

  va_start (args, first_element);
  str = g_build_pathv (separator, first_element, args);
  va_end (args);

  return str;
}

/**
 * g_build_filename:
 * @first_element: the first element in the path
 * @Varargs: remaining elements in path, terminated by %NULL
 * 
 * Creates a filename from a series of elements using the correct
 * separator for filenames.
 *
 * On Unix, this function behaves identically to <literal>g_build_path
 * (G_DIR_SEPARATOR_S, first_element, ....)</literal>.
 *
 * On Windows, it takes into account that either the backslash
 * (<literal>\</literal> or slash (<literal>/</literal>) can be used
 * as separator in filenames, but otherwise behaves as on Unix. When
 * file pathname separators need to be inserted, the one that last
 * previously occurred in the parameters (reading from left to right)
 * is used.
 *
 * No attempt is made to force the resulting filename to be an absolute
 * path. If the first element is a relative path, the result will
 * be a relative path. 
 * 
 * Return value: a newly-allocated string that must be freed with g_free().
 **/
gchar *
g_build_filename (const gchar *first_element, 
		  ...)
{
#ifndef G_OS_WIN32
  gchar *str;
  va_list args;

  va_start (args, first_element);
  str = g_build_pathv (G_DIR_SEPARATOR_S, first_element, args);
  va_end (args);

  return str;
#else
  /* Code copied from g_build_pathv(), and modifed to use two
   * alternative single-character separators.
   */
  va_list args;
  GString *result;
  gboolean is_first = TRUE;
  gboolean have_leading = FALSE;
  const gchar *single_element = NULL;
  const gchar *next_element;
  const gchar *last_trailing = NULL;
  gchar current_separator = '\\';

  va_start (args, first_element);

  result = g_string_new (NULL);

  next_element = first_element;

  while (TRUE)
    {
      const gchar *element;
      const gchar *start;
      const gchar *end;

      if (next_element)
	{
	  element = next_element;
	  next_element = va_arg (args, gchar *);
	}
      else
	break;

      /* Ignore empty elements */
      if (!*element)
	continue;
      
      start = element;

      if (TRUE)
	{
	  while (start &&
		 (*start == '\\' || *start == '/'))
	    {
	      current_separator = *start;
	      start++;
	    }
	}

      end = start + strlen (start);
      
      if (TRUE)
	{
	  while (end >= start + 1 &&
		 (end[-1] == '\\' || end[-1] == '/'))
	    {
	      current_separator = end[-1];
	      end--;
	    }
	  
	  last_trailing = end;
	  while (last_trailing >= element + 1 &&
		 (last_trailing[-1] == '\\' || last_trailing[-1] == '/'))
	    last_trailing--;

	  if (!have_leading)
	    {
	      /* If the leading and trailing separator strings are in the
	       * same element and overlap, the result is exactly that element
	       */
	      if (last_trailing <= start)
		single_element = element;
		  
	      g_string_append_len (result, element, start - element);
	      have_leading = TRUE;
	    }
	  else
	    single_element = NULL;
	}

      if (end == start)
	continue;

      if (!is_first)
	g_string_append_len (result, &current_separator, 1);
      
      g_string_append_len (result, start, end - start);
      is_first = FALSE;
    }

  va_end (args);

  if (single_element)
    {
      g_string_free (result, TRUE);
      return g_strdup (single_element);
    }
  else
    {
      if (last_trailing)
	g_string_append (result, last_trailing);
  
      return g_string_free (result, FALSE);
    }
#endif
}

/**
 * g_file_read_link:
 * @filename: the symbolic link
 * @error: return location for a #GError
 *
 * Reads the contents of the symbolic link @filename like the POSIX readlink() function.
 * The returned string is in the encoding used for filenames. Use g_filename_to_utf8() to 
 * convert it to UTF-8.
 *
 * Returns: A newly allocated string with the contents of the symbolic link, 
 *          or %NULL if an error occurred.
 *
 * Since: 2.4
 */
gchar *
g_file_read_link (const gchar *filename,
	          GError     **error)
{
#ifdef HAVE_READLINK
  gchar *buffer;
  guint size;
  gint read_size;    
  
  size = 256; 
  buffer = g_malloc (size);
  
  while (TRUE) 
    {
      read_size = readlink (filename, buffer, size);
      if (read_size < 0) {
	g_free (buffer);
	g_set_error (error,
		     G_FILE_ERROR,
		     g_file_error_from_errno (errno),
		     _("Failed to read the symbolic link '%s': %s"),
		     filename, g_strerror (errno));
	
	return NULL;
      }
    
      if (read_size < size) 
	{
	  buffer[read_size] = 0;
	  return buffer;
	}
      
      size *= 2;
      buffer = g_realloc (buffer, size);
    }
#else
  g_set_error (error,
	       G_FILE_ERROR,
	       G_FILE_ERROR_INVAL,
	       _("Symbolic links not supported"));
	
  return NULL;
#endif
}
