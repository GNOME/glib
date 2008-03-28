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

#include <port.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifndef _FEN_KERNEL_H_
#define _FEN_KERNEL_H_

#define FN_STAT	lstat

typedef struct fnode_event
{
    int e;
    gboolean has_twin;
    gboolean is_pending;
    gpointer user_data;
    GTimeVal t;
} fnode_event_t;

gboolean port_add (file_obj_t* fobj, off_t* len, gpointer f);
gboolean port_add_simple (file_obj_t* fobj, gpointer f);
void port_remove (gpointer f);
gboolean is_ported (gpointer f);

fnode_event_t* fnode_event_new (int event, gboolean has_twin, gpointer user_data);
void fnode_event_delete (fnode_event_t* ev);
const gchar * _event_string (int event);

extern gboolean port_class_init ();

#endif /* _FEN_KERNEL_H_ */
