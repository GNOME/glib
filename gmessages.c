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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "glib.h"

static GErrorFunc glib_error_func = NULL;
static GWarningFunc glib_warning_func = NULL;
static GPrintFunc glib_message_func = NULL;
static GPrintFunc glib_print_func = NULL;

extern char* g_vsprintf (const gchar *fmt, va_list *args, va_list *args2);

void
g_error (const gchar *format, ...)
{
  va_list args, args2;
  char *buf;
  static gboolean errored = 0;

  if (errored++)
    {
      write (2, "g_error: recursed!\n", 19);
      return;
    }
  
  va_start (args, format);
  va_start (args2, format);
  buf = g_vsprintf (format, &args, &args2);
  va_end (args);
  va_end (args2);

  if (glib_error_func)
    {
      (* glib_error_func) (buf);
    }
  else
    {
      /* Use write() here because we might be out of memory */
      write (2, "\n** ERROR **: ", 14);
      write (2, buf, strlen(buf));
      write (2, "\n", 1);
    }

  abort ();
}

void
g_warning (const gchar *format, ...)
{
  va_list args, args2;
  char *buf;

  va_start (args, format);
  va_start (args2, format);
  buf = g_vsprintf (format, &args, &args2);
  va_end (args);
  va_end (args2);

  if (glib_warning_func)
    {
      (* glib_warning_func) (buf);
    }
  else
    {
      fputs ("\n** WARNING **: ", stderr);
      fputs (buf, stderr);
      fputc ('\n', stderr);
    }
}

void
g_message (const gchar *format, ...)
{
  va_list args, args2;
  char *buf;

  va_start (args, format);
  va_start (args2, format);
  buf = g_vsprintf (format, &args, &args2);
  va_end (args);
  va_end (args2);

  if (glib_message_func)
    {
      (* glib_message_func) (buf);
    }
  else
    {
      fputs ("message: ", stdout);
      fputs (buf, stdout);
      fputc ('\n', stdout);
    }
}

void
g_print (const gchar *format, ...)
{
  va_list args, args2;
  char *buf;

  va_start (args, format);
  va_start (args2, format);
  buf = g_vsprintf (format, &args, &args2);
  va_end (args);
  va_end (args2);

  if (glib_print_func)
    {
      (* glib_print_func) (buf);
    }
  else
    {
      fputs (buf, stdout);
    }
}

GErrorFunc
g_set_error_handler (GErrorFunc func)
{
  GErrorFunc old_error_func;

  old_error_func = glib_error_func;
  glib_error_func = func;

  return old_error_func;
}

GWarningFunc
g_set_warning_handler (GWarningFunc func)
{
  GWarningFunc old_warning_func;

  old_warning_func = glib_warning_func;
  glib_warning_func = func;

  return old_warning_func;
}

GPrintFunc
g_set_message_handler (GPrintFunc func)
{
  GPrintFunc old_message_func;

  old_message_func = glib_message_func;
  glib_message_func = func;

  return old_message_func;
}

GPrintFunc
g_set_print_handler (GPrintFunc func)
{
  GPrintFunc old_print_func;

  old_print_func = glib_print_func;
  glib_print_func = func;
  
  return old_print_func;
}

