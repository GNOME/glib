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
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Alexander Larsson <alexl@redhat.com>
 */

#include <config.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <fcntl.h>
#include <gioerror.h>
#ifdef G_OS_WIN32
#include <io.h>
#ifndef pipe
#define pipe(fds) _pipe(fds, 4096, _O_BINARY)
#endif
#endif
#include "gcancellable.h"
#include "glibintl.h"

#include "gioalias.h"

/**
 * SECTION:gcancellable
 * @short_description: Thread-safe Operation Cancellation Stack
 * @include: gio/gio.h
 *
 * GCancellable is a thread-safe operation cancellation stack used 
 * throughout GIO to allow for cancellation of synchronous and
 * asynchronous operations.
 */

enum {
  CANCELLED,
  LAST_SIGNAL
};

struct _GCancellable
{
  GObject parent_instance;

  guint cancelled : 1;
  guint allocated_pipe : 1;
  int cancel_pipe[2];
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GCancellable, g_cancellable, G_TYPE_OBJECT);

static GStaticPrivate current_cancellable = G_STATIC_PRIVATE_INIT;
G_LOCK_DEFINE_STATIC(cancellable);
  
static void
g_cancellable_finalize (GObject *object)
{
  GCancellable *cancellable = G_CANCELLABLE (object);

  if (cancellable->cancel_pipe[0] != -1)
    close (cancellable->cancel_pipe[0]);
  
  if (cancellable->cancel_pipe[1] != -1)
    close (cancellable->cancel_pipe[1]);
  
  if (G_OBJECT_CLASS (g_cancellable_parent_class)->finalize)
    (*G_OBJECT_CLASS (g_cancellable_parent_class)->finalize) (object);
}

static void
g_cancellable_class_init (GCancellableClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  
  gobject_class->finalize = g_cancellable_finalize;

  /**
   * GCancellable::cancelled:
   * @cancellable: a #GCancellable.
   * 
   * Emitted when the operation has been cancelled from another thread.
   * 
   * Can be used by implementations of cancellable operations. This will
   * be emitted in the thread that tried to cancel the operation, not the
   * thread the is running the operation.
   */
  signals[CANCELLED] =
    g_signal_new (I_("cancelled"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GCancellableClass, cancelled),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__VOID,
		  G_TYPE_NONE, 0);
  
}

static void
set_fd_nonblocking (int fd)
{
#ifdef F_GETFL
  glong fcntl_flags;
  fcntl_flags = fcntl (fd, F_GETFL);

#ifdef O_NONBLOCK
  fcntl_flags |= O_NONBLOCK;
#else
  fcntl_flags |= O_NDELAY;
#endif

  fcntl (fd, F_SETFL, fcntl_flags);
#endif
}

static void
g_cancellable_open_pipe (GCancellable *cancellable)
{
  if (pipe (cancellable->cancel_pipe) == 0)
    {
      /* Make them nonblocking, just to be sure we don't block
       * on errors and stuff
       */
      set_fd_nonblocking (cancellable->cancel_pipe[0]);
      set_fd_nonblocking (cancellable->cancel_pipe[1]);
    }
  else
    g_warning ("Failed to create pipe for GCancellable. Out of file descriptors?");
}

static void
g_cancellable_init (GCancellable *cancellable)
{
  cancellable->cancel_pipe[0] = -1;
  cancellable->cancel_pipe[1] = -1;
}

/**
 * g_cancellable_new:
 * 
 * Creates a new #GCancellable object.
 *
 * Applications that want to start one or more operations
 * that should be cancellable should create a #GCancellable
 * and pass it to the operations.
 *
 * One #GCancellable can be used in multiple consecutive
 * operations, but not in multiple concurrent operations.
 *  
 * Returns: a #GCancellable.
 **/
GCancellable *
g_cancellable_new (void)
{
  return g_object_new (G_TYPE_CANCELLABLE, NULL);
}

/**
 * g_cancellable_push_current:
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 * 
 * Pushes @cancellable onto the cancellable stack. The current
 * cancllable can then be recieved using g_cancellable_get_current().
 *
 * This is useful when implementing cancellable operations in
 * code that does not allow you to pass down the cancellable object.
 *
 * This is typically called automatically by e.g. #GFile operations,
 * so you rarely have to call this yourself.
 **/
void
g_cancellable_push_current (GCancellable *cancellable)
{
  GSList *l;

  g_return_if_fail (cancellable != NULL);
  
  l = g_static_private_get (&current_cancellable);
  l = g_slist_prepend (l, cancellable);
  g_static_private_set (&current_cancellable, l, NULL);
}

/**
 * g_cancellable_pop_current:
 * @cancellable: optional #GCancellable object, %NULL to ignore.
 *
 * Pops @cancellable off the cancellable stack (verifying that @cancellable 
 * is on the top of the stack).
 **/
void
g_cancellable_pop_current (GCancellable *cancellable)
{
  GSList *l;
  
  l = g_static_private_get (&current_cancellable);
  
  g_return_if_fail (l != NULL);
  g_return_if_fail (l->data == cancellable);

  l = g_slist_delete_link (l, l);
  g_static_private_set (&current_cancellable, l, NULL);
}

/**
 * g_cancellable_get_current:
 * 
 * Gets the top cancellable from the stack.
 * 
 * Returns: a #GCancellable from the top of the stack, or %NULL
 * if the stack is empty. 
 **/
GCancellable *
g_cancellable_get_current  (void)
{
  GSList *l;
  
  l = g_static_private_get (&current_cancellable);
  if (l == NULL)
    return NULL;

  return G_CANCELLABLE (l->data);
}

/**
 * g_cancellable_reset:
 * @cancellable: a #GCancellable object.
 * 
 * Resets @cancellable to its uncancelled state. 
 **/
void 
g_cancellable_reset (GCancellable *cancellable)
{
  g_return_if_fail (G_IS_CANCELLABLE (cancellable));

  G_LOCK(cancellable);
  /* Make sure we're not leaving old cancel state around */
  if (cancellable->cancelled)
    {
      char ch;
      if (cancellable->cancel_pipe[0] != -1)
	read (cancellable->cancel_pipe[0], &ch, 1);
      cancellable->cancelled = FALSE;
    }
  G_UNLOCK(cancellable);
}

/**
 * g_cancellable_is_cancelled:
 * @cancellable: a #GCancellable or NULL.
 * 
 * Checks if a cancellable job has been cancelled.
 * 
 * Returns: %TRUE if @cancellable is cancelled, 
 * FALSE if called with %NULL or if item is not cancelled. 
 **/
gboolean
g_cancellable_is_cancelled (GCancellable *cancellable)
{
  return cancellable != NULL && cancellable->cancelled;
}

/**
 * g_cancellable_set_error_if_cancelled:
 * @cancellable: a #GCancellable object.
 * @error: #GError to append error state to.
 * 
 * If the @cancelalble is cancelled, sets the error to notify
 * that the operation was cancelled.
 * 
 * Returns: %TRUE if @cancellable was cancelled, %FALSE if it was not.
 **/
gboolean
g_cancellable_set_error_if_cancelled (GCancellable  *cancellable,
				      GError       **error)
{
  if (g_cancellable_is_cancelled (cancellable))
    {
      g_set_error (error,
		   G_IO_ERROR,
		   G_IO_ERROR_CANCELLED,
		   _("Operation was cancelled"));
      return TRUE;
    }
  
  return FALSE;
}

/**
 * g_cancellable_get_fd:
 * @cancellable: a #GCancellable.
 * 
 * Gets the file descriptor for a cancellable job. This can be used to
 * implement cancellable operations on Unix systems. The returned fd will
 * turn readable when @cancellable is cancelled.
 * 
 * Returns: A valid file descriptor. %-1 if the file descriptor 
 * is not supported, or on errors. 
 **/
int
g_cancellable_get_fd (GCancellable *cancellable)
{
  int fd;
  if (cancellable == NULL)
    return -1;
  
  G_LOCK(cancellable);
  if (!cancellable->allocated_pipe)
    {
      cancellable->allocated_pipe = TRUE;
      g_cancellable_open_pipe (cancellable);
    }
  
  fd = cancellable->cancel_pipe[0];
  G_UNLOCK(cancellable);
  
  return fd;
}

/**
 * g_cancellable_cancel:
 * @cancellable: a #GCancellable object.
 * 
 * Will set @cancellable to cancelled, and will emit the CANCELLED
 * signal.
 *
 * This function is thread-safe. In other words, you can safely call it from
 * another thread than the one running an operation that was passed
 * the @cancellable.
 **/
void
g_cancellable_cancel (GCancellable *cancellable)
{
  gboolean cancel;

  cancel = FALSE;
  
  G_LOCK(cancellable);
  if (cancellable != NULL &&
      !cancellable->cancelled)
    {
      char ch = 'x';
      cancel = TRUE;
      cancellable->cancelled = TRUE;
      if (cancellable->cancel_pipe[1] != -1)
	write (cancellable->cancel_pipe[1], &ch, 1);
    }
  G_UNLOCK(cancellable);

  if (cancel)
    {
      g_object_ref (cancellable);
      g_signal_emit (cancellable, signals[CANCELLED], 0);
      g_object_unref (cancellable);
    }
}

#define __G_CANCELLABLE_C__
#include "gioaliasdef.c"
