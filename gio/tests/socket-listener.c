/* GLib testing framework examples and tests
 *
 * Copyright 2014 Red Hat, Inc.
 * Copyright 2025 GNOME Foundation, Inc.
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
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see
 * <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <gio/gio.h>

#ifdef HAVE_RTLD_NEXT
#include <dlfcn.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdint.h>
#include <sys/socket.h>
#endif

/* Override the socket(), bind(), listen() and getsockopt() functions from libc
 * so that we can mock results from them in the tests. The libc implementations
 * are used by default (via `dlsym()`) unless a test sets a callback
 * deliberately.
 *
 * These override functions are used simply because the linker will resolve them
 * before it finds the symbols in libc. This is effectively like `LD_PRELOAD`,
 * except without using an external library for them.
 *
 * This mechanism is intended to be generic and not to force tests in this file
 * to be written in a certain way. Tests are free to override these functions
 * with their own implementations, or leave them as default. Different tests may
 * need to mock these socket functions differently.
 *
 * If a test overrides these functions, it *must* do so at the start of the test
 * (before starting any threads), and *must* clear them to `NULL` at the end of
 * the test. The overrides are not thread-safe and will not be automatically
 * cleared at the end of a test.
 */
/* FIXME: Not currently supported on macOS as its symbol lookup works
 * differently to Linux. It will likely need something like DYLD_INTERPOSE()
 * from getpwuid-preload.c here to work. At that point, this common code for
 * mocking arbitrary syscalls using dlsym(RTLD_NEXT) should probably be factored
 * out into a set of internal helpers, because various tests do it for various
 * syscalls. */
#if defined(HAVE_RTLD_NEXT) && !defined(__APPLE__)
#define MOCK_SOCKET_SUPPORTED
#endif

#ifdef MOCK_SOCKET_SUPPORTED
static int (*real_socket) (int, int, int);
static int (*real_bind) (int, const struct sockaddr *, socklen_t);
static int (*real_listen) (int, int);
static int (*real_getsockopt) (int, int, int, void *, socklen_t *);
static int (*mock_socket) (int, int, int);
static int (*mock_bind) (int, const struct sockaddr *, socklen_t);
static int (*mock_listen) (int, int);
static int (*mock_getsockopt) (int, int, int, void *, socklen_t *);

int
socket (int domain,
        int type,
        int protocol)
{
  if (real_socket == NULL)
    real_socket = dlsym (RTLD_NEXT, "socket");

  return ((mock_socket != NULL) ? mock_socket : real_socket) (domain, type, protocol);
}

int
bind (int                    sockfd,
      const struct sockaddr *addr,
      socklen_t              addrlen)
{
  if (real_bind == NULL)
    real_bind = dlsym (RTLD_NEXT, "bind");

  return ((mock_bind != NULL) ? mock_bind : real_bind) (sockfd, addr, addrlen);
}

int
listen (int sockfd,
        int backlog)
{
  if (real_listen == NULL)
    real_listen = dlsym (RTLD_NEXT, "listen");

  return ((mock_listen != NULL) ? mock_listen : real_listen) (sockfd, backlog);
}

int
getsockopt (int        sockfd,
            int        level,
            int        optname,
            void      *optval,
            socklen_t *optlen)
{
  if (real_getsockopt == NULL)
    real_getsockopt = dlsym (RTLD_NEXT, "getsockopt");

  return ((mock_getsockopt != NULL) ? mock_getsockopt : real_getsockopt) (sockfd, level, optname, optval, optlen);
}

#endif  /* MOCK_SOCKET_SUPPORTED */

/* Test event signals. */
static void
event_cb (GSocketListener      *listener,
          GSocketListenerEvent  event,
          GSocket              *socket,
          gpointer              data)
{
  static GSocketListenerEvent expected_event = G_SOCKET_LISTENER_BINDING;
  gboolean *success = (gboolean *)data;

  g_assert (G_IS_SOCKET_LISTENER (listener));
  g_assert (G_IS_SOCKET (socket));
  g_assert (event == expected_event);

  switch (event)
    {
      case G_SOCKET_LISTENER_BINDING:
        expected_event = G_SOCKET_LISTENER_BOUND;
        break;
      case G_SOCKET_LISTENER_BOUND:
        expected_event = G_SOCKET_LISTENER_LISTENING;
        break;
      case G_SOCKET_LISTENER_LISTENING:
        expected_event = G_SOCKET_LISTENER_LISTENED;
        break;
      case G_SOCKET_LISTENER_LISTENED:
        *success = TRUE;
        break;
    }
}

static void
test_event_signal (void)
{
  gboolean success = FALSE;
  GInetAddress *iaddr;
  GSocketAddress *saddr;
  GSocketListener *listener;
  GError *error = NULL;

  iaddr = g_inet_address_new_loopback (G_SOCKET_FAMILY_IPV4);
  saddr = g_inet_socket_address_new (iaddr, 0);
  g_object_unref (iaddr);

  listener = g_socket_listener_new ();

  g_signal_connect (listener, "event", G_CALLBACK (event_cb), &success);

  g_socket_listener_add_address (listener,
                                 saddr,
                                 G_SOCKET_TYPE_STREAM,
                                 G_SOCKET_PROTOCOL_TCP,
                                 NULL,
                                 NULL,
                                 &error);
  g_assert_no_error (error);
  g_assert_true (success);

  g_object_unref (saddr);
  g_object_unref (listener);
}

/* Provide a mock implementation of socket(), listen(), bind() and getsockopt()
 * which use a simple fixed configuration to either force a call to fail with a
 * given errno, or allow it to pass through to the system implementation (which
 * is assumed to succeed). Results are differentiated by protocol (IPv4 or IPv6)
 * but nothing more complex than that.
 *
 * This allows the `listen()` fallback code in
 * `g_socket_listener_add_any_inet_port()` and
 * `g_socket_listener_add_inet_port()` to be tested for different situations
 * where IPv4 and/or IPv6 sockets don’t work. It doesn’t allow the port
 * allocation retry logic to be tested (as it forces all IPv6 `bind()` calls to
 * have the same result), but this means it doesn’t have to do more complex
 * state tracking of fully mocked-up sockets.
 *
 * It also means that the test won’t work on systems which don’t support IPv6,
 * or which have a configuration which causes the first test case (which passes
 * all syscalls through to the system) to fail. On those systems, the test
 * should be skipped rather than the mock made more complex.
 */
#ifdef MOCK_SOCKET_SUPPORTED

typedef struct {
  gboolean ipv6_socket_supports_ipv4;
  int ipv4_socket_errno;  /* 0 for socket() to succeed on the IPv4 socket (i.e. IPv4 sockets are supported) */
  int ipv6_socket_errno;  /* similarly */
  int ipv4_bind_errno;  /* 0 for bind() to succeed on the IPv4 socket */
  int ipv6_bind_errno;  /* similarly */
  int ipv4_listen_errno;  /* 0 for listen() to succeed on the IPv4 socket */
  int ipv6_listen_errno;  /* similarly */
} ListenFailuresConfig;

static struct {
  /* Input: */
  ListenFailuresConfig config;

  /* State (we only support tracking one socket of each type): */
  int ipv4_socket_fd;
  int ipv6_socket_fd;
} listen_failures_mock_state;

static int
listen_failures_socket (int domain,
                        int type,
                        int protocol)
{
  int fd;

  /* Error out if told to */
  if (domain == AF_INET && listen_failures_mock_state.config.ipv4_socket_errno != 0)
    {
      errno = listen_failures_mock_state.config.ipv4_socket_errno;
      return -1;
    }
  else if (domain == AF_INET6 && listen_failures_mock_state.config.ipv6_socket_errno != 0)
    {
      errno = listen_failures_mock_state.config.ipv6_socket_errno;
      return -1;
    }
  else if (domain != AF_INET && domain != AF_INET6)
    {
      /* we don’t expect to support other socket types */
      g_assert_not_reached ();
    }

  /* Pass through to the system, which we require to succeed because we’re only
   * mocking errors and not the full socket stack state */
  fd = real_socket (domain, type, protocol);
  g_assert_no_errno (fd);

  /* Track the returned FD for each socket type */
  if (domain == AF_INET)
    {
      g_assert (listen_failures_mock_state.ipv4_socket_fd == 0);
      listen_failures_mock_state.ipv4_socket_fd = fd;
    }
  else if (domain == AF_INET6)
    {
      g_assert (listen_failures_mock_state.ipv6_socket_fd == 0);
      listen_failures_mock_state.ipv6_socket_fd = fd;
    }

  return fd;
}

static int
listen_failures_bind (int                    sockfd,
                      const struct sockaddr *addr,
                      socklen_t              addrlen)
{
  int retval;

  /* Error out if told to */
  if (listen_failures_mock_state.ipv4_socket_fd == sockfd &&
      listen_failures_mock_state.config.ipv4_bind_errno != 0)
    {
      errno = listen_failures_mock_state.config.ipv4_bind_errno;
      return -1;
    }
  else if (listen_failures_mock_state.ipv6_socket_fd == sockfd &&
           listen_failures_mock_state.config.ipv6_bind_errno != 0)
    {
      errno = listen_failures_mock_state.config.ipv6_bind_errno;
      return -1;
    }
  else if (listen_failures_mock_state.ipv4_socket_fd != sockfd &&
           listen_failures_mock_state.ipv6_socket_fd != sockfd)
    {
      g_assert_not_reached ();
    }

  /* Pass through to the system, which we require to succeed because we’re only
   * mocking errors and not the full socket stack state */
  retval = real_bind (sockfd, addr, addrlen);
  g_assert_no_errno (retval);

  return retval;
}

static int
listen_failures_listen (int sockfd,
                        int backlog)
{
  int retval;

  /* Error out if told to */
  if (listen_failures_mock_state.ipv4_socket_fd == sockfd &&
      listen_failures_mock_state.config.ipv4_listen_errno != 0)
    {
      errno = listen_failures_mock_state.config.ipv4_listen_errno;
      return -1;
    }
  else if (listen_failures_mock_state.ipv6_socket_fd == sockfd &&
           listen_failures_mock_state.config.ipv6_listen_errno != 0)
    {
      errno = listen_failures_mock_state.config.ipv6_listen_errno;
      return -1;
    }
  else if (listen_failures_mock_state.ipv4_socket_fd != sockfd &&
           listen_failures_mock_state.ipv6_socket_fd != sockfd)
    {
      g_assert_not_reached ();
    }

  /* Pass through to the system, which we require to succeed because we’re only
   * mocking errors and not the full socket stack state */
  retval = real_listen (sockfd, backlog);
  g_assert_no_errno (retval);

  return retval;
}

static int
listen_failures_getsockopt (int        sockfd,
                            int        level,
                            int        optname,
                            void      *optval,
                            socklen_t *optlen)
{
  /* Mock whether IPv6 sockets claim to support IPv4 */
#if defined (IPPROTO_IPV6) && defined (IPV6_V6ONLY)
  if (listen_failures_mock_state.ipv6_socket_fd == sockfd &&
      level == IPPROTO_IPV6 && optname == IPV6_V6ONLY)
    {
      int *v6_only = optval;
      *v6_only = !listen_failures_mock_state.config.ipv6_socket_supports_ipv4;
      return 0;
    }
#endif

  /* Don’t assert that the system getsockopt() succeeded, as it could be used
   * in complex ways, and it’s incidental to what we’re actually trying to test. */
  return real_getsockopt (sockfd, level, optname, optval, optlen);
}

#endif  /* MOCK_SOCKET_SUPPORTED */

static void
test_add_any_inet_port_listen_failures (void)
{
#ifdef MOCK_SOCKET_SUPPORTED
  const struct
    {
      ListenFailuresConfig config;
      GQuark expected_error_domain;  /* 0 if success is expected */
      int expected_error_code;  /* 0 if success is expected */
    }
  test_matrix[] =
    {
      /* If everything works, it should all work: */
      { { TRUE, 0, 0, 0, 0, 0, 0 }, 0, 0 },
      /* If IPv4 sockets are not supported, IPv6 should be used: */
      { { TRUE, EAFNOSUPPORT, 0, 0, 0, 0, 0 }, 0, 0 },
      /* If IPv6 sockets are not supported, IPv4 should be used: */
      { { TRUE, 0, EAFNOSUPPORT, 0, 0, 0, 0, }, 0, 0 },
      /* If no sockets are supported, everything should fail: */
      { { TRUE, EAFNOSUPPORT, EAFNOSUPPORT, 0, 0, 0, 0 },
        G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED },
      /* If binding IPv4 fails, IPv6 should be used: */
      { { TRUE, 0, 0, EADDRINUSE, 0, 0, 0 }, 0, 0 },
      /* If binding IPv6 fails, fail overall (the algorithm is not symmetric): */
      { { TRUE, 0, 0, 0, EADDRINUSE, 0, 0 },
        G_IO_ERROR, G_IO_ERROR_ADDRESS_IN_USE },
      /* If binding them both fails, fail overall: */
      { { TRUE, 0, 0, EADDRINUSE, EADDRINUSE, 0, 0 },
        G_IO_ERROR, G_IO_ERROR_ADDRESS_IN_USE },
      /* If listening on IPv4 fails, IPv6 should be used: */
      { { TRUE, 0, 0, 0, 0, EADDRINUSE, 0 }, 0, 0 },
      /* If listening on IPv6 fails, IPv4 should be used:
       * FIXME: If the IPv6 socket claims to support IPv4, this currently won’t
       * retry with an IPv4-only socket; see https://gitlab.gnome.org/GNOME/glib/-/issues/3604 */
      { { TRUE, 0, 0, 0, 0, 0, EADDRINUSE },
        G_IO_ERROR, G_IO_ERROR_ADDRESS_IN_USE },
      /* If listening on IPv6 fails (and the IPv6 socket doesn’t claim to
       * support IPv4), IPv4 should be used: */
      { { FALSE, 0, 0, 0, 0, 0, EADDRINUSE }, 0, 0 },
      /* If listening on both fails, fail overall: */
      { { TRUE, 0, 0, 0, 0, EADDRINUSE, EADDRINUSE },
        G_IO_ERROR, G_IO_ERROR_ADDRESS_IN_USE },
    };

  /* Override the socket(), bind(), listen() and getsockopt() functions */
  mock_socket = listen_failures_socket;
  mock_bind = listen_failures_bind;
  mock_listen = listen_failures_listen;
  mock_getsockopt = listen_failures_getsockopt;

  g_test_summary ("Test that adding a listening port succeeds if either "
                  "listening on IPv4 or IPv6 succeeds");

  for (size_t i = 0; i < G_N_ELEMENTS (test_matrix); i++)
    {
      GSocketService *service = NULL;
      GError *local_error = NULL;
      uint16_t port;

      g_test_message ("Test %" G_GSIZE_FORMAT, i);

      /* Configure the mock socket behaviour. */
      memset (&listen_failures_mock_state, 0, sizeof (listen_failures_mock_state));
      listen_failures_mock_state.config = test_matrix[i].config;

      /* Create a GSocketService to test. */
      service = g_socket_service_new ();
      port = g_socket_listener_add_any_inet_port (G_SOCKET_LISTENER (service), NULL, &local_error);

      if (test_matrix[i].expected_error_domain == 0)
        {
          g_assert_no_error (local_error);
          g_assert_cmpuint (port, !=, 0);
        }
      else
        {
          g_assert_error (local_error, test_matrix[i].expected_error_domain,
                          test_matrix[i].expected_error_code);
          g_assert_cmpuint (port, ==, 0);
        }

      g_clear_error (&local_error);
      g_socket_listener_close (G_SOCKET_LISTENER (service));
      g_clear_object (&service);
    }

  /* Tidy up. */
  mock_socket = NULL;
  mock_bind = NULL;
  mock_listen = NULL;
  mock_getsockopt = NULL;
  memset (&listen_failures_mock_state, 0, sizeof (listen_failures_mock_state));
#else  /* if !MOCK_SOCKET_SUPPORTED */
  g_test_skip ("Mock socket not supported");
#endif
}

static void
test_add_inet_port_listen_failures (void)
{
#ifdef MOCK_SOCKET_SUPPORTED
  const struct
    {
      ListenFailuresConfig config;
      GQuark expected_error_domain;  /* 0 if success is expected */
      int expected_error_code;  /* 0 if success is expected */
    }
  test_matrix[] =
    {
      /* If everything works, it should all work: */
      { { TRUE, 0, 0, 0, 0, 0, 0 }, 0, 0 },
      /* If IPv4 sockets are not supported, IPv6 should be used: */
      { { TRUE, EAFNOSUPPORT, 0, 0, 0, 0, 0 }, 0, 0 },
      /* If IPv6 sockets are not supported, IPv4 should be used: */
      { { TRUE, 0, EAFNOSUPPORT, 0, 0, 0, 0, }, 0, 0 },
      /* If no sockets are supported, everything should fail: */
      { { TRUE, EAFNOSUPPORT, EAFNOSUPPORT, 0, 0, 0, 0 },
        G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED },
      /* If binding IPv4 fails, IPv6 should be used: */
      { { TRUE, 0, 0, EADDRINUSE, 0, 0, 0 }, 0, 0 },
      /* If binding IPv6 fails, fail overall (the algorithm is not symmetric): */
      { { TRUE, 0, 0, 0, EADDRINUSE, 0, 0 },
        G_IO_ERROR, G_IO_ERROR_ADDRESS_IN_USE },
      /* If binding them both fails, fail overall: */
      { { TRUE, 0, 0, EADDRINUSE, EADDRINUSE, 0, 0 },
        G_IO_ERROR, G_IO_ERROR_ADDRESS_IN_USE },
      /* If listening on IPv4 fails, IPv6 should be used: */
      { { TRUE, 0, 0, 0, 0, EADDRINUSE, 0 }, 0, 0 },
      /* If listening on IPv6 fails, IPv4 should be used: */
      { { TRUE, 0, 0, 0, 0, 0, EADDRINUSE }, 0, 0 },
      /* If listening on IPv6 fails (and the IPv6 socket doesn’t claim to
       * support IPv4), IPv4 should be used: */
      { { FALSE, 0, 0, 0, 0, 0, EADDRINUSE }, 0, 0 },
      /* If listening on both fails, fail overall: */
      { { TRUE, 0, 0, 0, 0, EADDRINUSE, EADDRINUSE },
        G_IO_ERROR, G_IO_ERROR_ADDRESS_IN_USE },
    };

  /* Override the socket(), bind(), listen() and getsockopt() functions */
  mock_socket = listen_failures_socket;
  mock_bind = listen_failures_bind;
  mock_listen = listen_failures_listen;
  mock_getsockopt = listen_failures_getsockopt;

  g_test_summary ("Test that adding a listening port succeeds if either "
                  "listening on IPv4 or IPv6 succeeds");

  for (size_t i = 0; i < G_N_ELEMENTS (test_matrix); i++)
    {
      GSocketService *service = NULL;
      GError *local_error = NULL;
      gboolean retval;

      g_test_message ("Test %" G_GSIZE_FORMAT, i);

      /* Configure the mock socket behaviour. */
      memset (&listen_failures_mock_state, 0, sizeof (listen_failures_mock_state));
      listen_failures_mock_state.config = test_matrix[i].config;

      /* Create a GSocketService to test. */
      service = g_socket_service_new ();
      retval = g_socket_listener_add_inet_port (G_SOCKET_LISTENER (service), 4321, NULL, &local_error);

      if (test_matrix[i].expected_error_domain == 0)
        {
          g_assert_no_error (local_error);
          g_assert_true (retval);
        }
      else
        {
          g_assert_error (local_error, test_matrix[i].expected_error_domain,
                          test_matrix[i].expected_error_code);
          g_assert_false (retval);
        }

      g_clear_error (&local_error);

      g_socket_listener_close (G_SOCKET_LISTENER (service));
      g_clear_object (&service);
    }

  /* Tidy up. */
  mock_socket = NULL;
  mock_bind = NULL;
  mock_listen = NULL;
  mock_getsockopt = NULL;
  memset (&listen_failures_mock_state, 0, sizeof (listen_failures_mock_state));
#else  /* if !MOCK_SOCKET_SUPPORTED */
  g_test_skip ("Mock socket not supported");
#endif
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/socket-listener/event-signal", test_event_signal);
  g_test_add_func ("/socket-listener/add-any-inet-port/listen-failures", test_add_any_inet_port_listen_failures);
  g_test_add_func ("/socket-listener/add-inet-port/listen-failures", test_add_inet_port_listen_failures);

  return g_test_run();
}
