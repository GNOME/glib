/*
 * Copyright © 2014 Canonical Limited
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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Ryan Lortie <desrt@desrt.ca>
 */

#ifndef __gpollcore_h__
#define __gpollcore_h__

#include <glib.h>

#if defined(POLLCORE_KQUEUE)

  #include <sys/types.h>
  #include <sys/event.h>
  #include <sys/time.h>

  typedef gint ghandle;

  typedef struct kevent GPollEvent;

  #define g_poll_event_get_user_data(gpe)       ((gpe).udata)
  #define g_poll_event_get_revents(gpe)         (((gpe).filter == EVFILT_WRITE) ? POLLOUT : POLLIN)

  typedef struct
  {
    gint kqueue_fd;
    gint kqueue_timer;
  } GPollCore;

#elif defined(POLLCORE_EPOLL)

  #include <sys/epoll.h>

  typedef gint ghandle;

  typedef struct epoll_event GPollEvent;

  #define g_poll_event_get_user_data(gpe)       ((gpe).data.ptr)
  #define g_poll_event_get_revents(gpe)         ((gpe).events)

  typedef struct
  {
    gint epollfd;
    gint timerfd;
  } GPollCore;

#elif defined(POLLCORE_WIN32)

  #include <windows.h>

  typedef HANDLE ghandle;

  typedef gpointer GPollEvent;

  #define g_poll_event_get_user_data(gpe)       (gpe)
  #define g_poll_event_get_revents(gpe)         (G_IO_IN)

  typedef struct
  {
    gboolean polling_msgs;
    gpointer msgs_user_data;
    HANDLE   handles[MAXIMUM_WAIT_OBJECTS];
    gpointer user_data[MAXIMUM_WAIT_OBJECTS];
    gint     n_handles;
    gint64   ready_time;
    HANDLE   waiting_thread;
    GMutex   mutex;
  } GPollCore;

#elif defined(POLLCORE_POLL)

  typedef gint ghandle;

  typedef struct
  {
    struct pollfd *pfds;
    gpointer      *user_data;
    gint           n_pfds;
    gint           n_allocated_pfds;
    gint64         ready_time;
    gboolean       waiting;
    gint           pipes[2];
    GMutex         mutex;
  } GPollCore;

  typedef struct
  {
    guint    revents;
    gpointer user_data;
  } GPollEvent;

  #define g_poll_event_get_user_data(gpe)       ((gpe).user_data)
  #define g_poll_event_get_revents(gpe)         ((gpe).revents)

#else
  #error This should not be possible.  Check your configuration...
#endif

typedef struct
{
  gpointer user_data;
  gushort old_events;
  gushort new_events;
} GPollUpdate;

GLIB_AVAILABLE_IN_ALL
void            g_poll_core_init                (GPollCore  *core);
GLIB_AVAILABLE_IN_ALL
void            g_poll_core_clear               (GPollCore  *core);

/* Called from owner thread with lock held */
GLIB_AVAILABLE_IN_ALL
gint            g_poll_core_update_and_collect  (GPollCore  *core,
                                                 GHashTable *updates,
                                                 gint64     *ready_time_update,
                                                 GPollEvent *events,
                                                 gint        max_events);

/* Called with lock held and must release it before sleeping */
GLIB_AVAILABLE_IN_ALL
void            g_poll_core_wait                (GPollCore  *core,
                                                 GMutex     *mutex);

/* Called from another thread with context lock held */
GLIB_AVAILABLE_IN_ALL
void            g_poll_core_update              (GPollCore  *core,
                                                 ghandle     handle,
                                                 guint       old_events,
                                                 guint       new_events,
                                                 gpointer    user_data);

GLIB_AVAILABLE_IN_ALL
void            g_poll_core_set_ready_time      (GPollCore  *core,
                                                 gint64      ready_time);

/* Only on UNIX */
GLIB_AVAILABLE_IN_ALL
gint            g_poll_core_get_unix_fd         (GPollCore  *core);

#endif /* __gpollcore_h__ */
