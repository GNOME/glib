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
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdlib.h>

#include <glib.h>

#ifdef G_OS_WIN32
#include <windows.h>
#endif

GMainLoop *main_loop;
gint alive;

#ifdef G_OS_WIN32
char *argv0;
#endif

GPid
get_a_child (gint ttl)
{
  GPid pid;

#ifdef G_OS_WIN32
  STARTUPINFO si;
  PROCESS_INFORMATION pi;
  gchar *cmdline;

  memset (&si, 0, sizeof (si));
  si.cb = sizeof (&si);
  memset (&pi, 0, sizeof (pi));

  cmdline = g_strdup_printf( "child-test -c%d", ttl);

  if (!CreateProcess (argv0, cmdline, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
    exit (1);

  g_free(cmdline);

  CloseHandle (pi.hThread);
  pid = pi.hProcess;

  return pid;
#else
  pid = fork ();
  if (pid < 0)
    exit (1);

  if (pid > 0)
    return pid;

  sleep (ttl);
  _exit (0);
#endif /* G_OS_WIN32 */
}

gboolean
child_watch_callback (GPid pid, gint status, gpointer data)
{
  gint ttl = GPOINTER_TO_INT (data);

#ifndef G_OS_WIN32
  g_print ("child %d (ttl %d) exited, status %d\n", pid, ttl, status);
#else
  g_print ("child %p (ttl %d) exited, status %d\n", pid, ttl, status);
#endif

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
  g_source_set_callback (source, (GSourceFunc) child_watch_callback, data, NULL);
  g_source_attach (source, g_main_loop_get_context (new_main_loop));
  g_source_unref (source);

#ifdef G_OS_WIN32
  g_print ("whee! created pid: %p (ttl %d)\n", pid, ttl);
  CloseHandle(pid);
#else
  g_print ("whee! created pid: %d (ttl %d)\n", pid, ttl);
#endif

  g_main_loop_run (new_main_loop);

  return NULL;
}

int
main (int argc, char *argv[])
{
#ifdef G_OS_WIN32
  argv0 = argv[0];
  if (argc > 1 && argv[1][0] == '-' && argv[1][1] == 'c')
    {
      int ttl = atoi (argv[1] + 2);
      Sleep (ttl * 1000);
      exit (STILL_ACTIVE);
    }
#endif
  /* Only run the test, if threads are enabled and a default thread
     implementation is available */
#if defined(G_THREADS_ENABLED) && ! defined(G_THREADS_IMPL_NONE)
   g_thread_init (NULL);
  main_loop = g_main_loop_new (NULL, FALSE);

  system ("/bin/true");

  alive = 2;
  g_thread_create (test_thread, GINT_TO_POINTER (10), FALSE, NULL);
  g_thread_create (test_thread, GINT_TO_POINTER (20), FALSE, NULL);
  
  g_main_loop_run (main_loop);

#endif
   return 0;
}
