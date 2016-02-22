/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright (C) 2016 NICE s.r.l.
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

#ifndef __G_WIN32_NAMED_PIPE_CLIENT_H__
#define __G_WIN32_NAMED_PIPE_CLIENT_H__

#if !defined (__GIO_GIO_H_INSIDE__) && !defined (GIO_COMPILATION)
#error "Only <gio/gio.h> can be included directly."
#endif

#include <gio/giotypes.h>

G_BEGIN_DECLS

#define G_TYPE_WIN32_NAMED_PIPE_CLIENT            (g_win32_named_pipe_client_get_type ())
#define G_WIN32_NAMED_PIPE_CLIENT(o)              (G_TYPE_CHECK_INSTANCE_CAST ((o), G_TYPE_WIN32_NAMED_PIPE_CLIENT, GWin32NamedPipeClient))
#define G_WIN32_NAMED_PIPE_CLIENT_CLASS(k)        (G_TYPE_CHECK_CLASS_CAST ((k), G_TYPE_WIN32_NAMED_PIPE_CLIENT, GWin32NamedPipeClientClass))
#define G_IS_WIN32_NAMED_PIPE_CLIENT(o)           (G_TYPE_CHECK_INSTANCE_TYPE ((o), G_TYPE_WIN32_NAMED_PIPE_CLIENT))
#define G_IS_WIN32_NAMED_PIPE_CLIENT_CLASS(k)     (G_TYPE_CHECK_CLASS_TYPE ((k),  G_TYPE_WIN32_NAMED_PIPE_CLIENT))
#define G_WIN32_NAMED_PIPE_CLIENT_GET_CLASS(o)    (G_TYPE_INSTANCE_GET_CLASS ((o), G_TYPE_WIN32_NAMED_PIPE_CLIENT, GWin32NamedPipeClientClass))

typedef struct _GWin32NamedPipeClient                       GWin32NamedPipeClient;
typedef struct _GWin32NamedPipeClientClass                  GWin32NamedPipeClientClass;

struct _GWin32NamedPipeClient
{
  /*< private >*/
  GObject parent_instance;
};

struct _GWin32NamedPipeClientClass
{
  GObjectClass parent_class;

  /*< private >*/
  gpointer padding[10];
};

GLIB_AVAILABLE_IN_2_48
GType                       g_win32_named_pipe_client_get_type         (void) G_GNUC_CONST;

GLIB_AVAILABLE_IN_2_48
GWin32NamedPipeClient      *g_win32_named_pipe_client_new              (void);

GLIB_AVAILABLE_IN_2_48
GWin32NamedPipeConnection  *g_win32_named_pipe_client_connect          (GWin32NamedPipeClient  *client,
                                                                        const gchar            *pipe_name,
                                                                        GCancellable           *cancellable,
                                                                        GError                **error);

GLIB_AVAILABLE_IN_2_48
void                        g_win32_named_pipe_client_connect_async    (GWin32NamedPipeClient  *client,
                                                                        const gchar            *pipe_name,
                                                                        GCancellable           *cancellable,
                                                                        GAsyncReadyCallback     callback,
                                                                        gpointer                user_data);

GLIB_AVAILABLE_IN_2_48
GWin32NamedPipeConnection  *g_win32_named_pipe_client_connect_finish   (GWin32NamedPipeClient  *client,
                                                                        GAsyncResult           *result,
                                                                        GError                **error);

G_END_DECLS

#endif /* __G_WIN32_NAMED_PIPE_CLIENT_H__ */
