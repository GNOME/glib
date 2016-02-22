/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright (C) 2011 Red Hat, Inc.
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

#ifndef __G_WIN32_NAMED_PIPE_LISTENER_H__
#define __G_WIN32_NAMED_PIPE_LISTENER_H__

#if !defined (__GIO_GIO_H_INSIDE__) && !defined (GIO_COMPILATION)
#error "Only <gio/gio.h> can be included directly."
#endif

#include <gio/giotypes.h>

G_BEGIN_DECLS

#define G_TYPE_WIN32_NAMED_PIPE_LISTENER            (g_win32_named_pipe_listener_get_type ())
#define G_WIN32_NAMED_PIPE_LISTENER(o)              (G_TYPE_CHECK_INSTANCE_CAST ((o), G_TYPE_WIN32_NAMED_PIPE_LISTENER, GWin32NamedPipeListener))
#define G_WIN32_NAMED_PIPE_LISTENER_CLASS(k)        (G_TYPE_CHECK_CLASS_CAST ((k), G_TYPE_WIN32_NAMED_PIPE_LISTENER, GWin32NamedPipeListenerClass))
#define G_IS_WIN32_NAMED_PIPE_LISTENER(o)           (G_TYPE_CHECK_INSTANCE_TYPE ((o), G_TYPE_WIN32_NAMED_PIPE_LISTENER))
#define G_IS_WIN32_NAMED_PIPE_LISTENER_CLASS(k)     (G_TYPE_CHECK_CLASS_TYPE ((k),  G_TYPE_WIN32_NAMED_PIPE_LISTENER))
#define G_WIN32_NAMED_PIPE_LISTENER_GET_CLASS(o)    (G_TYPE_INSTANCE_GET_CLASS ((o), G_TYPE_WIN32_NAMED_PIPE_LISTENER, GWin32NamedPipeListenerClass))

typedef struct _GWin32NamedPipeListener                       GWin32NamedPipeListener;
typedef struct _GWin32NamedPipeListenerClass                  GWin32NamedPipeListenerClass;

struct _GWin32NamedPipeListener
{
  /*< private >*/
  GObject parent_instance;
};

struct _GWin32NamedPipeListenerClass
{
  GObjectClass parent_class;

  /*< private >*/
  gpointer padding[10];
};

GLIB_AVAILABLE_IN_2_48
GType                       g_win32_named_pipe_listener_get_type       (void) G_GNUC_CONST;

GLIB_AVAILABLE_IN_2_48
GWin32NamedPipeListener    *g_win32_named_pipe_listener_new            (void);

GLIB_AVAILABLE_IN_2_48
gboolean                    g_win32_named_pipe_listener_add_named_pipe (GWin32NamedPipeListener  *listener,
                                                                        const gchar              *pipe_name,
                                                                        GObject                  *source_object,
                                                                        GError                  **error);

GLIB_AVAILABLE_IN_2_48
GWin32NamedPipeConnection  *g_win32_named_pipe_listener_accept         (GWin32NamedPipeListener  *listener,
                                                                        GObject                 **source_object,
                                                                        GCancellable             *cancellable,
                                                                        GError                  **error);

GLIB_AVAILABLE_IN_2_48
void                        g_win32_named_pipe_listener_accept_async   (GWin32NamedPipeListener  *listener,
                                                                        GCancellable             *cancellable,
                                                                        GAsyncReadyCallback       callback,
                                                                        gpointer                  user_data);

GLIB_AVAILABLE_IN_2_48
GWin32NamedPipeConnection  *g_win32_named_pipe_listener_accept_finish  (GWin32NamedPipeListener  *listener,
                                                                        GAsyncResult             *result,
                                                                        GObject                 **source_object,
                                                                        GError                  **error);

G_END_DECLS

#endif /* __G_WIN32_NAMED_PIPE_LISTENER_H__ */
