/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * gdataset.c: Generic dataset mechanism, similar to GtkObject data.
 * Copyright (C) 1998 Tim Janik
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
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */
#include	"glib.h"



/* --- defines --- */
#define	G_DATASET_ID_BLOCK_SIZE			(1024)
#define	G_DATASET_MEM_CHUNK_PREALLOC		(512)
#define	G_DATASET_DATA_MEM_CHUNK_PREALLOC	(1024)


/* --- structures --- */
typedef struct _GDatasetData GDatasetData;
typedef struct _GDataset GDataset;
struct _GDatasetData
{
  GDatasetData *next;
  guint id;
  gpointer data;
  GDestroyNotify destroy_func;
};

struct _GDataset
{
  gconstpointer location;
  GDatasetData *data_list;
};


/* --- prototypes --- */
static inline GDataset*	g_dataset_lookup	(gconstpointer dataset_location);
static inline void	g_dataset_destroy_i	(GDataset     *dataset);
static void		g_dataset_initialize	(void);
static guint*		g_dataset_id_new	(void);


/* --- variables --- */
static GHashTable *g_dataset_location_ht = NULL;
static GHashTable *g_dataset_key_ht = NULL;
static GDataset *g_dataset_cached = NULL;
static GMemChunk *g_dataset_mem_chunk = NULL;
static GMemChunk *g_dataset_data_mem_chunk = NULL;
static guint *g_dataset_id_block = NULL;
static guint g_dataset_id_index = G_DATASET_ID_BLOCK_SIZE + 1;


/* --- functions --- */
static inline GDataset*
g_dataset_lookup (gconstpointer	dataset_location)
{
  register GDataset *dataset;
  
  if (g_dataset_cached && g_dataset_cached->location == dataset_location)
    return g_dataset_cached;
  
  if (!g_dataset_location_ht)
    g_dataset_initialize ();
  
  dataset = g_hash_table_lookup (g_dataset_location_ht, dataset_location);
  if (dataset)
    g_dataset_cached = dataset;
  
  return dataset;
}

static inline void
g_dataset_destroy_i (GDataset *dataset)
{
  register GDatasetData *list;
  
  if (dataset == g_dataset_cached)
    g_dataset_cached = NULL;
  g_hash_table_remove (g_dataset_location_ht, dataset->location);
  
  list = dataset->data_list;
  g_mem_chunk_free (g_dataset_mem_chunk, dataset);
  
  while (list)
    {
      register GDatasetData *prev;
      
      prev = list;
      list = prev->next;
      
      if (prev->destroy_func)
	prev->destroy_func (prev->data);
      
      g_mem_chunk_free (g_dataset_data_mem_chunk, prev);
    }
}

void
g_dataset_destroy (gconstpointer  dataset_location)
{
  register GDataset *dataset;
  
  g_return_if_fail (dataset_location != NULL);
  
  dataset = g_dataset_lookup (dataset_location);
  if (dataset)
    g_dataset_destroy_i (dataset);
}

void
g_dataset_id_set_destroy (gconstpointer  dataset_location,
			  guint          key_id,
			  GDestroyNotify destroy_func)
{
  g_return_if_fail (dataset_location != NULL);
  
  if (key_id)
    {
      register GDataset *dataset;
      
      dataset = g_dataset_lookup (dataset_location);
      if (dataset)
	{
	  register GDatasetData *list;
	  
	  list = dataset->data_list;
	  while (list)
	    {
	      if (list->id == key_id)
		{
		  list->destroy_func = destroy_func;
		  return;
		}
	    }
	}
    }
}

gpointer
g_dataset_id_get_data (gconstpointer  dataset_location,
		       guint          key_id)
{
  g_return_val_if_fail (dataset_location != NULL, NULL);
  
  if (key_id)
    {
      register GDataset *dataset;
      
      dataset = g_dataset_lookup (dataset_location);
      if (dataset)
	{
	  register GDatasetData *list;
	  
	  for (list = dataset->data_list; list; list = list->next)
	    if (list->id == key_id)
	      return list->data;
	}
    }
  
  return NULL;
}

void
g_dataset_id_set_data_full (gconstpointer  dataset_location,
			    guint          key_id,
			    gpointer       data,
			    GDestroyNotify destroy_func)
{
  register GDataset *dataset;
  register GDatasetData *list;
  
  g_return_if_fail (dataset_location != NULL);
  g_return_if_fail (key_id > 0);
  
  dataset = g_dataset_lookup (dataset_location);
  if (!dataset)
    {
      dataset = g_chunk_new (GDataset, g_dataset_mem_chunk);
      dataset->location = dataset_location;
      dataset->data_list = NULL;
      g_hash_table_insert (g_dataset_location_ht, 
			   (gpointer) dataset->location, /* Yuck */
			   dataset);
    }
  
  list = dataset->data_list;
  if (!data)
    {
      register GDatasetData *prev;
      
      prev = NULL;
      while (list)
	{
	  if (list->id == key_id)
	    {
	      if (prev)
		prev->next = list->next;
	      else
		{
		  dataset->data_list = list->next;
		  
		  if (!dataset->data_list)
		    g_dataset_destroy_i (dataset);
		}
	      
	      /* we need to have unlinked before invoking the destroy function
	       */
	      if (list->destroy_func)
		list->destroy_func (list->data);
	      
	      g_mem_chunk_free (g_dataset_data_mem_chunk, list);
	      break;
	    }
	  
	  prev = list;
	  list = list->next;
	}
    }
  else
    {
      register GDatasetData *prev;
      
      prev = NULL;
      while (list)
	{
	  if (list->id == key_id)
	    {
	      if (prev)
		prev->next = list->next;
	      else
		dataset->data_list = list->next;
	      
	      /* we need to have unlinked before invoking the destroy function
	       */
	      if (list->destroy_func)
		list->destroy_func (list->data);
	      
	      break;
	    }
	  
	  prev = list;
	  list = list->next;
	}
      
      if (!list)
	list = g_chunk_new (GDatasetData, g_dataset_data_mem_chunk);
      list->next = dataset->data_list;
      list->id = key_id;
      list->data = data;
      list->destroy_func = destroy_func;
      dataset->data_list = list;
    }
}

guint
g_dataset_try_key (const gchar    *key)
{
  register guint *id;
  g_return_val_if_fail (key != NULL, 0);
  
  if (g_dataset_key_ht)
    {
      id = g_hash_table_lookup (g_dataset_key_ht, (gpointer) key);
      
      if (id)
	return *id;
    }
  
  return 0;
}

guint
g_dataset_force_id (const gchar    *key)
{
  register guint *id;
  
  g_return_val_if_fail (key != NULL, 0);
  
  if (!g_dataset_key_ht)
    g_dataset_initialize ();
  
  id = g_hash_table_lookup (g_dataset_key_ht, (gpointer) key);
  if (!id)
    {
      id = g_dataset_id_new ();
      g_hash_table_insert (g_dataset_key_ht, g_strdup (key), id);
    }
  
  return *id;
}

static void
g_dataset_initialize (void)
{
  if (!g_dataset_location_ht)
    {
      g_dataset_location_ht = g_hash_table_new (g_direct_hash, NULL);
      g_dataset_key_ht = g_hash_table_new (g_str_hash, g_str_equal);
      g_dataset_cached = NULL;
      g_dataset_mem_chunk =
	g_mem_chunk_new ("GDataset MemChunk",
			 sizeof (GDataset),
			 sizeof (GDataset) * G_DATASET_MEM_CHUNK_PREALLOC,
			 G_ALLOC_AND_FREE);
      g_dataset_data_mem_chunk =
	g_mem_chunk_new ("GDatasetData MemChunk",
			 sizeof (GDatasetData),
			 sizeof (GDatasetData) * G_DATASET_DATA_MEM_CHUNK_PREALLOC,
			 G_ALLOC_AND_FREE);
    }
}

static guint*
g_dataset_id_new (void)
{
  static guint seq_id = 1;
  register guint *id;
  
  if (g_dataset_id_index >= G_DATASET_ID_BLOCK_SIZE)
    {
      g_dataset_id_block = g_new (guint, G_DATASET_ID_BLOCK_SIZE);
      g_dataset_id_index = 0;
    }
  
  id = &g_dataset_id_block[g_dataset_id_index++];
  *id = seq_id++;
  
  return id;
}
