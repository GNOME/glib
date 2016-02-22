/* GIO - GLib Input, Output and Streaming Library
 * 
 * Copyright (C) 2006-2007 Red Hat, Inc.
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
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Alexander Larsson <alexl@redhat.com>
 */

#include "config.h"

#include "gasynchelper.h"


/*< private >
 * SECTION:gasynchelper
 * @short_description: Asynchronous Helper Functions
 * @include: gio/gio.h
 * @see_also: #GAsyncResult
 * 
 * Provides helper functions for asynchronous operations.
 *
 **/

#ifdef G_OS_WIN32
gboolean
_g_win32_overlap_wait_result (HANDLE           hfile,
                              OVERLAPPED      *overlap,
                              DWORD           *transferred,
                              GCancellable    *cancellable)
{
  GPollFD pollfd[2];
  gboolean result = FALSE;
  gint num, npoll;

  pollfd[0].fd = (gint)overlap->hEvent;
  pollfd[0].events = G_IO_IN;
  num = 1;

  if (g_cancellable_make_pollfd (cancellable, &pollfd[1]))
    num++;

loop:
  npoll = g_poll (pollfd, num, -1);
  if (npoll <= 0)
    /* error out, should never happen */
    goto end;

  if (g_cancellable_is_cancelled (cancellable))
    {
      /* CancelIO only cancels pending operations issued by the
       * current thread and since we're doing only sync operations,
       * this is safe.... */
      /* CancelIoEx is only Vista+. Since we have only one overlap
       * operaton on this thread, we can just use: */
      result = CancelIo (hfile);
      g_warn_if_fail (result);
    }

  result = GetOverlappedResult (overlap->hEvent, overlap, transferred, FALSE);
  if (result == FALSE &&
      GetLastError () == ERROR_IO_INCOMPLETE &&
      !g_cancellable_is_cancelled (cancellable))
    goto loop;

end:
  if (num > 1)
    g_cancellable_release_fd (cancellable);

  return result;
}

typedef struct {
  GSource source;
  GPollFD pollfd;
} GWin32HandleSource;

static gboolean
g_win32_handle_source_prepare (GSource *source,
                               gint    *timeout)
{
  *timeout = -1;
  return FALSE;
}

static gboolean
g_win32_handle_source_check (GSource *source)
{
  GWin32HandleSource *hsource = (GWin32HandleSource *)source;

  return hsource->pollfd.revents;
}

static gboolean
g_win32_handle_source_dispatch (GSource     *source,
                                GSourceFunc  callback,
                                gpointer     user_data)
{
  GWin32HandleSourceFunc func = (GWin32HandleSourceFunc)callback;
  GWin32HandleSource *hsource = (GWin32HandleSource *)source;

  return func (hsource->pollfd.fd, user_data);
}

static void
g_win32_handle_source_finalize (GSource *source)
{
}

static gboolean
g_win32_handle_source_closure_callback (HANDLE   handle,
                                        gpointer data)
{
  GClosure *closure = data;

  GValue param = G_VALUE_INIT;
  GValue result_value = G_VALUE_INIT;
  gboolean result;

  g_value_init (&result_value, G_TYPE_BOOLEAN);

  g_value_init (&param, G_TYPE_POINTER);
  g_value_set_pointer (&param, handle);

  g_closure_invoke (closure, &result_value, 1, &param, NULL);

  result = g_value_get_boolean (&result_value);
  g_value_unset (&result_value);
  g_value_unset (&param);

  return result;
}

GSourceFuncs g_win32_handle_source_funcs = {
  g_win32_handle_source_prepare,
  g_win32_handle_source_check,
  g_win32_handle_source_dispatch,
  g_win32_handle_source_finalize,
  (GSourceFunc)g_win32_handle_source_closure_callback,
};

GSource *
_g_win32_handle_create_source (HANDLE        handle,
                               GCancellable *cancellable)
{
  GWin32HandleSource *hsource;
  GSource *source;

  source = g_source_new (&g_win32_handle_source_funcs, sizeof (GWin32HandleSource));
  hsource = (GWin32HandleSource *)source;
  g_source_set_name (source, "GWin32Handle");

  if (cancellable)
    {
      GSource *cancellable_source;

      cancellable_source = g_cancellable_source_new (cancellable);
      g_source_add_child_source (source, cancellable_source);
      g_source_set_dummy_callback (cancellable_source);
      g_source_unref (cancellable_source);
    }

  hsource->pollfd.fd = (gint)handle;
  hsource->pollfd.events = G_IO_IN;
  hsource->pollfd.revents = 0;
  g_source_add_poll (source, &hsource->pollfd);

  return source;
}
#endif
