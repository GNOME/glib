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

static void
test_mkstemp (void)
{
  char template[32];
  GError *error;
  int fd;
  int i;
  const char hello[] = "Hello, World";
  const int hellolen = sizeof (hello) - 1;
  char chars[62];

  strcpy (template, "foobar");
  fd = g_mkstemp (template);
  g_assert (fd == -1 && "g_mkstemp works even if template doesn't end in XXXXXX");
  close (fd);

  strcpy (template, "fooXXXXXX");
  fd = g_mkstemp (template);
  g_assert (fd != -1 && "g_mkstemp didn't work for template fooXXXXXX");
  i = write (fd, hello, hellolen);
  g_assert (i != -1 && "write() failed");
  g_assert (i == hellolen && "write() has written too few bytes");

  lseek (fd, 0, 0);
  i = read (fd, chars, sizeof (chars));
  g_assert (i != -1 && "read() failed: %s");
  g_assert (i == hellolen && "read() has got wrong number of bytes");

  chars[i] = 0;
  g_assert (strcmp (chars, hello) == 0 && "read() didn't get same string back");

  close (fd);
  remove (template);
}


int 
main (int argc, char *argv[])
{
  test_mkstemp ();

  return 0;
}
