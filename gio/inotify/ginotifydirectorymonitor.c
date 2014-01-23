/* GIO - GLib Input, Output and Streaming Library
 * 
 * Copyright (C) 2006-2007 Red Hat, Inc.
 * Copyright (C) 2007 Sebastian Dröge.
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
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Alexander Larsson <alexl@redhat.com>
 *          John McCutchan <john@johnmccutchan.com> 
 *          Sebastian Dröge <slomo@circular-chaos.org>
 */

#include "config.h"

#include "ginotifydirectorymonitor.h"
#include <gio/giomodule.h>

#define USE_INOTIFY 1
#include "inotify-helper.h"

struct _GInotifyDirectoryMonitor
{
  GLocalDirectoryMonitor parent_instance;
  inotify_sub *sub;
};

static gboolean g_inotify_directory_monitor_cancel (GFileMonitor* monitor);

#define g_inotify_directory_monitor_get_type _g_inotify_directory_monitor_get_type
G_DEFINE_TYPE_WITH_CODE (GInotifyDirectoryMonitor, g_inotify_directory_monitor, G_TYPE_LOCAL_DIRECTORY_MONITOR,
			 g_io_extension_point_implement (G_LOCAL_DIRECTORY_MONITOR_EXTENSION_POINT_NAME,
							 g_define_type_id,
							 "inotify",
							 20))

static void
g_inotify_directory_monitor_finalize (GObject *object)
{
  GInotifyDirectoryMonitor *inotify_monitor = G_INOTIFY_DIRECTORY_MONITOR (object);
  inotify_sub *sub = inotify_monitor->sub;

  if (sub)
    {
      _ih_sub_cancel (sub);
      _ih_sub_free (sub);
      inotify_monitor->sub = NULL;
    }

  G_OBJECT_CLASS (g_inotify_directory_monitor_parent_class)->finalize (object);
}

static void
g_inotify_directory_monitor_start (GLocalDirectoryMonitor *local_monitor)
{
  GInotifyDirectoryMonitor *inotify_monitor = G_INOTIFY_DIRECTORY_MONITOR (local_monitor);
  const gchar *dirname = NULL;
  inotify_sub *sub = NULL;
  gboolean ret_ih_startup; /* return value of _ih_startup, for asserting */
  gboolean pair_moves;

  dirname = local_monitor->dirname;
  g_assert (dirname != NULL);

  /* Will never fail as is_supported() should be called before instantiating
   * anyway */
  /* assert on return value */
  ret_ih_startup = _ih_startup();
  g_assert (ret_ih_startup);

  pair_moves = local_monitor->flags & G_FILE_MONITOR_SEND_MOVED;

  sub = _ih_sub_new (dirname, NULL, pair_moves, FALSE, inotify_monitor);
  /* FIXME: what to do about errors here? we can't return NULL or another
   * kind of error and an assertion is probably too hard */
  g_assert (sub != NULL);

  /* _ih_sub_add allways returns TRUE, see gio/inotify/inotify-helper.c line 109
   * g_assert (_ih_sub_add (sub)); */
  _ih_sub_add(sub);

  inotify_monitor->sub = sub;
}

static gboolean
g_inotify_directory_monitor_is_supported (void)
{
  return _ih_startup ();
}

static void
g_inotify_directory_monitor_class_init (GInotifyDirectoryMonitorClass* klass)
{
  GObjectClass* gobject_class = G_OBJECT_CLASS (klass);
  GFileMonitorClass *directory_monitor_class = G_FILE_MONITOR_CLASS (klass);
  GLocalDirectoryMonitorClass *local_directory_monitor_class = G_LOCAL_DIRECTORY_MONITOR_CLASS (klass);

  gobject_class->finalize = g_inotify_directory_monitor_finalize;
  directory_monitor_class->cancel = g_inotify_directory_monitor_cancel;

  local_directory_monitor_class->mount_notify = TRUE;
  local_directory_monitor_class->is_supported = g_inotify_directory_monitor_is_supported;
  local_directory_monitor_class->start = g_inotify_directory_monitor_start;
}

static void
g_inotify_directory_monitor_init (GInotifyDirectoryMonitor* monitor)
{
}

static gboolean
g_inotify_directory_monitor_cancel (GFileMonitor* monitor)
{
  GInotifyDirectoryMonitor *inotify_monitor = G_INOTIFY_DIRECTORY_MONITOR (monitor);
  inotify_sub *sub = inotify_monitor->sub;

  if (sub) 
    {
      _ih_sub_cancel (sub);
      _ih_sub_free (sub);
      inotify_monitor->sub = NULL;
    }

  if (G_FILE_MONITOR_CLASS (g_inotify_directory_monitor_parent_class)->cancel)
    (*G_FILE_MONITOR_CLASS (g_inotify_directory_monitor_parent_class)->cancel) (monitor);

  return TRUE;
}
