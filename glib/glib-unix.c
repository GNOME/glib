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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
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

#include <string.h>

/**
 * SECTION:gunix
 * @short_description: UNIX-specific utilities and integration
 * @include: glib-unix.h
 *
 * Most of GLib is intended to be portable; in constrast, this set of
 * functions is designed for programs which explicitly target UNIX, or
 * are using it to build higher level abstractions which would be
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
g_unix_set_error_from_errno (GError **error)
{
  int saved_errno = errno;
  g_set_error_literal (error,
		       G_UNIX_ERROR,
		       0,
		       g_strerror (errno));
  errno = saved_errno;
  return FALSE;
}

static gboolean
g_unix_set_error_from_errno_saved (GError **error,
				    int      saved_errno)
{
  g_set_error_literal (error,
		       G_UNIX_ERROR,
		       0,
		       g_strerror (saved_errno));
  errno = saved_errno;
  return FALSE;
}

/**
 * g_unix_pipe_flags:
 * @fds: Array of two integers
 * @flags: Bitfield of file descriptor flags, see "man 2 fcntl"
 * @error: a #GError
 *
 * Similar to the UNIX pipe() call, but on modern systems like Linux
 * uses the pipe2 system call, which atomically creates a pipe with
 * the configured flags.  The only supported flag currently is
 * FD_CLOEXEC.  If for example you want to configure O_NONBLOCK, that
 * must still be done separately with fcntl().
 *
 * <note>This function does *not* take O_CLOEXEC, it takes FD_CLOEXEC as if
 * for fcntl(); these are different on Linux/glibc.</note>
 *
 * Returns: %TRUE on success, %FALSE if not (and errno will be set).
 *
 * Since: 2.30
 */
gboolean
g_unix_pipe_flags (int     *fds,
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
      return g_unix_set_error_from_errno (error);
    /* Fall through on -ENOSYS, we must be running on an old kernel */
  }
#endif
  ecode = pipe (fds);
  if (ecode == -1)
    return g_unix_set_error_from_errno (error);
  ecode = fcntl (fds[0], flags);
  if (ecode == -1)
    {
      int saved_errno = errno;
      close (fds[0]);
      return g_unix_set_error_from_errno_saved (error, saved_errno);
    }
  ecode = fcntl (fds[0], flags);
  if (ecode == -1)
    {
      int saved_errno = errno;
      close (fds[0]);
      close (fds[1]);
      return g_unix_set_error_from_errno_saved (error, saved_errno);
    }
  return TRUE;
}
