/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
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

#ifndef __G_MAIN_H__
#define __G_MAIN_H__

#include <gtypes.h>

G_BEGIN_DECLS

/* Main loop
 */
typedef struct _GTimeVal        GTimeVal;
typedef struct _GSourceFuncs    GSourceFuncs;
typedef struct _GMainLoop       GMainLoop;      /* Opaque */

struct _GTimeVal
{
  glong tv_sec;
  glong tv_usec;
};
struct _GSourceFuncs
{
  gboolean (*prepare)  (gpointer  source_data,
                        GTimeVal *current_time,
                        gint     *timeout,
                        gpointer  user_data);
  gboolean (*check)    (gpointer  source_data,
                        GTimeVal *current_time,
                        gpointer  user_data);
  gboolean (*dispatch) (gpointer  source_data,
                        GTimeVal *dispatch_time,
                        gpointer  user_data);
  GDestroyNotify destroy;
};

/* Standard priorities */

#define G_PRIORITY_HIGH            -100
#define G_PRIORITY_DEFAULT          0
#define G_PRIORITY_HIGH_IDLE        100
#define G_PRIORITY_DEFAULT_IDLE     200
#define G_PRIORITY_LOW              300

typedef gboolean (*GSourceFunc) (gpointer data);

/* Hooks for adding to the main loop */
guint    g_source_add                        (gint           priority,
                                              gboolean       can_recurse,
                                              GSourceFuncs  *funcs,
                                              gpointer       source_data,
                                              gpointer       user_data,
                                              GDestroyNotify notify);
gboolean g_source_remove                     (guint          tag);
gboolean g_source_remove_by_user_data        (gpointer       user_data);
gboolean g_source_remove_by_source_data      (gpointer       source_data);
gboolean g_source_remove_by_funcs_user_data  (GSourceFuncs  *funcs,
                                              gpointer       user_data);

void g_get_current_time                 (GTimeVal       *result);

/* Running the main loop */
GMainLoop*      g_main_new              (gboolean        is_running);
void            g_main_run              (GMainLoop      *loop);
void            g_main_quit             (GMainLoop      *loop);
void            g_main_destroy          (GMainLoop      *loop);
gboolean        g_main_is_running       (GMainLoop      *loop);

/* Run a single iteration of the mainloop. If block is FALSE,
 * will never block
 */
gboolean        g_main_iteration        (gboolean       may_block);

/* See if any events are pending */
gboolean        g_main_pending          (void);

/* Idles and timeouts */
guint           g_timeout_add_full      (gint           priority,
                                         guint          interval,
                                         GSourceFunc    function,
                                         gpointer       data,
                                         GDestroyNotify notify);
guint           g_timeout_add           (guint          interval,
                                         GSourceFunc    function,
                                         gpointer       data);
guint           g_idle_add              (GSourceFunc    function,
                                         gpointer       data);
guint           g_idle_add_full         (gint           priority,
                                         GSourceFunc    function,
                                         gpointer       data,
                                         GDestroyNotify destroy);
gboolean        g_idle_remove_by_data   (gpointer       data);

/* GPollFD
 *
 * System-specific IO and main loop calls
 *
 * On Win32, the fd in a GPollFD should be Win32 HANDLE (*not* a file
 * descriptor as provided by the C runtime) that can be used by
 * MsgWaitForMultipleObjects. This does *not* include file handles
 * from CreateFile, SOCKETs, nor pipe handles. (But you can use
 * WSAEventSelect to signal events when a SOCKET is readable).
 *
 * On Win32, fd can also be the special value G_WIN32_MSG_HANDLE to
 * indicate polling for messages. These message queue GPollFDs should
 * be added with the g_main_poll_win32_msg_add function.
 *
 * But note that G_WIN32_MSG_HANDLE GPollFDs should not be used by GDK
 * (GTK) programs, as GDK itself wants to read messages and convert them
 * to GDK events.
 *
 * So, unless you really know what you are doing, it's best not to try
 * to use the main loop polling stuff for your own needs on
 * Win32. It's really only written for the GIMP's needs so
 * far.
 */

typedef struct _GPollFD GPollFD;
typedef gint    (*GPollFunc)    (GPollFD *ufds,
                                 guint    nfsd,
                                 gint     timeout);
struct _GPollFD
{
  gint          fd;
  gushort       events;
  gushort       revents;
};

void        g_main_add_poll          (GPollFD    *fd,
                                      gint        priority);
void        g_main_remove_poll       (GPollFD    *fd);
void        g_main_set_poll_func     (GPollFunc   func);

#ifdef G_OS_WIN32

/* Useful on other platforms, too? */

GPollFunc   g_main_win32_get_poll_func (void);

#endif

G_END_DECLS

#endif /* __G_MAIN_H__ */

