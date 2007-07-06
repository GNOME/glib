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

#include "config.h"

#include "glib.h"

#include <stdlib.h>
#include <winsock2.h>
#include <windows.h>
#include <conio.h>
#include <fcntl.h>
#include <io.h>
#include <process.h>
#include <errno.h>
#include <sys/stat.h>

#include "gstdio.h"
#include "glibintl.h"

#include "galias.h"

typedef struct _GIOWin32Channel GIOWin32Channel;
typedef struct _GIOWin32Watch GIOWin32Watch;

#define BUFFER_SIZE 4096

typedef enum {
  G_IO_WIN32_WINDOWS_MESSAGES,	/* Windows messages */
  G_IO_WIN32_FILE_DESC,		/* Unix-like file descriptors from
				 * _open() or _pipe(), except for console IO.
				 * Have to create separate thread to read.
				 */
  G_IO_WIN32_CONSOLE,		/* Console IO (usually stdin, stdout, stderr) */
  G_IO_WIN32_SOCKET		/* Sockets. No separate thread */
} GIOWin32ChannelType;

struct _GIOWin32Channel {
  GIOChannel channel;
  gint fd;			/* Either a Unix-like file handle as provided
				 * by the Microsoft C runtime, or a SOCKET
				 * as provided by WinSock.
				 */
  GIOWin32ChannelType type;
  
  gboolean debug;

  /* This is used by G_IO_WIN32_WINDOWS_MESSAGES channels */
  HWND hwnd;			/* handle of window, or NULL */
  
  /* Following fields are used by fd channels. */
  CRITICAL_SECTION mutex;

  int direction;		/* 0 means we read from it,
				 * 1 means we write to it.
				 */

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
  int event_mask;
  int last_events;
  int event;
  gboolean write_would_have_blocked;
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

static const char *
event_mask_to_string (int mask)
{
  char buf[100];
  int checked_bits = 0;
  char *bufp = buf;

  if (mask == 0)
    return "";

#define BIT(n) checked_bits |= FD_##n; if (mask & FD_##n) bufp += sprintf (bufp, "%s" #n, (bufp>buf ? "|" : ""))

  BIT (READ);
  BIT (WRITE);
  BIT (OOB);
  BIT (ACCEPT);
  BIT (CONNECT);
  BIT (CLOSE);
  BIT (QOS);
  BIT (GROUP_QOS);
  BIT (ROUTING_INTERFACE_CHANGE);
  BIT (ADDRESS_LIST_CHANGE);
  
#undef BIT

  if ((mask & ~checked_bits) != 0)
	  bufp += sprintf (bufp, "|%#x", mask & ~checked_bits);
  
  return g_quark_to_string (g_quark_from_string (buf));
}

static const char *
condition_to_string (GIOCondition condition)
{
  char buf[100];
  int checked_bits = 0;
  char *bufp = buf;

  if (condition == 0)
    return "";

#define BIT(n) checked_bits |= G_IO_##n; if (condition & G_IO_##n) bufp += sprintf (bufp, "%s" #n, (bufp>buf ? "|" : ""))

  BIT (IN);
  BIT (OUT);
  BIT (PRI);
  BIT (ERR);
  BIT (HUP);
  BIT (NVAL);
  
#undef BIT

  if ((condition & ~checked_bits) != 0)
	  bufp += sprintf (bufp, "|%#x", condition & ~checked_bits);
  
  return g_quark_to_string (g_quark_from_string (buf));
}

static gboolean
g_io_win32_get_debug_flag (void)
{
  return (getenv ("G_IO_WIN32_DEBUG") != NULL);
}

static char *
winsock_error_message (int number)
{
  static char unk[100];

  switch (number) {
  case WSAEINTR:
    return "Interrupted function call";
  case WSAEACCES:
    return "Permission denied";
  case WSAEFAULT:
    return "Bad address";
  case WSAEINVAL:
    return "Invalid argument";
  case WSAEMFILE:
    return "Too many open sockets";
  case WSAEWOULDBLOCK:
    return "Resource temporarily unavailable";
  case WSAEINPROGRESS:
    return "Operation now in progress";
  case WSAEALREADY:
    return "Operation already in progress";
  case WSAENOTSOCK:
    return "Socket operation on nonsocket";
  case WSAEDESTADDRREQ:
    return "Destination address required";
  case WSAEMSGSIZE:
    return "Message too long";
  case WSAEPROTOTYPE:
    return "Protocol wrong type for socket";
  case WSAENOPROTOOPT:
    return "Bad protocol option";
  case WSAEPROTONOSUPPORT:
    return "Protocol not supported";
  case WSAESOCKTNOSUPPORT:
    return "Socket type not supported";
  case WSAEOPNOTSUPP:
    return "Operation not supported on transport endpoint";
  case WSAEPFNOSUPPORT:
    return "Protocol family not supported";
  case WSAEAFNOSUPPORT:
    return "Address family not supported by protocol family";
  case WSAEADDRINUSE:
    return "Address already in use";
  case WSAEADDRNOTAVAIL:
    return "Address not available";
  case WSAENETDOWN:
    return "Network interface is not configured";
  case WSAENETUNREACH:
    return "Network is unreachable";
  case WSAENETRESET:
    return "Network dropped connection on reset";
  case WSAECONNABORTED:
    return "Software caused connection abort";
  case WSAECONNRESET:
    return "Connection reset by peer";
  case WSAENOBUFS:
    return "No buffer space available";
  case WSAEISCONN:
    return "Socket is already connected";
  case WSAENOTCONN:
    return "Socket is not connected";
  case WSAESHUTDOWN:
    return "Can't send after socket shutdown";
  case WSAETIMEDOUT:
    return "Connection timed out";
  case WSAECONNREFUSED:
    return "Connection refused";
  case WSAEHOSTDOWN:
    return "Host is down";
  case WSAEHOSTUNREACH:
    return "Host is unreachable";
  case WSAEPROCLIM:
    return "Too many processes";
  case WSASYSNOTREADY:
    return "Network subsystem is unavailable";
  case WSAVERNOTSUPPORTED:
    return "Winsock.dll version out of range";
  case WSANOTINITIALISED:
    return "Successful WSAStartup not yet performed";
  case WSAEDISCON:
    return "Graceful shutdown in progress";
  case WSATYPE_NOT_FOUND:
    return "Class type not found";
  case WSAHOST_NOT_FOUND:
    return "Host not found";
  case WSATRY_AGAIN:
    return "Nonauthoritative host not found";
  case WSANO_RECOVERY:
    return "This is a nonrecoverable error";
  case WSANO_DATA:
    return "Valid name, no data record of requested type";
  case WSA_INVALID_HANDLE:
    return "Specified event object handle is invalid";
  case WSA_INVALID_PARAMETER:
    return "One or more parameters are invalid";
  case WSA_IO_INCOMPLETE:
    return "Overlapped I/O event object not in signaled state";
  case WSA_NOT_ENOUGH_MEMORY:
    return "Insufficient memory available";
  case WSA_OPERATION_ABORTED:
    return "Overlapped operation aborted";
  case WSAEINVALIDPROCTABLE:
    return "Invalid procedure table from service provider";
  case WSAEINVALIDPROVIDER:
    return "Invalid service provider version number";
  case WSAEPROVIDERFAILEDINIT:
    return "Unable to initialize a service provider";
  case WSASYSCALLFAILURE:
    return "System call failure";
  default:
    sprintf (unk, "Unknown WinSock error %d", number);
    return unk;
  }
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
  channel->event_mask = 0;
  channel->last_events = 0;
  channel->event = 0;
  channel->write_would_have_blocked = FALSE;
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
      || !(channel->space_avail_event = CreateEvent (&sec_attrs, FALSE, FALSE, NULL)))
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
    g_print ("read_thread %#x: start fd=%d, data_avail=%#x space_avail=%#x\n",
	     channel->thread_id,
	     channel->fd,
	     (guint) channel->data_avail_event,
	     (guint) channel->space_avail_event);

  channel->direction = 0;
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

static unsigned __stdcall
write_thread (void *parameter)
{
  GIOWin32Channel *channel = parameter;
  guchar *buffer;
  guint nbytes;

  g_io_channel_ref ((GIOChannel *)channel);

  if (channel->debug)
    g_print ("write_thread %#x: start fd=%d, data_avail=%#x space_avail=%#x\n",
	     channel->thread_id,
	     channel->fd,
	     (guint) channel->data_avail_event,
	     (guint) channel->space_avail_event);
  
  channel->direction = 1;
  channel->buffer = g_malloc (BUFFER_SIZE);
  channel->rdp = channel->wrp = 0;
  channel->running = TRUE;

  SetEvent (channel->space_avail_event);

  /* We use the same event objects as for a reader thread, but with
   * reversed meaning. So, space_avail is used if data is available
   * for writing, and data_avail is used if space is available in the
   * write buffer.
   */

  LOCK (channel->mutex);
  while (channel->running || channel->rdp != channel->wrp)
    {
      if (channel->debug)
	g_print ("write_thread %#x: rdp=%d, wrp=%d\n",
		 channel->thread_id, channel->rdp, channel->wrp);
      if (channel->wrp == channel->rdp)
	{
	  /* Buffer is empty. */
	  if (channel->debug)
	    g_print ("write_thread %#x: resetting space_avail\n",
		     channel->thread_id);
	  ResetEvent (channel->space_avail_event);
	  if (channel->debug)
	    g_print ("write_thread %#x: waiting for data\n",
		     channel->thread_id);
	  channel->revents = G_IO_OUT;
	  SetEvent (channel->data_avail_event);
	  UNLOCK (channel->mutex);
	  WaitForSingleObject (channel->space_avail_event, INFINITE);

	  LOCK (channel->mutex);
	  if (channel->rdp == channel->wrp)
	    break;

	  if (channel->debug)
	    g_print ("write_thread %#x: rdp=%d, wrp=%d\n",
		     channel->thread_id, channel->rdp, channel->wrp);
	}
      
      buffer = channel->buffer + channel->rdp;
      if (channel->rdp < channel->wrp)
	nbytes = channel->wrp - channel->rdp;
      else
	nbytes = BUFFER_SIZE - channel->rdp;

      if (channel->debug)
	g_print ("write_thread %#x: calling write() for %d bytes\n",
		 channel->thread_id, nbytes);

      UNLOCK (channel->mutex);
      nbytes = write (channel->fd, buffer, nbytes);
      LOCK (channel->mutex);

      if (channel->debug)
	g_print ("write_thread %#x: write(%i) returned %d, rdp=%d, wrp=%d\n",
		 channel->thread_id, channel->fd, nbytes, channel->rdp, channel->wrp);

      channel->revents = 0;
      if (nbytes > 0)
	channel->revents |= G_IO_OUT;
      else if (nbytes <= 0)
	channel->revents |= G_IO_ERR;

      channel->rdp = (channel->rdp + nbytes) % BUFFER_SIZE;

      if (nbytes <= 0)
	break;

      if (channel->debug)
	g_print ("write_thread: setting data_avail for thread %#x\n",
		 channel->thread_id);
      SetEvent (channel->data_avail_event);
    }
  
  channel->running = FALSE;
  if (channel->needs_close)
    {
      if (channel->debug)
	g_print ("write_thread %#x: channel fd %d needs closing\n",
		 channel->thread_id, channel->fd);
      close (channel->fd);
      channel->fd = -1;
    }

  UNLOCK (channel->mutex);
  
  g_io_channel_unref ((GIOChannel *)channel);
  
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


static GIOStatus
buffer_write (GIOWin32Channel *channel,
	      const guchar    *dest,
	      gsize            count,
	      gsize           *bytes_written,
	      GError         **err)
{
  guint nbytes;
  guint left = count;
  
  LOCK (channel->mutex);
  if (channel->debug)
    g_print ("buffer_write: writing to thread %#x %d bytes, rdp=%d, wrp=%d\n",
	     channel->thread_id, count, channel->rdp, channel->wrp);
  
  if ((channel->wrp + 1) % BUFFER_SIZE == channel->rdp)
    {
      /* Buffer is full */
      if (channel->debug)
	g_print ("buffer_write: tid %#x: resetting data_avail\n",
		 channel->thread_id);
      ResetEvent (channel->data_avail_event);
      if (channel->debug)
	g_print ("buffer_write: tid %#x: waiting for space\n",
		 channel->thread_id);
      UNLOCK (channel->mutex);
      WaitForSingleObject (channel->data_avail_event, INFINITE);
      LOCK (channel->mutex);
      if (channel->debug)
	g_print ("buffer_write: tid %#x: rdp=%d, wrp=%d\n",
		 channel->thread_id, channel->rdp, channel->wrp);
    }
   
  nbytes = MIN ((channel->rdp + BUFFER_SIZE - channel->wrp - 1) % BUFFER_SIZE,
		BUFFER_SIZE - channel->wrp);

  UNLOCK (channel->mutex);
  nbytes = MIN (left, nbytes);
  if (channel->debug)
    g_print ("buffer_write: tid %#x: writing %d bytes\n",
	     channel->thread_id, nbytes);
  memcpy (channel->buffer + channel->wrp, dest, nbytes);
  dest += nbytes;
  left -= nbytes;
  LOCK (channel->mutex);

  channel->wrp = (channel->wrp + nbytes) % BUFFER_SIZE;
  if (channel->debug)
    g_print ("buffer_write: tid %#x: rdp=%d, wrp=%d, setting space_avail\n",
	     channel->thread_id, channel->rdp, channel->wrp);
  SetEvent (channel->space_avail_event);

  if ((channel->wrp + 1) % BUFFER_SIZE == channel->rdp)
    {
      /* Buffer is full */
      if (channel->debug)
	g_print ("buffer_write: tid %#x: resetting data_avail\n",
		 channel->thread_id);
      ResetEvent (channel->data_avail_event);
    }

  UNLOCK (channel->mutex);
  
  /* We have no way to indicate any errors form the actual
   * write() call in the writer thread. Should we have?
   */
  *bytes_written = count - left;
  return (*bytes_written > 0) ? G_IO_STATUS_NORMAL : G_IO_STATUS_EOF;
}


static gboolean
g_io_win32_prepare (GSource *source,
		    gint    *timeout)
{
  GIOWin32Watch *watch = (GIOWin32Watch *)source;
  GIOCondition buffer_condition = g_io_channel_get_buffer_condition (watch->channel);
  GIOWin32Channel *channel = (GIOWin32Channel *)watch->channel;
  int event_mask;
  
  *timeout = -1;
  
  switch (channel->type)
    {
    case G_IO_WIN32_WINDOWS_MESSAGES:
    case G_IO_WIN32_CONSOLE:
      break;

    case G_IO_WIN32_FILE_DESC:
      if (channel->debug)
	g_print ("g_io_win32_prepare: for thread %#x buffer_condition:{%s}\n"
		 "  watch->pollfd.events:{%s} watch->pollfd.revents:{%s} channel->revents:{%s}\n",
		 channel->thread_id, condition_to_string (buffer_condition),
		 condition_to_string (watch->pollfd.events),
		 condition_to_string (watch->pollfd.revents),
		 condition_to_string (channel->revents));
      
      LOCK (channel->mutex);
      if (channel->running)
	{
	  if (channel->direction == 0 && channel->wrp == channel->rdp)
	    {
	      if (channel->debug)
		g_print ("g_io_win32_prepare: for thread %#x, setting channel->revents = 0\n",
			 channel->thread_id);
	      channel->revents = 0;
	    }
	}
      else
	{
	  if (channel->direction == 1
	      && (channel->wrp + 1) % BUFFER_SIZE == channel->rdp)
	    {
	      if (channel->debug)
		g_print ("g_io_win32_prepare: for thread %#x, setting channel->revents = %i\n",
			 channel->thread_id, 0);
	      channel->revents = 0;
	    }
	}	  
      UNLOCK (channel->mutex);
      break;

    case G_IO_WIN32_SOCKET:
      event_mask = 0;
      if (watch->condition & G_IO_IN)
	event_mask |= (FD_READ | FD_ACCEPT);
      if (watch->condition & G_IO_OUT)
	event_mask |= (FD_WRITE | FD_CONNECT);
      event_mask |= FD_CLOSE;

      if (channel->event_mask != event_mask /* || channel->event != watch->pollfd.fd*/)
	{
	  if (channel->debug)
	    g_print ("g_io_win32_prepare: WSAEventSelect(%d, %#x, {%s}\n",
		     channel->fd, watch->pollfd.fd,
		     event_mask_to_string (event_mask));
	  if (WSAEventSelect (channel->fd, (HANDLE) watch->pollfd.fd,
			      event_mask) == SOCKET_ERROR)
	    ;			/* What? */
	  channel->event_mask = event_mask;
#if 0
	  channel->event = watch->pollfd.fd;
#endif
	  channel->last_events = 0;
	}
      break;

    default:
      g_assert_not_reached ();
      abort ();
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
  WSANETWORKEVENTS events;

  switch (channel->type)
    {
    case G_IO_WIN32_WINDOWS_MESSAGES:
      return (PeekMessage (&msg, channel->hwnd, 0, 0, PM_NOREMOVE));

    case G_IO_WIN32_FILE_DESC:
      if (channel->debug)
	g_print ("g_io_win32_check: for thread %#x buffer_condition=%s\n"
		 "  watch->pollfd.events={%s} watch->pollfd.revents={%s} channel->revents={%s}\n",
		 channel->thread_id, condition_to_string (buffer_condition),
		 condition_to_string (watch->pollfd.events),
		 condition_to_string (watch->pollfd.revents),
		 condition_to_string (channel->revents));
      
      watch->pollfd.revents = (watch->pollfd.events & channel->revents);

      return ((watch->pollfd.revents | buffer_condition) & watch->condition);

    case G_IO_WIN32_CONSOLE:
      if (watch->channel->is_writeable)
	return TRUE;
      else if (watch->channel->is_readable)
        {
	  INPUT_RECORD buffer;
	  DWORD n;
	  if (PeekConsoleInput ((HANDLE) watch->pollfd.fd, &buffer, 1, &n) &&
	      n == 1)
	    {
	      /* _kbhit() does quite complex processing to find out
	       * whether at least one of the key events pending corresponds
	       * to a "real" character that can be read.
	       */
	      if (_kbhit ())
		return TRUE;
	      
	      /* Discard all other kinds of events */
	      ReadConsoleInput ((HANDLE) watch->pollfd.fd, &buffer, 1, &n);
	    }
        }
      return FALSE;

    case G_IO_WIN32_SOCKET:
      if (channel->last_events & FD_WRITE)
	{
	  if (channel->debug)
	    g_print ("g_io_win32_check: sock=%d event=%#x last_events has FD_WRITE\n",
		     channel->fd, watch->pollfd.fd);
	}
      else
	{
	  WSAEnumNetworkEvents (channel->fd, 0, &events);

	  if (channel->debug)
	    g_print ("g_io_win32_check: WSAEnumNetworkEvents (%d, %#x) revents={%s} condition={%s} events={%s}\n",
		     channel->fd, watch->pollfd.fd,
		     condition_to_string (watch->pollfd.revents),
		     condition_to_string (watch->condition),
		     event_mask_to_string (events.lNetworkEvents));
	  
	  if (watch->pollfd.revents != 0 &&
	      events.lNetworkEvents == 0 &&
	      !(channel->event_mask & FD_WRITE))
	    {
	      channel->event_mask = 0;
	      if (channel->debug)
		g_print ("g_io_win32_check: WSAEventSelect(%d, %#x, {})\n",
			 channel->fd, watch->pollfd.fd);
	      WSAEventSelect (channel->fd, (HANDLE) watch->pollfd.fd, 0);
	      if (channel->debug)
		g_print ("g_io_win32_check: ResetEvent(%#x)\n",
			 watch->pollfd.fd);
	      ResetEvent ((HANDLE) watch->pollfd.fd);
	    }
	  channel->last_events = events.lNetworkEvents;
	}
      watch->pollfd.revents = 0;
      if (channel->last_events & (FD_READ | FD_ACCEPT))
	watch->pollfd.revents |= G_IO_IN;
      if (channel->last_events & FD_WRITE)
	watch->pollfd.revents |= G_IO_OUT;
      else
	{
	  /* We have called WSAEnumNetworkEvents() above but it didn't
	   * set FD_WRITE.
	   */
	  if (events.lNetworkEvents & FD_CONNECT)
	    {
	      if (events.iErrorCode[FD_CONNECT_BIT] == 0)
		watch->pollfd.revents |= G_IO_OUT;
	      else
		watch->pollfd.revents |= (G_IO_HUP | G_IO_ERR);
	    }
	  if (watch->pollfd.revents == 0 && (channel->last_events & (FD_CLOSE)))
	    watch->pollfd.revents |= G_IO_HUP;
	}

      /* Regardless of WSAEnumNetworkEvents() result, if watching for
       * writability, unless last write would have blocked set
       * G_IO_OUT. But never set both G_IO_OUT and G_IO_HUP.
       */
      if (!(watch->pollfd.revents & G_IO_HUP) &&
	  !channel->write_would_have_blocked &&
	  (channel->event_mask & FD_WRITE))
	watch->pollfd.revents |= G_IO_OUT;

      return ((watch->pollfd.revents | buffer_condition) & watch->condition);

    default:
      g_assert_not_reached ();
      abort ();
    }
}

static gboolean
g_io_win32_dispatch (GSource     *source,
		     GSourceFunc  callback,
		     gpointer     user_data)
{
  GIOFunc func = (GIOFunc)callback;
  GIOWin32Watch *watch = (GIOWin32Watch *)source;
  GIOWin32Channel *channel = (GIOWin32Channel *)watch->channel;
  GIOCondition buffer_condition = g_io_channel_get_buffer_condition (watch->channel);
  
  if (!func)
    {
      g_warning (G_STRLOC ": GIOWin32Watch dispatched without callback\n"
		 "You must call g_source_connect().");
      return FALSE;
    }
  
  if (channel->debug)
    g_print ("g_io_win32_dispatch: pollfd.revents=%s condition=%s result=%s\n",
	     condition_to_string (watch->pollfd.revents),
	     condition_to_string (watch->condition),
	     condition_to_string ((watch->pollfd.revents | buffer_condition) & watch->condition));

  return (*func) (watch->channel,
		  (watch->pollfd.revents | buffer_condition) & watch->condition,
		  user_data);
}

static void
g_io_win32_finalize (GSource *source)
{
  GIOWin32Watch *watch = (GIOWin32Watch *)source;
  GIOWin32Channel *channel = (GIOWin32Channel *)watch->channel;
  
  switch (channel->type)
    {
    case G_IO_WIN32_WINDOWS_MESSAGES:
    case G_IO_WIN32_CONSOLE:
      break;

    case G_IO_WIN32_FILE_DESC:
      LOCK (channel->mutex);
      if (channel->debug)
	g_print ("g_io_win32_finalize: channel with thread %#x\n",
		 channel->thread_id);
      UNLOCK (channel->mutex);
      break;

    case G_IO_WIN32_SOCKET:
      if (channel->debug)
	g_print ("g_io_win32_finalize: channel is for sock=%d\n", channel->fd);
#if 0
      CloseHandle ((HANDLE) watch->pollfd.fd);
      channel->event = 0;
      channel->event_mask = 0;
#endif
      break;

    default:
      g_assert_not_reached ();
      abort ();
    }
  g_io_channel_unref (watch->channel);
}

GSourceFuncs g_io_watch_funcs = {
  g_io_win32_prepare,
  g_io_win32_check,
  g_io_win32_dispatch,
  g_io_win32_finalize
};

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
    g_print ("g_io_win32_free channel fd=%d\n", win32_channel->fd);

  if (win32_channel->data_avail_event)
    CloseHandle (win32_channel->data_avail_event);
  if (win32_channel->space_avail_event)
    CloseHandle (win32_channel->space_avail_event);
  if (win32_channel->type == G_IO_WIN32_SOCKET)
    WSAEventSelect (win32_channel->fd, NULL, 0);
  DeleteCriticalSection (&win32_channel->mutex);

  g_free (win32_channel->buffer);
  g_free (win32_channel);
}

static GSource *
g_io_win32_msg_create_watch (GIOChannel   *channel,
			     GIOCondition  condition)
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
g_io_win32_fd_and_console_read (GIOChannel *channel,
				gchar      *buf,
				gsize       count,
				gsize      *bytes_read,
				GError    **err)
{
  GIOWin32Channel *win32_channel = (GIOWin32Channel *)channel;
  gint result;
  
  if (win32_channel->debug)
    g_print ("g_io_win32_fd_read: fd=%d count=%d\n",
	     win32_channel->fd, count);
  
  if (win32_channel->thread_id)
    {
      return buffer_read (win32_channel, buf, count, bytes_read, err);
    }

  result = read (win32_channel->fd, buf, count);

  if (win32_channel->debug)
    g_print ("g_io_win32_fd_read: read() => %d\n", result);

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
g_io_win32_fd_and_console_write (GIOChannel  *channel,
				 const gchar *buf,
				 gsize        count,
				 gsize       *bytes_written,
				 GError     **err)
{
  GIOWin32Channel *win32_channel = (GIOWin32Channel *)channel;
  gint result;

  if (win32_channel->thread_id)
    {
      return buffer_write (win32_channel, buf, count, bytes_written, err);
    }
  
  result = write (win32_channel->fd, buf, count);
  if (win32_channel->debug)
    g_print ("g_io_win32_fd_write: fd=%d count=%d => %d\n",
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
      abort ();
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
      if (win32_channel->direction == 0)
	SetEvent (win32_channel->data_avail_event);
      else
	SetEvent (win32_channel->space_avail_event);
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
  GIOWin32Channel *win32_channel = (GIOWin32Channel *)channel;
  GSource *source = g_source_new (&g_io_watch_funcs, sizeof (GIOWin32Watch));
  GIOWin32Watch *watch = (GIOWin32Watch *)source;

  watch->channel = channel;
  g_io_channel_ref (channel);
  
  watch->condition = condition;
  
  if (win32_channel->data_avail_event == NULL)
    create_events (win32_channel);

  watch->pollfd.fd = (gint) win32_channel->data_avail_event;
  watch->pollfd.events = condition;
  
  if (win32_channel->debug)
    g_print ("g_io_win32_fd_create_watch: fd=%d condition={%s} handle=%#x\n",
	     win32_channel->fd, condition_to_string (condition), watch->pollfd.fd);

  LOCK (win32_channel->mutex);
  if (win32_channel->thread_id == 0)
    {
      if (condition & G_IO_IN)
	create_thread (win32_channel, condition, read_thread);
      else if (condition & G_IO_OUT)
	create_thread (win32_channel, condition, write_thread);
    }

  g_source_add_poll (source, &watch->pollfd);
  UNLOCK (win32_channel->mutex);

  return source;
}

static GIOStatus
g_io_win32_console_close (GIOChannel *channel,
		          GError    **err)
{
  GIOWin32Channel *win32_channel = (GIOWin32Channel *)channel;
  
  if (close (win32_channel->fd) < 0)
    {
      g_set_error (err, G_IO_CHANNEL_ERROR,
		   g_io_channel_error_from_errno (errno),
		   g_strerror (errno));
      return G_IO_STATUS_ERROR;
    }

  return G_IO_STATUS_NORMAL;
}

static GSource *
g_io_win32_console_create_watch (GIOChannel    *channel,
				 GIOCondition   condition)
{
  GIOWin32Channel *win32_channel = (GIOWin32Channel *)channel;
  GSource *source = g_source_new (&g_io_watch_funcs, sizeof (GIOWin32Watch));
  GIOWin32Watch *watch = (GIOWin32Watch *)source;

  watch->channel = channel;
  g_io_channel_ref (channel);
  
  watch->condition = condition;
  
  watch->pollfd.fd = (gint) _get_osfhandle (win32_channel->fd);
  watch->pollfd.events = condition;
  
  g_source_add_poll (source, &watch->pollfd);

  return source;
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
  GIOChannelError error;
  int winsock_error;

  if (win32_channel->debug)
    g_print ("g_io_win32_sock_read: sockfd=%d count=%d\n",
	     win32_channel->fd, count);

  result = recv (win32_channel->fd, buf, count, 0);
  if (result == SOCKET_ERROR)
    winsock_error = WSAGetLastError ();

  if (win32_channel->debug)
    g_print ("g_io_win32_sock_read: recv=%d %s\n",
	     result,
	     (result == SOCKET_ERROR ? winsock_error_message (winsock_error) : ""));
  
  if (result == SOCKET_ERROR)
    {
      *bytes_read = 0;

      switch (winsock_error)
	{
	case WSAEINVAL:
          error = G_IO_CHANNEL_ERROR_INVAL;
          break;
	case WSAEWOULDBLOCK:
          return G_IO_STATUS_AGAIN;
	default:
	  error = G_IO_CHANNEL_ERROR_FAILED;
          break;
	}
      g_set_error (err, G_IO_CHANNEL_ERROR, error,
		   winsock_error_message (winsock_error));
      return G_IO_STATUS_ERROR;
    }
  else
    {
      *bytes_read = result;
      if (result == 0)
	return G_IO_STATUS_EOF;
      else
	return G_IO_STATUS_NORMAL;
    }
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
  GIOChannelError error;
  int winsock_error;
  
  if (win32_channel->debug)
    g_print ("g_io_win32_sock_write: sockfd=%d count=%d\n",
	     win32_channel->fd, count);

  result = send (win32_channel->fd, buf, count, 0);
  if (result == SOCKET_ERROR)
    winsock_error = WSAGetLastError ();

  if (win32_channel->debug)
    g_print ("g_io_win32_sock_write: send=%d %s\n",
	     result,
	     (result == SOCKET_ERROR ? winsock_error_message (winsock_error) : ""));
  
  if (result == SOCKET_ERROR)
    {
      *bytes_written = 0;

      switch (winsock_error)
	{
	case WSAEINVAL:
	  error = G_IO_CHANNEL_ERROR_INVAL;
          break;
	case WSAEWOULDBLOCK:
	  win32_channel->write_would_have_blocked = TRUE;
	  win32_channel->last_events = 0;
          return G_IO_STATUS_AGAIN;
	default:
	  error = G_IO_CHANNEL_ERROR_FAILED;
          break;
	}
      g_set_error (err, G_IO_CHANNEL_ERROR, error,
		   winsock_error_message (winsock_error));

      return G_IO_STATUS_ERROR;
    }
  else
    {
      *bytes_written = result;
      win32_channel->write_would_have_blocked = FALSE;

      return G_IO_STATUS_NORMAL;
    }
}

static GIOStatus
g_io_win32_sock_close (GIOChannel *channel,
		       GError    **err)
{
  GIOWin32Channel *win32_channel = (GIOWin32Channel *)channel;

  if (win32_channel->fd != -1)
    {
      if (win32_channel->debug)
	g_print ("g_io_win32_sock_close: closing socket %d\n",
		 win32_channel->fd);
      
      closesocket (win32_channel->fd);
      win32_channel->fd = -1;
    }

  /* FIXME error detection? */

  return G_IO_STATUS_NORMAL;
}

static GSource *
g_io_win32_sock_create_watch (GIOChannel    *channel,
			      GIOCondition   condition)
{
  GIOWin32Channel *win32_channel = (GIOWin32Channel *)channel;
  GSource *source = g_source_new (&g_io_watch_funcs, sizeof (GIOWin32Watch));
  GIOWin32Watch *watch = (GIOWin32Watch *)source;
  
  watch->channel = channel;
  g_io_channel_ref (channel);
  
  watch->condition = condition;

  if (win32_channel->event == 0)
    win32_channel->event = (int) WSACreateEvent ();

  watch->pollfd.fd = win32_channel->event;
  watch->pollfd.events = condition;
  
  if (win32_channel->debug)
    g_print ("g_io_win32_sock_create_watch: sock=%d handle=%#x condition={%s}\n",
	     win32_channel->fd, watch->pollfd.fd,
	     condition_to_string (watch->condition));

  g_source_add_poll (source, &watch->pollfd);

  return source;
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
	abort ();
    }

  /* always open 'untranslated' */
  fid = g_open (filename, flags | _O_BINARY, pmode);

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
	abort ();
    }

  return channel;
}

#ifdef G_OS_WIN32

#undef g_io_channel_new_file

/* Binary compatibility version. Not for newly compiled code. */

GIOChannel *
g_io_channel_new_file (const gchar  *filename,
                       const gchar  *mode,
                       GError      **error)
{
  gchar *utf8_filename = g_locale_to_utf8 (filename, -1, NULL, NULL, error);
  GIOChannel *retval;

  if (utf8_filename == NULL)
    return NULL;

  retval = g_io_channel_new_file_utf8 (utf8_filename, mode, error);

  g_free (utf8_filename);

  return retval;
}

#endif

static GIOStatus
g_io_win32_unimpl_set_flags (GIOChannel *channel,
			     GIOFlags    flags,
			     GError    **err)
{
  GIOWin32Channel *win32_channel = (GIOWin32Channel *)channel;

  if (win32_channel->debug)
    {
      g_print ("g_io_win32_unimpl_set_flags: ");
      g_win32_print_gioflags (flags);
      g_print ("\n");
    }

  g_set_error (err, G_IO_CHANNEL_ERROR,
	       G_IO_CHANNEL_ERROR_FAILED,
	       "Not implemented on Win32");

  return G_IO_STATUS_ERROR;
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
g_io_win32_console_get_flags_internal (GIOChannel  *channel)
{
  GIOWin32Channel *win32_channel = (GIOWin32Channel *) channel;
  HANDLE handle = (HANDLE) _get_osfhandle (win32_channel->fd);
  gchar c;
  DWORD count;
  INPUT_RECORD record;

  channel->is_readable = PeekConsoleInput (handle, &record, 1, &count);
  channel->is_writeable = WriteFile (handle, &c, 0, &count, NULL);
  channel->is_seekable = FALSE;

  return 0;
}

static GIOFlags
g_io_win32_console_get_flags (GIOChannel *channel)
{
  GIOWin32Channel *win32_channel = (GIOWin32Channel *)channel;

  g_return_val_if_fail (win32_channel != NULL, 0);
  g_return_val_if_fail (win32_channel->type == G_IO_WIN32_CONSOLE, 0);

  return g_io_win32_console_get_flags_internal (channel);
}

static GIOFlags
g_io_win32_msg_get_flags (GIOChannel *channel)
{
  return 0;
}

static GIOStatus
g_io_win32_sock_set_flags (GIOChannel *channel,
			   GIOFlags    flags,
			   GError    **err)
{
  GIOWin32Channel *win32_channel = (GIOWin32Channel *)channel;
  u_long arg;

  if (win32_channel->debug)
    {
      g_print ("g_io_win32_sock_set_flags: ");
      g_win32_print_gioflags (flags);
      g_print ("\n");
    }

  if (flags & G_IO_FLAG_NONBLOCK)
    {
      arg = 1;
      if (ioctlsocket (win32_channel->fd, FIONBIO, &arg) == SOCKET_ERROR)
	{
	  g_set_error (err, G_IO_CHANNEL_ERROR,
		       G_IO_CHANNEL_ERROR_FAILED,
		       winsock_error_message (WSAGetLastError ()));
	  return G_IO_STATUS_ERROR;
	}
    }
  else
    {
      arg = 0;
      if (ioctlsocket (win32_channel->fd, FIONBIO, &arg) == SOCKET_ERROR)
	{
	  g_set_error (err, G_IO_CHANNEL_ERROR,
		       G_IO_CHANNEL_ERROR_FAILED,
		       winsock_error_message (WSAGetLastError ()));
	  return G_IO_STATUS_ERROR;
	}
    }

  return G_IO_STATUS_NORMAL;
}

static GIOFlags
g_io_win32_sock_get_flags (GIOChannel *channel)
{
  /* Could we do something here? */
  return 0;
}

static GIOFuncs win32_channel_msg_funcs = {
  g_io_win32_msg_read,
  g_io_win32_msg_write,
  NULL,
  g_io_win32_msg_close,
  g_io_win32_msg_create_watch,
  g_io_win32_free,
  g_io_win32_unimpl_set_flags,
  g_io_win32_msg_get_flags,
};

static GIOFuncs win32_channel_fd_funcs = {
  g_io_win32_fd_and_console_read,
  g_io_win32_fd_and_console_write,
  g_io_win32_fd_seek,
  g_io_win32_fd_close,
  g_io_win32_fd_create_watch,
  g_io_win32_free,
  g_io_win32_unimpl_set_flags,
  g_io_win32_fd_get_flags,
};

static GIOFuncs win32_channel_console_funcs = {
  g_io_win32_fd_and_console_read,
  g_io_win32_fd_and_console_write,
  NULL,
  g_io_win32_console_close,
  g_io_win32_console_create_watch,
  g_io_win32_free,
  g_io_win32_unimpl_set_flags,
  g_io_win32_console_get_flags,
};

static GIOFuncs win32_channel_sock_funcs = {
  g_io_win32_sock_read,
  g_io_win32_sock_write,
  NULL,
  g_io_win32_sock_close,
  g_io_win32_sock_create_watch,
  g_io_win32_free,
  g_io_win32_sock_set_flags,
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
    g_print ("g_io_channel_win32_new_messages: hwnd=%#x\n", hwnd);
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

  win32_channel->fd = fd;

  if (win32_channel->debug)
    g_print ("g_io_channel_win32_new_fd: %u\n", fd);
  if (st->st_mode & _S_IFCHR) /* console */
    {
      channel->funcs = &win32_channel_console_funcs;
      win32_channel->type = G_IO_WIN32_CONSOLE;
      g_io_win32_console_get_flags_internal (channel);
    }
  else
    {
      channel->funcs = &win32_channel_fd_funcs;
      win32_channel->type = G_IO_WIN32_FILE_DESC;
      g_io_win32_fd_get_flags_internal (channel, st);
    }
  
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
  if (win32_channel->debug)
    g_print ("g_io_channel_win32_new_socket: sockfd=%d\n", socket);
  channel->funcs = &win32_channel_sock_funcs;
  win32_channel->type = G_IO_WIN32_SOCKET;
  win32_channel->fd = socket;

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

  switch (win32_channel->type)
    {
    case G_IO_WIN32_FILE_DESC:
      if (win32_channel->data_avail_event == NULL)
	create_events (win32_channel);

      fd->fd = (gint) win32_channel->data_avail_event;

      if (win32_channel->thread_id == 0 && (condition & G_IO_IN))
	{
	  if (condition & G_IO_IN)
	    create_thread (win32_channel, condition, read_thread);
	  else if (condition & G_IO_OUT)
	    create_thread (win32_channel, condition, write_thread);
	}
      break;

    case G_IO_WIN32_CONSOLE:
      fd->fd = (gint) _get_osfhandle (win32_channel->fd);
      break;

    case G_IO_WIN32_SOCKET:
      fd->fd = (int) WSACreateEvent ();
      break;
      
    case G_IO_WIN32_WINDOWS_MESSAGES:
      fd->fd = G_WIN32_MSG_HANDLE;
      break;

    default:
      g_assert_not_reached ();
      abort ();
    }
  
  fd->events = condition;
}

/* Binary compatibility */
GIOChannel *
g_io_channel_win32_new_stream_socket (int socket)
{
  return g_io_channel_win32_new_socket (socket);
}

#define __G_IO_WIN32_C__
#include "galiasdef.c"
