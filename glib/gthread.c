/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * gthread.c: MT safety related functions
 * Copyright 1998 Sebastian Wilhelmi; University of Karlsruhe
 *                Owen Taylor
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

/* Prelude {{{1 ----------------------------------------------------------- */

/*
 * Modified by the GLib Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GLib Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GLib at ftp://ftp.gtk.org/pub/gtk/.
 */

/*
 * MT safe
 */

/* implement gthread.h's inline functions */
#define G_IMPLEMENT_INLINES 1
#define __G_THREAD_C__

#include "config.h"

#include "gthread.h"
#include "gthreadprivate.h"

#include <string.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifndef G_OS_WIN32
#include <sys/time.h>
#include <time.h>
#else
#include <windows.h>
#endif /* G_OS_WIN32 */

#include "gslice.h"
#include "gtestutils.h"

/**
 * SECTION:threads
 * @title: Threads
 * @short_description: portable support for threads, mutexes, locks,
 *     conditions and thread private data
 * @see_also: #GThreadPool, #GAsyncQueue
 *
 * Threads act almost like processes, but unlike processes all threads
 * of one process share the same memory. This is good, as it provides
 * easy communication between the involved threads via this shared
 * memory, and it is bad, because strange things (so called
 * "Heisenbugs") might happen if the program is not carefully designed.
 * In particular, due to the concurrent nature of threads, no
 * assumptions on the order of execution of code running in different
 * threads can be made, unless order is explicitly forced by the
 * programmer through synchronization primitives.
 *
 * The aim of the thread-related functions in GLib is to provide a
 * portable means for writing multi-threaded software. There are
 * primitives for mutexes to protect the access to portions of memory
 * (#GMutex, #GRecMutex and #GRWLock). There is a facility to use
 * individual bits for locks (g_bit_lock()). There are primitives
 * for condition variables to allow synchronization of threads (#GCond).
 * There are primitives for thread-private data - data that every thread
 * has a private instance of (#GPrivate). There are
 * facilities for one-time initialization (#GOnce, g_once_init_enter()).
 * Finally there are primitives to create and manage threads (#GThread).
 *
 * The GLib threading system used to be initialized with g_thread_init().
 * This is no longer necessary. Since version 2.32, the GLib threading
 * system is automatically initialized at the start of your program,
 * and all thread-creation functions and synchronization primitives
 * are available right away. It is still possible to do thread-unsafe
 * initialization and setup at the beginning of your program, before
 * creating the first threads.
 *
 * GLib is internally completely thread-safe (all global data is
 * automatically locked), but individual data structure instances are
 * not automatically locked for performance reasons. For example,
 * you must coordinate accesses to the same #GHashTable from multiple
 * threads. The two notable exceptions from this rule are #GMainLoop
 * and #GAsyncQueue, which <emphasis>are</emphasis> thread-safe and
 * need no further application-level locking to be accessed from
 * multiple threads. Most refcounting functions such as g_object_ref()
 * are also thread-safe.
 */

/* G_LOCK Documentation {{{1 ---------------------------------------------- */

/**
 * G_LOCK_DEFINE:
 * @name: the name of the lock
 *
 * The %G_LOCK_* macros provide a convenient interface to #GMutex.
 * #G_LOCK_DEFINE defines a lock. It can appear in any place where
 * variable definitions may appear in programs, i.e. in the first block
 * of a function or outside of functions. The @name parameter will be
 * mangled to get the name of the #GMutex. This means that you
 * can use names of existing variables as the parameter - e.g. the name
 * of the variable you intend to protect with the lock. Look at our
 * <function>give_me_next_number()</function> example using the
 * %G_LOCK_* macros:
 *
 * <example>
 *  <title>Using the %G_LOCK_* convenience macros</title>
 *  <programlisting>
 *   G_LOCK_DEFINE (current_number);
 *
 *   int
 *   give_me_next_number (void)
 *   {
 *     static int current_number = 0;
 *     int ret_val;
 *
 *     G_LOCK (current_number);
 *     ret_val = current_number = calc_next_number (current_number);
 *     G_UNLOCK (current_number);
 *
 *     return ret_val;
 *   }
 *  </programlisting>
 * </example>
 */

/**
 * G_LOCK_DEFINE_STATIC:
 * @name: the name of the lock
 *
 * This works like #G_LOCK_DEFINE, but it creates a static object.
 */

/**
 * G_LOCK_EXTERN:
 * @name: the name of the lock
 *
 * This declares a lock, that is defined with #G_LOCK_DEFINE in another
 * module.
 */

/**
 * G_LOCK:
 * @name: the name of the lock
 *
 * Works like g_mutex_lock(), but for a lock defined with
 * #G_LOCK_DEFINE.
 */

/**
 * G_TRYLOCK:
 * @name: the name of the lock
 * @Returns: %TRUE, if the lock could be locked.
 *
 * Works like g_mutex_trylock(), but for a lock defined with
 * #G_LOCK_DEFINE.
 */

/**
 * G_UNLOCK:
 * @name: the name of the lock
 *
 * Works like g_mutex_unlock(), but for a lock defined with
 * #G_LOCK_DEFINE.
 */

/* GMutex Documentation {{{1 ------------------------------------------ */

/**
 * GMutex:
 *
 * The #GMutex struct is an opaque data structure to represent a mutex
 * (mutual exclusion). It can be used to protect data against shared
 * access. Take for example the following function:
 *
 * <example>
 *  <title>A function which will not work in a threaded environment</title>
 *  <programlisting>
 *   int
 *   give_me_next_number (void)
 *   {
 *     static int current_number = 0;
 *
 *     /<!-- -->* now do a very complicated calculation to calculate the new
 *      * number, this might for example be a random number generator
 *      *<!-- -->/
 *     current_number = calc_next_number (current_number);
 *
 *     return current_number;
 *   }
 *  </programlisting>
 * </example>
 *
 * It is easy to see that this won't work in a multi-threaded
 * application. There current_number must be protected against shared
 * access. A #GMutex can be used as a solution to this problem:
 *
 * <example>
 *  <title>Using GMutex to protected a shared variable</title>
 *  <programlisting>
 *   int
 *   give_me_next_number (void)
 *   {
 *     static GMutex mutex;
 *     static int current_number = 0;
 *     int ret_val;
 *
 *     g_mutex_lock (&amp;mutex);
 *     ret_val = current_number = calc_next_number (current_number);
 *     g_mutex_unlock (&amp;mutex);
 *
 *     return ret_val;
 *   }
 *  </programlisting>
 * </example>
 *
 * Notice that the #GMutex is not initialised to any particular value.
 * Its placement in static storage ensures that it will be initialised
 * to all-zeros, which is appropriate.
 *
 * If a #GMutex is placed in other contexts (eg: embedded in a struct)
 * then it must be explicitly initialised using g_mutex_init().
 *
 * A #GMutex should only be accessed via <function>g_mutex_</function>
 * functions.
 */

/* GRecMutex Documentation {{{1 -------------------------------------- */

/**
 * GRecMutex:
 *
 * The GRecMutex struct is an opaque data structure to represent a
 * recursive mutex. It is similar to a #GMutex with the difference
 * that it is possible to lock a GRecMutex multiple times in the same
 * thread without deadlock. When doing so, care has to be taken to
 * unlock the recursive mutex as often as it has been locked.
 *
 * If a #GRecMutex is allocated in static storage then it can be used
 * without initialisation.  Otherwise, you should call
 * g_rec_mutex_init() on it and g_rec_mutex_clear() when done.
 *
 * A GRecMutex should only be accessed with the
 * <function>g_rec_mutex_</function> functions.
 *
 * Since: 2.32
 */

/* GRWLock Documentation {{{1 ---------------------------------------- */

/**
 * GRWLock:
 *
 * The GRWLock struct is an opaque data structure to represent a
 * reader-writer lock. It is similar to a #GMutex in that it allows
 * multiple threads to coordinate access to a shared resource.
 *
 * The difference to a mutex is that a reader-writer lock discriminates
 * between read-only ('reader') and full ('writer') access. While only
 * one thread at a time is allowed write access (by holding the 'writer'
 * lock via g_rw_lock_writer_lock()), multiple threads can gain
 * simultaneous read-only access (by holding the 'reader' lock via
 * g_rw_lock_reader_lock()).
 *
 * <example>
 *  <title>An array with access functions</title>
 *  <programlisting>
 *   GRWLock lock;
 *   GPtrArray *array;
 *
 *   gpointer
 *   my_array_get (guint index)
 *   {
 *     gpointer retval = NULL;
 *
 *     if (!array)
 *       return NULL;
 *
 *     g_rw_lock_reader_lock (&amp;lock);
 *     if (index &lt; array->len)
 *       retval = g_ptr_array_index (array, index);
 *     g_rw_lock_reader_unlock (&amp;lock);
 *
 *     return retval;
 *   }
 *
 *   void
 *   my_array_set (guint index, gpointer data)
 *   {
 *     g_rw_lock_writer_lock (&amp;lock);
 *
 *     if (!array)
 *       array = g_ptr_array_new (<!-- -->);
 *
 *     if (index >= array->len)
 *       g_ptr_array_set_size (array, index+1);
 *     g_ptr_array_index (array, index) = data;
 *
 *     g_rw_lock_writer_unlock (&amp;lock);
 *   }
 *  </programlisting>
 *  <para>
 *    This example shows an array which can be accessed by many readers
 *    (the <function>my_array_get()</function> function) simultaneously,
 *    whereas the writers (the <function>my_array_set()</function>
 *    function) will only be allowed once at a time and only if no readers
 *    currently access the array. This is because of the potentially
 *    dangerous resizing of the array. Using these functions is fully
 *    multi-thread safe now.
 *  </para>
 * </example>
 *
 * If a #GRWLock is allocated in static storage then it can be used
 * without initialisation.  Otherwise, you should call
 * g_rw_lock_init() on it and g_rw_lock_clear() when done.
 *
 * A GRWLock should only be accessed with the
 * <function>g_rw_lock_</function> functions.
 *
 * Since: 2.32
 */

/* GCond Documentation {{{1 ------------------------------------------ */

/**
 * GCond:
 *
 * The #GCond struct is an opaque data structure that represents a
 * condition. Threads can block on a #GCond if they find a certain
 * condition to be false. If other threads change the state of this
 * condition they signal the #GCond, and that causes the waiting
 * threads to be woken up.
 *
 * <example>
 *  <title>
 *   Using GCond to block a thread until a condition is satisfied
 *  </title>
 *  <programlisting>
 *   GCond* data_cond = NULL; /<!-- -->* Must be initialized somewhere *<!-- -->/
 *   GMutex* data_mutex = NULL; /<!-- -->* Must be initialized somewhere *<!-- -->/
 *   gpointer current_data = NULL;
 *
 *   void
 *   push_data (gpointer data)
 *   {
 *     g_mutex_lock (data_mutex);
 *     current_data = data;
 *     g_cond_signal (data_cond);
 *     g_mutex_unlock (data_mutex);
 *   }
 *
 *   gpointer
 *   pop_data (void)
 *   {
 *     gpointer data;
 *
 *     g_mutex_lock (data_mutex);
 *     while (!current_data)
 *       g_cond_wait (data_cond, data_mutex);
 *     data = current_data;
 *     current_data = NULL;
 *     g_mutex_unlock (data_mutex);
 *
 *     return data;
 *   }
 *  </programlisting>
 * </example>
 *
 * Whenever a thread calls pop_data() now, it will wait until
 * current_data is non-%NULL, i.e. until some other thread
 * has called push_data().
 *
 * <note><para>It is important to use the g_cond_wait() and
 * g_cond_timed_wait() functions only inside a loop which checks for the
 * condition to be true.  It is not guaranteed that the waiting thread
 * will find the condition fulfilled after it wakes up, even if the
 * signaling thread left the condition in that state: another thread may
 * have altered the condition before the waiting thread got the chance
 * to be woken up, even if the condition itself is protected by a
 * #GMutex, like above.</para></note>
 *
 * If a #GCond is allocated in static storage then it can be used
 * without initialisation.  Otherwise, you should call g_cond_init() on
 * it and g_cond_clear() when done.
 *
 * A #GCond should only be accessed via the <function>g_cond_</function>
 * functions.
 */

/* GThread Documentation {{{1 ---------------------------------------- */

/**
 * GThread:
 *
 * The #GThread struct represents a running thread. This struct
 * is returned by g_thread_new() or g_thread_new_full(). You can
 * obtain the #GThread struct representing the current thead by
 * calling g_thread_self().
 *
 * The structure is opaque -- none of its fields may be directly
 * accessed.
 */

/**
 * GThreadFunc:
 * @data: data passed to the thread
 *
 * Specifies the type of the @func functions passed to
 * g_thread_new() or g_thread_new_full().
 *
 * If the thread is joinable, the return value of this function
 * is returned by a g_thread_join() call waiting for the thread.
 * If the thread is not joinable, the return value is ignored.
 *
 * Returns: the return value of the thread
 */

/**
 * g_thread_supported:
 *
 * This macro returns %TRUE if the thread system is initialized,
 * and %FALSE if it is not.
 *
 * For language bindings, g_thread_get_initialized() provides
 * the same functionality as a function.
 *
 * Returns: %TRUE, if the thread system is initialized
 */

/* GThreadError {{{1 ------------------------------------------------------- */
/**
 * GThreadError:
 * @G_THREAD_ERROR_AGAIN: a thread couldn't be created due to resource
 *                        shortage. Try again later.
 *
 * Possible errors of thread related functions.
 **/

/**
 * G_THREAD_ERROR:
 *
 * The error domain of the GLib thread subsystem.
 **/
GQuark
g_thread_error_quark (void)
{
  return g_quark_from_static_string ("g_thread_error");
}

/* Local Data {{{1 -------------------------------------------------------- */

GMutex           g_once_mutex;
static GCond     g_once_cond;
static GSList   *g_once_init_list = NULL;

static void g_thread_cleanup (gpointer data);
static GPrivate     g_thread_specific_private = G_PRIVATE_INIT (g_thread_cleanup);

G_LOCK_DEFINE_STATIC (g_thread_new);

/* GOnce {{{1 ------------------------------------------------------------- */

/**
 * GOnce:
 * @status: the status of the #GOnce
 * @retval: the value returned by the call to the function, if @status
 *          is %G_ONCE_STATUS_READY
 *
 * A #GOnce struct controls a one-time initialization function. Any
 * one-time initialization function must have its own unique #GOnce
 * struct.
 *
 * Since: 2.4
 */

/**
 * G_ONCE_INIT:
 *
 * A #GOnce must be initialized with this macro before it can be used.
 *
 * |[
 *   GOnce my_once = G_ONCE_INIT;
 * ]|
 *
 * Since: 2.4
 */

/**
 * GOnceStatus:
 * @G_ONCE_STATUS_NOTCALLED: the function has not been called yet.
 * @G_ONCE_STATUS_PROGRESS: the function call is currently in progress.
 * @G_ONCE_STATUS_READY: the function has been called.
 *
 * The possible statuses of a one-time initialization function
 * controlled by a #GOnce struct.
 *
 * Since: 2.4
 */

/**
 * g_once:
 * @once: a #GOnce structure
 * @func: the #GThreadFunc function associated to @once. This function
 *        is called only once, regardless of the number of times it and
 *        its associated #GOnce struct are passed to g_once().
 * @arg: data to be passed to @func
 *
 * The first call to this routine by a process with a given #GOnce
 * struct calls @func with the given argument. Thereafter, subsequent
 * calls to g_once()  with the same #GOnce struct do not call @func
 * again, but return the stored result of the first call. On return
 * from g_once(), the status of @once will be %G_ONCE_STATUS_READY.
 *
 * For example, a mutex or a thread-specific data key must be created
 * exactly once. In a threaded environment, calling g_once() ensures
 * that the initialization is serialized across multiple threads.
 *
 * Calling g_once() recursively on the same #GOnce struct in
 * @func will lead to a deadlock.
 *
 * |[
 *   gpointer
 *   get_debug_flags (void)
 *   {
 *     static GOnce my_once = G_ONCE_INIT;
 *
 *     g_once (&my_once, parse_debug_flags, NULL);
 *
 *     return my_once.retval;
 *   }
 * ]|
 *
 * Since: 2.4
 */
gpointer
g_once_impl (GOnce       *once,
	     GThreadFunc  func,
	     gpointer     arg)
{
  g_mutex_lock (&g_once_mutex);

  while (once->status == G_ONCE_STATUS_PROGRESS)
    g_cond_wait (&g_once_cond, &g_once_mutex);

  if (once->status != G_ONCE_STATUS_READY)
    {
      once->status = G_ONCE_STATUS_PROGRESS;
      g_mutex_unlock (&g_once_mutex);

      once->retval = func (arg);

      g_mutex_lock (&g_once_mutex);
      once->status = G_ONCE_STATUS_READY;
      g_cond_broadcast (&g_once_cond);
    }

  g_mutex_unlock (&g_once_mutex);

  return once->retval;
}

/**
 * g_once_init_enter:
 * @value_location: location of a static initializable variable
 *     containing 0
 *
 * Function to be called when starting a critical initialization
 * section. The argument @value_location must point to a static
 * 0-initialized variable that will be set to a value other than 0 at
 * the end of the initialization section. In combination with
 * g_once_init_leave() and the unique address @value_location, it can
 * be ensured that an initialization section will be executed only once
 * during a program's life time, and that concurrent threads are
 * blocked until initialization completed. To be used in constructs
 * like this:
 *
 * |[
 *   static gsize initialization_value = 0;
 *
 *   if (g_once_init_enter (&amp;initialization_value))
 *     {
 *       gsize setup_value = 42; /&ast;* initialization code here *&ast;/
 *
 *       g_once_init_leave (&amp;initialization_value, setup_value);
 *     }
 *
 *   /&ast;* use initialization_value here *&ast;/
 * ]|
 *
 * Returns: %TRUE if the initialization section should be entered,
 *     %FALSE and blocks otherwise
 *
 * Since: 2.14
 */
gboolean
(g_once_init_enter) (volatile void *pointer)
{
  volatile gsize *value_location = pointer;
  gboolean need_init = FALSE;
  g_mutex_lock (&g_once_mutex);
  if (g_atomic_pointer_get (value_location) == NULL)
    {
      if (!g_slist_find (g_once_init_list, (void*) value_location))
        {
          need_init = TRUE;
          g_once_init_list = g_slist_prepend (g_once_init_list, (void*) value_location);
        }
      else
        do
          g_cond_wait (&g_once_cond, &g_once_mutex);
        while (g_slist_find (g_once_init_list, (void*) value_location));
    }
  g_mutex_unlock (&g_once_mutex);
  return need_init;
}

/**
 * g_once_init_leave:
 * @value_location: location of a static initializable variable
 *     containing 0
 * @result: new non-0 value for *@value_location
 *
 * Counterpart to g_once_init_enter(). Expects a location of a static
 * 0-initialized initialization variable, and an initialization value
 * other than 0. Sets the variable to the initialization value, and
 * releases concurrent threads blocking in g_once_init_enter() on this
 * initialization variable.
 *
 * Since: 2.14
 */
void
(g_once_init_leave) (volatile void *pointer,
                     gsize          result)
{
  volatile gsize *value_location = pointer;

  g_return_if_fail (g_atomic_pointer_get (value_location) == NULL);
  g_return_if_fail (result != 0);
  g_return_if_fail (g_once_init_list != NULL);

  g_atomic_pointer_set (value_location, result);
  g_mutex_lock (&g_once_mutex);
  g_once_init_list = g_slist_remove (g_once_init_list, (void*) value_location);
  g_cond_broadcast (&g_once_cond);
  g_mutex_unlock (&g_once_mutex);
}

/* GThread {{{1 -------------------------------------------------------- */

static void
g_thread_cleanup (gpointer data)
{
  if (data)
    {
      GRealThread* thread = data;

      /* We only free the thread structure if it isn't joinable.
       * If it is, the structure is freed in g_thread_join()
       */
      if (!thread->thread.joinable)
        {
          /* Just to make sure, this isn't used any more */
          g_system_thread_assign (thread->system_thread, zero_thread);
          g_free (thread);
        }
    }
}

static gpointer
g_thread_create_proxy (gpointer data)
{
  GRealThread* thread = data;

  g_assert (data);

  if (thread->name)
    g_system_thread_set_name (thread->name);

  /* This has to happen before G_LOCK, as that might call g_thread_self */
  g_private_set (&g_thread_specific_private, data);

  if (thread->setup_func)
    thread->setup_func (thread);

  /* The lock makes sure that thread->system_thread is written,
   * before thread->thread.func is called. See g_thread_new_internal().
   */
  G_LOCK (g_thread_new);
  G_UNLOCK (g_thread_new);

  thread->retval = thread->thread.func (thread->thread.data);

  return NULL;
}

/**
 * g_thread_new:
 * @name: a name for the new thread
 * @func: a function to execute in the new thread
 * @data: an argument to supply to the new thread
 * @joinable: should this thread be joinable?
 * @error: return location for error
 *
 * This function creates a new thread. The new thread starts by invoking
 * @func with the argument data. The thread will run until @func returns
 * or until g_thread_exit() is called from the new thread.
 *
 * The @name can be useful for discriminating threads in
 * a debugger. Some systems restrict the length of @name to
 * 16 bytes.
 *
 * If @joinable is %TRUE, you can wait for this thread's termination
 * calling g_thread_join(). Resources for a joinable thread are not
 * fully released until g_thread_join() is called for that thread.
 * Otherwise the thread will just disappear when it terminates.
 *
 * @error can be %NULL to ignore errors, or non-%NULL to report errors.
 * The error is set, if and only if the function returns %NULL.
 *
 * Returns: the new #GThread, or %NULL if an error occurred
 *
 * Since: 2.32
 */
GThread *
g_thread_new (const gchar  *name,
              GThreadFunc   func,
              gpointer      data,
              gboolean      joinable,
              GError      **error)
{
  return g_thread_new_internal (name, func, data, joinable, 0, FALSE, error);
}

/**
 * g_thread_new_full:
 * @name: a name for the new thread
 * @func: a function to execute in the new thread
 * @data: an argument to supply to the new thread
 * @joinable: should this thread be joinable?
 * @stack_size: a stack size for the new thread
 * @error: return location for error
 *
 * This function creates a new thread. The new thread starts by
 * invoking @func with the argument data. The thread will run
 * until @func returns or until g_thread_exit() is called.
 *
 * The @name can be useful for discriminating threads in
 * a debugger. Some systems restrict the length of @name to
 * 16 bytes.
 *
 * If the underlying thread implementation supports it, the thread
 * gets a stack size of @stack_size or the default value for the
 * current platform, if @stack_size is 0. Note that you should only
 * use a non-zero @stack_size if you really can't use the default.
 * In most cases, using g_thread_new() (which doesn't take a
 * @stack_size) is better.
 *
 * If @joinable is %TRUE, you can wait for this thread's termination
 * calling g_thread_join(). Resources for a joinable thread are not
 * fully released until g_thread_join() is called for that thread.
 * Otherwise the thread will just disappear when it terminates.
 *
 * @error can be %NULL to ignore errors, or non-%NULL to report errors.
 * The error is set, if and only if the function returns %NULL.
 *
 * Returns: the new #GThread, or %NULL if an error occurred
 *
 * Since: 2.32
 */
GThread *
g_thread_new_full (const gchar  *name,
                   GThreadFunc   func,
                   gpointer      data,
                   gboolean      joinable,
                   gsize         stack_size,
                   GError      **error)
{
  return g_thread_new_internal (name, func, data, joinable, stack_size, FALSE, error);
}

GThread *
g_thread_new_internal (const gchar   *name,
                       GThreadFunc    func,
                       gpointer       data,
                       gboolean       joinable,
                       gsize          stack_size,
                       GThreadSetup   setup_func,
                       GError       **error)
{
  GRealThread *result;
  GError *local_error = NULL;

  g_return_val_if_fail (func != NULL, NULL);

  result = g_new0 (GRealThread, 1);

  result->thread.joinable = joinable;
  result->thread.func = func;
  result->thread.data = data;
  result->setup_func = setup_func;
  result->name = name;
  G_LOCK (g_thread_new);
  g_system_thread_create (g_thread_create_proxy, result,
                          stack_size, joinable,
                          &result->system_thread, &local_error);
  G_UNLOCK (g_thread_new);

  if (local_error)
    {
      g_propagate_error (error, local_error);
      g_free (result);
      return NULL;
    }

  return (GThread*) result;
}

/**
 * g_thread_exit:
 * @retval: the return value of this thread
 *
 * Terminates the current thread.
 *
 * If another thread is waiting for that thread using g_thread_join()
 * and the current thread is joinable, the waiting thread will be woken
 * up and get @retval as the return value of g_thread_join(). If the
 * current thread is not joinable, @retval is ignored.
 *
 * Calling <literal>g_thread_exit (retval)</literal> is equivalent to
 * returning @retval from the function @func, as given to g_thread_new().
 *
 * <note><para>Never call g_thread_exit() from within a thread of a
 * #GThreadPool, as that will mess up the bookkeeping and lead to funny
 * and unwanted results.</para></note>
 */
void
g_thread_exit (gpointer retval)
{
  GRealThread* real = (GRealThread*) g_thread_self ();
  real->retval = retval;

  g_system_thread_exit ();
}

/**
 * g_thread_join:
 * @thread: a joinable #GThread
 *
 * Waits until @thread finishes, i.e. the function @func, as
 * given to g_thread_new(), returns or g_thread_exit() is called.
 * If @thread has already terminated, then g_thread_join()
 * returns immediately. @thread must be joinable.
 *
 * Any thread can wait for any other (joinable) thread by calling
 * g_thread_join(), not just its 'creator'. Calling g_thread_join()
 * from multiple threads for the same @thread leads to undefined
 * behaviour.
 *
 * The value returned by @func or given to g_thread_exit() is
 * returned by this function.
 *
 * All resources of @thread including the #GThread struct are
 * released before g_thread_join() returns.
 *
 * Returns: the return value of the thread
 */
gpointer
g_thread_join (GThread *thread)
{
  GRealThread *real = (GRealThread*) thread;
  gpointer retval;

  g_return_val_if_fail (thread, NULL);
  g_return_val_if_fail (thread->joinable, NULL);
  g_return_val_if_fail (!g_system_thread_equal (&real->system_thread, &zero_thread), NULL);

  g_system_thread_join (&real->system_thread);

  retval = real->retval;

  /* Just to make sure, this isn't used any more */
  thread->joinable = 0;
  g_system_thread_assign (real->system_thread, zero_thread);

  /* the thread structure for non-joinable threads is freed upon
   * thread end. We free the memory here. This will leave a loose end,
   * if a joinable thread is not joined.
   */
  g_free (thread);

  return retval;
}

/**
 * g_thread_self:
 *
 * This functions returns the #GThread corresponding to the
 * current thread.
 *
 * Returns: the #GThread representing the current thread
 */
GThread*
g_thread_self (void)
{
  GRealThread* thread = g_private_get (&g_thread_specific_private);

  if (!thread)
    {
      /* If no thread data is available, provide and set one.
       * This can happen for the main thread and for threads
       * that are not created by GLib.
       */
      thread = g_new0 (GRealThread, 1);
      thread->thread.joinable = FALSE; /* This is a safe guess */
      thread->thread.func = NULL;
      thread->thread.data = NULL;

      g_system_thread_self (&thread->system_thread);

      g_private_set (&g_thread_specific_private, thread);
    }

  return (GThread*)thread;
}

/* Epilogue {{{1 */
/* vim: set foldmethod=marker: */
