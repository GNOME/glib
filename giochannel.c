/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * giochannel.c: IO Channel abstraction
 * Copyright 1998 Owen Taylor
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/* 
 * MT safe
 */

#include "glib.h"
#include <unistd.h>

typedef struct _GIOChannelPrivate GIOChannelPrivate;

struct _GIOChannelPrivate {
  GIOChannel channel;
  GIOFuncs *funcs;
  guint ref_count;
  gboolean closed;
};

GIOChannel *
g_io_channel_new (GIOFuncs *funcs,
		  gpointer  channel_data)
{
  GIOChannelPrivate *result;
  GIOChannel *channel;
  
  g_return_val_if_fail (funcs != NULL, NULL);
  
  result = g_new (GIOChannelPrivate, 1);
  channel = (GIOChannel *)result;
  
  result->funcs = funcs;
  result->ref_count = 1;
  result->closed = FALSE;

  channel->channel_data = channel_data;
  return channel;
}


void 
g_io_channel_ref (GIOChannel *channel)
{
  GIOChannelPrivate *private;
  g_return_if_fail (channel != NULL);

  private = (GIOChannelPrivate *)channel;

  private->ref_count++;
}

void 
g_io_channel_unref (GIOChannel *channel)
{
  GIOChannelPrivate *private;
  g_return_if_fail (channel != NULL);

  private = (GIOChannelPrivate *)channel;

  private->ref_count--;
  if (private->ref_count == 0)
    {
      /* We don't want to close the channel here, because
       * the channel may just be wrapping a file or socket
       * that the app is independently manipulating.
       */
      private->funcs->io_free (channel);
      g_free (private);
    }
}

GIOError 
g_io_channel_read (GIOChannel *channel, 
		   gchar      *buf, 
		   guint       count,
		   guint      *bytes_read)
{
  GIOChannelPrivate *private = (GIOChannelPrivate *)channel;

  g_return_val_if_fail (channel != NULL, G_IO_ERROR_UNKNOWN);
  g_return_val_if_fail (!private->closed, G_IO_ERROR_UNKNOWN);

  return private->funcs->io_read (channel, buf, count, bytes_read);
}

GIOError 
g_io_channel_write (GIOChannel *channel, 
		    gchar      *buf, 
		    guint       count,
		    guint      *bytes_written)
{
  GIOChannelPrivate *private = (GIOChannelPrivate *)channel;

  g_return_val_if_fail (channel != NULL, G_IO_ERROR_UNKNOWN);
  g_return_val_if_fail (!private->closed, G_IO_ERROR_UNKNOWN);

  return private->funcs->io_write (channel, buf, count, bytes_written);
}

GIOError 
g_io_channel_seek  (GIOChannel   *channel,
		    gint        offset, 
		    GSeekType   type)
{
  GIOChannelPrivate *private = (GIOChannelPrivate *)channel;

  g_return_val_if_fail (channel != NULL, G_IO_ERROR_UNKNOWN);
  g_return_val_if_fail (!private->closed, G_IO_ERROR_UNKNOWN);

  return private->funcs->io_seek (channel, offset, type);
}
     
void
g_io_channel_close (GIOChannel *channel)
{
  GIOChannelPrivate *private = (GIOChannelPrivate *)channel;

  g_return_if_fail (channel != NULL);
  g_return_if_fail (!private->closed);

  private->closed = TRUE;
  private->funcs->io_close (channel);
}

guint 
g_io_add_watch_full (GIOChannel      *channel,
		     gint           priority,
		     GIOCondition   condition,
		     GIOFunc        func,
		     gpointer       user_data,
		     GDestroyNotify notify)
{
  GIOChannelPrivate *private = (GIOChannelPrivate *)channel;

  g_return_val_if_fail (channel != NULL, 0);
  g_return_val_if_fail (!private->closed, 0);

  return private->funcs->io_add_watch (channel, priority, condition,
				       func, user_data, notify);
}

guint 
g_io_add_watch (GIOChannel      *channel,
		GIOCondition   condition,
		GIOFunc        func,
		gpointer       user_data)
{
  return g_io_add_watch_full (channel, 0, condition, func, user_data, NULL);
}
