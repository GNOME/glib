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
#include <glib.h>
#include <glib/gprintf.h>
#include "fen-node.h"
#include "fen-data.h"
#include "fen-kernel.h"
#include "fen-missing.h"
#include "fen-dump.h"

G_LOCK_EXTERN (fen_lock);

/*-------------------- node ------------------*/
static void
dump_node (node_t* node, gpointer data)
{
    if (data && node->user_data) {
        return;
    }
    g_printf ("[%s] < 0x%p : 0x%p > %s\n", __func__, node, node->user_data, NODE_NAME(node));
}

static gboolean
dump_node_tree (node_t* node, gpointer user_data)
{
    node_op_t op = {dump_node, NULL, NULL, user_data};
    GList* children;
    GList* i;
    if (G_TRYLOCK (fen_lock)) {
        if (node) {
            _travel_nodes (node, &op);
        }
        G_UNLOCK (fen_lock);
    }
    return TRUE;
}

/* ------------------ fdata port hash --------------------*/
void
dump_hash_cb (gpointer key,
  gpointer value,
  gpointer user_data)
{
    g_printf ("[%s] < 0x%p : 0x%p >\n", __func__, key, value);
}

gboolean
dump_hash (GHashTable* hash, gpointer user_data)
{
    if (G_TRYLOCK (fen_lock)) {
        if (g_hash_table_size (hash) > 0) {
            g_hash_table_foreach (hash, dump_hash_cb, user_data);
        }
        G_UNLOCK (fen_lock);
    }
    return TRUE;
}

/* ------------------ event --------------------*/
void
dump_event (fnode_event_t* ev, gpointer user_data)
{
    fdata* data = ev->user_data;
    g_printf ("[%s] < 0x%p : 0x%p > [ %10s ] %s\n", __func__, ev, ev->user_data, _event_string (ev->e), FN_NAME(data));
}

void
dump_event_queue (fdata* data, gpointer user_data)
{
    if (G_TRYLOCK (fen_lock)) {
        if (data->eventq) {
            g_queue_foreach (data->eventq, (GFunc)dump_event, user_data);
        }
        G_UNLOCK (fen_lock);
    }
}

