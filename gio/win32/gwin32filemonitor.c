/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright (C) 2006-2015 Red Hat, Inc.
 * Copyright (C) 2015 Chun-wei Fan
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
 * Author: Vlad Grecescu <b100dian@gmail.com>
 * Author: Chun-wei Fan <fanc999@yahoo.com.tw>
 *
 */

#include "config.h"

#include "gwin32filemonitor.h"
#include "gwin32filemonitorutils.h"

G_DEFINE_TYPE_WITH_CODE (GWin32FileMonitor, g_win32_file_monitor, G_TYPE_LOCAL_FILE_MONITOR,
                         g_io_extension_point_implement (G_LOCAL_FILE_MONITOR_EXTENSION_POINT_NAME,
                                                         g_define_type_id, "win32filemonitor", 20))

static void
g_win32_file_monitor_close_handle (HANDLE hDirectory)
{
  /* This triggers a last callback() with nBytes==0. */

  /* Actually I am not so sure about that, it seems to trigger a last
   * callback allright, but the way to recognize that it is the final
   * one is not to check for nBytes==0, I think that was a
   * misunderstanding.
   */
  if (hDirectory != INVALID_HANDLE_VALUE)
    {
      CloseHandle (hDirectory);
      hDirectory = INVALID_HANDLE_VALUE;
    }
}

static void
g_win32_file_monitor_free (GWin32FileMonitorInfo *info)
{
  if (info != NULL)
    {
      g_free (info->shortname);
      g_free (info->longname);
      g_free (info->name);
      g_free (info->dirname_with_long_prefix);
      g_free (info);
    }
}

static GWin32FileMonitorPrivate*
g_win32_file_monitor_create (void)
{
  GWin32FileMonitorPrivate* monitor = (GWin32FileMonitorPrivate*) g_new0 (GWin32FileMonitorPrivate, 1);
  g_assert (monitor != 0);

  monitor->buffer_allocated_bytes = 32784;
  monitor->file_notify_buffer = g_new0 (FILE_NOTIFY_INFORMATION, monitor->buffer_allocated_bytes);
  g_assert (monitor->file_notify_buffer);

  return monitor;
}

static void
g_win32_file_monitor_start (GLocalFileMonitor *monitor,
                            const gchar *dirname,
                            const gchar *basename,
                            const gchar *filename,
                            GFileMonitorSource *source)
{
  GWin32FileMonitor *win32_monitor = G_WIN32_FILE_MONITOR (monitor);
  gboolean isfile = (filename == NULL && basename == NULL) ? FALSE : TRUE;

  win32_monitor->priv->fms = source;
  win32_monitor->priv->ht_watched_names =
    g_hash_table_new_full (g_str_hash,
                           g_str_equal,
                           g_free,
                           g_win32_file_monitor_free);

  win32_monitor->priv->ht_watched_dirs =
    g_hash_table_new_full (g_direct_hash,
                           g_direct_equal,
                           g_win32_file_monitor_close_handle,
                           g_free);

  if (!isfile)
    win32_monitor->priv->ht_files_attribs =
      g_hash_table_new_full (g_str_hash,
                             g_str_equal,
                             g_free,
                             NULL);

  if (isfile)
    if (basename != NULL)
      g_win32_file_monitor_prepare (win32_monitor->priv, dirname, basename, TRUE);
    else
      g_win32_file_monitor_prepare (win32_monitor->priv, NULL, filename, TRUE);
  else
    g_win32_file_monitor_prepare (win32_monitor->priv, dirname, NULL, FALSE);
}

static gboolean
g_win32_file_monitor_is_supported (void)
{
  return TRUE;
}

static void
g_win32_file_monitor_init (GWin32FileMonitor* monitor)
{
  monitor->priv = g_win32_file_monitor_create ();

  monitor->priv->self = G_FILE_MONITOR (monitor);
}

static void
g_win32_destroy_monitor_hashtables (GWin32FileMonitorPrivate *monitor)
{
  if (monitor->ht_files_attribs != NULL)
    {
      GList *l_attrib_keys = g_hash_table_get_keys (monitor->ht_files_attribs);
      for (;l_attrib_keys != NULL; l_attrib_keys = l_attrib_keys->next)
        {
          GHashTable *ht_attribs = g_hash_table_lookup (monitor->ht_files_attribs,
                                                        l_attrib_keys->data);
          if (ht_attribs != NULL)
            g_hash_table_destroy (ht_attribs);
        }
    }

  if (monitor->ht_watched_names != NULL)
    {
      g_hash_table_destroy (monitor->ht_watched_names);
      monitor->ht_watched_names = NULL;
    }

  if (monitor->ht_watched_dirs != NULL)
    {
      g_hash_table_destroy (monitor->ht_watched_dirs);
      monitor->ht_watched_dirs = NULL;
    }
}

static void
g_win32_file_monitor_finalize (GObject *base)
{
  GWin32FileMonitor *monitor;
  monitor = G_WIN32_FILE_MONITOR (base);

  g_win32_destroy_monitor_hashtables (monitor->priv);

  g_free (monitor->priv->file_notify_buffer);
  monitor->priv->file_notify_buffer = NULL;

  g_free (monitor->priv);

  if (G_OBJECT_CLASS (g_win32_file_monitor_parent_class)->finalize)
    (*G_OBJECT_CLASS (g_win32_file_monitor_parent_class)->finalize) (base);
}

static gboolean
g_win32_file_monitor_cancel (GFileMonitor* monitor)
{
  GWin32FileMonitor *file_monitor = G_WIN32_FILE_MONITOR (monitor);

  g_win32_destroy_monitor_hashtables (file_monitor->priv);

  return TRUE;
}

static void
g_win32_file_monitor_class_init (GWin32FileMonitorClass *klass)
{
  GObjectClass* gobject_class = G_OBJECT_CLASS (klass);
  GFileMonitorClass *file_monitor_class = G_FILE_MONITOR_CLASS (klass);
  GLocalFileMonitorClass *local_file_monitor_class = G_LOCAL_FILE_MONITOR_CLASS (klass);

  gobject_class->finalize = g_win32_file_monitor_finalize;
  file_monitor_class->cancel = g_win32_file_monitor_cancel;

  local_file_monitor_class->is_supported = g_win32_file_monitor_is_supported;
  local_file_monitor_class->start = g_win32_file_monitor_start;
}
