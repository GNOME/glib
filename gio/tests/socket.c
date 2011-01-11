/* GLib testing framework examples and tests
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

#include <gio/gio.h>

#ifdef G_OS_UNIX
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <string.h>
#include <stdlib.h>
#include <gio/gunixconnection.h>
#endif

#ifdef G_OS_UNIX
static void
test_unix_from_fd (void)
{
  gint fd;
  GError *error;
  GSocket *s;

  fd = socket (AF_UNIX, SOCK_STREAM, 0);
  g_assert_cmpint (fd, !=, -1);

  error = NULL;
  s = g_socket_new_from_fd (fd, &error);
  g_assert_no_error (error);
  g_assert_cmpint (g_socket_get_family (s), ==, G_SOCKET_FAMILY_UNIX);
  g_assert_cmpint (g_socket_get_socket_type (s), ==, G_SOCKET_TYPE_STREAM);
  g_assert_cmpint (g_socket_get_protocol (s), ==, G_SOCKET_PROTOCOL_DEFAULT);
  g_object_unref (s);
}

static void
test_unix_connection (void)
{
  gint fd;
  GError *error;
  GSocket *s;
  GSocketConnection *c;

  fd = socket (AF_UNIX, SOCK_STREAM, 0);
  g_assert_cmpint (fd, !=, -1);

  error = NULL;
  s = g_socket_new_from_fd (fd, &error);
  g_assert_no_error (error);
  c = g_socket_connection_factory_create_connection (s);
  g_assert (G_IS_UNIX_CONNECTION (c));
  g_object_unref (c);
  g_object_unref (s);
}

static GSocketConnection *
create_connection_for_fd (int fd)
{
  GError *err = NULL;
  GSocket *socket;
  GSocketConnection *connection;

  socket = g_socket_new_from_fd (fd, &err);
  g_assert_no_error (err);
  g_assert (G_IS_SOCKET (socket));
  connection = g_socket_connection_factory_create_connection (socket);
  g_assert (G_IS_UNIX_CONNECTION (connection));
  g_object_unref (socket);
  return connection;
}

#define TEST_DATA "failure to say failure to say 'i love gnome-panel!'."

static void
test_unix_connection_ancillary_data (void)
{
  GError *err = NULL;
  gint pv[2], sv[3];
  gint status, fd, len;
  char buffer[1024];
  pid_t pid;

  status = pipe (pv);
  g_assert_cmpint (status, ==, 0);

  status = socketpair (PF_UNIX, SOCK_STREAM, 0, sv);
  g_assert_cmpint (status, ==, 0);

  pid = fork ();
  g_assert_cmpint (pid, >=, 0);

  /* Child: close its copy of the write end of the pipe, receive it
   * again from the parent over the socket, and write some text to it.
   *
   * Parent: send the write end of the pipe (still open for the
   * parent) over the socket, close it, and read some text from the
   * read end of the pipe.
   */
  if (pid == 0)
    {
      GSocketConnection *connection;

      close (sv[1]);
      connection = create_connection_for_fd (sv[0]);

      status = close (pv[1]);
      g_assert_cmpint (status, ==, 0);

      err = NULL;
      fd = g_unix_connection_receive_fd (G_UNIX_CONNECTION (connection), NULL,
					 &err);
      g_assert_no_error (err);
      g_assert_cmpint (fd, >, -1);
      g_object_unref (connection);

      do
	len = write (fd, TEST_DATA, sizeof (TEST_DATA));
      while (len == -1 && errno == EINTR);
      g_assert_cmpint (len, ==, sizeof (TEST_DATA));
      exit (0);
    }
  else
    {
      GSocketConnection *connection;

      close (sv[0]);
      connection = create_connection_for_fd (sv[1]);

      err = NULL;
      g_unix_connection_send_fd (G_UNIX_CONNECTION (connection), pv[1], NULL,
				 &err);
      g_assert_no_error (err);
      g_object_unref (connection);

      status = close (pv[1]);
      g_assert_cmpint (status, ==, 0);

      memset (buffer, 0xff, sizeof buffer);
      do
	len = read (pv[0], buffer, sizeof buffer);
      while (len == -1 && errno == EINTR);

      g_assert_cmpint (len, ==, sizeof (TEST_DATA));
      g_assert_cmpstr (buffer, ==, TEST_DATA);

      waitpid (pid, &status, 0);
      g_assert (WIFEXITED (status));
      g_assert_cmpint (WEXITSTATUS (status), ==, 0);
    }

  /* TODO: add test for g_unix_connection_send_credentials() and
   * g_unix_connection_receive_credentials().
   */
}
#endif /* G_OS_UNIX */

int
main (int   argc,
      char *argv[])
{
  g_type_init ();
  g_test_init (&argc, &argv, NULL);

#ifdef G_OS_UNIX
  g_test_add_func ("/socket/unix-from-fd", test_unix_from_fd);
  g_test_add_func ("/socket/unix-connection", test_unix_connection);
  g_test_add_func ("/socket/unix-connection-ancillary-data", test_unix_connection_ancillary_data);
#endif

  return g_test_run();
}

