/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright (C) 2010 Red Hat, Inc.
 * Copyright (C) 2009 Codethink Limited
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * See the included COPYING file for more information.
 *
 * Authors: David Zeuthen <davidz@redhat.com>
 */

/**
 * GUnixCredentialsMessage:
 *
 * This [class@Gio.SocketControlMessage] contains a [class@Gio.Credentials]
 * instance.  It may be sent using [method@Gio.Socket.send_message] and received
 * using [method@Gio.Socket.receive_message] over UNIX sockets (ie: sockets in
 * the `G_SOCKET_FAMILY_UNIX` family).
 *
 * For an easier way to send and receive credentials over
 * stream-oriented UNIX sockets, see
 * [method@Gio.UnixConnection.send_credentials] and
 * [method@Gio.UnixConnection.receive_credentials]. To receive credentials of
 * a foreign process connected to a socket, use
 * [method@Gio.Socket.get_credentials].
 *
 * Since GLib 2.72, `GUnixCredentialMessage` is available on all platforms. It
 * requires underlying system support (such as Windows 10 with `AF_UNIX`) at run
 * time.
 *
 * Before GLib 2.72, `<gio/gunixcredentialsmessage.h>` belonged to the UNIX-specific
 * GIO interfaces, thus you had to use the `gio-unix-2.0.pc` pkg-config file
 * when using it. This is no longer necessary since GLib 2.72.
 *
 * Since: 2.26
 */

#include "config.h"

/* ---------------------------------------------------------------------------------------------------- */

#include <fcntl.h>
#include <errno.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "gunixcredentialsmessage.h"
#include "gcredentials.h"
#include "gcredentialsprivate.h"
#include "gnetworking.h"

#include "glibintl.h"

struct _GUnixCredentialsMessagePrivate
{
  GCredentials *credentials;
};

enum
{
  PROP_0,
  PROP_CREDENTIALS
};

G_DEFINE_TYPE_WITH_PRIVATE (GUnixCredentialsMessage, g_unix_credentials_message, G_TYPE_SOCKET_CONTROL_MESSAGE)

static gsize
g_unix_credentials_message_get_size (GSocketControlMessage *message)
{
#if defined(G_CREDENTIALS_UNIX_CREDENTIALS_MESSAGE_SUPPORTED)
  return G_CREDENTIALS_NATIVE_SIZE;
#else
  return 0;
#endif
}

static int
g_unix_credentials_message_get_level (GSocketControlMessage *message)
{
#if defined(G_CREDENTIALS_UNIX_CREDENTIALS_MESSAGE_SUPPORTED)
  return SOL_SOCKET;
#else
  return 0;
#endif
}

static int
g_unix_credentials_message_get_msg_type (GSocketControlMessage *message)
{
#if defined(G_CREDENTIALS_USE_LINUX_UCRED)
  return SCM_CREDENTIALS;
#elif defined(G_CREDENTIALS_USE_FREEBSD_CMSGCRED)
  return SCM_CREDS;
#elif defined(G_CREDENTIALS_USE_NETBSD_UNPCBID)
  return SCM_CREDS;
#elif defined(G_CREDENTIALS_USE_SOLARIS_UCRED)
  return SCM_UCRED;
#elif defined(G_CREDENTIALS_UNIX_CREDENTIALS_MESSAGE_SUPPORTED)
  #error "G_CREDENTIALS_UNIX_CREDENTIALS_MESSAGE_SUPPORTED is set but there is no msg_type defined for this platform"
#else
  /* includes G_CREDENTIALS_USE_APPLE_XUCRED */
  return 0;
#endif
}

static GSocketControlMessage *
g_unix_credentials_message_deserialize (gint     level,
                                        gint     type,
                                        gsize    size,
                                        gpointer data)
{
#if defined(G_CREDENTIALS_UNIX_CREDENTIALS_MESSAGE_SUPPORTED)
  GSocketControlMessage *message;
  GCredentials *credentials;

  if (level != SOL_SOCKET || type != g_unix_credentials_message_get_msg_type (NULL))
    return NULL;

  if (size != G_CREDENTIALS_NATIVE_SIZE)
    {
      g_warning ("Expected a credentials struct of %" G_GSIZE_FORMAT " bytes but "
                 "got %" G_GSIZE_FORMAT " bytes of data",
                 G_CREDENTIALS_NATIVE_SIZE, size);
      return NULL;
    }

  credentials = g_credentials_new ();
  g_credentials_set_native (credentials, G_CREDENTIALS_NATIVE_TYPE, data);

  if (g_credentials_get_unix_user (credentials, NULL) == (uid_t) -1)
    {
      /* This happens on Linux if the remote side didn't pass the credentials */
      g_object_unref (credentials);
      return NULL;
    }

  message = g_unix_credentials_message_new_with_credentials (credentials);
  g_object_unref (credentials);

  return message;

#else /* !G_CREDENTIALS_UNIX_CREDENTIALS_MESSAGE_SUPPORTED */

  return NULL;
#endif
}

static void
g_unix_credentials_message_serialize (GSocketControlMessage *_message,
                                      gpointer               data)
{
#if defined(G_CREDENTIALS_UNIX_CREDENTIALS_MESSAGE_SUPPORTED)
  GUnixCredentialsMessage *message = G_UNIX_CREDENTIALS_MESSAGE (_message);

  memcpy (data,
          g_credentials_get_native (message->priv->credentials,
                                    G_CREDENTIALS_NATIVE_TYPE),
          G_CREDENTIALS_NATIVE_SIZE);
#endif
}

static void
g_unix_credentials_message_finalize (GObject *object)
{
  GUnixCredentialsMessage *message = G_UNIX_CREDENTIALS_MESSAGE (object);

  if (message->priv->credentials != NULL)
    g_object_unref (message->priv->credentials);

  G_OBJECT_CLASS (g_unix_credentials_message_parent_class)->finalize (object);
}

static void
g_unix_credentials_message_init (GUnixCredentialsMessage *message)
{
  message->priv = g_unix_credentials_message_get_instance_private (message);
}

static void
g_unix_credentials_message_get_property (GObject    *object,
                                         guint       prop_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
  GUnixCredentialsMessage *message = G_UNIX_CREDENTIALS_MESSAGE (object);

  switch (prop_id)
    {
    case PROP_CREDENTIALS:
      g_value_set_object (value, message->priv->credentials);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
g_unix_credentials_message_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  GUnixCredentialsMessage *message = G_UNIX_CREDENTIALS_MESSAGE (object);

  switch (prop_id)
    {
    case PROP_CREDENTIALS:
      message->priv->credentials = g_value_dup_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
g_unix_credentials_message_constructed (GObject *object)
{
  GUnixCredentialsMessage *message = G_UNIX_CREDENTIALS_MESSAGE (object);

  if (message->priv->credentials == NULL)
    message->priv->credentials = g_credentials_new ();

  if (G_OBJECT_CLASS (g_unix_credentials_message_parent_class)->constructed != NULL)
    G_OBJECT_CLASS (g_unix_credentials_message_parent_class)->constructed (object);
}

static void
g_unix_credentials_message_class_init (GUnixCredentialsMessageClass *class)
{
  GSocketControlMessageClass *scm_class;
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (class);
  gobject_class->get_property = g_unix_credentials_message_get_property;
  gobject_class->set_property = g_unix_credentials_message_set_property;
  gobject_class->finalize = g_unix_credentials_message_finalize;
  gobject_class->constructed = g_unix_credentials_message_constructed;

  scm_class = G_SOCKET_CONTROL_MESSAGE_CLASS (class);
  scm_class->get_size = g_unix_credentials_message_get_size;
  scm_class->get_level = g_unix_credentials_message_get_level;
  scm_class->get_type = g_unix_credentials_message_get_msg_type;
  scm_class->serialize = g_unix_credentials_message_serialize;
  scm_class->deserialize = g_unix_credentials_message_deserialize;

  /**
   * GUnixCredentialsMessage:credentials:
   *
   * The credentials stored in the message.
   *
   * Since: 2.26
   */
  g_object_class_install_property (gobject_class,
                                   PROP_CREDENTIALS,
                                   g_param_spec_object ("credentials", NULL, NULL,
                                                        G_TYPE_CREDENTIALS,
                                                        G_PARAM_READABLE |
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_NAME |
                                                        G_PARAM_STATIC_BLURB |
                                                        G_PARAM_STATIC_NICK));

}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_unix_credentials_message_is_supported:
 *
 * Checks if passing #GCredentials on a #GSocket is supported on this platform.
 *
 * Returns: %TRUE if supported, %FALSE otherwise
 *
 * Since: 2.26
 */
gboolean
g_unix_credentials_message_is_supported (void)
{
#if defined(G_CREDENTIALS_UNIX_CREDENTIALS_MESSAGE_SUPPORTED)
  return TRUE;
#else
  return FALSE;
#endif
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_unix_credentials_message_new:
 *
 * Creates a new #GUnixCredentialsMessage with credentials matching the current processes.
 *
 * Returns: a new #GUnixCredentialsMessage
 *
 * Since: 2.26
 */
GSocketControlMessage *
g_unix_credentials_message_new (void)
{
  g_return_val_if_fail (g_unix_credentials_message_is_supported (), NULL);
  return g_object_new (G_TYPE_UNIX_CREDENTIALS_MESSAGE,
                       NULL);
}

/**
 * g_unix_credentials_message_new_with_credentials:
 * @credentials: A #GCredentials object.
 *
 * Creates a new #GUnixCredentialsMessage holding @credentials.
 *
 * Returns: a new #GUnixCredentialsMessage
 *
 * Since: 2.26
 */
GSocketControlMessage *
g_unix_credentials_message_new_with_credentials (GCredentials *credentials)
{
  g_return_val_if_fail (G_IS_CREDENTIALS (credentials), NULL);
  g_return_val_if_fail (g_unix_credentials_message_is_supported (), NULL);
  return g_object_new (G_TYPE_UNIX_CREDENTIALS_MESSAGE,
                       "credentials", credentials,
                       NULL);
}

/**
 * g_unix_credentials_message_get_credentials:
 * @message: A #GUnixCredentialsMessage.
 *
 * Gets the credentials stored in @message.
 *
 * Returns: (transfer none): A #GCredentials instance. Do not free, it is owned by @message.
 *
 * Since: 2.26
 */
GCredentials *
g_unix_credentials_message_get_credentials (GUnixCredentialsMessage *message)
{
  g_return_val_if_fail (G_IS_UNIX_CREDENTIALS_MESSAGE (message), NULL);
  return message->priv->credentials;
}
