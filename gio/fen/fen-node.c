/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set expandtab ts=4 shiftwidth=4: */
/* 
 * Copyright (C) 2008 Sun Microsystems, Inc. All rights reserved.
 * Use is subject to license terms.
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
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: Lin Ma <lin.ma@sun.com>
 */

#include "config.h"
#include <errno.h>
#include <strings.h>
#include <glib.h>
#include "fen-node.h"
#include "fen-dump.h"

#define	NODE_STAT(n)	(((node_t*)(n))->stat)

struct _dnode {
    gchar* filename;
    node_op_t* op;
    GTimeVal tv;
};

#ifdef GIO_COMPILATION
#define FN_W if (fn_debug_enabled) g_warning
static gboolean fn_debug_enabled = FALSE;
#else
#include "gam_error.h"
#define FN_W(...) GAM_DEBUG(DEBUG_INFO, __VA_ARGS__)
#endif

G_LOCK_EXTERN (fen_lock);
#define	PROCESS_DELETING_INTERVAL	900 /* in second */

static node_t* _head = NULL;
static GList *deleting_nodes = NULL;
static guint deleting_nodes_id = 0;

static node_t* node_new (node_t* parent, const gchar* basename);
static void node_delete (node_t* parent);
static gboolean remove_node_internal (node_t* node, node_op_t* op);
static void children_add (node_t *p, node_t *f);
static void children_remove (node_t *p, node_t *f);
static guint children_foreach_remove (node_t *f, GHRFunc func, gpointer user_data);
static void children_foreach (node_t *f, GHFunc func, gpointer user_data);
static gboolean children_remove_cb (gpointer key,
  gpointer value,
  gpointer user_data);

static struct _dnode*
_dnode_new (const gchar* filename, node_op_t* op)
{
    struct _dnode* d;

    g_assert (op);
    if ((d = g_new (struct _dnode, 1)) != NULL) {
        d->filename = g_strdup (filename);
        d->op = g_memdup (op, sizeof (node_op_t));
        g_assert (d->op);
        g_get_current_time (&d->tv);
        g_time_val_add (&d->tv, PROCESS_DELETING_INTERVAL);
    }
    return d;
}

static void
_dnode_free (struct _dnode* d)
{
    g_assert (d);
    g_free (d->filename);
    g_free (d->op);
    g_free (d);
}

static gboolean
g_timeval_lt (GTimeVal *val1, GTimeVal *val2)
{
    if (val1->tv_sec < val2->tv_sec)
        return TRUE;
  
    if (val1->tv_sec > val2->tv_sec)
        return FALSE;
  
    /* val1->tv_sec == val2->tv_sec */
    if (val1->tv_usec < val2->tv_usec)
        return TRUE;
  
    return FALSE;
}

static gboolean
scan_deleting_nodes (gpointer data)
{
    struct _dnode* d;
    GTimeVal tv_now;
    GList* i;
    GList* deleted_list = NULL;
    gboolean ret = TRUE;
    node_t* node;

    g_get_current_time (&tv_now);

    if (G_TRYLOCK (fen_lock)) {
        for (i = deleting_nodes; i; i = i->next) {
            d = (struct _dnode*)i->data;
            /* Time to free, try only once */
            if (g_timeval_lt (&d->tv, &tv_now)) {
                if ((node = find_node (d->filename)) != NULL) {
                    remove_node_internal (node, d->op);
                }
                _dnode_free (d);
                deleted_list = g_list_prepend (deleted_list, i);
            }
        }

        for (i = deleted_list; i; i = i->next) {
            deleting_nodes = g_list_remove_link (deleting_nodes,
              (GList *)i->data);
            g_list_free_1 ((GList *)i->data);
        }
        g_list_free (deleted_list);

        if (deleting_nodes == NULL) {
            deleting_nodes_id = 0;
            ret = FALSE;
        }
        G_UNLOCK (fen_lock);
    }
    return ret;
}

gpointer
node_get_data (node_t* node)
{
    g_assert (node);
    return node->user_data;
}

gpointer
node_set_data (node_t* node, gpointer user_data)
{
    gpointer data = node->user_data;
    g_assert (node);
    node->user_data = user_data;
    return data;
}

void
travel_nodes (node_t* node, node_op_t* op)
{
    GList* children;
    GList* i;

    if (node) {
        if (op && op->hit) {
            op->hit (node, op->user_data);
        }
    }
    children = g_hash_table_get_values (node->children);
    if (children) {
        for (i = children; i; i = i->next) {
            travel_nodes (i->data, op);
        }
        g_list_free (children);
    }
}

static node_t*
find_node_internal (node_t* node, const gchar* filename, node_op_t* op)
{
    gchar* str;
    gchar* token;
    gchar* lasts;
    node_t* parent;
    node_t* child;
    
    g_assert (filename && filename[0] == '/');
    g_assert (node);
    
    parent = node;
    str = g_strdup (filename + strlen (NODE_NAME(parent)));
    
    if ((token = strtok_r (str, G_DIR_SEPARATOR_S, &lasts)) != NULL) {
        do {
            FN_W ("%s %s + %s\n", __func__, NODE_NAME(parent), token);
            child = children_find (parent, token);
            if (child) {
                parent = child;
            } else {
                if (op && op->add_missing) {
                    child = op->add_missing (parent, op->user_data);
                    goto L_hit;
                }
                break;
            }
        } while ((token = strtok_r (NULL, G_DIR_SEPARATOR_S, &lasts)) != NULL);
    } else {
        /* It's the head */
        g_assert (parent == _head);
        child = _head;
    }
    
    if (token == NULL && child) {
    L_hit:
        if (op && op->hit) {
            op->hit (child, op->user_data);
        }
    }
    g_free (str);
    return child;
}

node_t*
find_node (const gchar *filename)
{
    return find_node_internal (_head, filename, NULL);
}

node_t*
find_node_full (const gchar* filename, node_op_t* op)
{
    return find_node_internal (_head, filename, op);
}

node_t*
add_node (node_t* parent, const gchar* filename)
{
    gchar* str;
    gchar* token;
    gchar* lasts;
    node_t* child = NULL;

    g_assert (_head);
    g_assert (filename && filename[0] == '/');

    if (parent == NULL) {
        parent = _head;
    }
    
    str = g_strdup (filename + strlen (NODE_NAME(parent)));
    
    if ((token = strtok_r (str, G_DIR_SEPARATOR_S, &lasts)) != NULL) {
        do {
            FN_W ("%s %s + %s\n", __func__, NODE_NAME(parent), token);
            child = node_new (parent, token);
            if (child) {
                children_add (parent, child);
                parent = child;
            } else {
                break;
            }
        } while ((token = strtok_r (NULL, G_DIR_SEPARATOR_S, &lasts)) != NULL);
    }
    g_free (str);
    if (token == NULL) {
        return child;
    } else {
        return NULL;
    }
}

/**
 * delete recursively
 */
static gboolean
remove_children (node_t* node, node_op_t* op)
{
    FN_W ("%s 0x%p %s\n", __func__, node, NODE_NAME(node));
    if (children_num (node) > 0) {
        children_foreach_remove (node, children_remove_cb,
          (gpointer)op);
    }
    if (children_num (node) == 0) {
        return TRUE;
    }
    return FALSE;
}

static gboolean
remove_node_internal (node_t* node, node_op_t* op)
{
    node_t* parent = NULL;
    /*
     * If the parent is passive and doesn't have children, delete it.
     * NOTE node_delete_deep is a depth first delete recursively.
     * Top node is deleted in node_cancel_sub
     */
    g_assert (node);
    g_assert (op && op->pre_del);
    if (node != _head) {
        if (remove_children (node, op)) {
            if (node->user_data) {
                if (!op->pre_del (node, op->user_data)) {
                    return FALSE;
                }
            }
            parent = node->parent;
            children_remove (parent, node);
            node_delete (node);
            if (children_num (parent) == 0) {
                remove_node_internal (parent, op);
            }
            return TRUE;
        }
        return FALSE;
    }
    return TRUE;
}

void
pending_remove_node (node_t* node, node_op_t* op)
{
    struct _dnode* d;
    GList* l;
    
    for (l = deleting_nodes; l; l=l->next) {
        d = (struct _dnode*) l->data;
        if (g_ascii_strcasecmp (d->filename, NODE_NAME(node)) == 0) {
            return;
        }
    }
    
    d = _dnode_new (NODE_NAME(node), op);
    g_assert (d);
    deleting_nodes = g_list_prepend (deleting_nodes, d);
    if (deleting_nodes_id == 0) {
        deleting_nodes_id = g_timeout_add_seconds (PROCESS_DELETING_INTERVAL,
          scan_deleting_nodes,
          NULL);
        g_assert (deleting_nodes_id > 0);
    }
}

void
remove_node (node_t* node, node_op_t* op)
{
    remove_node_internal (node, op);
}

static node_t*
node_new (node_t* parent, const gchar* basename)
{
	node_t *f = NULL;

    g_assert (basename && basename[0]);
    if ((f = g_new0 (node_t, 1)) != NULL) {
        if (parent) {
            f->basename = g_strdup (basename);
            f->filename = g_build_filename (G_DIR_SEPARATOR_S,
              NODE_NAME(parent), basename, NULL);
        } else {
            f->basename = g_strdup (basename);
            f->filename = g_strdup (basename);
        }
        f->children = g_hash_table_new_full (g_str_hash, g_str_equal,
          NULL, (GDestroyNotify)node_delete);
        FN_W ("[ %s ] 0x%p %s\n", __func__, f, NODE_NAME(f));
    }
	return f;
}

static void
node_delete (node_t *f)
{
    FN_W ("[ %s ] 0x%p %s\n", __func__, f, NODE_NAME(f));
    g_assert (g_hash_table_size (f->children) == 0);
    g_assert (f->user_data == NULL);

    g_hash_table_unref (f->children);
    g_free (f->basename);
    g_free (f->filename);
    g_free (f);
}

static void
children_add (node_t *p, node_t *f)
{
    FN_W ("%s [p] %8s [c] %8s\n", __func__, p->basename, f->basename);
    g_hash_table_insert (p->children, f->basename, f);
    f->parent = p;
}

static void
children_remove (node_t *p, node_t *f)
{
    FN_W ("%s [p] %8s [c] %8s\n", __func__, p->basename, f->basename);
    g_hash_table_steal (p->children, f->basename);
    f->parent = NULL;
}

guint
children_num (node_t *f)
{
    return g_hash_table_size (f->children);
}

node_t *
children_find (node_t *f, const gchar *basename)
{
    return (node_t *) g_hash_table_lookup (f->children, (gpointer)basename);
}

/**
 * depth first delete recursively
 */
static gboolean
children_remove_cb (gpointer key,
  gpointer value,
  gpointer user_data)
{
    node_t* f = (node_t*)value;
    node_op_t* op = (node_op_t*) user_data;
    
    g_assert (f->parent);

    FN_W ("%s [p] %8s [c] %8s\n", __func__, f->parent->basename, f->basename);
    if (remove_children (f, op)) {
        if (f->user_data != NULL) {
            return op->pre_del (f, op->user_data);
        }
        return TRUE;
    }
    return FALSE;
}

static guint
children_foreach_remove (node_t *f, GHRFunc func, gpointer user_data)
{
    g_assert (f);
    
    return g_hash_table_foreach_remove (f->children, func, user_data);
}

static void
children_foreach (node_t *f, GHFunc func, gpointer user_data)
{
    g_assert (f);
    
    g_hash_table_foreach (f->children, func, user_data);
}

gboolean
node_class_init ()
{
    FN_W ("%s\n", __func__);
    if (_head == NULL) {
        _head = node_new (NULL, G_DIR_SEPARATOR_S);
    }
    return _head != NULL;
}
