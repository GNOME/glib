/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * giounix.c: IO Channels using unix file descriptors
 * Copyright 1998 Owen Taylor
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
 */

/*
 * Modified by the GLib Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GLib Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GLib at ftp://ftp.gtk.org/pub/gtk/. 
 */

/* 
 * MT safe
 */

#include "glib.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>

/*
 * Unix IO Channels
 */

typedef struct _GIOUnixChannel GIOUnixChannel;
typedef struct _GIOUnixWatch GIOUnixWatch;

struct _GIOUnixChannel
{
  GIOChannel channel;
  gint fd;
};

struct _GIOUnixWatch
{
  GSource       source;
  GPollFD       pollfd;
  GIOChannel   *channel;
  GIOCondition  condition;
  GIOFunc       callback;
};


static GIOStatus	g_io_unix_read		(GIOChannel   *channel,
						 gchar        *buf,
						 gsize         count,
						 gsize        *bytes_read,
						 GError      **err);
static GIOStatus	g_io_unix_write		(GIOChannel   *channel,
						 const gchar  *buf,
						 gsize         count,
						 gsize        *bytes_written,
						 GError      **err);
static GIOStatus	g_io_unix_seek		(GIOChannel   *channel,
						 glong         offset,
						 GSeekType     type,
						 GError      **err);
static GIOStatus	g_io_unix_close		(GIOChannel   *channel,
						 GError      **err);
static void		g_io_unix_free		(GIOChannel   *channel);
static GSource*		g_io_unix_create_watch	(GIOChannel   *channel,
						 GIOCondition  condition);
static GIOStatus	g_io_unix_set_flags	(GIOChannel   *channel,
                       				 GIOFlags      flags,
						 GError      **err);
static GIOFlags 	g_io_unix_get_flags	(GIOChannel   *channel);

static gboolean g_io_unix_prepare  (GSource     *source,
				    gint        *timeout);
static gboolean g_io_unix_check    (GSource     *source);
static gboolean g_io_unix_dispatch (GSource     *source,
				    GSourceFunc  callback,
				    gpointer     user_data);
static void     g_io_unix_finalize (GSource     *source);

GSourceFuncs unix_watch_funcs = {
  g_io_unix_prepare,
  g_io_unix_check,
  g_io_unix_dispatch,
  g_io_unix_finalize
};

GIOFuncs unix_channel_funcs = {
  g_io_unix_read,
  g_io_unix_write,
  g_io_unix_seek,
  g_io_unix_close,
  g_io_unix_create_watch,
  g_io_unix_free,
  g_io_unix_set_flags,
  g_io_unix_get_flags,
};

static gboolean 
g_io_unix_prepare (GSource  *source,
		   gint     *timeout)
{
  GIOUnixWatch *watch = (GIOUnixWatch *)source;
  GIOCondition buffer_condition = g_io_channel_get_buffer_condition (watch->channel);

  *timeout = -1;

  /* Only return TRUE here if _all_ bits in watch->condition will be set
   */
  return ((watch->condition & buffer_condition) == watch->condition);
}

static gboolean 
g_io_unix_check (GSource  *source)
{
  GIOUnixWatch *watch = (GIOUnixWatch *)source;
  GIOCondition buffer_condition = g_io_channel_get_buffer_condition (watch->channel);
  GIOCondition poll_condition = watch->pollfd.revents;

  return ((poll_condition | buffer_condition) & watch->condition);
}

static gboolean
g_io_unix_dispatch (GSource     *source,
		    GSourceFunc  callback,
		    gpointer     user_data)

{
  GIOFunc func = (GIOFunc)callback;
  GIOUnixWatch *watch = (GIOUnixWatch *)source;
  GIOCondition buffer_condition = g_io_channel_get_buffer_condition (watch->channel);

  if (!func)
    {
      g_warning ("IO watch dispatched without callback\n"
		 "You must call g_source_connect().");
      return FALSE;
    }
  
  return (*func) (watch->channel,
		  (watch->pollfd.revents | buffer_condition) & watch->condition,
		  user_data);
}

static void 
g_io_unix_finalize (GSource *source)
{
  GIOUnixWatch *watch = (GIOUnixWatch *)source;

  g_io_channel_unref (watch->channel);
}

static GIOStatus
g_io_unix_read (GIOChannel *channel, 
		gchar      *buf, 
		gsize       count,
		gsize      *bytes_read,
		GError    **err)
{
  GIOUnixChannel *unix_channel = (GIOUnixChannel *)channel;
  gssize result;

 retry:
  result = read (unix_channel->fd, buf, count);

  if (result < 0)
    {
      *bytes_read = 0;

      switch (errno)
        {
#ifdef EINTR
          case EINTR:
            goto retry;
#endif
#ifdef EAGAIN
          case EAGAIN:
            return G_IO_STATUS_AGAIN;
#endif
          default:
            g_set_error (err, G_IO_CHANNEL_ERROR,
                         g_channel_error_from_errno (errno),
                         strerror (errno));
            return G_IO_STATUS_ERROR;
        }
    }

  *bytes_read = result;

  return (result > 0) ? G_IO_STATUS_NORMAL : G_IO_STATUS_EOF;
}

static GIOStatus
g_io_unix_write (GIOChannel  *channel, 
		 const gchar *buf, 
		 gsize       count,
		 gsize      *bytes_written,
		 GError    **err)
{
  GIOUnixChannel *unix_channel = (GIOUnixChannel *)channel;
  gssize result;

 retry:
  result = write (unix_channel->fd, buf, count);

  if (result < 0)
    {
      *bytes_written = 0;

      switch (errno)
        {
#ifdef EINTR
          case EINTR:
            goto retry;
#endif
#ifdef EAGAIN
          case EAGAIN:
            return G_IO_STATUS_AGAIN;
#endif
          default:
            g_set_error (err, G_IO_CHANNEL_ERROR,
                         g_channel_error_from_errno (errno),
                         strerror (errno));
            return G_IO_STATUS_ERROR;
        }
    }

  *bytes_written = result;

  return G_IO_STATUS_NORMAL;
}

static GIOStatus
g_io_unix_seek (GIOChannel *channel,
		glong       offset, 
		GSeekType   type,
                GError    **err)
{
  GIOUnixChannel *unix_channel = (GIOUnixChannel *)channel;
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
      whence = -1; /* Shut the compiler up */
      g_assert_not_reached ();
    }

  result = lseek (unix_channel->fd, offset, whence);

  if (result < 0)
    {
      g_set_error (err, G_IO_CHANNEL_ERROR,
		   g_channel_error_from_errno (errno),
		   strerror (errno));
      return G_IO_STATUS_ERROR;
    }

  return G_IO_STATUS_NORMAL;
}


static GIOStatus
g_io_unix_close (GIOChannel *channel,
		 GError    **err)
{
  GIOUnixChannel *unix_channel = (GIOUnixChannel *)channel;

  if (close (unix_channel->fd) < 0)
    {
      g_set_error (err, G_IO_CHANNEL_ERROR,
		   g_channel_error_from_errno (errno),
		   strerror (errno));
      return G_IO_STATUS_ERROR;
    }

  return G_IO_STATUS_NORMAL;
}

static void 
g_io_unix_free (GIOChannel *channel)
{
  GIOUnixChannel *unix_channel = (GIOUnixChannel *)channel;

  g_free (unix_channel);
}

static GSource *
g_io_unix_create_watch (GIOChannel   *channel,
			GIOCondition  condition)
{
  GIOUnixChannel *unix_channel = (GIOUnixChannel *)channel;
  GSource *source;
  GIOUnixWatch *watch;


  source = g_source_new (&unix_watch_funcs, sizeof (GIOUnixWatch));
  watch = (GIOUnixWatch *)source;
  
  watch->channel = channel;
  g_io_channel_ref (channel);
  
  watch->condition = condition;

  watch->pollfd.fd = unix_channel->fd;
  watch->pollfd.events = condition;

  g_source_add_poll (source, &watch->pollfd);

  return source;
}

static const GIOFlags g_io_unix_fcntl_flags[] = {
  G_IO_FLAG_APPEND,
  G_IO_FLAG_NONBLOCK,
};
static const glong g_io_unix_fcntl_posix_flags[] = {
  O_APPEND,
#ifdef O_NONBLOCK
  O_NONBLOCK,
#else
  O_NDELAY,
#endif
};
#define G_IO_UNIX_NUM_FCNTL_FLAGS G_N_ELEMENTS (g_io_unix_fcntl_flags)

static const GIOFlags g_io_unix_fcntl_flags_read_only[] = {
  G_IO_FLAG_IS_READABLE,
  G_IO_FLAG_IS_WRITEABLE,
};
static const glong g_io_unix_fcntl_posix_flags_read_only[] = {
  O_RDONLY | O_RDWR,
  O_WRONLY | O_RDWR,
};
/* Only need to map posix_flags -> flags for read only, not the
 * other way around, so this works.
 */
#define G_IO_UNIX_NUM_FCNTL_FLAGS_READ_ONLY G_N_ELEMENTS (g_io_unix_fcntl_flags_read_only)

static GIOStatus
g_io_unix_set_flags (GIOChannel *channel,
                     GIOFlags    flags,
                     GError    **err)
{
  glong fcntl_flags;
  gint loop;
  GIOUnixChannel *unix_channel = (GIOUnixChannel *) channel;

  fcntl_flags = 0;

  for (loop = 0; loop < G_IO_UNIX_NUM_FCNTL_FLAGS; loop++)
    if (flags & g_io_unix_fcntl_flags[loop])
      fcntl_flags |= g_io_unix_fcntl_posix_flags[loop];

  if (fcntl (unix_channel->fd, F_SETFL, fcntl_flags) == -1)
    {
      g_set_error (err, G_IO_CHANNEL_ERROR,
		   g_channel_error_from_errno (errno),
		   g_strerror (errno));
      return G_IO_STATUS_ERROR;
    }

  return G_IO_STATUS_NORMAL;
}

static GIOFlags
g_io_unix_get_flags (GIOChannel *channel)
{
  GIOFlags flags = 0;
  glong fcntl_flags;
  gint loop;
  GIOUnixChannel *unix_channel = (GIOUnixChannel *) channel;
  struct stat buffer;

  fcntl_flags = fcntl (unix_channel->fd, F_GETFL);
  if (fcntl_flags == -1)
    {
      g_warning (G_STRLOC "Error while getting flags for FD: %s (%d)\n",
		 g_strerror (errno), errno);
      return 0;
    }
  if (!channel->seekable_cached)
    {
      channel->seekable_cached = TRUE;

      /* I'm not sure if fstat on a non-file (e.g., socket) works
       * it should be safe to sat if it fails, the fd isn't seekable.
       */
      if (fstat (unix_channel->fd, &buffer) == -1 ||
	  !S_ISREG (buffer.st_mode))
	channel->is_seekable = FALSE;
      else
	channel->is_seekable = TRUE;
    }

  for (loop = 0; loop < G_IO_UNIX_NUM_FCNTL_FLAGS; loop++)
    if (fcntl_flags & g_io_unix_fcntl_posix_flags[loop])
      flags |= g_io_unix_fcntl_flags[loop];

  for (loop = 0; loop < G_IO_UNIX_NUM_FCNTL_FLAGS_READ_ONLY; loop++)
    if (fcntl_flags & g_io_unix_fcntl_posix_flags_read_only[loop])
      flags |= g_io_unix_fcntl_flags_read_only[loop];

  if (channel->is_seekable)
    flags |= G_IO_FLAG_IS_SEEKABLE;

  return flags;
}

GIOChannel *
g_io_channel_new_file (const gchar *filename,
                       GIOFileMode  mode,
                       GError     **error)
{
  FILE *f;
  int fid;
  gchar *mode_name;
  GIOChannel *channel;

  switch (mode)
    {
      case G_IO_FILE_MODE_READ:
        mode_name = "r";
        break;
      case G_IO_FILE_MODE_WRITE:
        mode_name = "w";
        break;
      case G_IO_FILE_MODE_APPEND:
        mode_name = "a";
        break;
      case G_IO_FILE_MODE_READ_WRITE:
        mode_name = "r+";
        break;
      case G_IO_FILE_MODE_READ_WRITE_TRUNCATE:
        mode_name = "w+";
        break;
      case G_IO_FILE_MODE_READ_WRITE_APPEND:
        mode_name = "a+";
        break;
      default:
        g_warning ("Invalid GIOFileMode %i.\n", mode);
        return NULL;
    }

  f = fopen (filename, mode_name);
  if (!f)
    {
      g_set_error (error, G_FILE_ERROR,
                   g_file_error_from_errno (errno),
                   strerror (errno));
      return (GIOChannel *)NULL;
    }
    
  fid = fileno (f);

  channel = g_io_channel_unix_new (fid);
  channel->close_on_unref = TRUE;
  return channel;
}

GIOChannel *
g_io_channel_unix_new (gint fd)
{
  GIOUnixChannel *unix_channel = g_new (GIOUnixChannel, 1);
  GIOChannel *channel = (GIOChannel *)unix_channel;

  g_io_channel_init (channel);
  channel->funcs = &unix_channel_funcs;

  unix_channel->fd = fd;
  return channel;
}

gint
g_io_channel_unix_get_fd (GIOChannel *channel)
{
  GIOUnixChannel *unix_channel = (GIOUnixChannel *)channel;
  return unix_channel->fd;
}
