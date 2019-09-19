/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright 2019 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include <gio/gio.h>

static char *levels_str[] = {
	"low",
	"medium",
	"critical"
};

static void
warning_cb (GMemoryMonitor *m,
	    GMemoryMonitorWarningLevel level)
{
  g_debug ("Warning level: %s (%d)\n", levels_str[level], level);
}

static void
do_watch_memory (void)
{
  GMemoryMonitor *m;
  GMainLoop *loop;

  m = g_memory_monitor_get_default ();
  g_signal_connect (G_OBJECT (m), "low-memory-warning",
		    G_CALLBACK (warning_cb), NULL);

  loop = g_main_loop_new (NULL, TRUE);
  g_main_loop_run (loop);
}

int
main (int argc, char **argv)
{
  int ret;

  if (argc == 2 && !strcmp (argv[1], "--watch"))
    {
      do_watch_memory ();
      return 0;
    }

  g_test_init (&argc, &argv, NULL);

  /* FIXME implement */

  ret = g_test_run ();

  return ret;
}
