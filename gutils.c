/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1998  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GLib Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GLib Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GLib at ftp://ftp.gtk.org/pub/gtk/. 
 */

/* 
 * MT safe for the unix part, FIXME: make the win32 part MT safe as well.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "glibconfig.h"

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

#ifdef G_OS_WIN32
#  define STRICT			/* Strict typing, please */
#  include <windows.h>
#  include <direct.h>
#  include <errno.h>
#  include <ctype.h>
#  ifdef _MSC_VER
#    include <io.h>
#  endif /* _MSC_VER */
#endif /* G_OS_WIN32 */

/* implement Glib's inline functions
 */
#define	G_INLINE_FUNC extern
#define	G_CAN_INLINE 1
#include "glib.h"

#ifdef	MAXPATHLEN
#define	G_PATH_LENGTH	(MAXPATHLEN + 1)
#elif	defined (PATH_MAX)
#define	G_PATH_LENGTH	(PATH_MAX + 1)
#else	/* !MAXPATHLEN */
#define G_PATH_LENGTH   (2048 + 1)
#endif	/* !MAXPATHLEN && !PATH_MAX */

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
  gchar *error = NULL;

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

gint
g_snprintf (gchar	*str,
	    gulong	 n,
	    gchar const *fmt,
	    ...)
{
#ifdef	HAVE_VSNPRINTF
  va_list args;
  gint retval;
  
  va_start (args, fmt);
  retval = vsnprintf (str, n, fmt, args);
  va_end (args);

  if (retval < 0)
    {
      str[n-1] = '\0';
      retval = strlen (str);
    }

  return retval;
#else	/* !HAVE_VSNPRINTF */
  gchar *printed;
  va_list args;
  
  va_start (args, fmt);
  printed = g_strdup_vprintf (fmt, args);
  va_end (args);
  
  strncpy (str, printed, n);
  str[n-1] = '\0';

  g_free (printed);
  
  return strlen (str);
#endif	/* !HAVE_VSNPRINTF */
}

gint
g_vsnprintf (gchar	 *str,
	     gulong	  n,
	     gchar const *fmt,
	     va_list      args)
{
#ifdef	HAVE_VSNPRINTF
  gint retval;
  
  retval = vsnprintf (str, n, fmt, args);
  
  if (retval < 0)
    {
      str[n-1] = '\0';
      retval = strlen (str);
    }

  return retval;
#else	/* !HAVE_VSNPRINTF */
  gchar *printed;
  
  printed = g_strdup_vprintf (fmt, args);
  strncpy (str, printed, n);
  str[n-1] = '\0';

  g_free (printed);
  
  return strlen (str);
#endif /* !HAVE_VSNPRINTF */
}

guint	     
g_parse_debug_string  (const gchar *string, 
		       GDebugKey   *keys, 
		       guint	    nkeys)
{
  guint i;
  guint result = 0;
  
  g_return_val_if_fail (string != NULL, 0);
  
  if (!g_strcasecmp (string, "all"))
    {
      for (i=0; i<nkeys; i++)
	result |= keys[i].value;
    }
  else
    {
      gchar *str = g_strdup (string);
      gchar *p = str;
      gchar *q;
      gboolean done = FALSE;
      
      while (*p && !done)
	{
	  q = strchr (p, ':');
	  if (!q)
	    {
	      q = p + strlen(p);
	      done = TRUE;
	    }
	  
	  *q = 0;
	  
	  for (i=0; i<nkeys; i++)
	    if (!g_strcasecmp(keys[i].key, p))
	      result |= keys[i].value;
	  
	  p = q+1;
	}
      
      g_free (str);
    }
  
  return result;
}

gchar*
g_basename (const gchar	   *file_name)
{
  register gchar *base;
  
  g_return_val_if_fail (file_name != NULL, NULL);
  
  base = strrchr (file_name, G_DIR_SEPARATOR);
  if (base)
    return base + 1;

#ifdef G_OS_WIN32
  if (isalpha (file_name[0]) && file_name[1] == ':')
    return (gchar*) file_name + 2;
#endif /* G_OS_WIN32 */
  
  return (gchar*) file_name;
}

gboolean
g_path_is_absolute (const gchar *file_name)
{
  g_return_val_if_fail (file_name != NULL, FALSE);
  
  if (file_name[0] == G_DIR_SEPARATOR)
    return TRUE;

#ifdef G_OS_WIN32
  if (isalpha (file_name[0]) && file_name[1] == ':' && file_name[2] == G_DIR_SEPARATOR)
    return TRUE;
#endif

  return FALSE;
}

gchar*
g_path_skip_root (gchar *file_name)
{
  g_return_val_if_fail (file_name != NULL, NULL);
  
  if (file_name[0] == G_DIR_SEPARATOR)
    return file_name + 1;

#ifdef G_OS_WIN32
  if (isalpha (file_name[0]) && file_name[1] == ':' && file_name[2] == G_DIR_SEPARATOR)
    return file_name + 3;
#endif

  return NULL;
}

gchar*
g_dirname (const gchar	   *file_name)
{
  register gchar *base;
  register guint len;
  
  g_return_val_if_fail (file_name != NULL, NULL);
  
  base = strrchr (file_name, G_DIR_SEPARATOR);
  if (!base)
    return g_strdup (".");
  while (base > file_name && *base == G_DIR_SEPARATOR)
    base--;
  len = (guint) 1 + base - file_name;
  
  base = g_new (gchar, len + 1);
  g_memmove (base, file_name, len);
  base[len] = 0;
  
  return base;
}

gchar*
g_get_current_dir (void)
{
  gchar *buffer;
  gchar *dir;

  buffer = g_new (gchar, G_PATH_LENGTH);
  *buffer = 0;
  
  /* We don't use getcwd(3) on SUNOS, because, it does a popen("pwd")
   * and, if that wasn't bad enough, hangs in doing so.
   */
#if	defined (sun) && !defined (__SVR4)
  dir = getwd (buffer);
#else	/* !sun */
  dir = getcwd (buffer, G_PATH_LENGTH - 1);
#endif	/* !sun */
  
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

gchar*
g_getenv (const gchar *variable)
{
#ifndef G_OS_WIN32
  g_return_val_if_fail (variable != NULL, NULL);

  return getenv (variable);
#else
  gchar *v;
  guint k;
  static gchar *p = NULL;
  static gint l;
  gchar dummy[2];

  g_return_val_if_fail (variable != NULL, NULL);
  
  v = getenv (variable);
  if (!v)
    return NULL;
  
  /* On Windows NT, it is relatively typical that environment variables
   * contain references to other environment variables. Handle that by
   * calling ExpandEnvironmentStrings.
   */

  /* First check how much space we need */
  k = ExpandEnvironmentStrings (v, dummy, 2);
  /* Then allocate that much, and actualy do the expansion */
  if (p == NULL)
    {
      p = g_malloc (k);
      l = k;
    }
  else if (k > l)
    {
      p = g_realloc (p, k);
      l = k;
    }
  ExpandEnvironmentStrings (v, p, k);
  return p;
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
	  int k;
	  g_tmp_dir = g_strdup (P_tmpdir);
	  k = strlen (g_tmp_dir);
	  if (g_tmp_dir[k-1] == G_DIR_SEPARATOR)
	    g_tmp_dir[k-1] = '\0';
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
      
      if (!g_home_dir)
	g_home_dir = g_strdup (g_getenv ("HOME"));
      
#ifdef G_OS_WIN32
      if (!g_home_dir)
	{
	  /* The official way to specify a home directory on NT is
	   * the HOMEDRIVE and HOMEPATH environment variables.
	   *
	   * This is inside #ifdef G_OS_WIN32 because with the cygwin dll,
	   * HOME should be a POSIX style pathname.
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
#endif /* !G_OS_WIN32 */
      
#ifdef HAVE_PWD_H
      {
	struct passwd *pw = NULL;
	gpointer buffer = NULL;
	
#  ifdef HAVE_GETPWUID_R
        struct passwd pwd;
#    ifdef _SC_GETPW_R_SIZE_MAX  
	/* This reurns the maximum length */
        guint bufsize = sysconf (_SC_GETPW_R_SIZE_MAX);
#    else /* _SC_GETPW_R_SIZE_MAX */
        guint bufsize = 64;
#    endif /* _SC_GETPW_R_SIZE_MAX */
        gint error;
	
        do
          {
            g_free (buffer);
            buffer = g_malloc (bufsize);
	    errno = 0;
	    
#    ifdef HAVE_GETPWUID_R_POSIX
	    error = getpwuid_r (getuid (), &pwd, buffer, bufsize, &pw);
            error = error < 0 ? errno : error;
#    else /* !HAVE_GETPWUID_R_POSIX */
#      ifdef _AIX
	    error = getpwuid_r (getuid (), &pwd, buffer, bufsize);
	    pw = error == 0 ? &pwd : NULL;
#      else /* !_AIX */
            pw = getpwuid_r (getuid (), &pwd, buffer, bufsize);
            error = pw ? 0 : errno;
#      endif /* !_AIX */            
#    endif /* !HAVE_GETPWUID_R_POSIX */
	    
	    if (!pw)
	      {
		/* we bail out prematurely if the user id can't be found
		 * (should be pretty rare case actually), or if the buffer
		 * should be sufficiently big and lookups are still not
		 * successfull.
		 */
		if (error == 0 || error == ENOENT)
		  {
		    g_warning ("getpwuid_r(): failed due to: " 
			       "No such user: %lu.", (unsigned long)getuid ());
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
#  endif /* !HAVE_GETPWUID_R */
	
	if (!pw)
	  {
	    setpwent ();
	    pw = getpwuid (getuid ());
	    endpwent ();
	  }
	if (pw)
	  {
	    g_user_name = g_strdup (pw->pw_name);
#ifdef HAVE_PW_GECOS
	    g_real_name = g_strdup (pw->pw_gecos);
#else
	    g_real_name = g_strdup (g_user_name);
#endif
	    if (!g_home_dir)
	      g_home_dir = g_strdup (pw->pw_dir);
	  }
	g_free (buffer);
      }
      
#else /* !HAVE_PWD_H */
      
#  ifdef G_OS_WIN32
      {
	guint len = 17;
	gchar buffer[17];
	
	if (GetUserName (buffer, &len))
	  {
	    g_user_name = g_strdup (buffer);
	    g_real_name = g_strdup (buffer);
	  }
      }
#  endif /* G_OS_WIN32 */
      
#endif /* !HAVE_PWD_H */
      
#ifdef __EMX__
      /* change '\\' in %HOME% to '/' */
      g_strdelimit (g_home_dir, "\\",'/');
#endif
      if (!g_user_name)
	g_user_name = g_strdup ("somebody");
      if (!g_real_name)
	g_real_name = g_strdup ("Unknown");
      else
	{
	  gchar *p;

	  for (p = g_real_name; *p; p++)
	    if (*p == ',')
	      {
		*p = 0;
		p = g_strdup (g_real_name);
		g_free (g_real_name);
		g_real_name = p;
		break;
	      }
	}
    }
}

gchar*
g_get_user_name (void)
{
  G_LOCK (g_utils_global);
  if (!g_tmp_dir)
    g_get_any_init ();
  G_UNLOCK (g_utils_global);
  
  return g_user_name;
}

gchar*
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

gchar*
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

gchar*
g_get_tmp_dir (void)
{
  G_LOCK (g_utils_global);
  if (!g_tmp_dir)
    g_get_any_init ();
  G_UNLOCK (g_utils_global);
  
  return g_tmp_dir;
}

static gchar *g_prgname = NULL;

gchar*
g_get_prgname (void)
{
  gchar* retval;

  G_LOCK (g_utils_global);
  retval = g_prgname;
  G_UNLOCK (g_utils_global);

  return retval;
}

void
g_set_prgname (const gchar *prgname)
{
  gchar *c;
    
  G_LOCK (g_utils_global);
  c = g_prgname;
  g_prgname = g_strdup (prgname);
  g_free (c);
  G_UNLOCK (g_utils_global);
}

guint
g_direct_hash (gconstpointer v)
{
  return GPOINTER_TO_UINT (v);
}

gint
g_direct_equal (gconstpointer v1,
		gconstpointer v2)
{
  return v1 == v2;
}

gint
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

#ifdef G_OS_WIN32

int
gwin_ftruncate (gint  fd,
		guint size)
{
  HANDLE hfile;
  guint curpos;

  g_return_val_if_fail (fd >= 0, -1);
  
  hfile = (HANDLE) _get_osfhandle (fd);
  curpos = SetFilePointer (hfile, 0, NULL, FILE_CURRENT);
  if (curpos == 0xFFFFFFFF
      || SetFilePointer (hfile, size, NULL, FILE_BEGIN) == 0xFFFFFFFF
      || !SetEndOfFile (hfile))
    {
      gint error = GetLastError ();

      switch (error)
	{
	case ERROR_INVALID_HANDLE:
	  errno = EBADF;
	  break;
	default:
	  errno = EIO;
	  break;
	}

      return -1;
    }

  return 0;
}

DIR*
gwin_opendir (const char *dirname)
{
  DIR *result;
  gchar *mask;
  guint k;

  g_return_val_if_fail (dirname != NULL, NULL);

  result = g_new0 (DIR, 1);
  result->find_file_data = g_new0 (WIN32_FIND_DATA, 1);
  result->dir_name = g_strdup (dirname);
  
  k = strlen (result->dir_name);
  if (k && result->dir_name[k - 1] == '\\')
    {
      result->dir_name[k - 1] = '\0';
      k--;
    }
  mask = g_strdup_printf ("%s\\*", result->dir_name);

  result->find_file_handle = (guint) FindFirstFile (mask,
					     (LPWIN32_FIND_DATA) result->find_file_data);
  g_free (mask);

  if (result->find_file_handle == (guint) INVALID_HANDLE_VALUE)
    {
      int error = GetLastError ();

      g_free (result->dir_name);
      g_free (result->find_file_data);
      g_free (result);
      switch (error)
	{
	default:
	  errno = EIO;
	  return NULL;
	}
    }
  result->just_opened = TRUE;

  return result;
}

struct dirent*
gwin_readdir (DIR *dir)
{
  static struct dirent result;

  g_return_val_if_fail (dir != NULL, NULL);

  if (dir->just_opened)
    dir->just_opened = FALSE;
  else
    {
      if (!FindNextFile ((HANDLE) dir->find_file_handle,
			 (LPWIN32_FIND_DATA) dir->find_file_data))
	{
	  int error = GetLastError ();

	  switch (error)
	    {
	    case ERROR_NO_MORE_FILES:
	      return NULL;
	    default:
	      errno = EIO;
	      return NULL;
	    }
	}
    }
  strcpy (result.d_name, g_basename (((LPWIN32_FIND_DATA) dir->find_file_data)->cFileName));
      
  return &result;
}

void
gwin_rewinddir (DIR *dir)
{
  gchar *mask;

  g_return_if_fail (dir != NULL);

  if (!FindClose ((HANDLE) dir->find_file_handle))
    g_warning ("gwin_rewinddir(): FindClose() failed\n");

  mask = g_strdup_printf ("%s\\*", dir->dir_name);
  dir->find_file_handle = (guint) FindFirstFile (mask,
					  (LPWIN32_FIND_DATA) dir->find_file_data);
  g_free (mask);

  if (dir->find_file_handle == (guint) INVALID_HANDLE_VALUE)
    {
      int error = GetLastError ();

      switch (error)
	{
	default:
	  errno = EIO;
	  return;
	}
    }
  dir->just_opened = TRUE;
}  

gint
gwin_closedir (DIR *dir)
{
  g_return_val_if_fail (dir != NULL, -1);

  if (!FindClose ((HANDLE) dir->find_file_handle))
    {
      int error = GetLastError ();

      switch (error)
	{
	default:
	  errno = EIO; return -1;
	}
    }

  g_free (dir->dir_name);
  g_free (dir->find_file_data);
  g_free (dir);

  return 0;
}

/* Mingw32 headers don't have latest language and sublanguage codes */
#ifndef LANG_AFRIKAANS
#define LANG_AFRIKAANS 0x36
#endif
#ifndef LANG_ALBANIAN
#define LANG_ALBANIAN 0x1c
#endif
#ifndef LANG_ARABIC
#define LANG_ARABIC 0x01
#endif
#ifndef LANG_ARMENIAN
#define LANG_ARMENIAN 0x2b
#endif
#ifndef LANG_ASSAMESE
#define LANG_ASSAMESE 0x4d
#endif
#ifndef LANG_AZERI
#define LANG_AZERI 0x2c
#endif
#ifndef LANG_BASQUE
#define LANG_BASQUE 0x2d
#endif
#ifndef LANG_BELARUSIAN
#define LANG_BELARUSIAN 0x23
#endif
#ifndef LANG_BENGALI
#define LANG_BENGALI 0x45
#endif
#ifndef LANG_CATALAN
#define LANG_CATALAN 0x03
#endif
#ifndef LANG_ESTONIAN
#define LANG_ESTONIAN 0x25
#endif
#ifndef LANG_FAEROESE
#define LANG_FAEROESE 0x38
#endif
#ifndef LANG_FARSI
#define LANG_FARSI 0x29
#endif
#ifndef LANG_GEORGIAN
#define LANG_GEORGIAN 0x37
#endif
#ifndef LANG_GUJARATI
#define LANG_GUJARATI 0x47
#endif
#ifndef LANG_HEBREW
#define LANG_HEBREW 0x0d
#endif
#ifndef LANG_HINDI
#define LANG_HINDI 0x39
#endif
#ifndef LANG_INDONESIAN
#define LANG_INDONESIAN 0x21
#endif
#ifndef LANG_KANNADA
#define LANG_KANNADA 0x4b
#endif
#ifndef LANG_KASHMIRI
#define LANG_KASHMIRI 0x60
#endif
#ifndef LANG_KAZAK
#define LANG_KAZAK 0x3f
#endif
#ifndef LANG_KONKANI
#define LANG_KONKANI 0x57
#endif
#ifndef LANG_LATVIAN
#define LANG_LATVIAN 0x26
#endif
#ifndef LANG_LITHUANIAN
#define LANG_LITHUANIAN 0x27
#endif
#ifndef LANG_MACEDONIAN
#define LANG_MACEDONIAN 0x2f
#endif
#ifndef LANG_MALAY
#define LANG_MALAY 0x3e
#endif
#ifndef LANG_MALAYALAM
#define LANG_MALAYALAM 0x4c
#endif
#ifndef LANG_MANIPURI
#define LANG_MANIPURI 0x58
#endif
#ifndef LANG_MARATHI
#define LANG_MARATHI 0x4e
#endif
#ifndef LANG_NEPALI
#define LANG_NEPALI 0x61
#endif
#ifndef LANG_ORIYA
#define LANG_ORIYA 0x48
#endif
#ifndef LANG_PUNJABI
#define LANG_PUNJABI 0x46
#endif
#ifndef LANG_SANSKRIT
#define LANG_SANSKRIT 0x4f
#endif
#ifndef LANG_SERBIAN
#define LANG_SERBIAN 0x1a
#endif
#ifndef LANG_SINDHI
#define LANG_SINDHI 0x59
#endif
#ifndef LANG_SLOVAK
#define LANG_SLOVAK 0x1b
#endif
#ifndef LANG_SWAHILI
#define LANG_SWAHILI 0x41
#endif
#ifndef LANG_TAMIL
#define LANG_TAMIL 0x49
#endif
#ifndef LANG_TATAR
#define LANG_TATAR 0x44
#endif
#ifndef LANG_TELUGU
#define LANG_TELUGU 0x4a
#endif
#ifndef LANG_THAI
#define LANG_THAI 0x1e
#endif
#ifndef LANG_UKRAINIAN
#define LANG_UKRAINIAN 0x22
#endif
#ifndef LANG_URDU
#define LANG_URDU 0x20
#endif
#ifndef LANG_UZBEK
#define LANG_UZBEK 0x43
#endif
#ifndef LANG_VIETNAMESE
#define LANG_VIETNAMESE 0x2a
#endif
#ifndef SUBLANG_ARABIC_SAUDI_ARABIA
#define SUBLANG_ARABIC_SAUDI_ARABIA 0x01
#endif
#ifndef SUBLANG_ARABIC_IRAQ
#define SUBLANG_ARABIC_IRAQ 0x02
#endif
#ifndef SUBLANG_ARABIC_EGYPT
#define SUBLANG_ARABIC_EGYPT 0x03
#endif
#ifndef SUBLANG_ARABIC_LIBYA
#define SUBLANG_ARABIC_LIBYA 0x04
#endif
#ifndef SUBLANG_ARABIC_ALGERIA
#define SUBLANG_ARABIC_ALGERIA 0x05
#endif
#ifndef SUBLANG_ARABIC_MOROCCO
#define SUBLANG_ARABIC_MOROCCO 0x06
#endif
#ifndef SUBLANG_ARABIC_TUNISIA
#define SUBLANG_ARABIC_TUNISIA 0x07
#endif
#ifndef SUBLANG_ARABIC_OMAN
#define SUBLANG_ARABIC_OMAN 0x08
#endif
#ifndef SUBLANG_ARABIC_YEMEN
#define SUBLANG_ARABIC_YEMEN 0x09
#endif
#ifndef SUBLANG_ARABIC_SYRIA
#define SUBLANG_ARABIC_SYRIA 0x0a
#endif
#ifndef SUBLANG_ARABIC_JORDAN
#define SUBLANG_ARABIC_JORDAN 0x0b
#endif
#ifndef SUBLANG_ARABIC_LEBANON
#define SUBLANG_ARABIC_LEBANON 0x0c
#endif
#ifndef SUBLANG_ARABIC_KUWAIT
#define SUBLANG_ARABIC_KUWAIT 0x0d
#endif
#ifndef SUBLANG_ARABIC_UAE
#define SUBLANG_ARABIC_UAE 0x0e
#endif
#ifndef SUBLANG_ARABIC_BAHRAIN
#define SUBLANG_ARABIC_BAHRAIN 0x0f
#endif
#ifndef SUBLANG_ARABIC_QATAR
#define SUBLANG_ARABIC_QATAR 0x10
#endif
#ifndef SUBLANG_AZERI_LATIN
#define SUBLANG_AZERI_LATIN 0x01
#endif
#ifndef SUBLANG_AZERI_CYRILLIC
#define SUBLANG_AZERI_CYRILLIC 0x02
#endif
#ifndef SUBLANG_CHINESE_MACAU
#define SUBLANG_CHINESE_MACAU 0x05
#endif
#ifndef SUBLANG_ENGLISH_SOUTH_AFRICA
#define SUBLANG_ENGLISH_SOUTH_AFRICA 0x07
#endif
#ifndef SUBLANG_ENGLISH_JAMAICA
#define SUBLANG_ENGLISH_JAMAICA 0x08
#endif
#ifndef SUBLANG_ENGLISH_CARIBBEAN
#define SUBLANG_ENGLISH_CARIBBEAN 0x09
#endif
#ifndef SUBLANG_ENGLISH_BELIZE
#define SUBLANG_ENGLISH_BELIZE 0x0a
#endif
#ifndef SUBLANG_ENGLISH_TRINIDAD
#define SUBLANG_ENGLISH_TRINIDAD 0x0b
#endif
#ifndef SUBLANG_ENGLISH_ZIMBABWE
#define SUBLANG_ENGLISH_ZIMBABWE 0x0c
#endif
#ifndef SUBLANG_ENGLISH_PHILIPPINES
#define SUBLANG_ENGLISH_PHILIPPINES 0x0d
#endif
#ifndef SUBLANG_FRENCH_LUXEMBOURG
#define SUBLANG_FRENCH_LUXEMBOURG 0x05
#endif
#ifndef SUBLANG_FRENCH_MONACO
#define SUBLANG_FRENCH_MONACO 0x06
#endif
#ifndef SUBLANG_GERMAN_LUXEMBOURG
#define SUBLANG_GERMAN_LUXEMBOURG 0x04
#endif
#ifndef SUBLANG_GERMAN_LIECHTENSTEIN
#define SUBLANG_GERMAN_LIECHTENSTEIN 0x05
#endif
#ifndef SUBLANG_KASHMIRI_INDIA
#define SUBLANG_KASHMIRI_INDIA 0x02
#endif
#ifndef SUBLANG_MALAY_MALAYSIA
#define SUBLANG_MALAY_MALAYSIA 0x01
#endif
#ifndef SUBLANG_MALAY_BRUNEI_DARUSSALAM
#define SUBLANG_MALAY_BRUNEI_DARUSSALAM 0x02
#endif
#ifndef SUBLANG_NEPALI_INDIA
#define SUBLANG_NEPALI_INDIA 0x02
#endif
#ifndef SUBLANG_SERBIAN_LATIN
#define SUBLANG_SERBIAN_LATIN 0x02
#endif
#ifndef SUBLANG_SERBIAN_CYRILLIC
#define SUBLANG_SERBIAN_CYRILLIC 0x03
#endif
#ifndef SUBLANG_SPANISH_GUATEMALA
#define SUBLANG_SPANISH_GUATEMALA 0x04
#endif
#ifndef SUBLANG_SPANISH_COSTA_RICA
#define SUBLANG_SPANISH_COSTA_RICA 0x05
#endif
#ifndef SUBLANG_SPANISH_PANAMA
#define SUBLANG_SPANISH_PANAMA 0x06
#endif
#ifndef SUBLANG_SPANISH_DOMINICAN_REPUBLIC
#define SUBLANG_SPANISH_DOMINICAN_REPUBLIC 0x07
#endif
#ifndef SUBLANG_SPANISH_VENEZUELA
#define SUBLANG_SPANISH_VENEZUELA 0x08
#endif
#ifndef SUBLANG_SPANISH_COLOMBIA
#define SUBLANG_SPANISH_COLOMBIA 0x09
#endif
#ifndef SUBLANG_SPANISH_PERU
#define SUBLANG_SPANISH_PERU 0x0a
#endif
#ifndef SUBLANG_SPANISH_ARGENTINA
#define SUBLANG_SPANISH_ARGENTINA 0x0b
#endif
#ifndef SUBLANG_SPANISH_ECUADOR
#define SUBLANG_SPANISH_ECUADOR 0x0c
#endif
#ifndef SUBLANG_SPANISH_CHILE
#define SUBLANG_SPANISH_CHILE 0x0d
#endif
#ifndef SUBLANG_SPANISH_URUGUAY
#define SUBLANG_SPANISH_URUGUAY 0x0e
#endif
#ifndef SUBLANG_SPANISH_PARAGUAY
#define SUBLANG_SPANISH_PARAGUAY 0x0f
#endif
#ifndef SUBLANG_SPANISH_BOLIVIA
#define SUBLANG_SPANISH_BOLIVIA 0x10
#endif
#ifndef SUBLANG_SPANISH_EL_SALVADOR
#define SUBLANG_SPANISH_EL_SALVADOR 0x11
#endif
#ifndef SUBLANG_SPANISH_HONDURAS
#define SUBLANG_SPANISH_HONDURAS 0x12
#endif
#ifndef SUBLANG_SPANISH_NICARAGUA
#define SUBLANG_SPANISH_NICARAGUA 0x13
#endif
#ifndef SUBLANG_SPANISH_PUERTO_RICO
#define SUBLANG_SPANISH_PUERTO_RICO 0x14
#endif
#ifndef SUBLANG_SWEDISH_FINLAND
#define SUBLANG_SWEDISH_FINLAND 0x02
#endif
#ifndef SUBLANG_URDU_PAKISTAN
#define SUBLANG_URDU_PAKISTAN 0x01
#endif
#ifndef SUBLANG_URDU_INDIA
#define SUBLANG_URDU_INDIA 0x02
#endif
#ifndef SUBLANG_UZBEK_LATIN
#define SUBLANG_UZBEK_LATIN 0x01
#endif
#ifndef SUBLANG_UZBEK_CYRILLIC
#define SUBLANG_UZBEK_CYRILLIC 0x02
#endif

gchar *
gwin_getlocale (void)
{
  LCID lcid = GetThreadLocale ();
  gint primary, sub;
  gchar *l = NULL, *sl = NULL;
  gchar bfr[20];

  primary = PRIMARYLANGID (LANGIDFROMLCID (lcid));
  sub = SUBLANGID (LANGIDFROMLCID (lcid));
  switch (primary)
    {
    case LANG_AFRIKAANS: l = "af"; break;
    case LANG_ALBANIAN: l = "sq"; break;
    case LANG_ARABIC:
      l = "ar";
      switch (sub)
	{
	case SUBLANG_ARABIC_SAUDI_ARABIA: sl = "SA"; break;
	case SUBLANG_ARABIC_IRAQ: sl = "IQ"; break;
	case SUBLANG_ARABIC_EGYPT: sl = "EG"; break;
	case SUBLANG_ARABIC_LIBYA: sl = "LY"; break;
	case SUBLANG_ARABIC_ALGERIA: sl = "DZ"; break;
	case SUBLANG_ARABIC_MOROCCO: sl = "MA"; break;
	case SUBLANG_ARABIC_TUNISIA: sl = "TN"; break;
	case SUBLANG_ARABIC_OMAN: sl = "OM"; break;
	case SUBLANG_ARABIC_YEMEN: sl = "YE"; break;
	case SUBLANG_ARABIC_SYRIA: sl = "SY"; break;
	case SUBLANG_ARABIC_JORDAN: sl = "JO"; break;
	case SUBLANG_ARABIC_LEBANON: sl = "LB"; break;
	case SUBLANG_ARABIC_KUWAIT: sl = "KW"; break;
	case SUBLANG_ARABIC_UAE: sl = "AE"; break;
	case SUBLANG_ARABIC_BAHRAIN: sl = "BH"; break;
	case SUBLANG_ARABIC_QATAR: sl = "QA"; break;
	}
      break;
    case LANG_ARMENIAN: l = "hy"; break;
    case LANG_ASSAMESE: l = "as"; break;
    case LANG_AZERI: l = "az"; break;
    case LANG_BASQUE: l = "eu"; break;
    case LANG_BELARUSIAN: l = "be"; break;
    case LANG_BENGALI: l = "bn"; break;
    case LANG_BULGARIAN: l = "bg"; break;
    case LANG_CATALAN: l = "ca"; break;
    case LANG_CHINESE:
      l = "zh";
      switch (sub)
	{
	case SUBLANG_CHINESE_TRADITIONAL: sl = "TW"; break;
	case SUBLANG_CHINESE_SIMPLIFIED: sl = "CH"; break;
	case SUBLANG_CHINESE_HONGKONG: sl = "HK"; break;
	case SUBLANG_CHINESE_SINGAPORE: sl = "SG"; break;
	case SUBLANG_CHINESE_MACAU: sl = "MO"; break;
	}
      break;
    case LANG_CROATIAN:		/* LANG_CROATIAN == LANG_SERBIAN
				 * What used to be called Serbo-Croatian
				 * should really now be two separate
				 * languages because of political reasons.
				 * (Says tml, who knows nothing about Serbian
				 * or Croatian.)
				 * (I can feel those flames coming already.)
				 */
      switch (sub)
	{
	case SUBLANG_SERBIAN_LATIN: l = "hr"; break;
	case SUBLANG_SERBIAN_CYRILLIC: l = "sr"; break;
	default: l = "hr";	/* ??? */
	}
      break;
    case LANG_CZECH: l = "cs"; break;
    case LANG_DANISH: l = "da"; break;
    case LANG_DUTCH:
      l = "nl";
      switch (sub)
	{
	case SUBLANG_DUTCH_BELGIAN: sl = "BE"; break;
	}
      break;
    case LANG_ENGLISH:
      l = "en";
      switch (sub)
	{
	case SUBLANG_ENGLISH_US: sl = "US"; break;
	case SUBLANG_ENGLISH_UK: sl = "GB"; break;
	case SUBLANG_ENGLISH_AUS: sl = "AU"; break;
	case SUBLANG_ENGLISH_CAN: sl = "CA"; break;
	case SUBLANG_ENGLISH_NZ: sl = "NZ"; break;
	case SUBLANG_ENGLISH_EIRE: sl = "IE"; break;
	case SUBLANG_ENGLISH_SOUTH_AFRICA: sl = "SA"; break;
	case SUBLANG_ENGLISH_JAMAICA: sl = "JM"; break;
	case SUBLANG_ENGLISH_CARIBBEAN: sl = "@caribbean"; break; /* ??? */
	case SUBLANG_ENGLISH_BELIZE: sl = "BZ"; break;
	case SUBLANG_ENGLISH_TRINIDAD: sl = "TT"; break;
	case SUBLANG_ENGLISH_ZIMBABWE: sl = "ZW"; break;
	case SUBLANG_ENGLISH_PHILIPPINES: sl = "PH"; break;
	}
      break;
    case LANG_ESTONIAN: l = "et"; break;
    case LANG_FAEROESE: l = "fo"; break;
    case LANG_FARSI: l = "fa"; break;
    case LANG_FINNISH: l = "fi"; break;
    case LANG_FRENCH:
      l = "fr";
      switch (sub)
	{
	case SUBLANG_FRENCH_BELGIAN: sl = "BE"; break;
	case SUBLANG_FRENCH_CANADIAN: sl = "CA"; break;
	case SUBLANG_FRENCH_SWISS: sl = "CH"; break;
	case SUBLANG_FRENCH_LUXEMBOURG: sl = "LU"; break;
	case SUBLANG_FRENCH_MONACO: sl = "MC"; break;
	}
      break;
    case LANG_GEORGIAN: l = "ka"; break;
    case LANG_GERMAN:
      l = "de";
      switch (sub)
	{
	case SUBLANG_GERMAN_SWISS: sl = "CH"; break;
	case SUBLANG_GERMAN_AUSTRIAN: sl = "AT"; break;
	case SUBLANG_GERMAN_LUXEMBOURG: sl = "LU"; break;
	case SUBLANG_GERMAN_LIECHTENSTEIN: sl = "LI"; break;
	}
      break;
    case LANG_GREEK: l = "el"; break;
    case LANG_GUJARATI: l = "gu"; break;
    case LANG_HEBREW: l = "he"; break;
    case LANG_HINDI: l = "hi"; break;
    case LANG_HUNGARIAN: l = "hu"; break;
    case LANG_ICELANDIC: l = "is"; break;
    case LANG_INDONESIAN: l = "id"; break;
    case LANG_ITALIAN:
      l = "it";
      switch (sub)
	{
	case SUBLANG_ITALIAN_SWISS: sl = "CH"; break;
	}
      break;
    case LANG_JAPANESE: l = "ja"; break;
    case LANG_KANNADA: l = "kn"; break;
    case LANG_KASHMIRI:
      l = "ks";
      switch (sub)
	{
	case SUBLANG_KASHMIRI_INDIA: sl = "IN"; break;
	}
      break;
    case LANG_KAZAK: l = "kk"; break;
    case LANG_KONKANI: l = "kok"; break; /* ??? */
    case LANG_KOREAN: l = "ko"; break;
    case LANG_LATVIAN: l = "lv"; break;
    case LANG_LITHUANIAN: l = "lt"; break;
    case LANG_MACEDONIAN: l = "mk"; break;
    case LANG_MALAY:
      l = "ms";
      switch (sub)
	{
	case SUBLANG_MALAY_MALAYSIA: sl = "MY"; break;
	case SUBLANG_MALAY_BRUNEI_DARUSSALAM: sl = "BN"; break;
	}
      break;
    case LANG_MALAYALAM: l = "ml"; break;
    case LANG_MANIPURI: l = "mni"; break;
    case LANG_MARATHI: l = "mr"; break;
    case LANG_NEPALI:
      l = "ne";
      switch (sub)
	{
	case SUBLANG_NEPALI_INDIA: sl = "IN"; break;
	}
      break;
    case LANG_NORWEGIAN:
      l = "no";
      switch (sub)
	{
	case SUBLANG_NORWEGIAN_BOKMAL: sl = "@bokmal"; break;
	case SUBLANG_NORWEGIAN_NYNORSK: sl = "@nynorsk"; break;
	}
      break;
    case LANG_ORIYA: l = "or"; break;
    case LANG_POLISH: l = "pl"; break;
    case LANG_PORTUGUESE:
      l = "pt";
      switch (sub)
	{
	case SUBLANG_PORTUGUESE_BRAZILIAN: sl = "BR"; break;
	}
      break;
    case LANG_PUNJABI: l = "pa"; break;
    case LANG_ROMANIAN: l = "ro"; break;
    case LANG_RUSSIAN: l = "ru"; break;
    case LANG_SANSKRIT: l = "sa"; break;
    case LANG_SINDHI: l = "sd"; break;
    case LANG_SLOVAK: l = "sk"; break;
    case LANG_SLOVENIAN: l = "sl"; break;
    case LANG_SPANISH:
      l = "es";
      switch (sub)
	{
	case SUBLANG_SPANISH_MEXICAN: sl = "MX"; break;
	case SUBLANG_SPANISH_MODERN: sl = "@modern"; break;	/* ??? */
	case SUBLANG_SPANISH_GUATEMALA: sl = "GT"; break;
	case SUBLANG_SPANISH_COSTA_RICA: sl = "CR"; break;
	case SUBLANG_SPANISH_PANAMA: sl = "PA"; break;
	case SUBLANG_SPANISH_DOMINICAN_REPUBLIC: sl = "DO"; break;
	case SUBLANG_SPANISH_VENEZUELA: sl = "VE"; break;
	case SUBLANG_SPANISH_COLOMBIA: sl = "CO"; break;
	case SUBLANG_SPANISH_PERU: sl = "PE"; break;
	case SUBLANG_SPANISH_ARGENTINA: sl = "AR"; break;
	case SUBLANG_SPANISH_ECUADOR: sl = "EC"; break;
	case SUBLANG_SPANISH_CHILE: sl = "CL"; break;
	case SUBLANG_SPANISH_URUGUAY: sl = "UY"; break;
	case SUBLANG_SPANISH_PARAGUAY: sl = "PY"; break;
	case SUBLANG_SPANISH_BOLIVIA: sl = "BO"; break;
	case SUBLANG_SPANISH_EL_SALVADOR: sl = "SV"; break;
	case SUBLANG_SPANISH_HONDURAS: sl = "HN"; break;
	case SUBLANG_SPANISH_NICARAGUA: sl = "NI"; break;
	case SUBLANG_SPANISH_PUERTO_RICO: sl = "PR"; break;
	}
      break;
    case LANG_SWAHILI: l = "sw"; break;
    case LANG_SWEDISH:
      l = "sv";
      switch (sub)
	{
	case SUBLANG_SWEDISH_FINLAND: sl = "FI"; break;
	}
      break;
    case LANG_TAMIL: l = "ta"; break;
    case LANG_TATAR: l = "tt"; break;
    case LANG_TELUGU: l = "te"; break;
    case LANG_THAI: l = "th"; break;
    case LANG_TURKISH: l = "tr"; break;
    case LANG_UKRAINIAN: l = "uk"; break;
    case LANG_URDU:
      l = "ur";
      switch (sub)
	{
	case SUBLANG_URDU_PAKISTAN: sl = "PK"; break;
	case SUBLANG_URDU_INDIA: sl = "IN"; break;
	}
      break;
    case LANG_UZBEK:
      l = "uz";
      switch (sub)
	{
	case SUBLANG_UZBEK_LATIN: sl = "2latin"; break;
	case SUBLANG_UZBEK_CYRILLIC: sl = "@cyrillic"; break;
	}
      break;
    case LANG_VIETNAMESE: l = "vi"; break;
    default: l = "xx"; break;
    }
  strcpy (bfr, l);
  if (sl != NULL)
    {
      strcat (bfr, "_");
      strcat (bfr, sl);
    }

  return g_strdup (bfr);
}

#endif /* G_OS_WIN32 */
