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

#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

#include <glib.h>

GMainLoop *main_loop;
gint alive;

gint
get_a_child (gint ttl)
{
  GPid pid;

  pid = fork ();
  if (pid < 0)
    exit (1);

  if (pid > 0)
    return pid;

  sleep (ttl);
  _exit (0);
}

gboolean
child_watch_callback (GPid pid, gint status, gpointer data)
{
  g_print ("child %d exited, status %d\n", pid, status);

  if (--alive == 0)
    g_main_loop_quit (main_loop);

  return TRUE;
}

static gpointer
test_thread (gpointer data)
{
  GMainLoop *new_main_loop;
  GSource *source;
  GPid pid;
  gint ttl = GPOINTER_TO_INT (data);

  new_main_loop = g_main_loop_new (NULL, FALSE);

  pid = get_a_child (ttl);
  source = g_child_watch_source_new (pid);
  g_source_set_callback (source, (GSourceFunc) child_watch_callback, NULL, NULL);
  g_source_attach (source, g_main_loop_get_context (new_main_loop));
  g_source_unref (source);

  g_print ("whee! created pid: %d\n", pid);
  g_main_loop_run (new_main_loop);

}

int
main (int argc, char *argv[])
{
  g_thread_init (NULL);
  main_loop = g_main_loop_new (NULL, FALSE);

  system ("/bin/true");

  alive = 2;
  g_thread_create (test_thread, GINT_TO_POINTER (10), FALSE, NULL);
  g_thread_create (test_thread, GINT_TO_POINTER (20), FALSE, NULL);
  
  g_main_loop_run (main_loop);

  return 0;
}
