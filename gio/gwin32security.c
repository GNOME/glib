/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright (C) 2026 Dimitriy Ryazantcev
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
 */

#include "config.h"

#include "gwin32security.h"
#include "gwin32sid.h"
#include "gioerror.h"
#include "glibintl.h"
#include "glib/gstdio.h"

#include <sddl.h>
#include <aclapi.h>
#include <errno.h>
#include <fcntl.h>
#include <io.h>
#include <stdint.h>

/* The keyring directory and the cookie files in it are checked and made as the
 * D-Bus specification says for Windows:
 * https://dbus.freedesktop.org/doc/dbus-specification.html#auth-mechanisms-sha
 *
 * An object that fails the check is never repaired, as it may have been read
 * already: a cookie file is replaced by a new one, and a directory is refused.
 * G_DBUS_COOKIE_SHA1_KEYRING_DIR_IGNORE_PERMISSION skips the checks.
 */

#define SHARE_ALL (FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE)

/* Rights that others must not have (see the specification) */
#define DIRECTORY_FORBIDDEN_RIGHTS (FILE_WRITE_DATA | FILE_APPEND_DATA | FILE_DELETE_CHILD | DELETE | \
                                    WRITE_DAC | WRITE_OWNER)
#define FILE_FORBIDDEN_RIGHTS (FILE_READ_DATA | FILE_WRITE_DATA | FILE_APPEND_DATA | DELETE | \
                               WRITE_DAC | WRITE_OWNER)

static void
set_win32_error (GError     **error,
                 DWORD        code,
                 const gchar *what)
{
  GIOErrorEnum io_code;
  gchar *emsg = g_win32_error_message (code);

  switch (code)
    {
    case ERROR_ACCESS_DENIED:
      io_code = G_IO_ERROR_PERMISSION_DENIED;
      break;
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
      io_code = G_IO_ERROR_NOT_FOUND;
      break;
    default:
      io_code = g_io_error_from_win32_error (code);
    }

  g_set_error (error, G_IO_ERROR, io_code, _("%s failed: %s"), what, emsg);
  g_free (emsg);
}

/* Opens the plain directory (if @directory is set) or file @name with
 * @access, without following a link
 */
static HANDLE
open_plain (const gchar  *name,
            DWORD         access,
            gboolean      directory,
            GError      **error)
{
  BY_HANDLE_FILE_INFORMATION information;
  gunichar2 *wname;
  HANDLE handle;
  DWORD last_error;

  wname = g_utf8_to_utf16 (name, -1, NULL, NULL, NULL);
  if (wname == NULL)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                   _("The name “%s” is not valid"), name);
      return INVALID_HANDLE_VALUE;
    }

  handle = CreateFileW (wname, access | FILE_READ_ATTRIBUTES, SHARE_ALL,
                        NULL, OPEN_EXISTING,
                        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
  last_error = GetLastError ();
  g_free (wname);

  if (handle == INVALID_HANDLE_VALUE)
    {
      set_win32_error (error, last_error, "CreateFile");
      return INVALID_HANDLE_VALUE;
    }

  if (!GetFileInformationByHandle (handle, &information))
    {
      set_win32_error (error, GetLastError (), "GetFileInformationByHandle");
      CloseHandle (handle);
      return INVALID_HANDLE_VALUE;
    }

  if (((information.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) != directory ||
      (information.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED,
                   directory ? _("“%s” is not a plain directory") : _("“%s” is not a plain file"),
                   name);
      CloseHandle (handle);
      return INVALID_HANDLE_VALUE;
    }

  return handle;
}

/* Whether @sid is a trusted principal (see the specification).  The
 * administrators are trusted as they can take ownership of anything anyway.
 */
static gboolean
is_trusted_sid (PSID  sid,
                SID  *user)
{
  return sid != NULL && IsValidSid (sid) &&
         (EqualSid (sid, user) ||
          IsWellKnownSid (sid, WinLocalSystemSid) ||
          IsWellKnownSid (sid, WinBuiltinAdministratorsSid));
}

/* Whether @dacl grants others none of the forbidden rights, as the
 * specification says
 */
static gboolean
dacl_ok (PACL          dacl,
         SID          *user,
         gboolean      directory,
         const gchar  *name,
         GError      **error)
{
  GENERIC_MAPPING mapping = { FILE_GENERIC_READ, FILE_GENERIC_WRITE,
                              FILE_GENERIC_EXECUTE, FILE_ALL_ACCESS };
  DWORD forbidden = directory ? DIRECTORY_FORBIDDEN_RIGHTS : FILE_FORBIDDEN_RIGHTS;
  WORD i;

  if (dacl == NULL)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED,
                   _("The access rights of “%s” are open to everyone"), name);
      return FALSE;
    }

  /* A DACL that is malformed (a remote file server can send one) is refused */
  if (!IsValidAcl (dacl))
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED,
                   _("The access rights of “%s” are not valid"), name);
      return FALSE;
    }

  for (i = 0; i < dacl->AceCount; i++)
    {
      ACE_HEADER *ace;
      ACCESS_ALLOWED_ACE *allowed;
      DWORD mask;
      PSID sid;

      if (!GetAce (dacl, i, (void **) &ace))
        {
          set_win32_error (error, GetLastError (), "GetAce");
          return FALSE;
        }

      /* An entry that takes rights away gives nothing */
      if (ace->AceType == ACCESS_DENIED_ACE_TYPE)
        continue;

      /* Entries of other types are not understood, so not trusted */
      if (ace->AceType != ACCESS_ALLOWED_ACE_TYPE)
        {
          g_set_error (error, G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED,
                       _("The access rights of “%s” have an entry of the type %d, which is not supported"),
                       name, ace->AceType);
          return FALSE;
        }

      /* An entry that is too short or reaches past the DACL is refused */
      if (ace->AceSize < sizeof (ACCESS_ALLOWED_ACE) ||
          (BYTE *) ace + ace->AceSize > (BYTE *) dacl + dacl->AclSize)
        {
          g_set_error (error, G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED,
                       _("The access rights of “%s” have an entry that is not valid"), name);
          return FALSE;
        }

      allowed = (ACCESS_ALLOWED_ACE *) ace;
      sid = (PSID) &allowed->SidStart;

      /* An entry that is only for what is made in the directory does not
       * apply to the directory itself
       */
      if ((ace->AceFlags & INHERIT_ONLY_ACE) != 0 || is_trusted_sid (sid, user))
        continue;

      mask = allowed->Mask;
      MapGenericMask (&mask, &mapping);

      if ((mask & forbidden) != 0)
        {
          gchar *sid_string = _g_win32_sid_to_string (sid, NULL);

          g_set_error (error, G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED,
                       directory ? _("The access rights of “%s” give write access to %s")
                                 : _("The access rights of “%s” give access to %s"),
                       name, sid_string);
          g_free (sid_string);
          return FALSE;
        }
    }

  return TRUE;
}

/* Checks the volume, the owner and the DACL of the directory or file @handle
 * (opened with READ_CONTROL).  @name is used in the messages.
 */
static gboolean
check_handle (HANDLE        handle,
              const gchar  *name,
              gboolean      directory,
              GError      **error)
{
  PSID owner = NULL;
  PACL dacl = NULL;
  PSECURITY_DESCRIPTOR descriptor = NULL;
  DWORD code, flags = 0;
  SID *user;
  gboolean ok = FALSE;

  /* A volume without ACLs (FAT, exFAT) cannot keep anything private, and one
   * that cannot be queried is not assumed to keep them, as the specification
   * says that a volume that cannot store DACLs must not be used
   */
  if (!GetVolumeInformationByHandleW (handle, NULL, 0, NULL, NULL, &flags, NULL, 0) ||
      (flags & FILE_PERSISTENT_ACLS) == 0)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED,
                   _("The volume of “%s” cannot be shown to keep access rights, so nothing on it is private"),
                   name);
      return FALSE;
    }

  user = _g_win32_process_get_access_token_sid (0, error);
  if (user == NULL)
    return FALSE;

  code = GetSecurityInfo (handle, SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
                          &owner, NULL, &dacl, NULL, &descriptor);
  if (code != ERROR_SUCCESS)
    {
      set_win32_error (error, code, "GetSecurityInfo");
      goto out;
    }

  /* What libdbus or an older GLib made may be owned by SYSTEM or the
   * administrators, as it is when the process is elevated
   */
  if (!is_trusted_sid (owner, user))
    {
      gchar *owner_string = owner != NULL ? _g_win32_sid_to_string (owner, NULL) : NULL;

      g_set_error (error, G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED,
                   _("“%s” is owned by %s, not by the user, SYSTEM or the administrators"),
                   name, owner_string);
      g_free (owner_string);
      goto out;
    }

  ok = dacl_ok (dacl, user, directory, name, error);

out:
  LocalFree (descriptor);
  g_free (user);

  return ok;
}

/* The security descriptor of a private directory or file, as the specification
 * says, owned by the user by name (the default owner of an elevated process
 * is the administrators).  The entries are inheritable if @inheritable is set.
 * Free with LocalFree().
 */
static PSECURITY_DESCRIPTOR
make_private_descriptor (gboolean   inheritable,
                         GError   **error)
{
  PSECURITY_DESCRIPTOR descriptor = NULL;
  const gchar *flags = inheritable ? "OICI" : "";
  gchar *sid_string, *sddl;

  sid_string = _g_win32_current_process_sid_string (error);
  if (sid_string == NULL)
    return NULL;

  sddl = g_strconcat ("O:", sid_string, "D:P(A;", flags, ";FA;;;", sid_string,
                      ")(A;", flags, ";FA;;;SY)", NULL);
  if (!ConvertStringSecurityDescriptorToSecurityDescriptorA (sddl, SDDL_REVISION_1, &descriptor, NULL))
    set_win32_error (error, GetLastError (), "ConvertStringSecurityDescriptorToSecurityDescriptor");

  g_free (sddl);
  g_free (sid_string);

  return descriptor;
}

/* Makes the new directory (if @directory is set) or file @name with the
 * private descriptor, and for a file returns its handle in @file.  If @name
 * exists the error is %G_IO_ERROR_EXISTS.
 */
static gboolean
create_private (const gchar  *name,
                gboolean      directory,
                HANDLE       *file,
                GError      **error)
{
  SECURITY_ATTRIBUTES attributes = { sizeof (attributes), NULL, FALSE };
  gunichar2 *wname;
  gboolean ok;
  DWORD code;

  attributes.lpSecurityDescriptor = make_private_descriptor (directory, error);
  if (attributes.lpSecurityDescriptor == NULL)
    return FALSE;

  wname = g_utf8_to_utf16 (name, -1, NULL, NULL, NULL);
  if (wname == NULL)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                   _("The name “%s” is not valid"), name);
      LocalFree (attributes.lpSecurityDescriptor);
      return FALSE;
    }

  if (directory)
    ok = CreateDirectoryW (wname, &attributes);
  else
    {
      *file = CreateFileW (wname, GENERIC_READ | GENERIC_WRITE, SHARE_ALL, &attributes,
                           CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
      ok = *file != INVALID_HANDLE_VALUE;
    }
  code = GetLastError ();
  g_free (wname);
  LocalFree (attributes.lpSecurityDescriptor);

  if (ok)
    return TRUE;

  if (code == ERROR_FILE_EXISTS || code == ERROR_ALREADY_EXISTS)
    g_set_error (error, G_IO_ERROR, G_IO_ERROR_EXISTS, _("“%s” exists already"), name);
  else
    set_win32_error (error, code, directory ? "CreateDirectory" : "CreateFile");

  return FALSE;
}

static gboolean
check_directory (const gchar  *dir,
                 GError      **error)
{
  HANDLE handle = open_plain (dir, READ_CONTROL, TRUE, error);
  gboolean ok;

  if (handle == INVALID_HANDLE_VALUE)
    return FALSE;

  ok = check_handle (handle, dir, TRUE, error);
  CloseHandle (handle);

  return ok;
}

/**
 * g_win32_read_private_file: (skip)
 * @file: a file, as a UTF-8 path
 * @contents: (out) (transfer full): return location for the contents, which
 *   end with a nul byte
 * @error: return location for a #GError, or %NULL
 *
 * Reads @file if it passes the check of the specification, on the handle that
 * is read.  If @file does not exist @error is %G_IO_ERROR_NOT_FOUND, and if
 * it fails the check it is %G_IO_ERROR_PERMISSION_DENIED.
 *
 * Returns: %TRUE if @file was read, %FALSE with @error set otherwise
 */
gboolean
g_win32_read_private_file (const gchar  *file,
                           gchar       **contents,
                           GError      **error)
{
  HANDLE handle;
  LARGE_INTEGER size = { 0 };
  gchar *buffer;
  DWORD count = 0;
  BOOL ok;

  g_return_val_if_fail (file != NULL, FALSE);
  g_return_val_if_fail (contents != NULL, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  handle = open_plain (file, READ_CONTROL | GENERIC_READ, FALSE, error);
  if (handle == INVALID_HANDLE_VALUE)
    return FALSE;

  if (!check_handle (handle, file, FALSE, error))
    {
      CloseHandle (handle);
      return FALSE;
    }

  if (!GetFileSizeEx (handle, &size) || size.QuadPart > G_MAXINT)
    {
      set_win32_error (error, size.QuadPart > G_MAXINT ? ERROR_FILE_TOO_LARGE : GetLastError (),
                       "GetFileSizeEx");
      CloseHandle (handle);
      return FALSE;
    }

  /* A file on a local volume is read in one go */
  buffer = g_malloc (size.QuadPart + 1);
  ok = ReadFile (handle, buffer, (DWORD) size.QuadPart, &count, NULL);
  if (!ok || count != size.QuadPart)
    {
      set_win32_error (error, ok ? ERROR_HANDLE_EOF : GetLastError (), "ReadFile");
      g_free (buffer);
      CloseHandle (handle);
      return FALSE;
    }

  CloseHandle (handle);
  buffer[count] = '\0';
  *contents = buffer;

  return TRUE;
}

/**
 * g_win32_ensure_private_directory: (skip)
 * @dir: a directory, as a UTF-8 path
 * @error: return location for a #GError, or %NULL
 *
 * Makes @dir, with its parents, as a private directory if it does not exist.
 * An existing directory is only accepted if it passes the check of the
 * specification, and is not changed.
 *
 * Returns: %TRUE if @dir is a private directory, %FALSE with @error set
 *   otherwise
 */
gboolean
g_win32_ensure_private_directory (const gchar  *dir,
                                  GError      **error)
{
  GError *local_error = NULL;
  gchar *parent;
  gboolean ok;

  g_return_val_if_fail (dir != NULL, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  ok = check_directory (dir, &local_error);
  if (ok)
    return TRUE;

  if (!g_error_matches (local_error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
    {
      g_propagate_error (error, local_error);
      return FALSE;
    }

  g_clear_error (&local_error);
  parent = g_path_get_dirname (dir);
  g_mkdir_with_parents (parent, 0700);
  g_free (parent);

  if (!create_private (dir, TRUE, NULL, &local_error) &&
      !g_error_matches (local_error, G_IO_ERROR, G_IO_ERROR_EXISTS))
    {
      g_propagate_error (error, local_error);
      return FALSE;
    }

  /* The directory may have been replaced since, so it is checked */
  g_clear_error (&local_error);

  return check_directory (dir, error);
}

/* Makes the new private file @file for reading and writing.  It fails with
 * %G_IO_ERROR_EXISTS if @file exists.  Returns a file descriptor, or -1 with
 * @error set.
 */
static gint
create_private_file (const gchar  *file,
                     GError      **error)
{
  HANDLE handle;
  gint fd;

  if (!create_private (file, FALSE, &handle, error))
    return -1;

  fd = _open_osfhandle ((intptr_t) handle, _O_BINARY | _O_NOINHERIT);
  if (fd < 0)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                   _("Cannot use the file “%s”"), file);
      CloseHandle (handle);
    }

  return fd;
}

/**
 * g_win32_set_contents_private: (skip)
 * @file: a file, as a UTF-8 path
 * @contents: (array length=length): what to write
 * @length: the length of @contents, at most %G_MAXINT
 * @error: return location for a #GError, or %NULL
 *
 * Writes @contents to a new private file next to @file (see
 * create_private_file()), which then replaces @file, so @file is never seen
 * half written and has a private DACL whatever it had before.
 *
 * Returns: %TRUE if @file was written, %FALSE with @error set otherwise
 */
gboolean
g_win32_set_contents_private (const gchar  *file,
                              const gchar  *contents,
                              gsize         length,
                              GError      **error)
{
  gchar *temp;
  gboolean ok = FALSE;
  gint fd, errsv = 0, tries;

  g_return_val_if_fail (file != NULL, FALSE);
  g_return_val_if_fail (contents != NULL || length == 0, FALSE);
  g_return_val_if_fail (length <= G_MAXINT, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  /* The name is not made to be unpredictable: the file is created and does
   * not replace one, so a name that is taken just fails
   */
  temp = g_strdup_printf ("%s.%08x%08x.tmp", file, g_random_int (), g_random_int ());
  fd = create_private_file (temp, error);
  if (fd < 0)
    {
      g_free (temp);
      return FALSE;
    }

  if (_write (fd, contents, (unsigned int) length) != (gint) length)
    errsv = errno != 0 ? errno : ENOSPC;
  else if (g_fsync (fd) != 0)
    errsv = errno;

  /* The file is closed before it is moved, as it cannot be renamed otherwise */
  if (_close (fd) != 0 && errsv == 0)
    errsv = errno;

  /* A reader that has the file open without sharing it for deletion makes the
   * move fail for as long as it reads, which is not long
   */
  for (tries = 0; errsv == 0 && !(ok = g_rename (temp, file) == 0); tries++)
    {
      if (errno != EACCES || tries == 4)
        {
          errsv = errno;
          break;
        }

      g_usleep (10 * 1000);
    }

  if (!ok)
    {
      g_set_error (error, G_IO_ERROR, g_io_error_from_errno (errsv),
                   _("Error writing to file “%s”: %s"), file, g_strerror (errsv));
      g_remove (temp);
    }

  g_free (temp);

  return ok;
}
