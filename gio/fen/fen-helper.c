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
#include "fen-helper.h"
#include "fen-kernel.h"
#ifdef GIO_COMPILATION
#include "gfile.h"
#include "gfilemonitor.h"
#else
#include "gam_event.h"
#include "gam_server.h"
#include "gam_protocol.h"
#endif

#ifdef GIO_COMPILATION
#define FH_W if (fh_debug_enabled) g_warning
static gboolean fh_debug_enabled = FALSE;
#else
#include "gam_error.h"
#define FH_W(...) GAM_DEBUG(DEBUG_INFO, __VA_ARGS__)
#endif

G_LOCK_EXTERN (fen_lock);

static void default_emit_event_cb (fdata *f, int events);
static void default_emit_once_event_cb (fdata *f, int events, gpointer sub);
static int default_event_converter (int event);

static void
scan_children_init (node_t *f, gpointer sub)
{
	GDir *dir;
	GError *err = NULL;
    node_op_t op = {NULL, NULL, _pre_del_cb, NULL};
    fdata* pdata;
    
    FH_W ("%s %s [0x%p]\n", __func__, NODE_NAME(f), f);
    pdata = _node_get_data (f);

    dir = g_dir_open (NODE_NAME(f), 0, &err);
    if (dir) {
        const char *basename;
        
        while ((basename = g_dir_read_name (dir)))
        {
            node_t *childf = NULL;
            fdata* data;
            GList *idx;

            childf = _children_find (f, basename);
            if (childf == NULL) {
                gchar *filename;
            
                filename = g_build_filename (NODE_NAME(f), basename, NULL);
                childf = _add_node (f, filename);
                g_assert (childf);
                g_free (filename);
            }
            if ((data = _node_get_data (childf)) == NULL) {
                data = _fdata_new (childf, FALSE);
            }
            
            if (is_monitoring (data)) {
                /* Ignored */
            } else if (/* !_is_ported (data) && */
                _port_add (&data->fobj, &data->len, data)) {
                /* Emit created to all other subs */
                _fdata_emit_events (data, FN_EVENT_CREATED);
            }
            /* Emit created to the new sub */
#ifdef GIO_COMPILATION
            /* _fdata_emit_events_once (data, FN_EVENT_CREATED, sub); */
#else
            gam_server_emit_one_event (NODE_NAME(childf),
              gam_subscription_is_dir (sub), GAMIN_EVENT_EXISTS, sub, 1);
#endif
        }
        g_dir_close (dir);
    } else {
        FH_W (err->message);
        g_error_free (err);
    }
}

/**
 * _fen_add
 * 
 * Won't hold a ref, we have a timout callback to clean unused fdata.
 * If there is no value for a key, add it and return it; else return the old
 * one.
 */
void
_fen_add (const gchar *filename, gpointer sub, gboolean is_mondir)
{
    node_op_t op = {NULL, _add_missing_cb, _pre_del_cb, (gpointer)filename};
	node_t* f;
    fdata* data;
    
    g_assert (filename);
    g_assert (sub);

    G_LOCK (fen_lock);
	f = _find_node_full (filename, &op);
    FH_W ("[ %s ] f[0x%p] sub[0x%p] %s\n", __func__, f, sub, filename);
    g_assert (f);
    data = _node_get_data (f);
    if (data == NULL) {
        data = _fdata_new (f, is_mondir);
    }

    if (is_mondir) {
        data->mon_dir_num ++;
    }
    
    /* Change to active */
#ifdef GIO_COMPILATION
    if (_port_add (&data->fobj, &data->len, data) ||
      g_file_test (FN_NAME(data), G_FILE_TEST_EXISTS)) {
        if (is_mondir) {
            scan_children_init (f, sub);
        }
        _fdata_sub_add (data, sub);
    } else {
        _fdata_sub_add (data, sub);
        _fdata_adjust_deleted (data);
    }
#else
    if (_port_add (&data->fobj, &data->len, data) ||
      g_file_test (FN_NAME(data), G_FILE_TEST_EXISTS)) {
        gam_server_emit_one_event (FN_NAME(data),
          gam_subscription_is_dir (sub), GAMIN_EVENT_EXISTS, sub, 1);
        if (is_mondir) {
            scan_children_init (f, sub);
        }
        gam_server_emit_one_event (FN_NAME(data),
          gam_subscription_is_dir (sub), GAMIN_EVENT_ENDEXISTS, sub, 1);
        _fdata_sub_add (data, sub);
    } else {
        _fdata_sub_add (data, sub);
        gam_server_emit_one_event (FN_NAME(data),
          gam_subscription_is_dir (sub), GAMIN_EVENT_DELETED, sub, 1);
        _fdata_adjust_deleted (data);
        gam_server_emit_one_event (FN_NAME(data),
          gam_subscription_is_dir (sub), GAMIN_EVENT_ENDEXISTS, sub, 1);
    }
#endif
    G_UNLOCK (fen_lock);
}

void
_fen_remove (const gchar *filename, gpointer sub, gboolean is_mondir)
{
    node_op_t op = {NULL, _add_missing_cb, _pre_del_cb, (gpointer)filename};
    node_t* f;
    fdata* data;
    
    g_assert (filename);
    g_assert (sub);

    G_LOCK (fen_lock);
	f = _find_node (filename);
    FH_W ("[ %s ] f[0x%p] sub[0x%p] %s\n", __func__, f, sub, filename);

    g_assert (f);
    data = _node_get_data (f);
    g_assert (data);
    
    if (is_mondir) {
        data->mon_dir_num --;
    }
    _fdata_sub_remove (data, sub);
    if (FN_IS_PASSIVE(data)) {
#ifdef GIO_COMPILATION
        _pending_remove_node (f, &op);
#else
        _remove_node (f, &op);
#endif
    }
    G_UNLOCK (fen_lock);
}

static gboolean
fen_init_once_func (gpointer data)
{
    FH_W ("%s\n", __func__);
    if (!_node_class_init ()) {
        FH_W ("_node_class_init failed.");
        return FALSE;
    }
    if (!_fdata_class_init (default_emit_event_cb,
          default_emit_once_event_cb,
          default_event_converter)) {
        FH_W ("_fdata_class_init failed.");
        return FALSE;
    }
    return TRUE;
}

gboolean
_fen_init ()
{
#ifdef GIO_COMPILATION
    static GOnce fen_init_once = G_ONCE_INIT;
    g_once (&fen_init_once, (GThreadFunc)fen_init_once_func, NULL);
    return (gboolean)fen_init_once.retval;
#else
    return fen_init_once_func (NULL);
#endif
}

static void
default_emit_once_event_cb (fdata *f, int events, gpointer sub)
{
#ifdef GIO_COMPILATION
    GFile* child;
    fen_sub* _sub = (fen_sub*)sub;
    child = g_file_new_for_path (FN_NAME(f));
    g_file_monitor_emit_event (G_FILE_MONITOR (_sub->user_data),
      child, NULL, events);
    g_object_unref (child);
#else
    gam_server_emit_one_event (FN_NAME(f),
      gam_subscription_is_dir (sub), events, sub, 1);
#endif
}

static void
default_emit_event_cb (fdata *f, int events)
{
    GList* i;
    fdata* pdata;
    
#ifdef GIO_COMPILATION
    GFile* child;
    child = g_file_new_for_path (FN_NAME(f));
    for (i = f->subs; i; i = i->next) {
        fen_sub* sub = (fen_sub*)i->data;
        gboolean file_is_dir = sub->is_mondir;
        if ((events != G_FILE_MONITOR_EVENT_CHANGED &&
              events != G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED) ||
          !file_is_dir) {
            g_file_monitor_emit_event (G_FILE_MONITOR (sub->user_data),
              child, NULL, events);
        }
    }
    if ((pdata = _get_parent_data (f)) != NULL) {
        for (i = pdata->subs; i; i = i->next) {
            fen_sub* sub = (fen_sub*)i->data;
            gboolean file_is_dir = sub->is_mondir;
            g_file_monitor_emit_event (G_FILE_MONITOR (sub->user_data),
              child, NULL, events);
        }
    }
    g_object_unref (child);
#else
    for (i = f->subs; i; i = i->next) {
        gboolean file_is_dir = gam_subscription_is_dir (i->data);
        if (events != GAMIN_EVENT_CHANGED || !file_is_dir) {
            gam_server_emit_one_event (FN_NAME(f), file_is_dir, events, i->data, 1);
        }
    }
    if ((pdata = _get_parent_data (f)) != NULL) {
        for (i = pdata->subs; i; i = i->next) {
            gboolean file_is_dir = gam_subscription_is_dir (i->data);
            gam_server_emit_one_event (FN_NAME(f), file_is_dir, events, i->data, 1);
        }
    }
#endif
}

static int
default_event_converter (int event)
{
#ifdef GIO_COMPILATION
    switch (event) {
    case FN_EVENT_CREATED:
        return G_FILE_MONITOR_EVENT_CREATED;
    case FILE_DELETE:
    case FILE_RENAME_FROM:
        return G_FILE_MONITOR_EVENT_DELETED;
    case UNMOUNTED:
        return G_FILE_MONITOR_EVENT_UNMOUNTED;
    case FILE_ATTRIB:
        return G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED;
    case MOUNTEDOVER:
    case FILE_MODIFIED:
    case FILE_RENAME_TO:
        return G_FILE_MONITOR_EVENT_CHANGED;
    default:
        /* case FILE_ACCESS: */
        g_assert_not_reached ();
        return -1;
    }
#else
    switch (event) {
    case FN_EVENT_CREATED:
        return GAMIN_EVENT_CREATED;
    case FILE_DELETE:
    case FILE_RENAME_FROM:
        return GAMIN_EVENT_DELETED;
    case FILE_ATTRIB:
    case MOUNTEDOVER:
    case UNMOUNTED:
    case FILE_MODIFIED:
    case FILE_RENAME_TO:
        return GAMIN_EVENT_CHANGED;
    default:
        /* case FILE_ACCESS: */
        g_assert_not_reached ();
        return -1;
    }
#endif
}
