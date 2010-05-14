/* GDBus - GLib D-Bus Library
 *
 * Copyright (C) 2008-2010 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: David Zeuthen <davidz@redhat.com>
 */

#include "config.h"

#include <stdlib.h>
#include <gobject/gvaluecollector.h>

#include "gcredentials.h"
#include "gioerror.h"

#ifdef __linux__
#define __USE_GNU
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#endif

#include "glibintl.h"
#include "gioalias.h"

/**
 * SECTION:gcredentials
 * @short_description: An object containing credentials
 * @include: gio/gio.h
 *
 * The #GCredentials type is a reference-counted wrapper for the
 * native credentials type. This information is typically used for
 * identifying, authenticating and authorizing other processes.
 *
 * Some operating systems supports looking up the credentials of the
 * remote peer of a communication endpoint - see e.g.
 * g_socket_get_credentials().
 *
 * Some operating systems supports securely sending and receiving
 * credentials over a Unix Domain Socket, see
 * #GUnixCredentialsMessage, g_unix_connection_send_credentials() and
 * g_unix_connection_receive_credentials() for details.
 *
 * On Linux, the native credential type is a <literal>struct ucred</literal> - see
 * the <literal>unix(7)</literal> man page for details.
 */

struct _GCredentialsPrivate
{
#ifdef __linux__
  struct ucred native;
#else
#warning Please add GCredentials support for your OS
  guint foo;
#endif
};

G_DEFINE_TYPE (GCredentials, g_credentials, G_TYPE_OBJECT);

static void
g_credentials_finalize (GObject *object)
{
  G_GNUC_UNUSED GCredentials *credentials = G_CREDENTIALS (object);

  if (G_OBJECT_CLASS (g_credentials_parent_class)->finalize != NULL)
    G_OBJECT_CLASS (g_credentials_parent_class)->finalize (object);
}


static void
g_credentials_class_init (GCredentialsClass *klass)
{
  GObjectClass *gobject_class;

  g_type_class_add_private (klass, sizeof (GCredentialsPrivate));

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = g_credentials_finalize;
}

static void
g_credentials_init (GCredentials *credentials)
{
  credentials->priv = G_TYPE_INSTANCE_GET_PRIVATE (credentials, G_TYPE_CREDENTIALS, GCredentialsPrivate);
#ifdef __linux__
  credentials->priv->native.pid = getpid ();
  credentials->priv->native.uid = getuid ();
  credentials->priv->native.gid = getgid ();
#endif
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_credentials_new:
 *
 * Creates a new #GCredentials object with credentials matching the
 * the current process.
 *
 * Returns: A #GCredentials. Free with g_object_unref().
 *
 * Since: 2.26
 */
GCredentials *
g_credentials_new (void)
{
  return g_object_new (G_TYPE_CREDENTIALS, NULL);
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_credentials_to_string:
 * @credentials: A #GCredentials object.
 *
 * Creates a human-readable textual representation of @credentials
 * that can be used in logging and debug messages. The format of the
 * returned string may change in future GLib release.
 *
 * Returns: A string that should be freed with g_free().
 *
 * Since: 2.26
 */
gchar *
g_credentials_to_string (GCredentials *credentials)
{
  GString *ret;

  g_return_val_if_fail (G_IS_CREDENTIALS (credentials), NULL);

  ret = g_string_new ("GCredentials:");
#ifdef __linux__
  g_string_append (ret, "linux:");
  if (credentials->priv->native.pid != -1)
    g_string_append_printf (ret, "pid=%" G_GINT64_FORMAT ",", (gint64) credentials->priv->native.pid);
  if (credentials->priv->native.uid != -1)
    g_string_append_printf (ret, "uid=%" G_GINT64_FORMAT ",", (gint64) credentials->priv->native.uid);
  if (credentials->priv->native.gid != -1)
    g_string_append_printf (ret, "gid=%" G_GINT64_FORMAT ",", (gint64) credentials->priv->native.gid);
  if (ret->str[ret->len - 1] == ',')
    ret->str[ret->len - 1] = '\0';
#else
  g_string_append (ret, "unknown");
#endif

  return g_string_free (ret, FALSE);
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_credentials_is_same_user:
 * @credentials: A #GCredentials.
 * @other_credentials: A #GCredentials.
 * @error: Return location for error or %NULL.
 *
 * Checks if @credentials and @other_credentials is the same user.
 *
 * This operation can fail if #GCredentials is not supported on the
 * the OS.
 *
 * Returns: %TRUE if @credentials and @other_credentials has the same
 * user, %FALSE otherwise or if @error is set.
 *
 * Since: 2.26
 */
gboolean
g_credentials_is_same_user (GCredentials  *credentials,
                            GCredentials  *other_credentials,
                            GError       **error)
{
  gboolean ret;

  g_return_val_if_fail (G_IS_CREDENTIALS (credentials), FALSE);
  g_return_val_if_fail (G_IS_CREDENTIALS (other_credentials), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  ret = FALSE;
#ifdef __linux__
  if (credentials->priv->native.uid == other_credentials->priv->native.uid)
    ret = TRUE;
#else
  g_set_error_literal (error,
                       G_IO_ERROR,
                       G_IO_ERROR_NOT_SUPPORTED,
                       _("GCredentials is not implemented on this OS"));
#endif

  return ret;
}

/**
 * g_credentials_get_native:
 * @credentials: A #GCredentials.
 *
 * Gets a pointer to the native credentials structure.
 *
 * Returns: The pointer or %NULL if there is no #GCredentials support
 * for the OS. Do not free the returned data, it is owned by
 * @credentials.
 *
 * Since: 2.26
 */
gpointer
g_credentials_get_native (GCredentials *credentials)
{
  gpointer ret;
  g_return_val_if_fail (G_IS_CREDENTIALS (credentials), NULL);

#ifdef __linux__
  ret = &credentials->priv->native;
#else
  ret = NULL;
#endif

  return ret;
}

/**
 * g_credentials_set_native:
 * @credentials: A #GCredentials.
 * @native: A pointer to native credentials.
 *
 * Copies the native credentials from @native into @credentials.
 *
 * It is a programming error (which will cause an warning to be
 * logged) to use this method if there is no #GCredentials support for
 * the OS.
 *
 * Since: 2.26
 */
void
g_credentials_set_native (GCredentials    *credentials,
                          gpointer         native)
{
#ifdef __linux__
  memcpy (&credentials->priv->native, native, sizeof (struct ucred));
#else
  g_warning ("g_credentials_set_native: Trying to set credentials but GLib has no support "
             "for the native credentials type. Please add support.");
#endif
}

/* ---------------------------------------------------------------------------------------------------- */

#ifdef G_OS_UNIX
/**
 * g_credentials_get_unix_user:
 * @credentials: A #GCredentials
 * @error: Return location for error or %NULL.
 *
 * Tries to get the UNIX user identifier from @credentials. This
 * method is only available on UNIX platforms.
 *
 * This operation can fail if #GCredentials is not supported on the
 * OS or if the native credentials type does not contain information
 * about the UNIX user.
 *
 * Returns: The UNIX user identifier or -1 if @error is set.
 *
 * Since: 2.26
 */
uid_t
g_credentials_get_unix_user (GCredentials    *credentials,
                             GError         **error)
{
  uid_t ret;

  g_return_val_if_fail (G_IS_CREDENTIALS (credentials), -1);
  g_return_val_if_fail (error == NULL || *error == NULL, -1);

#ifdef __linux__
  ret = credentials->priv->native.uid;
#else
  ret = -1;
  g_set_error_literal (error,
                       G_IO_ERROR,
                       G_IO_ERROR_NOT_SUPPORTED,
                       _("There no GCredentials support for your your platform"));
#endif

  return ret;
}

/**
 * g_credentials_set_unix_user:
 * @credentials: A #GCredentials.
 * @uid: The UNIX user identifier to set.
 * @error: Return location for error or %NULL.
 *
 * Tries to set the UNIX user identifier on @credentials. This method
 * is only available on UNIX platforms.
 *
 * This operation can fail if #GCredentials is not supported on the
 * OS or if the native credentials type does not contain information
 * about the UNIX user.
 *
 * Returns: %TRUE if @uid was set, %FALSE if error is set.
 *
 * Since: 2.26
 */
gboolean
g_credentials_set_unix_user (GCredentials    *credentials,
                             uid_t            uid,
                             GError         **error)
{
  gboolean ret;

  g_return_val_if_fail (G_IS_CREDENTIALS (credentials), FALSE);
  g_return_val_if_fail (uid != -1, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  ret = FALSE;
#ifdef __linux__
  credentials->priv->native.uid = uid;
  ret = TRUE;
#else
  g_set_error_literal (error,
                       G_IO_ERROR,
                       G_IO_ERROR_NOT_SUPPORTED,
                       _("GCredentials is not implemented on this OS"));
#endif

  return ret;
}
#endif /* G_OS_UNIX */

#define __G_CREDENTIALS_C__
#include "gioaliasdef.c"
