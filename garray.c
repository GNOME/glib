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
#include <string.h>
#include "glib.h"


#define MIN_ARRAY_SIZE  16


typedef struct _GRealArray  GRealArray;

struct _GRealArray
{
  guint8 *data;
  guint   len;
  guint   alloc;
  guint   zero_terminated;
};


static gint g_nearest_pow        (gint        num);
static void g_array_maybe_expand (GRealArray *array,
				  gint        len);


static GMemChunk *array_mem_chunk = NULL;


GArray*
g_array_new (gint zero_terminated)
{
  GRealArray *array;

  if (!array_mem_chunk)
    array_mem_chunk = g_mem_chunk_new ("array mem chunk",
				       sizeof (GRealArray),
				       1024, G_ALLOC_AND_FREE);

  array = g_chunk_new (GRealArray, array_mem_chunk);

  array->data = NULL;
  array->len = 0;
  array->alloc = 0;
  array->zero_terminated = (zero_terminated ? 1 : 0);

  return (GArray*) array;
}

void
g_array_free (GArray *array,
	      gint    free_segment)
{
  if (free_segment)
    g_free (array->data);

  g_mem_chunk_free (array_mem_chunk, array);
}

GArray*
g_rarray_append (GArray   *array,
		 gpointer  data,
		 gint      size)
{
  g_array_maybe_expand ((GRealArray*) array, size);

  memcpy (array->data + array->len, data, size);

  array->len += size;

  return array;
}

GArray*
g_rarray_prepend (GArray   *array,
		  gpointer  data,
		  gint      size)
{
  g_array_maybe_expand ((GRealArray*) array, size);

  g_memmove (array->data + size, array->data, array->len);
  memcpy (array->data, data, size);

  array->len += size;

  return array;
}

GArray*
g_rarray_truncate (GArray *array,
		   gint    length,
		   gint    size)
{
  if (array->data)
    memset (array->data + length * size, 0, size);
  array->len = length * size;
  return array;
}


static gint
g_nearest_pow (gint num)
{
  gint n = 1;

  while (n < num)
    n <<= 1;

  return n;
}

static void
g_array_maybe_expand (GRealArray *array,
		      gint        len)
{
  guint old_alloc;

  if ((array->len + len) > array->alloc)
    {
      old_alloc = array->alloc;

      array->alloc = g_nearest_pow (array->len + array->zero_terminated + len);
      array->alloc = MAX (array->alloc, MIN_ARRAY_SIZE);
      array->data = g_realloc (array->data, array->alloc);

      memset (array->data + old_alloc, 0, array->alloc - old_alloc);
    }
}

/* Pointer Array
 */

typedef struct _GRealPtrArray  GRealPtrArray;

struct _GRealPtrArray
{
  gpointer *pdata;
  guint     len;
  guint     alloc;
};

static void g_ptr_array_maybe_expand (GRealPtrArray *array,
				      gint           len);


static GMemChunk *ptr_array_mem_chunk = NULL;



GPtrArray*
g_ptr_array_new ()
{
  GRealPtrArray *array;

  if (!ptr_array_mem_chunk)
    ptr_array_mem_chunk = g_mem_chunk_new ("array mem chunk",
					   sizeof (GRealPtrArray),
					   1024, G_ALLOC_AND_FREE);

  array = g_chunk_new (GRealPtrArray, ptr_array_mem_chunk);

  array->pdata = NULL;
  array->len = 0;
  array->alloc = 0;

  return (GPtrArray*) array;
}

void
g_ptr_array_free (GPtrArray   *array,
		  gboolean  free_segment)
{
  g_return_if_fail (array);

  if (free_segment)
    g_free (array->pdata);

  g_mem_chunk_free (ptr_array_mem_chunk, array);
}

static void
g_ptr_array_maybe_expand (GRealPtrArray *array,
			  gint        len)
{
  guint old_alloc;

  if ((array->len + len) > array->alloc)
    {
      old_alloc = array->alloc;

      array->alloc = g_nearest_pow (array->len + len);
      array->alloc = MAX (array->alloc, MIN_ARRAY_SIZE);
      if (array->pdata)
	array->pdata = g_realloc (array->pdata, sizeof(gpointer) * array->alloc);
      else
	array->pdata = g_new0 (gpointer, array->alloc);

      memset (array->pdata + old_alloc, 0, array->alloc - old_alloc);
    }
}

void
g_ptr_array_set_size  (GPtrArray   *farray,
		       gint	     length)
{
  GRealPtrArray* array = (GRealPtrArray*) farray;

  g_return_if_fail (array);

  if (length > array->len)
    g_ptr_array_maybe_expand (array, (length - array->len));

  array->len = length;
}

void
g_ptr_array_remove_index (GPtrArray* farray,
			  gint index)
{
  GRealPtrArray* array = (GRealPtrArray*) farray;

  g_return_if_fail (array);

  g_return_if_fail (index >= array->len);

  array->pdata[index] = array->pdata[array->len - 1];

  array->pdata[array->len - 1] = NULL;

  array->len -= 1;
}

gboolean
g_ptr_array_remove (GPtrArray* farray,
		    gpointer data)
{
  GRealPtrArray* array = (GRealPtrArray*) farray;
  int i;

  g_return_val_if_fail (array, FALSE);

  for (i = 0; i < array->len; i += 1)
    {
      if (array->pdata[i] == data)
	{
	  g_ptr_array_remove_index (farray, i);
	  return TRUE;
	}
    }

  return FALSE;
}

void
g_ptr_array_add (GPtrArray* farray,
		 gpointer data)
{
  GRealPtrArray* array = (GRealPtrArray*) farray;

  g_return_if_fail (array);

  g_ptr_array_maybe_expand (array, 1);

  array->pdata[array->len++] = data;
}

/* Byte arrays
 */

GByteArray* g_byte_array_new      (void)
{
  return (GByteArray*) g_array_new (FALSE);
}

void	    g_byte_array_free     (GByteArray *array,
			           gint	       free_segment)
{
  g_array_free ((GArray*) array, free_segment);
}

GByteArray* g_byte_array_append   (GByteArray *array,
				   const guint8 *data,
				   guint       len)
{
  g_rarray_append ((GArray*) array, (guint8*)data, len);

  return array;
}

GByteArray* g_byte_array_prepend  (GByteArray *array,
				   const guint8 *data,
				   guint       len)
{
  g_rarray_prepend ((GArray*) array, (guint8*)data, len);

  return array;
}

GByteArray* g_byte_array_truncate (GByteArray *array,
				   gint	       length)
{
  g_rarray_truncate ((GArray*) array, length, 1);

  return array;
}
