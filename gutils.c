/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
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
#include <unistd.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/param.h>
#include "glib.h"

const guint glib_major_version = GLIB_MAJOR_VERSION;
const guint glib_minor_version = GLIB_MINOR_VERSION;
const guint glib_micro_version = GLIB_MICRO_VERSION;
const guint glib_interface_age = GLIB_INTERFACE_AGE;
const guint glib_binary_age = GLIB_BINARY_AGE;

extern char* g_vsprintf (const gchar *fmt, va_list *args, va_list *args2);

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
  
  return retval;
#else	/* !HAVE_VSNPRINTF */
  gchar *printed;
  va_list args, args2;
  
  va_start (args, fmt);
  va_start (args2, fmt);
  
  printed = g_vsprintf (fmt, &args, &args2);
  strncpy (str, printed, n);
  str[n-1] = '\0';
  
  va_end (args2);
  va_end (args);
  
  return strlen (str);
#endif	/* !HAVE_VSNPRINTF */
}

gint
g_vsnprintf (gchar	 *str,
	     gulong	  n,
	     gchar const *fmt,
	     va_list     *args1,
	     va_list     *args2)
{
#ifdef	HAVE_VSNPRINTF
  gint retval;
  
  retval = vsnprintf (str, n, fmt, *args1);
  
  return retval;
#else	/* !HAVE_VSNPRINTF */
  gchar *printed;
  
  printed = g_vsprintf (fmt, args1, args2);
  strncpy (str, printed, n);
  str[n-1] = '\0';
  
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
  
  base = strrchr (file_name, '/');
  if (base)
    return base + 1;
  
  return (gchar*) file_name;
}

gchar*
g_dirname (const gchar	   *file_name)
{
  register gchar *base;
  register guint len;
  
  g_return_val_if_fail (file_name != NULL, NULL);
  
  base = strrchr (file_name, '/');
  if (!base)
    return g_strdup (".");
  while (base > file_name && *base == '/')
    base--;
  len = (guint) 1 + base - file_name;
  
  base = g_new (gchar, len + 1);
  g_memmove (base, file_name, len);
  base[len] = 0;
  
  return base;
}

#ifdef	MAXPATHLEN
#define	G_PATH_LENGTH	(MAXPATHLEN + 1)
#elif	defined (PATH_MAX)
#define	G_PATH_LENGTH	(PATH_MAX + 1)
#else	/* !MAXPATHLEN */
#define G_PATH_LENGTH   (2048 + 1)
#endif	/* !MAXPATHLEN && !PATH_MAX */

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
      buffer[0] = '/';
      buffer[1] = 0;
    }

  dir = g_strdup (buffer);
  g_free (buffer);
  
  return dir;
}

static	gchar	*g_tmp_dir = NULL;
static	gchar	*g_user_name = NULL;
static	gchar	*g_real_name = NULL;
static	gchar	*g_home_dir = NULL;

static void
g_get_any_init (void)
{
  if (!g_tmp_dir)
    {
      struct passwd *pw;
      
      g_tmp_dir = g_strdup (getenv ("TMPDIR"));
      if (!g_tmp_dir)
	g_tmp_dir = g_strdup (getenv ("TMP"));
      if (!g_tmp_dir)
	g_tmp_dir = g_strdup (getenv ("TEMP"));
      if (!g_tmp_dir)
	g_tmp_dir = g_strdup ("/tmp");
      
      g_home_dir = g_strdup (getenv ("HOME"));
      
      setpwent ();
      pw = getpwuid (getuid ());
      endpwent ();
      
      if (pw)
	{
	  g_user_name = g_strdup (pw->pw_name);
	  g_real_name = g_strdup (pw->pw_gecos);
	  if (!g_home_dir)
	    g_home_dir = g_strdup (pw->pw_dir);
	}
    }
}

gchar*
g_get_user_name (void)
{
  if (!g_tmp_dir)
    g_get_any_init ();
  
  return g_user_name;
}

gchar*
g_get_real_name (void)
{
  if (!g_tmp_dir)
    g_get_any_init ();
  
  return g_real_name;
}

gchar*
g_get_home_dir (void)
{
  if (!g_tmp_dir)
    g_get_any_init ();
  
  return g_home_dir;
}

gchar*
g_get_tmp_dir (void)
{
  if (!g_tmp_dir)
    g_get_any_init ();
  
  return g_tmp_dir;
}

static gchar *g_prgname = NULL;

gchar*
g_get_prgname (void)
{
  return g_prgname;
}

void
g_set_prgname (const gchar *prgname)
{
  gchar *c = g_prgname;
  
  g_prgname = g_strdup (prgname);
  g_free (c);
}

guint
g_direct_hash(gconstpointer v)
{
  return GPOINTER_TO_UINT (v);
}

gint
g_direct_equal(gconstpointer v, gconstpointer v2)
{
  return GPOINTER_TO_UINT (v) == GPOINTER_TO_UINT (v2);
}

gint
g_int_equal (gconstpointer v, gconstpointer v2)
{
  return *((const gint*) v) == *((const gint*) v2);
}

guint
g_int_hash (gconstpointer v)
{
  return *(const gint*) v;
}
