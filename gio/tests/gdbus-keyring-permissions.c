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
 *
 * Tests of how the DBUS_COOKIE_SHA1 mechanism treats the permissions of the
 * directory of its keyring, which G_DBUS_COOKIE_SHA1_KEYRING_DIR gives.
 *
 * The mechanism is driven directly, as a server and as a client, without a
 * bus.  The tests under win32/ test the private files and the directories of
 * gwin32security.c and the keyrings in the directories that only exist on
 * Windows.  They are skipped on other platforms.
 */

#include <gio/gio.h>
#include <glib/gstdio.h>

#include <string.h>

#ifndef GIO_COMPILATION
#define GIO_COMPILATION 1
#endif
#include "../gdbusauthmechanism.h"

#ifdef G_OS_WIN32
#include <windows.h>
#include <sddl.h>
#include <aclapi.h>
#include <fcntl.h>
#include <io.h>

/* The functions that are tested are not exported, so the test is built with
 * its own copy of gwin32security.c and gwin32sid.c
 */
#include "../gwin32security.h"
#include "../gwin32sid.h"
#endif

#define KEYRING_DIR_VARIABLE "G_DBUS_COOKIE_SHA1_KEYRING_DIR"
#define KEYRING_IGNORE_VARIABLE "G_DBUS_COOKIE_SHA1_KEYRING_DIR_IGNORE_PERMISSION"

static const gchar *const variables[] = { KEYRING_DIR_VARIABLE, KEYRING_IGNORE_VARIABLE };

typedef struct
{
  gchar *root;
  gchar *saved[G_N_ELEMENTS (variables)];
} Fixture;

static void
remove_all (const gchar *path)
{
  GDir *dir;
  const gchar *name;

  if (g_rmdir (path) == 0 || g_unlink (path) == 0)
    return;

  dir = g_dir_open (path, 0, NULL);
  if (dir != NULL)
    {
      while ((name = g_dir_read_name (dir)) != NULL)
        {
          gchar *child = g_build_filename (path, name, NULL);

          remove_all (child);
          g_free (child);
        }
      g_dir_close (dir);
    }
  g_rmdir (path);
}

#ifdef G_OS_WIN32
static void make_root_private (const gchar *root);
#endif

static void
set_up (Fixture       *fixture,
        gconstpointer  user_data)
{
  GError *error = NULL;
  gsize i;

  fixture->root = g_dir_make_tmp ("gdbus-keyring-XXXXXX", &error);
  g_assert_no_error (error);
#ifdef G_OS_WIN32
  make_root_private (fixture->root);
#endif

  /* Nothing leaks into the next test */
  for (i = 0; i < G_N_ELEMENTS (variables); i++)
    {
      fixture->saved[i] = g_strdup (g_getenv (variables[i]));
      g_unsetenv (variables[i]);
    }
}

static void
tear_down (Fixture       *fixture,
           gconstpointer  user_data)
{
  gsize i;

  for (i = 0; i < G_N_ELEMENTS (variables); i++)
    {
      if (fixture->saved[i] != NULL)
        g_setenv (variables[i], fixture->saved[i], TRUE);
      else
        g_unsetenv (variables[i]);
      g_free (fixture->saved[i]);
    }

  remove_all (fixture->root);
  g_free (fixture->root);
}

#ifdef G_OS_WIN32

static gunichar2 *
wide (const gchar *string)
{
  gunichar2 *wstring = g_utf8_to_utf16 (string, -1, NULL, NULL, NULL);

  g_assert_nonnull (wstring);

  return wstring;
}

static gchar *
current_sid (void)
{
  gchar *sid = _g_win32_current_process_sid_string (NULL);

  g_assert_nonnull (sid);

  return sid;
}

/* @sddl with the SID of the user in place of "$U" */
static gchar *
with_user (const gchar *sddl)
{
  gchar *sid = current_sid ();
  gchar **parts = g_strsplit (sddl, "$U", -1);
  gchar *result = g_strjoinv (sid, parts);

  g_free (sid);
  g_strfreev (parts);

  return result;
}

/* The descriptor that @sddl stands for, with its DACL.  Free with LocalFree(). */
static PSECURITY_DESCRIPTOR
parse_sddl (const gchar *sddl,
            PACL        *dacl)
{
  gunichar2 *wsddl = wide (sddl);
  PSECURITY_DESCRIPTOR descriptor = NULL;
  BOOL present = FALSE, defaulted = FALSE;
  PACL local_dacl = NULL;

  g_assert_true (ConvertStringSecurityDescriptorToSecurityDescriptorW (wsddl, SDDL_REVISION_1,
                                                                       &descriptor, NULL));
  g_assert_true (GetSecurityDescriptorDacl (descriptor, &present, &local_dacl, &defaulted));
  g_free (wsddl);

  if (dacl != NULL)
    *dacl = local_dacl;

  return descriptor;
}

/* Sets the DACL of @path to @dacl (with "$U" for the user), protected or not.
 * When it is not protected, the entries that the parent passes on are added.
 */
static void
set_dacl (const gchar *path,
          const gchar *dacl,
          gboolean     protect)
{
  gunichar2 *wpath = wide (path);
  gchar *sddl = with_user (dacl);
  PACL acl = NULL;
  PSECURITY_DESCRIPTOR descriptor = parse_sddl (sddl, &acl);

  g_assert_cmpuint (SetNamedSecurityInfoW (wpath, SE_FILE_OBJECT,
                                           DACL_SECURITY_INFORMATION |
                                           (protect ? PROTECTED_DACL_SECURITY_INFORMATION
                                                    : UNPROTECTED_DACL_SECURITY_INFORMATION),
                                           NULL, NULL, acl, NULL), ==, ERROR_SUCCESS);
  LocalFree (descriptor);
  g_free (sddl);
  g_free (wpath);
}

/* Stops @root from inheriting rights from its parent, so that what the tests
 * make in it inherits the same rights wherever the temporary directory is
 * (the parent can give write access to Authenticated Users, as C:\ does).
 */
static void
make_root_private (const gchar *root)
{
  set_dacl (root, "D:P(A;OICI;FA;;;$U)(A;OICI;FA;;;SY)(A;OICI;FA;;;BA)", TRUE);
}

/* A directory (@directory) or file with the DACL @dacl, with "$U" for the
 * user.  A file without @dacl has the rights that it inherits.  The owner of
 * a directory is @owner (a SID string), or the user.  The owner is named: an
 * administrator would own it as a group otherwise.
 */
static void
make_object (const gchar *path,
             const gchar *owner,
             const gchar *dacl,
             gboolean     directory)
{
  gunichar2 *wpath = wide (path);
  gchar *user = current_sid ();
  SECURITY_ATTRIBUTES attributes = { sizeof (attributes), NULL, FALSE };
  PSECURITY_DESCRIPTOR descriptor = NULL;

  if (dacl != NULL)
    {
      gchar *expanded = with_user (dacl);
      gchar *sddl = g_strconcat ("O:", owner != NULL ? owner : user, expanded, NULL);

      descriptor = parse_sddl (sddl, NULL);
      attributes.lpSecurityDescriptor = descriptor;
      g_free (expanded);
      g_free (sddl);
    }

  if (directory)
    {
      g_assert_true (CreateDirectoryW (wpath, &attributes));
    }
  else
    {
      HANDLE handle = CreateFileW (wpath, GENERIC_WRITE, 0, &attributes, CREATE_NEW,
                                   FILE_ATTRIBUTE_NORMAL, NULL);

      g_assert_true (handle != INVALID_HANDLE_VALUE);
      CloseHandle (handle);
    }

  LocalFree (descriptor);
  g_free (user);
  g_free (wpath);
}

static void
make_dir (const gchar *path,
          const gchar *dacl)
{
  make_object (path, NULL, dacl, TRUE);
}

static void
make_file (const gchar *path,
           const gchar *dacl)
{
  make_object (path, NULL, dacl, FALSE);
}

/* A directory that is not private: it inherits from its parent, and it also
 * has an entry for everyone and one for the user, which makes the rights the
 * same wherever the test runs.  It is owned by @owner, a SID as a string, or
 * by the user if that is NULL.
 */
static void
make_unprotected_directory (const gchar *path,
                            const gchar *owner)
{
  /* Made with the entries, and then unprotected: the rights of a new object
   * are protected when it is made with some, whatever its flags say
   */
  make_object (path, owner, "D:P(A;OICI;FA;;;WD)", TRUE);
  set_dacl (path, "D:(A;OICI;FA;;;WD)(A;OICI;FA;;;$U)", FALSE);
}

/* A directory as libdbus or an older GLib makes it in the profile of the user:
 * it has the rights of its parent, with entries for the user, the system and
 * the administrators.  The rights do not depend on where the test runs.
 */
static void
make_profile_directory (const gchar *path,
                        const gchar *owner)
{
  make_object (path, owner, "D:P(A;;FA;;;$U)", TRUE);
  set_dacl (path, "D:(A;OICI;FA;;;SY)(A;OICI;FA;;;BA)(A;OICI;FA;;;$U)", FALSE);
}

/* The default owner of the token of the process, as a string */
static gchar *
get_token_owner_string (void)
{
  HANDLE token;
  TOKEN_OWNER *owner;
  DWORD size = 0;
  gchar *string;

  g_assert_true (OpenProcessToken (GetCurrentProcess (), TOKEN_QUERY, &token));
  GetTokenInformation (token, TokenOwner, NULL, 0, &size);
  owner = g_malloc (size);
  g_assert_true (GetTokenInformation (token, TokenOwner, owner, size, &size));
  string = _g_win32_sid_to_string (owner->Owner, NULL);
  g_assert_nonnull (string);
  g_free (owner);
  CloseHandle (token);

  return string;
}

static gboolean
make_junction (const gchar *link,
               const gchar *target)
{
  const gchar *argv[] = { "cmd.exe", "/c", "mklink", "/J", link, target, NULL };

  g_spawn_sync (NULL, (gchar **) argv, NULL,
                G_SPAWN_SEARCH_PATH | G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL,
                NULL, NULL, NULL, NULL, NULL, NULL);

  return g_file_test (link, G_FILE_TEST_IS_DIR);
}

static void
make_plain_file (const gchar *path)
{
  g_assert_true (g_file_set_contents (path, "x", -1, NULL));
}

/* The DACL of @path, in SDDL */
static gchar *
get_dacl_sddl (const gchar *path)
{
  gunichar2 *wpath = wide (path);
  PSECURITY_DESCRIPTOR descriptor = NULL;
  gunichar2 *wsddl = NULL;
  gchar *sddl;

  g_assert_cmpuint (GetNamedSecurityInfoW (wpath, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION,
                                           NULL, NULL, NULL, NULL, &descriptor), ==, ERROR_SUCCESS);
  g_assert_true (ConvertSecurityDescriptorToStringSecurityDescriptorW (descriptor, SDDL_REVISION_1,
                                                                       DACL_SECURITY_INFORMATION,
                                                                       &wsddl, NULL));
  sddl = g_utf16_to_utf8 (wsddl, -1, NULL, NULL, NULL);

  LocalFree (wsddl);
  LocalFree (descriptor);
  g_free (wpath);

  return sddl;
}

static void
assert_refused (const gchar *dir,
                const gchar *message_part)
{
  GError *error = NULL;

  g_assert_false (g_win32_ensure_private_directory (dir, &error));
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED);
  if (message_part != NULL)
    g_assert_nonnull (strstr (error->message, message_part));
  g_error_free (error);
}

static void
assert_private (const gchar *dir)
{
  GError *error = NULL;

  g_assert_true (g_win32_ensure_private_directory (dir, &error));
  g_assert_no_error (error);
}

static void
assert_file_refused (const gchar *file)
{
  GError *error = NULL;
  gchar *contents = NULL;

  g_assert_false (g_win32_read_private_file (file, &contents, &error));
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED);
  g_assert_null (contents);
  g_error_free (error);
}

/* The DACL that everything made is given: protected, so nothing is inherited,
 * and for a directory its entries are inheritable
 */
static void
assert_made_dacl (const gchar *path,
                  gboolean     directory)
{
  gchar *user = current_sid ();
  gchar *expected = g_strdup_printf ("D:P(A;%s;FA;;;%s)(A;%s;FA;;;SY)",
                                     directory ? "OICI" : "", user, directory ? "OICI" : "");
  gchar *sddl = get_dacl_sddl (path);

  g_assert_cmpstr (sddl, ==, expected);

  g_free (sddl);
  g_free (expected);
  g_free (user);
}

static void
assert_file_private (const gchar *file)
{
  GError *error = NULL;
  gchar *contents = NULL;

  g_assert_true (g_win32_read_private_file (file, &contents, &error));
  g_assert_no_error (error);
  g_free (contents);
}

/* Every file that is in @dir is private, and there is at least one */
static void
assert_files_private (const gchar *dir)
{
  GDir *directory = g_dir_open (dir, 0, NULL);
  const gchar *name;
  guint n_files = 0;

  g_assert_nonnull (directory);
  while ((name = g_dir_read_name (directory)) != NULL)
    {
      gchar *file = g_build_filename (dir, name, NULL);

      assert_file_private (file);
      g_free (file);
      n_files++;
    }
  g_dir_close (directory);
  g_assert_cmpuint (n_files, >, 0);
}

/* The number of entries in @dir */
static guint
count_files (const gchar *dir)
{
  GDir *directory = g_dir_open (dir, 0, NULL);
  guint n_files = 0;

  g_assert_nonnull (directory);
  while (g_dir_read_name (directory) != NULL)
    n_files++;
  g_dir_close (directory);

  return n_files;
}

static void
set_contents (const gchar *file,
              const gchar *contents)
{
  GError *error = NULL;

  g_assert_true (g_win32_set_contents_private (file, contents, strlen (contents), &error));
  g_assert_no_error (error);
}

/* @file is usable and has the contents @expected */
static void
assert_contents (const gchar *file,
                 const gchar *expected)
{
  GError *error = NULL;
  gchar *contents = NULL;

  g_assert_true (g_win32_read_private_file (file, &contents, &error));
  g_assert_no_error (error);
  g_assert_cmpstr (contents, ==, expected);
  g_free (contents);
}

#endif  /* G_OS_WIN32 */

/* A directory that is open to everyone: the mode 0777 on Unix, and a DACL
 * that gives full access to Everyone on Windows.
 */
static void
make_open_directory (const gchar *path)
{
#ifdef G_OS_WIN32
  make_dir (path, "D:P(A;OICI;FA;;;WD)");
#else
  g_assert_cmpint (g_mkdir (path, 0700), ==, 0);
  /* chmod and not the mode of mkdir, which the umask changes */
  g_assert_cmpint (g_chmod (path, 0777), ==, 0);
#endif
}

/* The types of the mechanisms are registered when a connection is made */

static gboolean
server_connection_cb (GDBusServer     *server,
                      GDBusConnection *connection,
                      gpointer         user_data)
{
  GDBusConnection **accepted = user_data;

  *accepted = g_object_ref (connection);

  return TRUE;
}

static void
async_result_cb (GObject      *source_object,
                 GAsyncResult *res,
                 gpointer      user_data)
{
  GAsyncResult **result = user_data;

  g_assert_null (*result);
  *result = g_object_ref (res);
  g_main_context_wakeup (g_main_context_get_thread_default ());
}

static GType
get_sha1_mechanism_type (void)
{
  GDBusServer *server;
  GDBusConnection *accepted = NULL, *connected;
  GAsyncResult *connect_result = NULL;
  gchar *guid = g_dbus_generate_guid ();
  GError *error = NULL;
  GType type;

  if (g_type_from_name ("GDBusAuthMechanismSha1") == 0)
    {
      server = g_dbus_server_new_sync ("tcp:host=127.0.0.1", G_DBUS_SERVER_FLAGS_NONE,
                                       guid, NULL, NULL, &error);
      g_assert_no_error (error);
      g_signal_connect (server, "new-connection", G_CALLBACK (server_connection_cb), &accepted);
      g_dbus_server_start (server);

      g_dbus_connection_new_for_address (g_dbus_server_get_client_address (server),
                                         G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT,
                                         NULL, NULL, async_result_cb, &connect_result);

      while (accepted == NULL || connect_result == NULL)
        g_main_context_iteration (NULL, TRUE);

      connected = g_dbus_connection_new_for_address_finish (connect_result, &error);
      g_assert_no_error (error);
      g_clear_object (&connect_result);

      g_dbus_connection_close_sync (connected, NULL, NULL);
      g_dbus_connection_close_sync (accepted, NULL, NULL);
      g_object_unref (connected);
      g_object_unref (accepted);
      g_dbus_server_stop (server);
      g_object_unref (server);
    }

  type = g_type_from_name ("GDBusAuthMechanismSha1");
  g_free (guid);

  return type;
}

/* Called once the server made its challenge, and before the client looks the
 * cookie up in the keyring in @keyring_dir
 */
static void (*after_challenge) (const gchar *keyring_dir);

/* Whether the client did not use the keyring in the last run */
static gboolean client_refused;

/* Runs the mechanism with the keyring in @keyring_dir: the server asks for a
 * challenge, and the client answers it if the server has one.  Returns
 * whether the server accepted the client.  If it did not, @reject_reason is
 * the reason that the server gave.
 */
static gboolean
authenticate (const gchar  *keyring_dir,
              gchar       **reject_reason)
{
  GType type = get_sha1_mechanism_type ();
  GDBusAuthMechanism *server, *client;
  GDBusAuthMechanismClass *server_class, *client_class;
  gboolean accepted = FALSE;
  gchar *response = NULL, *challenge = NULL;
  gsize response_length = 0, challenge_length = 0, answer_length = 0;

  g_assert_cmpuint (type, !=, 0);

  g_setenv (KEYRING_DIR_VARIABLE, keyring_dir, TRUE);
  *reject_reason = NULL;
  client_refused = FALSE;

  server = g_object_new (type, NULL);
  client = g_object_new (type, NULL);
  server_class = G_DBUS_AUTH_MECHANISM_GET_CLASS (server);
  client_class = G_DBUS_AUTH_MECHANISM_GET_CLASS (client);

  g_assert_true (client_class->is_supported (client));
  response = client_class->client_initiate (client, G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT,
                                            &response_length);
  server_class->server_initiate (server, response, response_length);
  g_assert_cmpint (server_class->server_get_state (server), ==,
                   G_DBUS_AUTH_MECHANISM_STATE_HAVE_DATA_TO_SEND);

  /* The server adds an entry to the keyring for the challenge */
  challenge = server_class->server_data_send (server, &challenge_length);
  if (server_class->server_get_state (server) == G_DBUS_AUTH_MECHANISM_STATE_WAITING_FOR_DATA)
    {
      gchar *answer = NULL;

      g_assert_nonnull (challenge);

      if (after_challenge != NULL)
        after_challenge (keyring_dir);

      /* and the client looks the entry up in it */
      client_class->client_data_receive (client, challenge, challenge_length);
      if (client_class->client_get_state (client) == G_DBUS_AUTH_MECHANISM_STATE_REJECTED)
        {
          client_refused = TRUE;
        }
      else
        {
          g_assert_cmpint (client_class->client_get_state (client), ==,
                           G_DBUS_AUTH_MECHANISM_STATE_HAVE_DATA_TO_SEND);
          answer = client_class->client_data_send (client, &answer_length);
          g_assert_nonnull (answer);

          server_class->server_data_receive (server, answer, answer_length);
          accepted = server_class->server_get_state (server) == G_DBUS_AUTH_MECHANISM_STATE_ACCEPTED;
          if (!accepted)
            *reject_reason = server_class->server_get_reject_reason (server);
          g_free (answer);
        }
    }
  else
    {
      g_assert_cmpint (server_class->server_get_state (server), ==, G_DBUS_AUTH_MECHANISM_STATE_REJECTED);
      *reject_reason = server_class->server_get_reject_reason (server);
    }

  server_class->server_shutdown (server);
  client_class->client_shutdown (client);
  g_object_unref (server);
  g_object_unref (client);
  g_unsetenv (KEYRING_DIR_VARIABLE);
  g_free (response);
  g_free (challenge);

  return accepted;
}

static void
assert_keyring_works (const gchar *dir)
{
  gchar *reason = NULL;

  g_assert_true (authenticate (dir, &reason));
  g_assert_null (reason);
}

#ifdef G_OS_WIN32

/* The keyring in @dir is not used, and @message_part is in the reason */
static void
assert_keyring_refused (const gchar *dir,
                        const gchar *message_part)
{
  gchar *reason = NULL;

  g_assert_false (authenticate (dir, &reason));
  g_assert_nonnull (reason);
  g_assert_nonnull (strstr (reason, "keyring"));
  g_assert_nonnull (strstr (reason, message_part));
  g_free (reason);
}

#endif  /* G_OS_WIN32 */

/* A directory that GLib makes is private, and works */
static void
test_new (Fixture       *fixture,
          gconstpointer  user_data)
{
  gchar *dir = g_build_filename (fixture->root, "parent", "keyrings", NULL);

  assert_keyring_works (dir);
  g_assert_true (g_file_test (dir, G_FILE_TEST_IS_DIR));

#ifdef G_OS_UNIX
  {
    GStatBuf statbuf;

    g_assert_cmpint (g_stat (dir, &statbuf), ==, 0);
    g_assert_cmpuint (statbuf.st_mode & 0777, ==, 0700);
  }
#endif

  /* and the directory that was made is accepted the next time */
  assert_keyring_works (dir);
  g_free (dir);
}

/* A directory that is open to everyone is not used, and it is not changed */
static void
test_open_directory (Fixture       *fixture,
                     gconstpointer  user_data)
{
  gchar *dir = g_build_filename (fixture->root, "keyrings", NULL);
  gchar *reason = NULL;
#ifdef G_OS_WIN32
  gchar *before;
#endif

  make_open_directory (dir);
#ifdef G_OS_WIN32
  before = get_dacl_sddl (dir);
#endif

  g_assert_false (authenticate (dir, &reason));
  g_assert_nonnull (reason);
  g_assert_nonnull (strstr (reason, "keyring"));
#ifdef G_OS_UNIX
  g_assert_nonnull (strstr (reason, "Permissions on directory"));
  g_assert_nonnull (strstr (reason, "0777"));
#else
  g_assert_nonnull (strstr (reason, "Cannot use the directory"));
  g_assert_nonnull (strstr (reason, "give write access to"));

  {
    gchar *after = get_dacl_sddl (dir);

    g_assert_cmpstr (after, ==, before);
    g_free (after);
  }

  g_free (before);
#endif

  g_free (reason);
  g_free (dir);
}

/* G_DBUS_COOKIE_SHA1_KEYRING_DIR_IGNORE_PERMISSION allows the directory */
static void
test_ignore_permission (Fixture       *fixture,
                        gconstpointer  user_data)
{
  gchar *dir = g_build_filename (fixture->root, "keyrings", NULL);

  make_open_directory (dir);

  g_setenv (KEYRING_IGNORE_VARIABLE, "1", TRUE);
  assert_keyring_works (dir);
  g_unsetenv (KEYRING_IGNORE_VARIABLE);

#ifdef G_OS_WIN32
  /* The cookie file is written as it was before, so it inherits the open
   * rights of the directory instead of getting a private DACL
   */
  {
    GDir *directory = g_dir_open (dir, 0, NULL);
    const gchar *name;
    guint n_files = 0;

    g_assert_nonnull (directory);
    while ((name = g_dir_read_name (directory)) != NULL)
      {
        gchar *file = g_build_filename (dir, name, NULL);

        if (!g_str_has_suffix (name, ".lock"))
          {
            assert_file_refused (file);
            n_files++;
          }
        g_free (file);
      }
    g_dir_close (directory);
    g_assert_cmpuint (n_files, >, 0);
  }
#endif

  /* The directory is used as it is */
#ifdef G_OS_UNIX
  {
    GStatBuf statbuf;

    g_assert_cmpint (g_stat (dir, &statbuf), ==, 0);
    g_assert_cmpuint (statbuf.st_mode & 0777, ==, 0777);
  }
#else
  assert_refused (dir, "give write access to");
#endif

  g_free (dir);
}

#ifdef G_OS_WIN32

/* A directory that others can write to is refused, also when it is asked for */
static void
test_check_refused (Fixture       *fixture,
                    gconstpointer  user_data)
{
  const struct
    {
      const gchar *name;
      const gchar *dacl;
      const gchar *message;
    }
  cases[] =
    {
      { "everyone", "D:P(A;OICI;FA;;;WD)", "give write access to" },
      { "users-write", "D:P(A;OICI;FA;;;$U)(A;OICI;FA;;;SY)(A;OICI;FW;;;AU)", "give write access to" },
      /* Each of the rights that change a directory or what is in it */
      { "add-file", "D:P(A;;FA;;;$U)(A;;0x2;;;AU)", "give write access to" },
      { "add-directory", "D:P(A;;FA;;;$U)(A;;0x4;;;AU)", "give write access to" },
      { "delete-child", "D:P(A;;FA;;;$U)(A;;0x40;;;AU)", "give write access to" },
      { "delete", "D:P(A;;FA;;;$U)(A;;0x10000;;;AU)", "give write access to" },
      { "write-dac", "D:P(A;;FA;;;$U)(A;;0x40000;;;AU)", "give write access to" },
      { "write-owner", "D:P(A;;FA;;;$U)(A;;0x80000;;;AU)", "give write access to" },
      { "generic-write", "D:P(A;;FA;;;$U)(A;;GW;;;AU)", "give write access to" },
      { "generic-all", "D:P(A;;FA;;;$U)(A;;GA;;;AU)", "give write access to" },
      /* The owner of what is made in the directory is not the user for others */
      { "creator-owner", "D:P(A;;FA;;;$U)(A;;FA;;;CO)", "give write access to" },
      /* An entry of a type that is not understood is not trusted */
      { "callback", "D:P(A;;FA;;;$U)(XA;;FA;;;WD;(Member_of {SID(BA)}))", "is not supported" },
    };
  gchar *dir = NULL;
  GError *error = NULL;
  gunichar2 *wdir = NULL;
  gsize i;

  for (i = 0; i < G_N_ELEMENTS (cases); i++)
    {
      gchar *case_dir = g_build_filename (fixture->root, cases[i].name, NULL);

      make_dir (case_dir, cases[i].dacl);

      assert_refused (case_dir, cases[i].message);
      g_assert_false (g_win32_ensure_private_directory (case_dir, &error));
      g_assert_error (error, G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED);
      g_clear_error (&error);
      g_free (case_dir);
    }

  /* No DACL, which is access for everybody */
  dir = g_build_filename (fixture->root, "null", NULL);
  g_assert_cmpint (g_mkdir (dir, 0700), ==, 0);
  wdir = wide (dir);
  g_assert_cmpuint (SetNamedSecurityInfoW (wdir, SE_FILE_OBJECT,
                                           DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
                                           NULL, NULL, NULL, NULL), ==, ERROR_SUCCESS);
  assert_refused (dir, "open to everyone");
  g_free (dir);

  /* A file */
  dir = g_build_filename (fixture->root, "file", NULL);
  make_plain_file (dir);
  assert_refused (dir, "not a plain directory");
  g_free (dir);
  g_free (wdir);
}

/* A directory that is not open to others is used as it is, and not changed:
 * the one that the profile of the user gives (which has the entries of the
 * system and of the administrators), one that others may read, one that is
 * owned by the default owner of the token (the administrators, if elevated),
 * and entries that do not apply, or that deny
 */
static void
test_check_accepted (Fixture       *fixture,
                     gconstpointer  user_data)
{
  const struct
    {
      const gchar *name;
      const gchar *dacl;  /* NULL for the rights of a profile */
      gboolean     token_owner;
    }
  cases[] =
    {
      { "profile", NULL, FALSE },
      { "token-owner", NULL, TRUE },
      { "private", "D:P(A;OICI;FA;;;$U)(A;OICI;FA;;;SY)", FALSE },
      { "user-only", "D:P(A;;FA;;;$U)", FALSE },
      { "others-read", "D:P(A;OICI;FA;;;$U)(A;OICI;FA;;;SY)(A;OICI;FR;;;AU)", FALSE },
      { "administrators", "D:P(A;OICI;FA;;;$U)(A;OICI;FA;;;SY)(A;OICI;FA;;;BA)", FALSE },
      /* Only for what is made in the directory, and not for the directory */
      { "inherit-only", "D:P(A;OICI;FA;;;$U)(A;OICIIO;FA;;;AU)", FALSE },
      /* Entries that take rights away or audit give nothing */
      { "denied", "D:P(D;OICI;FW;;;AN)(A;OICI;FA;;;$U)(A;OICI;FA;;;SY)", FALSE },
      { "creator-owner-inherit-only", "D:P(A;OICI;FA;;;$U)(A;OICIIO;FA;;;CO)", FALSE },
    };
  gchar *owner = get_token_owner_string ();
  gchar *user = current_sid ();
  gsize i;

  for (i = 0; i < G_N_ELEMENTS (cases); i++)
    {
      gchar *dir = g_build_filename (fixture->root, cases[i].name, NULL);
      gchar *before, *after;

      /* The owner is the user, unless the token has another default one */
      if (cases[i].token_owner && strcmp (user, owner) == 0)
        {
          g_free (dir);
          continue;
        }

      if (cases[i].dacl != NULL)
        make_dir (dir, cases[i].dacl);
      else
        make_profile_directory (dir, cases[i].token_owner ? owner : NULL);

      before = get_dacl_sddl (dir);
      assert_private (dir);
      assert_keyring_works (dir);

      after = get_dacl_sddl (dir);
      g_assert_cmpstr (after, ==, before);

      g_free (dir);
      g_free (before);
      g_free (after);
    }

  g_free (owner);
  g_free (user);
}

/* The directory of somebody else (the Windows directory, if it is not owned by
 * the user or the default owner of the token) is not used, and not changed
 */
static void
test_foreign_directory (Fixture       *fixture,
                        gconstpointer  user_data)
{
  const gchar *foreign = g_getenv ("SystemRoot");

  if (foreign == NULL || g_win32_ensure_private_directory (foreign, NULL))
    {
      g_test_skip ("The Windows directory belongs to the user or to the default owner");
      return;
    }

  assert_refused (foreign, "is owned by");
  assert_keyring_refused (foreign, "is owned by");
}

/* A link to a directory is not used, also when it leads to a good one */
static void
test_junction (Fixture       *fixture,
               gconstpointer  user_data)
{
  gchar *good = g_build_filename (fixture->root, "good", NULL);
  gchar *link = g_build_filename (fixture->root, "link", NULL);
  GError *error = NULL;

  g_assert_true (g_win32_ensure_private_directory (good, &error));
  g_assert_no_error (error);

  if (!make_junction (link, good))
    {
      g_test_skip ("Cannot make a junction");
    }
  else
    {
      assert_refused (link, "not a plain directory");
      assert_keyring_refused (link, "not a plain directory");

      /* unless the rights are not to be looked at */
      g_setenv (KEYRING_IGNORE_VARIABLE, "1", TRUE);
      assert_keyring_works (link);
      g_unsetenv (KEYRING_IGNORE_VARIABLE);
    }

  g_free (good);
  g_free (link);
}

/* A directory that does not exist is made, with the directories above it, and
 * what the keyring makes in it is private.  Files are made private when they
 * are made, and the contents of a file are replaced by a private file,
 * whatever the rights of the file were.
 */
static void
test_create_private_files (Fixture       *fixture,
                           gconstpointer  user_data)
{
  gchar *new_dir = g_build_filename (fixture->root, "parent", "keyrings", NULL);
  gchar *dir = g_build_filename (fixture->root, "files", NULL);
  gchar *file = g_build_filename (dir, "file", NULL);
  gchar *inherited, *sddl, *user, *expected;
  GError *error = NULL;

  g_assert_true (g_win32_ensure_private_directory (new_dir, &error));
  g_assert_no_error (error);
  /* What is made is accepted, also when it is asked for again */
  g_assert_true (g_win32_ensure_private_directory (new_dir, &error));
  g_assert_no_error (error);
  assert_made_dacl (new_dir, TRUE);

  assert_keyring_works (new_dir);
  assert_files_private (new_dir);

  /* A file made with the default security, as libdbus does, inherits the
   * entries of the directory: only the user and SYSTEM, and it is accepted
   */
  inherited = g_build_filename (new_dir, "inherited", NULL);
  make_file (inherited, NULL);
  assert_file_private (inherited);
  sddl = get_dacl_sddl (inherited);
  user = current_sid ();
  expected = g_strdup_printf ("D:(A;ID;FA;;;%s)(A;ID;FA;;;SY)", user);
  g_assert_cmpstr (sddl, ==, expected);
  assert_keyring_works (new_dir);
  g_free (expected);
  g_free (sddl);
  g_free (user);
  g_free (inherited);

  make_unprotected_directory (dir, NULL);

  /* the file that is made has the private DACL */
  set_contents (file, "first");
  assert_file_private (file);
  assert_made_dacl (file, FALSE);

  /* and the contents replace the ones of a file with other rights */
  g_assert_cmpint (g_remove (file), ==, 0);
  make_file (file, "D:(A;;FR;;;WD)(A;;FA;;;$U)");
  assert_file_refused (file);

  set_contents (file, "cookie");
  assert_contents (file, "cookie");
  assert_made_dacl (file, FALSE);

  /* A file with the same entries that inherits them is not usable here, as
   * the directory passes on an entry for everyone, which counts like any other,
   * and the contents replace it with a file with the strict DACL
   */
  set_dacl (file, "D:(A;;FA;;;$U)(A;;FA;;;SY)", FALSE);
  assert_file_refused (file);
  set_contents (file, "cookie");
  assert_contents (file, "cookie");
  assert_made_dacl (file, FALSE);

  /* no temporary file is left */
  g_assert_cmpuint (count_files (dir), ==, 1);

  g_free (new_dir);
  g_free (file);
  g_free (dir);
}

/* A file that is read must be usable (nobody besides the user, SYSTEM and the
 * administrators can read or change it), whether it inherits its rights or not
 * and whatever else its DACL has, and the contents are the ones of the file
 * that was checked
 */
static void
test_read_private_file (Fixture       *fixture,
                        gconstpointer  user_data)
{
  const struct
    {
      const gchar *dacl;  /* NULL for the one of a private file */
      gboolean     protected;
      gboolean     accepted;
    }
  cases[] =
    {
      { NULL, TRUE, TRUE },
      /* Other entries for the user, SYSTEM and the administrators, which
       * need not be the ones of a private file, or protected
       */
      { "D:P(A;;FA;;;$U)", TRUE, TRUE },
      { "D:P(A;;FR;;;$U)(A;;FA;;;SY)", TRUE, TRUE },
      { "D:(A;;FA;;;$U)(A;;FA;;;SY)", FALSE, TRUE },
      { "D:P(A;;FA;;;$U)(A;;FA;;;SY)(A;;FA;;;BA)", TRUE, TRUE },
      /* An entry that denies is not trusted either, and it takes nothing away */
      { "D:P(D;;FA;;;AN)(A;;FA;;;$U)(A;;FA;;;SY)", TRUE, TRUE },
      { "D:P(D;;FW;;;AN)(A;;FA;;;$U)(A;;FA;;;SY)", TRUE, TRUE },
      /* Entries of others that give none of the rights that matter */
      { "D:P(A;;FA;;;$U)(A;;FX;;;WD)", TRUE, TRUE },
      { "D:P(A;;FA;;;$U)(A;;0x80;;;AU)", TRUE, TRUE },
      { "D:P(A;;FA;;;$U)(A;;0x20000;;;AU)", TRUE, TRUE },
      /* and entries that are only for what is made in a directory */
      { "D:P(A;OICIIO;FA;;;WD)(A;;FA;;;$U)", TRUE, TRUE },
      /* Each of the rights that read or change a file, for others */
      { "D:P(A;;FA;;;$U)(A;;FR;;;AU)", TRUE, FALSE },
      { "D:P(A;;FA;;;$U)(A;;FR;;;WD)", TRUE, FALSE },
      { "D:P(A;;FA;;;$U)(A;;0x1;;;AU)", TRUE, FALSE },
      { "D:P(A;;FA;;;$U)(A;;0x2;;;AU)", TRUE, FALSE },
      { "D:P(A;;FA;;;$U)(A;;0x4;;;AU)", TRUE, FALSE },
      { "D:P(A;;FA;;;$U)(A;;0x10000;;;AU)", TRUE, FALSE },
      { "D:P(A;;FA;;;$U)(A;;0x40000;;;AU)", TRUE, FALSE },
      { "D:P(A;;FA;;;$U)(A;;0x80000;;;AU)", TRUE, FALSE },
      { "D:P(A;;FA;;;$U)(A;;FW;;;AU)", TRUE, FALSE },
      { "D:(A;;FA;;;$U)(A;;FW;;;WD)", FALSE, FALSE },
      /* Generic rights are mapped to the ones of a file */
      { "D:P(A;;FA;;;$U)(A;;GR;;;AU)", TRUE, FALSE },
      { "D:P(A;;FA;;;$U)(A;;GW;;;AU)", TRUE, FALSE },
      { "D:P(A;;FA;;;$U)(A;;GA;;;AU)", TRUE, FALSE },
      { "D:P(A;;FA;;;$U)(A;;GX;;;AU)", TRUE, TRUE },
      /* An entry that denies does not make an entry that allows less of one */
      { "D:P(D;;FR;;;WD)(A;;FA;;;$U)(A;;FR;;;WD)", TRUE, FALSE },
      /* An entry of a type that is not understood is not trusted */
      { "D:P(A;;FA;;;$U)(XA;;FA;;;WD;(Member_of {SID(BA)}))", TRUE, FALSE },
    };
  gchar *dir = g_build_filename (fixture->root, "read", NULL);
  gchar *file = g_build_filename (dir, "file", NULL);
  gchar *contents = NULL;
  gsize i;
  GError *error = NULL;

  g_assert_true (g_win32_ensure_private_directory (dir, &error));
  g_assert_no_error (error);

  /* Not there */
  g_assert_false (g_win32_read_private_file (file, &contents, &error));
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND);
  g_clear_error (&error);

  for (i = 0; i < G_N_ELEMENTS (cases); i++)
    {
      g_test_message ("DACL %s", cases[i].dacl != NULL ? cases[i].dacl : "(private)");

      set_contents (file, "cookie");

      if (cases[i].dacl != NULL)
        set_dacl (file, cases[i].dacl, cases[i].protected);

      if (cases[i].accepted)
        {
          assert_file_private (file);
          g_assert_true (g_win32_read_private_file (file, &contents, &error));
          g_assert_no_error (error);
          g_assert_cmpstr (contents, ==, "cookie");
          g_clear_pointer (&contents, g_free);
        }
      else
        {
          g_assert_false (g_win32_read_private_file (file, &contents, &error));
          g_assert_error (error, G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED);
          g_clear_error (&error);
          g_assert_null (contents);
          assert_file_refused (file);
        }

      g_assert_cmpint (g_remove (file), ==, 0);
    }

  g_free (file);
  g_free (dir);

  /* The entry for the owner of what is made in a directory (CREATOR OWNER) is
   * one for another user, not for the owner of the file.  It is only kept in
   * the DACL of a file when the file is made with it.
   */
  dir = g_build_filename (fixture->root, "creator-owner", NULL);
  file = g_build_filename (dir, "file", NULL);
  make_profile_directory (dir, NULL);
  make_file (file, "D:P(A;;FA;;;$U)(A;;FA;;;CO)");
  {
    gchar *sddl = get_dacl_sddl (file);

    g_test_message ("DACL %s", sddl);
    if (strstr (sddl, ";CO)") != NULL)
      assert_file_refused (file);
    else
      g_test_message ("The system does not keep the entry of CREATOR OWNER");
    g_free (sddl);
  }
  g_free (file);
  g_free (dir);

  /* A file that inherits entries for others from its directory is refused,
   * and one that inherits entries of the trusted ones only is not
   */
  dir = g_build_filename (fixture->root, "inherit-open", NULL);
  file = g_build_filename (dir, "file", NULL);
  make_unprotected_directory (dir, NULL);
  make_file (file, NULL);
  assert_file_refused (file);
  g_free (file);
  g_free (dir);
}

/* A file that a reader has open is not replaced, and nothing is left from the
 * try: the old contents stay, and the file is replaced once it is closed
 */
static void
test_set_contents_busy (Fixture       *fixture,
                        gconstpointer  user_data)
{
  gchar *dir = g_build_filename (fixture->root, "busy", NULL);
  gchar *file = g_build_filename (dir, "file", NULL);
  gunichar2 *wfile;
  HANDLE reader;
  GError *error = NULL;

  g_assert_true (g_win32_ensure_private_directory (dir, &error));
  g_assert_no_error (error);
  set_contents (file, "old");

  wfile = wide (file);
  reader = CreateFileW (wfile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                        FILE_ATTRIBUTE_NORMAL, NULL);
  g_assert_true (reader != INVALID_HANDLE_VALUE);
  g_assert_false (g_win32_set_contents_private (file, "other", 5, &error));
  g_assert_nonnull (error);
  g_clear_error (&error);
  CloseHandle (reader);

  assert_contents (file, "old");

  /* No temporary file is left */
  g_assert_cmpuint (count_files (dir), ==, 1);

  set_contents (file, "new");
  assert_contents (file, "new");

  g_free (wfile);
  g_free (file);
  g_free (dir);
}

#define COOKIE_FILE "org_gtk_gdbus_general"
#define PLANTED_COOKIE "0123456789abcdef0123456789abcdef"

/* The cookie file in @dir, in place, with a cookie that somebody may know */
static void
plant_cookie (const gchar *file)
{
  gchar *line = g_strdup_printf ("1 %" G_GINT64_FORMAT " " PLANTED_COOKIE "\n",
                                 g_get_real_time () / G_USEC_PER_SEC);
  FILE *f = g_fopen (file, "wb");

  g_assert_nonnull (f);
  g_assert_cmpint (fputs (line, f), >=, 0);
  g_assert_cmpint (fclose (f), ==, 0);
  g_free (line);
}

/* A cookie file that others can read or change is not used, and not changed in
 * place: the server replaces it with a private one that has new cookies (so a
 * cookie that others may have read is not accepted), and the directory is not
 * changed.  A cookie file that the server finds usable although it did not make
 * it is used and not replaced, whether its DACL is inherited, or has an entry
 * for the administrators, or the user can only read it.
 */
static void
test_cookie_files (Fixture       *fixture,
                   gconstpointer  user_data)
{
  const struct
    {
      const gchar *name;
      const gchar *dacl;  /* NULL for the rights of the directory */
      gboolean     protect;
      gboolean     reused;
    }
  cases[] =
    {
      /* Everyone can read it, besides the rights that it inherits */
      { "open", "D:(A;;FR;;;WD)(A;;FA;;;$U)", TRUE, FALSE },
      { "protected", "D:P(A;;FR;;;WD)(A;;FA;;;$U)", TRUE, FALSE },
      { "others-write", "D:P(A;;FA;;;$U)(A;;FA;;;SY)(A;;FW;;;AU)", TRUE, FALSE },
      { "inherited", NULL, FALSE, TRUE },
      { "administrators", "D:P(A;;FA;;;$U)(A;;FA;;;SY)(A;;FA;;;BA)", FALSE, TRUE },
      { "read-only", "D:P(A;;FR;;;$U)(A;;FA;;;SY)", FALSE, TRUE },
      { "denied", "D:P(D;;FA;;;AN)(A;;FA;;;$U)(A;;FA;;;SY)", FALSE, TRUE },
    };
  gsize i;

  for (i = 0; i < G_N_ELEMENTS (cases); i++)
    {
      gchar *dir = g_build_filename (fixture->root, cases[i].name, NULL);
      gchar *file = g_build_filename (dir, COOKIE_FILE, NULL);
      gchar *contents = NULL;
      gchar *before;
      GError *error = NULL;

      make_profile_directory (dir, NULL);
      make_plain_file (file);
      plant_cookie (file);
      if (cases[i].dacl != NULL)
        set_dacl (file, cases[i].dacl, cases[i].protect);
      if (cases[i].reused)
        assert_file_private (file);
      else
        assert_file_refused (file);
      before = get_dacl_sddl (dir);

      assert_keyring_works (dir);
      assert_file_private (file);
      g_assert_true (g_file_get_contents (file, &contents, NULL, &error));
      g_assert_no_error (error);
      g_assert_cmpint (strstr (contents, PLANTED_COOKIE) != NULL, ==, cases[i].reused);

      g_free (contents);
      contents = get_dacl_sddl (dir);
      g_assert_cmpstr (contents, ==, before);

      g_free (contents);
      g_free (before);
      g_free (file);
      g_free (dir);
    }
}

/* Changes the DACL of the cookie file in @dir to the one in @after_challenge_dacl */
static const gchar *after_challenge_dacl;

static void
change_cookie_file (const gchar *dir)
{
  gchar *file = g_build_filename (dir, COOKIE_FILE, NULL);

  set_dacl (file, after_challenge_dacl, TRUE);
  g_free (file);
}

/* The client does not look a cookie up in a file that others can read or
 * change, whatever the server made, but does in one that the administrators
 * can use as well, and in any if the rights are not to be looked at
 */
static void
test_client_refuses (Fixture       *fixture,
                     gconstpointer  user_data)
{
  const struct
    {
      const gchar *dacl;
      gboolean     ignore;
      gboolean     refused;
    }
  cases[] =
    {
      { "D:P(A;;FA;;;$U)(A;;FA;;;SY)(A;;FR;;;WD)", FALSE, TRUE },
      { "D:P(A;;FA;;;$U)(A;;FA;;;SY)(A;;FW;;;AU)", FALSE, TRUE },
      { "D:P(A;;FA;;;$U)(A;;FA;;;SY)(A;;FA;;;BA)", FALSE, FALSE },
      { "D:P(A;;FA;;;$U)(A;;FA;;;SY)(A;;FR;;;WD)", TRUE, FALSE },
    };
  gsize i;

  for (i = 0; i < G_N_ELEMENTS (cases); i++)
    {
      gchar *name = g_strdup_printf ("keyrings-%" G_GSIZE_FORMAT, i);
      gchar *dir = g_build_filename (fixture->root, name, NULL);
      gchar *reason = NULL;

      g_assert_true (g_win32_ensure_private_directory (dir, NULL));

      if (cases[i].ignore)
        g_setenv (KEYRING_IGNORE_VARIABLE, "1", TRUE);
      after_challenge_dacl = cases[i].dacl;
      after_challenge = change_cookie_file;
      g_assert_cmpint (authenticate (dir, &reason), ==, !cases[i].refused);
      after_challenge = NULL;
      g_unsetenv (KEYRING_IGNORE_VARIABLE);
      g_assert_cmpint (client_refused, ==, cases[i].refused);
      g_assert_null (reason);

      /* The same without the change is accepted */
      if (cases[i].refused)
        {
          assert_keyring_works (dir);
          g_assert_false (client_refused);
        }

      g_free (name);
      g_free (dir);
    }
}

#else  /* !G_OS_WIN32 */

static void
test_win32_only (Fixture       *fixture,
                 gconstpointer  user_data)
{
  g_test_skip ("Only applies to Windows");
}

#endif  /* !G_OS_WIN32 */

#define add_test(path, func) g_test_add (path, Fixture, NULL, set_up, func, tear_down)

#ifdef G_OS_WIN32
#define add_win32_test(name, func) add_test ("/gdbus/keyring-permissions/win32/" name, func)
#else
#define add_win32_test(name, func) add_test ("/gdbus/keyring-permissions/win32/" name, test_win32_only)
#endif

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, G_TEST_OPTION_ISOLATE_DIRS, NULL);

  add_test ("/gdbus/keyring-permissions/new", test_new);
  add_test ("/gdbus/keyring-permissions/open-directory", test_open_directory);
  add_test ("/gdbus/keyring-permissions/ignore-permission", test_ignore_permission);

  add_win32_test ("check-refused", test_check_refused);
  add_win32_test ("check-accepted", test_check_accepted);
  add_win32_test ("foreign-directory", test_foreign_directory);
  add_win32_test ("junction", test_junction);
  add_win32_test ("create-private-files", test_create_private_files);
  add_win32_test ("read-private-file", test_read_private_file);
  add_win32_test ("set-contents-busy", test_set_contents_busy);
  add_win32_test ("cookie-files", test_cookie_files);
  add_win32_test ("client-refuses", test_client_refuses);

  return g_test_run ();
}
