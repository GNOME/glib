/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * GAtomic: atomic integer operation.
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
 
/*
 * Modified by the GLib Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GLib Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GLib at ftp://ftp.gtk.org/pub/gtk/.
 */
 
#ifndef __G_ATOMIC_H__
#define __G_ATOMIC_H__
 
#include <glib/gtypes.h>

G_BEGIN_DECLS
 
#ifdef G_THREADS_ENABLED

gint32   g_atomic_int_exchange_and_add_fallback         (gint32   *atomic, 
							 gint32    val);
void     g_atomic_int_add_fallback                      (gint32   *atomic, 
							 gint32    val);
gboolean g_atomic_int_compare_and_exchange_fallback     (gint32   *atomic, 
							 gint32    oldval, 
							 gint32    newval);
gboolean g_atomic_pointer_compare_and_exchange_fallback (gpointer *atomic, 
							 gpointer  oldval, 
							 gpointer  newval);

# if defined (__GNUC__)
#   if defined (G_ATOMIC_INLINED_IMPLEMENTATION_I486)
/* Adapted from CVS version 1.10 of glibc's sysdeps/i386/i486/bits/atomic.h 
 */
static inline gint32
g_atomic_int_exchange_and_add (gint32 *atomic, 
			       gint32 val)
{
  gint32 result;

  __asm__ __volatile__ ("lock; xaddl %0,%1"
                        : "=r" (result), "=m" (*atomic) 
			: "0" (val), "m" (*atomic));
  return result;
}
 
static inline void
g_atomic_int_add (gint32 *atomic, 
		  gint32 val)
{
  __asm__ __volatile__ ("lock; addl %1,%0"
			: "=m" (*atomic) 
			: "ir" (val), "m" (*atomic));
}

static inline gboolean
g_atomic_int_compare_and_exchange (gint32 *atomic, 
				   gint32 oldval, 
				   gint32 newval)
{
  gint32 result;
 
  __asm __volatile ("lock; cmpxchgl %2, %1"
		    : "=a" (result), "=m" (*atomic)
		    : "r" (newval), "m" (*atomic), "0" (oldval)); 

  return result == oldval;
}

/* The same code as above, as on i386 gpointer is 32 bit as well.
 * Duplicating the code here seems more natural than casting the
 * arguments and calling the former function */

static inline gboolean
g_atomic_pointer_compare_and_exchange (gpointer *atomic, 
				       gpointer  oldval, 
				       gpointer  newval)
{
  gpointer result;
 
  __asm __volatile ("lock; cmpxchgl %2, %1"
		    : "=a" (result), "=m" (*atomic)
		    : "r" (newval), "m" (*atomic), "0" (oldval)); 

  return result == oldval;
}

#      define G_ATOMIC_MEMORY_BARRIER() /* Not needed */

#   elif defined(G_ATOMIC_INLINED_IMPLEMENTATION_SPARCV9) \
   && (defined(__sparcv8) || defined(__sparcv9) || defined(__sparc_v9__))
/* Adapted from CVS version 1.3 of glibc's sysdeps/sparc/sparc64/bits/atomic.h
 */
/* Why the test for __sparcv8, wheras really the sparcv9 architecture
 * is required for the folowing assembler instructions? On
 * sparc-solaris the only difference detectable at compile time
 * between no -m and -mcpu=v9 is __sparcv8.
 *
 * However, in case -mcpu=v8 is set, the assembler will fail. This
 * should be rare however, as there are only very few v8-not-v9
 * machines still out there (and we can't do better).
 */
static inline gboolean
g_atomic_int_compare_and_exchange (gint32 *atomic, 
				   gint32 oldval, 
				   gint32 newval)
{
  gint32 result;
  __asm __volatile ("cas [%4], %2, %0"
                    : "=r" (result), "=m" (*atomic)
                    : "r" (oldval), "m" (*atomic), "r" (atomic),
                      "0" (newval));
  return result != 0;
}

#     if GLIB_SIZEOF_VOID_P == 4 /* 32-bit system */
static inline gboolean
g_atomic_pointer_compare_and_exchange (gpointer *atomic, 
				       gpointer  oldval, 
				       gpointer  newval)
{
  gpointer result;
  __asm __volatile ("cas [%4], %2, %0"
                    : "=r" (result), "=m" (*atomic)
                    : "r" (oldval), "m" (*atomic), "r" (atomic),
                      "0" (newval));
  return result != 0;
}
#     elif GLIB_SIZEOF_VOID_P == 8 /* 64-bit system */
static inline gboolean
g_atomic_pointer_compare_and_exchange (gpointer *atomic, 
				       gpointer  oldval, 
				       gpointer  newval)
{
  gpointer result;
  gpointer *a = atomic;
  __asm __volatile ("casx [%4], %2, %0"
                    : "=r" (result), "=m" (*a)
                    : "r" (oldval), "m" (*a), "r" (a),
                      "0" (newval));
  return result != 0;
}
#     else /* What's that */
#       error "Your system has an unsupported pointer size"
#     endif /* GLIB_SIZEOF_VOID_P */
static inline gint32
g_atomic_int_exchange_and_add (gint32 *atomic, 
			       gint32 val)
{
  gint32 result;
  do
    result = *atomic;
  while (!g_atomic_int_compare_and_exchange (atomic, result, result + val));

  return result;
}
 
static inline void
g_atomic_int_add (gint32 *atomic, 
		  gint32 val)
{
  g_atomic_int_exchange_and_add (atomic, val);
}

#      define G_ATOMIC_MEMORY_BARRIER()					\
  __asm __volatile ("membar #LoadLoad | #LoadStore"			\
                    " | #StoreLoad | #StoreStore" : : : "memory")

#   elif defined(G_ATOMIC_INLINED_IMPLEMENTATION_ALPHA)
/* Adapted from CVS version 1.3 of glibc's sysdeps/alpha/bits/atomic.h
 */
static inline gboolean
g_atomic_int_compare_and_exchange (gint32 *atomic, 
				   gint32 oldval, 
				   gint32 newval)
{
  gint32 result;
  gint32 prev;
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
          "Ir" ((gint64)oldval),
          "Ir" (newval)
        : "memory");
  return result != 0;
}
#     if GLIB_SIZEOF_VOID_P == 4 /* 32-bit system */
static inline gboolean
g_atomic_pointer_compare_and_exchange (gpointer *atomic, 
				       gpointer  oldval, 
				       gpointer  newval)
{
  gint32 result;
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
          "Ir" ((gint64)oldval),
          "Ir" (newval)
        : "memory");
  return result != 0;
}
#     elif GLIB_SIZEOF_VOID_P == 8 /* 64-bit system */
static inline gboolean
g_atomic_pointer_compare_and_exchange (gpointer *atomic, 
				       gpointer  oldval, 
				       gpointer  newval)
{
  gint32 result;
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
          "Ir" ((gint64)oldval),
          "Ir" (newval)
        : "memory");
  return result != 0;
}
#     else /* What's that */
#       error "Your system has an unsupported pointer size"
#     endif /* GLIB_SIZEOF_VOID_P */
static inline gint32
g_atomic_int_exchange_and_add (gint32 *atomic, 
			       gint32 val)
{
  gint32 result;
  do
    result = *atomic;
  while (!g_atomic_int_compare_and_exchange (atomic, result, result + val));

  return result;
}
 
static inline void
g_atomic_int_add (gint32 *atomic, 
		  gint32 val)
{
  g_atomic_int_exchange_and_add (atomic, val);
}

#      define G_ATOMIC_MEMORY_BARRIER() __asm ("mb" : : : "memory")

#   elif defined(G_ATOMIC_INLINED_IMPLEMENTATION_X86_64)
/* Adapted from CVS version 1.9 of glibc's sysdeps/x86_64/bits/atomic.h 
 */
static inline gint32
g_atomic_int_exchange_and_add (gint32 *atomic, 
			       gint32 val)
{
  gint32 result;

  __asm__ __volatile__ ("lock; xaddl %0,%1"
                        : "=r" (result), "=m" (*atomic) 
			: "0" (val), "m" (*atomic));
  return result;
}
 
static inline void
g_atomic_int_add (gint32 *atomic, 
		  gint32 val)
{
  __asm__ __volatile__ ("lock; addl %1,%0"
			: "=m" (*atomic) 
			: "ir" (val), "m" (*atomic));
}

static inline gboolean
g_atomic_int_compare_and_exchange (gint32 *atomic, 
				   gint32 oldval, 
				   gint32 newval)
{
  gint32 result;
 
  __asm __volatile ("lock; cmpxchgl %2, %1"
		    : "=a" (result), "=m" (*atomic)
		    : "r" (newval), "m" (*atomic), "0" (oldval)); 

  return result == oldval;
}

static inline gboolean
g_atomic_pointer_compare_and_exchange (gpointer *atomic, 
				   gpointer  oldval, 
				   gpointer  newval)
{
  gpointer result;
 
  __asm __volatile ("lock; cmpxchgq %q2, %1"
		    : "=a" (result), "=m" (*atomic)
		    : "r" (newval), "m" (*atomic), "0" (oldval)); 

  return result == oldval;
}

#     define  G_ATOMIC_MEMORY_BARRIER() /* Not needed */

#   elif defined(G_ATOMIC_INLINED_IMPLEMENTATION_POWERPC)
/* Adapted from CVS version 1.12 of glibc's sysdeps/powerpc/bits/atomic.h 
 * and CVS version 1.3 of glibc's sysdeps/powerpc/powerpc32/bits/atomic.h 
 * and CVS version 1.2 of glibc's sysdeps/powerpc/powerpc64/bits/atomic.h 
 */
static inline gint32
g_atomic_int_exchange_and_add (gint32 *atomic, 
			  gint32 val)
{
  gint32 result, temp;
  __asm __volatile ("1:       lwarx   %0,0,%3\n"
		    "         add     %1,%0,%4\n"
		    "         stwcx.  %1,0,%3\n"
		    "         bne-    1b"
		    : "=&b" (result), "=&r" (temp), "=m" (*atomic)
		    : "b" (atomic), "r" (val), "2" (*atomic)
		    : "cr0", "memory");
  return result;
}
 
static inline void
g_atomic_int_add (gint32 *atomic, 
		  gint32 val)
{
  g_atomic_int_exchange_and_add (atomic, val);
}

#     if GLIB_SIZEOF_VOID_P == 4 /* 32-bit system */
static inline gboolean
g_atomic_int_compare_and_exchange (gint32 *atomic, 
			       gint32 oldval, 
			       gint32 newval)
{
  gint32 result;
  __asm __volatile ("sync\n"
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

static inline gboolean
g_atomic_pointer_compare_and_exchange (gpointer *atomic, 
				   gpointer  oldval, 
				   gpointer  newval)
{
  gpointer result;
  __asm __volatile ("sync\n"
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
#     elif GLIB_SIZEOF_VOID_P == 8 /* 64-bit system */
static inline gboolean
g_atomic_int_compare_and_exchange (gint32 *atomic, 
			       gint32 oldval, 
			       gint32 newval)
{
  __asm __volatile ("sync\n"
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

static inline gboolean
g_atomic_pointer_compare_and_exchange (gpointer *atomic, 
				   gpointer  oldval, 
				   gpointer  newval)
{
  gpointer result;
  __asm __volatile ("sync\n"
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
#     else /* What's that */
#       error "Your system has an unsupported pointer size"
#     endif /* GLIB_SIZEOF_VOID_P */

#     define  G_ATOMIC_MEMORY_BARRIER() __asm ("sync" : : : "memory")

#   elif defined(G_ATOMIC_INLINED_IMPLEMENTATION_IA64)
/* Adapted from CVS version 1.8 of glibc's sysdeps/ia64/bits/atomic.h
 */
static inline gint32
g_atomic_int_exchange_and_add (gint32 *atomic, 
			  gint32 val)
{
  return __sync_fetch_and_add_si (atomic, val);
}
 
static inline void
g_atomic_int_add (gint32 *atomic, 
		  gint32 val)
{
  __sync_fetch_and_add_si (atomic, val);
}

static inline gboolean
g_atomic_int_compare_and_exchange (gint32 *atomic, 
			       gint32 oldval, 
			       gint32 newval)
{
  return __sync_bool_compare_and_exchange_si (atomic, oldval, newval);
}

static inline gboolean
g_atomic_pointer_compare_and_exchange (gpointer *atomic, 
				   gpointer  oldval, 
				   gpointer  newval)
{
  return __sync_bool_compare_and_exchange_di ((long *)atomic, 
					  (long)oldval, (long)newval);
}

#     define  G_ATOMIC_MEMORY_BARRIER() __sync_synchronize ()

#   else /* !G_ATOMIC_INLINED_IMPLEMENTATION_... */
#     define G_ATOMIC_USE_FALLBACK_IMPLEMENTATION
#   endif /* G_ATOMIC_INLINED_IMPLEMENTATION_... */
# else /* !__GNU__ */
#   define G_ATOMIC_USE_FALLBACK_IMPLEMENTATION
# endif /* __GNUC__ */
#else /* !G_THREADS_ENABLED */
gint32 g_atomic_int_exchange_and_add (gint32 *atomic, gint32  val);
# define g_atomic_int_add(atomic, val) (void)(*(atomic) += (val))
# define g_atomic_int_compare_and_exchange(atomic, oldval, newval)		\
  (*(atomic) == (oldval) ? (*(atomic) = (newval), TRUE) : FALSE)
# define g_atomic_pointer_compare_and_exchange(atomic, oldval, newval)	\
  (*(atomic) == (oldval) ? (*(atomic) = (newval), TRUE) : FALSE)
# define g_atomic_int_get(atomic) (*(atomic))
# define g_atomic_pointer_get(atomic) (*(atomic))
#endif /* G_THREADS_ENABLED */

#ifdef G_ATOMIC_USE_FALLBACK_IMPLEMENTATION
# define g_atomic_int_exchange_and_add					\
    g_atomic_int_exchange_and_add_fallback
# define g_atomic_int_add						\
    g_atomic_int_add_fallback
# define g_atomic_int_compare_and_exchange					\
    g_atomic_int_compare_and_exchange_fallback
# define g_atomic_pointer_compare_and_exchange 				\
    g_atomic_pointer_compare_and_exchange_fallback
# define g_atomic_int_get 						\
    g_atomic_int_get_fallback
# define g_atomic_pointer_get 						\
    g_atomic_pointer_get_fallback
#else /* !G_ATOMIC_USE_FALLBACK_IMPLEMENTATION */
static inline gint32
g_atomic_int_get (gint32 *atomic)
{
  gint32 result = *atomic;
  G_ATOMIC_MEMORY_BARRIER ();
  return result;
}

static inline gpointer
g_atomic_pointer_get (gpointer *atomic)
{
  gpointer result = *atomic;
  G_ATOMIC_MEMORY_BARRIER ();
  return result;
}   
#endif /* G_ATOMIC_USE_FALLBACK_IMPLEMENTATION */

#define g_atomic_int_inc(atomic) (g_atomic_int_add ((atomic), 1))
#define g_atomic_int_dec_and_test(atomic)				\
  (g_atomic_int_exchange_and_add ((atomic), -1) == 1)

G_END_DECLS
 
#endif /* __G_ATOMIC_H__ */
