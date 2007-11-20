/* This file is part of gtester
 *
 * AUTHORS
 *     Sven Herzberg  <herzi@gnome-de.org>
 *
 * Copyright (C) 2007  Sven Herzberg
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include <glib.h>

static void
child_watch_cb (GPid     pid,
		gint     status,
		gpointer data)
{
	GMainLoop* loop = data;

	g_main_loop_quit (loop);
}

int
main (int   argc,
      char**argv)
{
  GMainLoop* loop;
  GError   * error = NULL;
  GPid       pid = 0;
  gchar    * working_folder;
  gchar    * child_argv[] = {
    "cat",
    "/proc/cpuinfo",
    NULL
  };

  working_folder = g_get_current_dir ();
  g_spawn_async (working_folder,
		 child_argv, NULL /* envp */,
		 G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_SEARCH_PATH,
		 NULL, NULL,
		 &pid,
		 &error);
  g_free (working_folder);

  if (error)
    {
      g_error ("Couldn't execute child: %s", error->message);
      /* doesn't return */
    }

  loop = g_main_loop_new (NULL, FALSE);

  g_child_watch_add (pid,
		     child_watch_cb,
		     loop);

  g_main_loop_run (loop);
  g_main_loop_unref (loop);
  return 0;
}

