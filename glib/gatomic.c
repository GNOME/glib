/*
 * Copyright © 2011 Ryan Lortie
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * licence, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
 * USA.
 *
 * Author: Ryan Lortie <desrt@desrt.ca>
 */

#include "config.h"

#include "gatomic.h"

/**
 * SECTION:atomic_operations
 * @title: Atomic Operations
 * @short_description: basic atomic integer and pointer operations
 * @see_also: #GMutex
 *
 * The following is a collection of compiler macros to provide atomic
 * access to integer and pointer-sized values.
 *
 * The macros that have 'int' in the name will operate on pointers to
 * #gint and #guint.  The macros with 'pointer' in the name will operate
 * on pointers to any pointer-sized value, including #gsize.  There is
 * no support for 64bit operations on platforms with 32bit pointers
 * because it is not generally possible to perform these operations
 * atomically.
 *
 * The get, set and exchange operations for integers and pointers
 * nominally operate on #gint and #gpointer, respectively.  Of the
 * arithmetic operations, the 'add' operation operates on (and returns)
 * signed integer values (#gint and #gssize) and the 'and', 'or', and
 * 'xor' operations operate on (and return) unsigned integer values
 * (#guint and #gsize).
 *
 * All of the operations act as a full compiler and (where appropriate)
 * hardware memory barrier.  Acquire and release or producer and
 * consumer barrier semantics are not available through this API.
 *
 * On GCC, these macros are implemented using GCC intrinsic operations.
 * On non-GCC compilers they will evaluate to function calls to
 * functions implemented by GLib.
 *
 * If GLib itself was compiled with GCC then these functions will again
 * be implemented by the GCC intrinsics.  On Windows without GCC, the
 * interlocked API is used to implement the functions.
 *
 * With non-GCC compilers on non-Windows systems, the functions are
 * currently incapable of implementing true atomic operations --
 * instead, they fallback to holding a global lock while performing the
 * operation.  This provides atomicity between the threads of one
 * process, but not between separate processes.  For this reason, one
 * should exercise caution when attempting to use these options on
 * shared memory regions.
 *
 * It is very important that all accesses to a particular integer or
 * pointer be performed using only this API and that different sizes of
 * operation are not mixed or used on overlapping memory regions.  Never
 * read or assign directly from or to a value -- always use this API.
 *
 * For simple reference counting purposes you should use
 * g_atomic_int_inc() and g_atomic_int_dec_and_test().  Other uses that
 * fall outside of simple reference counting patterns are prone to
 * subtle bugs and occasionally undefined behaviour.  It is also worth
 * noting that since all of these operations require global
 * synchronisation of the entire machine, they can be quite slow.  In
 * the case of performing multiple atomic operations it can often be
 * faster to simply acquire a mutex lock around the critical area,
 * perform the operations normally and then release the lock.
 **/

#ifdef G_ATOMIC_OP_USE_GCC_BUILTINS

#ifndef __GNUC__
#error Using GCC builtin atomic ops, but not compiling with GCC?
#endif

/**
 * g_atomic_int_get:
 * @atomic: a pointer to a #gint or #guint
 *
 * Gets the current value of @atomic.
 *
 * This call acts as a full compiler and hardware
 * memory barrier (before the get).
 *
 * Returns: the value of the integer
 *
 * Since: 2.4
 **/
gint
(g_atomic_int_get) (volatile gint *atomic)
{
  return g_atomic_int_get (atomic);
}

/**
 * g_atomic_int_set:
 * @atomic: a pointer to a #gint or #guint
 * @newval: a new value to store
 *
 * Sets the value of @atomic to @newval.
 *
 * This call acts as a full compiler and hardware
 * memory barrier (after the set).
 *
 * Since: 2.4
 */
void
(g_atomic_int_set) (volatile gint *atomic,
                    gint           newval)
{
  g_atomic_int_set (atomic, newval);
}

/**
 * g_atomic_int_inc:
 * @atomic: a pointer to a #gint or #guint
 *
 * Increments the value of @atomic by 1.
 *
 * Think of this operation as an atomic version of
 * <literal>{ *@atomic += 1; }</literal>
 *
 * This call acts as a full compiler and hardware memory barrier.
 *
 * Since: 2.4
 **/
void
(g_atomic_int_inc) (volatile gint *atomic)
{
  g_atomic_int_inc (atomic);
}

/**
 * g_atomic_int_dec_and_test:
 * @atomic: a pointer to a #gint or #guint
 *
 * Decrements the value of @atomic by 1.
 *
 * Think of this operation as an atomic version of
 * <literal>{ *@atomic -= 1; return (*@atomic == 0); }</literal>
 *
 * This call acts as a full compiler and hardware memory barrier.
 *
 * Returns: %TRUE if the resultant value is zero
 *
 * Since: 2.4
 **/
gboolean
(g_atomic_int_dec_and_test) (volatile gint *atomic)
{
  return g_atomic_int_dec_and_test (atomic);
}

/**
 * g_atomic_int_compare_and_exchange:
 * @atomic: a pointer to a #gint or #guint
 * @oldval: the value to compare with
 * @newval: the value to conditionally replace with
 *
 * Compares @atomic to @oldval and, if equal, sets it to @newval.
 * If @atomic was not equal to @oldval then no change occurs.
 *
 * This compare and exchange is done atomically.
 *
 * Think of this operation as an atomic version of
 * <literal>{ if (*@atomic == @oldval) { *@atomic = @newval; return TRUE; } else return FALSE; }</literal>
 *
 * This call acts as a full compiler and hardware memory barrier.
 *
 * Returns: %TRUE if the exchange took place
 *
 * Since: 2.4
 **/
gboolean
(g_atomic_int_compare_and_exchange) (volatile gint *atomic,
                                     gint           oldval,
                                     gint           newval)
{
  return g_atomic_int_compare_and_exchange (atomic, oldval, newval);
}

/**
 * g_atomic_int_add:
 * @atomic: a pointer to a #gint or #guint
 * @val: the value to add
 *
 * Atomically adds @val to the value of @atomic.
 *
 * Think of this operation as an atomic version of
 * <literal>{ tmp = *atomic; *@atomic += @val; return tmp; }</literal>
 *
 * This call acts as a full compiler and hardware memory barrier.
 *
 * Before version 2.30, this function did not return a value
 * (but g_atomic_int_exchange_and_add() did, and had the same meaning).
 *
 * Returns: the value of @atomic before the add, signed
 *
 * Since: 2.4
 **/
gint
(g_atomic_int_add) (volatile gint *atomic,
                    gint           val)
{
  return g_atomic_int_add (atomic, val);
}

/**
 * g_atomic_int_and:
 * @atomic: a pointer to a #gint or #guint
 * @val: the value to 'and'
 *
 * Performs an atomic bitwise 'and' of the value of @atomic and @val,
 * storing the result back in @atomic.
 *
 * This call acts as a full compiler and hardware memory barrier.
 *
 * Think of this operation as an atomic version of
 * <literal>{ tmp = *atomic; *@atomic &= @val; return tmp; }</literal>
 *
 * Returns: the value of @atomic before the operation, unsigned
 *
 * Since: 2.30
 **/
guint
(g_atomic_int_and) (volatile guint *atomic,
                    guint           val)
{
  return g_atomic_int_and (atomic, val);
}

/**
 * g_atomic_int_or:
 * @atomic: a pointer to a #gint or #guint
 * @val: the value to 'or'
 *
 * Performs an atomic bitwise 'or' of the value of @atomic and @val,
 * storing the result back in @atomic.
 *
 * Think of this operation as an atomic version of
 * <literal>{ tmp = *atomic; *@atomic |= @val; return tmp; }</literal>
 *
 * This call acts as a full compiler and hardware memory barrier.
 *
 * Returns: the value of @atomic before the operation, unsigned
 *
 * Since: 2.30
 **/
guint
(g_atomic_int_or) (volatile guint *atomic,
                   guint           val)
{
  return g_atomic_int_or (atomic, val);
}

/**
 * g_atomic_int_xor:
 * @atomic: a pointer to a #gint or #guint
 * @val: the value to 'xor'
 *
 * Performs an atomic bitwise 'xor' of the value of @atomic and @val,
 * storing the result back in @atomic.
 *
 * Think of this operation as an atomic version of
 * <literal>{ tmp = *atomic; *@atomic ^= @val; return tmp; }</literal>
 *
 * This call acts as a full compiler and hardware memory barrier.
 *
 * Returns: the value of @atomic before the operation, unsigned
 *
 * Since: 2.30
 **/
guint
(g_atomic_int_xor) (volatile guint *atomic,
                    guint           val)
{
  return g_atomic_int_xor (atomic, val);
}


/**
 * g_atomic_pointer_get:
 * @atomic: a pointer to a #gpointer-sized value
 *
 * Gets the current value of @atomic.
 *
 * This call acts as a full compiler and hardware
 * memory barrier (before the get).
 *
 * Returns: the value of the pointer
 *
 * Since: 2.4
 **/
gpointer
(g_atomic_pointer_get) (volatile void *atomic)
{
  return g_atomic_pointer_get ((volatile gpointer *) atomic);
}

/**
 * g_atomic_pointer_set:
 * @atomic: a pointer to a #gpointer-sized value
 * @newval: a new value to store
 *
 * Sets the value of @atomic to @newval.
 *
 * This call acts as a full compiler and hardware
 * memory barrier (after the set).
 *
 * Since: 2.4
 **/
void
(g_atomic_pointer_set) (volatile void *atomic,
                        gpointer       newval)
{
  g_atomic_pointer_set ((volatile gpointer *) atomic, newval);
}

/**
 * g_atomic_pointer_compare_and_exchange:
 * @atomic: a pointer to a #gpointer-sized value
 * @oldval: the value to compare with
 * @newval: the value to conditionally replace with
 *
 * Compares @atomic to @oldval and, if equal, sets it to @newval.
 * If @atomic was not equal to @oldval then no change occurs.
 *
 * This compare and exchange is done atomically.
 *
 * Think of this operation as an atomic version of
 * <literal>{ if (*@atomic == @oldval) { *@atomic = @newval; return TRUE; } else return FALSE; }</literal>
 *
 * This call acts as a full compiler and hardware memory barrier.
 *
 * Returns: %TRUE if the exchange took place
 *
 * Since: 2.4
 **/
gboolean
(g_atomic_pointer_compare_and_exchange) (volatile void *atomic,
                                         gpointer       oldval,
                                         gpointer       newval)
{
  return g_atomic_pointer_compare_and_exchange ((volatile gpointer *) atomic,
                                                oldval, newval);
}

/**
 * g_atomic_pointer_add:
 * @atomic: a pointer to a #gpointer-sized value
 * @val: the value to add
 *
 * Atomically adds @val to the value of @atomic.
 *
 * Think of this operation as an atomic version of
 * <literal>{ tmp = *atomic; *@atomic += @val; return tmp; }</literal>
 *
 * This call acts as a full compiler and hardware memory barrier.
 *
 * Returns: the value of @atomic before the add, signed
 *
 * Since: 2.30
 **/
gssize
(g_atomic_pointer_add) (volatile void *atomic,
                        gssize         val)
{
  return g_atomic_pointer_add ((volatile gpointer *) atomic, val);
}

/**
 * g_atomic_pointer_and:
 * @atomic: a pointer to a #gpointer-sized value
 * @val: the value to 'and'
 *
 * Performs an atomic bitwise 'and' of the value of @atomic and @val,
 * storing the result back in @atomic.
 *
 * Think of this operation as an atomic version of
 * <literal>{ tmp = *atomic; *@atomic &= @val; return tmp; }</literal>
 *
 * This call acts as a full compiler and hardware memory barrier.
 *
 * Returns: the value of @atomic before the operation, unsigned
 *
 * Since: 2.30
 **/
gsize
(g_atomic_pointer_and) (volatile void *atomic,
                        gsize          val)
{
  return g_atomic_pointer_and ((volatile gpointer *) atomic, val);
}

/**
 * g_atomic_pointer_or:
 * @atomic: a pointer to a #gpointer-sized value
 * @val: the value to 'or'
 *
 * Performs an atomic bitwise 'or' of the value of @atomic and @val,
 * storing the result back in @atomic.
 *
 * Think of this operation as an atomic version of
 * <literal>{ tmp = *atomic; *@atomic |= @val; return tmp; }</literal>
 *
 * This call acts as a full compiler and hardware memory barrier.
 *
 * Returns: the value of @atomic before the operation, unsigned
 *
 * Since: 2.30
 **/
gsize
(g_atomic_pointer_or) (volatile void *atomic,
                       gsize          val)
{
  return g_atomic_pointer_or ((volatile gpointer *) atomic, val);
}

/**
 * g_atomic_pointer_xor:
 * @atomic: a pointer to a #gpointer-sized value
 * @val: the value to 'xor'
 *
 * Performs an atomic bitwise 'xor' of the value of @atomic and @val,
 * storing the result back in @atomic.
 *
 * Think of this operation as an atomic version of
 * <literal>{ tmp = *atomic; *@atomic ^= @val; return tmp; }</literal>
 *
 * This call acts as a full compiler and hardware memory barrier.
 *
 * Returns: the value of @atomic before the operation, unsigned
 *
 * Since: 2.30
 **/
gsize
(g_atomic_pointer_xor) (volatile void *atomic,
                        gsize          val)
{
  return g_atomic_pointer_xor ((volatile gpointer *) atomic, val);
}

#elif defined (G_PLATFORM_WIN32) && defined(HAVE_WIN32_BUILTINS_FOR_ATOMIC_OPERATIONS)

#include <windows.h>
#if !defined(_M_AMD64) && !defined (_M_IA64) && !defined(_M_X64)
#define InterlockedAnd _InterlockedAnd
#define InterlockedOr _InterlockedOr
#define InterlockedXor _InterlockedXor
#endif

/*
 * http://msdn.microsoft.com/en-us/library/ms684122(v=vs.85).aspx
 */
gint
(g_atomic_int_get) (volatile gint *atomic)
{
  MemoryBarrier ();
  return *atomic;
}

void
(g_atomic_int_set) (volatile gint *atomic,
                    gint           newval)
{
  *atomic = newval;
  MemoryBarrier ();
}

void
(g_atomic_int_inc) (volatile gint *atomic)
{
  InterlockedIncrement (atomic);
}

gboolean
(g_atomic_int_dec_and_test) (volatile gint *atomic)
{
  return InterlockedDecrement (atomic) == 0;
}

gboolean
(g_atomic_int_compare_and_exchange) (volatile gint *atomic,
                                     gint           oldval,
                                     gint           newval)
{
  return InterlockedCompareExchange (atomic, newval, oldval) == oldval;
}

gint
(g_atomic_int_add) (volatile gint *atomic,
                    gint           val)
{
  return InterlockedExchangeAdd (atomic, val);
}

guint
(g_atomic_int_and) (volatile guint *atomic,
                    guint           val)
{
  return InterlockedAnd (atomic, val);
}

guint
(g_atomic_int_or) (volatile guint *atomic,
                   guint           val)
{
  return InterlockedOr (atomic, val);
}

guint
(g_atomic_int_xor) (volatile guint *atomic,
                    guint           val)
{
  return InterlockedXor (atomic, val);
}


gpointer
(g_atomic_pointer_get) (volatile void *atomic)
{
  volatile gpointer *ptr = atomic;

  MemoryBarrier ();
  return *ptr;
}

void
(g_atomic_pointer_set) (volatile void *atomic,
                        gpointer       newval)
{
  volatile gpointer *ptr = atomic;

  *ptr = newval;
  MemoryBarrier ();
}

gboolean
(g_atomic_pointer_compare_and_exchange) (volatile void *atomic,
                                         gpointer       oldval,
                                         gpointer       newval)
{
  return InterlockedCompareExchangePointer (atomic, newval, oldval) == oldval;
}

gssize
(g_atomic_pointer_add) (volatile void *atomic,
                        gssize         val)
{
#if GLIB_SIZEOF_VOID_P == 8
  return InterlockedExchangeAdd64 (atomic, val);
#else
  return InterlockedExchangeAdd (atomic, val);
#endif
}

gsize
(g_atomic_pointer_and) (volatile void *atomic,
                        gsize          val)
{
#if GLIB_SIZEOF_VOID_P == 8
  return InterlockedAnd64 (atomic, val);
#else
  return InterlockedAnd (atomic, val);
#endif
}

gsize
(g_atomic_pointer_or) (volatile void *atomic,
                       gsize          val)
{
#if GLIB_SIZEOF_VOID_P == 8
  return InterlockedOr64 (atomic, val);
#else
  return InterlockedOr (atomic, val);
#endif
}

gsize
(g_atomic_pointer_xor) (volatile void *atomic,
                        gsize          val)
{
#if GLIB_SIZEOF_VOID_P == 8
  return InterlockedXor64 (atomic, val);
#else
  return InterlockedXor (atomic, val);
#endif
}

#else

#include "gthread.h"

static GStaticMutex g_atomic_lock;

gint
(g_atomic_int_get) (volatile gint *atomic)
{
  gint value;

  g_static_mutex_lock (&g_atomic_lock);
  value = *atomic;
  g_static_mutex_unlock (&g_atomic_lock);

  return value;
}

void
(g_atomic_int_set) (volatile gint *atomic,
                    gint           value)
{
  g_static_mutex_lock (&g_atomic_lock);
  *atomic = value;
  g_static_mutex_unlock (&g_atomic_lock);
}

void
(g_atomic_int_inc) (volatile gint *atomic)
{
  g_static_mutex_lock (&g_atomic_lock);
  (*atomic)++;
  g_static_mutex_unlock (&g_atomic_lock);
}

gboolean
(g_atomic_int_dec_and_test) (volatile gint *atomic)
{
  gboolean is_zero;

  g_static_mutex_lock (&g_atomic_lock);
  is_zero = --(*atomic) == 0;
  g_static_mutex_unlock (&g_atomic_lock);

  return is_zero;
}

gboolean
(g_atomic_int_compare_and_exchange) (volatile gint *atomic,
                                     gint           oldval,
                                     gint           newval)
{
  gboolean success;

  g_static_mutex_lock (&g_atomic_lock);

  if ((success = (*atomic == oldval)))
    *atomic = newval;

  g_static_mutex_unlock (&g_atomic_lock);

  return success;
}

gint
(g_atomic_int_add) (volatile gint *atomic,
                    gint           val)
{
  gint oldval;

  g_static_mutex_lock (&g_atomic_lock);
  oldval = *atomic;
  *atomic = oldval + val;
  g_static_mutex_unlock (&g_atomic_lock);

  return oldval;
}

guint
(g_atomic_int_and) (volatile guint *atomic,
                    guint           val)
{
  guint oldval;

  g_static_mutex_lock (&g_atomic_lock);
  oldval = *atomic;
  *atomic = oldval & val;
  g_static_mutex_unlock (&g_atomic_lock);

  return oldval;
}

guint
(g_atomic_int_or) (volatile guint *atomic,
                   guint           val)
{
  guint oldval;

  g_static_mutex_lock (&g_atomic_lock);
  oldval = *atomic;
  *atomic = oldval | val;
  g_static_mutex_unlock (&g_atomic_lock);

  return oldval;
}

guint
(g_atomic_int_xor) (volatile guint *atomic,
                    guint           val)
{
  guint oldval;

  g_static_mutex_lock (&g_atomic_lock);
  oldval = *atomic;
  *atomic = oldval ^ val;
  g_static_mutex_unlock (&g_atomic_lock);

  return oldval;
}


gpointer
(g_atomic_pointer_get) (volatile void *atomic)
{
  volatile gpointer *ptr = atomic;
  gpointer value;

  g_static_mutex_lock (&g_atomic_lock);
  value = *ptr;
  g_static_mutex_unlock (&g_atomic_lock);

  return value;
}

void
(g_atomic_pointer_set) (volatile void *atomic,
                        gpointer       newval)
{
  volatile gpointer *ptr = atomic;

  g_static_mutex_lock (&g_atomic_lock);
  *ptr = newval;
  g_static_mutex_unlock (&g_atomic_lock);
}

gboolean
(g_atomic_pointer_compare_and_exchange) (volatile void *atomic,
                                         gpointer       oldval,
                                         gpointer       newval)
{
  volatile gpointer *ptr = atomic;
  gboolean success;

  g_static_mutex_lock (&g_atomic_lock);

  if ((success = (*ptr == oldval)))
    *ptr = newval;

  g_static_mutex_unlock (&g_atomic_lock);

  return success;
}

gssize
(g_atomic_pointer_add) (volatile void *atomic,
                        gssize         val)
{
  volatile gssize *ptr = atomic;
  gssize oldval;

  g_static_mutex_lock (&g_atomic_lock);
  oldval = *ptr;
  *ptr = oldval + val;
  g_static_mutex_unlock (&g_atomic_lock);

  return oldval;
}

gsize
(g_atomic_pointer_and) (volatile void *atomic,
                        gsize          val)
{
  volatile gsize *ptr = atomic;
  gsize oldval;

  g_static_mutex_lock (&g_atomic_lock);
  oldval = *ptr;
  *ptr = oldval & val;
  g_static_mutex_unlock (&g_atomic_lock);

  return oldval;
}

gsize
(g_atomic_pointer_or) (volatile void *atomic,
                       gsize          val)
{
  volatile gsize *ptr = atomic;
  gsize oldval;

  g_static_mutex_lock (&g_atomic_lock);
  oldval = *ptr;
  *ptr = oldval | val;
  g_static_mutex_unlock (&g_atomic_lock);

  return oldval;
}

gsize
(g_atomic_pointer_xor) (volatile void *atomic,
                        gsize          val)
{
  volatile gsize *ptr = atomic;
  gsize oldval;

  g_static_mutex_lock (&g_atomic_lock);
  oldval = *ptr;
  *ptr = oldval ^ val;
  g_static_mutex_unlock (&g_atomic_lock);

  return oldval;
}

#endif

/**
 * g_atomic_int_exchange_and_add:
 * @atomic: a pointer to a #gint
 * @val: the value to add
 *
 * This function existed before g_atomic_int_add() returned the prior
 * value of the integer (which it now does).  It is retained only for
 * compatibility reasons.  Don't use this function in new code.
 *
 * Returns: the value of @atomic before the add, signed
 * Since: 2.4
 * Deprecated: 2.30: Use g_atomic_int_add() instead.
 **/
gint
g_atomic_int_exchange_and_add (volatile gint *atomic,
                               gint           val)
{
  return (g_atomic_int_add) (atomic, val);
}
