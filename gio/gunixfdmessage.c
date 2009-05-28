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

/**
 * SECTION: gunixfdmessage
 * @title: GUnixFDMessage
 * @short_description: A GSocketControlMessage containing a list of
 * file descriptors
 * @see_also: #GUnixConnection
 *
 * This #GSocketControlMessage contains a list of file descriptors.
 * It may be sent using g_socket_send_message() and received using
 * g_socket_receive_message() over UNIX sockets (ie: sockets in the
 * %G_SOCKET_ADDRESS_UNIX family).
 *
 * For an easier way to send and receive file descriptors over
 * stream-oriented UNIX sockets, see g_unix_connection_send_fd() and
 * g_unix_connection_receive_fd().
 */

#include "config.h"

#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "gunixfdmessage.h"
#include "gioerror.h"

#include "gioalias.h"


G_DEFINE_TYPE (GUnixFDMessage, g_unix_fd_message,
               G_TYPE_SOCKET_CONTROL_MESSAGE);

struct _GUnixFDMessagePrivate
{
  gint *fds;
  gint nfd;
};

static gsize
g_unix_fd_message_get_size (GSocketControlMessage *message)
{
  GUnixFDMessage *fd_message = G_UNIX_FD_MESSAGE (message);

  return fd_message->priv->nfd * sizeof (gint);
}

static int
g_unix_fd_message_get_level (GSocketControlMessage *message)
{
  return SOL_SOCKET;
}

static int
g_unix_fd_message_get_msg_type (GSocketControlMessage *message)
{
  return SCM_RIGHTS;
}

static GSocketControlMessage *
g_unix_fd_message_deserialize (int      level,
			       int      type,
			       gsize    size,
			       gpointer data)
{
  GUnixFDMessage *message;

  if (level != SOL_SOCKET ||
      level != SCM_RIGHTS)
    return NULL;
  
  if (size % 4 > 0)
    {
      g_warning ("Kernel returned non-integral number of fds");
      return NULL;
    }

  message = g_object_new (G_TYPE_UNIX_FD_MESSAGE, NULL);
  message->priv->nfd = size / sizeof (gint);
  message->priv->fds = g_new (gint, message->priv->nfd + 1);
  memcpy (message->priv->fds, data, size);
  message->priv->fds[message->priv->nfd] = -1;

  return G_SOCKET_CONTROL_MESSAGE (message);
}

static void
g_unix_fd_message_serialize (GSocketControlMessage *message,
			     gpointer               data)
{
  GUnixFDMessage *fd_message = G_UNIX_FD_MESSAGE (message);
  memcpy (data, fd_message->priv->fds,
	  sizeof (gint) * fd_message->priv->nfd);
}
static void
g_unix_fd_message_init (GUnixFDMessage *message)
{
  message->priv = G_TYPE_INSTANCE_GET_PRIVATE (message,
                                               G_TYPE_UNIX_FD_MESSAGE,
                                               GUnixFDMessagePrivate);
}

static void
g_unix_fd_message_finalize (GObject *object)
{
  GUnixFDMessage *message = G_UNIX_FD_MESSAGE (object);
  gint i;

  for (i = 0; i < message->priv->nfd; i++)
    close (message->priv->fds[i]);
  g_free (message->priv->fds);

  G_OBJECT_CLASS (g_unix_fd_message_parent_class)
    ->finalize (object);
}

static void
g_unix_fd_message_class_init (GUnixFDMessageClass *class)
{
  GSocketControlMessageClass *scm_class = G_SOCKET_CONTROL_MESSAGE_CLASS (class);
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  g_type_class_add_private (class, sizeof (GUnixFDMessagePrivate));
  scm_class->get_size = g_unix_fd_message_get_size;
  scm_class->get_level = g_unix_fd_message_get_level;
  scm_class->get_type = g_unix_fd_message_get_msg_type;
  scm_class->serialize = g_unix_fd_message_serialize;
  scm_class->deserialize = g_unix_fd_message_deserialize;
  object_class->finalize = g_unix_fd_message_finalize;
}

/**
 * g_unix_fd_message_new:
 *
 * Creates a new #GUnixFDMessage containing no file descriptors.
 *
 * Returns: a new #GUnixFDMessage
 *
 * Since: 2.22
 */
GSocketControlMessage *
g_unix_fd_message_new (void)
{
  return g_object_new (G_TYPE_UNIX_FD_MESSAGE, NULL);
}

/**
 * g_unix_fd_message_steal_fds:
 * @message: a #GUnixFDMessage
 * @length: pointer to the length of the returned array, or %NULL
 *
 * Returns the array of file descriptors that is contained in this
 * object.
 *
 * After this call, the descriptors are no longer contained in
 * @message. Further calls will return an empty list (unless more
 * descriptors have been added).
 *
 * The return result of this function must be freed with g_free().
 * The caller is also responsible for closing all of the file
 * descriptors.
 *
 * If @length is non-%NULL then it is set to the number of file
 * descriptors in the returned array. The returned array is also
 * terminated with -1.
 *
 * This function never returns %NULL. In case there are no file
 * descriptors contained in @message, an empty array is returned.
 *
 * Returns: an array of file descriptors
 *
 * Since: 2.22
 */
gint *
g_unix_fd_message_steal_fds (GUnixFDMessage *message,
                             gint           *length)
{
  gint *result;

  g_return_val_if_fail (G_IS_UNIX_FD_MESSAGE (message), NULL);

  /* will be true for fresh object or if we were just called */
  if (message->priv->fds == NULL)
    {
      message->priv->fds = g_new (gint, 1);
      message->priv->fds[0] = -1;
      message->priv->nfd = 0;
    }

  if (length)
    *length = message->priv->nfd;
  result = message->priv->fds;

  message->priv->fds = NULL;
  message->priv->nfd = 0;

  return result;
}

/**
 * g_unix_fd_message_append_fd:
 * @message: a #GUnixFDMessage
 * @fd: a valid open file descriptor
 * @error: a #GError pointer
 *
 * Adds a file descriptor to @message.
 *
 * The file descriptor is duplicated using dup(). You keep your copy
 * of the descriptor and the copy contained in @message will be closed
 * when @message is finalized.
 *
 * A possible cause of failure is exceeding the per-process or
 * system-wide file descriptor limit.
 *
 * Returns: %TRUE in case of success, else %FALSE (and @error is set)
 *
 * Since: 2.22
 */
gboolean
g_unix_fd_message_append_fd (GUnixFDMessage  *message,
                             gint             fd,
                             GError         **error)
{
  gint new_fd;

  g_return_val_if_fail (G_IS_UNIX_FD_MESSAGE (message), FALSE);
  g_return_val_if_fail (fd >= 0, FALSE);

  do
    new_fd = dup (fd);
  while (new_fd < 0 && (errno == EINTR));

  if (fd < 0)
    {
      int saved_errno = errno;

      g_set_error (error, G_IO_ERROR,
                   g_io_error_from_errno (saved_errno),
                   "dup: %s", g_strerror (saved_errno));

      return FALSE;
    }

  message->priv->fds = g_realloc (message->priv->fds,
                                  sizeof (gint) *
                                   (message->priv->nfd + 2));
  message->priv->fds[message->priv->nfd++] = new_fd;
  message->priv->fds[message->priv->nfd] = -1;

  return TRUE;
}

#define __G_UNIX_FD_MESSAGE_C__
#include "gioaliasdef.c"
