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

#include "config.h"

#include "gmessages.h"
#include "gmain.h"
#include "gthread.h"
#include "gthreadprivate.h"
#include "deprecated/gthread.h"

/* {{{1 Documentation */

/**
 * GThreadPriority:
 * @G_THREAD_PRIORITY_LOW: a priority lower than normal
 * @G_THREAD_PRIORITY_NORMAL: the default priority
 * @G_THREAD_PRIORITY_HIGH: a priority higher than normal
 * @G_THREAD_PRIORITY_URGENT: the highest priority
 *
 * Deprecated:2.32: thread priorities no longer have any effect.
 */

/**
 * GThreadFunctions:
 * @mutex_new: virtual function pointer for g_mutex_new()
 * @mutex_lock: virtual function pointer for g_mutex_lock()
 * @mutex_trylock: virtual function pointer for g_mutex_trylock()
 * @mutex_unlock: virtual function pointer for g_mutex_unlock()
 * @mutex_free: virtual function pointer for g_mutex_free()
 * @cond_new: virtual function pointer for g_cond_new()
 * @cond_signal: virtual function pointer for g_cond_signal()
 * @cond_broadcast: virtual function pointer for g_cond_broadcast()
 * @cond_wait: virtual function pointer for g_cond_wait()
 * @cond_timed_wait: virtual function pointer for g_cond_timed_wait()
 * @cond_free: virtual function pointer for g_cond_free()
 * @private_new: virtual function pointer for g_private_new()
 * @private_get: virtual function pointer for g_private_get()
 * @private_set: virtual function pointer for g_private_set()
 * @thread_create: virtual function pointer for g_thread_create()
 * @thread_yield: virtual function pointer for g_thread_yield()
 * @thread_join: virtual function pointer for g_thread_join()
 * @thread_exit: virtual function pointer for g_thread_exit()
 * @thread_set_priority: virtual function pointer for
 *                       g_thread_set_priority()
 * @thread_self: virtual function pointer for g_thread_self()
 * @thread_equal: used internally by recursive mutex locks and by some
 *                assertion checks
 *
 * This function table is no longer used by g_thread_init()
 * to initialize the thread system.
 */

/* {{{1 Exported Variables */

gboolean g_thread_use_default_impl = TRUE;

GThreadFunctions g_thread_functions_for_glib_use =
{
  g_mutex_new,
  g_mutex_lock,
  g_mutex_trylock,
  g_mutex_unlock,
  g_mutex_free,
  g_cond_new,
  g_cond_signal,
  g_cond_broadcast,
  g_cond_wait,
  g_cond_timed_wait,
  g_cond_free,
  g_private_new,
  g_private_get,
  g_private_set,
  NULL,
  g_thread_yield,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
};

static guint64
gettime (void)
{
  return g_get_monotonic_time () * 1000;
}

guint64 (*g_thread_gettime) (void) = gettime;

/* Misc. GThread functions {{{1 */

/**
 * g_thread_set_priority:
 * @thread: a #GThread.
 * @priority: ignored
 *
 * This function does nothing.
 *
 * Deprecated:2.32: Thread priorities no longer have any effect.
 */
void
g_thread_set_priority (GThread         *thread,
                       GThreadPriority  priority)
{
}

/**
 * g_thread_create_full:
 * @func: a function to execute in the new thread.
 * @data: an argument to supply to the new thread.
 * @stack_size: a stack size for the new thread.
 * @joinable: should this thread be joinable?
 * @bound: ignored
 * @priority: ignored
 * @error: return location for error.
 * @Returns: the new #GThread on success.
 *
 * This function creates a new thread.
 *
 * Deprecated:2.32: The @bound and @priority arguments are now ignored.
 * Use g_thread_create() or g_thread_create_with_stack_size() instead.
 */
GThread *
g_thread_create_full (GThreadFunc       func,
                      gpointer          data,
                      gulong            stack_size,
                      gboolean          joinable,
                      gboolean          bound,
                      GThreadPriority   priority,
                      GError          **error)
{
  return g_thread_create_with_stack_size (func, data, joinable, stack_size, error);
}

/* GStaticMutex {{{1 ------------------------------------------------------ */

/**
 * GStaticMutex:
 *
 * A #GStaticMutex works like a #GMutex.
 *
 * Prior to GLib 2.32, GStaticMutex had the significant advantage
 * that it doesn't need to be created at run-time, but can be defined
 * at compile-time. Since 2.32, #GMutex can be statically allocated
 * as well, and GStaticMutex has been deprecated.
 *
 * Here is a version of our give_me_next_number() example using
 * a GStaticMutex.
 *
 * <example>
 *  <title>
 *   Using <structname>GStaticMutex</structname>
 *   to simplify thread-safe programming
 *  </title>
 *  <programlisting>
 *   int
 *   give_me_next_number (void)
 *   {
 *     static int current_number = 0;
 *     int ret_val;
 *     static GStaticMutex mutex = G_STATIC_MUTEX_INIT;
 *
 *     g_static_mutex_lock (&amp;mutex);
 *     ret_val = current_number = calc_next_number (current_number);
 *     g_static_mutex_unlock (&amp;mutex);
 *
 *     return ret_val;
 *   }
 *  </programlisting>
 * </example>
 *
 * Sometimes you would like to dynamically create a mutex. If you don't
 * want to require prior calling to g_thread_init(), because your code
 * should also be usable in non-threaded programs, you are not able to
 * use g_mutex_new() and thus #GMutex, as that requires a prior call to
 * g_thread_init(). In theses cases you can also use a #GStaticMutex.
 * It must be initialized with g_static_mutex_init() before using it
 * and freed with with g_static_mutex_free() when not needed anymore to
 * free up any allocated resources.
 *
 * Even though #GStaticMutex is not opaque, it should only be used with
 * the following functions, as it is defined differently on different
 * platforms.
 *
 * All of the <function>g_static_mutex_*</function> functions apart
 * from <function>g_static_mutex_get_mutex</function> can also be used
 * even if g_thread_init() has not yet been called. Then they do
 * nothing, apart from <function>g_static_mutex_trylock</function>,
 * which does nothing but returning %TRUE.
 *
 * <note><para>All of the <function>g_static_mutex_*</function>
 * functions are actually macros. Apart from taking their addresses, you
 * can however use them as if they were functions.</para></note>
 **/

/**
 * G_STATIC_MUTEX_INIT:
 *
 * A #GStaticMutex must be initialized with this macro, before it can
 * be used. This macro can used be to initialize a variable, but it
 * cannot be assigned to a variable. In that case you have to use
 * g_static_mutex_init().
 *
 * |[
 * GStaticMutex my_mutex = G_STATIC_MUTEX_INIT;
 * ]|
 **/

/**
 * g_static_mutex_init:
 * @mutex: a #GStaticMutex to be initialized.
 *
 * Initializes @mutex.
 * Alternatively you can initialize it with #G_STATIC_MUTEX_INIT.
 *
 * Deprecated: 2.32: Use g_mutex_init()
 */
void
g_static_mutex_init (GStaticMutex *mutex)
{
  static const GStaticMutex init_mutex = G_STATIC_MUTEX_INIT;

  g_return_if_fail (mutex);

  *mutex = init_mutex;
}

/* IMPLEMENTATION NOTE:
 *
 * On some platforms a GStaticMutex is actually a normal GMutex stored
 * inside of a structure instead of being allocated dynamically.  We can
 * only do this for platforms on which we know, in advance, how to
 * allocate (size) and initialise (value) that memory.
 *
 * On other platforms, a GStaticMutex is nothing more than a pointer to
 * a GMutex.  In that case, the first access we make to the static mutex
 * must first allocate the normal GMutex and store it into the pointer.
 *
 * configure.ac writes macros into glibconfig.h to determine if
 * g_static_mutex_get_mutex() accesses the structure in memory directly
 * (on platforms where we are able to do that) or if it ends up here,
 * where we may have to allocate the GMutex before returning it.
 */

/**
 * g_static_mutex_get_mutex:
 * @mutex: a #GStaticMutex.
 * @Returns: the #GMutex corresponding to @mutex.
 *
 * For some operations (like g_cond_wait()) you must have a #GMutex
 * instead of a #GStaticMutex. This function will return the
 * corresponding #GMutex for @mutex.
 *
 * Deprecated: 2.32: Just use a #GMutex
 */
GMutex *
g_static_mutex_get_mutex_impl (GMutex** mutex)
{
  GMutex *result;

  if (!g_thread_supported ())
    return NULL;

  result = g_atomic_pointer_get (mutex);

  if (!result)
    {
      g_mutex_lock (&g_once_mutex);

      result = *mutex;
      if (!result)
        {
          result = g_mutex_new ();
          g_atomic_pointer_set (mutex, result);
        }

      g_mutex_unlock (&g_once_mutex);
    }

  return result;
}

/* IMPLEMENTATION NOTE:
 *
 * g_static_mutex_lock(), g_static_mutex_trylock() and
 * g_static_mutex_unlock() are all preprocessor macros that wrap the
 * corresponding g_mutex_*() function around a call to
 * g_static_mutex_get_mutex().
 */

/**
 * g_static_mutex_lock:
 * @mutex: a #GStaticMutex.
 *
 * Works like g_mutex_lock(), but for a #GStaticMutex.
 *
 * Deprecated: 2.32: Use g_mutex_lock()
 */

/**
 * g_static_mutex_trylock:
 * @mutex: a #GStaticMutex.
 * @Returns: %TRUE, if the #GStaticMutex could be locked.
 *
 * Works like g_mutex_trylock(), but for a #GStaticMutex.
 *
 * Deprecated: 2.32: Use g_mutex_trylock()
 */

/**
 * g_static_mutex_unlock:
 * @mutex: a #GStaticMutex.
 *
 * Works like g_mutex_unlock(), but for a #GStaticMutex.
 *
 * Deprecated: 2.32: Use g_mutex_unlock()
 */

/**
 * g_static_mutex_free:
 * @mutex: a #GStaticMutex to be freed.
 *
 * Releases all resources allocated to @mutex.
 *
 * You don't have to call this functions for a #GStaticMutex with an
 * unbounded lifetime, i.e. objects declared 'static', but if you have
 * a #GStaticMutex as a member of a structure and the structure is
 * freed, you should also free the #GStaticMutex.
 *
 * <note><para>Calling g_static_mutex_free() on a locked mutex may
 * result in undefined behaviour.</para></note>
 *
 * Deprecated: 2.32: Use g_mutex_free()
 */
void
g_static_mutex_free (GStaticMutex* mutex)
{
  GMutex **runtime_mutex;

  g_return_if_fail (mutex);

  /* The runtime_mutex is the first (or only) member of GStaticMutex,
   * see both versions (of glibconfig.h) in configure.ac. Note, that
   * this variable is NULL, if g_thread_init() hasn't been called or
   * if we're using the default thread implementation and it provides
   * static mutexes. */
  runtime_mutex = ((GMutex**)mutex);

  if (*runtime_mutex)
    g_mutex_free (*runtime_mutex);

  *runtime_mutex = NULL;
}

/* {{{1 GStaticRecMutex */

/**
 * GStaticRecMutex:
 *
 * A #GStaticRecMutex works like a #GStaticMutex, but it can be locked
 * multiple times by one thread. If you enter it n times, you have to
 * unlock it n times again to let other threads lock it. An exception
 * is the function g_static_rec_mutex_unlock_full(): that allows you to
 * unlock a #GStaticRecMutex completely returning the depth, (i.e. the
 * number of times this mutex was locked). The depth can later be used
 * to restore the state of the #GStaticRecMutex by calling
 * g_static_rec_mutex_lock_full(). In GLib 2.32, #GStaticRecMutex has
 * been deprecated in favor of #GRecMutex.
 *
 * Even though #GStaticRecMutex is not opaque, it should only be used
 * with the following functions.
 *
 * All of the <function>g_static_rec_mutex_*</function> functions can
 * be used even if g_thread_init() has not been called. Then they do
 * nothing, apart from <function>g_static_rec_mutex_trylock</function>,
 * which does nothing but returning %TRUE.
 **/

/**
 * G_STATIC_REC_MUTEX_INIT:
 *
 * A #GStaticRecMutex must be initialized with this macro before it can
 * be used. This macro can used be to initialize a variable, but it
 * cannot be assigned to a variable. In that case you have to use
 * g_static_rec_mutex_init().
 *
 * |[
 *   GStaticRecMutex my_mutex = G_STATIC_REC_MUTEX_INIT;
 * ]|
 */

/**
 * g_static_rec_mutex_init:
 * @mutex: a #GStaticRecMutex to be initialized.
 *
 * A #GStaticRecMutex must be initialized with this function before it
 * can be used. Alternatively you can initialize it with
 * #G_STATIC_REC_MUTEX_INIT.
 *
 * Deprecated: 2.32: Use g_rec_mutex_init()
 */
void
g_static_rec_mutex_init (GStaticRecMutex *mutex)
{
  static const GStaticRecMutex init_mutex = G_STATIC_REC_MUTEX_INIT;

  g_return_if_fail (mutex);

  *mutex = init_mutex;
}

/**
 * g_static_rec_mutex_lock:
 * @mutex: a #GStaticRecMutex to lock.
 *
 * Locks @mutex. If @mutex is already locked by another thread, the
 * current thread will block until @mutex is unlocked by the other
 * thread. If @mutex is already locked by the calling thread, this
 * functions increases the depth of @mutex and returns immediately.
 *
 * Deprecated: 2.32: Use g_rec_mutex_lock()
 */
void
g_static_rec_mutex_lock (GStaticRecMutex* mutex)
{
  GSystemThread self;

  g_return_if_fail (mutex);

  if (!g_thread_supported ())
    return;

  g_system_thread_self (&self);

  if (g_system_thread_equal (&self, &mutex->owner))
    {
      mutex->depth++;
      return;
    }
  g_static_mutex_lock (&mutex->mutex);
  g_system_thread_assign (mutex->owner, self);
  mutex->depth = 1;
}

/**
 * g_static_rec_mutex_trylock:
 * @mutex: a #GStaticRecMutex to lock.
 * @Returns: %TRUE, if @mutex could be locked.
 *
 * Tries to lock @mutex. If @mutex is already locked by another thread,
 * it immediately returns %FALSE. Otherwise it locks @mutex and returns
 * %TRUE. If @mutex is already locked by the calling thread, this
 * functions increases the depth of @mutex and immediately returns
 * %TRUE.
 *
 * Deprecated: 2.32: Use g_rec_mutex_trylock()
 */
gboolean
g_static_rec_mutex_trylock (GStaticRecMutex* mutex)
{
  GSystemThread self;

  g_return_val_if_fail (mutex, FALSE);

  if (!g_thread_supported ())
    return TRUE;

  g_system_thread_self (&self);

  if (g_system_thread_equal (&self, &mutex->owner))
    {
      mutex->depth++;
      return TRUE;
    }

  if (!g_static_mutex_trylock (&mutex->mutex))
    return FALSE;

  g_system_thread_assign (mutex->owner, self);
  mutex->depth = 1;
  return TRUE;
}

/**
 * g_static_rec_mutex_unlock:
 * @mutex: a #GStaticRecMutex to unlock.
 *
 * Unlocks @mutex. Another thread will be allowed to lock @mutex only
 * when it has been unlocked as many times as it had been locked
 * before. If @mutex is completely unlocked and another thread is
 * blocked in a g_static_rec_mutex_lock() call for @mutex, it will be
 * woken and can lock @mutex itself.
 *
 * Deprecated: 2.32: Use g_rec_mutex_unlock()
 */
void
g_static_rec_mutex_unlock (GStaticRecMutex* mutex)
{
  g_return_if_fail (mutex);

  if (!g_thread_supported ())
    return;

  if (mutex->depth > 1)
    {
      mutex->depth--;
      return;
    }
  g_system_thread_assign (mutex->owner, zero_thread);
  g_static_mutex_unlock (&mutex->mutex);
}

/**
 * g_static_rec_mutex_lock_full:
 * @mutex: a #GStaticRecMutex to lock.
 * @depth: number of times this mutex has to be unlocked to be
 *         completely unlocked.
 *
 * Works like calling g_static_rec_mutex_lock() for @mutex @depth times.
 *
 * Deprecated: 2.32: Use g_rec_mutex_lock()
 */
void
g_static_rec_mutex_lock_full (GStaticRecMutex *mutex,
                              guint            depth)
{
  GSystemThread self;
  g_return_if_fail (mutex);

  if (!g_thread_supported ())
    return;

  if (depth == 0)
    return;

  g_system_thread_self (&self);

  if (g_system_thread_equal (&self, &mutex->owner))
    {
      mutex->depth += depth;
      return;
    }
  g_static_mutex_lock (&mutex->mutex);
  g_system_thread_assign (mutex->owner, self);
  mutex->depth = depth;
}

/**
 * g_static_rec_mutex_unlock_full:
 * @mutex: a #GStaticRecMutex to completely unlock.
 * @Returns: number of times @mutex has been locked by the current
 *           thread.
 *
 * Completely unlocks @mutex. If another thread is blocked in a
 * g_static_rec_mutex_lock() call for @mutex, it will be woken and can
 * lock @mutex itself. This function returns the number of times that
 * @mutex has been locked by the current thread. To restore the state
 * before the call to g_static_rec_mutex_unlock_full() you can call
 * g_static_rec_mutex_lock_full() with the depth returned by this
 * function.
 *
 * Deprecated: 2.32: Use g_rec_mutex_unlock()
 */
guint
g_static_rec_mutex_unlock_full (GStaticRecMutex *mutex)
{
  guint depth;

  g_return_val_if_fail (mutex, 0);

  if (!g_thread_supported ())
    return 1;

  depth = mutex->depth;

  g_system_thread_assign (mutex->owner, zero_thread);
  mutex->depth = 0;
  g_static_mutex_unlock (&mutex->mutex);

  return depth;
}

/**
 * g_static_rec_mutex_free:
 * @mutex: a #GStaticRecMutex to be freed.
 *
 * Releases all resources allocated to a #GStaticRecMutex.
 *
 * You don't have to call this functions for a #GStaticRecMutex with an
 * unbounded lifetime, i.e. objects declared 'static', but if you have
 * a #GStaticRecMutex as a member of a structure and the structure is
 * freed, you should also free the #GStaticRecMutex.
 *
 * Deprecated: 2.32: Use g_rec_mutex_clear()
 */
void
g_static_rec_mutex_free (GStaticRecMutex *mutex)
{
  g_return_if_fail (mutex);

  g_static_mutex_free (&mutex->mutex);
}

/* GStaticRWLock {{{1 ----------------------------------------------------- */

/**
 * GStaticRWLock:
 *
 * The #GStaticRWLock struct represents a read-write lock. A read-write
 * lock can be used for protecting data that some portions of code only
 * read from, while others also write. In such situations it is
 * desirable that several readers can read at once, whereas of course
 * only one writer may write at a time. Take a look at the following
 * example:
 *
 * <example>
 *  <title>An array with access functions</title>
 *  <programlisting>
 *   GStaticRWLock rwlock = G_STATIC_RW_LOCK_INIT;
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
 *     g_static_rw_lock_reader_lock (&amp;rwlock);
 *     if (index &lt; array->len)
 *       retval = g_ptr_array_index (array, index);
 *     g_static_rw_lock_reader_unlock (&amp;rwlock);
 *
 *     return retval;
 *   }
 *
 *   void
 *   my_array_set (guint index, gpointer data)
 *   {
 *     g_static_rw_lock_writer_lock (&amp;rwlock);
 *
 *     if (!array)
 *       array = g_ptr_array_new (<!-- -->);
 *
 *     if (index >= array->len)
 *       g_ptr_array_set_size (array, index+1);
 *     g_ptr_array_index (array, index) = data;
 *
 *     g_static_rw_lock_writer_unlock (&amp;rwlock);
 *   }
 *  </programlisting>
 * </example>
 *
 * This example shows an array which can be accessed by many readers
 * (the <function>my_array_get()</function> function) simultaneously,
 * whereas the writers (the <function>my_array_set()</function>
 * function) will only be allowed once at a time and only if no readers
 * currently access the array. This is because of the potentially
 * dangerous resizing of the array. Using these functions is fully
 * multi-thread safe now.
 *
 * Most of the time, writers should have precedence over readers. That
 * means, for this implementation, that as soon as a writer wants to
 * lock the data, no other reader is allowed to lock the data, whereas,
 * of course, the readers that already have locked the data are allowed
 * to finish their operation. As soon as the last reader unlocks the
 * data, the writer will lock it.
 *
 * Even though #GStaticRWLock is not opaque, it should only be used
 * with the following functions.
 *
 * All of the <function>g_static_rw_lock_*</function> functions can be
 * used even if g_thread_init() has not been called. Then they do
 * nothing, apart from <function>g_static_rw_lock_*_trylock</function>,
 * which does nothing but returning %TRUE.
 *
 * <note><para>A read-write lock has a higher overhead than a mutex. For
 * example, both g_static_rw_lock_reader_lock() and
 * g_static_rw_lock_reader_unlock() have to lock and unlock a
 * #GStaticMutex, so it takes at least twice the time to lock and unlock
 * a #GStaticRWLock that it does to lock and unlock a #GStaticMutex. So
 * only data structures that are accessed by multiple readers, and which
 * keep the lock for a considerable time justify a #GStaticRWLock. The
 * above example most probably would fare better with a
 * #GStaticMutex.</para></note>
 *
 * Deprecated: 2.32: Use a #GRWLock instead
 **/

/**
 * G_STATIC_RW_LOCK_INIT:
 *
 * A #GStaticRWLock must be initialized with this macro before it can
 * be used. This macro can used be to initialize a variable, but it
 * cannot be assigned to a variable. In that case you have to use
 * g_static_rw_lock_init().
 *
 * |[
 *   GStaticRWLock my_lock = G_STATIC_RW_LOCK_INIT;
 * ]|
 */

/**
 * g_static_rw_lock_init:
 * @lock: a #GStaticRWLock to be initialized.
 *
 * A #GStaticRWLock must be initialized with this function before it
 * can be used. Alternatively you can initialize it with
 * #G_STATIC_RW_LOCK_INIT.
 *
 * Deprecated: 2.32: Use g_rw_lock_init() instead
 */
void
g_static_rw_lock_init (GStaticRWLock* lock)
{
  static const GStaticRWLock init_lock = G_STATIC_RW_LOCK_INIT;

  g_return_if_fail (lock);

  *lock = init_lock;
}

inline static void
g_static_rw_lock_wait (GCond** cond, GStaticMutex* mutex)
{
  if (!*cond)
      *cond = g_cond_new ();
  g_cond_wait (*cond, g_static_mutex_get_mutex (mutex));
}

inline static void
g_static_rw_lock_signal (GStaticRWLock* lock)
{
  if (lock->want_to_write && lock->write_cond)
    g_cond_signal (lock->write_cond);
  else if (lock->want_to_read && lock->read_cond)
    g_cond_broadcast (lock->read_cond);
}

/**
 * g_static_rw_lock_reader_lock:
 * @lock: a #GStaticRWLock to lock for reading.
 *
 * Locks @lock for reading. There may be unlimited concurrent locks for
 * reading of a #GStaticRWLock at the same time.  If @lock is already
 * locked for writing by another thread or if another thread is already
 * waiting to lock @lock for writing, this function will block until
 * @lock is unlocked by the other writing thread and no other writing
 * threads want to lock @lock. This lock has to be unlocked by
 * g_static_rw_lock_reader_unlock().
 *
 * #GStaticRWLock is not recursive. It might seem to be possible to
 * recursively lock for reading, but that can result in a deadlock, due
 * to writer preference.
 *
 * Deprecated: 2.32: Use g_rw_lock_reader_lock() instead
 */
void
g_static_rw_lock_reader_lock (GStaticRWLock* lock)
{
  g_return_if_fail (lock);

  if (!g_threads_got_initialized)
    return;

  g_static_mutex_lock (&lock->mutex);
  lock->want_to_read++;
  while (lock->have_writer || lock->want_to_write)
    g_static_rw_lock_wait (&lock->read_cond, &lock->mutex);
  lock->want_to_read--;
  lock->read_counter++;
  g_static_mutex_unlock (&lock->mutex);
}

/**
 * g_static_rw_lock_reader_trylock:
 * @lock: a #GStaticRWLock to lock for reading.
 * @Returns: %TRUE, if @lock could be locked for reading.
 *
 * Tries to lock @lock for reading. If @lock is already locked for
 * writing by another thread or if another thread is already waiting to
 * lock @lock for writing, immediately returns %FALSE. Otherwise locks
 * @lock for reading and returns %TRUE. This lock has to be unlocked by
 * g_static_rw_lock_reader_unlock().
 *
 * Deprectated: 2.32: Use g_rw_lock_reader_trylock() instead
 */
gboolean
g_static_rw_lock_reader_trylock (GStaticRWLock* lock)
{
  gboolean ret_val = FALSE;

  g_return_val_if_fail (lock, FALSE);

  if (!g_threads_got_initialized)
    return TRUE;

  g_static_mutex_lock (&lock->mutex);
  if (!lock->have_writer && !lock->want_to_write)
    {
      lock->read_counter++;
      ret_val = TRUE;
    }
  g_static_mutex_unlock (&lock->mutex);
  return ret_val;
}

/**
 * g_static_rw_lock_reader_unlock:
 * @lock: a #GStaticRWLock to unlock after reading.
 *
 * Unlocks @lock. If a thread waits to lock @lock for writing and all
 * locks for reading have been unlocked, the waiting thread is woken up
 * and can lock @lock for writing.
 *
 * Deprectated: 2.32: Use g_rw_lock_reader_unlock() instead
 */
void
g_static_rw_lock_reader_unlock  (GStaticRWLock* lock)
{
  g_return_if_fail (lock);

  if (!g_threads_got_initialized)
    return;

  g_static_mutex_lock (&lock->mutex);
  lock->read_counter--;
  if (lock->read_counter == 0)
    g_static_rw_lock_signal (lock);
  g_static_mutex_unlock (&lock->mutex);
}

/**
 * g_static_rw_lock_writer_lock:
 * @lock: a #GStaticRWLock to lock for writing.
 *
 * Locks @lock for writing. If @lock is already locked for writing or
 * reading by other threads, this function will block until @lock is
 * completely unlocked and then lock @lock for writing. While this
 * functions waits to lock @lock, no other thread can lock @lock for
 * reading. When @lock is locked for writing, no other thread can lock
 * @lock (neither for reading nor writing). This lock has to be
 * unlocked by g_static_rw_lock_writer_unlock().
 *
 * Deprectated: 2.32: Use g_rw_lock_writer_lock() instead
 */
void
g_static_rw_lock_writer_lock (GStaticRWLock* lock)
{
  g_return_if_fail (lock);

  if (!g_threads_got_initialized)
    return;

  g_static_mutex_lock (&lock->mutex);
  lock->want_to_write++;
  while (lock->have_writer || lock->read_counter)
    g_static_rw_lock_wait (&lock->write_cond, &lock->mutex);
  lock->want_to_write--;
  lock->have_writer = TRUE;
  g_static_mutex_unlock (&lock->mutex);
}

/**
 * g_static_rw_lock_writer_trylock:
 * @lock: a #GStaticRWLock to lock for writing.
 * @Returns: %TRUE, if @lock could be locked for writing.
 *
 * Tries to lock @lock for writing. If @lock is already locked (for
 * either reading or writing) by another thread, it immediately returns
 * %FALSE. Otherwise it locks @lock for writing and returns %TRUE. This
 * lock has to be unlocked by g_static_rw_lock_writer_unlock().
 *
 * Deprectated: 2.32: Use g_rw_lock_writer_trylock() instead
 */
gboolean
g_static_rw_lock_writer_trylock (GStaticRWLock* lock)
{
  gboolean ret_val = FALSE;

  g_return_val_if_fail (lock, FALSE);

  if (!g_threads_got_initialized)
    return TRUE;

  g_static_mutex_lock (&lock->mutex);
  if (!lock->have_writer && !lock->read_counter)
    {
      lock->have_writer = TRUE;
      ret_val = TRUE;
    }
  g_static_mutex_unlock (&lock->mutex);
  return ret_val;
}

/**
 * g_static_rw_lock_writer_unlock:
 * @lock: a #GStaticRWLock to unlock after writing.
 *
 * Unlocks @lock. If a thread is waiting to lock @lock for writing and
 * all locks for reading have been unlocked, the waiting thread is
 * woken up and can lock @lock for writing. If no thread is waiting to
 * lock @lock for writing, and some thread or threads are waiting to
 * lock @lock for reading, the waiting threads are woken up and can
 * lock @lock for reading.
 *
 * Deprectated: 2.32: Use g_rw_lock_writer_unlock() instead
 */
void
g_static_rw_lock_writer_unlock (GStaticRWLock* lock)
{
  g_return_if_fail (lock);

  if (!g_threads_got_initialized)
    return;

  g_static_mutex_lock (&lock->mutex);
  lock->have_writer = FALSE;
  g_static_rw_lock_signal (lock);
  g_static_mutex_unlock (&lock->mutex);
}

/**
 * g_static_rw_lock_free:
 * @lock: a #GStaticRWLock to be freed.
 *
 * Releases all resources allocated to @lock.
 *
 * You don't have to call this functions for a #GStaticRWLock with an
 * unbounded lifetime, i.e. objects declared 'static', but if you have
 * a #GStaticRWLock as a member of a structure, and the structure is
 * freed, you should also free the #GStaticRWLock.
 *
 * Deprecated: 2.32: Use a #GRWLock instead
 */
void
g_static_rw_lock_free (GStaticRWLock* lock)
{
  g_return_if_fail (lock);

  if (lock->read_cond)
    {
      g_cond_free (lock->read_cond);
      lock->read_cond = NULL;
    }
  if (lock->write_cond)
    {
      g_cond_free (lock->write_cond);
      lock->write_cond = NULL;
    }
  g_static_mutex_free (&lock->mutex);
}

/* vim: set foldmethod=marker: */

