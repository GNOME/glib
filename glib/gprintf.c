/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997, 2002  Peter Mattis, Red Hat, Inc.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "glib.h"
#include "gprintf.h"
#include "gprintfint.h"

/**
 * g_printf:
 * @format: the format string. See the printf() documentation.
 * @Varargs: the arguments to insert in the output.
 *
 * An implementation of the standard printf() function which supports 
 * positional parameters, as specified in the Single Unix Specification.
 *
 * Returns: the number of characters printed.
 *
 * Since: 2.2
 **/
gint
g_printf (gchar const *fmt,
	  ...)
{
  va_list args;
  gint retval;

  va_start (args, fmt);
  retval = g_vprintf (fmt, args);
  va_end (args);
  
  return retval;
}

/**
 * g_fprintf:
 * @file: the stream to write to.
 * @format: the format string. See the printf() documentation.
 * @Varargs: the arguments to insert in the output.
 *
 * An implementation of the standard fprintf() function which supports 
 * positional parameters, as specified in the Single Unix Specification.
 *
 * Returns: the number of characters printed.
 *
 * Since: 2.2
 **/
gint
g_fprintf (FILE *file, 
           gchar const *fmt,
	   ...)
{
  va_list args;
  gint retval;

  va_start (args, fmt);
  retval = g_vfprintf (file, fmt, args);
  va_end (args);
  
  return retval;
}

/**
 * g_sprintf:
 * @string: the buffer to hold the output.
 * @format: the format string. See the printf() documentation.
 * @Varargs: the arguments to insert in the output.
 *
 * An implementation of the standard sprintf() function which supports 
 * positional parameters, as specified in the Single Unix Specification.
 *
 * Returns: the number of characters printed.
 *
 * Since: 2.2
 **/
gint
g_sprintf (gchar	*str,
	   gchar const *fmt,
	   ...)
{
  va_list args;
  gint retval;

  va_start (args, fmt);
  retval = g_vsprintf (str, fmt, args);
  va_end (args);
  
  return retval;
}

/**
 * g_snprintf:
 * @string: the buffer to hold the output.
 * @n: the maximum number of characters to produce (including the 
 *     terminating nul character).
 * @format: the format string. See the printf() documentation.
 * @Varargs: the arguments to insert in the output.
 *
 * A safer form of the standard sprintf() function. The output is guaranteed
 * to not exceed @n characters (including the terminating nul character), so 
 * it is easy to ensure that a buffer overflow cannot occur.
 * 
 * See also g_strdup_printf().
 *
 * In versions of GLib prior to 1.2.3, this function may return -1 if the 
 * output was truncated, and the truncated string may not be nul-terminated. 
 * In versions prior to 1.3.12, this function returns the length of the output 
 * string.
 *
 * The return value of g_snprintf() conforms to the snprintf()
 * function as standardized in ISO C99. Note that this is different from 
 * traditional snprintf(), which returns the length of the output string.
 *
 * The format string may contain positional parameters, as specified in 
 * the Single Unix Specification.
 *
 * Returns: the number of characters which would be produced if the buffer 
 *     was large enough.
 **/
gint
g_snprintf (gchar	*str,
	    gulong	 n,
	    gchar const *fmt,
	    ...)
{
  va_list args;
  gint retval;

  va_start (args, fmt);
  retval = g_vsnprintf (str, n, fmt, args);
  va_end (args);
  
  return retval;
}

/**
 * g_vprintf:
 * @format: the format string. See the printf() documentation.
 * @args: the list of arguments to insert in the output.
 *
 * An implementation of the standard vprintf() function which supports 
 * positional parameters, as specified in the Single Unix Specification.
 *
 * Returns: the number of characters printed.
 *
 * Since: 2.2
 **/
gint
g_vprintf (gchar const *fmt,
	   va_list      args)
{
  g_return_val_if_fail (fmt != NULL, 0);

  return _g_vprintf (fmt, args);
}

/**
 * g_vfprintf:
 * @file: the stream to write to.
 * @format: the format string. See the printf() documentation.
 * @args: the list of arguments to insert in the output.
 *
 * An implementation of the standard fprintf() function which supports 
 * positional parameters, as specified in the Single Unix Specification.
 *
 * Returns: the number of characters printed.
 *
 * Since: 2.2
 **/
gint
g_vfprintf (FILE *file,
            gchar const *fmt,
	    va_list      args)
{
  g_return_val_if_fail (fmt != NULL, 0);

  return _g_vfprintf (file, fmt, args);
}

/**
 * g_vsprintf:
 * @string: the buffer to hold the output.
 * @format: the format string. See the printf() documentation.
 * @args: the list of arguments to insert in the output.
 *
 * An implementation of the standard vsprintf() function which supports 
 * positional parameters, as specified in the Single Unix Specification.
 *
 * Returns: the number of characters printed.
 *
 * Since: 2.2
 **/
gint
g_vsprintf (gchar	 *str,
	    gchar const *fmt,
	    va_list      args)
{
  g_return_val_if_fail (str != NULL, 0);
  g_return_val_if_fail (fmt != NULL, 0);

  return _g_vsprintf (str, fmt, args);
}

/** 
 * g_vsnprintf:
 * @string: the buffer to hold the output.
 * @n: the maximum number of characters to produce (including the 
 *     terminating nul character).
 * @format: the format string. See the printf() documentation.
 * @args: the list of arguments to insert in the output.
 *
 * A safer form of the standard vsprintf() function. The output is guaranteed
 * to not exceed @n characters (including the terminating nul character), so 
 * it is easy to ensure that a buffer overflow cannot occur.
 *
 * See also g_strdup_vprintf().
 *
 * In versions of GLib prior to 1.2.3, this function may return -1 if the 
 * output was truncated, and the truncated string may not be nul-terminated.
 * In versions prior to 1.3.12, this function returns the length of the output 
 * string.
 *
 * The return value of g_vsnprintf() conforms to the vsnprintf() function 
 * as standardized in ISO C99. Note that this is different from traditional 
 * vsnprintf(), which returns the length of the output string.
 *
 * The format string may contain positional parameters, as specified in 
 * the Single Unix Specification.
 *
 * Returns: the number of characters which would be produced if the buffer 
 *  was large enough.
 */
gint
g_vsnprintf (gchar	 *str,
	     gulong	  n,
	     gchar const *fmt,
	     va_list      args)
{
  g_return_val_if_fail (n == 0 || str != NULL, 0);
  g_return_val_if_fail (fmt != NULL, 0);

  return _g_vsnprintf (str, n, fmt, args);
}



