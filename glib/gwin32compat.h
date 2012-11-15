/* GLIB - Library of useful routines for C programming
 * Copyright (C) 2012 Red Hat, Inc.
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

#if !defined (__GLIB_H_INSIDE__) && !defined (GLIB_COMPILATION)
#error "Only <glib.h> can be included directly."
#endif

#ifndef __G_WIN32COMPAT_H__
#define __G_WIN32COMPAT_H__

#include <glib/gtypes.h>
#include <glib/gdir.h>
#include <glib/giochannel.h>
#include <glib/gspawn.h>

G_BEGIN_DECLS

#ifdef G_OS_WIN32

#define g_filename_to_utf8   g_filename_to_utf8_utf8
#define g_filename_from_utf8 g_filename_from_utf8_utf8
#define g_filename_from_uri  g_filename_from_uri_utf8
#define g_filename_to_uri    g_filename_to_uri_utf8

gchar* g_filename_to_utf8_utf8   (const gchar  *opsysstring,
                                  gssize        len,
                                  gsize        *bytes_read,
                                  gsize        *bytes_written,
                                  GError      **error) G_GNUC_MALLOC;
gchar* g_filename_from_utf8_utf8 (const gchar  *utf8string,
                                  gssize        len,
                                  gsize        *bytes_read,
                                  gsize        *bytes_written,
                                  GError      **error) G_GNUC_MALLOC;
gchar *g_filename_from_uri_utf8  (const gchar  *uri,
                                  gchar       **hostname,
                                  GError      **error) G_GNUC_MALLOC;
gchar *g_filename_to_uri_utf8    (const gchar  *filename,
                                  const gchar  *hostname,
                                  GError      **error) G_GNUC_MALLOC;


#define g_dir_open      g_dir_open_utf8
#define g_dir_read_name g_dir_read_name_utf8

GDir        *g_dir_open_utf8      (const gchar  *path,
                                   guint         flags,
                                   GError      **error);
const gchar *g_dir_read_name_utf8 (GDir         *dir);


#define g_getenv   g_getenv_utf8
#define g_setenv   g_setenv_utf8
#define g_unsetenv g_unsetenv_utf8

const gchar *g_getenv_utf8   (const gchar  *variable);
gboolean     g_setenv_utf8   (const gchar  *variable,
                              const gchar  *value,
                              gboolean      overwrite);
void         g_unsetenv_utf8 (const gchar  *variable);


#define g_file_test         g_file_test_utf8
#define g_file_get_contents g_file_get_contents_utf8
#define g_mkstemp           g_mkstemp_utf8
#define g_file_open_tmp     g_file_open_tmp_utf8
#define g_get_current_dir   g_get_current_dir_utf8

gboolean g_file_test_utf8         (const gchar  *filename,
                                   GFileTest     test);
gboolean g_file_get_contents_utf8 (const gchar  *filename,
                                   gchar       **contents,
                                   gsize        *length,
                                   GError      **error);
gint     g_mkstemp_utf8           (gchar        *tmpl);
gint     g_file_open_tmp_utf8     (const gchar  *tmpl,
                                   gchar       **name_used,
                                   GError      **error);
gchar   *g_get_current_dir_utf8   (void);


#define g_io_channel_new_file g_io_channel_new_file_utf8

GIOChannel *g_io_channel_new_file_utf8 (const gchar  *filename,
                                        const gchar  *mode,
                                        GError      **error);


#define g_spawn_async              g_spawn_async_utf8
#define g_spawn_async_with_pipes   g_spawn_async_with_pipes_utf8
#define g_spawn_sync               g_spawn_sync_utf8
#define g_spawn_command_line_sync  g_spawn_command_line_sync_utf8
#define g_spawn_command_line_async g_spawn_command_line_async_utf8

gboolean g_spawn_async_utf8              (const gchar           *working_directory,
                                          gchar                **argv,
                                          gchar                **envp,
                                          GSpawnFlags            flags,
                                          GSpawnChildSetupFunc   child_setup,
                                          gpointer               user_data,
                                          GPid                  *child_pid,
                                          GError               **error);
gboolean g_spawn_async_with_pipes_utf8   (const gchar           *working_directory,
                                          gchar                **argv,
                                          gchar                **envp,
                                          GSpawnFlags            flags,
                                          GSpawnChildSetupFunc   child_setup,
                                          gpointer               user_data,
                                          GPid                  *child_pid,
                                          gint                  *standard_input,
                                          gint                  *standard_output,
                                          gint                  *standard_error,
                                          GError               **error);
gboolean g_spawn_sync_utf8               (const gchar           *working_directory,
                                          gchar                **argv,
                                          gchar                **envp,
                                          GSpawnFlags            flags,
                                          GSpawnChildSetupFunc   child_setup,
                                          gpointer               user_data,
                                          gchar                **standard_output,
                                          gchar                **standard_error,
                                          gint                  *exit_status,
                                          GError               **error);

gboolean g_spawn_command_line_sync_utf8  (const gchar           *command_line,
                                          gchar                **standard_output,
                                          gchar                **standard_error,
                                          gint                  *exit_status,
                                          GError               **error);
gboolean g_spawn_command_line_async_utf8 (const gchar           *command_line,
                                          GError               **error);


#define g_get_user_name        g_get_user_name_utf8
#define g_get_real_name        g_get_real_name_utf8
#define g_get_home_dir         g_get_home_dir_utf8
#define g_get_tmp_dir          g_get_tmp_dir_utf8
#define g_find_program_in_path g_find_program_in_path_utf8

const gchar *g_get_user_name_utf8        (void);
const gchar *g_get_real_name_utf8        (void);
const gchar *g_get_home_dir_utf8         (void);
const gchar *g_get_tmp_dir_utf8          (void);
gchar       *g_find_program_in_path_utf8 (const gchar *program);


#ifdef _WIN64
#define g_win32_get_package_installation_directory g_win32_get_package_installation_directory_utf8
#define g_win32_get_package_installation_subdirectory g_win32_get_package_installation_subdirectory_utf8
#endif

gchar *g_win32_get_package_installation_directory_utf8    (const gchar *package,
                                                           const gchar *dll_name);
gchar *g_win32_get_package_installation_subdirectory_utf8 (const gchar *package,
                                                           const gchar *dll_name,
                                                           const gchar *subdir);

#endif

G_END_DECLS

#endif /* __G_WIN32COMPAT_H__ */
