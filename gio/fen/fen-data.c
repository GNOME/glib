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
#include <port.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <errno.h>
#include <glib.h>
#include "fen-data.h"
#include "fen-kernel.h"
#include "fen-missing.h"
#include "fen-dump.h"

#define	PROCESS_EVENTQ_TIME	10	/* in milliseconds */
#define PAIR_EVENTS_TIMEVAL	00000	/* in microseconds */
#define	PAIR_EVENTS_INC_TIMEVAL	0000	/* in microseconds */
#define SCAN_CHANGINGS_TIME	50	/* in milliseconds */
#define	SCAN_CHANGINGS_MAX_TIME	(4*100)	/* in milliseconds */
#define	SCAN_CHANGINGS_MIN_TIME	(4*100)	/* in milliseconds */
#define	INIT_CHANGES_NUM	2
#define	BASE_NUM	2

#ifdef GIO_COMPILATION
#define FD_W if (fd_debug_enabled) g_warning
static gboolean fd_debug_enabled = FALSE;
#else
#include "gam_error.h"
#define FD_W(...) GAM_DEBUG(DEBUG_INFO, __VA_ARGS__)
#endif

G_LOCK_EXTERN (fen_lock);
static GList *deleting_data = NULL;
static guint deleting_data_id = 0;

static void (*emit_once_cb) (fdata *f, int events, gpointer sub);
static void (*emit_cb) (fdata *f, int events);
static int (*_event_converter) (int event);

static gboolean fdata_delete (fdata* f);
static gint fdata_sub_find (gpointer a, gpointer b);
static void scan_children (node_t *f);
static void scan_known_children (node_t* f);

node_t*
_add_missing_cb (node_t* parent, gpointer user_data)
{
    g_assert (parent);
    FD_W ("%s p:0x%p %s\n", __func__, parent, (gchar*)user_data);
    return _add_node (parent, (gchar*)user_data);
}

gboolean
_pre_del_cb (node_t* node, gpointer user_data)
{
    fdata* data;
    
    g_assert (node);
    data = _node_get_data (node);
    FD_W ("%s node:0x%p %s\n", __func__, node, NODE_NAME(node));
    if (data != NULL) {
        if (!FN_IS_PASSIVE(data)) {
            return FALSE;
        }
        fdata_delete (data);
    }
    return TRUE;
}

static guint
_pow (guint x, guint y)
{
    guint z = 1;
    g_assert (x >= 0 && y >= 0);
    for (; y > 0; y--) {
        z *= x;
    }
    return z;
}

static guint
get_scalable_scan_time (fdata* data)
{
    guint sleep_time;
    /* Caculate from num = 0 */
    sleep_time = _pow (BASE_NUM, data->changed_event_num) * SCAN_CHANGINGS_TIME;
    if (sleep_time < SCAN_CHANGINGS_MIN_TIME) {
        sleep_time = SCAN_CHANGINGS_MIN_TIME;
    } else if (sleep_time > SCAN_CHANGINGS_MAX_TIME) {
        sleep_time = SCAN_CHANGINGS_MAX_TIME;
        data->change_update_id = INIT_CHANGES_NUM;
    }
    FD_W ("SCALABE SCAN num:time [ %4u : %4u ] %s\n", data->changed_event_num, sleep_time, FN_NAME(data));
    return sleep_time;
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

/*
 * If all active children nodes are ported, then cancel monitor the parent node
 *
 * Unsafe, need lock.
 */
static void
scan_known_children (node_t* f)
{
	GDir *dir;
	GError *err = NULL;
    fdata* pdata;
    
    FD_W ("%s %s [0x%p]\n", __func__, NODE_NAME(f), f);
    pdata = _node_get_data (f);
    /*
     * Currect fdata must is directly monitored. Be sure it is 1 level monitor.
     */
	dir = g_dir_open (NODE_NAME(f), 0, &err);
	if (dir) {
		const char *basename;
        
		while ((basename = g_dir_read_name (dir)))
		{
            node_t* childf = NULL;
            fdata* data;
			GList *idx;
			/*
             * If the node is existed, and isn't ported, then emit created
             * event. Ignore others.
             */
            childf = _children_find (f, basename);
            if (childf &&
              (data = _node_get_data (childf)) != NULL &&
              !FN_IS_PASSIVE (data)) {
                if (!is_monitoring (data) &&
                  _port_add (&data->fobj, &data->len, data)) {
                    _fdata_emit_events (data, FN_EVENT_CREATED);
                }
            }
        }
		g_dir_close (dir);
    } else {
        FD_W (err->message);
        g_error_free (err);
    }
}

static void
scan_children (node_t *f)
{
	GDir *dir;
	GError *err = NULL;
    fdata* pdata;
    
    FD_W ("%s %s [0x%p]\n", __func__, NODE_NAME(f), f);
    pdata = _node_get_data (f);
    /*
     * Currect fdata must is directly monitored. Be sure it is 1 level monitor.
     */
	dir = g_dir_open (NODE_NAME(f), 0, &err);
	if (dir) {
		const char *basename;
        
		while ((basename = g_dir_read_name (dir)))
		{
            node_t* childf = NULL;
            fdata* data;
			GList *idx;

            childf = _children_find (f, basename);
            if (childf == NULL) {
                gchar *filename;

                filename = g_build_filename (NODE_NAME(f), basename, NULL);
                childf = _add_node (f, filename);
                g_assert (childf);
                data = _fdata_new (childf, FALSE);
                g_free (filename);
            }
            if ((data = _node_get_data (childf)) == NULL) {
                data = _fdata_new (childf, FALSE);
            }
            /* Be sure data isn't ported and add to port successfully */
            /* Don't need delete it, it will be deleted by the parent */
            if (is_monitoring (data)) {
                /* Ignored */
            } else if (/* !_is_ported (data) && */
                _port_add (&data->fobj, &data->len, data)) {
                _fdata_emit_events (data, FN_EVENT_CREATED);
            }
        }
		g_dir_close (dir);
    } else {
        FD_W (err->message);
        g_error_free (err);
    }
}

static gboolean
scan_deleting_data (gpointer data)
{
    fdata *f;
    GList* i;
    GList* deleted_list = NULL;
    gboolean ret = TRUE;

    if (G_TRYLOCK (fen_lock)) {
        for (i = deleting_data; i; i = i->next) {
            f = (fdata*)i->data;
            if (fdata_delete (f)) {
                deleted_list = g_list_prepend (deleted_list, i);
            }
        }

        for (i = deleted_list; i; i = i->next) {
            deleting_data = g_list_remove_link (deleting_data,
              (GList *)i->data);
            g_list_free_1 ((GList *)i->data);
        }
        g_list_free (deleted_list);

        if (deleting_data == NULL) {
            deleting_data_id = 0;
            ret = FALSE;
        }
        G_UNLOCK (fen_lock);
    }
    return ret;
}

gboolean
is_monitoring (fdata* data)
{
    return _is_ported (data) || data->change_update_id > 0;
}

fdata*
_get_parent_data (fdata* data)
{
    if (FN_NODE(data) && !IS_TOPNODE(FN_NODE(data))) {
        return _node_get_data (FN_NODE(data)->parent);
    }
    return NULL;
}

node_t*
_get_parent_node (fdata* data)
{
    if (FN_NODE(data)) {
        return (FN_NODE(data)->parent);
    }
    return NULL;
}

fdata *
_fdata_new (node_t* node, gboolean is_mondir)
{
	fdata *f = NULL;

    g_assert (node);
	if ((f = g_new0 (fdata, 1)) != NULL) {
        FN_NODE(f) = node;
		FN_NAME(f) = g_strdup (NODE_NAME(node));
        f->is_dir = is_mondir;
        f->eventq = g_queue_new ();
        FD_W ("[ %s ] 0x%p %s\n", __func__, f, FN_NAME(f));
        _node_set_data (node, f);
	}
	return f;
}

static gboolean
fdata_delete (fdata *f)
{
    fnode_event_t *ev;

    FD_W ("[ TRY %s ] 0x%p id[%4d:%4d] %s\n", __func__, f, f->eventq_id, f->change_update_id, FN_NAME(f));
    g_assert (FN_IS_PASSIVE(f));

    _port_remove (f);
    /* _missing_remove (f); */

    if (f->node != NULL) {
        _node_set_data (f->node, NULL);
        f->node = NULL;
    }

    if (f->change_update_id > 0 || f->eventq_id > 0) {
        if (FN_IS_LIVING(f)) {
            f->is_cancelled = TRUE;
            deleting_data = g_list_prepend (deleting_data, f);
            if (deleting_data_id == 0) {
                deleting_data_id = g_idle_add (scan_deleting_data, NULL);
                g_assert (deleting_data_id > 0);
            }
        }
        return FALSE;
    }
    FD_W ("[ %s ] 0x%p %s\n", __func__, f, FN_NAME(f));

    while ((ev = g_queue_pop_head (f->eventq)) != NULL) {
        _fnode_event_delete (ev);
    }

    g_queue_free (f->eventq);
    g_free (FN_NAME(f));
    g_free (f);
    return TRUE;
}

void
_fdata_reset (fdata* data)
{
    fnode_event_t *ev;

    g_assert (data);

    while ((ev = g_queue_pop_head (data->eventq)) != NULL) {
        _fnode_event_delete (ev);
    }
}

static gint
fdata_sub_find (gpointer a, gpointer b)
{
    if (a != b) {
        return 1;
    } else {
        return 0;
    }
}

void
_fdata_sub_add (fdata *f, gpointer sub)
{
    FD_W ("[%s] [data: 0x%p ] [s: 0x%p ] %s\n", __func__, f, sub, FN_NAME(f));
    g_assert (g_list_find_custom (f->subs, sub, (GCompareFunc)fdata_sub_find) == NULL);
    f->subs = g_list_prepend (f->subs, sub);
}

void
_fdata_sub_remove (fdata *f, gpointer sub)
{
    GList *l;
    FD_W ("[%s] [data: 0x%p ] [s: 0x%p ] %s\n", __func__, f, sub, FN_NAME(f));
    g_assert (g_list_find_custom (f->subs, sub, (GCompareFunc)fdata_sub_find) != NULL);
    l = g_list_find_custom (f->subs, sub, (GCompareFunc)fdata_sub_find);
    g_assert (l);
    g_assert (sub == l->data);
    f->subs = g_list_delete_link (f->subs, l);
}

/*
 * Adjust self on failing to Port
 */
void
_fdata_adjust_deleted (fdata* f)
{
    node_t* parent;
    fdata* pdata;
    node_op_t op = {NULL, NULL, _pre_del_cb, NULL};

    /*
     * It's a top node. We move it to missing list.
     */
    parent = _get_parent_node (f);
    pdata = _get_parent_data (f);
    if (!FN_IS_PASSIVE(f) ||
      _children_num (FN_NODE(f)) > 0 ||
      (pdata && !FN_IS_PASSIVE(pdata))) {
        if (parent) {
            if (pdata == NULL) {
                pdata = _fdata_new (parent, FALSE);
            }
            g_assert (pdata);
            if (!_port_add (&pdata->fobj, &pdata->len, pdata)) {
                _fdata_adjust_deleted (pdata);
            }
        } else {
            /* f is root */
            g_assert (IS_TOPNODE(FN_NODE(f)));
            _missing_add (f);
        }
    } else {
#ifdef GIO_COMPILATION
        _pending_remove_node (FN_NODE(f), &op);
#else
        _remove_node (FN_NODE(f), &op);
#endif
    }
}

static gboolean
fdata_adjust_changed (fdata *f)
{
    fnode_event_t *ev;
    struct stat buf;
    node_t* parent;
    fdata* pdata;

    G_LOCK (fen_lock);
    parent = _get_parent_node (f);
    pdata = _get_parent_data (f);

    if (!FN_IS_LIVING(f) ||
      (_children_num (FN_NODE(f)) == 0 &&
        FN_IS_PASSIVE(f) &&
        pdata && FN_IS_PASSIVE(pdata))) {
        f->change_update_id = 0;
        G_UNLOCK (fen_lock);
        return FALSE;
    }

    FD_W ("[ %s ] %s\n", __func__, FN_NAME(f));
    if (FN_STAT (FN_NAME(f), &buf) != 0) {
        FD_W ("LSTAT [%-20s] %s\n", FN_NAME(f), g_strerror (errno));
        goto L_delete;
    }
    f->is_dir = S_ISDIR (buf.st_mode) ? TRUE : FALSE;
    if (f->len != buf.st_size) {
        /* FD_W ("LEN [%lld:%lld] %s\n", f->len, buf.st_size, FN_NAME(f)); */
        f->len = buf.st_size;
        ev = _fnode_event_new (FILE_MODIFIED, TRUE, f);
        if (ev != NULL) {
            ev->is_pending = TRUE;
            _fdata_add_event (f, ev);
        }
        /* Fdata is still changing, so scalable scan */
        f->change_update_id = g_timeout_add (get_scalable_scan_time (f),
          (GSourceFunc)fdata_adjust_changed,
          (gpointer)f);
        G_UNLOCK (fen_lock);
        return FALSE;
    } else {
        f->changed_event_num = 0;
        f->fobj.fo_atime = buf.st_atim;
        f->fobj.fo_mtime = buf.st_mtim;
        f->fobj.fo_ctime = buf.st_ctim;
        if (FN_IS_DIR(f)) {
            if (FN_IS_MONDIR(f)) {
                scan_children (FN_NODE(f));
            } else {
                scan_known_children (FN_NODE(f));
                if ((_children_num (FN_NODE(f)) == 0 &&
                      FN_IS_PASSIVE(f) &&
                      pdata && FN_IS_PASSIVE(pdata))) {
                    _port_remove (f);
                    goto L_exit;
                }
            }
        }
        if (!_port_add_simple (&f->fobj, f)) {
        L_delete:
            ev = _fnode_event_new (FILE_DELETE, FALSE, f);
            if (ev != NULL) {
                _fdata_add_event (f, ev);
            }
        }
    }
L_exit:
    f->change_update_id = 0;
    G_UNLOCK (fen_lock);
    return FALSE;
}

void
_fdata_emit_events_once (fdata *f, int event, gpointer sub)
{
    emit_once_cb (f, _event_converter (event), sub);
}

void
_fdata_emit_events (fdata *f, int event)
{
    emit_cb (f, _event_converter (event));
}

static gboolean
process_events (gpointer udata)
{
    node_op_t op = {NULL, NULL, _pre_del_cb, NULL};
    fdata* f;
    fnode_event_t* ev;
    int e;

    /* FD_W ("IN <======== %s\n", __func__); */

    f = (fdata*)udata;
    FD_W ("%s 0x%p id:%-4d %s\n", __func__, f, f->eventq_id, FN_NAME(f));
    
    G_LOCK (fen_lock);

    if (!FN_IS_LIVING(f)) {
        f->eventq_id = 0;
        G_UNLOCK (fen_lock);
        return FALSE;
    }
    
    if ((ev = (fnode_event_t*)g_queue_pop_head (f->eventq)) != NULL) {
        /* Send events to clients. */
        e = ev->e;
        if (!ev->is_pending) {
#ifdef GIO_COMPILATION
            if (ev->has_twin) {
                _fdata_emit_events (f, FILE_ATTRIB);
            }
#endif
            _fdata_emit_events (f, ev->e);
        }
        
        _fnode_event_delete (ev);
        ev = NULL;

        /* Adjust node state. */
        /*
         * Node the node has been created, so we can delete create event in
         * optimizing. To reduce the statings, we add it to Port on discoving
         * it then emit CREATED event. So we don't need to do anything here.
         */
        switch (e) {
        case FILE_MODIFIED:
        case MOUNTEDOVER:
        case UNMOUNTED:
            /* If the event is a changed event, then pending process it */
            if (f->change_update_id == 0) {
                f->change_update_id = g_timeout_add (get_scalable_scan_time(f),
                  (GSourceFunc)fdata_adjust_changed,
                  (gpointer)f);
                g_assert (f->change_update_id > 0);
            }
            break;
        case FILE_ATTRIB:
            g_assert (f->change_update_id == 0);
            if (!_port_add (&f->fobj, &f->len, f)) {
                ev = _fnode_event_new (FILE_DELETE, FALSE, f);
                if (ev != NULL) {
                    _fdata_add_event (f, ev);
                }
            }
            break;
        case FILE_DELETE: /* Ignored */
            break;
        default:
            g_assert_not_reached ();
            break;
        }
        /* Process one event a time */
        G_UNLOCK (fen_lock); 
        return TRUE;
    }
    f->eventq_id = 0;
    G_UNLOCK (fen_lock); 
    /* FD_W ("OUT ========> %s\n", __func__); */
    return FALSE;
}

void
_fdata_add_event (fdata *f, fnode_event_t *ev)
{
    node_op_t op = {NULL, NULL, _pre_del_cb, NULL};
    fnode_event_t *tail;

    if (!FN_IS_LIVING(f)) {
        _fnode_event_delete (ev);
        return;
    }
    
    FD_W ("%s %d\n", __func__, ev->e);
    g_get_current_time (&ev->t);
    /*
     * If created/deleted events of child node happened, then we use parent
     * event queue to handle.
     * If child node emits deleted event, it seems no changes for the parent
     * node, but the attr is changed. So we may try to cancel processing the
     * coming changed events of the parent node.
     */
    tail = (fnode_event_t*)g_queue_peek_tail (f->eventq);
    switch (ev->e) {
    case FILE_RENAME_FROM:
    case FILE_RENAME_TO:
    case FILE_ACCESS:
        _fnode_event_delete (ev);
        g_assert_not_reached ();
        return;
    case FILE_DELETE:
        /* clear changed event number */
        f->changed_event_num = 0;
        /*
         * We will cancel all previous events.
         */
        if (tail) {
            g_queue_pop_tail (f->eventq);
            do {
                _fnode_event_delete (tail);
            } while ((tail = (fnode_event_t*)g_queue_pop_tail (f->eventq)) != NULL);
        }
        /*
         * Given a node "f" is deleted, process it ASAP.
         */
        _fdata_emit_events (f, ev->e);
        _fnode_event_delete (ev);
        _fdata_adjust_deleted (f);
        return;
    case FILE_MODIFIED:
    case UNMOUNTED:
    case MOUNTEDOVER:
        /* clear changed event number */
        f->changed_event_num ++;
    case FILE_ATTRIB:
    default:
        /*
         * If in the time range, we will try optimizing
         * (changed+) to (changed)
         * (attrchanged changed) to ([changed, attrchanged])
         * (event attrchanged) to ([event, attrchanged])
         */
        if (tail) {
            do {
                if (tail->e == ev->e) {
                    if (g_timeval_lt (&ev->t, &tail->t)) {
                        g_queue_peek_tail (f->eventq);
                        /* Add the increment */
                        g_time_val_add (&ev->t, PAIR_EVENTS_INC_TIMEVAL);
                        /* skip the previous event */
                        FD_W ("SKIPPED -- %s\n", _event_string (tail->e));
                        _fnode_event_delete (tail);
                    } else {
                        break;
                    }
                } else if (ev->e == FILE_MODIFIED && tail->e == FILE_ATTRIB) {
                    ev->has_twin = TRUE;
                    _fnode_event_delete (tail);
                } else if (ev->e == FILE_ATTRIB && f->change_update_id > 0) {
                    tail->has_twin = TRUE;
                    /* skip the current event */
                    _fnode_event_delete (ev);
                    return;
                } else {
                    break;
                }
            } while ((tail = (fnode_event_t*)g_queue_peek_tail (f->eventq)) != NULL);
        }
    }

    /* must add the threshold time */
    g_time_val_add (&ev->t, PAIR_EVENTS_TIMEVAL);
    
    g_queue_push_tail (f->eventq, ev);

    /* starting process_events */
    if (f->eventq_id == 0) {
        f->eventq_id = g_timeout_add (PROCESS_EVENTQ_TIME,
          process_events,
          (gpointer)f);
        g_assert (f->eventq_id > 0);
    }
    FD_W ("%s 0x%p id:%-4d %s\n", __func__, f, f->eventq_id, FN_NAME(f));
}

gboolean
_fdata_class_init (void (*user_emit_cb) (fdata*, int),
  void (*user_emit_once_cb) (fdata*, int,  gpointer),
  int (*user_event_converter) (int event))
{
    FD_W ("%s\n", __func__);
    if (user_emit_cb == NULL) {
        return FALSE;
    }
    if (user_emit_once_cb == NULL) {
        return FALSE;
    }
    if (user_event_converter == NULL) {
        return FALSE;
    }
    emit_cb = user_emit_cb;
    emit_once_cb = user_emit_once_cb;
    _event_converter = user_event_converter;
    
    if (!_port_class_init (_fdata_add_event)) {
        FD_W ("_port_class_init failed.");
        return FALSE;
    }
    return TRUE;
}
