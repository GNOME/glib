/*
 * Copyright © 2016 NICE s.r.l.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2 of the licence or (at
 * your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Ignacio Casal Quinteiro <ignacio.casal@nice-software.com>
 */

#ifndef __G_WIN32_NAMED_PIPE_CONNECTION_H__
#define __G_WIN32_NAMED_PIPE_CONNECTION_H__

#if !defined (__GIO_GIO_H_INSIDE__) && !defined (GIO_COMPILATION)
#error "Only <gio/gio.h> can be included directly."
#endif

#include <gio/giotypes.h>
#include <gio/giostream.h>

G_BEGIN_DECLS

#define G_TYPE_WIN32_NAMED_PIPE_CONNECTION                  (g_win32_named_pipe_connection_get_type ())
#define G_WIN32_NAMED_PIPE_CONNECTION(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), G_TYPE_WIN32_NAMED_PIPE_CONNECTION, GWin32NamedPipeConnection))
#define G_IS_WIN32_NAMED_PIPE_CONNECTION(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), G_TYPE_WIN32_NAMED_PIPE_CONNECTION))

GLIB_AVAILABLE_IN_2_48
GType                         g_win32_named_pipe_connection_get_type         (void) G_GNUC_CONST;

G_END_DECLS

#endif /* __G_WIN32_NAMED_PIPE_CONNECTION_H__ */
