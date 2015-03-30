/* GIO - GLib Input, Output and Streaming Library
 *
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
 * Author: Chun-wei Fan <fanc999@yahoo.com.tw>
 *
 */

#include "config.h"

#include "gwin32filemonitorevents.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static gboolean
g_win32_file_monitor_handle_event (GWin32FileMonitorPrivate *monitor,
                                   gchar *filename,
                                   PFILE_NOTIFY_INFORMATION pfni,
                                   gboolean is_renamed_to)
{
  GFileMonitorEvent fme;
  PFILE_NOTIFY_INFORMATION pfni_next;
  gchar *from_to_file = NULL;
  glong file_name_len = 0;
  gboolean reacquire_names = FALSE;
  gchar *fullpath = g_hash_table_lookup (monitor->ht_watched_dirs, monitor->hDirectory);
  GWin32FileMonitorInfo *info = g_hash_table_lookup (monitor->ht_watched_names, fullpath);

  gboolean is_attrib_changed = FALSE;

  gboolean update_all_attribs = FALSE;
  gchar *update_file = NULL;

  switch (pfni->Action)
    {
      case FILE_ACTION_ADDED:
        fme = G_FILE_MONITOR_EVENT_CREATED;
        reacquire_names = TRUE;
        break;

      case FILE_ACTION_REMOVED:
        fme = G_FILE_MONITOR_EVENT_DELETED;
        reacquire_names = TRUE;
        break;

      case FILE_ACTION_MODIFIED:
        if (info->isfile)
          is_attrib_changed = g_win32_file_monitor_check_attrib_changed (info, filename, NULL);
        else
          {
            GHashTable *ht_attribs =  g_hash_table_lookup (monitor->ht_files_attribs, fullpath);
            is_attrib_changed = g_win32_file_monitor_check_attrib_changed (info, filename, ht_attribs);
            if (is_attrib_changed)
              {
                g_hash_table_remove (monitor->ht_files_attribs, fullpath);
                g_hash_table_insert (monitor->ht_files_attribs, g_strdup (fullpath), ht_attribs);
              }
          }
        if (is_attrib_changed)
          fme = G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED;
        else
          fme = G_FILE_MONITOR_EVENT_CHANGED;

        break;

      case FILE_ACTION_RENAMED_OLD_NAME:
        if (pfni->NextEntryOffset != 0)
          {
            /* If the file was renamed in the same directory, we would get a
             * FILE_ACTION_RENAMED_NEW_NAME action in the next FILE_NOTIFY_INFORMATION
             * structure.
             */

            pfni_next = (PFILE_NOTIFY_INFORMATION) ((BYTE*)pfni + pfni->NextEntryOffset);
            if (pfni_next->Action == FILE_ACTION_RENAMED_NEW_NAME)
              {
                from_to_file = g_utf16_to_utf8 (pfni_next->FileName,
                                                pfni_next->FileNameLength / sizeof(WCHAR),
                                                NULL, &file_name_len,
                                                NULL);

                fme = G_FILE_MONITOR_EVENT_RENAMED;
              }
            else
               fme = G_FILE_MONITOR_EVENT_MOVED_OUT;
          }
        else
          fme = G_FILE_MONITOR_EVENT_MOVED_OUT;

        reacquire_names = TRUE;
        break;

      case FILE_ACTION_RENAMED_NEW_NAME:
        if (monitor->pfni_prev != NULL &&
            monitor->pfni_prev->Action == FILE_ACTION_RENAMED_OLD_NAME)
          {
            if (is_renamed_to)
              {
                from_to_file = g_utf16_to_utf8 (monitor->pfni_prev->FileName,
                                                monitor->pfni_prev->FileNameLength / sizeof(WCHAR),
                                                NULL,
                                                &file_name_len,
                                                NULL);
                fme = G_FILE_MONITOR_EVENT_RENAMED;
              }
            else
              /* don't bother sending events, was already sent (renamed from monitored file) */
              fme = -1;
          }
        else
          fme = G_FILE_MONITOR_EVENT_MOVED_IN;

        reacquire_names = TRUE;

        break;

      default:
        /* The possible Windows actions are all above, so shouldn't get here */
        g_assert_not_reached ();
        break;
    }
  if (fme != -1)
    {
      gboolean interesting;

      if (info->isfile)
        {
          if (pfni->Action == FILE_ACTION_RENAMED_NEW_NAME &&
              is_renamed_to)
            {
              /* we are renaming to the monitored file */
              interesting = g_file_monitor_source_handle_event (monitor->fms,
                                                                fme,
                                                                from_to_file,
                                                                filename,
                                                                NULL,
                                                                g_get_monotonic_time ());
            }
          else
            {
              interesting = g_file_monitor_source_handle_event (monitor->fms,
                                                                fme,
                                                                filename,
                                                                from_to_file,
                                                                NULL,
                                                                g_get_monotonic_time ());
            }
        }
      else
        {
          gchar *from_to_file_stripped = NULL;
          gchar *filename_stripped = NULL;

          if (filename != NULL)
            {
              if (strrchr (filename, G_DIR_SEPARATOR) != NULL)
                filename_stripped = g_strdup (strrchr (filename, G_DIR_SEPARATOR) + 1);
              else
                filename_stripped = g_strdup (filename);
            }
          if (from_to_file != NULL)
            {
              if (strrchr (from_to_file, G_DIR_SEPARATOR) != NULL)
                from_to_file_stripped = g_strdup (strrchr (from_to_file, G_DIR_SEPARATOR) + 1);
              else
                from_to_file_stripped = g_strdup (from_to_file);
            }

          if (pfni->Action == FILE_ACTION_RENAMED_NEW_NAME &&
              is_renamed_to)
            {
              /* we are renaming to the monitored file */
              interesting = g_file_monitor_source_handle_event (monitor->fms,
                                                                fme,
                                                                from_to_file_stripped,
                                                                filename_stripped,
                                                                NULL,
                                                                g_get_monotonic_time ());
            }
          else
            {
              interesting = g_file_monitor_source_handle_event (monitor->fms,
                                                                fme,
                                                                filename_stripped,
                                                                from_to_file_stripped,
                                                                NULL,
                                                                g_get_monotonic_time ());
            }
          if (filename_stripped != NULL)
            g_free (filename_stripped);
          if (from_to_file_stripped != NULL)
            g_free (from_to_file_stripped);
        }

      if (from_to_file != NULL)
        g_free (from_to_file);
      if (reacquire_names)
        {
          g_win32_file_monitor_set_names (info);
          if (!info->isfile)
            g_win32_file_monitor_dir_refresh_attribs (monitor, info);
        }

      return interesting;
    }
  else
    return FALSE;
}

void CALLBACK
g_win32_file_monitor_callback (DWORD        error,
                               DWORD        nBytes,
                               LPOVERLAPPED lpOverlapped)
{
  gulong offset;
  PFILE_NOTIFY_INFORMATION pfile_notify_walker;

  GWin32FileMonitorPrivate *monitor = (GWin32FileMonitorPrivate *) lpOverlapped;
  GWin32FileMonitorInfo *info;

  DWORD notify_filter;

  gchar *fullpath;
  gchar *changed_file;
  glong file_name_len;
  GHashTable *ht_attribs;

  fullpath = g_hash_table_lookup (monitor->ht_watched_dirs, monitor->hDirectory);

  info = g_hash_table_lookup (monitor->ht_watched_names, fullpath);

  notify_filter = info->isfile ?
                  (FILE_NOTIFY_CHANGE_FILE_NAME |
                   FILE_NOTIFY_CHANGE_DIR_NAME |
                   FILE_NOTIFY_CHANGE_ATTRIBUTES |
                   FILE_NOTIFY_CHANGE_SIZE) :
                  (FILE_NOTIFY_CHANGE_FILE_NAME |
                   FILE_NOTIFY_CHANGE_DIR_NAME |
                   FILE_NOTIFY_CHANGE_ATTRIBUTES |
                   FILE_NOTIFY_CHANGE_SIZE);
  if (!info->isfile)
    ht_attribs = g_hash_table_lookup (monitor->ht_files_attribs,
                                      fullpath);

  /* If monitor->self is NULL the GWin32FileMonitor object has been destroyed. */
  if (monitor->self == NULL ||
      g_file_monitor_is_cancelled (monitor->self) ||
      monitor->file_notify_buffer == NULL)
    {
      g_free (monitor->file_notify_buffer);

      g_free (monitor);
      return;
    }

  offset = 0;

  do
    {
      pfile_notify_walker = (PFILE_NOTIFY_INFORMATION)((BYTE*)monitor->file_notify_buffer + offset);
      if (pfile_notify_walker->Action > 0)
        {
          gboolean is_rename = FALSE;
          gboolean is_renamed_to = FALSE;
          gboolean is_handle_event = FALSE;
          gchar *shortname_casefold, *longname_casefold, *changed_file_casefold;

          gint long_filename_length = g_utf8_strlen (info->longname, -1);
          gint short_filename_length = g_utf8_strlen (info->shortname, -1);
          PFILE_NOTIFY_INFORMATION pfni_next =
            (PFILE_NOTIFY_INFORMATION)((BYTE*)pfile_notify_walker + pfile_notify_walker->NextEntryOffset);

          gchar *longfilename_with_path, *shortfilename_with_path, *changed_file_with_path;

          longname_casefold = g_utf8_casefold (info->longname, -1);
          shortname_casefold = g_utf8_casefold (info->shortname, -1);

          longfilename_with_path = g_strconcat (info->dirname_with_long_prefix,
                                                G_DIR_SEPARATOR_S,
                                                longname_casefold,
                                                NULL);
          shortfilename_with_path = g_strconcat (info->dirname_with_long_prefix,
                                                 G_DIR_SEPARATOR_S,
                                                 shortname_casefold,
                                                 NULL);

          changed_file = g_utf16_to_utf8 (pfile_notify_walker->FileName,
                                          pfile_notify_walker->FileNameLength / sizeof(WCHAR),
                                          NULL,
                                          &file_name_len,
                                          NULL);
          changed_file_casefold = g_utf8_casefold (changed_file, -1);
          changed_file_with_path = g_strconcat (info->dirname_with_long_prefix,
                                                G_DIR_SEPARATOR_S,
                                                changed_file_casefold,
                                                NULL);

          if (pfile_notify_walker->Action == FILE_ACTION_RENAMED_OLD_NAME &&
              pfni_next->Action == FILE_ACTION_RENAMED_NEW_NAME)
            {
              is_rename = TRUE;
            }

          /* If monitoring a file, check that the changed file
           * in the directory matches the file that is to be monitored
           * We need to check both the long and short file names for the same file, as
           * ReadDirectoryChangesW() could return either one of them.
           *
           * We need to send in the name of the monitored file, not its long (or short) variant,
           * if such long and/or short filenames exist.
           */
          if (info->isfile)
            is_handle_event = g_str_equal (longfilename_with_path, changed_file_with_path) ||
                              g_str_equal (shortfilename_with_path, changed_file_with_path);

          else
            {
              /* don't monitor next directory, at least for now */
              gint changed_file_length = g_utf8_strlen (g_utf8_strrchr(changed_file_with_path, -1, G_DIR_SEPARATOR), -1);
              gchar *changed_dir = g_utf8_substring (changed_file_with_path,
                                                     0,
                                                     g_utf8_strlen (changed_file_with_path, -1) - changed_file_length);

              is_handle_event = g_str_equal (longfilename_with_path, changed_file_with_path) ||
                                g_str_equal (shortfilename_with_path, changed_file_with_path) ||
                                g_str_equal (longfilename_with_path, changed_dir) ||
                                g_str_equal (shortfilename_with_path, changed_dir);

              g_free (changed_dir);
            }

          if (is_rename = TRUE &&
              monitor->pfni_prev != NULL &&
              monitor->pfni_prev->Action == FILE_ACTION_RENAMED_OLD_NAME &&
              pfile_notify_walker->Action == FILE_ACTION_RENAMED_NEW_NAME)
            {
              gchar *next_file = g_utf16_to_utf8 (pfile_notify_walker->FileName,
                                                  pfile_notify_walker->FileNameLength / sizeof(WCHAR),
                                                  NULL,
                                                  &file_name_len,
                                                  NULL);
              gchar *next_file_with_path = g_strconcat (info->dirname_with_long_prefix,
                                                        G_DIR_SEPARATOR_S,
                                                        next_file,
                                                        NULL);
              if (g_str_equal (longfilename_with_path, next_file_with_path) ||
                  g_str_equal (shortfilename_with_path, next_file_with_path))
                {
                  is_renamed_to = TRUE;
                  is_handle_event = TRUE;
                }
              g_free (next_file);
            }
          g_free (longfilename_with_path);
          g_free (shortfilename_with_path);
          g_free (changed_file_with_path);

          if (is_handle_event)
            {
              if (info->isfile)
                g_win32_file_monitor_handle_event (monitor,
                                                   info->name,
                                                   pfile_notify_walker,
                                                   is_renamed_to);
              else
                g_win32_file_monitor_handle_event (monitor,
                                                   changed_file,
                                                   pfile_notify_walker,
                                                   is_renamed_to);
            }
          g_free (changed_file);
        }
      monitor->pfni_prev = pfile_notify_walker;
      offset += pfile_notify_walker->NextEntryOffset;
    }
  while (pfile_notify_walker->NextEntryOffset);

  ReadDirectoryChangesW (monitor->hDirectory,
                         monitor->file_notify_buffer,
                         monitor->buffer_allocated_bytes,
                         !info->isfile,
                         notify_filter,
                         &monitor->buffer_filled_bytes,
                         &monitor->overlapped,
                         g_win32_file_monitor_callback);
}
