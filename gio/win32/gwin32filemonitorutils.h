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

#ifndef __G_WIN32_FILE_MONITOR_UTILS_H__
#define __G_WIN32_FILE_MONITOR_UTILS_H__

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "gio/glocalfilemonitor.h"

#include "gio/gfilemonitor.h"

#define MAX_PATH_LONG 32767 /* Support Paths longer than MAX_PATH (260) characters */
#define LONGPFX "\\\\?\\" /* MSDN: Append Paths with \\?\ to support over paths over 256 characters */
#define G_DIR_SEPARATOR_W L'\\'
#define STRIP_PFX_LENGTH g_utf8_strlen (LONGPFX, -1)

G_BEGIN_DECLS

typedef struct _GWin32FileMonitorPrivate GWin32FileMonitorPrivate;
typedef struct _GWin32FileMonitorInfo GWin32FileMonitorInfo;
typedef struct _GWin32FileMonitorDirInfo GWin32FileMonitorDirInfo;

struct _GWin32FileMonitorPrivate
{
  OVERLAPPED overlapped;
  DWORD buffer_allocated_bytes;
  PFILE_NOTIFY_INFORMATION file_notify_buffer;
  DWORD buffer_filled_bytes;
  HANDLE hDirectory;
  gboolean isfile;
  GHashTable *ht_watched_dirs;
  GHashTable *ht_watched_names;
  GHashTable *ht_files_attribs;
  DWORD file_attribs;
  PFILE_NOTIFY_INFORMATION pfni_prev;
  /* Needed in the APC where we only have this private struct */
  GFileMonitor *self;
  GFileMonitorSource *fms;
};

struct _GWin32FileMonitorInfo
{
  gchar *name;
  gchar *dirname_with_long_prefix;
  gchar *longname;
  gchar *shortname;
  gboolean isfile;
  WIN32_FILE_ATTRIBUTE_DATA attribs;
};

void g_win32_file_monitor_prepare (GWin32FileMonitorPrivate *monitor,
                                   gchar *dirname,
                                   gchar *filename,
                                   gboolean isfile);

void g_win32_file_monitor_set_names (GWin32FileMonitorInfo *info);

GHashTable* g_win32_file_monitor_dir_refresh_attribs (GWin32FileMonitorPrivate *monitor,
                                                      GWin32FileMonitorInfo *info);

gboolean g_win32_file_monitor_check_attrib_changed (GWin32FileMonitorInfo *info,
                                                    gchar *filename,
                                                    GHashTable *ht_attribs);

G_END_DECLS

#endif
