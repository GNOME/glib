/* GLIB - Library of useful routines for C programming
 *
 * gthreadprivate.h - GLib internal thread system related declarations.
 *
 *  Copyright (C) 2003 Sebastian Wilhelmi
 *
 * The Gnome Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * The Gnome Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the Gnome Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 *   Boston, MA 02111-1307, USA.
 */

#ifndef __G_THREADPRIVATE_H__
#define __G_THREADPRIVATE_H__

G_BEGIN_DECLS

/* System thread identifier comparison and assignment */
#if GLIB_SIZEOF_SYSTEM_THREAD == SIZEOF_VOID_P
# define g_system_thread_assign(dest, src)				\
   ((dest).dummy_pointer = (src).dummy_pointer)
#else /* GLIB_SIZEOF_SYSTEM_THREAD != SIZEOF_VOID_P */
# define g_system_thread_assign(dest, src)				\
   (memcpy (&(dest), &(src), GLIB_SIZEOF_SYSTEM_THREAD))
#endif /* GLIB_SIZEOF_SYSTEM_THREAD == SIZEOF_VOID_P */

G_GNUC_INTERNAL gboolean g_system_thread_equal (gpointer thread1,
                                                gpointer thread2);

G_GNUC_INTERNAL void     g_system_thread_exit  (void);

/* Is called from gthread/gthread-impl.c */
void g_thread_init_glib (void);

/* initializers that may also use g_private_new() */
G_GNUC_INTERNAL void _g_messages_thread_init_nomessage      (void);

/* full fledged initializers */
G_GNUC_INTERNAL void _g_thread_impl_init  (void);

struct _GPrivate
{
  gpointer single_value;
  gboolean ready;
#ifdef G_OS_WIN32
  gint index;
#else
  pthread_key_t key;
#endif
};

G_GNUC_INTERNAL void g_private_init (GPrivate       *key,
                                     GDestroyNotify  notify);

G_END_DECLS

#endif /* __G_THREADPRIVATE_H__ */
