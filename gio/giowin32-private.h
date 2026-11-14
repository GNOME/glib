/*
 * Copyright 2026 Philip Withnall
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
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
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "gio.h"

G_BEGIN_DECLS

gsize g_utf16_len (const gunichar2 *str);
gunichar2 *g_wcsdup (const gunichar2 *str, gssize str_len);
gboolean g_utf16_to_utf8_and_fold (const gunichar2  *str,
                                   gssize            length,
                                   gchar           **str_u8,
                                   gchar           **str_u8_folded);

const gunichar2 *g_utf16_find_basename (const gunichar2 *filename,
                                        gssize           len);
const gchar *g_utf8_find_basename (const gchar *filename,
                                   gssize       len);

void _g_win32_parse_filename (const gunichar2  *commandline,
                              gboolean          comma_separator,
                              const gunichar2 **executable_start,
                              gssize           *executable_len,
                              const gunichar2 **executable_basename,
                              const gunichar2 **after_executable);

void _g_win32_extract_executable (const gunichar2  *commandline,
                                  gchar           **ex_out,
                                  gchar           **ex_basename_out,
                                  gchar           **ex_folded_out,
                                  gchar           **ex_folded_basename_out,
                                  gchar           **dll_function_out);

void _g_win32_fixup_broken_microsoft_rundll_commandline (gunichar2 *commandline);

gboolean _g_win32_expand_application_parameters (const char    *app_name,
                                                 const gchar   *exec_line,
                                                 GList        **objs,
                                                 int           *argc,
                                                 char        ***argv,
                                                 GError       **error);

typedef struct {
  char *uri;
  char *file;
} file_or_uri;

void free_file_or_uri (gpointer ptr);

G_END_DECLS
