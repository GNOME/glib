/* GDBus - GLib D-Bus Library
 *
 * Copyright (C) 2008-2009 Red Hat, Inc.
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
#include <glib/gi18n.h>
#include <gobject/gvaluecollector.h>

#include "gcredentials.h"
#include "gioerror.h"

#ifdef G_OS_UNIX
#include <sys/types.h>
#include <unistd.h>
#endif

/**
 * SECTION:gcredentials
 * @short_description: An object containing credentials
 * @include: gio/gio.h
 *
 * The #GCredentials type is used for storing information that can be
 * used for identifying, authenticating and authorizing processes.
 *
 * Most UNIX and UNIX-like operating systems support a secure exchange
 * of credentials over a Unix Domain Socket, see
 * #GUnixCredentialsMessage, g_unix_connection_send_credentials() and
 * g_unix_connection_receive_credentials() for details.
 */

struct _GCredentialsPrivate
{
  gint64 unix_user;
  gint64 unix_group;
  gint64 unix_process;
  gchar *windows_user;
};

G_DEFINE_TYPE (GCredentials, g_credentials, G_TYPE_OBJECT);

static void
g_credentials_finalize (GObject *object)
{
  GCredentials *credentials = G_CREDENTIALS (object);

  g_free (credentials->priv->windows_user);

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

  credentials->priv->unix_user = -1;
  credentials->priv->unix_group = -1;
  credentials->priv->unix_process = -1;
  credentials->priv->windows_user = NULL;
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_credentials_new:
 *
 * Creates a new empty credentials object.
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

#ifdef G_OS_UNIX
static GCredentials *
g_credentials_new_for_unix_process (void)
{
  GCredentials *credentials;
  credentials = g_credentials_new ();
  credentials->priv->unix_user = getuid ();
  credentials->priv->unix_group = getgid ();
  credentials->priv->unix_process = getpid ();
  return credentials;
}
#endif

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_credentials_new_for_process:
 *
 * Gets the credentials for the current process. Note that the exact
 * set of credentials in the returned object vary according to
 * platform.
 *
 * Returns: A #GCredentials. Free with g_object_unref().
 *
 * Since: 2.26
 */
GCredentials *
g_credentials_new_for_process (void)
{
#ifdef G_OS_UNIX
  return g_credentials_new_for_unix_process ();
#elif G_OS_WIN32
  return g_credentials_new_for_win32_process ();
#else
#warning Please implement g_credentials_new_for_process() for your OS. For now g_credentials_new_for_process() will return empty credentials.
  return g_credentials_new ();
#endif
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_credentials_new_for_string:
 * @str: A string returned from g_credentials_to_string().
 * @error: Return location for error.
 *
 * Constructs a #GCredentials instance from @str.
 *
 * Returns: A #GCredentials or %NULL if @error is set. The return
 * object must be freed with g_object_unref().
 *
 * Since: 2.26
 */
GCredentials *
g_credentials_new_for_string (const gchar  *str,
                              GError      **error)
{
  GCredentials *credentials;
  gchar **tokens;
  guint n;

  g_return_val_if_fail (str != NULL, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  tokens = NULL;
  credentials = g_credentials_new ();

  if (!g_str_has_prefix (str, "GCredentials:"))
    goto fail;

  tokens = g_strsplit (str + sizeof "GCredentials:" - 1, ",", 0);
  for (n = 0; tokens[n] != NULL; n++)
    {
      const gchar *token = tokens[n];
      if (g_str_has_prefix (token, "unix-user:"))
        g_credentials_set_unix_user (credentials, atoi (token + sizeof ("unix-user:") - 1));
      else if (g_str_has_prefix (token, "unix-group:"))
        g_credentials_set_unix_group (credentials, atoi (token + sizeof ("unix-group:") - 1));
      else if (g_str_has_prefix (token, "unix-process:"))
        g_credentials_set_unix_process (credentials, atoi (token + sizeof ("unix-process:") - 1));
      else if (g_str_has_prefix (token, "windows-user:"))
        g_credentials_set_windows_user (credentials, token + sizeof ("windows-user:"));
      else
        goto fail;
    }
  g_strfreev (tokens);
  return credentials;

 fail:
  g_set_error (error,
               G_IO_ERROR,
               G_IO_ERROR_FAILED,
               _("The string `%s' is not a valid credentials string"),
               str);
  g_object_unref (credentials);
  g_strfreev (tokens);
  return NULL;
}

/**
 * g_credentials_to_string:
 * @credentials: A #GCredentials object.
 *
 * Serializes @credentials to a string that can be used with
 * g_credentials_new_for_string().
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
  if (credentials->priv->unix_user != -1)
    g_string_append_printf (ret, "unix-user=%" G_GINT64_FORMAT ",", credentials->priv->unix_user);
  if (credentials->priv->unix_group != -1)
    g_string_append_printf (ret, "unix-group=%" G_GINT64_FORMAT ",", credentials->priv->unix_group);
  if (credentials->priv->unix_process != -1)
    g_string_append_printf (ret, "unix-process=%" G_GINT64_FORMAT ",", credentials->priv->unix_process);
  if (credentials->priv->windows_user != NULL)
    g_string_append_printf (ret, "windows-user=%s,", credentials->priv->windows_user);
  if (ret->str[ret->len - 1] == ',')
    ret->str[ret->len - 1] = '\0';

  return g_string_free (ret, FALSE);
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_credentials_has_unix_user:
 * @credentials: A #GCredentials.
 *
 * Checks if @credentials has a UNIX user credential.
 *
 * Returns: %TRUE if @credentials has this type of credential, %FALSE otherwise.
 *
 * Since: 2.26
 */
gboolean
g_credentials_has_unix_user (GCredentials *credentials)
{
  g_return_val_if_fail (G_IS_CREDENTIALS (credentials), FALSE);
  return credentials->priv->unix_user != -1;
}

/**
 * g_credentials_get_unix_user:
 * @credentials: A #GCredentials.
 *
 * Gets the UNIX user identifier from @credentials.
 *
 * Returns: The identifier or -1 if unset.
 *
 * Since: 2.26
 */
gint64
g_credentials_get_unix_user (GCredentials *credentials)
{
  g_return_val_if_fail (G_IS_CREDENTIALS (credentials), -1);
  return credentials->priv->unix_user;
}

/**
 * g_credentials_set_unix_user:
 * @credentials: A #GCredentials.
 * @user_id: A UNIX user identifier (typically type #uid_t) or -1 to unset it.
 *
 * Sets the UNIX user identifier.
 *
 * Since: 2.26
 */
void
g_credentials_set_unix_user (GCredentials *credentials,
                             gint64        user_id)
{
  g_return_if_fail (G_IS_CREDENTIALS (credentials));
  credentials->priv->unix_user = user_id;
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_credentials_has_unix_group:
 * @credentials: A #GCredentials.
 *
 * Checks if @credentials has a UNIX group credential.
 *
 * Returns: %TRUE if @credentials has this type of credential, %FALSE otherwise.
 *
 * Since: 2.26
 */
gboolean
g_credentials_has_unix_group (GCredentials *credentials)
{
  g_return_val_if_fail (G_IS_CREDENTIALS (credentials), FALSE);
  return credentials->priv->unix_group != -1;
}

/**
 * g_credentials_get_unix_group:
 * @credentials: A #GCredentials.
 *
 * Gets the UNIX group identifier from @credentials.
 *
 * Returns: The identifier or -1 if unset.
 *
 * Since: 2.26
 */
gint64
g_credentials_get_unix_group (GCredentials *credentials)
{
  g_return_val_if_fail (G_IS_CREDENTIALS (credentials), -1);
  return credentials->priv->unix_group;
}

/**
 * g_credentials_set_unix_group:
 * @credentials: A #GCredentials.
 * @group_id: A UNIX group identifier (typically type #gid_t) or -1 to unset.
 *
 * Sets the UNIX group identifier.
 *
 * Since: 2.26
 */
void
g_credentials_set_unix_group (GCredentials *credentials,
                              gint64        group_id)
{
  g_return_if_fail (G_IS_CREDENTIALS (credentials));
  credentials->priv->unix_group = group_id;
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_credentials_has_unix_process:
 * @credentials: A #GCredentials.
 *
 * Checks if @credentials has a UNIX process credential.
 *
 * Returns: %TRUE if @credentials has this type of credential, %FALSE otherwise.
 *
 * Since: 2.26
 */
gboolean
g_credentials_has_unix_process (GCredentials *credentials)
{
  g_return_val_if_fail (G_IS_CREDENTIALS (credentials), FALSE);
  return credentials->priv->unix_process != -1;
}

/**
 * g_credentials_get_unix_process:
 * @credentials: A #GCredentials.
 *
 * Gets the UNIX process identifier from @credentials.
 *
 * Returns: The identifier or -1 if unset.
 *
 * Since: 2.26
 */
gint64
g_credentials_get_unix_process (GCredentials *credentials)
{
  g_return_val_if_fail (G_IS_CREDENTIALS (credentials), -1);
  return credentials->priv->unix_process;
}

/**
 * g_credentials_set_unix_process:
 * @credentials: A #GCredentials.
 * @process_id: A UNIX process identifier (typically type #pid_t/#GPid) or -1 to unset.
 *
 * Sets the UNIX process identifier.
 *
 * Since: 2.26
 */
void
g_credentials_set_unix_process (GCredentials *credentials,
                                gint64        process_id)
{
  g_return_if_fail (G_IS_CREDENTIALS (credentials));
  credentials->priv->unix_process = process_id;
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_credentials_has_windows_user:
 * @credentials: A #GCredentials.
 *
 * Checks if @credentials has a Windows user SID (Security Identifier).
 *
 * Returns: %TRUE if @credentials has this type of credential, %FALSE otherwise.
 *
 * Since: 2.26
 */
gboolean
g_credentials_has_windows_user  (GCredentials *credentials)
{
  g_return_val_if_fail (G_IS_CREDENTIALS (credentials), FALSE);
  return credentials->priv->windows_user != NULL;
}

/**
 * g_credentials_get_windows_user:
 * @credentials: A #GCredentials.
 *
 * Gets the Windows User SID from @credentials.
 *
 * Returns: A string or %NULL if unset. Do not free, the string is owned by @credentials.
 *
 * Since: 2.26
 */
const gchar *
g_credentials_get_windows_user  (GCredentials *credentials)
{
  g_return_val_if_fail (G_IS_CREDENTIALS (credentials), NULL);
  return credentials->priv->windows_user;
}

/**
 * g_credentials_set_windows_user:
 * @credentials: A #GCredentials.
 * @user_sid: The Windows User SID or %NULL to unset.
 *
 * Sets the Windows User SID.
 *
 * Since: 2.26
 */
void
g_credentials_set_windows_user (GCredentials    *credentials,
                                const gchar     *user_sid)
{
  g_return_if_fail (G_IS_CREDENTIALS (credentials));
  g_free (credentials->priv->windows_user);
  credentials->priv->windows_user = g_strdup (user_sid);
}

/* ---------------------------------------------------------------------------------------------------- */
