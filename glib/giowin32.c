/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * giowin32.c: IO Channels for Win32.
 * Copyright 1998 Owen Taylor and Tor Lillqvist
 * Copyright 1999-2000 Tor Lillqvist and Craig Setera
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

#include "glib.h"

#include <stdlib.h>
#include <windows.h>
#include <winsock.h>          /* Not everybody has winsock2 */
#include <fcntl.h>
#include <io.h>
#include <process.h>
#include <errno.h>
#include <sys/stat.h>

typedef struct _GIOWin32Channel GIOWin32Channel;
typedef struct _GIOWin32Watch GIOWin32Watch;

#define BUFFER_SIZE 4096

typedef enum {
  G_IO_WINDOWS_MESSAGES,	/* Windows messages */
  G_IO_FILE_DESC,		/* Unix-like file descriptors from
				 * _open() or _pipe(). Read with read().
				 * Have to create separate thread to read.
				 */
  G_IO_STREAM_SOCKET		/* Stream sockets. Similar as fds, but
				 * read with recv().
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

  /* This is used by G_IO_WINDOWS_MESSAGES channels */
  HWND hwnd;			/* handle of window, or NULL */
  
  /* Following fields used by fd and socket channels for input */
  
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
  gboolean running;		/* Is reader thread running. FALSE if
				 * EOF has been reached.
				 */
  gboolean needs_close;		/* If the channel has been closed while
				 * the reader thread was still running.
				 */
  guint thread_id;		/* If non-NULL has a reader thread, or has
				 * had.*/
  HANDLE thread_handle;
  HANDLE data_avail_event;
  HANDLE space_avail_event;
  CRITICAL_SECTION mutex;
  
  /* Function that actually reads from fd */
  int (*reader) (int fd, guchar *buf, int len);
};

#define LOCK(mutex) EnterCriticalSection (&mutex)
#define UNLOCK(mutex) LeaveCriticalSection (&mutex)

struct _GIOWin32Watch {
  GSource       source;
  GPollFD       pollfd;
  GIOChannel   *channel;
  GIOCondition  condition;
  GIOFunc       callback;
};

static void
g_io_channel_win32_init (GIOWin32Channel *channel)
{
#ifdef G_IO_WIN32_DEBUG
  channel->debug = TRUE;
#else
  if (getenv ("G_IO_WIN32_DEBUG") != NULL)
    channel->debug = TRUE;
  else
    channel->debug = FALSE;
#endif
  channel->buffer = NULL;
  channel->running = FALSE;
  channel->needs_close = FALSE;
  channel->thread_id = 0;
  channel->data_avail_event = NULL;
  channel->space_avail_event = NULL;
  InitializeCriticalSection (&channel->mutex);
}

static void
create_events (GIOWin32Channel *channel)
{
  SECURITY_ATTRIBUTES sec_attrs;
  
  sec_attrs.nLength = sizeof(SECURITY_ATTRIBUTES);
  sec_attrs.lpSecurityDescriptor = NULL;
  sec_attrs.bInheritHandle = FALSE;

  /* The data available event is manual reset, the space available event
   * is automatic reset.
   */
  if (!(channel->data_avail_event = CreateEvent (&sec_attrs, TRUE, FALSE, NULL))
      || !(channel->space_avail_event = CreateEvent (&sec_attrs, FALSE, FALSE, NULL)))
    {
      gchar *msg = g_win32_error_message (GetLastError ());
      g_error ("Error creating event: %s", msg);
    }
}

static unsigned __stdcall
reader_thread (void *parameter)
{
  GIOWin32Channel *channel = parameter;
  guchar *buffer;
  guint nbytes;

  g_io_channel_ref ((GIOChannel *) channel);

  if (channel->debug)
    g_print ("thread %#x: starting. pid:%#x, fd:%d, data_avail:%#x, space_avail:%#x\n",
	     channel->thread_id,
	     (guint) GetCurrentProcessId (),
	     channel->fd,
	     (guint) channel->data_avail_event,
	     (guint) channel->space_avail_event);
  
  channel->buffer = g_malloc (BUFFER_SIZE);
  channel->rdp = channel->wrp = 0;
  channel->running = TRUE;

  SetEvent (channel->space_avail_event);
  
  while (channel->running)
    {
      LOCK (channel->mutex);
      if (channel->debug)
	g_print ("thread %#x: rdp=%d, wrp=%d\n",
		 channel->thread_id, channel->rdp, channel->wrp);
      if ((channel->wrp + 1) % BUFFER_SIZE == channel->rdp)
	{
	  /* Buffer is full */
	  if (channel->debug)
	    g_print ("thread %#x: resetting space_available\n",
		     channel->thread_id);
	  ResetEvent (channel->space_avail_event);
	  if (channel->debug)
	    g_print ("thread %#x: waiting for space\n", channel->thread_id);
	  UNLOCK (channel->mutex);
	  WaitForSingleObject (channel->space_avail_event, INFINITE);
	  LOCK (channel->mutex);
	  if (channel->debug)
	    g_print ("thread %#x: rdp=%d, wrp=%d\n",
		     channel->thread_id, channel->rdp, channel->wrp);
	}
      
      buffer = channel->buffer + channel->wrp;
      
      /* Always leave at least one byte unused gap to be able to
       * distinguish between the full and empty condition...
       */
      nbytes = MIN ((channel->rdp + BUFFER_SIZE - channel->wrp - 1) % BUFFER_SIZE,
		    BUFFER_SIZE - channel->wrp);

      if (channel->debug)
	g_print ("thread %#x: calling reader for %d bytes\n",
		 channel->thread_id, nbytes);

      UNLOCK (channel->mutex);

      nbytes = (*channel->reader) (channel->fd, buffer, nbytes);
      
      LOCK (channel->mutex);

      if (channel->debug)
	g_print ("thread %#x: got %d bytes, rdp=%d, wrp=%d\n",
		 channel->thread_id, nbytes, channel->rdp, channel->wrp);

      if (nbytes <= 0)
	break;

      channel->wrp = (channel->wrp + nbytes) % BUFFER_SIZE;
      if (channel->debug)
	g_print ("thread %#x: rdp=%d, wrp=%d, setting data available\n",
		 channel->thread_id, channel->rdp, channel->wrp);
      SetEvent (channel->data_avail_event);
      UNLOCK (channel->mutex);
    }
  
  channel->running = FALSE;
  if (channel->debug)
    g_print ("thread %#x: got EOF, rdp=%d, wrp=%d, setting data available\n",
	     channel->thread_id, channel->rdp, channel->wrp);

  if (channel->needs_close)
    {
      if (channel->debug)
	g_print ("thread %#x: channel fd %d needs closing\n",
		 channel->thread_id, channel->fd);
      if (channel->type == G_IO_FILE_DESC)
	close (channel->fd);
      else if (channel->type == G_IO_STREAM_SOCKET)
	closesocket (channel->fd);
      channel->fd = -1;
    }

  SetEvent (channel->data_avail_event);
  UNLOCK (channel->mutex);
  
  g_io_channel_unref((GIOChannel *) channel);
  
  /* No need to call _endthreadex(), the actual thread starter routine
   * in MSVCRT (see crt/src/threadex.c:_threadstartex) calls
   * _endthreadex() for us.
   */

  CloseHandle (channel->thread_handle);

  return 0;
}

static void
create_reader_thread (GIOWin32Channel *channel,
		      gpointer         reader)
{
  channel->reader = reader;

  if ((channel->thread_handle =
       (HANDLE) _beginthreadex (NULL, 0, reader_thread, channel, 0,
				&channel->thread_id)) == 0)
    g_warning ("Error creating reader thread: %s", strerror (errno));
  WaitForSingleObject (channel->space_avail_event, INFINITE);
}

static int
buffer_read (GIOWin32Channel *channel,
	     guchar          *dest,
	     guint            count,
	     GIOError        *error)
{
  guint nbytes;
  guint left = count;
  
  LOCK (channel->mutex);
  if (channel->debug)
    g_print ("reading from thread %#x %d bytes, rdp=%d, wrp=%d\n",
	     channel->thread_id, count, channel->rdp, channel->wrp);
  
  if (channel->rdp == channel->wrp)
    {
      UNLOCK (channel->mutex);
      if (channel->debug)
	g_print ("waiting for data from thread %#x\n", channel->thread_id);
      WaitForSingleObject (channel->data_avail_event, INFINITE);
      if (channel->debug)
	g_print ("done waiting for data from thread %#x\n", channel->thread_id);
      LOCK (channel->mutex);
      if (channel->rdp == channel->wrp && !channel->running)
	{
	  UNLOCK (channel->mutex);
	  return 0;
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
    g_print ("setting space available for thread %#x\n", channel->thread_id);
  SetEvent (channel->space_avail_event);
  if (channel->debug)
    g_print ("for thread %#x: rdp=%d, wrp=%d\n",
	     channel->thread_id, channel->rdp, channel->wrp);
  if (channel->running && channel->rdp == channel->wrp)
    {
      if (channel->debug)
	g_print ("resetting data_available of thread %#x\n",
		 channel->thread_id);
      ResetEvent (channel->data_avail_event);
    };
  UNLOCK (channel->mutex);
  
  /* We have no way to indicate any errors form the actual
   * read() or recv() call in the reader thread. Should we have?
   */
  *error = G_IO_ERROR_NONE;
  return count - left;
}

static gboolean
g_io_win32_prepare (GSource *source,
		    gint    *timeout)
{
  *timeout = -1;
  
  return FALSE;
}

static gboolean
g_io_win32_check (GSource *source)
{
  GIOWin32Watch *watch = (GIOWin32Watch *)source;
  GIOWin32Channel *channel = (GIOWin32Channel *) watch->channel;
  
  /* If the thread has died, we have encountered EOF. If the buffer
   * also is emtpty set the HUP bit.
   */
  if (!channel->running && channel->rdp == channel->wrp)
    {
      if (channel->debug)
	g_print ("g_io_win32_check: setting G_IO_HUP thread %#x rdp=%d wrp=%d\n",
		 channel->thread_id, channel->rdp, channel->wrp);
      watch->pollfd.revents |= G_IO_HUP;
      return TRUE;
    }
  
  return (watch->pollfd.revents & watch->condition);
}

static gboolean
g_io_win32_dispatch (GSource     *source,
		     GSourceFunc  callback,
		     gpointer     user_data)
{
  GIOFunc func = (GIOFunc)callback;
  GIOWin32Watch *watch = (GIOWin32Watch *)source;
  
  if (!func)
    {
      g_warning ("GIOWin32Watch dispatched without callback\n"
		 "You must call g_source_connect().");
      return FALSE;
    }
  
  return (*func) (watch->channel,
		  watch->pollfd.revents & watch->condition,
		  user_data);
}

static void
g_io_win32_destroy (GSource *source)
{
  GIOWin32Watch *watch = (GIOWin32Watch *)source;
  
  g_io_channel_unref (watch->channel);
}

static GSourceFuncs win32_watch_funcs = {
  g_io_win32_prepare,
  g_io_win32_check,
  g_io_win32_dispatch,
  g_io_win32_destroy
};

static GSource *
g_io_win32_create_watch (GIOChannel    *channel,
			 GIOCondition   condition,
			 int (*reader) (int, guchar *, int))
{
  GIOWin32Channel *win32_channel = (GIOWin32Channel *) channel;
  GIOWin32Watch *watch;
  GSource *source;

  source = g_source_new (&win32_watch_funcs, sizeof (GIOWin32Watch));
  watch = (GIOWin32Watch *)source;
  
  watch->channel = channel;
  g_io_channel_ref (channel);
  
  watch->condition = condition;
  
  if (win32_channel->data_avail_event == NULL)
    create_events (win32_channel);

  watch->pollfd.fd = (gint) win32_channel->data_avail_event;
  watch->pollfd.events = condition;
  
  if (win32_channel->debug)
    g_print ("g_io_win32_create_watch: fd:%d handle:%#x\n",
	     win32_channel->fd, watch->pollfd.fd);
  
  if (win32_channel->thread_id == 0)
    create_reader_thread (win32_channel, reader);

  g_source_add_poll (source, &watch->pollfd);
  
  return source;
}

static GIOError
g_io_win32_msg_read (GIOChannel *channel,
		     gchar      *buf,
		     guint       count,
		     guint      *bytes_read)
{
  GIOWin32Channel *win32_channel = (GIOWin32Channel *) channel;
  MSG msg;               /* In case of alignment problems */
  
  if (count < sizeof (MSG))
    return G_IO_ERROR_INVAL;
  
  if (!PeekMessage (&msg, win32_channel->hwnd, 0, 0, PM_REMOVE))
    return G_IO_ERROR_AGAIN;
  
  memmove (buf, &msg, sizeof (MSG));
  *bytes_read = sizeof (MSG);
  return G_IO_ERROR_NONE;
}

static GIOError
g_io_win32_msg_write (GIOChannel *channel,
		      gchar      *buf,
		      guint       count,
		      guint      *bytes_written)
{
  GIOWin32Channel *win32_channel = (GIOWin32Channel *) channel;
  MSG msg;
  
  if (count != sizeof (MSG))
    return G_IO_ERROR_INVAL;
  
  /* In case of alignment problems */
  memmove (&msg, buf, sizeof (MSG));
  if (!PostMessage (win32_channel->hwnd, msg.message, msg.wParam, msg.lParam))
    return G_IO_ERROR_UNKNOWN;
  
  *bytes_written = sizeof (MSG);
  return G_IO_ERROR_NONE;
}

static GIOError
g_io_win32_no_seek (GIOChannel *channel,
		    gint        offset,
		    GSeekType   type)
{
  return G_IO_ERROR_UNKNOWN;
}

static void
g_io_win32_msg_close (GIOChannel *channel)
{
  /* Nothing to be done. Or should we set hwnd to some invalid value? */
}

static void
g_io_win32_free (GIOChannel *channel)
{
  GIOWin32Channel *win32_channel = (GIOWin32Channel *) channel;
  
  if (win32_channel->debug)
    g_print ("thread %#x: freeing channel, fd: %d\n",
	     win32_channel->thread_id,
	     win32_channel->fd);

  if (win32_channel->buffer)
    {
      CloseHandle (win32_channel->data_avail_event);
      CloseHandle (win32_channel->space_avail_event);
      DeleteCriticalSection (&win32_channel->mutex);
    }

  g_free (win32_channel->buffer);
  g_free (win32_channel);
}

static GSource *
g_io_win32_msg_create_watch (GIOChannel    *channel,
			     GIOCondition   condition)
{
  GIOWin32Watch *watch;
  GSource *source;

  source = g_source_new (&win32_watch_funcs, sizeof (GIOWin32Watch));
  watch = (GIOWin32Watch *)source;
  
  watch->channel = channel;
  g_io_channel_ref (channel);
  
  watch->condition = condition;
  
  watch->pollfd.fd = G_WIN32_MSG_HANDLE;
  watch->pollfd.events = condition;
  
  g_source_add_poll (source, &watch->pollfd);
  
  return source;
}

static GIOError
g_io_win32_fd_read (GIOChannel *channel,
		    gchar      *buf,
		    guint       count,
		    guint      *bytes_read)
{
  GIOWin32Channel *win32_channel = (GIOWin32Channel *) channel;
  gint result;
  GIOError error;
  
  if (win32_channel->debug)
    g_print ("g_io_win32_fd_read: fd:%d count:%d\n",
	     win32_channel->fd, count);
  
  if (win32_channel->thread_id)
    {
      result = buffer_read (win32_channel, buf, count, &error);
      if (result < 0)
	{
	  *bytes_read = 0;
	  return error;
	}
      else
	{
	  *bytes_read = result;
	  return G_IO_ERROR_NONE;
	}
    }

  result = read (win32_channel->fd, buf, count);

  if (result < 0)
    {
      *bytes_read = 0;
      if (errno == EINVAL)
	return G_IO_ERROR_INVAL;
      else
	return G_IO_ERROR_UNKNOWN;
    }
  else
    {
      *bytes_read = result;
      return G_IO_ERROR_NONE;
    }
}

static GIOError
g_io_win32_fd_write (GIOChannel *channel,
		     gchar      *buf,
		     guint       count,
		     guint      *bytes_written)
{
  GIOWin32Channel *win32_channel = (GIOWin32Channel *) channel;
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
	case EINVAL:
	  return G_IO_ERROR_INVAL;
	case EAGAIN:
	  return G_IO_ERROR_AGAIN;
	default:
	  return G_IO_ERROR_UNKNOWN;
	}
    }
  else
    {
      *bytes_written = result;
      return G_IO_ERROR_NONE;
    }
}

static GIOError
g_io_win32_fd_seek (GIOChannel *channel,
		    gint        offset,
		    GSeekType   type)
{
  GIOWin32Channel *win32_channel = (GIOWin32Channel *) channel;
  int whence;
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
      g_warning ("g_io_win32_fd_seek: unknown seek type");
      return G_IO_ERROR_UNKNOWN;
    }
  
  result = lseek (win32_channel->fd, offset, whence);
  
  if (result < 0)
    {
      switch (errno)
	{
	case EINVAL:
	  return G_IO_ERROR_INVAL;
	default:
	  return G_IO_ERROR_UNKNOWN;
	}
    }
  else
    return G_IO_ERROR_NONE;
}

static void
g_io_win32_fd_close (GIOChannel *channel)
{
  GIOWin32Channel *win32_channel = (GIOWin32Channel *) channel;
  
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
}

static int
fd_reader (int     fd,
	   guchar *buf,
	   int     len)
{
  return read (fd, buf, len);
}

static GSource *
g_io_win32_fd_create_watch (GIOChannel    *channel,
			    GIOCondition   condition)
{
  return g_io_win32_create_watch (channel, condition, fd_reader);
}

static GIOError
g_io_win32_sock_read (GIOChannel *channel,
		      gchar      *buf,
		      guint       count,
		      guint      *bytes_read)
{
  GIOWin32Channel *win32_channel = (GIOWin32Channel *) channel;
  gint result;
  GIOError error;
  
  if (win32_channel->thread_id)
    {
      result = buffer_read (win32_channel, buf, count, &error);
      if (result < 0)
	{
	  *bytes_read = 0;
	  return error;
	}
      else
	{
	  *bytes_read = result;
	  return G_IO_ERROR_NONE;
	}
    }

  result = recv (win32_channel->fd, buf, count, 0);

  if (result < 0)
    {
      *bytes_read = 0;
      return G_IO_ERROR_UNKNOWN;
    }
  else
    {
      *bytes_read = result;
      return G_IO_ERROR_NONE;
    }
}

static GIOError
g_io_win32_sock_write (GIOChannel *channel,
		       gchar      *buf,
		       guint       count,
		       guint      *bytes_written)
{
  GIOWin32Channel *win32_channel = (GIOWin32Channel *) channel;
  gint result;
  
  result = send (win32_channel->fd, buf, count, 0);
  
  if (result == SOCKET_ERROR)
    {
      *bytes_written = 0;
      switch (WSAGetLastError ())
	{
	case WSAEINVAL:
	  return G_IO_ERROR_INVAL;
	case WSAEWOULDBLOCK:
	case WSAEINTR:
	  return G_IO_ERROR_AGAIN;
	default:
	  return G_IO_ERROR_UNKNOWN;
	}
    }
  else
    {
      *bytes_written = result;
      return G_IO_ERROR_NONE;
    }
}

static void
g_io_win32_sock_close (GIOChannel *channel)
{
  GIOWin32Channel *win32_channel = (GIOWin32Channel *) channel;

  if (win32_channel->debug)
    g_print ("thread %#x: closing socket %d\n",
	     win32_channel->thread_id,
	     win32_channel->fd);
  closesocket (win32_channel->fd);
  win32_channel->fd = -1;
}

static int
sock_reader (int     fd,
	     guchar *buf,
	     int     len)
{
  return recv (fd, buf, len, 0);
}

static GSource *
g_io_win32_sock_create_watch (GIOChannel    *channel,
			      GIOCondition   condition)
{
  return g_io_win32_create_watch (channel, condition, sock_reader);
}

static GIOFuncs win32_channel_msg_funcs = {
  g_io_win32_msg_read,
  g_io_win32_msg_write,
  g_io_win32_no_seek,
  g_io_win32_msg_close,
  g_io_win32_msg_create_watch,
  g_io_win32_free
};

static GIOFuncs win32_channel_fd_funcs = {
  g_io_win32_fd_read,
  g_io_win32_fd_write,
  g_io_win32_fd_seek,
  g_io_win32_fd_close,
  g_io_win32_fd_create_watch,
  g_io_win32_free
};

static GIOFuncs win32_channel_sock_funcs = {
  g_io_win32_sock_read,
  g_io_win32_sock_write,
  g_io_win32_no_seek,
  g_io_win32_sock_close,
  g_io_win32_sock_create_watch,
  g_io_win32_free
};

GIOChannel *
g_io_channel_win32_new_messages (guint hwnd)
{
  GIOWin32Channel *win32_channel = g_new (GIOWin32Channel, 1);
  GIOChannel *channel = (GIOChannel *) win32_channel;

  g_io_channel_init (channel);
  g_io_channel_win32_init (win32_channel);
  if (win32_channel->debug)
    g_print ("g_io_channel_win32_new_messages: hwnd = %ud\n", hwnd);
  channel->funcs = &win32_channel_msg_funcs;
  win32_channel->type = G_IO_WINDOWS_MESSAGES;
  win32_channel->hwnd = (HWND) hwnd;

  return channel;
}

GIOChannel *
g_io_channel_win32_new_fd (gint fd)
{
  GIOWin32Channel *win32_channel;
  GIOChannel *channel;
  struct stat st;

  if (fstat (fd, &st) == -1)
    {
      g_warning ("%d isn't a (emulated) file descriptor", fd);
      return NULL;
    }

  win32_channel = g_new (GIOWin32Channel, 1);
  channel = (GIOChannel *) win32_channel;

  g_io_channel_init (channel);
  g_io_channel_win32_init (win32_channel);
  if (win32_channel->debug)
    g_print ("g_io_channel_win32_new_fd: fd = %d\n", fd);
  channel->funcs = &win32_channel_fd_funcs;
  win32_channel->type = G_IO_FILE_DESC;
  win32_channel->fd = fd;

  return channel;
}

gint
g_io_channel_win32_get_fd (GIOChannel *channel)
{
  GIOWin32Channel *win32_channel = (GIOWin32Channel *) channel;

  return win32_channel->fd;
}

GIOChannel *
g_io_channel_win32_new_stream_socket (int socket)
{
  GIOWin32Channel *win32_channel = g_new (GIOWin32Channel, 1);
  GIOChannel *channel = (GIOChannel *) win32_channel;

  g_io_channel_init (channel);
  g_io_channel_win32_init (win32_channel);
  if (win32_channel->debug)
    g_print ("g_io_channel_win32_new_stream_socket: socket = %d\n", socket);
  channel->funcs = &win32_channel_sock_funcs;
  win32_channel->type = G_IO_STREAM_SOCKET;
  win32_channel->fd = socket;

  return channel;
}

GIOChannel *
g_io_channel_unix_new (gint fd)
{
  struct stat st;

  if (fstat (fd, &st) == 0)
    return g_io_channel_win32_new_fd (fd);
  
  if (getsockopt (fd, SOL_SOCKET, SO_TYPE, NULL, NULL) != SO_ERROR)
    return g_io_channel_win32_new_stream_socket(fd);

  g_warning ("%d isn't a file descriptor or a socket", fd);
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
  GIOWin32Channel *win32_channel = (GIOWin32Channel *) channel;

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
  GIOWin32Channel *win32_channel = (GIOWin32Channel *) channel;

  if (win32_channel->data_avail_event == NULL)
    create_events (win32_channel);
  
  fd->fd = (gint) win32_channel->data_avail_event;
  fd->events = condition;

  if (win32_channel->thread_id == 0)
    if (win32_channel->type == G_IO_FILE_DESC)
      create_reader_thread (win32_channel, fd_reader);
    else if (win32_channel->type == G_IO_STREAM_SOCKET)
      create_reader_thread (win32_channel, sock_reader);
}
