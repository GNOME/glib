/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * g_atomic_*: atomic operations.
 * Copyright (C) 2003 Sebastian Wilhelmi
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
 
#include "config.h"

#include "glib.h"
#include "gthreadinit.h"

#if defined (__GNUC__)
# if defined (G_ATOMIC_I486)
/* Adapted from CVS version 1.10 of glibc's sysdeps/i386/i486/bits/atomic.h 
 */
gint
g_atomic_int_exchange_and_add (gint *atomic, 
			       gint val)
{
  gint result;

  __asm__ __volatile__ ("lock; xaddl %0,%1"
                        : "=r" (result), "=m" (*atomic) 
			: "0" (val), "m" (*atomic));
  return result;
}
 
void
g_atomic_int_add (gint *atomic, 
		  gint val)
{
  __asm__ __volatile__ ("lock; addl %1,%0"
			: "=m" (*atomic) 
			: "ir" (val), "m" (*atomic));
}

gboolean
g_atomic_int_compare_and_exchange (gint *atomic, 
				   gint oldval, 
				   gint newval)
{
  gint result;
 
  __asm__ __volatile__ ("lock; cmpxchgl %2, %1"
			: "=a" (result), "=m" (*atomic)
			: "r" (newval), "m" (*atomic), "0" (oldval)); 

  return result == oldval;
}

/* The same code as above, as on i386 gpointer is 32 bit as well.
 * Duplicating the code here seems more natural than casting the
 * arguments and calling the former function */

gboolean
g_atomic_pointer_compare_and_exchange (gpointer *atomic, 
				       gpointer  oldval, 
				       gpointer  newval)
{
  gpointer result;
 
  __asm__ __volatile__ ("lock; cmpxchgl %2, %1"
			: "=a" (result), "=m" (*atomic)
			: "r" (newval), "m" (*atomic), "0" (oldval)); 

  return result == oldval;
}

# elif defined (G_ATOMIC_SPARCV9)
/* Adapted from CVS version 1.3 of glibc's sysdeps/sparc/sparc64/bits/atomic.h
 */
#  define ATOMIC_INT_CMP_XCHG(atomic, oldval, newval)			\
  ({ 									\
     gint __result;							\
     __asm__ __volatile__ ("cas [%4], %2, %0"				\
                           : "=r" (__result), "=m" (*(atomic))		\
                           : "r" (oldval), "m" (*(atomic)), "r" (atomic),\
                           "0" (newval));				\
     __result == oldval;						\
  })

#  if GLIB_SIZEOF_VOID_P == 4 /* 32-bit system */
gboolean
g_atomic_pointer_compare_and_exchange (gpointer *atomic, 
				       gpointer  oldval, 
				       gpointer  newval)
{
  gpointer result;
  __asm__ __volatile__ ("cas [%4], %2, %0"
			: "=r" (result), "=m" (*atomic)
			: "r" (oldval), "m" (*atomic), "r" (atomic),
			"0" (newval));
  return result == oldval;
}
#  elif GLIB_SIZEOF_VOID_P == 8 /* 64-bit system */
gboolean
g_atomic_pointer_compare_and_exchange (gpointer *atomic, 
				       gpointer  oldval, 
				       gpointer  newval)
{
  gpointer result;
  gpointer *a = atomic;
  __asm__ __volatile__ ("casx [%4], %2, %0"
			: "=r" (result), "=m" (*a)
			: "r" (oldval), "m" (*a), "r" (a),
			"0" (newval));
  return result != 0;
}
#  else /* What's that */
#    error "Your system has an unsupported pointer size"
#  endif /* GLIB_SIZEOF_VOID_P */
#  define G_ATOMIC_MEMORY_BARRIER					\
  __asm__ __volatile__ ("membar #LoadLoad | #LoadStore"			\
                        " | #StoreLoad | #StoreStore" : : : "memory")

# elif defined (G_ATOMIC_ALPHA)
/* Adapted from CVS version 1.3 of glibc's sysdeps/alpha/bits/atomic.h
 */
#  define ATOMIC_INT_CMP_XCHG(atomic, oldval, newval)			\
  ({ 									\
     gint __result;							\
     gint __prev;							\
     __asm__ __volatile__ (						\
        "       mb\n"							\
        "1:     ldl_l   %0,%2\n"					\
        "       cmpeq   %0,%3,%1\n"					\
        "       beq     %1,2f\n"					\
        "       mov     %4,%1\n"					\
        "       stl_c   %1,%2\n"					\
        "       beq     %1,1b\n"					\
        "       mb\n"							\
        "2:"								\
        : "=&r" (__prev), 						\
          "=&r" (__result)						\
        : "m" (*(atomic)),						\
          "Ir" (oldval),						\
          "Ir" (newval)							\
        : "memory");							\
     __result != 0;							\
  })
#  if GLIB_SIZEOF_VOID_P == 4 /* 32-bit system */
gboolean
g_atomic_pointer_compare_and_exchange (gpointer *atomic, 
				       gpointer  oldval, 
				       gpointer  newval)
{
  gint result;
  gpointer prev;
  __asm__ __volatile__ (
        "       mb\n"
        "1:     ldl_l   %0,%2\n"
        "       cmpeq   %0,%3,%1\n"
        "       beq     %1,2f\n"
        "       mov     %4,%1\n"
        "       stl_c   %1,%2\n"
        "       beq     %1,1b\n"
        "       mb\n"
        "2:"
        : "=&r" (prev), 
          "=&r" (result)
        : "m" (*atomic),
          "Ir" (oldval),
          "Ir" (newval)
        : "memory");
  return result != 0;
}
#  elif GLIB_SIZEOF_VOID_P == 8 /* 64-bit system */
gboolean
g_atomic_pointer_compare_and_exchange (gpointer *atomic, 
				       gpointer  oldval, 
				       gpointer  newval)
{
  gint result;
  gpointer prev;
  __asm__ __volatile__ (
        "       mb\n"
        "1:     ldq_l   %0,%2\n"
        "       cmpeq   %0,%3,%1\n"
        "       beq     %1,2f\n"
        "       mov     %4,%1\n"
        "       stq_c   %1,%2\n"
        "       beq     %1,1b\n"
        "       mb\n"
        "2:"
        : "=&r" (prev), 
          "=&r" (result)
        : "m" (*atomic),
          "Ir" (oldval),
          "Ir" (newval)
        : "memory");
  return result != 0;
}
#  else /* What's that */
#   error "Your system has an unsupported pointer size"
#  endif /* GLIB_SIZEOF_VOID_P */
#  define G_ATOMIC_MEMORY_BARRIER  __asm__ ("mb" : : : "memory")
# elif defined (G_ATOMIC_X86_64)
/* Adapted from CVS version 1.9 of glibc's sysdeps/x86_64/bits/atomic.h 
 */
gint
g_atomic_int_exchange_and_add (gint *atomic, 
			       gint val)
{
  gint result;

  __asm__ __volatile__ ("lock; xaddl %0,%1"
                        : "=r" (result), "=m" (*atomic) 
			: "0" (val), "m" (*atomic));
  return result;
}
 
void
g_atomic_int_add (gint *atomic, 
		  gint val)
{
  __asm__ __volatile__ ("lock; addl %1,%0"
			: "=m" (*atomic) 
			: "ir" (val), "m" (*atomic));
}

gboolean
g_atomic_int_compare_and_exchange (gint *atomic, 
				   gint oldval, 
				   gint newval)
{
  gint result;
 
  __asm__ __volatile__ ("lock; cmpxchgl %2, %1"
			: "=a" (result), "=m" (*atomic)
			: "r" (newval), "m" (*atomic), "0" (oldval)); 

  return result == oldval;
}

gboolean
g_atomic_pointer_compare_and_exchange (gpointer *atomic, 
				       gpointer  oldval, 
				       gpointer  newval)
{
  gpointer result;
 
  __asm__ __volatile__ ("lock; cmpxchgq %q2, %1"
			: "=a" (result), "=m" (*atomic)
			: "r" (newval), "m" (*atomic), "0" (oldval)); 

  return result == oldval;
}

# elif defined (G_ATOMIC_POWERPC)
/* Adapted from CVS version 1.12 of glibc's sysdeps/powerpc/bits/atomic.h 
 * and CVS version 1.3 of glibc's sysdeps/powerpc/powerpc32/bits/atomic.h 
 * and CVS version 1.2 of glibc's sysdeps/powerpc/powerpc64/bits/atomic.h 
 */
#   ifdef __OPTIMIZE__
/* Non-optimizing compile bails on the following two asm statements
 * for reasons unknown to the author */
gint
g_atomic_int_exchange_and_add (gint *atomic, 
			       gint val)
{
  gint result, temp;
  __asm__ __volatile__ ("1:       lwarx   %0,0,%3\n"
			"         add     %1,%0,%4\n"
			"         stwcx.  %1,0,%3\n"
			"         bne-    1b"
			: "=&b" (result), "=&r" (temp), "=m" (*atomic)
			: "b" (atomic), "r" (val), "2" (*atomic)
			: "cr0", "memory");
  return result;
}
 
/* The same as above, to save a function call repeated here */
void
g_atomic_int_add (gint *atomic, 
		  gint val)
{
  gint result, temp;  
  __asm__ __volatile__ ("1:       lwarx   %0,0,%3\n"
			"         add     %1,%0,%4\n"
			"         stwcx.  %1,0,%3\n"
			"         bne-    1b"
			: "=&b" (result), "=&r" (temp), "=m" (*atomic)
			: "b" (atomic), "r" (val), "2" (*atomic)
			: "cr0", "memory");
}
#   else /* !__OPTIMIZE__ */
gint
g_atomic_int_exchange_and_add (gint *atomic, 
			       gint val)
{
  gint result;
  do
    result = *atomic;
  while (!g_atomic_int_compare_and_exchange (atomic, result, result + val));

  return result;
}
 
void
g_atomic_int_add (gint *atomic, 
		  gint val)
{
  gint result;
  do
    result = *atomic;
  while (!g_atomic_int_compare_and_exchange (atomic, result, result + val));
}
#   endif /* !__OPTIMIZE__ */

#   if GLIB_SIZEOF_VOID_P == 4 /* 32-bit system */
gboolean
g_atomic_int_compare_and_exchange (gint *atomic, 
				   gint oldval, 
				   gint newval)
{
  gint result;
  __asm__ __volatile__ ("sync\n"
			"1: lwarx   %0,0,%1\n"
			"   subf.   %0,%2,%0\n"
			"   bne     2f\n"
			"   stwcx.  %3,0,%1\n"
			"   bne-    1b\n"
			"2: isync"
			: "=&r" (result)
			: "b" (atomic), "r" (oldval), "r" (newval)
			: "cr0", "memory"); 
  return result == 0;
}

gboolean
g_atomic_pointer_compare_and_exchange (gpointer *atomic, 
				       gpointer  oldval, 
				       gpointer  newval)
{
  gpointer result;
  __asm__ __volatile__ ("sync\n"
			"1: lwarx   %0,0,%1\n"
			"   subf.   %0,%2,%0\n"
			"   bne     2f\n"
			"   stwcx.  %3,0,%1\n"
			"   bne-    1b\n"
			"2: isync"
			: "=&r" (result)
			: "b" (atomic), "r" (oldval), "r" (newval)
			: "cr0", "memory"); 
  return result == 0;
}
#   elif GLIB_SIZEOF_VOID_P == 8 /* 64-bit system */
gboolean
g_atomic_int_compare_and_exchange (gint *atomic, 
				   gint oldval, 
				   gint newval)
{
  gpointer result;
  __asm__ __volatile__ ("sync\n"
			"1: lwarx   %0,0,%1\n"
			"   extsw   %0,%0\n"
			"   subf.   %0,%2,%0\n"
			"   bne     2f\n"
			"   stwcx.  %3,0,%1\n"
			"   bne-    1b\n"
			"2: isync"
			: "=&r" (result)
			: "b" (atomic), "r" (oldval), "r" (newval)
			: "cr0", "memory"); 
  return result == 0;
}

gboolean
g_atomic_pointer_compare_and_exchange (gpointer *atomic, 
				       gpointer  oldval, 
				       gpointer  newval)
{
  gpointer result;
  __asm__ __volatile__ ("sync\n"
			"1: ldarx   %0,0,%1\n"
			"   subf.   %0,%2,%0\n"
			"   bne     2f\n"
			"   stdcx.  %3,0,%1\n"
			"   bne-    1b\n"
			"2: isync"
			: "=&r" (result)
			: "b" (atomic), "r" (oldval), "r" (newval)
			: "cr0", "memory"); 
  return result == 0;
}
#  else /* What's that */
#   error "Your system has an unsupported pointer size"
#  endif /* GLIB_SIZEOF_VOID_P */

#  define G_ATOMIC_MEMORY_BARRIER __asm__ ("sync" : : : "memory")

# elif defined (G_ATOMIC_IA64)
/* Adapted from CVS version 1.8 of glibc's sysdeps/ia64/bits/atomic.h
 */
gint
g_atomic_int_exchange_and_add (gint *atomic, 
			       gint val)
{
  return __sync_fetch_and_add_si (atomic, val);
}
 
void
g_atomic_int_add (gint *atomic, 
		  gint val)
{
  __sync_fetch_and_add_si (atomic, val);
}

gboolean
g_atomic_int_compare_and_exchange (gint *atomic, 
				   gint oldval, 
				   gint newval)
{
  return __sync_bool_compare_and_swap_si (atomic, oldval, newval);
}

gboolean
g_atomic_pointer_compare_and_exchange (gpointer *atomic, 
				       gpointer  oldval, 
				       gpointer  newval)
{
  return __sync_bool_compare_and_swap_di ((long *)atomic, 
					  (long)oldval, (long)newval);
}

#  define G_ATOMIC_MEMORY_BARRIER __sync_synchronize ()
# else /* !G_ATOMIC */
#  define DEFINE_WITH_MUTEXES
# endif /* G_ATOMIC */
#else /* !__GNUC__ */
# ifdef G_PLATFORM_WIN32
#  define DEFINE_WITH_WIN32_INTERLOCKED
# else
#  define DEFINE_WITH_MUTEXES
# endif
#endif /* __GNUC__ */

#ifdef DEFINE_WITH_WIN32_INTERLOCKED
# include <windows.h>
gint32   
g_atomic_int_exchange_and_add (gint32   *atomic, 
			       gint32    val)
{
  return InterlockedExchangeAdd (atomic, val);
}

void     
g_atomic_int_add (gint32   *atomic, 
		  gint32    val)
{
  InterlockedExchangeAdd (atomic, val);
}

gboolean 
g_atomic_int_compare_and_exchange (gint32   *atomic, 
				   gint32    oldval, 
				   gint32    newval)
{
  return (guint32)InterlockedCompareExchange ((PVOID*)atomic, 
                                              (PVOID)newval, 
                                              (PVOID)oldval) == oldval;
}

gboolean 
g_atomic_pointer_compare_and_exchange (gpointer *atomic, 
				       gpointer  oldval, 
				       gpointer  newval)
{
# if GLIB_SIZEOF_VOID_P != 4 /* no 32-bit system */
#  error "InterlockedCompareExchangePointer needed"
# else
   return InterlockedCompareExchange (atomic, newval, oldval) == oldval;
# endif
}
#endif /* DEFINE_WITH_WIN32_INTERLOCKED */

#ifdef DEFINE_WITH_MUTEXES
/* We have to use the slow, but safe locking method */
static GMutex *g_atomic_mutex; 

gint
g_atomic_int_exchange_and_add (gint *atomic, 
			       gint  val)
{
  gint result;
    
  g_mutex_lock (g_atomic_mutex);
  result = *atomic;
  *atomic += val;
  g_mutex_unlock (g_atomic_mutex);

  return result;
}


void
g_atomic_int_add (gint *atomic,
		  gint  val)
{
  g_mutex_lock (g_atomic_mutex);
  *atomic += val;
  g_mutex_unlock (g_atomic_mutex);
}

gboolean
g_atomic_int_compare_and_exchange (gint *atomic, 
				   gint  oldval, 
				   gint  newval)
{
  gboolean result;
    
  g_mutex_lock (g_atomic_mutex);
  if (*atomic == oldval)
    {
      result = TRUE;
      *atomic = newval;
    }
  else
    result = FALSE;
  g_mutex_unlock (g_atomic_mutex);

  return result;
}

gboolean
g_atomic_pointer_compare_and_exchange (gpointer *atomic, 
				       gpointer  oldval, 
				       gpointer  newval)
{
  gboolean result;
    
  g_mutex_lock (g_atomic_mutex);
  if (*atomic == oldval)
    {
      result = TRUE;
      *atomic = newval;
    }
  else
    result = FALSE;
  g_mutex_unlock (g_atomic_mutex);

  return result;
}

#ifdef G_ATOMIC_OP_MEMORY_BARRIER_NEEDED
gint
g_atomic_int_get (gint *atomic)
{
  gint result;

  g_mutex_lock (g_atomic_mutex);
  result = *atomic;
  g_mutex_unlock (g_atomic_mutex);

  return result;
}

gpointer
g_atomic_pointer_get (gpointer *atomic)
{
  gpointer result;

  g_mutex_lock (g_atomic_mutex);
  result = *atomic;
  g_mutex_unlock (g_atomic_mutex);

  return result;
}
#endif /* G_ATOMIC_OP_MEMORY_BARRIER_NEEDED */   
#elif defined (G_ATOMIC_OP_MEMORY_BARRIER_NEEDED)
gint
g_atomic_int_get (gint *atomic)
{
  gint result = *atomic;

  G_ATOMIC_MEMORY_BARRIER;

  return result;
}

gpointer
g_atomic_pointer_get (gpointer *atomic)
{
  gpointer result = *atomic;

  G_ATOMIC_MEMORY_BARRIER;

  return result;
}   
#endif /* DEFINE_WITH_MUTEXES || G_ATOMIC_OP_MEMORY_BARRIER_NEEDED */

#ifdef ATOMIC_INT_CMP_XCHG
gboolean
g_atomic_int_compare_and_exchange (gint *atomic, 
				   gint oldval, 
				   gint newval)
{
  return ATOMIC_INT_CMP_XCHG (atomic, oldval, newval);
}

gint
g_atomic_int_exchange_and_add (gint *atomic, 
			       gint val)
{
  gint result;
  do
    result = *atomic;
  while (!ATOMIC_INT_CMP_XCHG (atomic, result, result + val));

  return result;
}
 
void
g_atomic_int_add (gint *atomic, 
		  gint val)
{
  gint result;
  do
    result = *atomic;
  while (!ATOMIC_INT_CMP_XCHG (atomic, result, result + val));
}
#endif /* ATOMIC_INT_CMP_XCHG */

void 
_g_atomic_thread_init ()
{
#ifdef DEFINE_WITH_MUTEXES
  g_atomic_mutex = g_mutex_new ();
#endif /* DEFINE_WITH_MUTEXES */
}
