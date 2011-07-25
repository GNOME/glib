/*
 * Copyright © 2011 Canonical Limited
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the licence, or (at your option) any later version.
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
 * Author: Ryan Lortie <desrt@desrt.ca>
 */

#include "config.h"

#include "gwakeup.h"

#ifdef _WIN32

#include <windows.h>
#include "gmessages.h"
#include "giochannel.h"
#include "gwin32.h"

GWakeup *
g_wakeup_new (void)
{
  HANDLE wakeup;

  wakeup = CreateEvent (NULL, TRUE, FALSE, NULL);

  if (wakeup == NULL)
    g_error ("Cannot create event for GWakeup: %s",
             g_win32_error_message (GetLastError ()));

  return (GWakeup *) wakeup;
}

void
g_wakeup_get_pollfd (GWakeup *wakeup,
                     GPollFD *fd)
{
  fd->fd = (gintptr) wakeup;
  fd->events = G_IO_IN;
}

void
g_wakeup_acknowledge (GWakeup *wakeup)
{
  ResetEvent ((HANDLE) wakeup);
}

void
g_wakeup_signal (GWakeup *wakeup)
{
  SetEvent ((HANDLE) wakeup);
}

void
g_wakeup_free (GWakeup *wakeup)
{
  CloseHandle ((HANDLE) wakeup);
}

#else

#include "glib-unix.h"
#include <fcntl.h>

#if defined (HAVE_EVENTFD)
#include <sys/eventfd.h>
#endif

struct _GWakeup
{
  gint fds[2];
};

GWakeup *
g_wakeup_new (void)
{
  GError *error = NULL;
  GWakeup *wakeup;

  wakeup = g_slice_new (GWakeup);

  /* try eventfd first, if we think we can */
#if defined (HAVE_EVENTFD)
  wakeup->fds[0] = eventfd (0, EFD_CLOEXEC | EFD_NONBLOCK);

  if (wakeup->fds[0] != -1)
    {
      wakeup->fds[1] = -1;
      return wakeup;
    }

  /* for any failure, try a pipe instead */
#endif

  if (!g_unix_open_pipe (wakeup->fds, FD_CLOEXEC, &error))
    g_error ("Creating pipes for GWakeup: %s\n", error->message);

  if (!g_unix_set_fd_nonblocking (wakeup->fds[0], TRUE, &error) ||
      !g_unix_set_fd_nonblocking (wakeup->fds[1], TRUE, &error))
    g_error ("Set pipes non-blocking for GWakeup: %s\n", error->message);

  return wakeup;
}

void
g_wakeup_get_pollfd (GWakeup *wakeup,
                     GPollFD *fd)
{
  fd->fd = wakeup->fds[0];
  fd->events = G_IO_IN;
}

void
g_wakeup_acknowledge (GWakeup *wakeup)
{
  char buffer[16];

  /* read until it is empty */
  while (read (wakeup->fds[0], buffer, sizeof buffer) == sizeof buffer);
}

void
g_wakeup_signal (GWakeup *wakeup)
{
  guint64 one = 1;

  if (wakeup->fds[1] == -1)
    write (wakeup->fds[0], &one, sizeof one);
  else
    write (wakeup->fds[1], &one, 1);
}

void
g_wakeup_free (GWakeup *wakeup)
{
  close (wakeup->fds[0]);

  if (wakeup->fds[1] != -1)
    close (wakeup->fds[1]);

  g_slice_free (GWakeup, wakeup);
}

#endif /* !_WIN32 */
