/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright (C) 2026 Dimitriy Ryazantcev
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
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __G_WIN32_SECURITY_H__
#define __G_WIN32_SECURITY_H__

#include <glib.h>

G_BEGIN_DECLS

gboolean   g_win32_read_private_file                     (const gchar  *file,
                                                          gchar       **contents,
                                                          GError      **error);

gboolean   g_win32_ensure_private_directory              (const gchar  *dir,
                                                          GError      **error);

gboolean   g_win32_set_contents_private                  (const gchar  *file,
                                                          const gchar  *contents,
                                                          gsize         length,
                                                          GError      **error);

G_END_DECLS

#endif /* __G_WIN32_SECURITY_H__ */
