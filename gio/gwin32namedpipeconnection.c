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


#include "config.h"

#include "gwin32namedpipeconnection.h"
#include "gwin32inputstream.h"
#include "gwin32outputstream.h"

#include "glibintl.h"

#include <windows.h>

/**
 * SECTION:gwin32namedpipeconnection
 * @short_description: A wrapper around a Windows pipe handle.
 * @include: gio/gio.h
 * @see_also: #GIOStream
 *
 * GWin32NamedPipeConnection creates a #GIOStream from an arbitrary handle.
 *
 * Since: 2.48
 */

/**
 * GWin32NamedPipeConnection:
 *
 * A wrapper around a Windows pipe handle.
 *
 * Since: 2.48
 */
struct _GWin32NamedPipeConnection
{
  GIOStream parent;

  void *handle;
  gboolean close_handle;

  GInputStream *input_stream;
  GOutputStream *output_stream;
};

struct _GWin32NamedPipeConnectionClass
{
  GIOStreamClass parent;
};

typedef struct _GWin32NamedPipeConnectionClass GWin32NamedPipeConnectionClass;

enum
{
  PROP_0,
  PROP_HANDLE,
  PROP_CLOSE_HANDLE
};

G_DEFINE_TYPE (GWin32NamedPipeConnection, g_win32_named_pipe_connection, G_TYPE_IO_STREAM)

static void
g_win32_named_pipe_connection_finalize (GObject *object)
{
  GWin32NamedPipeConnection *connection = G_WIN32_NAMED_PIPE_CONNECTION (object);

  if (connection->input_stream)
    g_object_unref (connection->input_stream);

  if (connection->output_stream)
    g_object_unref (connection->output_stream);

  if (connection->close_handle && connection->handle != INVALID_HANDLE_VALUE)
    CloseHandle (connection->handle);

  G_OBJECT_CLASS (g_win32_named_pipe_connection_parent_class)->finalize (object);
}

static void
g_win32_named_pipe_connection_set_property (GObject      *object,
                                            guint         prop_id,
                                            const GValue *value,
                                            GParamSpec   *pspec)
{
  GWin32NamedPipeConnection *connection = G_WIN32_NAMED_PIPE_CONNECTION (object);

  switch (prop_id)
    {
    case PROP_HANDLE:
      connection->handle = g_value_get_pointer (value);
      if (connection->handle != NULL && connection->handle != INVALID_HANDLE_VALUE)
        {
          connection->input_stream = g_win32_input_stream_new (connection->handle, FALSE);
          connection->output_stream = g_win32_output_stream_new (connection->handle, FALSE);
        }
      break;

    case PROP_CLOSE_HANDLE:
      connection->close_handle = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
g_win32_named_pipe_connection_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  GWin32NamedPipeConnection *connection = G_WIN32_NAMED_PIPE_CONNECTION (object);

  switch (prop_id)
    {
    case PROP_HANDLE:
      g_value_set_pointer (value, connection->handle);
      break;

    case PROP_CLOSE_HANDLE:
      g_value_set_boolean (value, connection->close_handle);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static GInputStream *
g_win32_named_pipe_connection_get_input_stream (GIOStream *stream)
{
  GWin32NamedPipeConnection *connection = G_WIN32_NAMED_PIPE_CONNECTION (stream);

  return connection->input_stream;
}

static GOutputStream *
g_win32_named_pipe_connection_get_output_stream (GIOStream *stream)
{
  GWin32NamedPipeConnection *connection = G_WIN32_NAMED_PIPE_CONNECTION (stream);

  return connection->output_stream;
}

static void
g_win32_named_pipe_connection_class_init (GWin32NamedPipeConnectionClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);
  GIOStreamClass *io_class = G_IO_STREAM_CLASS (class);

  gobject_class->finalize = g_win32_named_pipe_connection_finalize;
  gobject_class->get_property = g_win32_named_pipe_connection_get_property;
  gobject_class->set_property = g_win32_named_pipe_connection_set_property;

  io_class->get_input_stream = g_win32_named_pipe_connection_get_input_stream;
  io_class->get_output_stream = g_win32_named_pipe_connection_get_output_stream;

  /**
   * GWin32NamedPipeConnection:handle:
   *
   * The handle for the connection.
   *
   * Since: 2.48
   */
  g_object_class_install_property (gobject_class,
                                   PROP_HANDLE,
                                   g_param_spec_pointer ("handle",
                                                         P_("File handle"),
                                                         P_("The file handle to read from"),
                                                         G_PARAM_READABLE |
                                                         G_PARAM_WRITABLE |
                                                         G_PARAM_CONSTRUCT_ONLY |
                                                         G_PARAM_STATIC_STRINGS));

  /**
   * GWin32NamedPipeConnection:close-handle:
   *
   * Whether to close the file handle when the pipe connection is disposed.
   *
   * Since: 2.48
   */
  g_object_class_install_property (gobject_class,
                                   PROP_CLOSE_HANDLE,
                                   g_param_spec_boolean ("close-handle",
                                                         P_("Close file handle"),
                                                         P_("Whether to close the file handle when the stream is closed"),
                                                         TRUE,
                                                         G_PARAM_READABLE |
                                                         G_PARAM_WRITABLE |
                                                         G_PARAM_STATIC_STRINGS));
}

static void
g_win32_named_pipe_connection_init (GWin32NamedPipeConnection *stream)
{
}
