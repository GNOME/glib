/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * giowin32.c: IO Channels for Win32.
 * Copyright 1998 Owen Taylor and Tor Lillqvist
 * Copyright 1999-2000 Tor Lillqvist and Craig Setera
 * Copyright 2001-2003 Andrew Lanoix
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
 */

/*
 * Modified by the GLib Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GLib Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GLib at ftp://ftp.gtk.org/pub/gtk/.
 */

/* Define this to get (very) verbose logging of all channels */
/* #define G_IO_WIN32_DEBUG */

#include "config.h"

#include "glib.h"

#include <stdlib.h>
#include <windows.h>
#include <winsock.h>          /* Not everybody has winsock2 */
#include <fcntl.h>
#include <io.h>
#include <process.h>
#include <errno.h>
#include <sys/stat.h>

#include "glibintl.h"

typedef struct _GIOWin32Channel GIOWin32Channel;
typedef struct _GIOWin32Watch GIOWin32Watch;

#define BUFFER_SIZE 4096

typedef enum {
  G_IO_WIN32_WINDOWS_MESSAGES,	/* Windows messages */
  G_IO_WIN32_FILE_DESC,		/* Unix-like file descriptors from
				 * _open() or _pipe(). Read with read().
				 * Have to create separate thread to read.
				 */
  G_IO_WIN32_SOCKET		/* Sockets. A separate thread is blocked
				 * in select() most of the time.
				 */
} GIOWin32ChannelType;

struct _GIOWin32Channel {
  GIOChannel channel;
  gint fd;			/* Either a Unix-like file handle as provided
				 * by the Microsoft C runtime, or a SOCKET
				 * as provided by WinSock.
				 */
  GIOWin32ChannelType type;
  
  gboolean debug;

  CRITICAL_SECTION mutex;

  /* This is used by G_IO_WIN32_WINDOWS_MESSAGES channels */
  HWND hwnd;			/* handle of window, or NULL */
  
  /* Following fields are used by both fd and socket channels. */
  gboolean running;		/* Is reader thread running. FALSE if
				 * EOF has been reached.
				 */
  gboolean needs_close;		/* If the channel has been closed while
				 * the reader thread was still running.
				 */
  guint thread_id;		/* If non-NULL has a reader thread, or has
				 * had.*/
  HANDLE data_avail_event;

  gushort revents;

  /* Following fields used by fd channels for input */
  
  /* Data is kept in a circular buffer. To be able to distinguish between
   * empty and full buffer, we cannot fill it completely, but have to
   * leave a one character gap.
   *
   * Data available is between indexes rdp and wrp-1 (modulo BUFFER_SIZE).
   *
   * Empty:    wrp == rdp
   * Full:     (wrp + 1) % BUFFER_SIZE == rdp
   * Partial:  otherwise
   */
  guchar *buffer;		/* (Circular) buffer */
  gint wrp, rdp;		/* Buffer indices for writing and reading */
  HANDLE space_avail_event;

  /* Following fields used by socket channels */
  GSList *watches;
  HANDLE data_avail_noticed_event;
  gint reset_send; /* socket used to send data so select_thread() can reset/re-loop */
  gint reset_recv; /* socket used to recv data so select_thread() can reset/re-loop */
};

#define LOCK(mutex) EnterCriticalSection (&mutex)
#define UNLOCK(mutex) LeaveCriticalSection (&mutex)

struct _GIOWin32Watch {
  GSource       source;
  GPollFD       pollfd;
  GIOChannel   *channel;
  GIOCondition  condition;
};

static void
g_win32_print_access_mode (int flags)
{
  g_print ("%s%s%s%s%s%s%s%s%s%s",
	   ((flags & 0x3) == _O_RDWR ? "O_RDWR" :
	    ((flags & 0x3) == _O_RDONLY ? "O_RDONLY" :
	     ((flags & 0x3) == _O_WRONLY ? "O_WRONLY" : "0"))),
	   (flags & _O_APPEND ? "|O_APPEND" : ""),
	   (flags & _O_RANDOM ? "|O_RANDOM" : ""),
	   (flags & _O_SEQUENTIAL ? "|O_SEQUENTIAL" : ""),
	   (flags & _O_TEMPORARY ? "|O_TEMPORARY" : ""),
	   (flags & _O_CREAT ? "|O_CREAT" : ""),
	   (flags & _O_TRUNC ? "|O_TRUNC" : ""),
	   (flags & _O_EXCL ? "|O_EXCL" : ""),
	   (flags & _O_TEXT ? "|O_TEXT" : ""),
	   (flags & _O_BINARY ? "|O_BINARY" : ""));
}

static void
g_win32_print_gioflags (GIOFlags flags)
{
  char *bar = "";

  if (flags & G_IO_FLAG_APPEND)
    bar = "|", g_print ("APPEND");
  if (flags & G_IO_FLAG_NONBLOCK)
    g_print ("%sNONBLOCK", bar), bar = "|";
  if (flags & G_IO_FLAG_IS_READABLE)
    g_print ("%sREADABLE", bar), bar = "|";
  if (flags & G_IO_FLAG_IS_WRITEABLE)
    g_print ("%sWRITEABLE", bar), bar = "|";
  if (flags & G_IO_FLAG_IS_SEEKABLE)
    g_print ("%sSEEKABLE", bar), bar = "|";
}

static gboolean
g_io_win32_get_debug_flag (void)
{
#ifdef G_IO_WIN32_DEBUG
  return TRUE;
#else
  if (getenv ("G_IO_WIN32_DEBUG") != NULL)
    return TRUE;
  else
    return FALSE;
#endif
}  

static void
g_io_channel_win32_init (GIOWin32Channel *channel)
{
  channel->debug = g_io_win32_get_debug_flag ();
  channel->buffer = NULL;
  channel->running = FALSE;
  channel->needs_close = FALSE;
  channel->thread_id = 0;
  channel->data_avail_event = NULL;
  channel->revents = 0;
  channel->space_avail_event = NULL;
  channel->data_avail_noticed_event = NULL;
  channel->watches = NULL;
  InitializeCriticalSection (&channel->mutex);
}

static void
create_events (GIOWin32Channel *channel)
{
  SECURITY_ATTRIBUTES sec_attrs;
  
  sec_attrs.nLength = sizeof (SECURITY_ATTRIBUTES);
  sec_attrs.lpSecurityDescriptor = NULL;
  sec_attrs.bInheritHandle = FALSE;

  /* The data available event is manual reset, the space available event
   * is automatic reset.
   */
  if (!(channel->data_avail_event = CreateEvent (&sec_attrs, TRUE, FALSE, NULL))
      || !(channel->space_avail_event = CreateEvent (&sec_attrs, FALSE, FALSE, NULL))
      || !(channel->data_avail_noticed_event = CreateEvent (&sec_attrs, FALSE, FALSE, NULL)))
    {
      gchar *emsg = g_win32_error_message (GetLastError ());
      g_error ("Error creating event: %s", emsg);
      g_free (emsg);
    }
}

static unsigned __stdcall
read_thread (void *parameter)
{
  GIOWin32Channel *channel = parameter;
  guchar *buffer;
  guint nbytes;

  g_io_channel_ref ((GIOChannel *)channel);

  if (channel->debug)
    g_print ("read_thread %#x: start fd:%d, data_avail:%#x space_avail:%#x\n",
	     channel->thread_id,
	     channel->fd,
	     (guint) channel->data_avail_event,
	     (guint) channel->space_avail_event);
  
  channel->buffer = g_malloc (BUFFER_SIZE);
  channel->rdp = channel->wrp = 0;
  channel->running = TRUE;

  SetEvent (channel->space_avail_event);
  
  LOCK (channel->mutex);
  while (channel->running)
    {
      if (channel->debug)
	g_print ("read_thread %#x: rdp=%d, wrp=%d\n",
		 channel->thread_id, channel->rdp, channel->wrp);
      if ((channel->wrp + 1) % BUFFER_SIZE == channel->rdp)
	{
	  /* Buffer is full */
	  if (channel->debug)
	    g_print ("read_thread %#x: resetting space_avail\n",
		     channel->thread_id);
	  ResetEvent (channel->space_avail_event);
	  if (channel->debug)
	    g_print ("read_thread %#x: waiting for space\n",
		     channel->thread_id);
	  UNLOCK (channel->mutex);
	  WaitForSingleObject (channel->space_avail_event, INFINITE);
	  LOCK (channel->mutex);
	  if (channel->debug)
	    g_print ("read_thread %#x: rdp=%d, wrp=%d\n",
		     channel->thread_id, channel->rdp, channel->wrp);
	}
      
      buffer = channel->buffer + channel->wrp;
      
      /* Always leave at least one byte unused gap to be able to
       * distinguish between the full and empty condition...
       */
      nbytes = MIN ((channel->rdp + BUFFER_SIZE - channel->wrp - 1) % BUFFER_SIZE,
		    BUFFER_SIZE - channel->wrp);

      if (channel->debug)
	g_print ("read_thread %#x: calling read() for %d bytes\n",
		 channel->thread_id, nbytes);

      UNLOCK (channel->mutex);

      nbytes = read (channel->fd, buffer, nbytes);
      
      LOCK (channel->mutex);

      channel->revents = G_IO_IN;
      if (nbytes == 0)
	channel->revents |= G_IO_HUP;
      else if (nbytes < 0)
	channel->revents |= G_IO_ERR;

      if (channel->debug)
	g_print ("read_thread %#x: read() returned %d, rdp=%d, wrp=%d\n",
		 channel->thread_id, nbytes, channel->rdp, channel->wrp);

      if (nbytes <= 0)
	break;

      channel->wrp = (channel->wrp + nbytes) % BUFFER_SIZE;
      if (channel->debug)
	g_print ("read_thread %#x: rdp=%d, wrp=%d, setting data_avail\n",
		 channel->thread_id, channel->rdp, channel->wrp);
      SetEvent (channel->data_avail_event);
    }
  
  channel->running = FALSE;
  if (channel->needs_close)
    {
      if (channel->debug)
	g_print ("read_thread %#x: channel fd %d needs closing\n",
		 channel->thread_id, channel->fd);
      close (channel->fd);
      channel->fd = -1;
    }

  if (channel->debug)
    g_print ("read_thread %#x: EOF, rdp=%d, wrp=%d, setting data_avail\n",
	     channel->thread_id, channel->rdp, channel->wrp);
  SetEvent (channel->data_avail_event);
  UNLOCK (channel->mutex);
  
  g_io_channel_unref ((GIOChannel *)channel);
  
  /* No need to call _endthreadex(), the actual thread starter routine
   * in MSVCRT (see crt/src/threadex.c:_threadstartex) calls
   * _endthreadex() for us.
   */

  return 0;
}

static void
create_thread (GIOWin32Channel     *channel,
	       GIOCondition         condition,
	       unsigned (__stdcall *thread) (void *parameter))
{
  HANDLE thread_handle;

  thread_handle = (HANDLE) _beginthreadex (NULL, 0, thread, channel, 0,
					   &channel->thread_id);
  if (thread_handle == 0)
    g_warning (G_STRLOC ": Error creating reader thread: %s",
	       g_strerror (errno));
  else if (!CloseHandle (thread_handle))
    g_warning (G_STRLOC ": Error closing thread handle: %s\n",
	       g_win32_error_message (GetLastError ()));

  WaitForSingleObject (channel->space_avail_event, INFINITE);
}

static void
init_reset_sockets (GIOWin32Channel *channel)
{
  struct sockaddr_in local, local2, server;
  int len;

  channel->reset_send = (gint) socket (AF_INET, SOCK_DGRAM, 0);
  if (channel->reset_send == INVALID_SOCKET)
    {
      g_warning (G_STRLOC ": Error creating reset_send socket: %s\n",
		 g_win32_error_message (WSAGetLastError ()));
    }

  local.sin_family = AF_INET;
  local.sin_port = 0;
  local.sin_addr.s_addr = htonl (INADDR_LOOPBACK);

  if (bind (channel->reset_send, (struct sockaddr *)&local, sizeof (local)) == SOCKET_ERROR)
    {
      g_warning (G_STRLOC ": Error binding to reset_send socket: %s\n",
		 g_win32_error_message (WSAGetLastError ()));
  }

  local2.sin_family = AF_INET;
  local2.sin_port = 0;
  local2.sin_addr.s_addr = htonl (INADDR_LOOPBACK);

  channel->reset_recv = (gint) socket (AF_INET, SOCK_DGRAM, 0);
  if (channel->reset_recv == INVALID_SOCKET)
    {
      g_warning (G_STRLOC ": Error creating reset_recv socket: %s\n",
		 g_win32_error_message (WSAGetLastError ()));
  }

  if (bind (channel->reset_recv, (struct sockaddr *)&local2, sizeof (local)) == SOCKET_ERROR)
    {
      g_warning (G_STRLOC ": Error binding to reset_recv socket: %s\n",
		 g_win32_error_message (WSAGetLastError ()));
    }
  
  len = sizeof (local2);
  if (getsockname (channel->reset_recv, (struct sockaddr *)&local2, &len) == SOCKET_ERROR)
    {
      g_warning (G_STRLOC ": Error getsockname with reset_recv socket: %s\n",
		 g_win32_error_message (WSAGetLastError ()));
    }

  memset (&server, 0, sizeof (server));
  server.sin_addr.s_addr = htonl (INADDR_LOOPBACK);
  server.sin_family = AF_INET;
  server.sin_port = local2.sin_port;

  if (connect (channel->reset_send, (struct sockaddr  *)&server, sizeof (server)) == SOCKET_ERROR)
    {
      g_warning (G_STRLOC ": connect to reset_recv socket: %s\n",
		 g_win32_error_message (WSAGetLastError ()));
  }

}

static GIOStatus
buffer_read (GIOWin32Channel *channel,
	     guchar          *dest,
	     gsize            count,
	     gsize           *bytes_read,
	     GError         **err)
{
  guint nbytes;
  guint left = count;
  
  LOCK (channel->mutex);
  if (channel->debug)
    g_print ("reading from thread %#x %d bytes, rdp=%d, wrp=%d\n",
	     channel->thread_id, count, channel->rdp, channel->wrp);
  
  if (channel->wrp == channel->rdp)
    {
      UNLOCK (channel->mutex);
      if (channel->debug)
	g_print ("waiting for data from thread %#x\n", channel->thread_id);
      WaitForSingleObject (channel->data_avail_event, INFINITE);
      if (channel->debug)
	g_print ("done waiting for data from thread %#x\n", channel->thread_id);
      LOCK (channel->mutex);
      if (channel->wrp == channel->rdp && !channel->running)
	{
	  if (channel->debug)
	    g_print ("wrp==rdp, !running\n");
	  UNLOCK (channel->mutex);
          *bytes_read = 0;
	  return G_IO_STATUS_EOF;
	}
    }
  
  if (channel->rdp < channel->wrp)
    nbytes = channel->wrp - channel->rdp;
  else
    nbytes = BUFFER_SIZE - channel->rdp;
  UNLOCK (channel->mutex);
  nbytes = MIN (left, nbytes);
  if (channel->debug)
    g_print ("moving %d bytes from thread %#x\n",
	     nbytes, channel->thread_id);
  memcpy (dest, channel->buffer + channel->rdp, nbytes);
  dest += nbytes;
  left -= nbytes;
  LOCK (channel->mutex);
  channel->rdp = (channel->rdp + nbytes) % BUFFER_SIZE;
  if (channel->debug)
    g_print ("setting space_avail for thread %#x\n", channel->thread_id);
  SetEvent (channel->space_avail_event);
  if (channel->debug)
    g_print ("for thread %#x: rdp=%d, wrp=%d\n",
	     channel->thread_id, channel->rdp, channel->wrp);
  if (channel->running && channel->wrp == channel->rdp)
    {
      if (channel->debug)
	g_print ("resetting data_avail of thread %#x\n",
		 channel->thread_id);
      ResetEvent (channel->data_avail_event);
    };
  UNLOCK (channel->mutex);
  
  /* We have no way to indicate any errors form the actual
   * read() or recv() call in the reader thread. Should we have?
   */
  *bytes_read = count - left;
  return (*bytes_read > 0) ? G_IO_STATUS_NORMAL : G_IO_STATUS_EOF;
}

static unsigned __stdcall
select_thread (void *parameter)
{
  GIOWin32Channel *channel = parameter;
  fd_set read_fds, write_fds, except_fds;
  GSList *tmp;
  int n;
  char buffer[8];

  g_io_channel_ref ((GIOChannel *)channel);

  if (channel->debug)
    g_print ("select_thread %#x: start fd:%d data_avail:%#x data_avail_noticed:%#x\n",
	     channel->thread_id,
	     channel->fd,
	     (guint) channel->data_avail_event,
	     (guint) channel->data_avail_noticed_event);
  
  channel->rdp = channel->wrp = 0;
  channel->running = TRUE;

  SetEvent (channel->space_avail_event);
  
  while (channel->running)
    {
      FD_ZERO (&read_fds);
      FD_ZERO (&write_fds);
      FD_ZERO (&except_fds);
      FD_SET (channel->reset_recv, &read_fds);

      LOCK (channel->mutex);
      tmp = channel->watches;
      while (tmp)
	{
	  GIOWin32Watch *watch = (GIOWin32Watch *)tmp->data;

	  if (watch->condition & (G_IO_IN | G_IO_HUP))
	    FD_SET (channel->fd, &read_fds);
	  if (watch->condition & G_IO_OUT)
	    FD_SET (channel->fd, &write_fds);
	  if (watch->condition & G_IO_ERR)
	    FD_SET (channel->fd, &except_fds);
	  
	  tmp = tmp->next;
	}
      UNLOCK (channel->mutex);

      if (channel->debug)
	g_print ("select_thread %#x: calling select() for%s%s%s\n",
		 channel->thread_id,
		 (FD_ISSET (channel->fd, &read_fds) ? " IN" : ""),
		 (FD_ISSET (channel->fd, &write_fds) ? " OUT" : ""),
		 (FD_ISSET (channel->fd, &except_fds) ? " ERR" : ""));
      
      n = select (1, &read_fds, &write_fds, &except_fds, NULL);
      
      LOCK (channel->mutex);
      if (channel->needs_close)
	{
	  UNLOCK (channel->mutex);
	  break;
	}
      UNLOCK (channel->mutex);

      if (n == SOCKET_ERROR)
	{
	  if (channel->debug)
	    g_print ("select_thread %#x: select returned SOCKET_ERROR\n",
		     channel->thread_id);
	  break;
	}

    if (FD_ISSET (channel->reset_recv, &read_fds))
    {
      if (channel->debug)
        g_print ("select_thread %#x: re-looping\n",
            channel->thread_id);
      recv (channel->reset_recv,  (char *)&buffer, (int) sizeof (buffer), 0);
      continue;
    }

    if (channel->debug)
      g_print ("select_thread %#x: got%s%s%s\n",
	       channel->thread_id,
	       (FD_ISSET (channel->fd, &read_fds) ? " IN" : ""),
	       (FD_ISSET (channel->fd, &write_fds) ? " OUT" : ""),
	       (FD_ISSET (channel->fd, &except_fds) ? " ERR" : ""));
    
    if (FD_ISSET (channel->fd, &read_fds))
      channel->revents |= G_IO_IN;
    if (FD_ISSET (channel->fd, &write_fds))
      channel->revents |= G_IO_OUT;
    if (FD_ISSET (channel->fd, &except_fds))
      channel->revents |= G_IO_ERR;

    if (channel->debug)
      g_print ("select_thread %#x: resetting data_avail_noticed, setting data_avail\n",
	       channel->thread_id);

    LOCK (channel->mutex);
    ResetEvent (channel->data_avail_noticed_event);
    SetEvent (channel->data_avail_event);
    if (channel->needs_close)
      {
	UNLOCK (channel->mutex);
	break;
      }
    UNLOCK (channel->mutex);

    if (channel->debug)
      g_print ("select_thread %#x: waiting for data_avail_noticed\n",
        channel->thread_id);

    WaitForSingleObject (channel->data_avail_noticed_event, INFINITE);
    if (channel->debug)
      g_print ("select_thread %#x: got data_avail_noticed\n",
		 channel->thread_id);
    }

  LOCK (channel->mutex);
  channel->running = FALSE;
  if (channel->debug)
    g_print ("select_thread %#x: got error, setting data_avail\n",
	     channel->thread_id);
  SetEvent (channel->data_avail_event);
  g_io_channel_unref ((GIOChannel *)channel);
  UNLOCK (channel->mutex);

  /* No need to call _endthreadex(), the actual thread starter routine
   * in MSVCRT (see crt/src/threadex.c:_threadstartex) calls
   * _endthreadex() for us.
   */

  return 0;
}

static gboolean
g_io_win32_prepare (GSource *source,
		    gint    *timeout)
{
  GIOWin32Watch *watch = (GIOWin32Watch *)source;
  GIOCondition buffer_condition = g_io_channel_get_buffer_condition (watch->channel);
  GIOWin32Channel *channel = (GIOWin32Channel *)watch->channel;
  
  *timeout = -1;
  
  if (channel->debug)
    g_print ("g_io_win32_prepare: for thread %#x buffer_condition:%#x\n"
	     "  watch->pollfd.events:%#x watch->pollfd.revents:%#x channel->revents:%#x\n",
	     channel->thread_id, buffer_condition,
	     watch->pollfd.events, watch->pollfd.revents, channel->revents);

  if (channel->type == G_IO_WIN32_FILE_DESC)
    {
      LOCK (channel->mutex);
      if (channel->running && channel->wrp == channel->rdp)
	{
	  if (channel->debug)
	    g_print ("g_io_win32_prepare: for thread %#x, setting channel->revents = 0\n",
		     channel->thread_id);
	  channel->revents = 0;
	}
      UNLOCK (channel->mutex);
    }
  else if (channel->type == G_IO_WIN32_SOCKET)
    {
      LOCK (channel->mutex);
      channel->revents = 0;
      if (channel->debug)
	g_print ("g_io_win32_prepare: for thread %#x, setting data_avail_noticed\n",
		 channel->thread_id);
      SetEvent (channel->data_avail_noticed_event);
      if (channel->debug)
	g_print ("g_io_win32_prepare: thread %#x, there.\n",
		 channel->thread_id);
      UNLOCK (channel->mutex);
    }

  return ((watch->condition & buffer_condition) == watch->condition);
}

static gboolean
g_io_win32_check (GSource *source)
{
  MSG msg;
  GIOWin32Watch *watch = (GIOWin32Watch *)source;
  GIOWin32Channel *channel = (GIOWin32Channel *)watch->channel;
  GIOCondition buffer_condition = g_io_channel_get_buffer_condition (watch->channel);

  if (channel->debug)
    g_print ("g_io_win32_check: for thread %#x buffer_condition:%#x\n"
	     "  watch->pollfd.events:%#x watch->pollfd.revents:%#x channel->revents:%#x\n",
	     channel->thread_id, buffer_condition,
	     watch->pollfd.events, watch->pollfd.revents, channel->revents);

  if (channel->type != G_IO_WIN32_WINDOWS_MESSAGES)
    {
      watch->pollfd.revents = (watch->pollfd.events & channel->revents);
    }
  else
    {
      return (PeekMessage (&msg, channel->hwnd, 0, 0, PM_NOREMOVE));
    }
  
  if (channel->type == G_IO_WIN32_SOCKET)
    {
      LOCK (channel->mutex);
      if (channel->debug)
	g_print ("g_io_win32_check: thread %#x, resetting data_avail\n",
		 channel->thread_id);
      ResetEvent (channel->data_avail_event);
      if (channel->debug)
	g_print ("g_io_win32_check: thread %#x, there.\n",
		 channel->thread_id);
      UNLOCK (channel->mutex);
    }

  return ((watch->pollfd.revents | buffer_condition) & watch->condition);
}

static gboolean
g_io_win32_dispatch (GSource     *source,
		     GSourceFunc  callback,
		     gpointer     user_data)
{
  GIOFunc func = (GIOFunc)callback;
  GIOWin32Watch *watch = (GIOWin32Watch *)source;
  GIOCondition buffer_condition = g_io_channel_get_buffer_condition (watch->channel);
  
  if (!func)
    {
      g_warning (G_STRLOC ": GIOWin32Watch dispatched without callback\n"
		 "You must call g_source_connect().");
      return FALSE;
    }
  
  return (*func) (watch->channel,
		  (watch->pollfd.revents | buffer_condition) & watch->condition,
		  user_data);
}

static void
g_io_win32_finalize (GSource *source)
{
  GIOWin32Watch *watch = (GIOWin32Watch *)source;
  GIOWin32Channel *channel = (GIOWin32Channel *)watch->channel;
  char send_buffer[] = "f";
  
  LOCK (channel->mutex);
  if (channel->debug)
    g_print ("g_io_win32_finalize: channel with thread %#x\n",
	     channel->thread_id);

  channel->watches = g_slist_remove (channel->watches, watch);

  SetEvent (channel->data_avail_noticed_event);
  if (channel->type == G_IO_WIN32_SOCKET)
    send (channel->reset_send, send_buffer, sizeof (send_buffer), 0);

  g_io_channel_unref (watch->channel);
  UNLOCK (channel->mutex);
}

GSourceFuncs g_io_watch_funcs = {
  g_io_win32_prepare,
  g_io_win32_check,
  g_io_win32_dispatch,
  g_io_win32_finalize
};

static GSource *
g_io_win32_create_watch (GIOChannel    *channel,
			 GIOCondition   condition,
			 unsigned (__stdcall *thread) (void *parameter))
{
  GIOWin32Channel *win32_channel = (GIOWin32Channel *)channel;
  GIOWin32Watch *watch;
  GSource *source;
  char send_buffer[] = "c";

  source = g_source_new (&g_io_watch_funcs, sizeof (GIOWin32Watch));
  watch = (GIOWin32Watch *)source;
  
  watch->channel = channel;
  g_io_channel_ref (channel);
  
  watch->condition = condition;
  
  if (win32_channel->data_avail_event == NULL)
    create_events (win32_channel);

  watch->pollfd.fd = (gint) win32_channel->data_avail_event;
  watch->pollfd.events = condition;
  
  if (win32_channel->debug)
    g_print ("g_io_win32_create_watch: fd:%d condition:%#x handle:%#x\n",
	     win32_channel->fd, condition, watch->pollfd.fd);

  LOCK (win32_channel->mutex);
  win32_channel->watches = g_slist_append (win32_channel->watches, watch);

  if (win32_channel->thread_id == 0)
    create_thread (win32_channel, condition, thread);
  else
    send (win32_channel->reset_send, send_buffer, sizeof (send_buffer), 0);

  g_source_add_poll (source, &watch->pollfd);
  UNLOCK (win32_channel->mutex);

  return source;
}

static GIOStatus
g_io_win32_msg_read (GIOChannel *channel,
		     gchar      *buf,
		     gsize       count,
		     gsize      *bytes_read,
		     GError    **err)
{
  GIOWin32Channel *win32_channel = (GIOWin32Channel *)channel;
  MSG msg;               /* In case of alignment problems */
  
  if (count < sizeof (MSG))
    {
      g_set_error (err, G_IO_CHANNEL_ERROR, G_IO_CHANNEL_ERROR_INVAL,
		   "Incorrect message size"); /* Informative enough error message? */
      return G_IO_STATUS_ERROR;
    }
  
  if (win32_channel->debug)
    g_print ("g_io_win32_msg_read: for %#x\n",
	     (guint) win32_channel->hwnd);
  if (!PeekMessage (&msg, win32_channel->hwnd, 0, 0, PM_REMOVE))
    return G_IO_STATUS_AGAIN;

  memmove (buf, &msg, sizeof (MSG));
  *bytes_read = sizeof (MSG);

  return G_IO_STATUS_NORMAL;
}

static GIOStatus
g_io_win32_msg_write (GIOChannel  *channel,
		      const gchar *buf,
		      gsize        count,
		      gsize       *bytes_written,
		      GError     **err)
{
  GIOWin32Channel *win32_channel = (GIOWin32Channel *)channel;
  MSG msg;
  
  if (count != sizeof (MSG))
    {
      g_set_error (err, G_IO_CHANNEL_ERROR, G_IO_CHANNEL_ERROR_INVAL,
		   "Incorrect message size"); /* Informative enough error message? */
      return G_IO_STATUS_ERROR;
    }
  
  /* In case of alignment problems */
  memmove (&msg, buf, sizeof (MSG));
  if (!PostMessage (win32_channel->hwnd, msg.message, msg.wParam, msg.lParam))
    {
      gchar *emsg = g_win32_error_message (GetLastError ());
      g_set_error (err, G_IO_CHANNEL_ERROR, G_IO_CHANNEL_ERROR_FAILED, emsg);
      g_free (emsg);
      return G_IO_STATUS_ERROR;
    }

  *bytes_written = sizeof (MSG);

  return G_IO_STATUS_NORMAL;
}

static GIOStatus
g_io_win32_msg_close (GIOChannel *channel,
		      GError    **err)
{
  /* Nothing to be done. Or should we set hwnd to some invalid value? */

  return G_IO_STATUS_NORMAL;
}

static void
g_io_win32_free (GIOChannel *channel)
{
  GIOWin32Channel *win32_channel = (GIOWin32Channel *)channel;
  
  if (win32_channel->debug)
    g_print ("thread %#x: freeing channel, fd: %d\n",
	     win32_channel->thread_id,
	     win32_channel->fd);

  if (win32_channel->reset_send)
    closesocket (win32_channel->reset_send);
  if (win32_channel->reset_recv)
    closesocket (win32_channel->reset_recv);
  if (win32_channel->data_avail_event)
    CloseHandle (win32_channel->data_avail_event);
  if (win32_channel->space_avail_event)
    CloseHandle (win32_channel->space_avail_event);
  if (win32_channel->data_avail_noticed_event)
    CloseHandle (win32_channel->data_avail_noticed_event);
  DeleteCriticalSection (&win32_channel->mutex);

  g_free (win32_channel->buffer);
  g_slist_free (win32_channel->watches);
  g_free (win32_channel);
}

static GSource *
g_io_win32_msg_create_watch (GIOChannel    *channel,
			     GIOCondition   condition)
{
  GIOWin32Watch *watch;
  GSource *source;

  source = g_source_new (&g_io_watch_funcs, sizeof (GIOWin32Watch));
  watch = (GIOWin32Watch *)source;
  
  watch->channel = channel;
  g_io_channel_ref (channel);
  
  watch->condition = condition;
  
  watch->pollfd.fd = G_WIN32_MSG_HANDLE;
  watch->pollfd.events = condition;
  
  g_source_add_poll (source, &watch->pollfd);
  
  return source;
}

static GIOStatus
g_io_win32_fd_read (GIOChannel *channel,
		    gchar      *buf,
		    gsize       count,
		    gsize      *bytes_read,
		    GError    **err)
{
  GIOWin32Channel *win32_channel = (GIOWin32Channel *)channel;
  gint result;
  
  if (win32_channel->debug)
    g_print ("g_io_win32_fd_read: fd:%d count:%d\n",
	     win32_channel->fd, count);
  
  if (win32_channel->thread_id)
    {
      return buffer_read (win32_channel, buf, count, bytes_read, err);
    }

  result = read (win32_channel->fd, buf, count);

  if (win32_channel->debug)
    g_print ("g_io_win32_fd_read: read() = %d\n", result);

  if (result < 0)
    {
      *bytes_read = 0;

      switch (errno)
        {
#ifdef EAGAIN
	case EAGAIN:
	  return G_IO_STATUS_AGAIN;
#endif
	default:
	  g_set_error (err, G_IO_CHANNEL_ERROR,
		       g_io_channel_error_from_errno (errno),
		       g_strerror (errno));
	  return G_IO_STATUS_ERROR;
        }
    }

  *bytes_read = result;

  return (result > 0) ? G_IO_STATUS_NORMAL : G_IO_STATUS_EOF;
}

static GIOStatus
g_io_win32_fd_write (GIOChannel  *channel,
		     const gchar *buf,
		     gsize        count,
		     gsize       *bytes_written,
		     GError     **err)
{
  GIOWin32Channel *win32_channel = (GIOWin32Channel *)channel;
  gint result;
  
  result = write (win32_channel->fd, buf, count);
  if (win32_channel->debug)
    g_print ("g_io_win32_fd_write: fd:%d count:%d = %d\n",
	     win32_channel->fd, count, result);

  if (result < 0)
    {
      *bytes_written = 0;

      switch (errno)
        {
#ifdef EAGAIN
	case EAGAIN:
	  return G_IO_STATUS_AGAIN;
#endif
	default:
	  g_set_error (err, G_IO_CHANNEL_ERROR,
		       g_io_channel_error_from_errno (errno),
		       g_strerror (errno));
	  return G_IO_STATUS_ERROR;
        }
    }

  *bytes_written = result;

  return G_IO_STATUS_NORMAL;
}

static GIOStatus
g_io_win32_fd_seek (GIOChannel *channel,
		    gint64      offset,
		    GSeekType   type,
		    GError    **err)
{
  GIOWin32Channel *win32_channel = (GIOWin32Channel *)channel;
  int whence;
  off_t tmp_offset;
  off_t result;
  
  switch (type)
    {
    case G_SEEK_SET:
      whence = SEEK_SET;
      break;
    case G_SEEK_CUR:
      whence = SEEK_CUR;
      break;
    case G_SEEK_END:
      whence = SEEK_END;
      break;
    default:
      whence = -1; /* Keep the compiler quiet */
      g_assert_not_reached ();
    }

  tmp_offset = offset;
  if (tmp_offset != offset)
    {
      g_set_error (err, G_IO_CHANNEL_ERROR,
		   g_io_channel_error_from_errno (EINVAL),
		   g_strerror (EINVAL));
      return G_IO_STATUS_ERROR;
    }
  
  result = lseek (win32_channel->fd, tmp_offset, whence);
  
  if (result < 0)
    {
      g_set_error (err, G_IO_CHANNEL_ERROR,
		   g_io_channel_error_from_errno (errno),
		   g_strerror (errno));
      return G_IO_STATUS_ERROR;
    }

  return G_IO_STATUS_NORMAL;
}

static GIOStatus
g_io_win32_fd_close (GIOChannel *channel,
	             GError    **err)
{
  GIOWin32Channel *win32_channel = (GIOWin32Channel *)channel;
  
  if (win32_channel->debug)
    g_print ("thread %#x: closing fd %d\n",
	     win32_channel->thread_id,
	     win32_channel->fd);
  LOCK (win32_channel->mutex);
  if (win32_channel->running)
    {
      if (win32_channel->debug)
	g_print ("thread %#x: running, marking fd %d for later close\n",
		 win32_channel->thread_id, win32_channel->fd);
      win32_channel->running = FALSE;
      win32_channel->needs_close = TRUE;
      SetEvent (win32_channel->data_avail_event);
    }
  else
    {
      if (win32_channel->debug)
	g_print ("closing fd %d\n", win32_channel->fd);
      close (win32_channel->fd);
      if (win32_channel->debug)
	g_print ("closed fd %d, setting to -1\n",
		 win32_channel->fd);
      win32_channel->fd = -1;
    }
  UNLOCK (win32_channel->mutex);

  /* FIXME error detection? */

  return G_IO_STATUS_NORMAL;
}

static GSource *
g_io_win32_fd_create_watch (GIOChannel    *channel,
			    GIOCondition   condition)
{
  return g_io_win32_create_watch (channel, condition, read_thread);
}

static GIOStatus
g_io_win32_sock_read (GIOChannel *channel,
		      gchar      *buf,
		      gsize       count,
		      gsize      *bytes_read,
		      GError    **err)
{
  GIOWin32Channel *win32_channel = (GIOWin32Channel *)channel;
  gint result;
  GIOChannelError error = G_IO_STATUS_NORMAL;
  GIOStatus internal_status = G_IO_STATUS_NORMAL;
  char send_buffer[] = "sr";

  if (win32_channel->debug)
    g_print ("g_io_win32_sock_read: sockfd:%d count:%d\n",
	     win32_channel->fd, count);
#ifdef WE_NEED_TO_HANDLE_WSAEINTR
repeat:
#endif
  result = recv (win32_channel->fd, buf, count, 0);

  if (win32_channel->debug)
    g_print ("g_io_win32_sock_read: recv:%d\n", result);
  
  if (result == SOCKET_ERROR)
    {
      *bytes_read = 0;

      switch (WSAGetLastError ())
	{
	case WSAEINVAL:
          error = G_IO_CHANNEL_ERROR_INVAL;
          break;
	case WSAEWOULDBLOCK:
          return G_IO_STATUS_AGAIN;
#ifdef WE_NEED_TO_HANDLE_WSAEINTR /* not anymore with wsock2 ? */
	case WSAEINTR:
          goto repeat;
#endif
	default:
	  error = G_IO_CHANNEL_ERROR_FAILED;
          break;
	}
      g_set_error (err, G_IO_CHANNEL_ERROR, error, "Socket read error");
      internal_status = G_IO_STATUS_ERROR;
      /* FIXME get all errors, better error messages */
    }
  else
    {
      *bytes_read = result;
      if (result == 0)
	internal_status = G_IO_STATUS_EOF;
    }

  if ((internal_status == G_IO_STATUS_EOF) || 
      (internal_status == G_IO_STATUS_ERROR))
    {
      LOCK (win32_channel->mutex);
      SetEvent (win32_channel->data_avail_noticed_event);
      win32_channel->needs_close = 1;
      send (win32_channel->reset_send, send_buffer, sizeof (send_buffer), 0);
      UNLOCK (win32_channel->mutex);
    }
  return internal_status;
}

static GIOStatus
g_io_win32_sock_write (GIOChannel  *channel,
		       const gchar *buf,
		       gsize        count,
		       gsize       *bytes_written,
		       GError     **err)
{
  GIOWin32Channel *win32_channel = (GIOWin32Channel *)channel;
  gint result;
  GIOChannelError error = G_IO_STATUS_NORMAL;
  char send_buffer[] = "sw";
  
  if (win32_channel->debug)
    g_print ("g_io_win32_sock_write: sockfd:%d count:%d\n",
	     win32_channel->fd, count);
#ifdef WE_NEED_TO_HANDLE_WSAEINTR
repeat:
#endif
  result = send (win32_channel->fd, buf, count, 0);
  
  if (win32_channel->debug)
    g_print ("g_io_win32_sock_write: send:%d\n", result);
  
  if (result == SOCKET_ERROR)
    {
      *bytes_written = 0;

      switch (WSAGetLastError ())
	{
	case WSAEINVAL:
	  error = G_IO_CHANNEL_ERROR_INVAL;
          break;
	case WSAEWOULDBLOCK:
          return G_IO_STATUS_AGAIN;
#ifdef WE_NEED_TO_HANDLE_WSAEINTR /* not anymore with wsock2 ? */
	case WSAEINTR:
          goto repeat;
#endif
	default:
	  error = G_IO_CHANNEL_ERROR_FAILED;
          break;
	}
      g_set_error (err, G_IO_CHANNEL_ERROR, error, "Socket write error");
      LOCK (win32_channel->mutex);
      SetEvent (win32_channel->data_avail_noticed_event);
      win32_channel->needs_close = 1;
      send (win32_channel->reset_send, send_buffer, sizeof (send_buffer), 0);
      UNLOCK (win32_channel->mutex);
      return G_IO_STATUS_ERROR;
      /* FIXME get all errors, better error messages */
    }
  else
    {
      *bytes_written = result;

      return G_IO_STATUS_NORMAL;
    }
}

static GIOStatus
g_io_win32_sock_close (GIOChannel *channel,
		       GError    **err)
{
  GIOWin32Channel *win32_channel = (GIOWin32Channel *)channel;

  LOCK (win32_channel->mutex);
  if (win32_channel->running)
    {
      if (win32_channel->debug)
	g_print ("thread %#x: running, marking for later close\n",
		 win32_channel->thread_id);
      win32_channel->running = FALSE;
      win32_channel->needs_close = TRUE;
      SetEvent(win32_channel->data_avail_noticed_event);
    }
  if (win32_channel->fd != -1)
    {
      if (win32_channel->debug)
	g_print ("thread %#x: closing socket %d\n",
		 win32_channel->thread_id,
		 win32_channel->fd);
      
      closesocket (win32_channel->fd);
      win32_channel->fd = -1;
    }
  UNLOCK (win32_channel->mutex);

  /* FIXME error detection? */

  return G_IO_STATUS_NORMAL;
}

static GSource *
g_io_win32_sock_create_watch (GIOChannel    *channel,
			      GIOCondition   condition)
{
  return g_io_win32_create_watch (channel, condition, select_thread);
}

GIOChannel *
g_io_channel_new_file (const gchar  *filename,
                       const gchar  *mode,
                       GError      **error)
{
  int fid, flags, pmode;
  GIOChannel *channel;

  enum { /* Cheesy hack */
    MODE_R = 1 << 0,
    MODE_W = 1 << 1,
    MODE_A = 1 << 2,
    MODE_PLUS = 1 << 3,
  } mode_num;

  g_return_val_if_fail (filename != NULL, NULL);
  g_return_val_if_fail (mode != NULL, NULL);
  g_return_val_if_fail ((error == NULL) || (*error == NULL), NULL);

  switch (mode[0])
    {
      case 'r':
        mode_num = MODE_R;
        break;
      case 'w':
        mode_num = MODE_W;
        break;
      case 'a':
        mode_num = MODE_A;
        break;
      default:
        g_warning ("Invalid GIOFileMode %s.\n", mode);
        return NULL;
    }

  switch (mode[1])
    {
      case '\0':
        break;
      case '+':
        if (mode[2] == '\0')
          {
            mode_num |= MODE_PLUS;
            break;
          }
        /* Fall through */
      default:
        g_warning ("Invalid GIOFileMode %s.\n", mode);
        return NULL;
    }

  switch (mode_num)
    {
      case MODE_R:
        flags = O_RDONLY;
        pmode = _S_IREAD;
        break;
      case MODE_W:
        flags = O_WRONLY | O_TRUNC | O_CREAT;
        pmode = _S_IWRITE;
        break;
      case MODE_A:
        flags = O_WRONLY | O_APPEND | O_CREAT;
        pmode = _S_IWRITE;
        break;
      case MODE_R | MODE_PLUS:
        flags = O_RDWR;
        pmode = _S_IREAD | _S_IWRITE;
        break;
      case MODE_W | MODE_PLUS:
        flags = O_RDWR | O_TRUNC | O_CREAT;
        pmode = _S_IREAD | _S_IWRITE;
        break;
      case MODE_A | MODE_PLUS:
        flags = O_RDWR | O_APPEND | O_CREAT;
        pmode = _S_IREAD | _S_IWRITE;
        break;
      default:
        g_assert_not_reached ();
        flags = 0;
        pmode = 0;
    }

  /* always open 'untranslated' */
  fid = open (filename, flags | _O_BINARY, pmode);

  if (g_io_win32_get_debug_flag ())
    {
      g_print ("g_io_channel_win32_new_file: open(\"%s\", ", filename);
      g_win32_print_access_mode (flags|_O_BINARY);
      g_print (",%#o)=%d\n", pmode, fid);
    }

  if (fid < 0)
    {
      g_set_error (error, G_FILE_ERROR,
                   g_file_error_from_errno (errno),
                   g_strerror (errno));
      return (GIOChannel *)NULL;
    }

  channel = g_io_channel_win32_new_fd (fid);

  /* XXX: move this to g_io_channel_win32_new_fd () */
  channel->close_on_unref = TRUE;
  channel->is_seekable = TRUE;

  /* g_io_channel_win32_new_fd sets is_readable and is_writeable to
   * correspond to actual readability/writeability. Set to FALSE those
   * that mode doesn't allow
   */
  switch (mode_num)
    {
      case MODE_R:
        channel->is_writeable = FALSE;
        break;
      case MODE_W:
      case MODE_A:
        channel->is_readable = FALSE;
        break;
      case MODE_R | MODE_PLUS:
      case MODE_W | MODE_PLUS:
      case MODE_A | MODE_PLUS:
        break;
      default:
        g_assert_not_reached ();
    }

  return channel;
}

static GIOStatus
g_io_win32_set_flags (GIOChannel *channel,
                      GIOFlags    flags,
                      GError    **err)
{
  GIOWin32Channel *win32_channel = (GIOWin32Channel *)channel;

  if (win32_channel->debug)
    {
      g_print ("g_io_win32_set_flags: ");
      g_win32_print_gioflags (flags);
      g_print ("\n");
    }

  g_warning ("g_io_win32_set_flags () not implemented.\n");

  return G_IO_STATUS_NORMAL;
}

static GIOFlags
g_io_win32_fd_get_flags_internal (GIOChannel  *channel,
				  struct stat *st)
{
  GIOWin32Channel *win32_channel = (GIOWin32Channel *) channel;
  gchar c;
  DWORD count;

  if (st->st_mode & _S_IFIFO)
    {
      channel->is_readable =
	(PeekNamedPipe ((HANDLE) _get_osfhandle (win32_channel->fd), &c, 0, &count, NULL, NULL) != 0) || GetLastError () == ERROR_BROKEN_PIPE;
      channel->is_writeable =
	(WriteFile ((HANDLE) _get_osfhandle (win32_channel->fd), &c, 0, &count, NULL) != 0);
      channel->is_seekable  = FALSE;
    }
  else if (st->st_mode & _S_IFCHR)
    {
      /* XXX Seems there is no way to find out the readability of file
       * handles to device files (consoles, mostly) without doing a
       * blocking read. So punt, use st->st_mode.
       */
      channel->is_readable  = !!(st->st_mode & _S_IREAD);

      channel->is_writeable =
	(WriteFile ((HANDLE) _get_osfhandle (win32_channel->fd), &c, 0, &count, NULL) != 0);

      /* XXX What about devices that actually *are* seekable? But
       * those would probably not be handled using the C runtime
       * anyway, but using Windows-specific code.
       */
      channel->is_seekable = FALSE;
    }
  else
    {
      channel->is_readable =
	(ReadFile ((HANDLE) _get_osfhandle (win32_channel->fd), &c, 0, &count, NULL) != 0);
      channel->is_writeable =
	(WriteFile ((HANDLE) _get_osfhandle (win32_channel->fd), &c, 0, &count, NULL) != 0);
      channel->is_seekable = TRUE;
    }

  /* XXX: G_IO_FLAG_APPEND */
  /* XXX: G_IO_FLAG_NONBLOCK */

  return 0;
}

static GIOFlags
g_io_win32_fd_get_flags (GIOChannel *channel)
{
  struct stat st;
  GIOWin32Channel *win32_channel = (GIOWin32Channel *)channel;

  g_return_val_if_fail (win32_channel != NULL, 0);
  g_return_val_if_fail (win32_channel->type == G_IO_WIN32_FILE_DESC, 0);

  if (0 == fstat (win32_channel->fd, &st))
    return g_io_win32_fd_get_flags_internal (channel, &st);
  else
    return 0;
}

static GIOFlags
g_io_win32_msg_get_flags (GIOChannel *channel)
{
  return 0;
}

static GIOFlags
g_io_win32_sock_get_flags (GIOChannel *channel)
{
  /* XXX Could do something here. */
  return 0;
}

static GIOFuncs win32_channel_msg_funcs = {
  g_io_win32_msg_read,
  g_io_win32_msg_write,
  NULL,
  g_io_win32_msg_close,
  g_io_win32_msg_create_watch,
  g_io_win32_free,
  g_io_win32_set_flags,
  g_io_win32_msg_get_flags,
};

static GIOFuncs win32_channel_fd_funcs = {
  g_io_win32_fd_read,
  g_io_win32_fd_write,
  g_io_win32_fd_seek,
  g_io_win32_fd_close,
  g_io_win32_fd_create_watch,
  g_io_win32_free,
  g_io_win32_set_flags,
  g_io_win32_fd_get_flags,
};

static GIOFuncs win32_channel_sock_funcs = {
  g_io_win32_sock_read,
  g_io_win32_sock_write,
  NULL,
  g_io_win32_sock_close,
  g_io_win32_sock_create_watch,
  g_io_win32_free,
  g_io_win32_set_flags,
  g_io_win32_sock_get_flags,
};

GIOChannel *
g_io_channel_win32_new_messages (guint hwnd)
{
  GIOWin32Channel *win32_channel = g_new (GIOWin32Channel, 1);
  GIOChannel *channel = (GIOChannel *)win32_channel;

  g_io_channel_init (channel);
  g_io_channel_win32_init (win32_channel);
  if (win32_channel->debug)
    g_print ("g_io_channel_win32_new_messages: hwnd = %ud\n", hwnd);
  channel->funcs = &win32_channel_msg_funcs;
  win32_channel->type = G_IO_WIN32_WINDOWS_MESSAGES;
  win32_channel->hwnd = (HWND) hwnd;

  /* XXX: check this. */
  channel->is_readable = IsWindow (win32_channel->hwnd);
  channel->is_writeable = IsWindow (win32_channel->hwnd);

  channel->is_seekable = FALSE;

  return channel;
}

static GIOChannel *
g_io_channel_win32_new_fd_internal (gint         fd,
				    struct stat *st)
{
  GIOWin32Channel *win32_channel;
  GIOChannel *channel;

  win32_channel = g_new (GIOWin32Channel, 1);
  channel = (GIOChannel *)win32_channel;

  g_io_channel_init (channel);
  g_io_channel_win32_init (win32_channel);
  if (win32_channel->debug)
    g_print ("g_io_channel_win32_new_fd: %u\n", fd);
  channel->funcs = &win32_channel_fd_funcs;
  win32_channel->type = G_IO_WIN32_FILE_DESC;
  win32_channel->fd = fd;

  g_io_win32_fd_get_flags_internal (channel, st);
  
  return channel;
}

GIOChannel *
g_io_channel_win32_new_fd (gint fd)
{
  struct stat st;

  if (fstat (fd, &st) == -1)
    {
      g_warning (G_STRLOC ": %d isn't a C library file descriptor", fd);
      return NULL;
    }

  return g_io_channel_win32_new_fd_internal (fd, &st);
}

gint
g_io_channel_win32_get_fd (GIOChannel *channel)
{
  GIOWin32Channel *win32_channel = (GIOWin32Channel *)channel;

  return win32_channel->fd;
}

GIOChannel *
g_io_channel_win32_new_socket (int socket)
{
  GIOWin32Channel *win32_channel = g_new (GIOWin32Channel, 1);
  GIOChannel *channel = (GIOChannel *)win32_channel;

  g_io_channel_init (channel);
  g_io_channel_win32_init (win32_channel);
  init_reset_sockets (win32_channel);
  if (win32_channel->debug)
    g_print ("g_io_channel_win32_new_socket: sockfd:%d\n", socket);
  channel->funcs = &win32_channel_sock_funcs;
  win32_channel->type = G_IO_WIN32_SOCKET;
  win32_channel->fd = socket;

  /* XXX: check this */
  channel->is_readable = TRUE;
  channel->is_writeable = TRUE;

  channel->is_seekable = FALSE;

  return channel;
}

GIOChannel *
g_io_channel_unix_new (gint fd)
{
  gboolean is_fd, is_socket;
  struct stat st;
  int optval, optlen;

  is_fd = (fstat (fd, &st) == 0);

  optlen = sizeof (optval);
  is_socket = (getsockopt (fd, SOL_SOCKET, SO_TYPE, (char *) &optval, &optlen) != SOCKET_ERROR);

  if (is_fd && is_socket)
    g_warning (G_STRLOC ": %d is both a file descriptor and a socket, file descriptor interpretation assumed.", fd);

  if (is_fd)
    return g_io_channel_win32_new_fd_internal (fd, &st);

  if (is_socket)
    return g_io_channel_win32_new_socket(fd);

  g_warning (G_STRLOC ": %d is neither a file descriptor or a socket", fd);

  return NULL;
}

gint
g_io_channel_unix_get_fd (GIOChannel *channel)
{
  return g_io_channel_win32_get_fd (channel);
}

void
g_io_channel_win32_set_debug (GIOChannel *channel,
			      gboolean    flag)
{
  GIOWin32Channel *win32_channel = (GIOWin32Channel *)channel;

  win32_channel->debug = flag;
}

gint
g_io_channel_win32_poll (GPollFD *fds,
			 gint     n_fds,
			 gint     timeout)
{
  int result;

  g_return_val_if_fail (n_fds >= 0, 0);

  result = (*g_main_context_get_poll_func (NULL)) (fds, n_fds, timeout);

  return result;
}

void
g_io_channel_win32_make_pollfd (GIOChannel   *channel,
				GIOCondition  condition,
				GPollFD      *fd)
{
  GIOWin32Channel *win32_channel = (GIOWin32Channel *)channel;

  if (win32_channel->data_avail_event == NULL)
    create_events (win32_channel);
  
  fd->fd = (gint) win32_channel->data_avail_event;
  fd->events = condition;

  if (win32_channel->thread_id == 0)
    {
      if ((condition & G_IO_IN) && win32_channel->type == G_IO_WIN32_FILE_DESC)
	create_thread (win32_channel, condition, read_thread);
      else if (win32_channel->type == G_IO_WIN32_SOCKET)
	create_thread (win32_channel, condition, select_thread);
    }
}

/* Binary compatibility */
GIOChannel *
g_io_channel_win32_new_stream_socket (int socket)
{
  return g_io_channel_win32_new_socket (socket);
}
