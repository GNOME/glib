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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#ifndef __G_LIB_H__
#define __G_LIB_H__

#include <glibconfig.h>

#ifdef USE_DMALLOC
#include "dmalloc.h"
#endif


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
 */

#ifdef HAVE_FLOAT_H

#include <float.h>

#define G_MINFLOAT   FLT_MIN
#define G_MAXFLOAT   FLT_MAX
#define G_MINDOUBLE  DBL_MIN
#define G_MAXDOUBLE  DBL_MAX

#elif HAVE_VALUES_H

#include <values.h>

#define G_MINFLOAT  MINFLOAT
#define G_MAXFLOAT  MAXFLOAT
#define G_MINDOUBLE MINDOUBLE
#define G_MAXDOUBLE MAXDOUBLE

#endif /* HAVE_VALUES_H */


#ifdef HAVE_LIMITS_H

#include <limits.h>

#define G_MINSHORT  SHRT_MIN
#define G_MAXSHORT  SHRT_MAX
#define G_MININT    INT_MIN
#define G_MAXINT    INT_MAX
#define G_MINLONG   LONG_MIN
#define G_MAXLONG   LONG_MAX

#elif HAVE_VALUES_H

#ifdef HAVE_FLOAT_H
#include <values.h>
#endif /* HAVE_FLOAT_H */

#define G_MINSHORT  MINSHORT
#define G_MAXSHORT  MAXSHORT
#define G_MININT    MININT
#define G_MAXINT    MAXINT
#define G_MINLONG   MINLONG
#define G_MAXLONG   MAXLONG

#endif /* HAVE_VALUES_H */


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


/* Provide simple enum value macro wrappers that ease automated enum value
 * stringification code.
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


/* Provide simple macro statement wrappers (adapted from Pearl):
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
#else   /* !__GNUC__ */
#define G_GNUC_PRINTF( format_idx, arg_idx )
#define G_GNUC_SCANF( format_idx, arg_idx )
#define G_GNUC_FORMAT( arg_idx )
#define G_GNUC_NORETURN
#define G_GNUC_CONST
#endif  /* !__GNUC__ */

/* Hacker macro to place breakpoints for x86 machines.
 * Actuall use is strongly deprecated of course ;)
 */
#if	defined (__i386__)
#define	G_BREAKPOINT()		G_STMT_START{ __asm__ ("int $03"); }G_STMT_END
#else	/* !__i386__ */
#define	G_BREAKPOINT()
#endif	/* __i386__ */

/* Wrap the __PRETTY_FUNCTION__ and __FUNCTION__ variables with macros,
 * so we can refer to them as strings unconditionally.
 */
#ifdef	__GNUC__
#define	G_GNUC_FUNCTION		(__FUNCTION__)
#define	G_GNUC_PRETTY_FUNCTION	(__PRETTY_FUNCTION__)
#else	/* !__GNUC__ */
#define	G_GNUC_FUNCTION		("")
#define	G_GNUC_PRETTY_FUNCTION	("")
#endif	/* !__GNUC__ */


#ifndef ATEXIT
#  ifdef HAVE_ATEXIT
#    define ATEXIT(proc)   (atexit (proc))
#  elif defined (HAVE_ON_EXIT)
#    define ATEXIT(proc)   (on_exit ((void (*)(int, void *))(proc), NULL))
#  endif    
#endif /* ATEXIT */


/* Provide macros for easily allocating memory. The macros
 *  will cast the allocated memory to the specified type
 *  in order to avoid compiler warnings. (Makes the code neater).
 */

#ifdef __DMALLOC_H__

#define g_new(type,count)	 ALLOC(type,count)
#define g_new0(type,count)	 CALLOC(type,count)

#else /* __DMALLOC_H__ */

#define g_new(type, count)	  \
    ((type *) g_malloc ((unsigned) sizeof (type) * (count)))
#define g_new0(type, count)	  \
    ((type *) g_malloc0 ((unsigned) sizeof (type) * (count)))
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
  (type *) memset (g_mem_chunk_alloc (chunk), 0, sizeof (type)) \
)
#define	g_chunk_free(mem, mem_chunk)    G_STMT_START { \
  g_mem_chunk_free ((mem_chunk), (mem)); \
} G_STMT_END

#define g_string(x) #x


/* Provide macros for error handling. The "assert" macros will
 *  exit on failure. The "return" macros will exit the current
 *  function. Two different definitions are given for the macros
 *  in order to support gcc's __PRETTY_FUNCTION__ capability.
 */

#ifdef G_DISABLE_ASSERT

#define g_assert(expr)
#define g_assert_not_reached()

#else /* !G_DISABLE_ASSERT */

#ifdef __GNUC__

#define g_assert(expr)			G_STMT_START{\
     if (!(expr))				     \
       g_error ("file %s: line %d (%s): \"%s\"",     \
		__FILE__,			     \
		__LINE__,			     \
		__PRETTY_FUNCTION__,		     \
		#expr);			}G_STMT_END

#define g_assert_not_reached()		G_STMT_START{		      \
     g_error ("file %s: line %d (%s): \"should not be reached\"",     \
	      __FILE__,						      \
	      __LINE__,						      \
	      __PRETTY_FUNCTION__);	}G_STMT_END

#else /* !__GNUC__ */

#define g_assert(expr)			G_STMT_START{\
     if (!(expr))				     \
       g_error ("file %s: line %d: \"%s\"",	     \
		__FILE__,			     \
		__LINE__,			     \
		#expr);			}G_STMT_END

#define g_assert_not_reached()		G_STMT_START{		      \
     g_error ("file %s: line %d: \"should not be reached\"",	      \
	      __FILE__,						      \
	      __LINE__);		}G_STMT_END

#endif /* __GNUC__ */

#endif /* G_DISABLE_ASSERT */

#ifdef G_DISABLE_CHECKS

#define g_return_if_fail(expr)
#define g_return_val_if_fail(expr,val)

#else /* !G_DISABLE_CHECKS */

#ifdef __GNUC__

#define g_return_if_fail(expr)		G_STMT_START{	       \
     if (!(expr))					       \
       {						       \
	 g_warning ("file %s: line %d (%s): \"%s\"",	       \
		    __FILE__,				       \
		    __LINE__,				       \
		    __PRETTY_FUNCTION__,		       \
		    #expr);				       \
	 return;					       \
       };				}G_STMT_END

#define g_return_val_if_fail(expr,val)	G_STMT_START{	       \
     if (!(expr))					       \
       {						       \
	 g_warning ("file %s: line %d (%s): \"%s\"",	       \
		    __FILE__,				       \
		    __LINE__,				       \
		    __PRETTY_FUNCTION__,		       \
		    #expr);				       \
	 return val;					       \
       };				}G_STMT_END

#else /* !__GNUC__ */

#define g_return_if_fail(expr)		G_STMT_START{	 \
     if (!(expr))					 \
       {						 \
	 g_warning ("file %s: line %d: \"%s\"",		 \
		    __FILE__,				 \
		    __LINE__,				 \
		    #expr);				 \
	 return;					 \
       };				}G_STMT_END

#define g_return_val_if_fail(expr, val)	G_STMT_START{	 \
     if (!(expr))					 \
       {						 \
	 g_warning ("file %s: line %d: \"%s\"",		 \
		    __FILE__,				 \
		    __LINE__,				 \
		    #expr);				 \
	 return val;					 \
       };				}G_STMT_END

#endif /* !__GNUC__ */

#endif /* G_DISABLE_CHECKS */


#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

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

/* Define macros for storing integers inside pointers */

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
/* This should never happen */
#endif

typedef gint32  gssize;
typedef guint32 gsize;
typedef gint32  gtime;


typedef struct _GList		GList;
typedef struct _GSList		GSList;
typedef struct _GHashTable	GHashTable;
typedef struct _GCache		GCache;
typedef struct _GTree		GTree;
typedef struct _GTimer		GTimer;
typedef struct _GMemChunk	GMemChunk;
typedef struct _GListAllocator	GListAllocator;
typedef struct _GStringChunk	GStringChunk;
typedef struct _GString		GString;
typedef struct _GArray		GArray;
typedef struct _GPtrArray	GPtrArray;
typedef struct _GByteArray	GByteArray;
typedef struct _GDebugKey	GDebugKey;
typedef struct _GScannerConfig	GScannerConfig;
typedef struct _GScanner	GScanner;
typedef union  _GValue		GValue;
typedef struct _GRelation      GRelation;
typedef struct _GTuples        GTuples;


typedef void		(*GFunc)		(gpointer  data,
						 gpointer  user_data);
typedef void		(*GHFunc)		(gpointer  key,
						 gpointer  value,
						 gpointer  user_data);
typedef gpointer	(*GCacheNewFunc)	(gpointer  key);
typedef gpointer	(*GCacheDupFunc)	(gpointer  value);
typedef void		(*GCacheDestroyFunc)	(gpointer  value);
typedef gint		(*GTraverseFunc)	(gpointer  key,
						 gpointer  value,
						 gpointer  data);
typedef gint		(*GSearchFunc)		(gpointer  key,
						 gpointer  data);
typedef void		(*GErrorFunc)		(gchar	  *str);
typedef void		(*GWarningFunc)		(gchar    *str);
typedef void		(*GPrintFunc)		(gchar    *str);
typedef void		(*GScannerMsgFunc)	(GScanner *scanner,
						 gchar	  *message,
						 gint	   error);
typedef void		(*GDestroyNotify)	(gpointer  data);

typedef guint		(*GHashFunc)		(gconstpointer    key);
typedef gint		(*GCompareFunc)		(gconstpointer    a,
						 gconstpointer    b);

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
  guint   len;
};

struct _GPtrArray
{
  gpointer *pdata;
  guint     len;
};

struct _GTuples
{
  guint len;
};

struct _GDebugKey
{
  gchar *key;
  guint  value;
};

struct _GHashTable { gint dummy; };
struct _GCache { gint dummy; };
struct _GTree { gint dummy; };
struct _GTimer { gint dummy; };
struct _GMemChunk { gint dummy; };
struct _GListAllocator { gint dummy; };
struct _GStringChunk { gint dummy; };

typedef enum
{
  G_IN_ORDER,
  G_PRE_ORDER,
  G_POST_ORDER
} GTraverseType;

/* Doubly linked lists
 */
GList* g_list_alloc	  	(void);
void   g_list_free	  	(GList     	*list);
void   g_list_free_1	  	(GList     	*list);
GList* g_list_append	  	(GList     	*list,
				 gpointer  	 data);
GList* g_list_prepend	  	(GList     	*list,
				 gpointer  	 data);
GList* g_list_insert	  	(GList     	*list,
				 gpointer  	 data,
				 gint	     	 position);
GList* g_list_insert_sorted	(GList		*list,
				 gpointer  	 data,
				 GCompareFunc	 func);			    
GList* g_list_concat	  	(GList     	*list1, 
				 GList     	*list2);
GList* g_list_remove	  	(GList     	*list,
				 gpointer  	 data);
GList* g_list_remove_link 	(GList     	*list,
				 GList     	*link);
GList* g_list_reverse	  	(GList     	*list);
GList* g_list_nth	  	(GList     	*list,
				 guint     	 n);
GList* g_list_find	  	(GList     	*list,
				 gpointer  	 data);
GList* g_list_find_custom 	(GList     	*list,
				 gpointer  	 data,
				 GCompareFunc 	 func);
gint   g_list_position    	(GList     	*list,
				 GList     	*link);
gint   g_list_index	  	(GList     	*list,
				 gpointer  	 data);
GList* g_list_last	  	(GList     	*list);
GList* g_list_first	  	(GList     	*list);
guint  g_list_length	  	(GList     	*list);
void   g_list_foreach	  	(GList     	*list,
				 GFunc     	 func,
				 gpointer  	 user_data);
gpointer g_list_nth_data  	(GList     	*list,
				 guint      	 n);

#define g_list_previous(list) 	((list) ? (((GList *)(list))->prev) : NULL)
#define g_list_next(list)	((list) ? (((GList *)(list))->next) : NULL)

/* Singly linked lists
 */
GSList* g_slist_alloc	    	(void);
void	g_slist_free	    	(GSList		*list);
void	g_slist_free_1	    	(GSList   	*list);
GSList* g_slist_append	    	(GSList   	*list,
				 gpointer  	 data);
GSList* g_slist_prepend	    	(GSList   	*list,
				 gpointer  	 data);
GSList* g_slist_insert	    	(GSList   	*list,
				 gpointer 	 data,
				 gint     	 position);
GSList* g_slist_insert_sorted	(GSList   	*list,
				 gpointer 	 data,
				 GCompareFunc	 func);			    
GSList* g_slist_concat	    	(GSList   	*list1, 
				 GSList   	*list2);
GSList* g_slist_remove	    	(GSList   	*list,
				 gpointer 	 data);
GSList* g_slist_remove_link 	(GSList   	*list,
				 GSList   	*link);
GSList* g_slist_reverse	    	(GSList   	*list);
GSList* g_slist_nth	    	(GSList   	*list,
				 guint    	 n);
GSList* g_slist_find	    	(GSList   	*list,
				 gpointer 	 data);
GSList* g_slist_find_custom 	(GSList   	*list,
				 gpointer 	 data,
				 GCompareFunc	 func);
gint    g_slist_position    	(GSList   	*list,
				 GSList   	*link);
gint	g_slist_index	    	(GSList   	*list,
				 gpointer 	 data);
GSList* g_slist_last	    	(GSList   	*list);
guint	g_slist_length	    	(GSList   	*list);
void	g_slist_foreach	    	(GSList   	*list,
				 GFunc    	 func,
				 gpointer 	 user_data);
gpointer g_slist_nth_data   	(GSList   	*list,
				 guint    	 n);

#define g_slist_next(slist)	((slist) ? (((GSList *)(slist))->next) : NULL)

/* List Allocators
 */
GListAllocator* g_list_allocator_new  (void);
void		g_list_allocator_free (GListAllocator* allocator);
GListAllocator* g_slist_set_allocator (GListAllocator* allocator);
GListAllocator* g_list_set_allocator  (GListAllocator* allocator);


/* Hash tables
 */
GHashTable* g_hash_table_new	 (GHashFunc	  hash_func,
				  GCompareFunc	  key_compare_func);
void	    g_hash_table_destroy (GHashTable	 *hash_table);
void	    g_hash_table_insert	 (GHashTable	 *hash_table,
				  gpointer        key,
				  gpointer        value);
void	    g_hash_table_remove	 (GHashTable	 *hash_table,
				  gconstpointer   key);
gpointer    g_hash_table_lookup	 (GHashTable	 *hash_table,
				  gconstpointer    key);
void	    g_hash_table_freeze	 (GHashTable	 *hash_table);
void	    g_hash_table_thaw	 (GHashTable	 *hash_table);
void	    g_hash_table_foreach (GHashTable	 *hash_table,
				  GHFunc	  func,
				  gpointer	  user_data);
gint	    g_hash_table_size    (GHashTable	 *hash_table);


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


/* Trees
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


/* Memory
 */

#ifdef USE_DMALLOC

#define g_malloc(size)	     (gpointer) MALLOC(size)
#define g_malloc0(size)	     (gpointer) CALLOC(char,size)
#define g_realloc(mem,size)  (gpointer) REALLOC(mem,char,size)
#define g_free(mem)	     FREE(mem)

#else /* USE_DMALLOC */

gpointer g_malloc      (gulong	  size);
gpointer g_malloc0     (gulong	  size);
gpointer g_realloc     (gpointer  mem,
			gulong	  size);
void	 g_free	       (gpointer  mem);

#endif /* USE_DMALLOC */

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


/* Output
 */
void g_error   (const gchar *format, ...) G_GNUC_PRINTF (1, 2);
void g_warning (const gchar *format, ...) G_GNUC_PRINTF (1, 2);
void g_message (const gchar *format, ...) G_GNUC_PRINTF (1, 2);
void g_print   (const gchar *format, ...) G_GNUC_PRINTF (1, 2);

/* Utility functions
 */
#define G_STR_DELIMITERS     "_-|> <."
void    g_strdelimit		(gchar       *string,
				 const gchar *delimiters,
				 gchar        new_delimiter);
gchar*	g_strdup     		(const gchar *str);
gchar*	g_strconcat  		(const gchar *string1,
				 ...); /* NULL terminated */
gdouble g_strtod     		(const gchar *nptr,
				 gchar      **endptr);
gchar*	g_strerror   		(gint         errnum);
gchar*	g_strsignal  		(gint         signum);
gint    g_strcasecmp 		(const gchar *s1,
				 const gchar *s2);
void    g_strdown    		(gchar       *string);
void    g_strup      		(gchar       *string);
void    g_strreverse   		(gchar       *string);
guint   g_parse_debug_string	(const gchar *string, 
				 GDebugKey   *keys, 
				 guint        nkeys);
gint    g_snprintf		(gchar       *string,
				 gulong       n,
				 gchar const *format,
				 ...) G_GNUC_PRINTF (3, 4);

/* We make the assumption that if memmove isn't available, then
 * bcopy will do the job. This isn't safe everywhere. (bcopy can't
 * necessarily handle overlapping copies) */
#ifdef HAVE_MEMMOVE
#define g_memmove memmove
#else 
#define g_memmove(a,b,c) bcopy(b,a,c)
#endif

/* Errors
 */
GErrorFunc   g_set_error_handler   (GErrorFunc	 func);
GWarningFunc g_set_warning_handler (GWarningFunc func);
GPrintFunc   g_set_message_handler (GPrintFunc func);
GPrintFunc   g_set_print_handler   (GPrintFunc func);

void g_debug	      (const gchar *progname);
void g_attach_process (const gchar *progname,
		       gint         query);
void g_stack_trace    (const gchar *progname,
		       gint         query);


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
void	 g_string_free	    (GString     *string,
			     gint         free_segment);
GString* g_string_assign    (GString     *lval,
			     const gchar *rval);
GString* g_string_truncate  (GString     *string,
			     gint         len);
GString* g_string_append    (GString     *string,
			     const gchar *val);
GString* g_string_append_c  (GString     *string,
			     gchar        c);
GString* g_string_prepend   (GString     *string,
			     const gchar *val);
GString* g_string_prepend_c (GString     *string,
			     gchar        c);
GString* g_string_insert    (GString	 *string, 
			     gint 	  pos, 
			     const gchar *val);
GString* g_string_insert_c  (GString     *string, 
			     gint         pos, 
			     gchar        c);
GString* g_string_erase     (GString     *string, 
			     gint         pos, 
			     gint         len);
GString* g_string_down      (GString     *string);
GString* g_string_up        (GString     *string);
void	 g_string_sprintf   (GString     *string,
			     const gchar *format,
			     ...) G_GNUC_PRINTF (2, 3);
void	 g_string_sprintfa  (GString     *string,
			     const gchar *format,
			     ...) G_GNUC_PRINTF (2, 3);

/* Resizable arrays
 */
#define g_array_append_val(array,type,val) \
     g_rarray_append (array, (gpointer) &val, sizeof (type))
#define g_array_append_vals(array,type,vals,nvals) \
     g_rarray_append (array, (gpointer) vals, sizeof (type) * nvals)
#define g_array_prepend_val(array,type,val) \
     g_rarray_prepend (array, (gpointer) &val, sizeof (type))
#define g_array_prepend_vals(array,type,vals,nvals) \
     g_rarray_prepend (array, (gpointer) vals, sizeof (type) * nvals)
#define g_array_truncate(array,type,length) \
     g_rarray_truncate (array, length, sizeof (type))
#define g_array_index(array,type,index) \
     ((type*) array->data)[index]

GArray* g_array_new	  (gint	     zero_terminated);
void	g_array_free	  (GArray   *array,
			   gint	     free_segment);
GArray* g_rarray_append	  (GArray   *array,
			   gpointer  data,
			   gint	     size);
GArray* g_rarray_prepend  (GArray   *array,
			   gpointer  data,
			   gint	     size);
GArray* g_rarray_truncate (GArray   *array,
			   gint	     length,
			   gint	     size);

/* Resizable pointer array.  This interface is much less complicated
 * than the above.  Add appends appends a pointer.  Remove fills any
 * cleared spot and shortens the array.
 */

#define     g_ptr_array_index(array,index) (array->pdata)[index]

GPtrArray*  g_ptr_array_new	           (void);
void   	    g_ptr_array_free	           (GPtrArray   *array,
					    gboolean     free_seg);
void        g_ptr_array_set_size           (GPtrArray   *array,
					    gint	 length);
void        g_ptr_array_remove_index       (GPtrArray   *array,
					    gint         index);
gboolean    g_ptr_array_remove             (GPtrArray   *array,
					    gpointer     data);
void        g_ptr_array_add                (GPtrArray   *array,
					    gpointer     data);

/* Byte arrays, an array of guint8.  Implemented as a GArray,
 * but type-safe.
 */

GByteArray* g_byte_array_new      (void);
void	    g_byte_array_free     (GByteArray *array,
			           gint	       free_segment);

GByteArray* g_byte_array_append   (GByteArray   *array,
				   const guint8 *data,
				   guint         len);

GByteArray* g_byte_array_prepend  (GByteArray   *array,
				   const guint8 *data,
				   guint         len);

GByteArray* g_byte_array_truncate (GByteArray *array,
				   gint	       length);


/* Hash Functions
 */
gint  g_str_equal (gconstpointer   v,
		   gconstpointer   v2);
guint g_str_hash  (gconstpointer v);

gint  g_int_equal (gconstpointer v,
		   gconstpointer v2);
guint g_int_hash  (gconstpointer v);

/* This "hash" function will just return the key's adress as an
 * unsigned integer. Useful for hashing on plain adresses or
 * simple integer values.
 */
guint g_direct_hash  (gconstpointer v);
gint  g_direct_equal (gconstpointer v,
		      gconstpointer v2);



/* Location Associated Data
 */
void	  g_dataset_destroy		(gconstpointer   dataset_location);
guint	  g_dataset_try_key		(const gchar    *key);
guint	  g_dataset_force_id		(const gchar    *key);
gchar*	  g_dataset_retrive_key		(guint           key_id);
gpointer  g_dataset_id_get_data		(gconstpointer   dataset_location,
					 guint		 key_id);
void	  g_dataset_id_set_data_full	(gconstpointer   dataset_location,
					 guint		 key_id,
					 gpointer        data,
					 GDestroyNotify	 destroy_func);
void	  g_dataset_id_set_destroy	(gconstpointer   dataset_location,
					 guint		 key_id,
					 GDestroyNotify	 destroy_func);

#define	  g_dataset_id_set_data(l,k,d)	G_STMT_START{g_dataset_id_set_data_full((l),(k),(d),NULL);}G_STMT_END
#define	  g_dataset_id_remove_data(l,k)	G_STMT_START{g_dataset_id_set_data((l),(k),NULL);}G_STMT_END
#define	  g_dataset_get_data(l,k)	(g_dataset_id_get_data((l),g_dataset_try_key(k)))
#define	  g_dataset_set_data_full(l,k,d,f) G_STMT_START{g_dataset_id_set_data_full((l),g_dataset_force_id(k),(d),(f));}G_STMT_END
#define	  g_dataset_set_destroy(l,k,f)  G_STMT_START{g_dataset_id_set_destroy((l),g_dataset_force_id(k),(f));}G_STMT_END
#define	  g_dataset_set_data(l,k,d)	G_STMT_START{g_dataset_set_data_full((l),(k),(d),NULL);}G_STMT_END
#define	  g_dataset_remove_data(l,k)	G_STMT_START{g_dataset_set_data((l),(k),NULL);}G_STMT_END


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

union	_GValue
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
};

struct	_GScanner
{
  /* unused portions */
  gpointer		user_data;
  const gchar		*input_name;
  guint			parse_errors;
  guint			max_parse_errors;
  
  /* maintained/used by the g_scanner_*() functions */
  GScannerConfig	*config;
  GTokenType		token;
  GValue		value;
  guint			line;
  guint			position;
  
  /* to be considered private */
  GTokenType		next_token;
  GValue		next_value;
  guint			next_line;
  guint			next_position;
  GHashTable		*symbol_table;
  const gchar		*text;
  guint			text_len;
  gint			input_fd;
  gint			peeked_char;

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
GValue		g_scanner_cur_value		(GScanner	*scanner);
guint		g_scanner_cur_line		(GScanner	*scanner);
guint		g_scanner_cur_position		(GScanner	*scanner);
gboolean	g_scanner_eof			(GScanner	*scanner);
void		g_scanner_add_symbol		(GScanner	*scanner,
						 const gchar	*symbol,
						 gpointer	value);
gpointer	g_scanner_lookup_symbol		(GScanner	*scanner,
						 const gchar	*symbol);
void		g_scanner_foreach_symbol	(GScanner	*scanner,
						 GHFunc		 func,
						 gpointer	 func_data);
void		g_scanner_remove_symbol		(GScanner	*scanner,
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


/* Completion */

typedef gchar* (*GCompletionFunc)(gpointer);

typedef struct _GCompletion GCompletion;

struct _GCompletion {
	GList* items;
	GCompletionFunc func;

	gchar* prefix;
	GList* cache;

};

GCompletion* g_completion_new          (GCompletionFunc func);
void         g_completion_add_items    (GCompletion* cmp, 
                                        GList* items);
void         g_completion_remove_items (GCompletion* cmp, 
                                        GList* items);
void         g_completion_clear_items  (GCompletion* cmp);
GList*       g_completion_complete     (GCompletion* cmp, 
                                        gchar* prefix,
                                        gchar** new_prefix);
void         g_completion_free         (GCompletion* cmp);

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

GRelation* g_relation_new     (gint         fields);
void       g_relation_destroy (GRelation   *relation);
void       g_relation_index   (GRelation   *relation,
			       gint         field,
			       GHashFunc    hash_func,
			       GCompareFunc key_compare_func);
void       g_relation_insert  (GRelation   *relation,
			       ...);
gint       g_relation_delete  (GRelation   *relation,
			       gconstpointer  key,
			       gint         field);
GTuples*   g_relation_select  (GRelation   *relation,
			       gconstpointer  key,
			       gint         field);
gint       g_relation_count   (GRelation   *relation,
			       gconstpointer  key,
			       gint         field);
gboolean   g_relation_exists  (GRelation   *relation,
			       ...);
void       g_relation_print   (GRelation   *relation);

void       g_tuples_destroy   (GTuples     *tuples);
gpointer   g_tuples_index     (GTuples     *tuples,
			       gint         index,
			       gint         field);


/* Glib version.
 */
extern const guint glib_major_version;
extern const guint glib_minor_version;
extern const guint glib_micro_version;

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __G_LIB_H__ */
