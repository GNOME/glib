/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright © 2009 Codethink Limited
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2 of the licence or (at
 * your option) any later version.
 *
 * See the included COPYING file for more information.
 *
 * Authors: Ryan Lortie <desrt@desrt.ca>
 */

#include "config.h"
#include "gunixconnection.h"
#include "glibintl.h"

/**
 * SECTION: gunixconnection
 * @title: GUnixConnection
 * @short_description: a Unix domain #GSocketConnection
 * @see_also: #GSocketConnection.
 *
 * This is the subclass of #GSocketConnection that is created
 * for UNIX domain sockets.
 *
 * It contains functions to do some of the unix socket specific
 * functionallity like passing file descriptors.
 *
 * Since: 2.22
 */

#include <gio/gsocketcontrolmessage.h>
#include <gio/gunixfdmessage.h>
#include <gio/gsocket.h>
#include <unistd.h>

#include "gioalias.h"

G_DEFINE_TYPE_WITH_CODE (GUnixConnection, g_unix_connection,
			 G_TYPE_SOCKET_CONNECTION,
  g_socket_connection_factory_register_type (g_define_type_id,
					     G_SOCKET_FAMILY_UNIX,
					     G_SOCKET_TYPE_STREAM,
					     G_SOCKET_PROTOCOL_DEFAULT);
			 );

/**
 * g_unix_connection_send_fd:
 * @connection: a #GUnixConnection
 * @fd: a file descriptor
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * @error: #GError for error reporting, or %NULL to ignore.
 *
 * Passes a file descriptor to the recieving side of the
 * connection. The recieving end has to call g_unix_connection_receive_fd()
 * to accept the file descriptor.
 *
 * As well as sending the fd this also writes a single byte to the
 * stream, as this is required for fd passing to work on some
 * implementations.
 *
 * Returns: a %TRUE on success, %NULL on error.
 *
 * Since: 2.22
 */
gboolean
g_unix_connection_send_fd (GUnixConnection  *connection,
                           gint              fd,
                           GCancellable     *cancellable,
                           GError          **error)
{
  GSocketControlMessage *scm;
  GSocket *socket;

  g_return_val_if_fail (G_IS_UNIX_CONNECTION (connection), FALSE);
  g_return_val_if_fail (fd >= 0, FALSE);

  scm = g_unix_fd_message_new ();

  if (!g_unix_fd_message_append_fd (G_UNIX_FD_MESSAGE (scm), fd, error))
    {
      g_object_unref (scm);
      return FALSE;
    }

  g_object_get (connection, "socket", &socket, NULL);
  if (g_socket_send_message (socket, NULL, NULL, 0, &scm, 1, 0, cancellable, error) != 1)
    /* XXX could it 'fail' with zero? */
    {
      g_object_unref (socket);
      g_object_unref (scm);

      return FALSE;
    }

  g_object_unref (socket);
  g_object_unref (scm);

  return TRUE;
}

/**
 * g_unix_connection_receive_fd:
 * @connection: a #GUnixConnection
 * @cancellable: optional #GCancellable object, %NULL to ignore
 * @error: #GError for error reporting, or %NULL to ignore
 *
 * Receives a file descriptor from the sending end of the connection.
 * The sending end has to call g_unix_connection_send_fd() for this
 * to work.
 *
 * As well as reading the fd this also reads a single byte from the
 * stream, as this is required for fd passing to work on some
 * implementations.
 *
 * Returns: a file descriptor on success, -1 on error.
 *
 * Since: 2.22
 */
gint
g_unix_connection_receive_fd (GUnixConnection  *connection,
                              GCancellable     *cancellable,
                              GError          **error)
{
  GSocketControlMessage **scms;
  gint *fds, nfd, fd, nscm;
  GUnixFDMessage *fdmsg;
  GSocket *socket;

  g_return_val_if_fail (G_IS_UNIX_CONNECTION (connection), -1);

  g_object_get (connection, "socket", &socket, NULL);
  if (g_socket_receive_message (socket, NULL, NULL, 0,
                                &scms, &nscm, NULL, cancellable, error) != 1)
    /* XXX it _could_ 'fail' with zero. */
    {
      g_object_unref (socket);

      return -1;
    }

  g_object_unref (socket);

  if (nscm != 1)
    {
      gint i;

      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
		   _("Expecting 1 control message, got %d"), nscm);

      for (i = 0; i < nscm; i++)
        g_object_unref (scms[i]);

      g_free (scms);

      return -1;
    }

  if (!G_IS_UNIX_FD_MESSAGE (scms[0]))
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
			   _("Unexpected type of ancillary data"));
      g_object_unref (scms[0]);
      g_free (scms);

      return -1;
    }

  fdmsg = G_UNIX_FD_MESSAGE (scms[0]);
  g_free (scms);

  fds = g_unix_fd_message_steal_fds (fdmsg, &nfd);
  g_object_unref (fdmsg);

  if (nfd != 1)
    {
      gint i;

      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
		   _("Expecting one fd, but got %d\n"), nfd);

      for (i = 0; i < nfd; i++)
        close (fds[i]);

      g_free (fds);

      return -1;
    }

  fd = *fds;
  g_free (fds);

  if (fd < 0)
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                           _("Received invalid fd"));
      fd = -1;
    }

  return fd;
}

static void
g_unix_connection_init (GUnixConnection *connection)
{
}

static void
g_unix_connection_class_init (GUnixConnectionClass *class)
{
}

/* TODO: Other stuff we might want to add are:
void                    g_unix_connection_send_fd_async                 (GUnixConnection      *connection,
                                                                         gint                  fd,
                                                                         gboolean              close,
                                                                         gint                  io_priority,
                                                                         GAsyncReadyCallback   callback,
                                                                         gpointer              user_data);
gboolean                g_unix_connection_send_fd_finish                (GUnixConnection      *connection,
                                                                         GError              **error);

gboolean                g_unix_connection_send_fds                      (GUnixConnection      *connection,
                                                                         gint                 *fds,
                                                                         gint                  nfds,
                                                                         GError              **error);
void                    g_unix_connection_send_fds_async                (GUnixConnection      *connection,
                                                                         gint                 *fds,
                                                                         gint                  nfds,
                                                                         gint                  io_priority,
                                                                         GAsyncReadyCallback   callback,
                                                                         gpointer              user_data);
gboolean                g_unix_connection_send_fds_finish               (GUnixConnection      *connection,
                                                                         GError              **error);

void                    g_unix_connection_receive_fd_async              (GUnixConnection      *connection,
                                                                         gint                  io_priority,
                                                                         GAsyncReadyCallback   callback,
                                                                         gpointer              user_data);
gint                    g_unix_connection_receive_fd_finish             (GUnixConnection      *connection,
                                                                         GError              **error);


gboolean                g_unix_connection_send_credentials              (GUnixConnection      *connection,
                                                                         GError              **error);
void                    g_unix_connection_send_credentials_async        (GUnixConnection      *connection,
                                                                         gint                  io_priority,
                                                                         GAsyncReadyCallback   callback,
                                                                         gpointer              user_data);
gboolean                g_unix_connection_send_credentials_finish       (GUnixConnection      *connection,
                                                                         GError              **error);

gboolean                g_unix_connection_send_fake_credentials         (GUnixConnection      *connection,
                                                                         guint64               pid,
                                                                         guint64               uid,
                                                                         guint64               gid,
                                                                         GError              **error);
void                    g_unix_connection_send_fake_credentials_async   (GUnixConnection      *connection,
                                                                         guint64               pid,
                                                                         guint64               uid,
                                                                         guint64               gid,
                                                                         gint                  io_priority,
                                                                         GAsyncReadyCallback   callback,
                                                                         gpointer              user_data);
gboolean                g_unix_connection_send_fake_credentials_finish  (GUnixConnection      *connection,
                                                                         GError              **error);

gboolean                g_unix_connection_receive_credentials           (GUnixConnection      *connection,
                                                                         guint64              *pid,
                                                                         guint64              *uid,
                                                                         guint64              *gid,
                                                                         GError              **error);
void                    g_unix_connection_receive_credentials_async     (GUnixConnection      *connection,
                                                                         gint                  io_priority,
                                                                         GAsyncReadyCallback   callback,
                                                                         gpointer              user_data);
gboolean                g_unix_connection_receive_credentials_finish    (GUnixConnection      *connection,
                                                                         guint64              *pid,
                                                                         guint64              *uid,
                                                                         guint64              *gid,
                                                                         GError              **error);

gboolean                g_unix_connection_create_pair                   (GUnixConnection     **one,
                                                                         GUnixConnection     **two,
                                                                         GError              **error);
*/

#define __G_UNIX_CONNECTION_C__
#include "gioaliasdef.c"
