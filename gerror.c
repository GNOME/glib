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

/*
 * Modified by the GLib Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GLib Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GLib at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "glib.h"

static GError* 
g_error_new_valist(GQuark         domain,
                   gint           code,
                   const gchar   *format,
                   va_list        args)
{
  GError *error;
  
  error = g_new (GError, 1);
  
  error->domain = domain;
  error->code = code;
  error->message = g_strdup_vprintf (format, args);
  
  return error;
}

GError*
g_error_new (GQuark       domain,
             gint         code,
             const gchar *format,
             ...)
{
  GError* error;
  va_list args;

  g_return_val_if_fail (format != NULL, NULL);
  g_return_val_if_fail (domain != 0, NULL);

  va_start (args, format);
  error = g_error_new_valist (domain, code, format, args);
  va_end (args);

  return error;
}

GError*
g_error_new_literal (GQuark         domain,
                     gint           code,
                     const gchar   *message)
{
  GError* err;

  g_return_val_if_fail (message != NULL, NULL);
  g_return_val_if_fail (domain != 0, NULL);

  err = g_new (GError, 1);

  err->domain = domain;
  err->code = code;
  err->message = g_strdup (message);
  
  return err;
}

void
g_error_free (GError *error)
{
  g_return_if_fail (error != NULL);  

  g_free (error->message);

  g_free (error);
}

GError*
g_error_copy (const GError *error)
{
  GError *copy;
  
  g_return_val_if_fail (error != NULL, NULL);

  copy = g_new (GError, 1);

  *copy = *error;

  copy->message = g_strdup (error->message);

  return copy;
}

gboolean
g_error_matches (const GError *error,
                 GQuark        domain,
                 gint          code)
{
  return error &&
    error->domain == domain &&
    error->code == code;
}

void
g_set_error (GError     **err,
             GQuark       domain,
             gint         code,
             const gchar *format,
             ...)
{
  va_list args;

  if (err == NULL)
    return;

  if (*err != NULL)
    g_warning ("GError set over the top of a previous GError or uninitialized memory.\n"
               "This indicates a bug in someone's code. You must ensure an error is NULL before it's set.");
  
  va_start (args, format);
  *err = g_error_new_valist (domain, code, format, args);
  va_end (args);
}

void
g_clear_error (GError **err)
{
  if (err && *err)
    {
      g_error_free (*err);
      *err = NULL;
    }
}
