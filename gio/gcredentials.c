/* GDBus - GLib D-Bus Library
 *
 * Copyright (C) 2008-2010 Red Hat, Inc.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
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
 *
 * Author: David Zeuthen <davidz@redhat.com>
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>

#include <gobject/gvaluecollector.h>

#include "gcredentials.h"
#include "gcredentialsprivate.h"
#include "gnetworking.h"
#include "gioerror.h"
#include "gioenumtypes.h"

#include "glibintl.h"

/**
 * GCredentials:
 *
 * The `GCredentials` type is a reference-counted wrapper for native
 * credentials.
 *
 * The information in `GCredentials` is typically used for identifying,
 * authenticating and authorizing other processes.
 *
 * Some operating systems supports looking up the credentials of the remote
 * peer of a communication endpoint - see e.g. [method@Gio.Socket.get_credentials].
 *
 * Some operating systems supports securely sending and receiving
 * credentials over a Unix Domain Socket, see [class@Gio.UnixCredentialsMessage],
 * [method@Gio.UnixConnection.send_credentials] and
 * [method@Gio.UnixConnection.receive_credentials] for details.
 *
 * On Linux, the native credential type is a `struct ucred` - see the
 * [`unix(7)` man page](man:unix(7)) for details. This corresponds to
 * `G_CREDENTIALS_TYPE_LINUX_UCRED`.
 *
 * On Apple operating systems (including iOS, tvOS, and macOS), the native credential
 * type is a `struct xucred`. This corresponds to `G_CREDENTIALS_TYPE_APPLE_XUCRED`.
 *
 * On FreeBSD, Debian GNU/kFreeBSD, and GNU/Hurd, the native credential type is a
 * `struct cmsgcred`. This corresponds to `G_CREDENTIALS_TYPE_FREEBSD_CMSGCRED`.
 *
 * On NetBSD, the native credential type is a `struct unpcbid`.
 * This corresponds to `G_CREDENTIALS_TYPE_NETBSD_UNPCBID`.
 *
 * On OpenBSD, the native credential type is a `struct sockpeercred`.
 * This corresponds to `G_CREDENTIALS_TYPE_OPENBSD_SOCKPEERCRED`.
 *
 * On Solaris (including OpenSolaris and its derivatives), the native credential type
 * is a `ucred_t`. This corresponds to `G_CREDENTIALS_TYPE_SOLARIS_UCRED`.
 *
 * Since GLib 2.72, on Windows, the native credentials may contain the PID of a
 * process. This corresponds to `G_CREDENTIALS_TYPE_WIN32_PID`.
 *
 * Since: 2.26
 */

struct _GCredentials
{
  /*< private >*/
  GObject parent_instance;

#if defined(G_CREDENTIALS_USE_LINUX_UCRED)
  struct ucred native;
#elif defined(G_CREDENTIALS_USE_APPLE_XUCRED)
  struct xucred native;
  pid_t pid;
#elif defined(G_CREDENTIALS_USE_FREEBSD_CMSGCRED)
  struct cmsgcred native;
#elif defined(G_CREDENTIALS_USE_NETBSD_UNPCBID)
  struct unpcbid native;
#elif defined(G_CREDENTIALS_USE_OPENBSD_SOCKPEERCRED)
  struct sockpeercred native;
#elif defined(G_CREDENTIALS_USE_SOLARIS_UCRED)
  ucred_t *native;
#elif defined(G_CREDENTIALS_USE_WIN32_PID)
  DWORD native;
#else
  #ifdef __GNUC__
  #pragma GCC diagnostic push
  #pragma GCC diagnostic warning "-Wcpp"
  #warning Please add GCredentials support for your OS
  #pragma GCC diagnostic pop
  #endif
#endif
};

/**
 * GCredentialsClass:
 *
 * Class structure for #GCredentials.
 *
 * Since: 2.26
 */
struct _GCredentialsClass
{
  /*< private >*/
  GObjectClass parent_class;
};

G_DEFINE_TYPE (GCredentials, g_credentials, G_TYPE_OBJECT)

static void
g_credentials_finalize (GObject *object)
{
#if defined(G_CREDENTIALS_USE_SOLARIS_UCRED)
  GCredentials *credentials = G_CREDENTIALS (object);

  ucred_free (credentials->native);
#endif

  if (G_OBJECT_CLASS (g_credentials_parent_class)->finalize != NULL)
    G_OBJECT_CLASS (g_credentials_parent_class)->finalize (object);
}


static void
g_credentials_class_init (GCredentialsClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = g_credentials_finalize;
}

static void
g_credentials_init (GCredentials *credentials)
{
#if defined(G_CREDENTIALS_USE_LINUX_UCRED)
  credentials->native.pid = getpid ();
  credentials->native.uid = geteuid ();
  credentials->native.gid = getegid ();
#elif defined(G_CREDENTIALS_USE_APPLE_XUCRED)
  gsize i;

  credentials->native.cr_version = XUCRED_VERSION;
  credentials->native.cr_uid = geteuid ();
  credentials->native.cr_ngroups = 1;
  credentials->native.cr_groups[0] = getegid ();

  /* FIXME: In principle this could use getgroups() to fill in the rest
   * of cr_groups, but then we'd have to handle the case where a process
   * can have more than NGROUPS groups, if that's even possible. A macOS
   * user would have to develop and test this.
   *
   * For now we fill it with -1 (meaning "no data"). */
  for (i = 1; i < NGROUPS; i++)
    credentials->native.cr_groups[i] = -1;

  credentials->pid = -1;
#elif defined(G_CREDENTIALS_USE_FREEBSD_CMSGCRED)
  memset (&credentials->native, 0, sizeof (struct cmsgcred));
  credentials->native.cmcred_pid  = getpid ();
  credentials->native.cmcred_euid = geteuid ();
  credentials->native.cmcred_gid  = getegid ();
#elif defined(G_CREDENTIALS_USE_NETBSD_UNPCBID)
  credentials->native.unp_pid = getpid ();
  credentials->native.unp_euid = geteuid ();
  credentials->native.unp_egid = getegid ();
#elif defined(G_CREDENTIALS_USE_OPENBSD_SOCKPEERCRED)
  credentials->native.pid = getpid ();
  credentials->native.uid = geteuid ();
  credentials->native.gid = getegid ();
#elif defined(G_CREDENTIALS_USE_SOLARIS_UCRED)
  credentials->native = ucred_get (P_MYID);
#elif defined(G_CREDENTIALS_USE_WIN32_PID)
  credentials->native = GetCurrentProcessId ();
#endif
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_credentials_new:
 *
 * Creates a new #GCredentials object with credentials matching the
 * the current process.
 *
 * Returns: (transfer full): A #GCredentials. Free with g_object_unref().
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
 * Returns: (transfer full): A string that should be freed with g_free().
 *
 * Since: 2.26
 */
gchar *
g_credentials_to_string (GCredentials *credentials)
{
  GString *ret;
#if defined(G_CREDENTIALS_USE_APPLE_XUCRED)
  glib_typeof (credentials->native.cr_ngroups) i;
#endif

  g_return_val_if_fail (G_IS_CREDENTIALS (credentials), NULL);

  ret = g_string_new ("GCredentials:");
#if defined(G_CREDENTIALS_USE_LINUX_UCRED)
  g_string_append (ret, "linux-ucred:");
  if (credentials->native.pid != (pid_t) -1)
    g_string_append_printf (ret, "pid=%" G_GINT64_FORMAT ",", (gint64) credentials->native.pid);
  if (credentials->native.uid != (uid_t) -1)
    g_string_append_printf (ret, "uid=%" G_GINT64_FORMAT ",", (gint64) credentials->native.uid);
  if (credentials->native.gid != (gid_t) -1)
    g_string_append_printf (ret, "gid=%" G_GINT64_FORMAT ",", (gint64) credentials->native.gid);
  if (ret->str[ret->len - 1] == ',')
    ret->str[ret->len - 1] = '\0';
#elif defined(G_CREDENTIALS_USE_APPLE_XUCRED)
  g_string_append (ret, "apple-xucred:");
  g_string_append_printf (ret, "version=%u,", credentials->native.cr_version);
  if (credentials->native.cr_uid != (uid_t) -1)
    g_string_append_printf (ret, "uid=%" G_GINT64_FORMAT ",", (gint64) credentials->native.cr_uid);
  for (i = 0; i < credentials->native.cr_ngroups; i++)
    g_string_append_printf (ret, "gid=%" G_GINT64_FORMAT ",", (gint64) credentials->native.cr_groups[i]);
  if (ret->str[ret->len - 1] == ',')
    ret->str[ret->len - 1] = '\0';
#elif defined(G_CREDENTIALS_USE_FREEBSD_CMSGCRED)
  g_string_append (ret, "freebsd-cmsgcred:");
  if (credentials->native.cmcred_pid != (pid_t) -1)
    g_string_append_printf (ret, "pid=%" G_GINT64_FORMAT ",", (gint64) credentials->native.cmcred_pid);
  if (credentials->native.cmcred_euid != (uid_t) -1)
    g_string_append_printf (ret, "uid=%" G_GINT64_FORMAT ",", (gint64) credentials->native.cmcred_euid);
  if (credentials->native.cmcred_gid != (gid_t) -1)
    g_string_append_printf (ret, "gid=%" G_GINT64_FORMAT ",", (gint64) credentials->native.cmcred_gid);
#elif defined(G_CREDENTIALS_USE_NETBSD_UNPCBID)
  g_string_append (ret, "netbsd-unpcbid:");
  if (credentials->native.unp_pid != (pid_t) -1)
    g_string_append_printf (ret, "pid=%" G_GINT64_FORMAT ",", (gint64) credentials->native.unp_pid);
  if (credentials->native.unp_euid != (uid_t) -1)
    g_string_append_printf (ret, "uid=%" G_GINT64_FORMAT ",", (gint64) credentials->native.unp_euid);
  if (credentials->native.unp_egid != (gid_t) -1)
    g_string_append_printf (ret, "gid=%" G_GINT64_FORMAT ",", (gint64) credentials->native.unp_egid);
  ret->str[ret->len - 1] = '\0';
#elif defined(G_CREDENTIALS_USE_OPENBSD_SOCKPEERCRED)
  g_string_append (ret, "openbsd-sockpeercred:");
  if (credentials->native.pid != (pid_t) -1)
    g_string_append_printf (ret, "pid=%" G_GINT64_FORMAT ",", (gint64) credentials->native.pid);
  if (credentials->native.uid != (uid_t) -1)
    g_string_append_printf (ret, "uid=%" G_GINT64_FORMAT ",", (gint64) credentials->native.uid);
  if (credentials->native.gid != (gid_t) -1)
    g_string_append_printf (ret, "gid=%" G_GINT64_FORMAT ",", (gint64) credentials->native.gid);
  if (ret->str[ret->len - 1] == ',')
    ret->str[ret->len - 1] = '\0';
#elif defined(G_CREDENTIALS_USE_SOLARIS_UCRED)
  g_string_append (ret, "solaris-ucred:");
  {
    id_t id;
    if ((id = ucred_getpid (credentials->native)) != (id_t) -1)
      g_string_append_printf (ret, "pid=%" G_GINT64_FORMAT ",", (gint64) id);
    if ((id = ucred_geteuid (credentials->native)) != (id_t) -1)
      g_string_append_printf (ret, "uid=%" G_GINT64_FORMAT ",", (gint64) id);
    if ((id = ucred_getegid (credentials->native)) != (id_t) -1)
      g_string_append_printf (ret, "gid=%" G_GINT64_FORMAT ",", (gint64) id);
    if (ret->str[ret->len - 1] == ',')
      ret->str[ret->len - 1] = '\0';
  }
#elif defined(G_CREDENTIALS_USE_WIN32_PID)
  g_string_append_printf (ret, "win32-pid:pid=%lu", credentials->native);
#else
  g_string_append (ret, "unknown");
#endif

  return g_string_free (ret, FALSE);
}

/* ---------------------------------------------------------------------------------------------------- */

#if defined(G_CREDENTIALS_USE_LINUX_UCRED)
/*
 * Check whether @native contains invalid data. If getsockopt SO_PEERCRED
 * is used on a TCP socket, it succeeds but yields a credentials structure
 * with pid 0, uid -1 and gid -1. Similarly, if SO_PASSCRED is used on a
 * receiving Unix socket when the sending socket did not also enable
 * SO_PASSCRED, it can succeed but yield a credentials structure with
 * pid 0, uid /proc/sys/kernel/overflowuid and gid
 * /proc/sys/kernel/overflowgid.
 */
static gboolean
linux_ucred_check_valid (struct ucred  *native,
                         GError       **error)
{
  if (native->pid == 0
      || native->uid == (uid_t) -1
      || native->gid == (gid_t) -1)
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_INVALID_DATA,
                           _("GCredentials contains invalid data"));
      return FALSE;
    }

  return TRUE;
}
#endif

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
#if defined(G_CREDENTIALS_USE_LINUX_UCRED)
  if (linux_ucred_check_valid (&credentials->native, NULL)
      && credentials->native.uid == other_credentials->native.uid)
    ret = TRUE;
#elif defined(G_CREDENTIALS_USE_APPLE_XUCRED)
  if (credentials->native.cr_version == other_credentials->native.cr_version &&
      credentials->native.cr_uid == other_credentials->native.cr_uid)
    ret = TRUE;
#elif defined(G_CREDENTIALS_USE_FREEBSD_CMSGCRED)
  if (credentials->native.cmcred_euid == other_credentials->native.cmcred_euid)
    ret = TRUE;
#elif defined(G_CREDENTIALS_USE_NETBSD_UNPCBID)
  if (credentials->native.unp_euid == other_credentials->native.unp_euid)
    ret = TRUE;
#elif defined(G_CREDENTIALS_USE_OPENBSD_SOCKPEERCRED)
  if (credentials->native.uid == other_credentials->native.uid)
    ret = TRUE;
#elif defined(G_CREDENTIALS_USE_SOLARIS_UCRED)
  if (ucred_geteuid (credentials->native) == ucred_geteuid (other_credentials->native))
    ret = TRUE;
#else
  g_set_error_literal (error,
                       G_IO_ERROR,
                       G_IO_ERROR_NOT_SUPPORTED,
                       _("GCredentials is not implemented on this OS"));
#endif

  return ret;
}

static gboolean
credentials_native_type_check (GCredentialsType  requested_type,
                               const char       *op)
{
  GEnumClass *enum_class;
  GEnumValue *requested;
#if defined(G_CREDENTIALS_SUPPORTED)
  GEnumValue *supported;
#endif

#if defined(G_CREDENTIALS_SUPPORTED)
  if (requested_type == G_CREDENTIALS_NATIVE_TYPE)
    return TRUE;
#endif

  enum_class = g_type_class_ref (g_credentials_type_get_type ());
  requested = g_enum_get_value (enum_class, requested_type);

#if defined(G_CREDENTIALS_SUPPORTED)
  supported = g_enum_get_value (enum_class, G_CREDENTIALS_NATIVE_TYPE);
  g_assert (supported);
  g_warning ("g_credentials_%s_native: Trying to %s credentials of type %s "
             "but only %s is supported on this platform.",
             op, op,
             requested ? requested->value_name : "(unknown)",
             supported->value_name);
#else
  g_warning ("g_credentials_%s_native: Trying to %s credentials of type %s "
             "but there is no support for GCredentials on this platform.",
             op, op,
             requested ? requested->value_name : "(unknown)");
#endif

  g_type_class_unref (enum_class);
  return FALSE;
}

/**
 * g_credentials_get_native: (skip)
 * @credentials: A #GCredentials.
 * @native_type: The type of native credentials to get.
 *
 * Gets a pointer to native credentials of type @native_type from
 * @credentials.
 *
 * It is a programming error (which will cause a warning to be
 * logged) to use this method if there is no #GCredentials support for
 * the OS or if @native_type isn't supported by the OS.
 *
 * Returns: (transfer none) (nullable): The pointer to native credentials or
 *     %NULL if there is no #GCredentials support for the OS or if @native_type
 *     isn't supported by the OS. Do not free the returned data, it is owned
 *     by @credentials.
 *
 * Since: 2.26
 */
gpointer
g_credentials_get_native (GCredentials     *credentials,
                          GCredentialsType  native_type)
{
  g_return_val_if_fail (G_IS_CREDENTIALS (credentials), NULL);

  if (!credentials_native_type_check (native_type, "get"))
    return NULL;

#if defined(G_CREDENTIALS_USE_SOLARIS_UCRED)
  return credentials->native;
#elif defined(G_CREDENTIALS_SUPPORTED)
  return &credentials->native;
#else
  g_assert_not_reached ();
#endif
}

/**
 * g_credentials_set_native:
 * @credentials: A #GCredentials.
 * @native_type: The type of native credentials to set.
 * @native: (not nullable): A pointer to native credentials.
 *
 * Copies the native credentials of type @native_type from @native
 * into @credentials.
 *
 * It is a programming error (which will cause a warning to be
 * logged) to use this method if there is no #GCredentials support for
 * the OS or if @native_type isn't supported by the OS.
 *
 * Since: 2.26
 */
void
g_credentials_set_native (GCredentials     *credentials,
                          GCredentialsType  native_type,
                          gpointer          native)
{
  if (!credentials_native_type_check (native_type, "set"))
    return;

#if defined(G_CREDENTIALS_USE_SOLARIS_UCRED)
  memcpy (credentials->native, native, ucred_size ());
#elif defined(G_CREDENTIALS_SUPPORTED)
  memcpy (&credentials->native, native, sizeof (credentials->native));
#else
  g_assert_not_reached ();
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
 * Returns: The UNIX user identifier or `-1` if @error is set.
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

#if defined(G_CREDENTIALS_USE_LINUX_UCRED)
  if (linux_ucred_check_valid (&credentials->native, error))
    ret = credentials->native.uid;
  else
    ret = -1;
#elif defined(G_CREDENTIALS_USE_APPLE_XUCRED)
  if (credentials->native.cr_version == XUCRED_VERSION)
    {
      ret = credentials->native.cr_uid;
    }
  else
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                   /* No point in translating the part in parentheses... */
                   "%s (struct xucred cr_version %u != %u)",
                   _("There is no GCredentials support for your platform"),
                   credentials->native.cr_version,
                   XUCRED_VERSION);
      ret = -1;
    }
#elif defined(G_CREDENTIALS_USE_FREEBSD_CMSGCRED)
  ret = credentials->native.cmcred_euid;
#elif defined(G_CREDENTIALS_USE_NETBSD_UNPCBID)
  ret = credentials->native.unp_euid;
#elif defined(G_CREDENTIALS_USE_OPENBSD_SOCKPEERCRED)
  ret = credentials->native.uid;
#elif defined(G_CREDENTIALS_USE_SOLARIS_UCRED)
  ret = ucred_geteuid (credentials->native);
#else
  ret = -1;
  g_set_error_literal (error,
                       G_IO_ERROR,
                       G_IO_ERROR_NOT_SUPPORTED,
                       _("There is no GCredentials support for your platform"));
#endif

  return ret;
}

/**
 * g_credentials_get_unix_pid:
 * @credentials: A #GCredentials
 * @error: Return location for error or %NULL.
 *
 * Tries to get the UNIX process identifier from @credentials. This
 * method is only available on UNIX platforms.
 *
 * This operation can fail if #GCredentials is not supported on the
 * OS or if the native credentials type does not contain information
 * about the UNIX process ID.
 *
 * Returns: The UNIX process ID, or `-1` if @error is set.
 *
 * Since: 2.36
 */
pid_t
g_credentials_get_unix_pid (GCredentials    *credentials,
                            GError         **error)
{
  pid_t ret;

  g_return_val_if_fail (G_IS_CREDENTIALS (credentials), -1);
  g_return_val_if_fail (error == NULL || *error == NULL, -1);

#if defined(G_CREDENTIALS_USE_LINUX_UCRED)
  if (linux_ucred_check_valid (&credentials->native, error))
    ret = credentials->native.pid;
  else
    ret = -1;
#elif defined(G_CREDENTIALS_USE_FREEBSD_CMSGCRED)
  ret = credentials->native.cmcred_pid;
#elif defined(G_CREDENTIALS_USE_NETBSD_UNPCBID)
  ret = credentials->native.unp_pid;
#elif defined(G_CREDENTIALS_USE_OPENBSD_SOCKPEERCRED)
  ret = credentials->native.pid;
#elif defined(G_CREDENTIALS_USE_SOLARIS_UCRED)
  ret = ucred_getpid (credentials->native);
#elif defined(G_CREDENTIALS_USE_WIN32_PID)
  ret = credentials->native;
#else

#if defined(G_CREDENTIALS_USE_APPLE_XUCRED)
  ret = credentials->pid;
#else
  ret = -1;
#endif

  if (ret == -1)
    g_set_error_literal (error,
                         G_IO_ERROR,
                         G_IO_ERROR_NOT_SUPPORTED,
                         _("GCredentials does not contain a process ID on this OS"));
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
 * about the UNIX user. It can also fail if the OS does not allow the
 * use of "spoofed" credentials.
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
  gboolean ret = FALSE;

  g_return_val_if_fail (G_IS_CREDENTIALS (credentials), FALSE);
  g_return_val_if_fail (uid != (uid_t) -1, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

#if defined(G_CREDENTIALS_USE_LINUX_UCRED)
  credentials->native.uid = uid;
  ret = TRUE;
#elif defined(G_CREDENTIALS_USE_APPLE_XUCRED)
  credentials->native.cr_uid = uid;
  ret = TRUE;
#elif defined(G_CREDENTIALS_USE_FREEBSD_CMSGCRED)
  credentials->native.cmcred_euid = uid;
  ret = TRUE;
#elif defined(G_CREDENTIALS_USE_NETBSD_UNPCBID)
  credentials->native.unp_euid = uid;
  ret = TRUE;
#elif defined(G_CREDENTIALS_USE_OPENBSD_SOCKPEERCRED)
  credentials->native.uid = uid;
  ret = TRUE;
#elif !defined(G_CREDENTIALS_SPOOFING_SUPPORTED)
  g_set_error_literal (error,
                       G_IO_ERROR,
                       G_IO_ERROR_PERMISSION_DENIED,
                       _("Credentials spoofing is not possible on this OS"));
  ret = FALSE;
#else
  g_set_error_literal (error,
                       G_IO_ERROR,
                       G_IO_ERROR_NOT_SUPPORTED,
                       _("GCredentials is not implemented on this OS"));
  ret = FALSE;
#endif

  return ret;
}

#ifdef __APPLE__
void
_g_credentials_set_local_peerid (GCredentials *credentials,
                                 pid_t         pid)
{
  g_return_if_fail (G_IS_CREDENTIALS (credentials));
  g_return_if_fail (pid >= 0);

  credentials->pid = pid;
}
#endif /* __APPLE__ */

#endif /* G_OS_UNIX */
