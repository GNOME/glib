/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GLib Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GLib Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GLib at ftp://ftp.gtk.org/pub/gtk/. 
 */

/* 
 * MT safe
 */

#include "config.h"

#include "gtree.h"

#include "gatomic.h"
#include "gtestutils.h"
#include "gslice.h"

#define MAX_GTREE_HEIGHT 40
/* G_MAXUINT nodes will be covered by tree height of log2(G_MAXUINT) + 2. */
G_STATIC_ASSERT ((G_GUINT64_CONSTANT (1) << (MAX_GTREE_HEIGHT - 2)) >= G_MAXUINT);

/**
 * GTree:
 *
 * The GTree struct is an opaque data structure representing a
 * [balanced binary tree](data-structures.html#binary-trees). It should be
 * accessed only by using the following functions.
 */
struct _GTree
{
  GTreeNode        *root;
  GCompareDataFunc  key_compare;
  GDestroyNotify    key_destroy_func;
  GDestroyNotify    value_destroy_func;
  gpointer          key_compare_data;
  guint             nnodes;
  gint              ref_count;
};

struct _GTreeNode
{
  gpointer   key;         /* key for this node */
  gpointer   value;       /* value stored at this node */
  GTreeNode *left;        /* left subtree */
  GTreeNode *right;       /* right subtree */
  gint8      balance;     /* height (right) - height (left) */
  guint8     left_child;
  guint8     right_child;
};


static GTreeNode* g_tree_node_new                   (gpointer       key,
                                                     gpointer       value);
static GTreeNode *g_tree_insert_internal (GTree *tree,
                                          gpointer key,
                                          gpointer value,
                                          gboolean replace,
                                          gboolean null_ret_ok);
static gboolean   g_tree_remove_internal            (GTree         *tree,
                                                     gconstpointer  key,
                                                     gboolean       steal);
static GTreeNode* g_tree_node_balance               (GTreeNode     *node);
static GTreeNode *g_tree_find_node                  (GTree         *tree,
                                                     gconstpointer  key);
static gint       g_tree_node_pre_order             (GTreeNode     *node,
                                                     GTraverseFunc  traverse_func,
                                                     gpointer       data);
static gint       g_tree_node_in_order              (GTreeNode     *node,
                                                     GTraverseFunc  traverse_func,
                                                     gpointer       data);
static gint       g_tree_node_post_order            (GTreeNode     *node,
                                                     GTraverseFunc  traverse_func,
                                                     gpointer       data);
static GTreeNode *g_tree_node_search (GTreeNode *node,
                                      GCompareFunc search_func,
                                      gconstpointer data);
static GTreeNode* g_tree_node_rotate_left           (GTreeNode     *node);
static GTreeNode* g_tree_node_rotate_right          (GTreeNode     *node);
#ifdef G_TREE_DEBUG
static void       g_tree_node_check                 (GTreeNode     *node);
#endif


static GTreeNode*
g_tree_node_new (gpointer key,
                 gpointer value)
{
  GTreeNode *node = g_slice_new (GTreeNode);

  node->balance = 0;
  node->left = NULL;
  node->right = NULL;
  node->left_child = FALSE;
  node->right_child = FALSE;
  node->key = key;
  node->value = value;

  return node;
}

/**
 * g_tree_new: (constructor)
 * @key_compare_func: the function used to order the nodes in the #GTree.
 *   It should return values similar to the standard strcmp() function -
 *   0 if the two arguments are equal, a negative value if the first argument 
 *   comes before the second, or a positive value if the first argument comes 
 *   after the second.
 * 
 * Creates a new #GTree.
 * 
 * Returns: a newly allocated #GTree
 */
GTree *
g_tree_new (GCompareFunc key_compare_func)
{
  g_return_val_if_fail (key_compare_func != NULL, NULL);

  return g_tree_new_full ((GCompareDataFunc) key_compare_func, NULL,
                          NULL, NULL);
}

/**
 * g_tree_new_with_data:
 * @key_compare_func: qsort()-style comparison function
 * @key_compare_data: data to pass to comparison function
 * 
 * Creates a new #GTree with a comparison function that accepts user data.
 * See g_tree_new() for more details.
 * 
 * Returns: a newly allocated #GTree
 */
GTree *
g_tree_new_with_data (GCompareDataFunc key_compare_func,
                      gpointer         key_compare_data)
{
  g_return_val_if_fail (key_compare_func != NULL, NULL);
  
  return g_tree_new_full (key_compare_func, key_compare_data, 
                          NULL, NULL);
}

/**
 * g_tree_new_full:
 * @key_compare_func: qsort()-style comparison function
 * @key_compare_data: data to pass to comparison function
 * @key_destroy_func: a function to free the memory allocated for the key 
 *   used when removing the entry from the #GTree or %NULL if you don't
 *   want to supply such a function
 * @value_destroy_func: a function to free the memory allocated for the 
 *   value used when removing the entry from the #GTree or %NULL if you 
 *   don't want to supply such a function
 * 
 * Creates a new #GTree like g_tree_new() and allows to specify functions 
 * to free the memory allocated for the key and value that get called when 
 * removing the entry from the #GTree.
 * 
 * Returns: a newly allocated #GTree
 */
GTree *
g_tree_new_full (GCompareDataFunc key_compare_func,
                 gpointer         key_compare_data,
                 GDestroyNotify   key_destroy_func,
                 GDestroyNotify   value_destroy_func)
{
  GTree *tree;
  
  g_return_val_if_fail (key_compare_func != NULL, NULL);
  
  tree = g_slice_new (GTree);
  tree->root               = NULL;
  tree->key_compare        = key_compare_func;
  tree->key_destroy_func   = key_destroy_func;
  tree->value_destroy_func = value_destroy_func;
  tree->key_compare_data   = key_compare_data;
  tree->nnodes             = 0;
  tree->ref_count          = 1;
  
  return tree;
}

/**
 * g_tree_node_first:
 * @tree: a #GTree
 *
 * Returns the first in-order node of the tree, or %NULL
 * for an empty tree.
 *
 * Returns: (nullable) (transfer none): the first node in the tree
 *
 * Since: 2.68
 */
GTreeNode *
g_tree_node_first (GTree *tree)
{
  GTreeNode *tmp;

  g_return_val_if_fail (tree != NULL, NULL);

  if (!tree->root)
    return NULL;

  tmp = tree->root;

  while (tmp->left_child)
    tmp = tmp->left;

  return tmp;
}

/**
 * g_tree_node_last:
 * @tree: a #GTree
 *
 * Returns the last in-order node of the tree, or %NULL
 * for an empty tree.
 *
 * Returns: (nullable) (transfer none): the last node in the tree
 *
 * Since: 2.68
 */
GTreeNode *
g_tree_node_last (GTree *tree)
{
  GTreeNode *tmp;

  g_return_val_if_fail (tree != NULL, NULL);

  if (!tree->root)
    return NULL;

  tmp = tree->root;

  while (tmp->right_child)
    tmp = tmp->right;

  return tmp;
}

/**
 * g_tree_node_previous
 * @node: a #GTree node
 *
 * Returns the previous in-order node of the tree, or %NULL
 * if the passed node was already the first one.
 *
 * Returns: (nullable) (transfer none): the previous node in the tree
 *
 * Since: 2.68
 */
GTreeNode *
g_tree_node_previous (GTreeNode *node)
{
  GTreeNode *tmp;

  g_return_val_if_fail (node != NULL, NULL);

  tmp = node->left;

  if (node->left_child)
    while (tmp->right_child)
      tmp = tmp->right;

  return tmp;
}

/**
 * g_tree_node_next
 * @node: a #GTree node
 *
 * Returns the next in-order node of the tree, or %NULL
 * if the passed node was already the last one.
 *
 * Returns: (nullable) (transfer none): the next node in the tree
 *
 * Since: 2.68
 */
GTreeNode *
g_tree_node_next (GTreeNode *node)
{
  GTreeNode *tmp;

  g_return_val_if_fail (node != NULL, NULL);

  tmp = node->right;

  if (node->right_child)
    while (tmp->left_child)
      tmp = tmp->left;

  return tmp;
}

/**
 * g_tree_remove_all:
 * @tree: a #GTree
 *
 * Removes all nodes from a #GTree and destroys their keys and values,
 * then resets the #GTree’s root to %NULL.
 *
 * Since: 2.70
 */
void
g_tree_remove_all (GTree *tree)
{
  GTreeNode *node;
  GTreeNode *next;

  g_return_if_fail (tree != NULL);

  node = g_tree_node_first (tree);

  while (node)
    {
      next = g_tree_node_next (node);

      if (tree->key_destroy_func)
        tree->key_destroy_func (node->key);
      if (tree->value_destroy_func)
        tree->value_destroy_func (node->value);
      g_slice_free (GTreeNode, node);

#ifdef G_TREE_DEBUG
      g_assert (tree->nnodes > 0);
      tree->nnodes--;
#endif

      node = next;
    }

#ifdef G_TREE_DEBUG
  g_assert (tree->nnodes == 0);
#endif

  tree->root = NULL;
#ifndef G_TREE_DEBUG
  tree->nnodes = 0;
#endif
}

/**
 * g_tree_ref:
 * @tree: a #GTree
 *
 * Increments the reference count of @tree by one.
 *
 * It is safe to call this function from any thread.
 *
 * Returns: the passed in #GTree
 *
 * Since: 2.22
 */
GTree *
g_tree_ref (GTree *tree)
{
  g_return_val_if_fail (tree != NULL, NULL);

  g_atomic_int_inc (&tree->ref_count);

  return tree;
}

/**
 * g_tree_unref:
 * @tree: a #GTree
 *
 * Decrements the reference count of @tree by one.
 * If the reference count drops to 0, all keys and values will
 * be destroyed (if destroy functions were specified) and all
 * memory allocated by @tree will be released.
 *
 * It is safe to call this function from any thread.
 *
 * Since: 2.22
 */
void
g_tree_unref (GTree *tree)
{
  g_return_if_fail (tree != NULL);

  if (g_atomic_int_dec_and_test (&tree->ref_count))
    {
      g_tree_remove_all (tree);
      g_slice_free (GTree, tree);
    }
}

/**
 * g_tree_destroy:
 * @tree: a #GTree
 * 
 * Removes all keys and values from the #GTree and decreases its
 * reference count by one. If keys and/or values are dynamically
 * allocated, you should either free them first or create the #GTree
 * using g_tree_new_full(). In the latter case the destroy functions
 * you supplied will be called on all keys and values before destroying
 * the #GTree.
 */
void
g_tree_destroy (GTree *tree)
{
  g_return_if_fail (tree != NULL);

  g_tree_remove_all (tree);
  g_tree_unref (tree);
}

static GTreeNode *
g_tree_insert_replace_node_internal (GTree *tree,
                                     gpointer key,
                                     gpointer value,
                                     gboolean replace,
                                     gboolean null_ret_ok)
{
  GTreeNode *node;

  g_return_val_if_fail (tree != NULL, NULL);

  node = g_tree_insert_internal (tree, key, value, replace, null_ret_ok);

#ifdef G_TREE_DEBUG
  g_tree_node_check (tree->root);
#endif

  return node;
}

/**
 * g_tree_insert_node:
 * @tree: a #GTree
 * @key: the key to insert
 * @value: the value corresponding to the key
 * 
 * Inserts a key/value pair into a #GTree.
 *
 * If the given key already exists in the #GTree its corresponding value
 * is set to the new value. If you supplied a @value_destroy_func when
 * creating the #GTree, the old value is freed using that function. If
 * you supplied a @key_destroy_func when creating the #GTree, the passed
 * key is freed using that function.
 *
 * The tree is automatically 'balanced' as new key/value pairs are added,
 * so that the distance from the root to every leaf is as small as possible.
 * The cost of maintaining a balanced tree while inserting new key/value
 * result in a O(n log(n)) operation where most of the other operations
 * are O(log(n)).
 *
 * Returns: (transfer none) (nullable): the inserted (or set) node or %NULL
 * if insertion would overflow the tree node counter.
 *
 * Since: 2.68
 */
GTreeNode *
g_tree_insert_node (GTree    *tree,
                    gpointer  key,
                    gpointer  value)
{
  return g_tree_insert_replace_node_internal (tree, key, value, FALSE, TRUE);
}

/**
 * g_tree_insert:
 * @tree: a #GTree
 * @key: the key to insert
 * @value: the value corresponding to the key
 * 
 * Inserts a key/value pair into a #GTree.
 *
 * Inserts a new key and value into a #GTree as g_tree_insert_node() does,
 * only this function does not return the inserted or set node.
 */
void
g_tree_insert (GTree    *tree,
               gpointer  key,
               gpointer  value)
{
  g_tree_insert_replace_node_internal (tree, key, value, FALSE, FALSE);
}

/**
 * g_tree_replace_node:
 * @tree: a #GTree
 * @key: the key to insert
 * @value: the value corresponding to the key
 *
 * Inserts a new key and value into a #GTree similar to g_tree_insert_node().
 * The difference is that if the key already exists in the #GTree, it gets 
 * replaced by the new key. If you supplied a @value_destroy_func when 
 * creating the #GTree, the old value is freed using that function. If you 
 * supplied a @key_destroy_func when creating the #GTree, the old key is 
 * freed using that function. 
 *
 * The tree is automatically 'balanced' as new key/value pairs are added,
 * so that the distance from the root to every leaf is as small as possible.
 *
 * Returns: (transfer none) (nullable): the inserted (or set) node or %NULL
 * if insertion would overflow the tree node counter.
 *
 * Since: 2.68
 */
GTreeNode *
g_tree_replace_node (GTree    *tree,
                     gpointer  key,
                     gpointer  value)
{
  return g_tree_insert_replace_node_internal (tree, key, value, TRUE, TRUE);
}

/**
 * g_tree_replace:
 * @tree: a #GTree
 * @key: the key to insert
 * @value: the value corresponding to the key
 *
 * Inserts a new key and value into a #GTree as g_tree_replace_node() does,
 * only this function does not return the inserted or set node.
 */
void
g_tree_replace (GTree    *tree,
                gpointer  key,
                gpointer  value)
{
  g_tree_insert_replace_node_internal (tree, key, value, TRUE, FALSE);
}

/* internal checked nnodes increment routine */
static gboolean
g_tree_nnodes_inc_checked (GTree *tree, gboolean overflow_fatal)
{
  if (G_UNLIKELY (tree->nnodes == G_MAXUINT))
    {
      if (overflow_fatal)
        {
          g_error ("Incrementing GTree nnodes counter would overflow");
        }

      return FALSE;
    }

  tree->nnodes++;

  return TRUE;
}

/* internal insert routine */
static GTreeNode *
g_tree_insert_internal (GTree    *tree,
                        gpointer  key,
                        gpointer  value,
                        gboolean  replace,
                        gboolean  null_ret_ok)
{
  GTreeNode *node, *retnode;
  GTreeNode *path[MAX_GTREE_HEIGHT];
  int idx;

  g_return_val_if_fail (tree != NULL, NULL);

  if (!tree->root)
    {
      tree->root = g_tree_node_new (key, value);

#ifdef G_TREE_DEBUG
      g_assert (tree->nnodes == 0);
#endif
      tree->nnodes++;

      return tree->root;
    }

  idx = 0;
  path[idx++] = NULL;
  node = tree->root;

  while (1)
    {
      int cmp = tree->key_compare (key, node->key, tree->key_compare_data);
      
      if (cmp == 0)
        {
          if (tree->value_destroy_func)
            tree->value_destroy_func (node->value);

          node->value = value;

          if (replace)
            {
              if (tree->key_destroy_func)
                tree->key_destroy_func (node->key);

              node->key = key;
            }
          else
            {
              /* free the passed key */
              if (tree->key_destroy_func)
                tree->key_destroy_func (key);
            }

          return node;
        }
      else if (cmp < 0)
        {
          if (node->left_child)
            {
              path[idx++] = node;
              node = node->left;
            }
          else
            {
              GTreeNode *child;

              if (!g_tree_nnodes_inc_checked (tree, !null_ret_ok))
                {
                  return NULL;
                }

              child = g_tree_node_new (key, value);
              child->left = node->left;
              child->right = node;
              node->left = child;
              node->left_child = TRUE;
              node->balance -= 1;

              retnode = child;
              break;
            }
        }
      else
        {
          if (node->right_child)
            {
              path[idx++] = node;
              node = node->right;
            }
          else
            {
              GTreeNode *child;

              if (!g_tree_nnodes_inc_checked (tree, !null_ret_ok))
                {
                  return NULL;
                }

              child = g_tree_node_new (key, value);
              child->right = node->right;
              child->left = node;
              node->right = child;
              node->right_child = TRUE;
              node->balance += 1;

              retnode = child;
              break;
            }
        }
    }

  /* Restore balance. This is the goodness of a non-recursive
   * implementation, when we are done with balancing we 'break'
   * the loop and we are done.
   */
  while (1)
    {
      GTreeNode *bparent = path[--idx];
      gboolean left_node = (bparent && node == bparent->left);
      g_assert (!bparent || bparent->left == node || bparent->right == node);

      if (node->balance < -1 || node->balance > 1)
        {
          node = g_tree_node_balance (node);
          if (bparent == NULL)
            tree->root = node;
          else if (left_node)
            bparent->left = node;
          else
            bparent->right = node;
        }

      if (node->balance == 0 || bparent == NULL)
        break;
      
      if (left_node)
        bparent->balance -= 1;
      else
        bparent->balance += 1;

      node = bparent;
    }

  return retnode;
}

/**
 * g_tree_remove:
 * @tree: a #GTree
 * @key: the key to remove
 * 
 * Removes a key/value pair from a #GTree.
 *
 * If the #GTree was created using g_tree_new_full(), the key and value 
 * are freed using the supplied destroy functions, otherwise you have to 
 * make sure that any dynamically allocated values are freed yourself.
 * If the key does not exist in the #GTree, the function does nothing.
 *
 * The cost of maintaining a balanced tree while removing a key/value
 * result in a O(n log(n)) operation where most of the other operations
 * are O(log(n)).
 *
 * Returns: %TRUE if the key was found (prior to 2.8, this function
 *     returned nothing)
 */
gboolean
g_tree_remove (GTree         *tree,
               gconstpointer  key)
{
  gboolean removed;

  g_return_val_if_fail (tree != NULL, FALSE);

  removed = g_tree_remove_internal (tree, key, FALSE);

#ifdef G_TREE_DEBUG
  g_tree_node_check (tree->root);
#endif

  return removed;
}

/**
 * g_tree_steal:
 * @tree: a #GTree
 * @key: the key to remove
 * 
 * Removes a key and its associated value from a #GTree without calling 
 * the key and value destroy functions.
 *
 * If the key does not exist in the #GTree, the function does nothing.
 *
 * Returns: %TRUE if the key was found (prior to 2.8, this function
 *     returned nothing)
 */
gboolean
g_tree_steal (GTree         *tree,
              gconstpointer  key)
{
  gboolean removed;

  g_return_val_if_fail (tree != NULL, FALSE);

  removed = g_tree_remove_internal (tree, key, TRUE);

#ifdef G_TREE_DEBUG
  g_tree_node_check (tree->root);
#endif

  return removed;
}

/* internal remove routine */
static gboolean
g_tree_remove_internal (GTree         *tree,
                        gconstpointer  key,
                        gboolean       steal)
{
  GTreeNode *node, *parent, *balance;
  GTreeNode *path[MAX_GTREE_HEIGHT];
  int idx;
  gboolean left_node;

  g_return_val_if_fail (tree != NULL, FALSE);

  if (!tree->root)
    return FALSE;

  idx = 0;
  path[idx++] = NULL;
  node = tree->root;

  while (1)
    {
      int cmp = tree->key_compare (key, node->key, tree->key_compare_data);

      if (cmp == 0)
        break;
      else if (cmp < 0)
        {
          if (!node->left_child)
            return FALSE;

          path[idx++] = node;
          node = node->left;
        }
      else
        {
          if (!node->right_child)
            return FALSE;

          path[idx++] = node;
          node = node->right;
        }
    }

  /* The following code is almost equal to g_tree_remove_node,
   * except that we do not have to call g_tree_node_parent.
   */
  balance = parent = path[--idx];
  g_assert (!parent || parent->left == node || parent->right == node);
  left_node = (parent && node == parent->left);

  if (!node->left_child)
    {
      if (!node->right_child)
        {
          if (!parent)
            tree->root = NULL;
          else if (left_node)
            {
              parent->left_child = FALSE;
              parent->left = node->left;
              parent->balance += 1;
            }
          else
            {
              parent->right_child = FALSE;
              parent->right = node->right;
              parent->balance -= 1;
            }
        }
      else /* node has a right child */
        {
          GTreeNode *tmp = g_tree_node_next (node);
          tmp->left = node->left;

          if (!parent)
            tree->root = node->right;
          else if (left_node)
            {
              parent->left = node->right;
              parent->balance += 1;
            }
          else
            {
              parent->right = node->right;
              parent->balance -= 1;
            }
        }
    }
  else /* node has a left child */
    {
      if (!node->right_child)
        {
          GTreeNode *tmp = g_tree_node_previous (node);
          tmp->right = node->right;

          if (parent == NULL)
            tree->root = node->left;
          else if (left_node)
            {
              parent->left = node->left;
              parent->balance += 1;
            }
          else
            {
              parent->right = node->left;
              parent->balance -= 1;
            }
        }
      else /* node has a both children (pant, pant!) */
        {
          GTreeNode *prev = node->left;
          GTreeNode *next = node->right;
          GTreeNode *nextp = node;
          int old_idx = idx + 1;
          idx++;

          /* path[idx] == parent */
          /* find the immediately next node (and its parent) */
          while (next->left_child)
            {
              path[++idx] = nextp = next;
              next = next->left;
            }

          path[old_idx] = next;
          balance = path[idx];

          /* remove 'next' from the tree */
          if (nextp != node)
            {
              if (next->right_child)
                nextp->left = next->right;
              else
                nextp->left_child = FALSE;
              nextp->balance += 1;

              next->right_child = TRUE;
              next->right = node->right;
            }
          else
            node->balance -= 1;

          /* set the prev to point to the right place */
          while (prev->right_child)
            prev = prev->right;
          prev->right = next;

          /* prepare 'next' to replace 'node' */
          next->left_child = TRUE;
          next->left = node->left;
          next->balance = node->balance;

          if (!parent)
            tree->root = next;
          else if (left_node)
            parent->left = next;
          else
            parent->right = next;
        }
    }

  /* restore balance */
  if (balance)
    while (1)
      {
        GTreeNode *bparent = path[--idx];
        g_assert (!bparent || bparent->left == balance || bparent->right == balance);
        left_node = (bparent && balance == bparent->left);

        if(balance->balance < -1 || balance->balance > 1)
          {
            balance = g_tree_node_balance (balance);
            if (!bparent)
              tree->root = balance;
            else if (left_node)
              bparent->left = balance;
            else
              bparent->right = balance;
          }

        if (balance->balance != 0 || !bparent)
          break;

        if (left_node)
          bparent->balance += 1;
        else
          bparent->balance -= 1;

        balance = bparent;
      }

  if (!steal)
    {
      if (tree->key_destroy_func)
        tree->key_destroy_func (node->key);
      if (tree->value_destroy_func)
        tree->value_destroy_func (node->value);
    }

  g_slice_free (GTreeNode, node);

  tree->nnodes--;

  return TRUE;
}

/**
 * g_tree_node_key:
 * @node: a #GTree node
 *
 * Gets the key stored at a particular tree node.
 *
 * Returns: (nullable) (transfer none): the key at the node.
 *
 * Since: 2.68
 */
gpointer
g_tree_node_key (GTreeNode *node)
{
  g_return_val_if_fail (node != NULL, NULL);

  return node->key;
}

/**
 * g_tree_node_value:
 * @node: a #GTree node
 *
 * Gets the value stored at a particular tree node.
 *
 * Returns: (nullable) (transfer none): the value at the node.
 *
 * Since: 2.68
 */
gpointer
g_tree_node_value (GTreeNode *node)
{
  g_return_val_if_fail (node != NULL, NULL);

  return node->value;
}

/**
 * g_tree_lookup_node:
 * @tree: a #GTree
 * @key: the key to look up
 *
 * Gets the tree node corresponding to the given key. Since a #GTree is
 * automatically balanced as key/value pairs are added, key lookup
 * is O(log n) (where n is the number of key/value pairs in the tree).
 *
 * Returns: (nullable) (transfer none): the tree node corresponding to
 *          the key, or %NULL if the key was not found
 *
 * Since: 2.68
 */
GTreeNode *
g_tree_lookup_node (GTree         *tree,
                    gconstpointer  key)
{
  g_return_val_if_fail (tree != NULL, NULL);

  return g_tree_find_node (tree, key);
}

/**
 * g_tree_lookup:
 * @tree: a #GTree
 * @key: the key to look up
 * 
 * Gets the value corresponding to the given key. Since a #GTree is 
 * automatically balanced as key/value pairs are added, key lookup
 * is O(log n) (where n is the number of key/value pairs in the tree).
 *
 * Returns: the value corresponding to the key, or %NULL
 *     if the key was not found
 */
gpointer
g_tree_lookup (GTree         *tree,
               gconstpointer  key)
{
  GTreeNode *node;

  node = g_tree_lookup_node (tree, key);

  return node ? node->value : NULL;
}

/**
 * g_tree_lookup_extended:
 * @tree: a #GTree
 * @lookup_key: the key to look up
 * @orig_key: (out) (optional) (nullable): returns the original key
 * @value: (out) (optional) (nullable): returns the value associated with the key
 * 
 * Looks up a key in the #GTree, returning the original key and the
 * associated value. This is useful if you need to free the memory
 * allocated for the original key, for example before calling
 * g_tree_remove().
 * 
 * Returns: %TRUE if the key was found in the #GTree
 */
gboolean
g_tree_lookup_extended (GTree         *tree,
                        gconstpointer  lookup_key,
                        gpointer      *orig_key,
                        gpointer      *value)
{
  GTreeNode *node;
  
  g_return_val_if_fail (tree != NULL, FALSE);
  
  node = g_tree_find_node (tree, lookup_key);
  
  if (node)
    {
      if (orig_key)
        *orig_key = node->key;
      if (value)
        *value = node->value;
      return TRUE;
    }
  else
    return FALSE;
}

/**
 * g_tree_foreach:
 * @tree: a #GTree
 * @func: (scope call): the function to call for each node visited.
 *     If this function returns %TRUE, the traversal is stopped.
 * @user_data: user data to pass to the function
 *
 * Calls the given function for each of the key/value pairs in the #GTree.
 * The function is passed the key and value of each pair, and the given
 * @data parameter. The tree is traversed in sorted order.
 *
 * The tree may not be modified while iterating over it (you can't 
 * add/remove items). To remove all items matching a predicate, you need 
 * to add each item to a list in your #GTraverseFunc as you walk over 
 * the tree, then walk the list and remove each item.
 */
void
g_tree_foreach (GTree         *tree,
                GTraverseFunc  func,
                gpointer       user_data)
{
  GTreeNode *node;

  g_return_if_fail (tree != NULL);
  
  if (!tree->root)
    return;

  node = g_tree_node_first (tree);

  while (node)
    {
      if ((*func) (node->key, node->value, user_data))
        break;
      
      node = g_tree_node_next (node);
    }
}

/**
 * g_tree_foreach_node:
 * @tree: a #GTree
 * @func: (scope call): the function to call for each node visited.
 *     If this function returns %TRUE, the traversal is stopped.
 * @user_data: user data to pass to the function
 *
 * Calls the given function for each of the nodes in the #GTree.
 * The function is passed the pointer to the particular node, and the given
 * @data parameter. The tree traversal happens in-order.
 *
 * The tree may not be modified while iterating over it (you can't
 * add/remove items). To remove all items matching a predicate, you need
 * to add each item to a list in your #GTraverseFunc as you walk over
 * the tree, then walk the list and remove each item.
 *
 * Since: 2.68
 */
void
g_tree_foreach_node (GTree             *tree,
                     GTraverseNodeFunc  func,
                     gpointer           user_data)
{
  GTreeNode *node;

  g_return_if_fail (tree != NULL);

  if (!tree->root)
    return;

  node = g_tree_node_first (tree);

  while (node)
    {
      if ((*func) (node, user_data))
        break;

      node = g_tree_node_next (node);
    }
}

/**
 * g_tree_traverse:
 * @tree: a #GTree
 * @traverse_func: (scope call): the function to call for each node visited. If this
 *   function returns %TRUE, the traversal is stopped.
 * @traverse_type: the order in which nodes are visited, one of %G_IN_ORDER,
 *   %G_PRE_ORDER and %G_POST_ORDER
 * @user_data: user data to pass to the function
 * 
 * Calls the given function for each node in the #GTree. 
 *
 * Deprecated:2.2: The order of a balanced tree is somewhat arbitrary.
 *     If you just want to visit all nodes in sorted order, use
 *     g_tree_foreach() instead. If you really need to visit nodes in
 *     a different order, consider using an [n-ary tree](data-structures.html#n-ary-trees).
 */
/**
 * GTraverseFunc:
 * @key: a key of a #GTree node
 * @value: the value corresponding to the key
 * @data: user data passed to g_tree_traverse()
 *
 * Specifies the type of function passed to g_tree_traverse(). It is
 * passed the key and value of each node, together with the @user_data
 * parameter passed to g_tree_traverse(). If the function returns
 * %TRUE, the traversal is stopped.
 *
 * Returns: %TRUE to stop the traversal
 */
void
g_tree_traverse (GTree         *tree,
                 GTraverseFunc  traverse_func,
                 GTraverseType  traverse_type,
                 gpointer       user_data)
{
  g_return_if_fail (tree != NULL);

  if (!tree->root)
    return;

  switch (traverse_type)
    {
    case G_PRE_ORDER:
      g_tree_node_pre_order (tree->root, traverse_func, user_data);
      break;

    case G_IN_ORDER:
      g_tree_node_in_order (tree->root, traverse_func, user_data);
      break;

    case G_POST_ORDER:
      g_tree_node_post_order (tree->root, traverse_func, user_data);
      break;
    
    case G_LEVEL_ORDER:
      g_warning ("g_tree_traverse(): traverse type G_LEVEL_ORDER isn't implemented.");
      break;
    }
}

/**
 * g_tree_search_node:
 * @tree: a #GTree
 * @search_func: (scope call): a function used to search the #GTree
 * @user_data: the data passed as the second argument to @search_func
 *
 * Searches a #GTree using @search_func.
 *
 * The @search_func is called with a pointer to the key of a key/value
 * pair in the tree, and the passed in @user_data. If @search_func returns
 * 0 for a key/value pair, then the corresponding node is returned as
 * the result of g_tree_search(). If @search_func returns -1, searching
 * will proceed among the key/value pairs that have a smaller key; if
 * @search_func returns 1, searching will proceed among the key/value
 * pairs that have a larger key.
 *
 * Returns: (nullable) (transfer none): the node corresponding to the
 *          found key, or %NULL if the key was not found
 *
 * Since: 2.68
 */
GTreeNode *
g_tree_search_node (GTree         *tree,
                    GCompareFunc   search_func,
                    gconstpointer  user_data)
{
  g_return_val_if_fail (tree != NULL, NULL);

  if (!tree->root)
    return NULL;

  return g_tree_node_search (tree->root, search_func, user_data);
}

/**
 * g_tree_search:
 * @tree: a #GTree
 * @search_func: (scope call): a function used to search the #GTree
 * @user_data: the data passed as the second argument to @search_func
 *
 * Searches a #GTree using @search_func.
 *
 * The @search_func is called with a pointer to the key of a key/value
 * pair in the tree, and the passed in @user_data. If @search_func returns
 * 0 for a key/value pair, then the corresponding value is returned as
 * the result of g_tree_search(). If @search_func returns -1, searching
 * will proceed among the key/value pairs that have a smaller key; if
 * @search_func returns 1, searching will proceed among the key/value
 * pairs that have a larger key.
 *
 * Returns: the value corresponding to the found key, or %NULL
 *     if the key was not found
 */
gpointer
g_tree_search (GTree         *tree,
               GCompareFunc   search_func,
               gconstpointer  user_data)
{
  GTreeNode *node;

  node = g_tree_search_node (tree, search_func, user_data);

  return node ? node->value : NULL;
}

/**
 * g_tree_lower_bound:
 * @tree: a #GTree
 * @key: the key to calculate the lower bound for
 *
 * Gets the lower bound node corresponding to the given key,
 * or %NULL if the tree is empty or all the nodes in the tree
 * have keys that are strictly lower than the searched key.
 *
 * The lower bound is the first node that has its key greater
 * than or equal to the searched key.
 *
 * Returns: (nullable) (transfer none): the tree node corresponding to
 *          the lower bound, or %NULL if the tree is empty or has only
 *          keys strictly lower than the searched key.
 *
 * Since: 2.68
 */
GTreeNode *
g_tree_lower_bound (GTree         *tree,
                    gconstpointer  key)
{
  GTreeNode *node, *result;
  gint cmp;

  g_return_val_if_fail (tree != NULL, NULL);

  node = tree->root;
  if (!node)
    return NULL;

  result = NULL;
  while (1)
    {
      cmp = tree->key_compare (key, node->key, tree->key_compare_data);
      if (cmp <= 0)
        {
          result = node;

          if (!node->left_child)
            return result;

          node = node->left;
        }
      else
        {
          if (!node->right_child)
            return result;

          node = node->right;
        }
    }
}

/**
 * g_tree_upper_bound:
 * @tree: a #GTree
 * @key: the key to calculate the upper bound for
 *
 * Gets the upper bound node corresponding to the given key,
 * or %NULL if the tree is empty or all the nodes in the tree
 * have keys that are lower than or equal to the searched key.
 *
 * The upper bound is the first node that has its key strictly greater
 * than the searched key.
 *
 * Returns: (nullable) (transfer none): the tree node corresponding to the
 *          upper bound, or %NULL if the tree is empty or has only keys
 *          lower than or equal to the searched key.
 *
 * Since: 2.68
 */
GTreeNode *
g_tree_upper_bound (GTree         *tree,
                    gconstpointer  key)
{
  GTreeNode *node, *result;
  gint cmp;

  g_return_val_if_fail (tree != NULL, NULL);

  node = tree->root;
  if (!node)
    return NULL;

  result = NULL;
  while (1)
    {
      cmp = tree->key_compare (key, node->key, tree->key_compare_data);
      if (cmp < 0)
        {
          result = node;

          if (!node->left_child)
            return result;

          node = node->left;
        }
      else
        {
          if (!node->right_child)
            return result;

          node = node->right;
        }
    }
}

/**
 * g_tree_height:
 * @tree: a #GTree
 * 
 * Gets the height of a #GTree.
 *
 * If the #GTree contains no nodes, the height is 0.
 * If the #GTree contains only one root node the height is 1.
 * If the root node has children the height is 2, etc.
 * 
 * Returns: the height of @tree
 */
gint
g_tree_height (GTree *tree)
{
  GTreeNode *node;
  gint height;

  g_return_val_if_fail (tree != NULL, 0);

  if (!tree->root)
    return 0;

  height = 0;
  node = tree->root;

  while (1)
    {
      height += 1 + MAX(node->balance, 0);

      if (!node->left_child)
        return height;
      
      node = node->left;
    }
}

/**
 * g_tree_nnodes:
 * @tree: a #GTree
 * 
 * Gets the number of nodes in a #GTree.
 * 
 * Returns: the number of nodes in @tree
 *
 * The node counter value type is really a #guint,
 * but it is returned as a #gint due to backward
 * compatibility issues (can be cast back to #guint to
 * support its full range of values).
 */
gint
g_tree_nnodes (GTree *tree)
{
  g_return_val_if_fail (tree != NULL, 0);

  return tree->nnodes;
}

static GTreeNode *
g_tree_node_balance (GTreeNode *node)
{
  if (node->balance < -1)
    {
      if (node->left->balance > 0)
        node->left = g_tree_node_rotate_left (node->left);
      node = g_tree_node_rotate_right (node);
    }
  else if (node->balance > 1)
    {
      if (node->right->balance < 0)
        node->right = g_tree_node_rotate_right (node->right);
      node = g_tree_node_rotate_left (node);
    }

  return node;
}

static GTreeNode *
g_tree_find_node (GTree        *tree,
                  gconstpointer key)
{
  GTreeNode *node;
  gint cmp;

  node = tree->root;
  if (!node)
    return NULL;

  while (1)
    {
      cmp = tree->key_compare (key, node->key, tree->key_compare_data);
      if (cmp == 0)
        return node;
      else if (cmp < 0)
        {
          if (!node->left_child)
            return NULL;

          node = node->left;
        }
      else
        {
          if (!node->right_child)
            return NULL;

          node = node->right;
        }
    }
}

static gint
g_tree_node_pre_order (GTreeNode     *node,
                       GTraverseFunc  traverse_func,
                       gpointer       data)
{
  if ((*traverse_func) (node->key, node->value, data))
    return TRUE;

  if (node->left_child)
    {
      if (g_tree_node_pre_order (node->left, traverse_func, data))
        return TRUE;
    }

  if (node->right_child)
    {
      if (g_tree_node_pre_order (node->right, traverse_func, data))
        return TRUE;
    }

  return FALSE;
}

static gint
g_tree_node_in_order (GTreeNode     *node,
                      GTraverseFunc  traverse_func,
                      gpointer       data)
{
  if (node->left_child)
    {
      if (g_tree_node_in_order (node->left, traverse_func, data))
        return TRUE;
    }

  if ((*traverse_func) (node->key, node->value, data))
    return TRUE;

  if (node->right_child)
    {
      if (g_tree_node_in_order (node->right, traverse_func, data))
        return TRUE;
    }
  
  return FALSE;
}

static gint
g_tree_node_post_order (GTreeNode     *node,
                        GTraverseFunc  traverse_func,
                        gpointer       data)
{
  if (node->left_child)
    {
      if (g_tree_node_post_order (node->left, traverse_func, data))
        return TRUE;
    }

  if (node->right_child)
    {
      if (g_tree_node_post_order (node->right, traverse_func, data))
        return TRUE;
    }

  if ((*traverse_func) (node->key, node->value, data))
    return TRUE;

  return FALSE;
}

static GTreeNode *
g_tree_node_search (GTreeNode     *node,
                    GCompareFunc   search_func,
                    gconstpointer  data)
{
  gint dir;

  if (!node)
    return NULL;

  while (1) 
    {
      dir = (* search_func) (node->key, data);
      if (dir == 0)
        return node;
      else if (dir < 0) 
        {
          if (!node->left_child)
            return NULL;

          node = node->left;
        }
      else
        {
          if (!node->right_child)
            return NULL;

          node = node->right;
        }
    }
}

static GTreeNode *
g_tree_node_rotate_left (GTreeNode *node)
{
  GTreeNode *right;
  gint a_bal;
  gint b_bal;

  right = node->right;

  if (right->left_child)
    node->right = right->left;
  else
    {
      node->right_child = FALSE;
      right->left_child = TRUE;
    }
  right->left = node;

  a_bal = node->balance;
  b_bal = right->balance;

  if (b_bal <= 0)
    {
      if (a_bal >= 1)
        right->balance = b_bal - 1;
      else
        right->balance = a_bal + b_bal - 2;
      node->balance = a_bal - 1;
    }
  else
    {
      if (a_bal <= b_bal)
        right->balance = a_bal - 2;
      else
        right->balance = b_bal - 1;
      node->balance = a_bal - b_bal - 1;
    }

  return right;
}

static GTreeNode *
g_tree_node_rotate_right (GTreeNode *node)
{
  GTreeNode *left;
  gint a_bal;
  gint b_bal;

  left = node->left;

  if (left->right_child)
    node->left = left->right;
  else
    {
      node->left_child = FALSE;
      left->right_child = TRUE;
    }
  left->right = node;

  a_bal = node->balance;
  b_bal = left->balance;

  if (b_bal <= 0)
    {
      if (b_bal > a_bal)
        left->balance = b_bal + 1;
      else
        left->balance = a_bal + 2;
      node->balance = a_bal - b_bal + 1;
    }
  else
    {
      if (a_bal <= -1)
        left->balance = b_bal + 1;
      else
        left->balance = a_bal + b_bal + 2;
      node->balance = a_bal + 1;
    }

  return left;
}

#ifdef G_TREE_DEBUG
static gint
g_tree_node_height (GTreeNode *node)
{
  gint left_height;
  gint right_height;

  if (node)
    {
      left_height = 0;
      right_height = 0;

      if (node->left_child)
        left_height = g_tree_node_height (node->left);

      if (node->right_child)
        right_height = g_tree_node_height (node->right);

      return MAX (left_height, right_height) + 1;
    }

  return 0;
}

static void
g_tree_node_check (GTreeNode *node)
{
  gint left_height;
  gint right_height;
  gint balance;
  GTreeNode *tmp;

  if (node)
    {
      if (node->left_child)
        {
          tmp = g_tree_node_previous (node);
          g_assert (tmp->right == node);
        }

      if (node->right_child)
        {
          tmp = g_tree_node_next (node);
          g_assert (tmp->left == node);
        }

      left_height = 0;
      right_height = 0;
      
      if (node->left_child)
        left_height = g_tree_node_height (node->left);
      if (node->right_child)
        right_height = g_tree_node_height (node->right);
      
      balance = right_height - left_height;
      g_assert (balance == node->balance);
      
      if (node->left_child)
        g_tree_node_check (node->left);
      if (node->right_child)
        g_tree_node_check (node->right);
    }
}

static void
g_tree_node_dump (GTreeNode *node, 
                  gint       indent)
{
  g_print ("%*s%c\n", indent, "", *(char *)node->key);

  if (node->left_child)
    {
      g_print ("%*sLEFT\n", indent, "");
      g_tree_node_dump (node->left, indent + 2);
    }
  else if (node->left)
    g_print ("%*s<%c\n", indent + 2, "", *(char *)node->left->key);

  if (node->right_child)
    {
      g_print ("%*sRIGHT\n", indent, "");
      g_tree_node_dump (node->right, indent + 2);
    }
  else if (node->right)
    g_print ("%*s>%c\n", indent + 2, "", *(char *)node->right->key);
}

void
g_tree_dump (GTree *tree)
{
  if (tree->root)
    g_tree_node_dump (tree->root, 0);
}
#endif
