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
#include "fen-data.h"
#include "fen-missing.h"

G_LOCK_EXTERN (fen_lock);
#define SCAN_MISSING_INTERVAL 4000	/* in milliseconds */

#ifdef GIO_COMPILATION
#define FM_W if (fm_debug_enabled) g_warning
gboolean fm_debug_enabled = FALSE;
#else
#include "gam_error.h"
#define FM_W(...) GAM_DEBUG(DEBUG_INFO, __VA_ARGS__)
#endif

/* global data structure for scan missing files */
static GList *missing_list = NULL;
static guint scan_missing_source_id = 0;

static gboolean scan_missing_list (gpointer data);

static gboolean
scan_missing_list (gpointer data)
{
    GList *existing_list = NULL;
    GList *idx = NULL;
    fdata *f;
    gboolean ret = TRUE;

    G_LOCK (fen_lock);
    
    for (idx = missing_list; idx; idx = idx->next) {
        f = (fdata*)idx->data;
        
        if (_port_add (&f->fobj, &f->len, f)) {
            /* TODO - emit CREATE event */
            _fdata_emit_events (f, FN_EVENT_CREATED);
            existing_list = g_list_prepend (existing_list, idx);
        }
    }
    
    for (idx = existing_list; idx; idx = idx->next) {
        missing_list = g_list_remove_link (missing_list, (GList *)idx->data);
        g_list_free_1 ((GList *)idx->data);
    }
    g_list_free (existing_list);

    if (missing_list == NULL) {
        scan_missing_source_id = 0;
        ret = FALSE;
    }

    G_UNLOCK (fen_lock);
    return ret;
}

/**
 * missing_add
 *
 * Unsafe, need lock fen_lock.
 */
void
_missing_add (fdata *f)
{
    GList *idx;
    
    g_assert (!_is_ported (f));

    if (g_list_find (missing_list, f) != NULL) {
        FM_W ("%s is ALREADY added %s\n", __func__, FN_NAME(f));
        return;
    }
    FM_W ("%s is added %s\n", __func__, FN_NAME(f));
    
    missing_list = g_list_prepend (missing_list, f);
    
    /* if doesn't scan, then start */
    if (scan_missing_source_id == 0) {
        scan_missing_source_id = g_timeout_add (SCAN_MISSING_INTERVAL,
          scan_missing_list,
          NULL);
        g_assert (scan_missing_source_id > 0);
    }
}

/**
 * missing_remove
 *
 * Unsafe, need lock fen_lock.
 */
void
_missing_remove (fdata *f)
{
    FM_W ("%s %s\n", __func__, FN_NAME(f));
    missing_list = g_list_remove (missing_list, f);
}
