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

/* the read buffer size in bytes */
#define READ_BUFFER_SIZE 1024

static GIOChannel* out = NULL;

static gboolean
child_out_cb (GIOChannel  * source,
	      GIOCondition  condition,
	      gpointer      data)
{
  GError* error = NULL;
  gsize length = 0;
  gchar buffer[READ_BUFFER_SIZE];
  GIOStatus status = G_IO_STATUS_NORMAL;

  while (status == G_IO_STATUS_NORMAL) {
    status = g_io_channel_read_chars (source, buffer, sizeof (buffer), &length, &error);

    switch (status) {
    case G_IO_STATUS_NORMAL:
	    // FIXME: this is where the parsing happens
	    g_print ("%d\n", length);
	    break;
    case G_IO_STATUS_AGAIN:
	    /* retry later */
	    break;
    case G_IO_STATUS_ERROR:
	    /* fall through into EOF */
	    g_warning ("Error while reading data: %s",
		       error->message);
	    g_error_free (error);
    case G_IO_STATUS_EOF:
	    return FALSE;
    }
  }

  return TRUE;
}

static void
child_watch_cb (GPid     pid,
		gint     status,
		gpointer data)
{
  GMainLoop* loop = data;

  g_spawn_close_pid (pid);

  /* read the remaining data - also stops the io watch from being polled */
  child_out_cb (out, G_IO_IN, data);
  g_main_loop_quit (data);
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
    "git-annotate",
    "--incremental",
    "ChangeLog",
    NULL
  };
  gint        child_out;

  working_folder = g_strdup ("/home/herzi/Hacking/Imendio/WebKit/WebCore"); //g_get_current_dir ();
  g_spawn_async_with_pipes (working_folder,
		 child_argv, NULL /* envp */,
		 G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_SEARCH_PATH,
		 NULL, NULL,
		 &pid,
		 NULL,
		 &child_out,
		 NULL,
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

  out = g_io_channel_unix_new (child_out);
  g_io_channel_set_flags (out, G_IO_FLAG_NONBLOCK, NULL); // FIXME: GError
  g_io_add_watch (out, G_IO_IN,
		  child_out_cb, loop);

  g_main_loop_run (loop);
  g_main_loop_unref (loop);
  return 0;
}

