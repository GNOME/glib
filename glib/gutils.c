/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1998  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GLib Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GLib Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GLib at ftp://ftp.gtk.org/pub/gtk/. 
 */

/* 
 * MT safe for the unix part, FIXME: make the win32 part MT safe as well.
 */

#include "config.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#ifdef HAVE_PWD_H
#include <pwd.h>
#endif
#include <sys/types.h>
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif

/* implement gutils's inline functions
 */
#define	G_IMPLEMENT_INLINES 1
#define	__G_UTILS_C__
#include "glib.h"
#include "gprintfint.h"

#ifdef	MAXPATHLEN
#define	G_PATH_LENGTH	MAXPATHLEN
#elif	defined (PATH_MAX)
#define	G_PATH_LENGTH	PATH_MAX
#elif   defined (_PC_PATH_MAX)
#define	G_PATH_LENGTH	sysconf(_PC_PATH_MAX)
#else	
#define G_PATH_LENGTH   2048
#endif

#ifdef G_PLATFORM_WIN32
#  define STRICT		/* Strict typing, please */
#  include <windows.h>
#  undef STRICT
#  include <lmcons.h>		/* For UNLEN */
#  include <ctype.h>
#endif /* G_PLATFORM_WIN32 */

#ifdef G_OS_WIN32
#  include <direct.h>
#endif

#ifdef HAVE_CODESET
#include <langinfo.h>
#endif

#ifdef HAVE_BIND_TEXTDOMAIN_CODESET
#include <libintl.h>
#endif

/* G_IS_DIR_SEPARATOR probably should be made public in GLib 2.4 */
#ifdef G_OS_WIN32
#define G_IS_DIR_SEPARATOR(c) (c == G_DIR_SEPARATOR || c == '/')
#else
#define G_IS_DIR_SEPARATOR(c) (c == G_DIR_SEPARATOR)
#endif

const guint glib_major_version = GLIB_MAJOR_VERSION;
const guint glib_minor_version = GLIB_MINOR_VERSION;
const guint glib_micro_version = GLIB_MICRO_VERSION;
const guint glib_interface_age = GLIB_INTERFACE_AGE;
const guint glib_binary_age = GLIB_BINARY_AGE;

#if !defined (HAVE_MEMMOVE) && !defined (HAVE_WORKING_BCOPY)
void 
g_memmove (gpointer dest, gconstpointer src, gulong len)
{
  gchar* destptr = dest;
  const gchar* srcptr = src;
  if (src + len < dest || dest + len < src)
    {
      bcopy (src, dest, len);
      return;
    }
  else if (dest <= src)
    {
      while (len--)
	*(destptr++) = *(srcptr++);
    }
  else
    {
      destptr += len;
      srcptr += len;
      while (len--)
	*(--destptr) = *(--srcptr);
    }
}
#endif /* !HAVE_MEMMOVE && !HAVE_WORKING_BCOPY */

void
g_atexit (GVoidFunc func)
{
  gint result;
  const gchar *error = NULL;

  /* keep this in sync with glib.h */

#ifdef	G_NATIVE_ATEXIT
  result = ATEXIT (func);
  if (result)
    error = g_strerror (errno);
#elif defined (HAVE_ATEXIT)
#  ifdef NeXT /* @#%@! NeXTStep */
  result = !atexit ((void (*)(void)) func);
  if (result)
    error = g_strerror (errno);
#  else
  result = atexit ((void (*)(void)) func);
  if (result)
    error = g_strerror (errno);
#  endif /* NeXT */
#elif defined (HAVE_ON_EXIT)
  result = on_exit ((void (*)(int, void *)) func, NULL);
  if (result)
    error = g_strerror (errno);
#else
  result = 0;
  error = "no implementation";
#endif /* G_NATIVE_ATEXIT */

  if (error)
    g_error ("Could not register atexit() function: %s", error);
}

/* Based on execvp() from GNU Libc.
 * Some of this code is cut-and-pasted into gspawn.c
 */

static gchar*
my_strchrnul (const gchar *str, gchar c)
{
  gchar *p = (gchar*)str;
  while (*p && (*p != c))
    ++p;

  return p;
}

#ifdef G_OS_WIN32

static gchar *inner_find_program_in_path (const gchar *program);

gchar*
g_find_program_in_path (const gchar *program)
{
  const gchar *last_dot = strrchr (program, '.');

  if (last_dot == NULL || strchr (last_dot, '\\') != NULL)
    {
      const gint program_length = strlen (program);
      const gchar *pathext = getenv ("PATHEXT");
      const gchar *p;
      gchar *decorated_program;
      gchar *retval;

      if (pathext == NULL)
	pathext = ".com;.exe;.bat";

      p = pathext;
      do
	{
	  pathext = p;
	  p = my_strchrnul (pathext, ';');

	  decorated_program = g_malloc (program_length + (p-pathext) + 1);
	  memcpy (decorated_program, program, program_length);
	  memcpy (decorated_program+program_length, pathext, p-pathext);
	  decorated_program [program_length + (p-pathext)] = '\0';
	  
	  retval = inner_find_program_in_path (decorated_program);
	  g_free (decorated_program);

	  if (retval != NULL)
	    return retval;
	} while (*p++ != '\0');
      return NULL;
    }
  else
    return inner_find_program_in_path (program);
}

#define g_find_program_in_path inner_find_program_in_path
#endif

/**
 * g_find_program_in_path:
 * @program: a program name
 * 
 * Locates the first executable named @program in the user's path, in the
 * same way that execvp() would locate it. Returns an allocated string
 * with the absolute path name, or NULL if the program is not found in
 * the path. If @program is already an absolute path, returns a copy of
 * @program if @program exists and is executable, and NULL otherwise.
 * 
 * On Windows, if @program does not have a file type suffix, tries to
 * append the suffixes in the PATHEXT environment variable (if that
 * doesn't exists, the suffixes .com, .exe, and .bat) in turn, and
 * then look for the resulting file name in the same way as
 * CreateProcess() would. This means first in the directory where the
 * program was loaded from, then in the current directory, then in the
 * Windows 32-bit system directory, then in the Windows directory, and
 * finally in the directories in the PATH environment variable. If
 * the program is found, the return value contains the full name
 * including the type suffix.
 *
 * Return value: absolute path, or NULL
 **/
#ifdef G_OS_WIN32
static
#endif
gchar*
g_find_program_in_path (const gchar *program)
{
  const gchar *path, *p;
  gchar *name, *freeme;
#ifdef G_OS_WIN32
  gchar *path_tmp;
#endif
  size_t len;
  size_t pathlen;

  g_return_val_if_fail (program != NULL, NULL);

  /* If it is an absolute path, or a relative path including subdirectories,
   * don't look in PATH.
   */
  if (g_path_is_absolute (program)
      || strchr (program, G_DIR_SEPARATOR) != NULL
#ifdef G_OS_WIN32
      || strchr (program, '/') != NULL
#endif
      )
    {
      if (g_file_test (program, G_FILE_TEST_IS_EXECUTABLE))
        return g_strdup (program);
      else
        return NULL;
    }
  
  path = g_getenv ("PATH");
#ifdef G_OS_UNIX
  if (path == NULL)
    {
      /* There is no `PATH' in the environment.  The default
       * search path in GNU libc is the current directory followed by
       * the path `confstr' returns for `_CS_PATH'.
       */
      
      /* In GLib we put . last, for security, and don't use the
       * unportable confstr(); UNIX98 does not actually specify
       * what to search if PATH is unset. POSIX may, dunno.
       */
      
      path = "/bin:/usr/bin:.";
    }
#else
  {
    gchar *tmp;
    gchar moddir[MAXPATHLEN], sysdir[MAXPATHLEN], windir[MAXPATHLEN];

    GetModuleFileName (NULL, moddir, sizeof (moddir));
    tmp = g_path_get_dirname (moddir);
    GetSystemDirectory (sysdir, sizeof (sysdir));
    GetWindowsDirectory (windir, sizeof (windir));
    path_tmp = g_strconcat (tmp, ";.;", sysdir, ";", windir,
			    (path != NULL ? ";" : NULL),
			    (path != NULL ? path : NULL),
			    NULL);
    g_free (tmp);
    path = path_tmp;
  }
#endif
  
  len = strlen (program) + 1;
  pathlen = strlen (path);
  freeme = name = g_malloc (pathlen + len + 1);
  
  /* Copy the file name at the top, including '\0'  */
  memcpy (name + pathlen + 1, program, len);
  name = name + pathlen;
  /* And add the slash before the filename  */
  *name = G_DIR_SEPARATOR;
  
  p = path;
  do
    {
      char *startp;

      path = p;
      p = my_strchrnul (path, G_SEARCHPATH_SEPARATOR);

      if (p == path)
        /* Two adjacent colons, or a colon at the beginning or the end
         * of `PATH' means to search the current directory.
         */
        startp = name + 1;
      else
        startp = memcpy (name - (p - path), path, p - path);

      if (g_file_test (startp, G_FILE_TEST_IS_EXECUTABLE))
        {
          gchar *ret;
          ret = g_strdup (startp);
          g_free (freeme);
#ifdef G_OS_WIN32
	  g_free (path_tmp);
#endif
          return ret;
        }
    }
  while (*p++ != '\0');
  
  g_free (freeme);
#ifdef G_OS_WIN32
  g_free (path_tmp);
#endif

  return NULL;
}

guint	     
g_parse_debug_string  (const gchar     *string, 
		       const GDebugKey *keys, 
		       guint	        nkeys)
{
  guint i;
  guint result = 0;
  
  g_return_val_if_fail (string != NULL, 0);
  
  if (!g_ascii_strcasecmp (string, "all"))
    {
      for (i=0; i<nkeys; i++)
	result |= keys[i].value;
    }
  else
    {
      const gchar *p = string;
      const gchar *q;
      gboolean done = FALSE;
      
      while (*p && !done)
	{
	  q = strchr (p, ':');
	  if (!q)
	    {
	      q = p + strlen(p);
	      done = TRUE;
	    }
	  
	  for (i=0; i<nkeys; i++)
	    if (g_ascii_strncasecmp(keys[i].key, p, q - p) == 0 &&
		keys[i].key[q - p] == '\0')
	      result |= keys[i].value;
	  
	  p = q + 1;
	}
    }
  
  return result;
}

/**
 * g_basename:
 * @file_name: the name of the file.
 * 
 * Gets the name of the file without any leading directory components.  
 * It returns a pointer into the given file name string.
 * 
 * Return value: the name of the file without any leading directory components.
 *
 * Deprecated: Use g_path_get_basename() instead, but notice that
 * g_path_get_basename() allocates new memory for the returned string, unlike
 * this function which returns a pointer into the argument.
 **/
G_CONST_RETURN gchar*
g_basename (const gchar	   *file_name)
{
  register gchar *base;
  
  g_return_val_if_fail (file_name != NULL, NULL);
  
  base = strrchr (file_name, G_DIR_SEPARATOR);

#ifdef G_OS_WIN32
  {
    gchar *q = strrchr (file_name, '/');
    if (base == NULL || (q != NULL && q > base))
	base = q;
  }
#endif

  if (base)
    return base + 1;

#ifdef G_OS_WIN32
  if (g_ascii_isalpha (file_name[0]) && file_name[1] == ':')
    return (gchar*) file_name + 2;
#endif /* G_OS_WIN32 */
  
  return (gchar*) file_name;
}

/**
 * g_path_get_basename:
 * @file_name: the name of the file.
 *
 * Gets the last component of the filename. If @file_name ends with a 
 * directory separator it gets the component before the last slash. If 
 * @file_name consists only of directory separators (and on Windows, 
 * possibly a drive letter), a single separator is returned. If
 * @file_name is empty, it gets ".".
 *
 * Return value: a newly allocated string containing the last component of 
 *   the filename.
 */
gchar*
g_path_get_basename (const gchar   *file_name)
{
  register gssize base;             
  register gssize last_nonslash;    
  gsize len;    
  gchar *retval;
 
  g_return_val_if_fail (file_name != NULL, NULL);

  if (file_name[0] == '\0')
    /* empty string */
    return g_strdup (".");
  
  last_nonslash = strlen (file_name) - 1;

  while (last_nonslash >= 0 && G_IS_DIR_SEPARATOR (file_name [last_nonslash]))
    last_nonslash--;

  if (last_nonslash == -1)
    /* string only containing slashes */
    return g_strdup (G_DIR_SEPARATOR_S);

#ifdef G_OS_WIN32
  if (last_nonslash == 1 && g_ascii_isalpha (file_name[0]) && file_name[1] == ':')
    /* string only containing slashes and a drive */
    return g_strdup (G_DIR_SEPARATOR_S);
#endif /* G_OS_WIN32 */

  base = last_nonslash;

  while (base >=0 && !G_IS_DIR_SEPARATOR (file_name [base]))
    base--;

#ifdef G_OS_WIN32
  if (base == -1 && g_ascii_isalpha (file_name[0]) && file_name[1] == ':')
    base = 1;
#endif /* G_OS_WIN32 */

  len = last_nonslash - base;
  retval = g_malloc (len + 1);
  memcpy (retval, file_name + base + 1, len);
  retval [len] = '\0';
  return retval;
}

gboolean
g_path_is_absolute (const gchar *file_name)
{
  g_return_val_if_fail (file_name != NULL, FALSE);
  
  if (G_IS_DIR_SEPARATOR (file_name[0]))
    return TRUE;

#ifdef G_OS_WIN32
  /* Recognize drive letter on native Windows */
  if (g_ascii_isalpha (file_name[0]) && file_name[1] == ':' && G_IS_DIR_SEPARATOR (file_name[2]))
    return TRUE;
#endif /* G_OS_WIN32 */

  return FALSE;
}

G_CONST_RETURN gchar*
g_path_skip_root (const gchar *file_name)
{
  g_return_val_if_fail (file_name != NULL, NULL);
  
#ifdef G_PLATFORM_WIN32
  /* Skip \\server\share or //server/share */
  if (G_IS_DIR_SEPARATOR (file_name[0]) &&
      G_IS_DIR_SEPARATOR (file_name[1]) &&
      file_name[2])
    {
      gchar *p;

      p = strchr (file_name + 2, G_DIR_SEPARATOR);
#ifdef G_OS_WIN32
      {
	gchar *q = strchr (file_name + 2, '/');
	if (p == NULL || (q != NULL && q < p))
	  p = q;
      }
#endif
      if (p &&
	  p > file_name + 2 &&
	  p[1])
	{
	  file_name = p + 1;

	  while (file_name[0] && !G_IS_DIR_SEPARATOR (file_name[0]))
	    file_name++;

	  /* Possibly skip a backslash after the share name */
	  if (G_IS_DIR_SEPARATOR (file_name[0]))
	    file_name++;

	  return (gchar *)file_name;
	}
    }
#endif
  
  /* Skip initial slashes */
  if (G_IS_DIR_SEPARATOR (file_name[0]))
    {
      while (G_IS_DIR_SEPARATOR (file_name[0]))
	file_name++;
      return (gchar *)file_name;
    }

#ifdef G_OS_WIN32
  /* Skip X:\ */
  if (g_ascii_isalpha (file_name[0]) && file_name[1] == ':' && G_IS_DIR_SEPARATOR (file_name[2]))
    return (gchar *)file_name + 3;
#endif

  return NULL;
}

gchar*
g_path_get_dirname (const gchar	   *file_name)
{
  register gchar *base;
  register gsize len;    
  
  g_return_val_if_fail (file_name != NULL, NULL);
  
  base = strrchr (file_name, G_DIR_SEPARATOR);
#ifdef G_OS_WIN32
  {
    gchar *q = strrchr (file_name, '/');
    if (base == NULL || (q != NULL && q > base))
	base = q;
  }
#endif
  if (!base)
    {
#ifdef G_OS_WIN32
      if (g_ascii_isalpha (file_name[0]) && file_name[1] == ':')
	{
	  gchar drive_colon_dot[4];

	  drive_colon_dot[0] = file_name[0];
	  drive_colon_dot[1] = ':';
	  drive_colon_dot[2] = '.';
	  drive_colon_dot[3] = '\0';

	  return g_strdup (drive_colon_dot);
	}
#endif
    return g_strdup (".");
    }

  while (base > file_name && G_IS_DIR_SEPARATOR (*base))
    base--;

#ifdef G_OS_WIN32
  if (base == file_name + 1 && g_ascii_isalpha (file_name[0]) && file_name[1] == ':')
      base++;
#endif

  len = (guint) 1 + base - file_name;
  
  base = g_new (gchar, len + 1);
  g_memmove (base, file_name, len);
  base[len] = 0;
  
  return base;
}

gchar*
g_get_current_dir (void)
{
  gchar *buffer = NULL;
  gchar *dir = NULL;
  static gulong max_len = 0;

  if (max_len == 0) 
    max_len = (G_PATH_LENGTH == -1) ? 2048 : G_PATH_LENGTH;
  
  /* We don't use getcwd(3) on SUNOS, because, it does a popen("pwd")
   * and, if that wasn't bad enough, hangs in doing so.
   */
#if	(defined (sun) && !defined (__SVR4)) || !defined(HAVE_GETCWD)
  buffer = g_new (gchar, max_len + 1);
  *buffer = 0;
  dir = getwd (buffer);
#else	/* !sun || !HAVE_GETCWD */
  while (max_len < G_MAXULONG / 2)
    {
      buffer = g_new (gchar, max_len + 1);
      *buffer = 0;
      dir = getcwd (buffer, max_len);

      if (dir || errno != ERANGE)
	break;

      g_free (buffer);
      max_len *= 2;
    }
#endif	/* !sun || !HAVE_GETCWD */
  
  if (!dir || !*buffer)
    {
      /* hm, should we g_error() out here?
       * this can happen if e.g. "./" has mode \0000
       */
      buffer[0] = G_DIR_SEPARATOR;
      buffer[1] = 0;
    }

  dir = g_strdup (buffer);
  g_free (buffer);
  
  return dir;
}

/**
 * g_getenv:
 * @variable: the environment variable to get.
 * 
 * Returns an environment variable.
 * 
 * Return value: the value of the environment variable, or %NULL if the environment
 * variable is not found. The returned string may be overwritten by the next call to g_getenv(),
 * g_setenv() or g_unsetenv().
 **/
G_CONST_RETURN gchar*
g_getenv (const gchar *variable)
{
#ifndef G_OS_WIN32
  g_return_val_if_fail (variable != NULL, NULL);

  return getenv (variable);
#else
  GQuark quark;
  gchar *system_env;
  gchar *expanded_env;
  guint length;
  gchar dummy[2];

  g_return_val_if_fail (variable != NULL, NULL);
  
  system_env = getenv (variable);
  if (!system_env)
    return NULL;

  /* On Windows NT, it is relatively typical that environment
   * variables contain references to other environment variables. If
   * so, use ExpandEnvironmentStrings(). (If all software was written
   * in the best possible way, such environment variables would be
   * stored in the Registry as REG_EXPAND_SZ type values, and would
   * then get automatically expanded before the program sees them. But
   * there is broken software that stores environment variables as
   * REG_SZ values even if they contain references to other
   * environment variables.
   */

  if (strchr (system_env, '%') == NULL)
    {
      /* No reference to other variable(s), return value as such. */
      return system_env;
    }

  /* First check how much space we need */
  length = ExpandEnvironmentStrings (system_env, dummy, 2);
  
  expanded_env = g_malloc (length);
  
  ExpandEnvironmentStrings (system_env, expanded_env, length);
  
  quark = g_quark_from_string (expanded_env);
  g_free (expanded_env);
  
  return g_quark_to_string (quark);
#endif
}

/**
 * g_setenv:
 * @variable: the environment variable to set, must not contain '='.
 * @value: the value for to set the variable to.
 * @overwrite: whether to change the variable if it already exists.
 *
 * Sets an environment variable.
 *
 * Note that on some systems, the memory used for the variable and its value 
 * can't be reclaimed later.
 *
 * Returns: %FALSE if the environment variable couldn't be set.
 *
 * Since: 2.4
 */
gboolean
g_setenv (const gchar *variable, 
	  const gchar *value, 
	  gboolean     overwrite)
{
  gint result;
#ifndef HAVE_SETENV
  gchar *string;
#endif

  g_return_val_if_fail (strchr (variable, '=') == NULL, FALSE);

#ifdef HAVE_SETENV
  result = setenv (variable, value, overwrite);
#else
  if (!overwrite && g_getenv (variable) != NULL)
    return TRUE;
  
  /* This results in a leak when you overwrite existing
   * settings. It would be fairly easy to fix this by keeping
   * our own parallel array or hash table.
   */
  string = g_strconcat (variable, "=", value, NULL);
  result = putenv (string);
#endif
  return result == 0;
}

#ifndef HAVE_UNSETENV     
/* According to the Single Unix Specification, environ is not in 
 * any system header, although unistd.h often declares it.
 */
extern char **environ;
#endif
           
/**
 * g_unsetenv:
 * @variable: the environment variable to remove, must not contain '='.
 * 
 * Removes an environment variable from the environment.
 *
 * Note that on some systems, the memory used for the variable and its value 
 * can't be reclaimed. Furthermore, this function can't be guaranteed to operate in a 
 * threadsafe way.
 *
 * Since: 2.4 
 **/
void
g_unsetenv (const gchar *variable)
{
#ifdef HAVE_UNSETENV
  g_return_if_fail (strchr (variable, '=') == NULL);

  unsetenv (variable);
#else
  int len;
  gchar **e, **f;

  g_return_if_fail (strchr (variable, '=') == NULL);

  len = strlen (variable);
  
  /* Mess directly with the environ array.
   * This seems to be the only portable way to do this.
   *
   * Note that we remove *all* environment entries for
   * the variable name, not just the first.
   */
  e = f = environ;
  while (*e != NULL) 
    {
      if (strncmp (*e, variable, len) != 0 || (*e)[len] != '=') 
	{
	  *f = *e;
	  f++;
	}
      e++;
    }
  *f = NULL;
#endif
}

G_LOCK_DEFINE_STATIC (g_utils_global);

static	gchar	*g_tmp_dir = NULL;
static	gchar	*g_user_name = NULL;
static	gchar	*g_real_name = NULL;
static	gchar	*g_home_dir = NULL;

/* HOLDS: g_utils_global_lock */
static void
g_get_any_init (void)
{
  if (!g_tmp_dir)
    {
      g_tmp_dir = g_strdup (g_getenv ("TMPDIR"));
      if (!g_tmp_dir)
	g_tmp_dir = g_strdup (g_getenv ("TMP"));
      if (!g_tmp_dir)
	g_tmp_dir = g_strdup (g_getenv ("TEMP"));
      
#ifdef P_tmpdir
      if (!g_tmp_dir)
	{
	  gsize k;    
	  g_tmp_dir = g_strdup (P_tmpdir);
	  k = strlen (g_tmp_dir);
	  if (k > 1 && G_IS_DIR_SEPARATOR (g_tmp_dir[k - 1]))
	    g_tmp_dir[k - 1] = '\0';
	}
#endif
      
      if (!g_tmp_dir)
	{
#ifndef G_OS_WIN32
	  g_tmp_dir = g_strdup ("/tmp");
#else /* G_OS_WIN32 */
	  g_tmp_dir = g_strdup ("C:\\");
#endif /* G_OS_WIN32 */
	}
      
#ifdef G_OS_WIN32
      /* We check $HOME first for Win32, though it is a last resort for Unix
       * where we prefer the results of getpwuid().
       */
      g_home_dir = g_strdup (g_getenv ("HOME"));
      
      /* In case HOME is Unix-style (it happens), convert it to
       * Windows style.
       */
      if (g_home_dir)
	{
	  gchar *p;
	  while ((p = strchr (g_home_dir, '/')) != NULL)
	    *p = '\\';
	}

      if (!g_home_dir)
	{
	  /* USERPROFILE is probably the closest equivalent to $HOME? */
	  if (getenv ("USERPROFILE") != NULL)
	    g_home_dir = g_strdup (g_getenv ("USERPROFILE"));
	}

      if (!g_home_dir)
	{
	  /* At least at some time, HOMEDRIVE and HOMEPATH were used
	   * to point to the home directory, I think. But on Windows
	   * 2000 HOMEDRIVE seems to be equal to SYSTEMDRIVE, and
	   * HOMEPATH is its root "\"?
	   */
	  if (getenv ("HOMEDRIVE") != NULL && getenv ("HOMEPATH") != NULL)
	    {
	      gchar *homedrive, *homepath;
	      
	      homedrive = g_strdup (g_getenv ("HOMEDRIVE"));
	      homepath = g_strdup (g_getenv ("HOMEPATH"));
	      
	      g_home_dir = g_strconcat (homedrive, homepath, NULL);
	      g_free (homedrive);
	      g_free (homepath);
	    }
	}
#endif /* G_OS_WIN32 */
      
#ifdef HAVE_PWD_H
      {
	struct passwd *pw = NULL;
	gpointer buffer = NULL;
        gint error;
	
#  if defined (HAVE_POSIX_GETPWUID_R) || defined (HAVE_NONPOSIX_GETPWUID_R)
        struct passwd pwd;
#    ifdef _SC_GETPW_R_SIZE_MAX  
	/* This reurns the maximum length */
        glong bufsize = sysconf (_SC_GETPW_R_SIZE_MAX);
	
	if (bufsize < 0)
	  bufsize = 64;
#    else /* _SC_GETPW_R_SIZE_MAX */
        glong bufsize = 64;
#    endif /* _SC_GETPW_R_SIZE_MAX */
	
        do
          {
            g_free (buffer);
            buffer = g_malloc (bufsize);
	    errno = 0;
	    
#    ifdef HAVE_POSIX_GETPWUID_R
	    error = getpwuid_r (getuid (), &pwd, buffer, bufsize, &pw);
            error = error < 0 ? errno : error;
#    else /* HAVE_NONPOSIX_GETPWUID_R */
       /* HPUX 11 falls into the HAVE_POSIX_GETPWUID_R case */
#      if defined(_AIX) || defined(__hpux)
	    error = getpwuid_r (getuid (), &pwd, buffer, bufsize);
	    pw = error == 0 ? &pwd : NULL;
#      else /* !_AIX */
            pw = getpwuid_r (getuid (), &pwd, buffer, bufsize);
            error = pw ? 0 : errno;
#      endif /* !_AIX */            
#    endif /* HAVE_NONPOSIX_GETPWUID_R */
	    
	    if (!pw)
	      {
		/* we bail out prematurely if the user id can't be found
		 * (should be pretty rare case actually), or if the buffer
		 * should be sufficiently big and lookups are still not
		 * successfull.
		 */
		if (error == 0 || error == ENOENT)
		  {
		    g_warning ("getpwuid_r(): failed due to unknown user id (%lu)",
			       (gulong) getuid ());
		    break;
		  }
		if (bufsize > 32 * 1024)
		  {
		    g_warning ("getpwuid_r(): failed due to: %s.",
			       g_strerror (error));
		    break;
		  }
		
		bufsize *= 2;
	      }
	  }
	while (!pw);
#  endif /* HAVE_POSIX_GETPWUID_R || HAVE_NONPOSIX_GETPWUID_R */
	
	if (!pw)
	  {
	    setpwent ();
	    pw = getpwuid (getuid ());
	    endpwent ();
	  }
	if (pw)
	  {
 	    g_user_name = g_strdup (pw->pw_name);

	    if (pw->pw_gecos && *pw->pw_gecos != '\0') 
	      {
		gchar **gecos_fields;
		gchar **name_parts;

		/* split the gecos field and substitute '&' */
		gecos_fields = g_strsplit (pw->pw_gecos, ",", 0);
		name_parts = g_strsplit (gecos_fields[0], "&", 0);
		pw->pw_name[0] = g_ascii_toupper (pw->pw_name[0]);
		g_real_name = g_strjoinv (pw->pw_name, name_parts);
		g_strfreev (gecos_fields);
		g_strfreev (name_parts);
	      }

	    if (!g_home_dir)
	      g_home_dir = g_strdup (pw->pw_dir);
	  }
	g_free (buffer);
      }
      
#else /* !HAVE_PWD_H */
      
#  ifdef G_OS_WIN32
      {
	guint len = UNLEN+1;
	gchar buffer[UNLEN+1];
	
	if (GetUserName ((LPTSTR) buffer, (LPDWORD) &len))
	  {
	    g_user_name = g_strdup (buffer);
	    g_real_name = g_strdup (buffer);
	  }
      }
#  endif /* G_OS_WIN32 */

#endif /* !HAVE_PWD_H */

      if (!g_home_dir)
	g_home_dir = g_strdup (g_getenv ("HOME"));
      
#ifdef __EMX__
      /* change '\\' in %HOME% to '/' */
      g_strdelimit (g_home_dir, "\\",'/');
#endif
      if (!g_user_name)
	g_user_name = g_strdup ("somebody");
      if (!g_real_name)
	g_real_name = g_strdup ("Unknown");
    }
}

G_CONST_RETURN gchar*
g_get_user_name (void)
{
  G_LOCK (g_utils_global);
  if (!g_tmp_dir)
    g_get_any_init ();
  G_UNLOCK (g_utils_global);
  
  return g_user_name;
}

G_CONST_RETURN gchar*
g_get_real_name (void)
{
  G_LOCK (g_utils_global);
  if (!g_tmp_dir)
    g_get_any_init ();
  G_UNLOCK (g_utils_global);
 
  return g_real_name;
}

/* Return the home directory of the user. If there is a HOME
 * environment variable, its value is returned, otherwise use some
 * system-dependent way of finding it out. If no home directory can be
 * deduced, return NULL.
 */

G_CONST_RETURN gchar*
g_get_home_dir (void)
{
  G_LOCK (g_utils_global);
  if (!g_tmp_dir)
    g_get_any_init ();
  G_UNLOCK (g_utils_global);
  
  return g_home_dir;
}

/* Return a directory to be used to store temporary files. This is the
 * value of the TMPDIR, TMP or TEMP environment variables (they are
 * checked in that order). If none of those exist, use P_tmpdir from
 * stdio.h.  If that isn't defined, return "/tmp" on POSIXly systems,
 * and C:\ on Windows.
 */

G_CONST_RETURN gchar*
g_get_tmp_dir (void)
{
  G_LOCK (g_utils_global);
  if (!g_tmp_dir)
    g_get_any_init ();
  G_UNLOCK (g_utils_global);
  
  return g_tmp_dir;
}

G_LOCK_DEFINE_STATIC (g_prgname);
static gchar *g_prgname = NULL;

gchar*
g_get_prgname (void)
{
  gchar* retval;

  G_LOCK (g_prgname);
  retval = g_prgname;
  G_UNLOCK (g_prgname);

  return retval;
}

void
g_set_prgname (const gchar *prgname)
{
  G_LOCK (g_prgname);
  g_free (g_prgname);
  g_prgname = g_strdup (prgname);
  G_UNLOCK (g_prgname);
}

G_LOCK_DEFINE_STATIC (g_application_name);
static gchar *g_application_name = NULL;

/**
 * g_get_application_name:
 * 
 * Gets a human-readable name for the application, as set by
 * g_set_application_name(). This name should be localized if
 * possible, and is intended for display to the user.  Contrast with
 * g_get_prgname(), which gets a non-localized name. If
 * g_set_application_name() has not been called, returns the result of
 * g_get_prgname() (which may be %NULL if g_set_prgname() has also not
 * been called).
 * 
 * Return value: human-readable application name. may return %NULL
 *
 * Since: 2.2
 **/
G_CONST_RETURN gchar*
g_get_application_name (void)
{
  gchar* retval;

  G_LOCK (g_application_name);
  retval = g_application_name;
  G_UNLOCK (g_application_name);

  if (retval == NULL)
    return g_get_prgname ();
  
  return retval;
}

/**
 * g_set_application_name:
 * @application_name: localized name of the application
 *
 * Sets a human-readable name for the application. This name should be
 * localized if possible, and is intended for display to the user.
 * Contrast with g_set_prgname(), which sets a non-localized name.
 * g_set_prgname() will be called automatically by gtk_init(),
 * but g_set_application_name() will not.
 *
 * Note that for thread safety reasons, this function can only
 * be called once.
 *
 * The application name will be used in contexts such as error messages,
 * or when displaying an application's name in the task list.
 * 
 **/
void
g_set_application_name (const gchar *application_name)
{
  gboolean already_set = FALSE;
	
  G_LOCK (g_application_name);
  if (g_application_name)
    already_set = TRUE;
  else
    g_application_name = g_strdup (application_name);
  G_UNLOCK (g_application_name);

  if (already_set)
    g_warning ("g_set_application() name called multiple times");
}

guint
g_direct_hash (gconstpointer v)
{
  return GPOINTER_TO_UINT (v);
}

gboolean
g_direct_equal (gconstpointer v1,
		gconstpointer v2)
{
  return v1 == v2;
}

gboolean
g_int_equal (gconstpointer v1,
	     gconstpointer v2)
{
  return *((const gint*) v1) == *((const gint*) v2);
}

guint
g_int_hash (gconstpointer v)
{
  return *(const gint*) v;
}

/**
 * g_nullify_pointer:
 * @nullify_location: the memory address of the pointer.
 * 
 * Set the pointer at the specified location to %NULL.
 **/
void
g_nullify_pointer (gpointer *nullify_location)
{
  g_return_if_fail (nullify_location != NULL);

  *nullify_location = NULL;
}

/**
 * g_get_codeset:
 * 
 * Get the codeset for the current locale.
 * 
 * Return value: a newly allocated string containing the name
 * of the codeset. This string must be freed with g_free().
 **/
gchar *
g_get_codeset (void)
{
  gchar *charset;

  g_get_charset (&charset);

  return charset;
}

#ifdef ENABLE_NLS

#include <libintl.h>

#ifdef G_PLATFORM_WIN32

G_WIN32_DLLMAIN_FOR_DLL_NAME (static, dll_name)

static const gchar *
_glib_get_locale_dir (void)
{
  static const gchar *cache = NULL;
  if (cache == NULL)
    cache = g_win32_get_package_installation_subdirectory
      (GETTEXT_PACKAGE, dll_name, "lib\\locale");

  return cache;
}

#undef GLIB_LOCALE_DIR
#define GLIB_LOCALE_DIR _glib_get_locale_dir ()

#endif /* G_PLATFORM_WIN32 */

G_CONST_RETURN gchar *
_glib_gettext (const gchar *str)
{
  static gboolean _glib_gettext_initialized = FALSE;

  if (!_glib_gettext_initialized)
    {
      bindtextdomain(GETTEXT_PACKAGE, GLIB_LOCALE_DIR);
#    ifdef HAVE_BIND_TEXTDOMAIN_CODESET
      bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
#    endif
      _glib_gettext_initialized = TRUE;
    }
  
  return dgettext (GETTEXT_PACKAGE, str);
}

#endif /* ENABLE_NLS */
