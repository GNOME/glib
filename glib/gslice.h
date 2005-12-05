/* GLIB sliced memory - fast threaded memory chunk allocator
 * Copyright (C) 2005 Tim Janik
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
#ifndef __G_SLICE_H__
#define __G_SLICE_H__

#ifndef __G_MEM_H__
#error Include <glib.h> instead of <gslice.h>
#endif

#include <glib/gtypes.h>

G_BEGIN_DECLS

/* slices - fast allocation/release of small memory blocks
 */
gpointer g_slice_alloc          (gsize	  block_size) G_GNUC_MALLOC;
gpointer g_slice_alloc0         (gsize    block_size) G_GNUC_MALLOC;
void     g_slice_free1          (gsize    block_size,
				 gpointer mem_block);
void     g_slice_free_chain     (gsize    block_size,
				 gpointer mem_chain,
				 gsize    next_offset);
#define  g_slice_new(type)      ((type*) g_slice_alloc (sizeof (type)))
#define  g_slice_new0(type)     ((type*) g_slice_alloc0 (sizeof (type)))
/*       g_slice_free(type,mem) g_slice_free1 (sizeof (type), mem) */

#if 	__GNUC__ >= 2
/* for GCC, define a type-safe variant of g_slice_free() */
#define g_slice_free(type, mem)        ({                       \
  void (*g_slice_free) (gsize, type*);                          \
  while (0) g_slice_free (sizeof (type), mem);                  \
  g_slice_free1 (sizeof (type), mem);                           \
})
#else	/* !__GNUC__ */
#define	g_slice_free(type, mem)	g_slice_free1 (sizeof (type) + (gsize) (type*) 0, mem)
/* we go through the extra (gsize)(type*)0 hoop to ensure a known type argument */
#endif	/* !__GNUC__ */

/* --- internal debugging API --- */
typedef enum {
  G_SLICE_CONFIG_ALWAYS_MALLOC = 1,
  G_SLICE_CONFIG_BYPASS_MAGAZINES,
  G_SLICE_CONFIG_ALWAYS_FREE,
  G_SLICE_CONFIG_WORKING_SET_MSECS,
  G_SLICE_CONFIG_CHUNK_SIZES,
  G_SLICE_CONFIG_CONTENTION_COUNTER
} GSliceConfig;
void     g_slice_set_config	   (GSliceConfig ckey, gint64 value);
gint64   g_slice_get_config	   (GSliceConfig ckey);
gint64*  g_slice_get_config_state  (GSliceConfig ckey, gint64 address, guint *n_values);

G_END_DECLS

#endif /* __G_SLICE_H__ */
