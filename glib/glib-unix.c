/* GLIB - Library of useful routines for C programming
 * Copyright (C) 2011 Red Hat, Inc.
 *
 * glib-unix.c: UNIX specific API wrappers and convenience functions
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
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: Colin Walters <walters@verbum.org>
 */

#include "config.h"

#include "glib-unix.h"
#include "gmain-internal.h"

#include <string.h>

/**
 * SECTION:gunix
 * @title: UNIX-specific utilities and integration
 * @short_description: pipes, signal handling
 * @include: glib-unix.h
 *
 * Most of GLib is intended to be portable; in contrast, this set of
 * functions is designed for programs which explicitly target UNIX,
 * or are using it to build higher level abstractions which would be
 * conditionally compiled if the platform matches G_OS_UNIX.
 *
 * To use these functions, you must explicitly include the
 * "glib-unix.h" header.
 */

GQuark
g_unix_error_quark (void)
{
  return g_quark_from_static_string ("g-unix-error-quark");
}

static gboolean
g_unix_set_error_from_errno (GError **error,
                             gint     saved_errno)
{
  g_set_error_literal (error,
                       G_UNIX_ERROR,
                       0,
                       g_strerror (saved_errno));
  errno = saved_errno;
  return FALSE;
}

/**
 * g_unix_open_pipe:
 * @fds: Array of two integers
 * @flags: Bitfield of file descriptor flags, see "man 2 fcntl"
 * @error: a #GError
 *
 * Similar to the UNIX pipe() call, but on modern systems like Linux
 * uses the pipe2() system call, which atomically creates a pipe with
 * the configured flags.  The only supported flag currently is
 * <literal>FD_CLOEXEC</literal>.  If for example you want to configure
 * <literal>O_NONBLOCK</literal>, that must still be done separately with
 * fcntl().
 *
 * <note>This function does *not* take <literal>O_CLOEXEC</literal>, it takes
 * <literal>FD_CLOEXEC</literal> as if for fcntl(); these are
 * different on Linux/glibc.</note>
 *
 * Returns: %TRUE on success, %FALSE if not (and errno will be set).
 *
 * Since: 2.30
 */
gboolean
g_unix_open_pipe (int     *fds,
                  int      flags,
                  GError **error)
{
  int ecode;

  /* We only support FD_CLOEXEC */
  g_return_val_if_fail ((flags & (FD_CLOEXEC)) == flags, FALSE);

#ifdef HAVE_PIPE2
  {
    int pipe2_flags = 0;
    if (flags & FD_CLOEXEC)
      pipe2_flags |= O_CLOEXEC;
    /* Atomic */
    ecode = pipe2 (fds, pipe2_flags);
    if (ecode == -1 && errno != ENOSYS)
      return g_unix_set_error_from_errno (error, errno);
    else if (ecode == 0)
      return TRUE;
    /* Fall through on -ENOSYS, we must be running on an old kernel */
  }
#endif
  ecode = pipe (fds);
  if (ecode == -1)
    return g_unix_set_error_from_errno (error, errno);
  ecode = fcntl (fds[0], flags);
  if (ecode == -1)
    {
      int saved_errno = errno;
      close (fds[0]);
      close (fds[1]);
      return g_unix_set_error_from_errno (error, saved_errno);
    }
  ecode = fcntl (fds[1], flags);
  if (ecode == -1)
    {
      int saved_errno = errno;
      close (fds[0]);
      close (fds[1]);
      return g_unix_set_error_from_errno (error, saved_errno);
    }
  return TRUE;
}

/**
 * g_unix_set_fd_nonblocking:
 * @fd: A file descriptor
 * @nonblock: If %TRUE, set the descriptor to be non-blocking
 * @error: a #GError
 *
 * Control the non-blocking state of the given file descriptor,
 * according to @nonblock.  On most systems this uses <literal>O_NONBLOCK</literal>, but
 * on some older ones may use <literal>O_NDELAY</literal>.
 *
 * Returns: %TRUE if successful
 *
 * Since: 2.30
 */
gboolean
g_unix_set_fd_nonblocking (gint       fd,
                           gboolean   nonblock,
                           GError   **error)
{
#ifdef F_GETFL
  glong fcntl_flags;
  fcntl_flags = fcntl (fd, F_GETFL);

  if (fcntl_flags == -1)
    return g_unix_set_error_from_errno (error, errno);

  if (nonblock)
    {
#ifdef O_NONBLOCK
      fcntl_flags |= O_NONBLOCK;
#else
      fcntl_flags |= O_NDELAY;
#endif
    }
  else
    {
#ifdef O_NONBLOCK
      fcntl_flags &= ~O_NONBLOCK;
#else
      fcntl_flags &= ~O_NDELAY;
#endif
    }

  if (fcntl (fd, F_SETFL, fcntl_flags) == -1)
    return g_unix_set_error_from_errno (error, errno);
  return TRUE;
#else
  return g_unix_set_error_from_errno (error, EINVAL);
#endif
}


/**
 * g_unix_signal_source_new:
 * @signum: A signal number
 *
 * Create a #GSource that will be dispatched upon delivery of the UNIX
 * signal @signum.  Currently only <literal>SIGHUP</literal>,
 * <literal>SIGINT</literal>, and <literal>SIGTERM</literal> can
 * be monitored.  Note that unlike the UNIX default, all sources which
 * have created a watch will be dispatched, regardless of which
 * underlying thread invoked g_unix_signal_source_new().
 *
 * For example, an effective use of this function is to handle <literal>SIGTERM</literal>
 * cleanly; flushing any outstanding files, and then calling
 * g_main_loop_quit ().  It is not safe to do any of this a regular
 * UNIX signal handler; your handler may be invoked while malloc() or
 * another library function is running, causing reentrancy if you
 * attempt to use it from the handler.  None of the GLib/GObject API
 * is safe against this kind of reentrancy.
 *
 * The interaction of this source when combined with native UNIX
 * functions like sigprocmask() is not defined.
 *
 * The source will not initially be associated with any #GMainContext
 * and must be added to one with g_source_attach() before it will be
 * executed.
 *
 * Returns: A newly created #GSource
 *
 * Since: 2.30
 */
GSource *
g_unix_signal_source_new (int signum)
{
  g_return_val_if_fail (signum == SIGHUP || signum == SIGINT || signum == SIGTERM, NULL);

  return _g_main_create_unix_signal_watch (signum);
}

/**
 * g_unix_signal_add_full:
 * @priority: the priority of the signal source. Typically this will be in
 *            the range between #G_PRIORITY_DEFAULT and #G_PRIORITY_HIGH.
 * @signum: Signal number
 * @handler: Callback
 * @user_data: Data for @handler
 * @notify: #GDestroyNotify for @handler
 *
 * A convenience function for g_unix_signal_source_new(), which
 * attaches to the default #GMainContext.  You can remove the watch
 * using g_source_remove().
 *
 * Returns: An ID (greater than 0) for the event source
 *
 * Since: 2.30
 */
guint
g_unix_signal_add_full (int            priority,
                        int            signum,
                        GSourceFunc    handler,
                        gpointer       user_data,
                        GDestroyNotify notify)
{
  guint id;
  GSource *source;

  source = g_unix_signal_source_new (signum);

  if (priority != G_PRIORITY_DEFAULT)
    g_source_set_priority (source, priority);

  g_source_set_callback (source, handler, user_data, notify);
  id = g_source_attach (source, NULL);
  g_source_unref (source);

  return id;
}

/**
 * g_unix_signal_add:
 * @signum: Signal number
 * @handler: Callback
 * @user_data: Data for @handler
 *
 * A convenience function for g_unix_signal_source_new(), which
 * attaches to the default #GMainContext.  You can remove the watch
 * using g_source_remove().
 *
 * Returns: An ID (greater than 0) for the event source
 *
 * Since: 2.30
 */
guint
g_unix_signal_add (int         signum,
                   GSourceFunc handler,
                   gpointer    user_data)
{
  return g_unix_signal_add_full (G_PRIORITY_DEFAULT, signum, handler, user_data, NULL);
}
