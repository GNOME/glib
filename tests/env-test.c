/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
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

/*
 * Modified by the GLib Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GLib Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GLib at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "config.h"

#undef G_DISABLE_ASSERT
#undef G_LOG_DOMAIN

#ifdef GLIB_COMPILATION
#undef GLIB_COMPILATION
#endif

#include <stdio.h>
#include <string.h>

#include <glib.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

int 
main (int argc, char *argv[])
{
  gboolean result;
  const gchar *data;
  gchar *variable = "TEST_G_SETENV";
  gchar *value1 = "works";
  gchar *value2 = "again";

  data = g_getenv (variable);
  g_assert (data == NULL && "TEST_G_SETENV already set");
  
  result = g_setenv (variable, value1, TRUE);
  g_assert (result && "g_setenv() failed");
  
  data = g_getenv (variable);
  g_assert (data != NULL && "g_getenv() returns NULL");
  g_assert (strcmp (data, value1) == 0 && "g_getenv() returns wrong value");

  result = g_setenv (variable, value2, FALSE);
  g_assert (result && "g_setenv() failed");
  
  data = g_getenv (variable);
  g_assert (data != NULL && "g_getenv() returns NULL");
  g_assert (strcmp (data, value2) != 0 && "g_setenv() always overwrites");
  g_assert (strcmp (data, value1) == 0 && "g_getenv() returns wrong value");

  result = g_setenv (variable, value2, TRUE);
  g_assert (result && "g_setenv() failed");
  
  data = g_getenv (variable);
  g_assert (data != NULL && "g_getenv() returns NULL");
  g_assert (strcmp (data, value1) != 0 && "g_setenv() doesn't overwrite");
  g_assert (strcmp (data, value2) == 0 && "g_getenv() returns wrong value");

  g_unsetenv (variable);
  data = g_getenv (variable);
  g_assert (data == NULL && "g_unsetenv() doesn't work");

  return 0;
}
