/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright 2018 Red Hat, Inc.
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

#ifndef __G_TRASH_PORTAL_H__

#include <glib.h>
#include <gio/gio.h>

G_BEGIN_DECLS

gboolean g_trash_portal_trash_file (GFile   *file,
                                    GError **error);

G_END_DECLS

#endif
