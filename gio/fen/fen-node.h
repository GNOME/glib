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

#ifndef _FEN_NODE_H_
#define _FEN_NODE_H_

typedef struct node node_t;

struct node
{
    gchar *filename;
    gchar *basename;
    gint stat;
    
	/* the parent and children of node */
    node_t *parent;
    GHashTable *children; /* children in basename */

    gpointer user_data;
};

#define	IS_TOPNODE(fp)	(((node_t *)(fp))->parent == NULL)
#define NODE_NAME(fp)	(((node_t *)(fp))->filename)

typedef struct node_op
{
    /* find */
    void (*hit) (node_t* node, gpointer user_data);
    node_t* (*add_missing) (node_t* parent, gpointer user_data);
    /* delete */
    gboolean (*pre_del) (node_t* node, gpointer user_data);
	/* data */
    gpointer user_data;
} node_op_t;

node_t* _add_node (node_t* parent, const gchar* filename);
void _remove_node (node_t* node, node_op_t* op);
void _pending_remove_node (node_t* node, node_op_t* op);

void _travel_nodes (node_t* node, node_op_t* op);
node_t* _find_node_full (const gchar* filename, node_op_t* op);
node_t* _find_node (const gchar *filename);

node_t* _children_find (node_t *f, const gchar *basename);
guint _children_num (node_t *f);

gpointer _node_get_data (node_t* node);
gpointer _node_set_data (node_t* node, gpointer user_data);

gboolean _node_class_init ();

#endif /* _FEN_NODE_H_ */
