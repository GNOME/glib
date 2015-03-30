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

#include "gwin32filemonitorutils.h"
#include "gwin32filemonitorevents.h"
#include "gio/gfile.h"

/*
 * We are using the Wide/Unicode (W) variants of the Windows APIs as:
 * -There isn't a ReadDirectoryChangesA()
 * -We want to support long paths, over 256 characters, and using the W variants is required in this case
 */

/*
 * Note: We don't support monitoring for "normal" unmounts in Windows as the Windows file/directory monitoring
 * mechanism will disallow the unmounting of the monitored volume (unless of course we pull out the USB stick,
 * for instance, without ejecting it
 */

/*
 * On Windows, there is the notion of hard links, but they are treated more or less like standard files, but any
 * changes to them will also cause a notification to be sent for the directory where the original file resides,
 * so ReadDirectoryChangesW() also works with hard links, though transparently, so there isn't a special code path
 * for hard links here.  The notification, however, may be sent only when the file (or hard link) in question
 * is being accessed.
 */

gboolean
g_win32_file_monitor_long_pfx_needed (const gchar* path)
{
  if (g_utf8_strlen (path, -1) > MAX_PATH)
    return TRUE;
  else
    return FALSE;
}

GHashTable*
g_win32_file_monitor_dir_refresh_attribs (GWin32FileMonitorPrivate *monitor,
                                          GWin32FileMonitorInfo *info)
{
  GHashTable *ht_attribs;
  WIN32_FILE_ATTRIBUTE_DATA *attrib_data;
  gchar *fullpath, *dirname, *dirname_casefold;
  wchar_t *wfullpath;
  HANDLE hFind = INVALID_HANDLE_VALUE;
  WIN32_FIND_DATA file_data = {0,};

  if (info->isfile)
    return NULL;

  fullpath = g_strconcat (info->dirname_with_long_prefix,
                          G_DIR_SEPARATOR_S,
                          info->name,
                          G_DIR_SEPARATOR_S,
                          "*",
                          NULL);
  wfullpath = g_utf8_to_utf16 (fullpath, -1, NULL, NULL, NULL);
  dirname = g_strconcat (info->dirname_with_long_prefix + STRIP_PFX_LENGTH,
                         G_DIR_SEPARATOR_S,
                         info->name,
                         NULL);
  dirname_casefold = g_utf8_casefold (dirname, -1);

  hFind = FindFirstFileExW (wfullpath,
                            FindExInfoBasic,
                            &file_data,
                            0,
                            NULL,
                            0);

  ht_attribs = g_hash_table_lookup (monitor->ht_files_attribs, dirname_casefold);
  if (ht_attribs == NULL)
    {
      ht_attribs = g_hash_table_new_full (g_str_hash,
                                          g_str_equal,
                                          g_free,
                                          g_free);
    }
  else
    g_hash_table_remove_all (ht_attribs);

  g_free (dirname_casefold);
  g_free (dirname);

  if (hFind == INVALID_HANDLE_VALUE)
    /*
     * Initially, we won't get here, if we started monitoring on a non-existing directory.
     * If we do later on, the directory was moved or deleted, forget about updating the info
     */
     return NULL;
  else
    {
      do
        {
          attrib_data = g_new0 (WIN32_FILE_ATTRIBUTE_DATA, 1);
          attrib_data->dwFileAttributes = file_data.dwFileAttributes;
          attrib_data->ftCreationTime = file_data.ftCreationTime;
          attrib_data->ftLastAccessTime = file_data.ftLastAccessTime;
          attrib_data->ftLastWriteTime = file_data.ftLastWriteTime;
          attrib_data->nFileSizeHigh = file_data.nFileSizeHigh;
          attrib_data->nFileSizeLow = file_data.nFileSizeLow;

          g_hash_table_insert (ht_attribs,
                               g_utf8_casefold (file_data.cFileName, -1),
                               attrib_data);
        }
      while (FindNextFile (hFind, &file_data));
      FindClose (hFind);
    }
  return ht_attribs;
}

/*
 * ReadDirectoryChangesW() can return the normal filename or the
 * "8.3" format filename, so we need to keep track of both these names
 * so that we can check against them later when it returns
 */
void
g_win32_file_monitor_set_names (GWin32FileMonitorInfo *info)
{
  wchar_t *wfullpath, *wbasename_long, *wbasename_short;
  wchar_t wlongname[MAX_PATH_LONG];
  wchar_t wshortname[MAX_PATH_LONG];
  gboolean success_attribs;

  gchar *fullpath_with_long_pfx = g_strconcat (info->dirname_with_long_prefix,
                                               G_DIR_SEPARATOR_S,
                                               info->name,
                                               NULL);

  wfullpath = g_utf8_to_utf16 (fullpath_with_long_pfx, -1, NULL, NULL, NULL);
  if (info->longname != NULL)
    g_free (info->longname);
  if (info->shortname != NULL)
    g_free (info->shortname);

  /*
   * Get long and short paths of the specified path, if it exists, otherwise
   * just assume the filename part in the passed in filename and directory as
   * the long and short filenames
   */
  if (GetLongPathNameW (wfullpath, wlongname, MAX_PATH_LONG) != 0)
    {
      wbasename_long = wcsrchr (wlongname, G_DIR_SEPARATOR_W) != NULL ?
                       wcsrchr (wlongname, G_DIR_SEPARATOR_W) + 1 :
                       wlongname + STRIP_PFX_LENGTH;
    }
  else
    {
      wbasename_long = wcsrchr (wfullpath, G_DIR_SEPARATOR_W) != NULL ?
                       wcsrchr (wfullpath, G_DIR_SEPARATOR_W) + 1 :
                       wfullpath + STRIP_PFX_LENGTH;
    }
  if (GetShortPathNameW (wfullpath, wshortname, MAX_PATH_LONG) != 0)
    {
      wbasename_short = wcsrchr (wshortname, G_DIR_SEPARATOR_W) != NULL ?
                        wcsrchr (wshortname, G_DIR_SEPARATOR_W) + 1 :
                        wshortname + STRIP_PFX_LENGTH;
    }
  else
    {
      wbasename_short = wcsrchr (wfullpath, G_DIR_SEPARATOR_W) != NULL ?
                        wcsrchr (wfullpath, G_DIR_SEPARATOR_W) + 1 :
                        wfullpath + STRIP_PFX_LENGTH;
    }

  info->longname = g_utf16_to_utf8 (wbasename_long, -1, NULL, NULL, NULL);
  info->shortname = g_utf16_to_utf8 (wbasename_short, -1, NULL, NULL, NULL);

  /* Store up current file attributes */
  success_attribs = GetFileAttributesExW (wfullpath,
                                          GetFileExInfoStandard,
                                          &info->attribs);

  g_free (wfullpath);
  g_free (fullpath_with_long_pfx);

  if (!success_attribs)
    info->attribs.dwFileAttributes = INVALID_FILE_ATTRIBUTES;
}

gboolean
g_win32_file_monitor_check_attrib_changed (GWin32FileMonitorInfo *info,
                                           gchar *filename,
                                           GHashTable *ht_attribs)
{
  gchar *fullpath_with_long_prefix = g_strconcat (info->dirname_with_long_prefix,
                                                  G_DIR_SEPARATOR_S,
                                                  filename,
                                                  NULL);
  wchar_t *wfullpath_with_long_prefix = g_utf8_to_utf16 (fullpath_with_long_prefix, -1, NULL, NULL, NULL);
  gboolean is_attribs_changed = FALSE;
  WIN32_FILE_ATTRIBUTE_DATA attrib_data = {0, };
  gboolean success_attribs = GetFileAttributesExW (wfullpath_with_long_prefix,
                                                   GetFileExInfoStandard,
                                                   &attrib_data);

  if (!success_attribs)
    attrib_data.dwFileAttributes = INVALID_FILE_ATTRIBUTES;

  if (info->isfile)
    {
      /*
       * Monitoring a file: Simply check if attributes changed, or file changed, and
       * store up the new attributes for the file if needed.
       */
      if (info->attribs.dwFileAttributes != INVALID_FILE_ATTRIBUTES &&
          success_attribs &&
          attrib_data.dwFileAttributes != info->attribs.dwFileAttributes)
        {
          is_attribs_changed = TRUE;
          info->attribs = attrib_data;
        }
      return is_attribs_changed;
    }
  else
    {
      /*
       * Monitoring directories: The situation is more complex, we need to first see whether
       * a file changed, or a directory changed.  If it is just the directory that changed,
       * than it's like the file case, that is to check only the attribute of the directory.
       * If it is on a file in a directory, we need to check against the attributes of the file,
       * against the attribues of the files in the directory, which we stored up when we were
       * creating the monitor object, and update our stored records on that file if the attributes
       * of that file changed.
       */

      WIN32_FILE_ATTRIBUTE_DATA *attrib_data_orig;
      gchar *filename_casefold;

      if (g_utf8_strrchr (filename, -1, G_DIR_SEPARATOR) != NULL)
        {
          /* A file in the monitored directory changed */
          gboolean attribs_success;
          gchar *longname;
          wchar_t wfullpath_long[MAX_PATH_LONG];

          WIN32_FILE_ATTRIBUTE_DATA *curr_attrib_data = g_new0 (WIN32_FILE_ATTRIBUTE_DATA, 1);
          gchar *fullpath = g_strconcat (info->dirname_with_long_prefix,
                                         G_DIR_SEPARATOR_S,
                                         info->name,
                                         G_DIR_SEPARATOR_S,
                                         g_utf8_strrchr (filename, -1, G_DIR_SEPARATOR) + 1,
                                         NULL);
          wchar_t *wfullpath = g_utf8_to_utf16 (fullpath, -1, NULL, NULL, NULL);

          attribs_success = GetFileAttributesExW (wfullpath,
                                                  GetFileExInfoStandard,
                                                  curr_attrib_data);

          if (!attribs_success)
            {
              curr_attrib_data->dwFileAttributes = INVALID_FILE_ATTRIBUTES;
              longname = g_strdup (g_utf8_strrchr (longname, -1, G_DIR_SEPARATOR) + 1);
            }
          else
            {
              if (GetLongPathNameW (wfullpath, wfullpath_long, MAX_PATH_LONG) != 0)
                longname = g_utf16_to_utf8 (wcsrchr (wfullpath_long, G_DIR_SEPARATOR_W) + 1,
                                            -1,
                                            NULL,
                                            NULL,
                                            NULL);
              else
                longname = g_strdup (g_utf8_strrchr (longname, -1, G_DIR_SEPARATOR) + 1);
            }

          filename_casefold = g_utf8_casefold (longname, -1);
          g_free (longname);
          attrib_data_orig = g_hash_table_lookup (ht_attribs, filename_casefold);

          if (attrib_data_orig->dwFileAttributes != INVALID_FILE_ATTRIBUTES &&
              success_attribs &&
              curr_attrib_data->dwFileAttributes != attrib_data_orig->dwFileAttributes)
            {
              is_attribs_changed = TRUE;
            }

          if (is_attribs_changed)
            g_hash_table_replace (ht_attribs, g_strdup (filename_casefold), curr_attrib_data);
          else
            g_free (curr_attrib_data);

          g_free (filename_casefold);
          return is_attribs_changed;
        }
      else
        {
          /* The monitored directory itself changed */
          if (info->attribs.dwFileAttributes != INVALID_FILE_ATTRIBUTES &&
              success_attribs &&
              attrib_data.dwFileAttributes != info->attribs.dwFileAttributes)
            {
              is_attribs_changed = TRUE;
              info->attribs = attrib_data;
            }
          return is_attribs_changed;
        }
    }
}

static GWin32FileMonitorInfo *
g_win32_file_monitor_set_paths (GWin32FileMonitorPrivate *monitor,
                              const gchar *dirname,
                              const gchar *filename)
{
  GWin32FileMonitorInfo *info = g_new0 (GWin32FileMonitorInfo, 1);
  gboolean updated;

  if (filename != NULL)
    {
      /* monitor a file */
      gchar *fullpath;
      info->isfile = TRUE;
      info->dirname_with_long_prefix = g_strconcat (LONGPFX, dirname, NULL);
      info->name = g_strdup (filename);
      fullpath = g_strconcat (dirname,
                              G_DIR_SEPARATOR_S,
                              info->name,
                              NULL);
      updated = g_hash_table_replace (monitor->ht_watched_names,
                                      g_utf8_casefold (fullpath, -1),
                                      info);
      g_free (fullpath);
    }
  else
    {
      /* monitor a directory */
      gchar *parentdir = g_malloc0 (g_utf8_strlen (dirname, -1) + 1);
      GHashTable *ht_attribs;

      g_utf8_strncpy (parentdir,
                      dirname,
                      g_utf8_strlen (dirname, -1) -
                      g_utf8_strlen (g_utf8_strrchr (dirname, -1, G_DIR_SEPARATOR), -1));

      info->isfile = FALSE;
      info->dirname_with_long_prefix = g_strconcat (LONGPFX, parentdir, NULL);
      info->name = g_strdup (g_utf8_strrchr (dirname, -1, G_DIR_SEPARATOR) + 1);

      g_free (parentdir);

      ht_attribs = g_win32_file_monitor_dir_refresh_attribs (monitor, info);

      if (ht_attribs != NULL)
        g_hash_table_replace (monitor->ht_files_attribs,
                              g_utf8_casefold (dirname, -1),
                              ht_attribs);
      g_hash_table_replace (monitor->ht_watched_names,
                            g_utf8_casefold (dirname, -1),
                            info);
    }

  g_win32_file_monitor_set_names (info);
  return info;
}

/* Set up call to ReadDirectoryChangesW() */
static void
g_win32_file_monitor_begin_monitor (GWin32FileMonitorPrivate *monitor,
                                  GWin32FileMonitorInfo *info)
{
  gchar *fullpath;
  wchar_t *wdirname;

  DWORD notify_filter = info->isfile ?
                        (FILE_NOTIFY_CHANGE_FILE_NAME |
                         FILE_NOTIFY_CHANGE_ATTRIBUTES |
                         FILE_NOTIFY_CHANGE_SIZE) :
                        (FILE_NOTIFY_CHANGE_FILE_NAME |
                         FILE_NOTIFY_CHANGE_DIR_NAME |
                         FILE_NOTIFY_CHANGE_ATTRIBUTES |
                         FILE_NOTIFY_CHANGE_SIZE);

  fullpath = g_strconcat (info->dirname_with_long_prefix + STRIP_PFX_LENGTH,
                          G_DIR_SEPARATOR_S,
                          info->name,
                          NULL);

  wdirname = g_utf8_to_utf16 (info->dirname_with_long_prefix, -1, NULL, NULL, NULL);

  monitor->hDirectory = CreateFileW (wdirname,
                                     FILE_GENERIC_READ | FILE_GENERIC_WRITE,
                                     FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE,
                                     NULL,
                                     OPEN_EXISTING,
                                     FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
                                     NULL);

  g_hash_table_insert (monitor->ht_watched_dirs,
                       monitor->hDirectory,
                       g_utf8_casefold (fullpath, -1));

  if (monitor->hDirectory != INVALID_HANDLE_VALUE)
    {
      ReadDirectoryChangesW (monitor->hDirectory,
                             monitor->file_notify_buffer,
                             monitor->buffer_allocated_bytes,
                             !info->isfile,
                             notify_filter,
                             &monitor->buffer_filled_bytes,
                             &monitor->overlapped,
                             g_win32_file_monitor_callback);
    }
}

void
g_win32_file_monitor_prepare (GWin32FileMonitorPrivate *monitor,
                         gchar *dirname,
                         gchar *filename,
                         gboolean isfile)
{
  GWin32FileMonitorInfo *info;
  gchar *fullpath_with_long_prefix, *dirname_with_long_prefix;
  DWORD notify_filter = isfile ?
                        (FILE_NOTIFY_CHANGE_FILE_NAME |
                         FILE_NOTIFY_CHANGE_ATTRIBUTES |
                         FILE_NOTIFY_CHANGE_SIZE) :
                        (FILE_NOTIFY_CHANGE_FILE_NAME |
                         FILE_NOTIFY_CHANGE_DIR_NAME |
                         FILE_NOTIFY_CHANGE_ATTRIBUTES |
                         FILE_NOTIFY_CHANGE_SIZE);

  monitor->pfni_prev = NULL;

  if (isfile)
    info = g_win32_file_monitor_set_paths (monitor, dirname, filename);
  else
    info = g_win32_file_monitor_set_paths (monitor, dirname, NULL);

  g_win32_file_monitor_begin_monitor (monitor, info);
}
