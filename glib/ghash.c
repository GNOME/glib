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
#include "glib.h"


#define HASH_TABLE_MIN_SIZE 11
#define HASH_TABLE_MAX_SIZE 13845163


typedef struct _GHashNode      GHashNode;
typedef struct _GRealHashTable GRealHashTable;

struct _GHashNode
{
  gpointer key;
  gpointer value;
  GHashNode *next;
};

struct _GRealHashTable
{
  gint size;
  gint nnodes;
  gint frozen;
  GHashNode **nodes;
  GHashFunc hash_func;
  GCompareFunc key_compare_func;
};


static void       g_hash_table_resize  (GHashTable *hash_table);
static gint       g_hash_closest_prime (gint        num);
static GHashNode* g_hash_node_new      (gpointer    key,
					gpointer    value);
static void       g_hash_node_destroy  (GHashNode  *hash_node);
static void       g_hash_nodes_destroy (GHashNode  *hash_node);


extern gint g_primes[];
extern gint g_nprimes;

static GMemChunk *node_mem_chunk = NULL;
static GHashNode *node_free_list = NULL;


GHashTable*
g_hash_table_new (GHashFunc    hash_func,
		  GCompareFunc key_compare_func)
{
  GRealHashTable *hash_table;

  g_return_val_if_fail (hash_func != NULL, NULL);

  hash_table = g_new (GRealHashTable, 1);
  hash_table->size = 0;
  hash_table->nnodes = 0;
  hash_table->frozen = FALSE;
  hash_table->nodes = NULL;
  hash_table->hash_func = hash_func;
  hash_table->key_compare_func = key_compare_func;

  return ((GHashTable*) hash_table);
}

void
g_hash_table_destroy (GHashTable *hash_table)
{
  GRealHashTable *rhash_table;
  gint i;

  if (hash_table)
    {
      rhash_table = (GRealHashTable*) hash_table;

      for (i = 0; i < rhash_table->size; i++)
	g_hash_nodes_destroy (rhash_table->nodes[i]);

      if (rhash_table->nodes)
	g_free (rhash_table->nodes);
      g_free (rhash_table);
    }
}

void
g_hash_table_insert (GHashTable *hash_table,
		     gpointer    key,
		     gpointer    value)
{
  GRealHashTable *rhash_table;
  GHashNode *node;
  guint hash_val;

  if (hash_table)
    {
      rhash_table = (GRealHashTable*) hash_table;

      if (rhash_table->size == 0)
	g_hash_table_resize (hash_table);

      hash_val = (* rhash_table->hash_func) (key) % rhash_table->size;

      node = rhash_table->nodes[hash_val];
      while (node)
	{
	  if ((rhash_table->key_compare_func &&
	       (* rhash_table->key_compare_func) (node->key, key)) ||
	      (node->key == key))
	    {
	      /* do not reset node->key in this place, keeping
	       * the old key might be intended.
	       * a g_hash_table_remove/g_hash_table_insert pair
	       * can be used otherwise.
	       *
	       * node->key = key;
	       */
	      node->value = value;
	      return;
	    }
	  node = node->next;
	}

      node = g_hash_node_new (key, value);
      node->next = rhash_table->nodes[hash_val];
      rhash_table->nodes[hash_val] = node;

      rhash_table->nnodes += 1;
      g_hash_table_resize (hash_table);
    }
}

void
g_hash_table_remove (GHashTable      *hash_table,
		     gconstpointer    key)
{
  GRealHashTable *rhash_table;
  GHashNode *node;
  GHashNode *prev;
  guint hash_val;

  rhash_table = (GRealHashTable*) hash_table;
  if (hash_table && rhash_table->size)
    {
      hash_val = (* rhash_table->hash_func) (key) % rhash_table->size;

      prev = NULL;
      node = rhash_table->nodes[hash_val];

      while (node)
	{
	  if ((rhash_table->key_compare_func &&
	       (* rhash_table->key_compare_func) (node->key, key)) ||
	      (node->key == key))
	    {
	      if (prev)
		prev->next = node->next;
	      if (node == rhash_table->nodes[hash_val])
		rhash_table->nodes[hash_val] = node->next;

	      g_hash_node_destroy (node);

	      rhash_table->nnodes -= 1;
	      g_hash_table_resize (hash_table);
	      break;
	    }

	  prev = node;
	  node = node->next;
	}
    }
}

gpointer
g_hash_table_lookup (GHashTable   *hash_table,
		     gconstpointer key)
{
  GRealHashTable *rhash_table;
  GHashNode *node;
  guint hash_val;

  rhash_table = (GRealHashTable*) hash_table;
  if (hash_table && rhash_table->size)
    {
      hash_val = (* rhash_table->hash_func) (key) % rhash_table->size;

      node = rhash_table->nodes[hash_val];

      /* Hash table lookup needs to be fast.
       *  We therefore remove the extra conditional of testing
       *  whether to call the key_compare_func or not from
       *  the inner loop.
       */
      if (rhash_table->key_compare_func)
	{
	  while (node)
	    {
	      if ((* rhash_table->key_compare_func) (node->key, key))
		return node->value;
	      node = node->next;
	    }
	}
      else
	{
	  while (node)
	    {
	      if (node->key == key)
		return node->value;
	      node = node->next;
	    }
	}
    }

  return NULL;
}

void
g_hash_table_freeze (GHashTable *hash_table)
{
  GRealHashTable *rhash_table;

  if (hash_table)
    {
      rhash_table = (GRealHashTable*) hash_table;
      rhash_table->frozen = TRUE;
    }
}

void
g_hash_table_thaw (GHashTable *hash_table)
{
  GRealHashTable *rhash_table;

  if (hash_table)
    {
      rhash_table = (GRealHashTable*) hash_table;
      rhash_table->frozen = FALSE;

      g_hash_table_resize (hash_table);
    }
}

void
g_hash_table_foreach (GHashTable *hash_table,
		      GHFunc      func,
		      gpointer    user_data)
{
  GRealHashTable *rhash_table;
  GHashNode *node;
  gint i;

  if (hash_table)
    {
      rhash_table = (GRealHashTable*) hash_table;

      for (i = 0; i < rhash_table->size; i++)
	{
	  node = rhash_table->nodes[i];

	  while (node)
	    {
	      (* func) (node->key, node->value, user_data);
	      node = node->next;
	    }
	}
    }
}


static void
g_hash_table_resize (GHashTable *hash_table)
{
  GRealHashTable *rhash_table;
  GHashNode **new_nodes;
  GHashNode *node;
  GHashNode *next;
  gfloat nodes_per_list;
  guint hash_val;
  gint new_size;
  gint need_resize;
  gint i;

  if (hash_table)
    {
      rhash_table = (GRealHashTable*) hash_table;

      if (rhash_table->size == 0)
	{
	  rhash_table->size = HASH_TABLE_MIN_SIZE;
	  rhash_table->nodes = g_new (GHashNode*, rhash_table->size);

	  for (i = 0; i < rhash_table->size; i++)
	    rhash_table->nodes[i] = NULL;
	}
      else if (!rhash_table->frozen)
	{
	  need_resize = FALSE;
	  nodes_per_list = (gfloat) rhash_table->nnodes / (gfloat) rhash_table->size;

	  if (nodes_per_list < 0.3)
	    {
	      if (rhash_table->size > HASH_TABLE_MIN_SIZE)
		need_resize = TRUE;
	    }
	  else if (nodes_per_list > 3.0)
	    {
	      if (rhash_table->size < HASH_TABLE_MAX_SIZE)
		need_resize = TRUE;
	    }

	  if (need_resize)
	    {
	      new_size = g_hash_closest_prime (rhash_table->nnodes);
	      if (new_size < HASH_TABLE_MIN_SIZE)
		new_size = HASH_TABLE_MIN_SIZE;
	      else if (new_size > HASH_TABLE_MAX_SIZE)
		new_size = HASH_TABLE_MAX_SIZE;

	      new_nodes = g_new (GHashNode*, new_size);

	      for (i = 0; i < new_size; i++)
		new_nodes[i] = NULL;

	      for (i = 0; i < rhash_table->size; i++)
		{
		  node = rhash_table->nodes[i];

		  while (node)
		    {
		      next = node->next;

		      hash_val = (* rhash_table->hash_func) (node->key) % new_size;
		      node->next = new_nodes[hash_val];
		      new_nodes[hash_val] = node;

		      node = next;
		    }
		}

	      g_free (rhash_table->nodes);

	      rhash_table->nodes = new_nodes;
	      rhash_table->size = new_size;
	    }
	}
    }
}

static gint
g_hash_closest_prime (gint num)
{
  gint i;

  for (i = 0; i < g_nprimes; i++)
    if ((g_primes[i] - num) > 0)
      return g_primes[i];

  return g_primes[g_nprimes - 1];
}

static GHashNode*
g_hash_node_new (gpointer key,
		 gpointer value)
{
  GHashNode *hash_node;

  if (node_free_list)
    {
      hash_node = node_free_list;
      node_free_list = node_free_list->next;
    }
  else
    {
      if (!node_mem_chunk)
	node_mem_chunk = g_mem_chunk_new ("hash node mem chunk",
					  sizeof (GHashNode),
					  1024, G_ALLOC_ONLY);

      hash_node = g_chunk_new (GHashNode, node_mem_chunk);
    }

  hash_node->key = key;
  hash_node->value = value;
  hash_node->next = NULL;

  return hash_node;
}

static void
g_hash_node_destroy (GHashNode *hash_node)
{
  if (hash_node)
    {
      hash_node->next = node_free_list;
      node_free_list = hash_node;
    }
}

static void
g_hash_nodes_destroy (GHashNode *hash_node)
{
  GHashNode *node;

  if (hash_node)
    {
      node = hash_node;
      while (node->next)
	node = node->next;
      node->next = node_free_list;
      node_free_list = hash_node;
    }
}
