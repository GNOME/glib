/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
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
#ifndef __G_LIB_H__
#define __G_LIB_H__

/* system specific config file
 */
#include <glibconfig.h>

/* include varargs functions for assertment macros
 */
#include <stdarg.h>

/* optionally feature DMALLOC memory allocation debugger
 */
#ifdef USE_DMALLOC
#include "dmalloc.h"
#endif


#ifdef NATIVE_WIN32

/* On native Win32, directory separator is the backslash, and search path
 * separator is the semicolon.
 */
#define G_DIR_SEPARATOR '\\'
#define G_DIR_SEPARATOR_S "\\"
#define G_SEARCHPATH_SEPARATOR ';'
#define G_SEARCHPATH_SEPARATOR_S ";"

#else  /* !NATIVE_WIN32 */

/* Unix */

#define G_DIR_SEPARATOR '/'
#define G_DIR_SEPARATOR_S "/"
#define G_SEARCHPATH_SEPARATOR ':'
#define G_SEARCHPATH_SEPARATOR_S ":"

#endif /* !NATIVE_WIN32 */

#ifdef _MSC_VER
/* Make MSVC more pedantic, this is a recommended pragma list
 * from _Win32_Programming_ by Rector and Newcomer.
 */
#pragma warning(error:4002)
#pragma warning(error:4003)
#pragma warning(1:4010)
#pragma warning(error:4013)
#pragma warning(1:4016)
#pragma warning(error:4020)
#pragma warning(error:4021)
#pragma warning(error:4027)
#pragma warning(error:4029)
#pragma warning(error:4033)
#pragma warning(error:4035)
#pragma warning(error:4045)
#pragma warning(error:4047)
#pragma warning(error:4049)
#pragma warning(error:4053)
#pragma warning(error:4071)
#pragma warning(disable:4101)
#pragma warning(error:4150)

#pragma warning(disable:4244)	/* No possible loss of data warnings, please */
#endif /* _MSC_VER */

/* glib provides definitions for the extrema of many
 *  of the standard types. These are:
 * G_MINFLOAT
 * G_MAXFLOAT
 * G_MINDOUBLE
 * G_MAXDOUBLE
 * G_MINSHORT
 * G_MAXSHORT
 * G_MININT
 * G_MAXINT
 * G_MINLONG
 * G_MAXLONG
 *
 * We include limits.h before float.h to work around a egcs 1.1
 * oddity on Solaris 2.5.1
 */
#ifdef HAVE_LIMITS_H
#  include <limits.h>
#  define G_MINSHORT  SHRT_MIN
#  define G_MAXSHORT  SHRT_MAX
#  define G_MININT    INT_MIN
#  define G_MAXINT    INT_MAX
#  define G_MINLONG   LONG_MIN
#  define G_MAXLONG   LONG_MAX
#elif HAVE_VALUES_H
#  ifdef HAVE_FLOAT_H
#    include <values.h>
#  endif /* HAVE_FLOAT_H */
#  define G_MINSHORT  MINSHORT
#  define G_MAXSHORT  MAXSHORT
#  define G_MININT    MININT
#  define G_MAXINT    MAXINT
#  define G_MINLONG   MINLONG
#  define G_MAXLONG   MAXLONG
#endif /* HAVE_VALUES_H */

#ifdef HAVE_FLOAT_H
#  include <float.h>
#  define G_MINFLOAT   FLT_MIN
#  define G_MAXFLOAT   FLT_MAX
#  define G_MINDOUBLE  DBL_MIN
#  define G_MAXDOUBLE  DBL_MAX
#elif HAVE_VALUES_H
#  include <values.h>
#  define G_MINFLOAT  MINFLOAT
#  define G_MAXFLOAT  MAXFLOAT
#  define G_MINDOUBLE MINDOUBLE
#  define G_MAXDOUBLE MAXDOUBLE
#endif /* HAVE_VALUES_H */


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


/* Provide definitions for some commonly used macros.
 *  Some of them are only provided if they haven't already
 *  been defined. It is assumed that if they are already
 *  defined then the current definition is correct.
 */
#ifndef	NULL
#define	NULL	((void*) 0)
#endif

#ifndef	FALSE
#define	FALSE	(0)
#endif

#ifndef	TRUE
#define	TRUE	(!FALSE)
#endif

#undef	MAX
#define MAX(a, b)  (((a) > (b)) ? (a) : (b))

#undef	MIN
#define MIN(a, b)  (((a) < (b)) ? (a) : (b))

#undef	ABS
#define ABS(a)	   (((a) < 0) ? -(a) : (a))

#undef	CLAMP
#define CLAMP(x, low, high)  (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))


/* Define G_VA_COPY() to do the right thing for copying va_list variables.
 * glibconfig.h may have already defined G_VA_COPY as va_copy or __va_copy.
 */
#if !defined (G_VA_COPY)
#  if defined (__GNUC__) && defined (__PPC__) && (defined (_CALL_SYSV) || defined (_WIN32))
#  define G_VA_COPY(ap1, ap2)	  (*(ap1) = *(ap2))
#  elif defined (G_VA_COPY_AS_ARRAY)
#  define G_VA_COPY(ap1, ap2)	  g_memmove ((ap1), (ap2), sizeof (va_list))
#  else /* va_list is a pointer */
#  define G_VA_COPY(ap1, ap2)	  ((ap1) = (ap2))
#  endif /* va_list is a pointer */
#endif /* !G_VA_COPY */


/* Provide simple enum value macro wrappers that ease automated
 * enum value stringification code. [abandoned]
 */
#if	!defined (G_CODE_GENERATION)
#define G_ENUM( EnumerationName )		EnumerationName
#define G_FLAGS( EnumerationName )		EnumerationName
#define G_NV( VALUE_NAME , value_nick, VALUE)	VALUE_NAME = (VALUE)
#define G_SV( VALUE_NAME, value_nick )		VALUE_NAME
#else	/* G_CODE_GENERATION */
#define G_ENUM( EnumerationName )		G_ENUM_E + EnumerationName +
#define G_FLAGS( EnumerationName )		G_ENUM_F + EnumerationName +
#define G_NV( VALUE_NAME , value_nick, VALUE)	G_ENUM_V + VALUE_NAME + value_nick +
#define G_SV( VALUE_NAME, value_nick )		G_ENUM_V + VALUE_NAME + value_nick +
#endif	/* G_CODE_GENERATION */


/* inlining hassle. for compilers that don't allow the `inline' keyword,
 * mostly because of strict ANSI C compliance or dumbness, we try to fall
 * back to either `__inline__' or `__inline'.
 * we define G_CAN_INLINE, if the compiler seems to be actually
 * *capable* to do function inlining, in which case inline function bodys
 * do make sense. we also define G_INLINE_FUNC to properly export the
 * function prototypes if no inlinig can be performed.
 * we special case most of the stuff, so inline functions can have a normal
 * implementation by defining G_INLINE_FUNC to extern and G_CAN_INLINE to 1.
 */
#ifndef G_INLINE_FUNC
#  define G_CAN_INLINE 1
#endif
#ifdef G_HAVE_INLINE
#  if defined (__GNUC__) && defined (__STRICT_ANSI__)
#    undef inline
#    define inline __inline__
#  endif
#else /* !G_HAVE_INLINE */
#  undef inline
#  if defined (G_HAVE___INLINE__)
#    define inline __inline__
#  else /* !inline && !__inline__ */
#    if defined (G_HAVE___INLINE)
#      define inline __inline
#    else /* !inline && !__inline__ && !__inline */
#      define inline /* don't inline, then */
#      ifndef G_INLINE_FUNC
#	 undef G_CAN_INLINE
#      endif
#    endif
#  endif
#endif
#ifndef G_INLINE_FUNC
#  ifdef __GNUC__
#    ifdef __OPTIMIZE__
#      define G_INLINE_FUNC extern inline
#    else
#      undef G_CAN_INLINE
#      define G_INLINE_FUNC extern
#    endif
#  else /* !__GNUC__ */
#    ifdef G_CAN_INLINE
#      define G_INLINE_FUNC static inline
#    else
#      define G_INLINE_FUNC extern
#    endif
#  endif /* !__GNUC__ */
#endif /* !G_INLINE_FUNC */


/* Provide simple macro statement wrappers (adapted from Perl):
 *  G_STMT_START { statements; } G_STMT_END;
 *  can be used as a single statement, as in
 *  if (x) G_STMT_START { ... } G_STMT_END; else ...
 *
 *  For gcc we will wrap the statements within `({' and `})' braces.
 *  For SunOS they will be wrapped within `if (1)' and `else (void) 0',
 *  and otherwise within `do' and `while (0)'.
 */
#if !(defined (G_STMT_START) && defined (G_STMT_END))
#  if defined (__GNUC__) && !defined (__STRICT_ANSI__) && !defined (__cplusplus)
#    define G_STMT_START	(void)(
#    define G_STMT_END		)
#  else
#    if (defined (sun) || defined (__sun__))
#      define G_STMT_START	if (1)
#      define G_STMT_END	else (void)0
#    else
#      define G_STMT_START	do
#      define G_STMT_END	while (0)
#    endif
#  endif
#endif


/* Provide macros to feature the GCC function attribute.
 */
#if	__GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ > 4)
#define G_GNUC_PRINTF( format_idx, arg_idx )	\
  __attribute__((format (printf, format_idx, arg_idx)))
#define G_GNUC_SCANF( format_idx, arg_idx )	\
  __attribute__((format (scanf, format_idx, arg_idx)))
#define G_GNUC_FORMAT( arg_idx )		\
  __attribute__((format_arg (arg_idx)))
#define G_GNUC_NORETURN				\
  __attribute__((noreturn))
#define G_GNUC_CONST				\
  __attribute__((const))
#define G_GNUC_UNUSED				\
  __attribute__((unused))
#else	/* !__GNUC__ */
#define G_GNUC_PRINTF( format_idx, arg_idx )
#define G_GNUC_SCANF( format_idx, arg_idx )
#define G_GNUC_FORMAT( arg_idx )
#define G_GNUC_NORETURN
#define G_GNUC_CONST
#define	G_GNUC_UNUSED
#endif	/* !__GNUC__ */


/* Wrap the gcc __PRETTY_FUNCTION__ and __FUNCTION__ variables with
 * macros, so we can refer to them as strings unconditionally.
 */
#ifdef	__GNUC__
#define	G_GNUC_FUNCTION		(__FUNCTION__)
#define	G_GNUC_PRETTY_FUNCTION	(__PRETTY_FUNCTION__)
#else	/* !__GNUC__ */
#define	G_GNUC_FUNCTION		("")
#define	G_GNUC_PRETTY_FUNCTION	("")
#endif	/* !__GNUC__ */


/* we try to provide a usefull equivalent for ATEXIT if it is
 * not defined, but use is actually abandoned. people should
 * use g_atexit() instead.
 * keep this in sync with gutils.c.
 */
#ifndef ATEXIT
#  ifdef HAVE_ATEXIT
#    ifdef NeXT /* @#%@! NeXTStep */
#      define ATEXIT(proc)   (!atexit (proc))
#    else /* !NeXT */
#      define ATEXIT(proc)   (atexit (proc))
#    endif /* !NeXT */
#  elif defined (HAVE_ON_EXIT)
#    define ATEXIT(proc)   (on_exit ((void (*)(int, void *))(proc), NULL))
#  else
#  error Could not determine proper atexit() implementation
#  endif
#else
#  define G_NATIVE_ATEXIT
#endif /* ATEXIT */

/* Hacker macro to place breakpoints for x86 machines.
 * Actual use is strongly deprecated of course ;)
 */
#if defined (__i386__) && defined (__GNUC__)
#define	G_BREAKPOINT()		G_STMT_START{ __asm__ __volatile__ ("int $03"); }G_STMT_END
#else	/* !__i386__ */
#define	G_BREAKPOINT()
#endif	/* __i386__ */


/* Provide macros for easily allocating memory. The macros
 *  will cast the allocated memory to the specified type
 *  in order to avoid compiler warnings. (Makes the code neater).
 */

#ifdef __DMALLOC_H__
#  define g_new(type, count)		(ALLOC (type, count))
#  define g_new0(type, count)		(CALLOC (type, count))
#  define g_renew(type, mem, count)	(REALLOC (mem, type, count))
#else /* __DMALLOC_H__ */
#  define g_new(type, count)	  \
      ((type *) g_malloc ((unsigned) sizeof (type) * (count)))
#  define g_new0(type, count)	  \
      ((type *) g_malloc0 ((unsigned) sizeof (type) * (count)))
#  define g_renew(type, mem, count)	  \
      ((type *) g_realloc (mem, (unsigned) sizeof (type) * (count)))
#endif /* __DMALLOC_H__ */

#define g_mem_chunk_create(type, pre_alloc, alloc_type)	( \
  g_mem_chunk_new (#type " mem chunks (" #pre_alloc ")", \
		   sizeof (type), \
		   sizeof (type) * (pre_alloc), \
		   (alloc_type)) \
)
#define g_chunk_new(type, chunk)	( \
  (type *) g_mem_chunk_alloc (chunk) \
)
#define g_chunk_new0(type, chunk)	( \
  (type *) g_mem_chunk_alloc0 (chunk) \
)
#define g_chunk_free(mem, mem_chunk)	G_STMT_START { \
  g_mem_chunk_free ((mem_chunk), (mem)); \
} G_STMT_END


#define g_string(x) #x


/* Provide macros for error handling. The "assert" macros will
 *  exit on failure. The "return" macros will exit the current
 *  function. Two different definitions are given for the macros
 *  if G_DISABLE_ASSERT is not defined, in order to support gcc's
 *  __PRETTY_FUNCTION__ capability.
 */

#ifdef G_DISABLE_ASSERT

#define g_assert(expr)
#define g_assert_not_reached()

#else /* !G_DISABLE_ASSERT */

#ifdef __GNUC__

#define g_assert(expr)			G_STMT_START{		\
     if (!(expr))						\
       g_log (G_LOG_DOMAIN,					\
	      G_LOG_LEVEL_ERROR,				\
	      "file %s: line %d (%s): assertion failed: (%s)",	\
	      __FILE__,						\
	      __LINE__,						\
	      __PRETTY_FUNCTION__,				\
	      #expr);			}G_STMT_END

#define g_assert_not_reached()		G_STMT_START{		\
     g_log (G_LOG_DOMAIN,					\
	    G_LOG_LEVEL_ERROR,					\
	    "file %s: line %d (%s): should not be reached",	\
	    __FILE__,						\
	    __LINE__,						\
	    __PRETTY_FUNCTION__);	}G_STMT_END

#else /* !__GNUC__ */

#define g_assert(expr)			G_STMT_START{		\
     if (!(expr))						\
       g_log (G_LOG_DOMAIN,					\
	      G_LOG_LEVEL_ERROR,				\
	      "file %s: line %d: assertion failed: (%s)",	\
	      __FILE__,						\
	      __LINE__,						\
	      #expr);			}G_STMT_END

#define g_assert_not_reached()		G_STMT_START{	\
     g_log (G_LOG_DOMAIN,				\
	    G_LOG_LEVEL_ERROR,				\
	    "file %s: line %d: should not be reached",	\
	    __FILE__,					\
	    __LINE__);		}G_STMT_END

#endif /* __GNUC__ */

#endif /* !G_DISABLE_ASSERT */


#ifdef G_DISABLE_CHECKS

#define g_return_if_fail(expr)
#define g_return_val_if_fail(expr,val)

#else /* !G_DISABLE_CHECKS */

#ifdef __GNUC__

#define g_return_if_fail(expr)		G_STMT_START{			\
     if (!(expr))							\
       {								\
	 g_log (G_LOG_DOMAIN,						\
		G_LOG_LEVEL_CRITICAL,					\
		"file %s: line %d (%s): assertion `%s' failed.",	\
		__FILE__,						\
		__LINE__,						\
		__PRETTY_FUNCTION__,					\
		#expr);							\
	 return;							\
       };				}G_STMT_END

#define g_return_val_if_fail(expr,val)	G_STMT_START{			\
     if (!(expr))							\
       {								\
	 g_log (G_LOG_DOMAIN,						\
		G_LOG_LEVEL_CRITICAL,					\
		"file %s: line %d (%s): assertion `%s' failed.",	\
		__FILE__,						\
		__LINE__,						\
		__PRETTY_FUNCTION__,					\
		#expr);							\
	 return val;							\
       };				}G_STMT_END

#else /* !__GNUC__ */

#define g_return_if_fail(expr)		G_STMT_START{		\
     if (!(expr))						\
       {							\
	 g_log (G_LOG_DOMAIN,					\
		G_LOG_LEVEL_CRITICAL,				\
		"file %s: line %d: assertion `%s' failed.",	\
		__FILE__,					\
		__LINE__,					\
		#expr);						\
	 return;						\
       };				}G_STMT_END

#define g_return_val_if_fail(expr, val)	G_STMT_START{		\
     if (!(expr))						\
       {							\
	 g_log (G_LOG_DOMAIN,					\
		G_LOG_LEVEL_CRITICAL,				\
		"file %s: line %d: assertion `%s' failed.",	\
		__FILE__,					\
		__LINE__,					\
		#expr);						\
	 return val;						\
       };				}G_STMT_END

#endif /* !__GNUC__ */

#endif /* !G_DISABLE_CHECKS */


/* Provide type definitions for commonly used types.
 *  These are useful because a "gint8" can be adjusted
 *  to be 1 byte (8 bits) on all platforms. Similarly and
 *  more importantly, "gint32" can be adjusted to be
 *  4 bytes (32 bits) on all platforms.
 */

typedef char   gchar;
typedef short  gshort;
typedef long   glong;
typedef int    gint;
typedef gint   gboolean;

typedef unsigned char	guchar;
typedef unsigned short	gushort;
typedef unsigned long	gulong;
typedef unsigned int	guint;

typedef float	gfloat;
typedef double	gdouble;

/* HAVE_LONG_DOUBLE doesn't work correctly on all platforms.
 * Since gldouble isn't used anywhere, just disable it for now */

#if 0
#ifdef HAVE_LONG_DOUBLE
typedef long double gldouble;
#else /* HAVE_LONG_DOUBLE */
typedef double gldouble;
#endif /* HAVE_LONG_DOUBLE */
#endif /* 0 */

typedef void* gpointer;
typedef const void *gconstpointer;

#if (SIZEOF_CHAR == 1)
typedef signed char	gint8;
typedef unsigned char	guint8;
#endif /* SIZEOF_CHAR */

#if (SIZEOF_SHORT == 2)
typedef signed short	gint16;
typedef unsigned short	guint16;
#endif /* SIZEOF_SHORT */

#if (SIZEOF_INT == 4)
typedef signed int	gint32;
typedef unsigned int	guint32;
#elif (SIZEOF_LONG == 4)
typedef signed long	gint32;
typedef unsigned long	guint32;
#endif /* SIZEOF_INT */

#if (SIZEOF_LONG == 8)
#define HAVE_GINT64 1
typedef signed long gint64;
typedef unsigned long guint64;
#elif (SIZEOF_LONG_LONG == 8)
#define HAVE_GINT64 1
typedef signed long long gint64;
typedef unsigned long long guint64;
#else
/* No gint64 */
#undef HAVE_GINT64
#endif


/* Define macros for storing integers inside pointers
 */
#if (SIZEOF_INT == SIZEOF_VOID_P)

#define GPOINTER_TO_INT(p) ((gint)(p))
#define GPOINTER_TO_UINT(p) ((guint)(p))

#define GINT_TO_POINTER(i) ((gpointer)(i))
#define GUINT_TO_POINTER(u) ((gpointer)(u))

#elif (SIZEOF_LONG == SIZEOF_VOID_P)

#define GPOINTER_TO_INT(p) ((gint)(glong)(p))
#define GPOINTER_TO_UINT(p) ((guint)(gulong)(p))

#define GINT_TO_POINTER(i) ((gpointer)(glong)(i))
#define GUINT_TO_POINTER(u) ((gpointer)(gulong)(u))

#else
#error SIZEOF_VOID_P unknown - This should never happen
#endif

typedef gint32	gssize;
typedef guint32 gsize;
typedef guint32 GQuark;
typedef gint32	GTime;


/* Portable endian checks and conversions
 */

#define G_LITTLE_ENDIAN 1234
#define G_BIG_ENDIAN    4321
#define G_PDP_ENDIAN    3412		/* unused, need specific PDP check */	

#ifdef WORDS_BIGENDIAN
#define G_BYTE_ORDER G_BIG_ENDIAN
#else
#define G_BYTE_ORDER G_LITTLE_ENDIAN
#endif

/* Basic bit swapping functions
 */
#define GUINT16_SWAP_LE_BE_CONSTANT(val)	((guint16) ( \
    (((guint16) (val) & (guint16) 0x00ffU) << 8) | \
    (((guint16) (val) & (guint16) 0xff00U) >> 8)))
#define GUINT32_SWAP_LE_BE_CONSTANT(val)	((guint32) ( \
    (((guint32) (val) & (guint32) 0x000000ffU) << 24) | \
    (((guint32) (val) & (guint32) 0x0000ff00U) <<  8) | \
    (((guint32) (val) & (guint32) 0x00ff0000U) >>  8) | \
    (((guint32) (val) & (guint32) 0xff000000U) >> 24)))

/* Intel specific stuff for speed
 */
#if defined (__i386__) && (defined __GNUC__)

#  define GUINT16_SWAP_LE_BE_X86(val) \
     (__extension__						\
      ({ register guint16 __v;					\
	 if (__builtin_constant_p (val))			\
	   __v = GUINT16_SWAP_LE_BE_CONSTANT (val);		\
	 else							\
	   __asm__ __volatile__ ("rorw $8, %w0"			\
				 : "=r" (__v)			\
				 : "0" ((guint16) (val))	\
				 : "cc");			\
	__v; }))

#  define GUINT16_SWAP_LE_BE(val) \
     ((guint16) GUINT16_SWAP_LE_BE_X86 ((guint16) (val)))

#  if !defined(__i486__) && !defined(__i586__) \
      && !defined(__pentium__) && !defined(__pentiumpro__) && !defined(__i686__)
#     define GUINT32_SWAP_LE_BE_X86(val) \
        (__extension__						\
         ({ register guint32 __v;				\
	    if (__builtin_constant_p (val))			\
	      __v = GUINT32_SWAP_LE_BE_CONSTANT (val);		\
	  else							\
	    __asm__ __volatile__ ("rorw $8, %w0\n\t"		\
				  "rorl $16, %0\n\t"		\
				  "rorw $8, %w0"		\
				  : "=r" (__v)			\
				  : "0" ((guint32) (val))	\
				  : "cc");			\
	__v; }))

#  else /* 486 and higher has bswap */
#     define GUINT32_SWAP_LE_BE_X86(val) \
        (__extension__						\
         ({ register guint32 __v;				\
	    if (__builtin_constant_p (val))			\
	      __v = GUINT32_SWAP_LE_BE_CONSTANT (val);		\
	  else							\
	    __asm__ __volatile__ ("bswap %0"			\
				  : "=r" (__v)			\
				  : "0" ((guint32) (val)));	\
	__v; }))
#  endif /* processor specific 32-bit stuff */

#  define GUINT32_SWAP_LE_BE(val) \
     ((guint32) GUINT32_SWAP_LE_BE_X86 ((guint32) (val)))

#else /* !__i386__ */
#  define GUINT16_SWAP_LE_BE(val) (GUINT16_SWAP_LE_BE_CONSTANT (val))
#  define GUINT32_SWAP_LE_BE(val) (GUINT32_SWAP_LE_BE_CONSTANT (val))
#endif /* __i386__ */

#ifdef HAVE_GINT64
#define GUINT64_SWAP_LE_BE(val)         ((guint64) ( \
    (((guint64) (val) & (guint64) 0x00000000000000ffU) << 56) | \
    (((guint64) (val) & (guint64) 0x000000000000ff00U) << 40) | \
    (((guint64) (val) & (guint64) 0x0000000000ff0000U) << 24) | \
    (((guint64) (val) & (guint64) 0x00000000ff000000U) <<  8) | \
    (((guint64) (val) & (guint64) 0x000000ff00000000U) >>  8) | \
    (((guint64) (val) & (guint64) 0x0000ff0000000000U) >> 24) | \
    (((guint64) (val) & (guint64) 0x00ff000000000000U) >> 40) | \
    (((guint64) (val) & (guint64) 0xff00000000000000U) >> 56)))
#endif

#define GUINT16_SWAP_LE_PDP(val)	((guint16) (val))
#define GUINT16_SWAP_BE_PDP(val)	(GUINT16_SWAP_LE_BE (val))
#define GUINT32_SWAP_LE_PDP(val)	((guint32) ( \
    (((guint32) (val) & (guint32) 0x0000ffffU) << 16) | \
    (((guint32) (val) & (guint32) 0xffff0000U) >> 16)))
#define GUINT32_SWAP_BE_PDP(val)	((guint32) ( \
    (((guint32) (val) & (guint32) 0x00ff00ffU) << 8) | \
    (((guint32) (val) & (guint32) 0xff00ff00U) >> 8)))

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
#  define GINT16_TO_LE(val)		((gint16) (val))
#  define GUINT16_TO_LE(val)		((guint16) (val))
#  define GINT16_TO_BE(val)		((gint16) GUINT16_SWAP_LE_BE (val))
#  define GUINT16_TO_BE(val)		(GUINT16_SWAP_LE_BE (val))
#  define GINT16_FROM_LE(val)		((gint16) (val))
#  define GUINT16_FROM_LE(val)		((guint16) (val))
#  define GINT16_FROM_BE(val)		((gint16) GUINT16_SWAP_LE_BE (val))
#  define GUINT16_FROM_BE(val)		(GUINT16_SWAP_LE_BE (val))
#  define GINT32_TO_LE(val)		((gint32) (val))
#  define GUINT32_TO_LE(val)		((guint32) (val))
#  define GINT32_TO_BE(val)		((gint32) GUINT32_SWAP_LE_BE (val))
#  define GUINT32_TO_BE(val)		(GUINT32_SWAP_LE_BE (val))
#  define GINT32_FROM_LE(val)		((gint32) (val))
#  define GUINT32_FROM_LE(val)		((guint32) (val))
#  define GINT32_FROM_BE(val)		((gint32) GUINT32_SWAP_LE_BE (val))
#  define GUINT32_FROM_BE(val)		(GUINT32_SWAP_LE_BE (val))
#  ifdef HAVE_GINT64
#  define GINT64_TO_LE(val)		((gint64) (val))
#  define GUINT64_TO_LE(val)		((guint64) (val))
#  define GINT64_TO_BE(val)		((gint64) GUINT64_SWAP_LE_BE (val))
#  define GUINT64_TO_BE(val)		(GUINT64_SWAP_LE_BE (val))
#  define GINT64_FROM_LE(val)		((gint64) (val))
#  define GUINT64_FROM_LE(val)		((guint64) (val))
#  define GINT64_FROM_BE(val)		((gint64) GUINT64_SWAP_LE_BE (val))
#  define GUINT64_FROM_BE(val)		(GUINT64_SWAP_LE_BE (val))
#  endif
#elif G_BYTE_ORDER == G_BIG_ENDIAN
#  define GINT16_TO_BE(val)		((gint16) (val))
#  define GUINT16_TO_BE(val)		((guint16) (val))
#  define GINT16_TO_LE(val)		((gint16) GUINT16_SWAP_LE_BE (val))
#  define GUINT16_TO_LE(val)		(GUINT16_SWAP_LE_BE (val))
#  define GINT16_FROM_BE(val)		((gint16) (val))
#  define GUINT16_FROM_BE(val)		((guint16) (val))
#  define GINT16_FROM_LE(val)		((gint16) GUINT16_SWAP_LE_BE (val))
#  define GUINT16_FROM_LE(val)		(GUINT16_SWAP_LE_BE (val))
#  define GINT32_TO_BE(val)		((gint32) (val))
#  define GUINT32_TO_BE(val)		((guint32) (val))
#  define GINT32_TO_LE(val)		((gint32) GUINT32_SWAP_LE_BE (val))
#  define GUINT32_TO_LE(val)		(GUINT32_SWAP_LE_BE (val))
#  define GINT32_FROM_BE(val)		((gint32) (val))
#  define GUINT32_FROM_BE(val)		((guint32) (val))
#  define GINT32_FROM_LE(val)		((gint32) GUINT32_SWAP_LE_BE (val))
#  define GUINT32_FROM_LE(val)		(GUINT32_SWAP_LE_BE (val))
#  ifdef HAVE_GINT64
#  define GINT64_TO_BE(val)		((gint64) (val))
#  define GUINT64_TO_BE(val)		((guint64) (val))
#  define GINT64_TO_LE(val)		((gint64) GUINT64_SWAP_LE_BE (val))
#  define GUINT64_TO_LE(val)		(GUINT64_SWAP_LE_BE (val))
#  define GINT64_FROM_BE(val)		((gint64) (val))
#  define GUINT64_FROM_BE(val)		((guint64) (val))
#  define GINT64_FROM_LE(val)		((gint64) GUINT64_SWAP_LE_BE (val))
#  define GUINT64_FROM_LE(val)		(GUINT64_SWAP_LE_BE (val))
#  endif
#else
/* PDP stuff not implemented */
#endif

#if (SIZEOF_LONG == 8)
#  define GLONG_TO_LE(val)		((glong) GINT64_TO_LE (val))
#  define GULONG_TO_LE(val)		((gulong) GUINT64_TO_LE (val))
#  define GLONG_TO_BE(val)		((glong) GINT64_TO_BE (val))
#  define GULONG_TO_BE(val)		((gulong) GUINT64_TO_BE (val))
#  define GLONG_FROM_LE(val)		((glong) GINT64_FROM_LE (val))
#  define GULONG_FROM_LE(val)		((gulong) GUINT64_FROM_LE (val))
#  define GLONG_FROM_BE(val)		((glong) GINT64_FROM_BE (val))
#  define GULONG_FROM_BE(val)		((gulong) GUINT64_FROM_BE (val))
#elif (SIZEOF_LONG == 4)
#  define GLONG_TO_LE(val)		((glong) GINT32_TO_LE (val))
#  define GULONG_TO_LE(val)		((gulong) GUINT32_TO_LE (val))
#  define GLONG_TO_BE(val)		((glong) GINT32_TO_BE (val))
#  define GULONG_TO_BE(val)		((gulong) GUINT32_TO_BE (val))
#  define GLONG_FROM_LE(val)		((glong) GINT32_FROM_LE (val))
#  define GULONG_FROM_LE(val)		((gulong) GUINT32_FROM_LE (val))
#  define GLONG_FROM_BE(val)		((glong) GINT32_FROM_BE (val))
#  define GULONG_FROM_BE(val)		((gulong) GUINT32_FROM_BE (val))
#endif

#if (SIZEOF_INT == 8)
#  define GINT_TO_LE(val)		((gint) GINT64_TO_LE (val))
#  define GUINT_TO_LE(val)		((guint) GUINT64_TO_LE (val))
#  define GINT_TO_BE(val)		((gint) GINT64_TO_BE (val))
#  define GUINT_TO_BE(val)		((guint) GUINT64_TO_BE (val))
#  define GINT_FROM_LE(val)		((gint) GINT64_FROM_LE (val))
#  define GUINT_FROM_LE(val)		((guint) GUINT64_FROM_LE (val))
#  define GINT_FROM_BE(val)		((gint) GINT64_FROM_BE (val))
#  define GUINT_FROM_BE(val)		((guint) GUINT64_FROM_BE (val))
#elif (SIZEOF_INT == 4)
#  define GINT_TO_LE(val)		((gint) GINT32_TO_LE (val))
#  define GUINT_TO_LE(val)		((guint) GUINT32_TO_LE (val))
#  define GINT_TO_BE(val)		((gint) GINT32_TO_BE (val))
#  define GUINT_TO_BE(val)		((guint) GUINT32_TO_BE (val))
#  define GINT_FROM_LE(val)		((gint) GINT32_FROM_LE (val))
#  define GUINT_FROM_LE(val)		((guint) GUINT32_FROM_LE (val))
#  define GINT_FROM_BE(val)		((gint) GINT32_FROM_BE (val))
#  define GUINT_FROM_BE(val)		((guint) GUINT32_FROM_BE (val))
#elif (SIZEOF_INT == 2)
#  define GINT_TO_LE(val)		((gint) GINT16_TO_LE (val))
#  define GUINT_TO_LE(val)		((guint) GUINT16_TO_LE (val))
#  define GINT_TO_BE(val)		((gint) GINT16_TO_BE (val))
#  define GUINT_TO_BE(val)		((guint) GUINT16_TO_BE (val))
#  define GINT_FROM_LE(val)		((gint) GINT16_FROM_LE (val))
#  define GUINT_FROM_LE(val)		((guint) GUINT16_FROM_LE (val))
#  define GINT_FROM_BE(val)		((gint) GINT16_FROM_BE (val))
#  define GUINT_FROM_BE(val)		((guint) GUINT16_FROM_BE (val))
#endif

/* Portable versions of host-network order stuff
 */
#define g_ntohl(val) (GUINT32_FROM_BE (val))
#define g_ntohs(val) (GUINT16_FROM_BE (val))
#define g_htonl(val) (GUINT32_TO_BE (val))
#define g_htons(val) (GUINT16_TO_BE (val))


/* Glib version.
 * we prefix variable declarations so they can
 * properly get exported in windows dlls.
 */
#ifdef NATIVE_WIN32
#  ifdef GLIB_COMPILATION
#    define GUTILS_C_VAR __declspec(dllexport)
#  else /* !GLIB_COMPILATION */
#    define GUTILS_C_VAR __declspec(dllimport)
#  endif /* !GLIB_COMPILATION */
#else /* !NATIVE_WIN32 */
#  define GUTILS_C_VAR extern
#endif /* !NATIVE_WIN32 */

GUTILS_C_VAR const guint glib_major_version;
GUTILS_C_VAR const guint glib_minor_version;
GUTILS_C_VAR const guint glib_micro_version;
GUTILS_C_VAR const guint glib_interface_age;
GUTILS_C_VAR const guint glib_binary_age;

/* Forward declarations of glib types.
 */

typedef struct _GArray		GArray;
typedef struct _GByteArray	GByteArray;
typedef struct _GCache		GCache;
typedef struct _GCompletion	GCompletion;
typedef	struct _GData		GData;
typedef struct _GDebugKey	GDebugKey;
typedef struct _GHashTable	GHashTable;
typedef struct _GHook		GHook;
typedef struct _GHookList	GHookList;
typedef struct _GList		GList;
typedef struct _GListAllocator	GListAllocator;
typedef struct _GMemChunk	GMemChunk;
typedef struct _GNode		GNode;
typedef struct _GPtrArray	GPtrArray;
typedef struct _GRelation	GRelation;
typedef struct _GScanner	GScanner;
typedef struct _GScannerConfig	GScannerConfig;
typedef struct _GSList		GSList;
typedef struct _GString		GString;
typedef struct _GStringChunk	GStringChunk;
typedef struct _GTimer		GTimer;
typedef struct _GTree		GTree;
typedef struct _GTuples		GTuples;
typedef union  _GTokenValue	GTokenValue;
typedef struct _GIOChannel	GIOChannel;


typedef enum
{
  G_TRAVERSE_LEAFS	= 1 << 0,
  G_TRAVERSE_NON_LEAFS	= 1 << 1,
  G_TRAVERSE_ALL	= G_TRAVERSE_LEAFS | G_TRAVERSE_NON_LEAFS,
  G_TRAVERSE_MASK	= 0x03
} GTraverseFlags;

typedef enum
{
  G_IN_ORDER,
  G_PRE_ORDER,
  G_POST_ORDER,
  G_LEVEL_ORDER
} GTraverseType;

/* Log level shift offset for user defined
 * log levels (0-7 are used by GLib).
 */
#define	G_LOG_LEVEL_USER_SHIFT	(8)

/* Glib log levels and flags.
 */
typedef enum
{
  /* log flags */
  G_LOG_FLAG_RECURSION		= 1 << 0,
  G_LOG_FLAG_FATAL		= 1 << 1,
  
  /* GLib log levels */
  G_LOG_LEVEL_ERROR		= 1 << 2,	/* always fatal */
  G_LOG_LEVEL_CRITICAL		= 1 << 3,
  G_LOG_LEVEL_WARNING		= 1 << 4,
  G_LOG_LEVEL_MESSAGE		= 1 << 5,
  G_LOG_LEVEL_INFO		= 1 << 6,
  G_LOG_LEVEL_DEBUG		= 1 << 7,
  
  G_LOG_LEVEL_MASK		= ~(G_LOG_FLAG_RECURSION | G_LOG_FLAG_FATAL)
} GLogLevelFlags;

/* GLib log levels that are considered fatal by default */
#define	G_LOG_FATAL_MASK	(G_LOG_FLAG_RECURSION | G_LOG_LEVEL_ERROR)


typedef gpointer	(*GCacheNewFunc)	(gpointer	key);
typedef gpointer	(*GCacheDupFunc)	(gpointer	value);
typedef void		(*GCacheDestroyFunc)	(gpointer	value);
typedef gint		(*GCompareFunc)		(gconstpointer	a,
						 gconstpointer	b);
typedef gchar*		(*GCompletionFunc)	(gpointer);
typedef void		(*GDestroyNotify)	(gpointer	data);
typedef void		(*GDataForeachFunc)	(GQuark		key_id,
						 gpointer	data,
						 gpointer	user_data);
typedef void		(*GFunc)		(gpointer	data,
						 gpointer	user_data);
typedef guint		(*GHashFunc)		(gconstpointer	key);
typedef void		(*GHFunc)		(gpointer	key,
						 gpointer	value,
						 gpointer	user_data);
typedef gboolean	(*GHRFunc)		(gpointer	key,
						 gpointer	value,
						 gpointer	user_data);
typedef gint		(*GHookCompareFunc)	(GHook		*new_hook,
						 GHook		*sibling);
typedef gboolean	(*GHookFindFunc)	(GHook		*hook,
						 gpointer	 data);
typedef void		(*GHookMarshaller)	(GHook		*hook,
						 gpointer	 data);
typedef void		(*GHookFunc)		(gpointer	 data);
typedef gboolean	(*GHookCheckFunc)	(gpointer	 data);
typedef void		(*GLogFunc)		(const gchar   *log_domain,
						 GLogLevelFlags	log_level,
						 const gchar   *message,
						 gpointer	user_data);
typedef gboolean	(*GNodeTraverseFunc)	(GNode	       *node,
						 gpointer	data);
typedef void		(*GNodeForeachFunc)	(GNode	       *node,
						 gpointer	data);
typedef gint		(*GSearchFunc)		(gpointer	key,
						 gpointer	data);
typedef void		(*GScannerMsgFunc)	(GScanner      *scanner,
						 gchar	       *message,
						 gint		error);
typedef gint		(*GTraverseFunc)	(gpointer	key,
						 gpointer	value,
						 gpointer	data);
typedef	void		(*GVoidFunc)		(void);


struct _GList
{
  gpointer data;
  GList *next;
  GList *prev;
};

struct _GSList
{
  gpointer data;
  GSList *next;
};

struct _GString
{
  gchar *str;
  gint len;
};

struct _GArray
{
  gchar *data;
  guint len;
};

struct _GByteArray
{
  guint8 *data;
  guint	  len;
};

struct _GPtrArray
{
  gpointer *pdata;
  guint	    len;
};

struct _GTuples
{
  guint len;
};

struct _GDebugKey
{
  gchar *key;
  guint	 value;
};


/* Doubly linked lists
 */
GList* g_list_alloc		(void);
void   g_list_free		(GList		*list);
void   g_list_free_1		(GList		*list);
GList* g_list_append		(GList		*list,
				 gpointer	 data);
GList* g_list_prepend		(GList		*list,
				 gpointer	 data);
GList* g_list_insert		(GList		*list,
				 gpointer	 data,
				 gint		 position);
GList* g_list_insert_sorted	(GList		*list,
				 gpointer	 data,
				 GCompareFunc	 func);
GList* g_list_concat		(GList		*list1,
				 GList		*list2);
GList* g_list_remove		(GList		*list,
				 gpointer	 data);
GList* g_list_remove_link	(GList		*list,
				 GList		*llink);
GList* g_list_reverse		(GList		*list);
GList* g_list_nth		(GList		*list,
				 guint		 n);
GList* g_list_find		(GList		*list,
				 gpointer	 data);
GList* g_list_find_custom	(GList		*list,
				 gpointer	 data,
				 GCompareFunc	 func);
gint   g_list_position		(GList		*list,
				 GList		*llink);
gint   g_list_index		(GList		*list,
				 gpointer	 data);
GList* g_list_last		(GList		*list);
GList* g_list_first		(GList		*list);
guint  g_list_length		(GList		*list);
void   g_list_foreach		(GList		*list,
				 GFunc		 func,
				 gpointer	 user_data);
gpointer g_list_nth_data	(GList		*list,
				 guint		 n);
#define g_list_previous(list)	((list) ? (((GList *)(list))->prev) : NULL)
#define g_list_next(list)	((list) ? (((GList *)(list))->next) : NULL)


/* Singly linked lists
 */
GSList* g_slist_alloc		(void);
void	g_slist_free		(GSList		*list);
void	g_slist_free_1		(GSList		*list);
GSList* g_slist_append		(GSList		*list,
				 gpointer	 data);
GSList* g_slist_prepend		(GSList		*list,
				 gpointer	 data);
GSList* g_slist_insert		(GSList		*list,
				 gpointer	 data,
				 gint		 position);
GSList* g_slist_insert_sorted	(GSList		*list,
				 gpointer	 data,
				 GCompareFunc	 func);
GSList* g_slist_concat		(GSList		*list1,
				 GSList		*list2);
GSList* g_slist_remove		(GSList		*list,
				 gpointer	 data);
GSList* g_slist_remove_link	(GSList		*list,
				 GSList		*llink);
GSList* g_slist_reverse		(GSList		*list);
GSList* g_slist_nth		(GSList		*list,
				 guint		 n);
GSList* g_slist_find		(GSList		*list,
				 gpointer	 data);
GSList* g_slist_find_custom	(GSList		*list,
				 gpointer	 data,
				 GCompareFunc	 func);
gint	g_slist_position	(GSList		*list,
				 GSList		*llink);
gint	g_slist_index		(GSList		*list,
				 gpointer	 data);
GSList* g_slist_last		(GSList		*list);
guint	g_slist_length		(GSList		*list);
void	g_slist_foreach		(GSList		*list,
				 GFunc		 func,
				 gpointer	 user_data);
gpointer g_slist_nth_data	(GSList		*list,
				 guint		 n);
#define g_slist_next(slist)	((slist) ? (((GSList *)(slist))->next) : NULL)


/* List Allocators
 */
GListAllocator* g_list_allocator_new  (void);
void		g_list_allocator_free (GListAllocator* allocator);
GListAllocator* g_slist_set_allocator (GListAllocator* allocator);
GListAllocator* g_list_set_allocator  (GListAllocator* allocator);


/* Hash tables
 */
GHashTable* g_hash_table_new		(GHashFunc	 hash_func,
					 GCompareFunc	 key_compare_func);
void	    g_hash_table_destroy	(GHashTable	*hash_table);
void	    g_hash_table_insert		(GHashTable	*hash_table,
					 gpointer	 key,
					 gpointer	 value);
void	    g_hash_table_remove		(GHashTable	*hash_table,
					 gconstpointer	 key);
gpointer    g_hash_table_lookup		(GHashTable	*hash_table,
					 gconstpointer	 key);
gboolean    g_hash_table_lookup_extended(GHashTable	*hash_table,
					 gconstpointer	 lookup_key,
					 gpointer	*orig_key,
					 gpointer	*value);
void	    g_hash_table_freeze		(GHashTable	*hash_table);
void	    g_hash_table_thaw		(GHashTable	*hash_table);
void	    g_hash_table_foreach	(GHashTable	*hash_table,
					 GHFunc		 func,
					 gpointer	 user_data);
gint	    g_hash_table_foreach_remove	(GHashTable	*hash_table,
					 GHRFunc	 func,
					 gpointer	 user_data);
gint	    g_hash_table_size		(GHashTable	*hash_table);


/* Caches
 */
GCache*	 g_cache_new	       (GCacheNewFunc	   value_new_func,
				GCacheDestroyFunc  value_destroy_func,
				GCacheDupFunc	   key_dup_func,
				GCacheDestroyFunc  key_destroy_func,
				GHashFunc	   hash_key_func,
				GHashFunc	   hash_value_func,
				GCompareFunc	   key_compare_func);
void	 g_cache_destroy       (GCache		  *cache);
gpointer g_cache_insert	       (GCache		  *cache,
				gpointer	   key);
void	 g_cache_remove	       (GCache		  *cache,
				gpointer	   value);
void	 g_cache_key_foreach   (GCache		  *cache,
				GHFunc		   func,
				gpointer	   user_data);
void	 g_cache_value_foreach (GCache		  *cache,
				GHFunc		   func,
				gpointer	   user_data);


/* Balanced binary trees
 */
GTree*	 g_tree_new	 (GCompareFunc	 key_compare_func);
void	 g_tree_destroy	 (GTree		*tree);
void	 g_tree_insert	 (GTree		*tree,
			  gpointer	 key,
			  gpointer	 value);
void	 g_tree_remove	 (GTree		*tree,
			  gpointer	 key);
gpointer g_tree_lookup	 (GTree		*tree,
			  gpointer	 key);
void	 g_tree_traverse (GTree		*tree,
			  GTraverseFunc	 traverse_func,
			  GTraverseType	 traverse_type,
			  gpointer	 data);
gpointer g_tree_search	 (GTree		*tree,
			  GSearchFunc	 search_func,
			  gpointer	 data);
gint	 g_tree_height	 (GTree		*tree);
gint	 g_tree_nnodes	 (GTree		*tree);



/* N-way tree implementation
 */
struct _GNode
{
  gpointer data;
  GNode	  *next;
  GNode	  *prev;
  GNode	  *parent;
  GNode	  *children;
};

#define	 G_NODE_IS_ROOT(node)	(((GNode*) (node))->parent == NULL && \
				 ((GNode*) (node))->prev == NULL && \
				 ((GNode*) (node))->next == NULL)
#define	 G_NODE_IS_LEAF(node)	(((GNode*) (node))->children == NULL)

GNode*	 g_node_new		(gpointer	   data);
void	 g_node_destroy		(GNode		  *root);
void	 g_node_unlink		(GNode		  *node);
GNode*	 g_node_insert		(GNode		  *parent,
				 gint		   position,
				 GNode		  *node);
GNode*	 g_node_insert_before	(GNode		  *parent,
				 GNode		  *sibling,
				 GNode		  *node);
GNode*	 g_node_prepend		(GNode		  *parent,
				 GNode		  *node);
guint	 g_node_n_nodes		(GNode		  *root,
				 GTraverseFlags	   flags);
GNode*	 g_node_get_root	(GNode		  *node);
gboolean g_node_is_ancestor	(GNode		  *node,
				 GNode		  *descendant);
guint	 g_node_depth		(GNode		  *node);
GNode*	 g_node_find		(GNode		  *root,
				 GTraverseType	   order,
				 GTraverseFlags	   flags,
				 gpointer	   data);

/* convenience macros */
#define g_node_append(parent, node)				\
     g_node_insert_before ((parent), NULL, (node))
#define	g_node_insert_data(parent, position, data)		\
     g_node_insert ((parent), (position), g_node_new (data))
#define	g_node_insert_data_before(parent, sibling, data)	\
     g_node_insert_before ((parent), (sibling), g_node_new (data))
#define	g_node_prepend_data(parent, data)			\
     g_node_prepend ((parent), g_node_new (data))
#define	g_node_append_data(parent, data)			\
     g_node_insert_before ((parent), NULL, g_node_new (data))

/* traversal function, assumes that `node' is root
 * (only traverses `node' and its subtree).
 * this function is just a high level interface to
 * low level traversal functions, optimized for speed.
 */
void	 g_node_traverse	(GNode		  *root,
				 GTraverseType	   order,
				 GTraverseFlags	   flags,
				 gint		   max_depth,
				 GNodeTraverseFunc func,
				 gpointer	   data);

/* return the maximum tree height starting with `node', this is an expensive
 * operation, since we need to visit all nodes. this could be shortened by
 * adding `guint height' to struct _GNode, but then again, this is not very
 * often needed, and would make g_node_insert() more time consuming.
 */
guint	 g_node_max_height	 (GNode *root);

void	 g_node_children_foreach (GNode		  *node,
				  GTraverseFlags   flags,
				  GNodeForeachFunc func,
				  gpointer	   data);
void	 g_node_reverse_children (GNode		  *node);
guint	 g_node_n_children	 (GNode		  *node);
GNode*	 g_node_nth_child	 (GNode		  *node,
				  guint		   n);
GNode*	 g_node_last_child	 (GNode		  *node);
GNode*	 g_node_find_child	 (GNode		  *node,
				  GTraverseFlags   flags,
				  gpointer	   data);
gint	 g_node_child_position	 (GNode		  *node,
				  GNode		  *child);
gint	 g_node_child_index	 (GNode		  *node,
				  gpointer	   data);

GNode*	 g_node_first_sibling	 (GNode		  *node);
GNode*	 g_node_last_sibling	 (GNode		  *node);

#define	 g_node_prev_sibling(node)	((node) ? \
					 ((GNode*) (node))->prev : NULL)
#define	 g_node_next_sibling(node)	((node) ? \
					 ((GNode*) (node))->next : NULL)
#define	 g_node_first_child(node)	((node) ? \
					 ((GNode*) (node))->children : NULL)


/* Callback maintenance functions
 */
#define G_HOOK_FLAG_USER_SHIFT	(4)
typedef enum
{
  G_HOOK_FLAG_ACTIVE	= 1 << 0,
  G_HOOK_FLAG_IN_CALL	= 1 << 1,
  G_HOOK_FLAG_MASK	= 0x0f
} GHookFlagMask;

struct _GHookList
{
  guint		 seq_id;
  guint		 hook_size;
  guint		 is_setup : 1;
  GHook		*hooks;
  GMemChunk	*hook_memchunk;
};

struct _GHook
{
  gpointer	 data;
  GHook		*next;
  GHook		*prev;
  guint		 ref_count;
  guint		 hook_id;
  guint		 flags;
  gpointer	 func;
  GDestroyNotify destroy;
};

#define	G_HOOK_ACTIVE(hook)		((((GHook*) hook)->flags & \
					  G_HOOK_FLAG_ACTIVE) != 0)
#define	G_HOOK_IN_CALL(hook)		((((GHook*) hook)->flags & \
					  G_HOOK_FLAG_IN_CALL) != 0)
#define G_HOOK_IS_VALID(hook)		(((GHook*) hook)->hook_id != 0 && \
					 G_HOOK_ACTIVE (hook))
#define G_HOOK_IS_UNLINKED(hook)	(((GHook*) hook)->next == NULL && \
					 ((GHook*) hook)->prev == NULL && \
					 ((GHook*) hook)->hook_id == 0 && \
					 ((GHook*) hook)->ref_count == 0)

void	 g_hook_list_init		(GHookList		*hook_list,
					 guint			 hook_size);
void	 g_hook_list_clear		(GHookList		*hook_list);
GHook*	 g_hook_alloc			(GHookList		*hook_list);
void	 g_hook_free			(GHookList		*hook_list,
					 GHook			*hook);
void	 g_hook_ref			(GHookList		*hook_list,
					 GHook			*hook);
void	 g_hook_unref			(GHookList		*hook_list,
					 GHook			*hook);
gboolean g_hook_destroy			(GHookList		*hook_list,
					 guint			 hook_id);
void	 g_hook_destroy_link		(GHookList		*hook_list,
					 GHook			*hook);
void	 g_hook_prepend			(GHookList		*hook_list,
					 GHook			*hook);
void	 g_hook_insert_before		(GHookList		*hook_list,
					 GHook			*sibling,
					 GHook			*hook);
void	 g_hook_insert_sorted		(GHookList		*hook_list,
					 GHook			*hook,
					 GHookCompareFunc	 func);
GHook*	 g_hook_get			(GHookList		*hook_list,
					 guint			 hook_id);
GHook*	 g_hook_find			(GHookList		*hook_list,
					 gboolean		 need_valids,
					 GHookFindFunc		 func,
					 gpointer		 data);
GHook*	 g_hook_find_data		(GHookList		*hook_list,
					 gboolean		 need_valids,
					 gpointer		 data);
GHook*	 g_hook_find_func		(GHookList		*hook_list,
					 gboolean		 need_valids,
					 gpointer		 func);
GHook*	 g_hook_find_func_data		(GHookList		*hook_list,
					 gboolean		 need_valids,
					 gpointer		 func,
					 gpointer		 data);
GHook*	 g_hook_first_valid		(GHookList		*hook_list,
					 gboolean		 may_be_in_call);
GHook*	 g_hook_next_valid		(GHook			*hook,
					 gboolean		 may_be_in_call);

/* GHookCompareFunc implementation to insert hooks sorted by their id */
gint	 g_hook_compare_ids		(GHook			*new_hook,
					 GHook			*sibling);

/* convenience macros */
#define	 g_hook_append( hook_list, hook )  \
     g_hook_insert_before ((hook_list), NULL, (hook))

/* invoke all valid hooks with the (*GHookFunc) signature.
 */
void	 g_hook_list_invoke		(GHookList		*hook_list,
					 gboolean		 may_recurse);
/* invoke all valid hooks with the (*GHookCheckFunc) signature,
 * and destroy the hook if FALSE is returned.
 */
void	 g_hook_list_invoke_check	(GHookList		*hook_list,
					 gboolean		 may_recurse);
/* invoke a marshaller on all valid hooks.
 */
void	 g_hook_list_marshal		(GHookList		*hook_list,
					 gboolean		 may_recurse,
					 GHookMarshaller	 marshaller,
					 gpointer		 data);


/* Fatal error handlers.
 * g_on_error_query() will prompt the user to either
 * [E]xit, [H]alt, [P]roceed or show [S]tack trace.
 * g_on_error_stack_trace() invokes gdb, which attaches to the current
 * process and shows a stack trace.
 * These function may cause different actions on non-unix platforms.
 * The prg_name arg is required by gdb to find the executable, if it is
 * passed as NULL, g_on_error_query() will try g_get_prgname().
 */
void g_on_error_query (const gchar *prg_name);
void g_on_error_stack_trace (const gchar *prg_name);


/* Logging mechanism
 */
extern	        const gchar		*g_log_domain_glib;
guint		g_log_set_handler	(const gchar	*log_domain,
					 GLogLevelFlags	 log_levels,
					 GLogFunc	 log_func,
					 gpointer	 user_data);
void		g_log_remove_handler	(const gchar	*log_domain,
					 guint		 handler_id);
void		g_log_default_handler	(const gchar	*log_domain,
					 GLogLevelFlags	 log_level,
					 const gchar	*message,
					 gpointer	 unused_data);
void		g_log			(const gchar	*log_domain,
					 GLogLevelFlags	 log_level,
					 const gchar	*format,
					 ...) G_GNUC_PRINTF (3, 4);
void		g_logv			(const gchar	*log_domain,
					 GLogLevelFlags	 log_level,
					 const gchar	*format,
					 va_list	 args);
GLogLevelFlags	g_log_set_fatal_mask	(const gchar	*log_domain,
					 GLogLevelFlags	 fatal_mask);
GLogLevelFlags	g_log_set_always_fatal	(GLogLevelFlags	 fatal_mask);
#ifndef	G_LOG_DOMAIN
#define	G_LOG_DOMAIN	(NULL)
#endif	/* G_LOG_DOMAIN */
#ifdef	__GNUC__
#define	g_error(format, args...)	g_log (G_LOG_DOMAIN, \
					       G_LOG_LEVEL_ERROR, \
					       format, ##args)
#define	g_message(format, args...)	g_log (G_LOG_DOMAIN, \
					       G_LOG_LEVEL_MESSAGE, \
					       format, ##args)
#define	g_warning(format, args...)	g_log (G_LOG_DOMAIN, \
					       G_LOG_LEVEL_WARNING, \
					       format, ##args)
#else	/* !__GNUC__ */
static inline void
g_error (const gchar *format,
	 ...)
{
  va_list args;
  va_start (args, format);
  g_logv (G_LOG_DOMAIN, G_LOG_LEVEL_ERROR, format, args);
  va_end (args);
}
static inline void
g_message (const gchar *format,
	   ...)
{
  va_list args;
  va_start (args, format);
  g_logv (G_LOG_DOMAIN, G_LOG_LEVEL_MESSAGE, format, args);
  va_end (args);
}
static inline void
g_warning (const gchar *format,
	   ...)
{
  va_list args;
  va_start (args, format);
  g_logv (G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, format, args);
  va_end (args);
}
#endif	/* !__GNUC__ */

typedef void	(*GPrintFunc)		(const gchar	*string);
void		g_print			(const gchar	*format,
					 ...) G_GNUC_PRINTF (1, 2);
GPrintFunc	g_set_print_handler	(GPrintFunc	 func);
void		g_printerr		(const gchar	*format,
					 ...) G_GNUC_PRINTF (1, 2);
GPrintFunc	g_set_printerr_handler	(GPrintFunc	 func);

/* deprecated compatibility functions, use g_log_set_handler() instead */
typedef void		(*GErrorFunc)		(const gchar *str);
typedef void		(*GWarningFunc)		(const gchar *str);
GErrorFunc   g_set_error_handler   (GErrorFunc	 func);
GWarningFunc g_set_warning_handler (GWarningFunc func);
GPrintFunc   g_set_message_handler (GPrintFunc func);


/* Memory allocation and debugging
 */
#ifdef USE_DMALLOC

#define g_malloc(size)	     ((gpointer) MALLOC (size))
#define g_malloc0(size)	     ((gpointer) CALLOC (char, size))
#define g_realloc(mem,size)  ((gpointer) REALLOC (mem, char, size))
#define g_free(mem)	     FREE (mem)

#else /* !USE_DMALLOC */

gpointer g_malloc      (gulong	  size);
gpointer g_malloc0     (gulong	  size);
gpointer g_realloc     (gpointer  mem,
			gulong	  size);
void	 g_free	       (gpointer  mem);

#endif /* !USE_DMALLOC */

void	 g_mem_profile (void);
void	 g_mem_check   (gpointer  mem);


/* "g_mem_chunk_new" creates a new memory chunk.
 * Memory chunks are used to allocate pieces of memory which are
 *  always the same size. Lists are a good example of such a data type.
 * The memory chunk allocates and frees blocks of memory as needed.
 *  Just be sure to call "g_mem_chunk_free" and not "g_free" on data
 *  allocated in a mem chunk. ("g_free" will most likely cause a seg
 *  fault...somewhere).
 *
 * Oh yeah, GMemChunk is an opaque data type. (You don't really
 *  want to know what's going on inside do you?)
 */

/* ALLOC_ONLY MemChunk's can only allocate memory. The free operation
 *  is interpreted as a no op. ALLOC_ONLY MemChunk's save 4 bytes per
 *  atom. (They are also useful for lists which use MemChunk to allocate
 *  memory but are also part of the MemChunk implementation).
 * ALLOC_AND_FREE MemChunk's can allocate and free memory.
 */

#define G_ALLOC_ONLY	  1
#define G_ALLOC_AND_FREE  2

GMemChunk* g_mem_chunk_new     (gchar	  *name,
				gint	   atom_size,
				gulong	   area_size,
				gint	   type);
void	   g_mem_chunk_destroy (GMemChunk *mem_chunk);
gpointer   g_mem_chunk_alloc   (GMemChunk *mem_chunk);
gpointer   g_mem_chunk_alloc0  (GMemChunk *mem_chunk);
void	   g_mem_chunk_free    (GMemChunk *mem_chunk,
				gpointer   mem);
void	   g_mem_chunk_clean   (GMemChunk *mem_chunk);
void	   g_mem_chunk_reset   (GMemChunk *mem_chunk);
void	   g_mem_chunk_print   (GMemChunk *mem_chunk);
void	   g_mem_chunk_info    (void);

/* Ah yes...we have a "g_blow_chunks" function.
 * "g_blow_chunks" simply compresses all the chunks. This operation
 *  consists of freeing every memory area that should be freed (but
 *  which we haven't gotten around to doing yet). And, no,
 *  "g_blow_chunks" doesn't follow the naming scheme, but it is a
 *  much better name than "g_mem_chunk_clean_all" or something
 *  similar.
 */
void g_blow_chunks (void);


/* Timer
 */
GTimer* g_timer_new	(void);
void	g_timer_destroy (GTimer	 *timer);
void	g_timer_start	(GTimer	 *timer);
void	g_timer_stop	(GTimer	 *timer);
void	g_timer_reset	(GTimer	 *timer);
gdouble g_timer_elapsed (GTimer	 *timer,
			 gulong	 *microseconds);


/* String utility functions that modify a string argument or
 * return a constant string that must not be freed.
 */
#define	 G_STR_DELIMITERS	"_-|> <."
gchar*	 g_strdelimit		(gchar	     *string,
				 const gchar *delimiters,
				 gchar	      new_delimiter);
gdouble	 g_strtod		(const gchar *nptr,
				 gchar	    **endptr);
gchar*	 g_strerror		(gint	      errnum);
gchar*	 g_strsignal		(gint	      signum);
gint	 g_strcasecmp		(const gchar *s1,
				 const gchar *s2);
void	 g_strdown		(gchar	     *string);
void	 g_strup		(gchar	     *string);
void	 g_strreverse		(gchar	     *string);
/* removes leading spaces */
gchar*   g_strchug              (gchar        *string);
/* removes trailing spaces */
gchar*  g_strchomp              (gchar        *string);
/* removes leading & trailing spaces */
#define g_strstrip( string )	g_strchomp (g_strchug (string))

/* String utility functions that return a newly allocated string which
 * ought to be freed from the caller at some point.
 */
gchar*	 g_strdup		(const gchar *str);
gchar*	 g_strdup_printf	(const gchar *format,
				 ...) G_GNUC_PRINTF (1, 2);
gchar*	 g_strdup_vprintf	(const gchar *format,
				 va_list      args);
gchar*	 g_strndup		(const gchar *str,
				 guint	      n);
gchar*	 g_strnfill		(guint	      length,
				 gchar	      fill_char);
gchar*	 g_strconcat		(const gchar *string1,
				 ...); /* NULL terminated */
gchar*   g_strjoin		(const gchar  *separator,
				 ...); /* NULL terminated */
gchar*	 g_strescape		(gchar	      *string);
gpointer g_memdup		(gconstpointer mem,
				 guint	       byte_size);

/* NULL terminated string arrays.
 * g_strsplit() splits up string into max_tokens tokens at delim and
 * returns a newly allocated string array.
 * g_strjoinv() concatenates all of str_array's strings, sliding in an
 * optional separator, the returned string is newly allocated.
 * g_strfreev() frees the array itself and all of its strings.
 */
gchar**	 g_strsplit		(const gchar  *string,
				 const gchar  *delimiter,
				 gint          max_tokens);
gchar*   g_strjoinv		(const gchar  *separator,
				 gchar       **str_array);
void     g_strfreev		(gchar       **str_array);



/* calculate a string size, guarranteed to fit format + args.
 */
guint	g_printf_string_upper_bound (const gchar* format,
				     va_list	  args);


/* Retrive static string info
 */
gchar*	g_get_user_name		(void);
gchar*	g_get_real_name		(void);
gchar*	g_get_home_dir		(void);
gchar*	g_get_tmp_dir		(void);
gchar*	g_get_prgname		(void);
void	g_set_prgname		(const gchar *prgname);


/* Miscellaneous utility functions
 */
guint	g_parse_debug_string	(const gchar *string,
				 GDebugKey   *keys,
				 guint	      nkeys);
gint	g_snprintf		(gchar	     *string,
				 gulong	      n,
				 gchar const *format,
				 ...) G_GNUC_PRINTF (3, 4);
gint	g_vsnprintf		(gchar	     *string,
				 gulong	      n,
				 gchar const *format,
				 va_list      args);
gchar*	g_basename		(const gchar *file_name);
/* Check if a file name is an absolute path */
gboolean g_path_is_absolute	(const gchar *file_name);
/* In case of absolute paths, skip the root part */
gchar*  g_path_skip_root	(gchar       *file_name);

/* strings are newly allocated with g_malloc() */
gchar*	g_dirname		(const gchar *file_name);
gchar*	g_get_current_dir	(void);
gchar*  g_getenv		(const gchar *variable);

/* We make the assumption that if memmove isn't available, then
 * bcopy will do the job. This isn't safe everywhere. (bcopy can't
 * necessarily handle overlapping copies).
 * Either way, g_memmove() will not return a value.
 */
#ifdef HAVE_MEMMOVE
#define g_memmove(dest, src, size)	G_STMT_START {	\
     memmove ((dest), (src), (size));			\
} G_STMT_END
#else
#define g_memmove(dest, src, size)	G_STMT_START {	\
     bcopy ((src), (dest), (size));			\
} G_STMT_END
#endif

/* we use a GLib function as a replacement for ATEXIT, so
 * the programmer is not required to check the return value
 * (if there is any in the implementation) and doesn't encounter
 * missing include files.
 */
void	g_atexit		(GVoidFunc    func);


/* Bit tests
 */
G_INLINE_FUNC gint	g_bit_nth_lsf (guint32 mask,
				       gint    nth_bit);
#ifdef	G_CAN_INLINE
G_INLINE_FUNC gint
g_bit_nth_lsf (guint32 mask,
	       gint    nth_bit)
{
  do
    {
      nth_bit++;
      if (mask & (1 << (guint) nth_bit))
	return nth_bit;
    }
  while (nth_bit < 32);
  return -1;
}
#endif	/* G_CAN_INLINE */

G_INLINE_FUNC gint	g_bit_nth_msf (guint32 mask,
				       gint    nth_bit);
#ifdef G_CAN_INLINE
G_INLINE_FUNC gint
g_bit_nth_msf (guint32 mask,
	       gint    nth_bit)
{
  if (nth_bit < 0)
    nth_bit = 33;
  do
    {
      nth_bit--;
      if (mask & (1 << (guint) nth_bit))
	return nth_bit;
    }
  while (nth_bit > 0);
  return -1;
}
#endif	/* G_CAN_INLINE */

G_INLINE_FUNC guint	g_bit_storage (guint number);
#ifdef G_CAN_INLINE
G_INLINE_FUNC guint
g_bit_storage (guint number)
{
  register guint n_bits = 0;
  
  do
    {
      n_bits++;
      number >>= 1;
    }
  while (number);
  return n_bits;
}
#endif	/* G_CAN_INLINE */

/* String Chunks
 */
GStringChunk* g_string_chunk_new	   (gint size);
void	      g_string_chunk_free	   (GStringChunk *chunk);
gchar*	      g_string_chunk_insert	   (GStringChunk *chunk,
					    const gchar	 *string);
gchar*	      g_string_chunk_insert_const  (GStringChunk *chunk,
					    const gchar	 *string);


/* Strings
 */
GString* g_string_new	    (const gchar *init);
GString* g_string_sized_new (guint	  dfl_size);
void	 g_string_free	    (GString	 *string,
			     gint	  free_segment);
GString* g_string_assign    (GString	 *lval,
			     const gchar *rval);
GString* g_string_truncate  (GString	 *string,
			     gint	  len);
GString* g_string_append    (GString	 *string,
			     const gchar *val);
GString* g_string_append_c  (GString	 *string,
			     gchar	  c);
GString* g_string_prepend   (GString	 *string,
			     const gchar *val);
GString* g_string_prepend_c (GString	 *string,
			     gchar	  c);
GString* g_string_insert    (GString	 *string,
			     gint	  pos,
			     const gchar *val);
GString* g_string_insert_c  (GString	 *string,
			     gint	  pos,
			     gchar	  c);
GString* g_string_erase	    (GString	 *string,
			     gint	  pos,
			     gint	  len);
GString* g_string_down	    (GString	 *string);
GString* g_string_up	    (GString	 *string);
void	 g_string_sprintf   (GString	 *string,
			     const gchar *format,
			     ...) G_GNUC_PRINTF (2, 3);
void	 g_string_sprintfa  (GString	 *string,
			     const gchar *format,
			     ...) G_GNUC_PRINTF (2, 3);


/* Resizable arrays
 */

#define g_array_append_val(a,v) g_array_append_vals(a,&v,1)
#define g_array_prepend_val(a,v) g_array_prepend_vals(a,&v,1)
#define g_array_index(a,t,i) (((t*)a->data)[i])

GArray* g_array_new	     (gboolean	    zero_terminated,
			      gboolean	    clear,
			      guint	    element_size);
void	g_array_free	     (GArray	   *array,
			      gboolean	    free_segment);
GArray* g_array_append_vals  (GArray	   *array,
			      gconstpointer data,
			      guint	    len);
GArray* g_array_prepend_vals (GArray	   *array,
			      gconstpointer data,
			      guint	    len);
GArray* g_array_set_size     (GArray	   *array,
			      guint	    length);

/* Resizable pointer array.  This interface is much less complicated
 * than the above.  Add appends appends a pointer.  Remove fills any
 * cleared spot and shortens the array.
 */
#define	    g_ptr_array_index(array,index) (array->pdata)[index]
GPtrArray*  g_ptr_array_new		   (void);
void	    g_ptr_array_free		   (GPtrArray	*array,
					    gboolean	 free_seg);
void	    g_ptr_array_set_size	   (GPtrArray	*array,
					    gint	 length);
gpointer    g_ptr_array_remove_index	   (GPtrArray	*array,
					    gint	 index);
gboolean    g_ptr_array_remove		   (GPtrArray	*array,
					    gpointer	 data);
void	    g_ptr_array_add		   (GPtrArray	*array,
					    gpointer	 data);

/* Byte arrays, an array of guint8.  Implemented as a GArray,
 * but type-safe.
 */

GByteArray* g_byte_array_new	  (void);
void	    g_byte_array_free	  (GByteArray	*array,
				   gboolean	 free_segment);
GByteArray* g_byte_array_append	  (GByteArray	*array,
				   const guint8 *data,
				   guint	 len);
GByteArray* g_byte_array_prepend  (GByteArray	*array,
				   const guint8 *data,
				   guint	 len);
GByteArray* g_byte_array_set_size (GByteArray	*array,
				   guint	 length);


/* Hash Functions
 */
gint  g_str_equal (gconstpointer   v,
		   gconstpointer   v2);
guint g_str_hash  (gconstpointer   v);

gint  g_int_equal (gconstpointer   v,
		   gconstpointer   v2);
guint g_int_hash  (gconstpointer   v);

/* This "hash" function will just return the key's adress as an
 * unsigned integer. Useful for hashing on plain adresses or
 * simple integer values.
 */
guint g_direct_hash  (gconstpointer v);
gint  g_direct_equal (gconstpointer v,
		      gconstpointer v2);


/* Quarks (string<->id association)
 */
GQuark	  g_quark_try_string		(const gchar	*string);
GQuark	  g_quark_from_static_string	(const gchar	*string);
GQuark	  g_quark_from_string		(const gchar	*string);
gchar*	  g_quark_to_string		(GQuark		 quark);


/* Keyed Data List
 */
void	  g_datalist_init		 (GData		 **datalist);
void	  g_datalist_clear		 (GData		 **datalist);
gpointer  g_datalist_id_get_data	 (GData		 **datalist,
					  GQuark	   key_id);
void	  g_datalist_id_set_data_full	 (GData		 **datalist,
					  GQuark	   key_id,
					  gpointer	   data,
					  GDestroyNotify   destroy_func);
void	  g_datalist_id_remove_no_notify (GData		 **datalist,
					  GQuark	   key_id);
void	  g_datalist_foreach		 (GData		 **datalist,
					  GDataForeachFunc func,
					  gpointer	   user_data);
#define	  g_datalist_id_set_data(dl, q, d)	\
     g_datalist_id_set_data_full ((dl), (q), (d), NULL)
#define	  g_datalist_id_remove_data(dl, q)	\
     g_datalist_id_set_data ((dl), (q), NULL)
#define	  g_datalist_get_data(dl, k)		\
     (g_datalist_id_get_data ((dl), g_quark_try_string (k)))
#define	  g_datalist_set_data_full(dl, k, d, f)	\
     g_datalist_id_set_data_full ((dl), g_quark_from_string (k), (d), (f))
#define	  g_datalist_remove_no_notify(dl, k)	\
     g_datalist_id_remove_no_notify ((dl), g_quark_try_string (k))
#define	  g_datalist_set_data(dl, k, d)		\
     g_datalist_set_data_full ((dl), (k), (d), NULL)
#define	  g_datalist_remove_data(dl, k)		\
     g_datalist_id_set_data ((dl), g_quark_try_string (k), NULL)


/* Location Associated Keyed Data
 */
void	  g_dataset_destroy		(gconstpointer	  dataset_location);
gpointer  g_dataset_id_get_data		(gconstpointer	  dataset_location,
					 GQuark		  key_id);
void	  g_dataset_id_set_data_full	(gconstpointer	  dataset_location,
					 GQuark		  key_id,
					 gpointer	  data,
					 GDestroyNotify	  destroy_func);
void	  g_dataset_id_remove_no_notify	(gconstpointer	  dataset_location,
					 GQuark		  key_id);
void	  g_dataset_foreach		(gconstpointer	  dataset_location,
					 GDataForeachFunc func,
					 gpointer	  user_data);
#define	  g_dataset_id_set_data(l, k, d)	\
     g_dataset_id_set_data_full ((l), (k), (d), NULL)
#define	  g_dataset_id_remove_data(l, k)	\
     g_dataset_id_set_data ((l), (k), NULL)
#define	  g_dataset_get_data(l, k)		\
     (g_dataset_id_get_data ((l), g_quark_try_string (k)))
#define	  g_dataset_set_data_full(l, k, d, f)	\
     g_dataset_id_set_data_full ((l), g_quark_from_string (k), (d), (f))
#define	  g_dataset_remove_no_notify(l, k)	\
     g_dataset_id_remove_no_notify ((l), g_quark_try_string (k))
#define	  g_dataset_set_data(l, k, d)		\
     g_dataset_set_data_full ((l), (k), (d), NULL)
#define	  g_dataset_remove_data(l, k)		\
     g_dataset_id_set_data ((l), g_quark_try_string (k), NULL)


/* GScanner: Flexible lexical scanner for general purpose.
 */

/* Character sets */
#define G_CSET_A_2_Z	"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
#define G_CSET_a_2_z	"abcdefghijklmnopqrstuvwxyz"
#define G_CSET_LATINC	"\300\301\302\303\304\305\306"\
			"\307\310\311\312\313\314\315\316\317\320"\
			"\321\322\323\324\325\326"\
			"\330\331\332\333\334\335\336"
#define G_CSET_LATINS	"\337\340\341\342\343\344\345\346"\
			"\347\350\351\352\353\354\355\356\357\360"\
			"\361\362\363\364\365\366"\
			"\370\371\372\373\374\375\376\377"

/* Error types */
typedef enum
{
  G_ERR_UNKNOWN,
  G_ERR_UNEXP_EOF,
  G_ERR_UNEXP_EOF_IN_STRING,
  G_ERR_UNEXP_EOF_IN_COMMENT,
  G_ERR_NON_DIGIT_IN_CONST,
  G_ERR_DIGIT_RADIX,
  G_ERR_FLOAT_RADIX,
  G_ERR_FLOAT_MALFORMED
} GErrorType;

/* Token types */
typedef enum
{
  G_TOKEN_EOF			=   0,
  
  G_TOKEN_LEFT_PAREN		= '(',
  G_TOKEN_RIGHT_PAREN		= ')',
  G_TOKEN_LEFT_CURLY		= '{',
  G_TOKEN_RIGHT_CURLY		= '}',
  G_TOKEN_LEFT_BRACE		= '[',
  G_TOKEN_RIGHT_BRACE		= ']',
  G_TOKEN_EQUAL_SIGN		= '=',
  G_TOKEN_COMMA			= ',',
  
  G_TOKEN_NONE			= 256,
  
  G_TOKEN_ERROR,
  
  G_TOKEN_CHAR,
  G_TOKEN_BINARY,
  G_TOKEN_OCTAL,
  G_TOKEN_INT,
  G_TOKEN_HEX,
  G_TOKEN_FLOAT,
  G_TOKEN_STRING,
  
  G_TOKEN_SYMBOL,
  G_TOKEN_IDENTIFIER,
  G_TOKEN_IDENTIFIER_NULL,
  
  G_TOKEN_COMMENT_SINGLE,
  G_TOKEN_COMMENT_MULTI,
  G_TOKEN_LAST
} GTokenType;

union	_GTokenValue
{
  gpointer	v_symbol;
  gchar		*v_identifier;
  gulong	v_binary;
  gulong	v_octal;
  gulong	v_int;
  gdouble	v_float;
  gulong	v_hex;
  gchar		*v_string;
  gchar		*v_comment;
  guchar	v_char;
  guint		v_error;
};

struct	_GScannerConfig
{
  /* Character sets
   */
  gchar		*cset_skip_characters;		/* default: " \t\n" */
  gchar		*cset_identifier_first;
  gchar		*cset_identifier_nth;
  gchar		*cpair_comment_single;		/* default: "#\n" */
  
  /* Should symbol lookup work case sensitive?
   */
  guint		case_sensitive : 1;
  
  /* Boolean values to be adjusted "on the fly"
   * to configure scanning behaviour.
   */
  guint		skip_comment_multi : 1;		/* C like comment */
  guint		skip_comment_single : 1;	/* single line comment */
  guint		scan_comment_multi : 1;		/* scan multi line comments? */
  guint		scan_identifier : 1;
  guint		scan_identifier_1char : 1;
  guint		scan_identifier_NULL : 1;
  guint		scan_symbols : 1;
  guint		scan_binary : 1;
  guint		scan_octal : 1;
  guint		scan_float : 1;
  guint		scan_hex : 1;			/* `0x0ff0' */
  guint		scan_hex_dollar : 1;		/* `$0ff0' */
  guint		scan_string_sq : 1;		/* string: 'anything' */
  guint		scan_string_dq : 1;		/* string: "\\-escapes!\n" */
  guint		numbers_2_int : 1;		/* bin, octal, hex => int */
  guint		int_2_float : 1;		/* int => G_TOKEN_FLOAT? */
  guint		identifier_2_string : 1;
  guint		char_2_token : 1;		/* return G_TOKEN_CHAR? */
  guint		symbol_2_token : 1;
  guint		scope_0_fallback : 1;		/* try scope 0 on lookups? */
};

struct	_GScanner
{
  /* unused fields */
  gpointer		user_data;
  guint			max_parse_errors;
  
  /* g_scanner_error() increments this field */
  guint			parse_errors;
  
  /* name of input stream, featured by the default message handler */
  const gchar		*input_name;
  
  /* data pointer for derived structures */
  gpointer		derived_data;
  
  /* link into the scanner configuration */
  GScannerConfig	*config;
  
  /* fields filled in after g_scanner_get_next_token() */
  GTokenType		token;
  GTokenValue		value;
  guint			line;
  guint			position;
  
  /* fields filled in after g_scanner_peek_next_token() */
  GTokenType		next_token;
  GTokenValue		next_value;
  guint			next_line;
  guint			next_position;
  
  /* to be considered private */
  GHashTable		*symbol_table;
  gint			input_fd;
  const gchar		*text;
  const gchar		*text_end;
  gchar			*buffer;
  guint			scope_id;
  
  /* handler function for _warn and _error */
  GScannerMsgFunc	msg_handler;
};

GScanner*	g_scanner_new			(GScannerConfig *config_templ);
void		g_scanner_destroy		(GScanner	*scanner);
void		g_scanner_input_file		(GScanner	*scanner,
						 gint		input_fd);
void		g_scanner_input_text		(GScanner	*scanner,
						 const	gchar	*text,
						 guint		text_len);
GTokenType	g_scanner_get_next_token	(GScanner	*scanner);
GTokenType	g_scanner_peek_next_token	(GScanner	*scanner);
GTokenType	g_scanner_cur_token		(GScanner	*scanner);
GTokenValue	g_scanner_cur_value		(GScanner	*scanner);
guint		g_scanner_cur_line		(GScanner	*scanner);
guint		g_scanner_cur_position		(GScanner	*scanner);
gboolean	g_scanner_eof			(GScanner	*scanner);
guint		g_scanner_set_scope		(GScanner	*scanner,
						 guint		 scope_id);
void		g_scanner_scope_add_symbol	(GScanner	*scanner,
						 guint		 scope_id,
						 const gchar	*symbol,
						 gpointer	value);
void		g_scanner_scope_remove_symbol	(GScanner	*scanner,
						 guint		 scope_id,
						 const gchar	*symbol);
gpointer	g_scanner_scope_lookup_symbol	(GScanner	*scanner,
						 guint		 scope_id,
						 const gchar	*symbol);
void		g_scanner_scope_foreach_symbol	(GScanner	*scanner,
						 guint		 scope_id,
						 GHFunc		 func,
						 gpointer	 func_data);
gpointer	g_scanner_lookup_symbol		(GScanner	*scanner,
						 const gchar	*symbol);
void		g_scanner_freeze_symbol_table	(GScanner	*scanner);
void		g_scanner_thaw_symbol_table	(GScanner	*scanner);
void		g_scanner_unexp_token		(GScanner	*scanner,
						 GTokenType	expected_token,
						 const gchar	*identifier_spec,
						 const gchar	*symbol_spec,
						 const gchar	*symbol_name,
						 const gchar	*message,
						 gint		 is_error);
void		g_scanner_error			(GScanner	*scanner,
						 const gchar	*format,
						 ...) G_GNUC_PRINTF (2,3);
void		g_scanner_warn			(GScanner	*scanner,
						 const gchar	*format,
						 ...) G_GNUC_PRINTF (2,3);
gint		g_scanner_stat_mode		(const gchar	*filename);
/* keep downward source compatibility */
#define		g_scanner_add_symbol( scanner, symbol, value )	G_STMT_START { \
  g_scanner_scope_add_symbol ((scanner), 0, (symbol), (value)); \
} G_STMT_END
#define		g_scanner_remove_symbol( scanner, symbol )	G_STMT_START { \
  g_scanner_scope_remove_symbol ((scanner), 0, (symbol)); \
} G_STMT_END
#define		g_scanner_foreach_symbol( scanner, func, data )	G_STMT_START { \
  g_scanner_scope_foreach_symbol ((scanner), 0, (func), (data)); \
} G_STMT_END


/* Completion */

struct _GCompletion
{
  GList* items;
  GCompletionFunc func;
  
  gchar* prefix;
  GList* cache;
};

GCompletion* g_completion_new	       (GCompletionFunc func);
void	     g_completion_add_items    (GCompletion*	cmp,
					GList*		items);
void	     g_completion_remove_items (GCompletion*	cmp,
					GList*		items);
void	     g_completion_clear_items  (GCompletion*	cmp);
GList*	     g_completion_complete     (GCompletion*	cmp,
					gchar*		prefix,
					gchar**		new_prefix);
void	     g_completion_free	       (GCompletion*	cmp);


/* GRelation: Indexed Relations.  Imagine a really simple table in a
 * database.  Relations are not ordered.  This data type is meant for
 * maintaining a N-way mapping.
 *
 * g_relation_new() creates a relation with FIELDS fields
 *
 * g_relation_destroy() frees all resources
 * g_tuples_destroy() frees the result of g_relation_select()
 *
 * g_relation_index() indexes relation FIELD with the provided
 *   equality and hash functions.  this must be done before any
 *   calls to insert are made.
 *
 * g_relation_insert() inserts a new tuple.  you are expected to
 *   provide the right number of fields.
 *
 * g_relation_delete() deletes all relations with KEY in FIELD
 * g_relation_select() returns ...
 * g_relation_count() counts ...
 */

GRelation* g_relation_new     (gint	    fields);
void	   g_relation_destroy (GRelation   *relation);
void	   g_relation_index   (GRelation   *relation,
			       gint	    field,
			       GHashFunc    hash_func,
			       GCompareFunc key_compare_func);
void	   g_relation_insert  (GRelation   *relation,
			       ...);
gint	   g_relation_delete  (GRelation   *relation,
			       gconstpointer  key,
			       gint	    field);
GTuples*   g_relation_select  (GRelation   *relation,
			       gconstpointer  key,
			       gint	    field);
gint	   g_relation_count   (GRelation   *relation,
			       gconstpointer  key,
			       gint	    field);
gboolean   g_relation_exists  (GRelation   *relation,
			       ...);
void	   g_relation_print   (GRelation   *relation);

void	   g_tuples_destroy   (GTuples	   *tuples);
gpointer   g_tuples_index     (GTuples	   *tuples,
			       gint	    index,
			       gint	    field);


/* Prime numbers.
 */

/* This function returns prime numbers spaced by approximately 1.5-2.0
 * and is for use in resizing data structures which prefer
 * prime-valued sizes.	The closest spaced prime function returns the
 * next largest prime, or the highest it knows about which is about
 * MAXINT/4.
 */
guint	   g_spaced_primes_closest (guint num);


/* IO Channels.
 * These are used for plug-in communication in the GIMP, for instance.
 * On Unix, it's simply an encapsulated file descriptor (a pipe).
 * On Windows, it's a handle to an anonymouos pipe, *and* (in the case
 * of the writable end) a thread id to post a message to when you have written
 * stuff.
 */
struct _GIOChannel
{
  gint fd;			/* file handle (pseudo such in Win32) */
#ifdef NATIVE_WIN32
  guint peer;			/* thread to post message to */
  guint peer_fd;		/* read handle (in the other process) */
  guint offset;			/* counter of accumulated bytes, to
				 * be included in the message posted
				 * so we keep in sync.
				 */
  guint peer_offset;		/* in input channels where the writer's
				 * offset is, so we don't try to read too much
				 */
#endif
};

GIOChannel *g_iochannel_new            (gint        fd);
void        g_iochannel_free           (GIOChannel *channel);
void        g_iochannel_close_and_free (GIOChannel *channel);
void        g_iochannel_wakeup_peer    (GIOChannel *channel);
#ifndef NATIVE_WIN32
#  define   g_iochannel_wakeup_peer(channel) G_STMT_START { } G_STMT_END
#endif


/* Windows emulation stubs for common unix functions
 */
#ifdef NATIVE_WIN32

#define MAXPATHLEN 1024

#ifdef _MSC_VER
typedef int pid_t;

/* These POSIXish functions are available in the Microsoft C library
 * prefixed with underscore (which of course technically speaking is
 * the Right Thing, as they are non-ANSI. Not that being non-ANSI
 * prevents Microsoft from practically requiring you to include
 * <windows.h> every now and then...).
 *
 * You still need to include the appropriate headers to get the
 * prototypes, <io.h> or <direct.h>.
 *
 * For some functions, we provide emulators in glib, which are prefixed
 * with gwin_.
 */
#define getcwd			_getcwd
#define getpid			_getpid
#define access			_access
#define open			_open
#define read			_read
#define write			_write
#define lseek			_lseek
#define close			_close
#define pipe(phandles)		_pipe (phandles, 4096, _O_BINARY)
#define popen			_popen
#define pclose			_pclose
#define fdopen			_fdopen
#define ftruncate(fd, size)	gwin_ftruncate (fd, size)
#define opendir			gwin_opendir
#define readdir			gwin_readdir
#define rewinddir		gwin_rewinddir
#define closedir		gwin_closedir

#define NAME_MAX 255

struct DIR
{
  gchar    *dir_name;

  gboolean  just_opened;
  guint     find_file_handle;
  gpointer  find_file_data;
};
typedef struct DIR DIR;
struct dirent
{
  gchar  d_name[NAME_MAX + 1];
};

/* emulation functions */
extern int	gwin_ftruncate	(gint		 f,
				 guint		 size);
DIR*		gwin_opendir	(const gchar	*dirname);
struct dirent*	gwin_readdir  	(DIR		*dir);
void		gwin_rewinddir 	(DIR		*dir);
gint		gwin_closedir  	(DIR		*dir);

#endif /* _MSC_VER */

#define g_ntohl(x) \
  ((guint32)((((guint32)(x) & 0x000000ffU) << 24) | \
	     (((guint32)(x) & 0x0000ff00U) <<  8) | \
	     (((guint32)(x) & 0x00ff0000U) >>  8) | \
	     (((guint32)(x) & 0xff000000U) >> 24)))

#define g_htonl(x)	g_ntohl(x)

#define g_ntohs(x) \
  ((guint16)((((guint16)(x) & 0x00ff) << 8) | \
	     (((guint16)(x) & 0xff00) >> 8)))

#define g_htons(x)	g_ntohs(x)

#endif /* NATIVE_WIN32 */




#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __G_LIB_H__ */
