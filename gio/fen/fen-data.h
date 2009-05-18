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

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "fen-node.h"
#include "fen-kernel.h"

#ifndef _FEN_DATA_H_
#define _FEN_DATA_H_

#define FN_EVENT_CREATED	0
#define FN_NAME(fp)	(((fdata*)(fp))->fobj.fo_name)
#define FN_NODE(fp)	(((fdata*)(fp))->node)
#define	FN_IS_DIR(fp)	(((fdata*)(fp))->is_dir)
#define	FN_IS_PASSIVE(fp)	(((fdata*)(fp))->subs == NULL)
#define	FN_IS_MONDIR(fp)	(((fdata*)(fp))->mon_dir_num > 0)
#define	FN_IS_LIVING(fp)	(!((fdata*)(fp))->is_cancelled)

typedef struct
{
	file_obj_t fobj;
    off_t len;
    gboolean is_cancelled;

    node_t* node;
	/* to identify if the path is dir */
	gboolean is_dir;
    guint mon_dir_num;

	/* List of subscriptions monitoring this fdata/path */
	GList *subs;

    /* prcessed changed events num */
    guint changed_event_num;
    
    /* process events source id */
    GQueue* eventq;
    guint eventq_id;
    guint change_update_id;
} fdata;

/* fdata functions */
fdata* _fdata_new (node_t* node, gboolean is_mondir);
void _fdata_reset (fdata* data);
void _fdata_emit_events_once (fdata *f, int event, gpointer sub);
void _fdata_emit_events (fdata *f, int event);
void _fdata_add_event (fdata *f, fnode_event_t *ev);
void _fdata_adjust_deleted (fdata *f);
fdata* _get_parent_data (fdata* data);
node_t* _get_parent_node (fdata* data);
gboolean _is_monitoring (fdata* data);

/* sub */
void _fdata_sub_add (fdata *f, gpointer sub);
void _fdata_sub_remove (fdata *f, gpointer sub);

/* misc */
node_t* _add_missing_cb (node_t* parent, gpointer user_data);
gboolean _pre_del_cb (node_t* node, gpointer user_data);

/* init */
gboolean _fdata_class_init (void (*user_emit_cb) (fdata*, int),
  void (*user_emit_once_cb) (fdata*, int,  gpointer),
  int (*user_event_converter) (int event));

#endif /* _FEN_DATA_H_ */
