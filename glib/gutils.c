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

/* implement Glib's inline functions
 */
#define	G_INLINE_FUNC extern
#define	G_CAN_INLINE 1
#include "glib.h"

#ifdef	MAXPATHLEN
#define	G_PATH_LENGTH	MAXPATHLEN
#elif	defined (PATH_MAX)
#define	G_PATH_LENGTH	PATH_MAX
#elif   defined (_PC_PATH_MAX)
#define	G_PATH_LENGTH	sysconf(_PC_PATH_MAX)
#else	
#define G_PATH_LENGTH   2048
#endif

#ifdef G_OS_WIN32
#  define STRICT			/* Strict typing, please */
#  include <windows.h>
#  include <ctype.h>
#  include <direct.h>
#  include <io.h>
#endif /* G_OS_WIN32 */

#ifdef HAVE_CODESET
#include <langinfo.h>
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
  
  g_return_val_if_fail (str != NULL, 0);
  g_return_val_if_fail (n > 0, 0);
  g_return_val_if_fail (fmt != NULL, 0);

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
  
  g_return_val_if_fail (str != NULL, 0);
  g_return_val_if_fail (n > 0, 0);
  g_return_val_if_fail (fmt != NULL, 0);

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
  
  g_return_val_if_fail (str != NULL, 0);
  g_return_val_if_fail (n > 0, 0);
  g_return_val_if_fail (fmt != NULL, 0);

  retval = vsnprintf (str, n, fmt, args);
  
  if (retval < 0)
    {
      str[n-1] = '\0';
      retval = strlen (str);
    }

  return retval;
#else	/* !HAVE_VSNPRINTF */
  gchar *printed;
  
  g_return_val_if_fail (str != NULL, 0);
  g_return_val_if_fail (n > 0, 0);
  g_return_val_if_fail (fmt != NULL, 0);

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
  gchar *buffer = NULL;
  gchar *dir = NULL;
  static gulong max_len = (G_PATH_LENGTH == -1) ? 2048 : G_PATH_LENGTH;
  
  /* We don't use getcwd(3) on SUNOS, because, it does a popen("pwd")
   * and, if that wasn't bad enough, hangs in doing so.
   */
#if	defined (sun) && !defined (__SVR4)
  buffer = g_new (gchar, max_len + 1);
  *buffer = 0;
  dir = getwd (buffer);
#else	/* !sun */
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
  G_LOCK_DEFINE_STATIC (getenv);
  struct env_struct
  {
    gchar *key;
    gchar *value;
  } *env;
  static GArray *environs = NULL;
  gchar *system_env;
  guint length, i;
  gchar dummy[2];

  g_return_val_if_fail (variable != NULL, NULL);
  
  G_LOCK (getenv);

  if (!environs)
    environs = g_array_new (FALSE, FALSE, sizeof (struct env_struct));

  /* First we try to find the envinronment variable inside the already
   * found ones.
   */

  for (i = 0; i < environs->len; i++)
    {
      env = &g_array_index (environs, struct env_struct, i);
      if (strcmp (env->key, variable) == 0)
	{
	  g_assert (env->value);
	  G_UNLOCK (getenv);
	  return env->value;
	}
    }

  /* If not found, we ask the system */

  system_env = getenv (variable);
  if (!system_env)
    {
      G_UNLOCK (getenv);
      return NULL;
    }

  /* On Windows NT, it is relatively typical that environment variables
   * contain references to other environment variables. Handle that by
   * calling ExpandEnvironmentStrings.
   */

  g_array_set_size (environs, environs->len + 1);

  env = &g_array_index (environs, struct env_struct, environs->len - 1);

  /* First check how much space we need */
  length = ExpandEnvironmentStrings (system_env, dummy, 2);

  /* Then allocate that much, and actualy do the expansion and insert
   * the new found pair into our buffer 
   */

  env->value = g_malloc (length);
  env->key = g_strdup (variable);

  ExpandEnvironmentStrings (system_env, env->value, length);

  G_UNLOCK (getenv);
  return env->value;
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
#endif /* G_OS_WIN32 */
      
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
	    g_real_name = g_strdup (pw->pw_gecos);
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
	
	if (GetUserName ((LPTSTR) buffer, (LPDWORD) &len))
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
#ifdef HAVE_CODESET  
  char *result = nl_langinfo (CODESET);
  return g_strdup (result);
#else
#ifndef G_OS_WIN32
  /* FIXME: Do something more intelligent based on setlocale (LC_CTYPE, NULL)
   */
  return g_strdup ("ISO-8859-1");
#else
  /* On Win32 we always use UTF-8. At least in GDK. SO should we
   * therefore return that?
   */
  return g_strdup ("UTF-8");
#endif
#endif
}
