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
#include <rctl.h>
#include <strings.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <glib.h>
#include "fen-kernel.h"
#include "fen-dump.h"

#ifdef GIO_COMPILATION
#define FK_W if (fk_debug_enabled) g_warning
static gboolean fk_debug_enabled = FALSE;
#else
#include "gam_error.h"
#define FK_W(...) GAM_DEBUG(DEBUG_INFO, __VA_ARGS__)
#endif

G_GNUC_INTERNAL G_LOCK_DEFINE (fen_lock);
#define PE_ALLOC	64
#define F_PORT(pfo)		(((_f *)(pfo))->port->port)
#define F_NAME(pfo)		(((_f *)(pfo))->fobj->fo_name)
#define FEN_ALL_EVENTS	(FILE_MODIFIED | FILE_ATTRIB | FILE_NOFOLLOW)
#define FEN_IGNORE_EVENTS	(FILE_ACCESS)
#define PROCESS_PORT_EVENTS_TIME	400	/* in milliseconds */

static GHashTable *_obj_fen_hash = NULL;	/* <user_data, port> */
static ulong max_port_events = 512;
static GList *pn_vq;	/* the queue of ports which don't have the max objs */
static GList *pn_fq;	/* the queue of ports which have the max objs */
static GQueue *g_eventq = NULL;
static void (*add_event_cb) (gpointer, fnode_event_t*);

typedef struct pnode
{
	long ref;	/* how many fds are associated to this port */
	int port;
    guint port_source_id;
} pnode_t;

typedef struct {
    pnode_t*	port;
    file_obj_t*	fobj;

    gboolean	is_active;
    gpointer	user_data;
} _f;

static gboolean port_fetch_event_cb (void *arg);
static pnode_t *pnode_new ();
static void pnode_delete (pnode_t *pn);

gboolean
is_ported (gpointer f)
{
    _f* fo = g_hash_table_lookup (_obj_fen_hash, f);
    
    if (fo) {
        return fo->is_active;
    }
    return FALSE;
}

static gchar*
printevent (const char *pname, int event, const char *tag)
{
    static gchar	*event_string = NULL;
    GString			*str;

    if (event_string) {
        g_free(event_string);
    }

    str = g_string_new ("");
    g_string_printf (str, "[%s] [%-20s]", tag, pname);
    if (event & FILE_ACCESS) {
        str = g_string_append (str, " ACCESS");
    }
    if (event & FILE_MODIFIED) {
        str = g_string_append (str, " MODIFIED");
    }
    if (event & FILE_ATTRIB) {
        str = g_string_append (str, " ATTRIB");
    }
    if (event & FILE_DELETE) {
        str = g_string_append (str, " DELETE");
    }
    if (event & FILE_RENAME_TO) {
        str = g_string_append (str, " RENAME_TO");
    }
    if (event & FILE_RENAME_FROM) {
        str = g_string_append (str, " RENAME_FROM");
    }
    if (event & UNMOUNTED) {
        str = g_string_append (str, " UNMOUNTED");
    }
    if (event & MOUNTEDOVER) {
        str = g_string_append (str, " MOUNTEDOVER");
    }
    event_string = str->str;
    g_string_free (str, FALSE);
    return event_string;
}

static void
port_add_kevent (int e, gpointer f)
{
    fnode_event_t *ev, *tail;
    GTimeVal t;
    gboolean has_twin = FALSE;
    
    /*
     * Child FILE_DELETE | FILE_RENAME_FROM will trigger parent FILE_MODIFIED.
     * FILE_MODIFIED will trigger FILE_ATTRIB.
     */

    if ((e & FILE_ATTRIB) && e != FILE_ATTRIB) {
        e ^= FILE_ATTRIB;
        has_twin = TRUE;
    }
    if (e == FILE_RENAME_FROM) {
        e = FILE_DELETE;
    }
    if (e == FILE_RENAME_TO) {
        e = FILE_MODIFIED;
    }
    
    switch (e) {
    case FILE_DELETE:
    case FILE_RENAME_FROM:
    case FILE_MODIFIED:
    case FILE_ATTRIB:
    case UNMOUNTED:
    case MOUNTEDOVER:
        break;
    case FILE_RENAME_TO:
    case FILE_ACCESS:
    default:
        g_assert_not_reached ();
        return;
    }

    tail = (fnode_event_t*) g_queue_peek_tail (g_eventq);
    if (tail) {
        if (tail->user_data == f) {
            if (tail->e == e) {
                tail->has_twin = (has_twin | (tail->has_twin ^ has_twin));
                /* skip the current */
                return;
            } else if (e == FILE_MODIFIED && !has_twin
              && tail->e == FILE_ATTRIB) {
                tail->e = FILE_MODIFIED;
                tail->has_twin = TRUE;
                return;
            } else if (e == FILE_ATTRIB
              && tail->e == FILE_MODIFIED && !tail->has_twin) {
                tail->has_twin = TRUE;
                return;
            }
        }
    }
    
    if ((ev = fnode_event_new (e, has_twin, f)) != NULL) {
        g_queue_push_tail (g_eventq, ev);
    }
}

static void
port_process_kevents ()
{
    fnode_event_t *ev;
    
    while ((ev = (fnode_event_t*)g_queue_pop_head (g_eventq)) != NULL) {
        FK_W ("[%s] 0x%p %s\n", __func__, ev, _event_string (ev->e));
        add_event_cb (ev->user_data, ev);
    }
}

static gboolean
port_fetch_event_cb (void *arg)
{
	pnode_t *pn = (pnode_t *)arg;
    _f* fo;
	uint_t nget = 0;
	port_event_t pe[PE_ALLOC];
    timespec_t timeout;
    gpointer f;
    gboolean ret = TRUE;
    
    /* FK_W ("IN <======== %s\n", __func__); */
    G_LOCK (fen_lock);
    
    memset (&timeout, 0, sizeof (timespec_t));
    do {
        nget = 1;
        if (port_getn (pn->port, pe, PE_ALLOC, &nget, &timeout) == 0) {
            int i;
            for (i = 0; i < nget; i++) {
                fo = (_f*)pe[i].portev_user;
                /* handle event */
                switch (pe[i].portev_source) {
                case PORT_SOURCE_FILE:
                    /* If got FILE_EXCEPTION or add to port failed,
                       delete the pnode */
                    fo->is_active = FALSE;
                    if (fo->user_data) {
                        FK_W("%s\n",
                          printevent(F_NAME(fo), pe[i].portev_events, "RAW"));
                        port_add_kevent (pe[i].portev_events, fo->user_data);
                    } else {
                        /* fnode is deleted */
                        goto L_delete;
                    }
                    if (pe[i].portev_events & FILE_EXCEPTION) {
                        g_hash_table_remove (_obj_fen_hash, fo->user_data);
                    L_delete:
                        FK_W ("[ FREE_FO ] [0x%p]\n", fo);
                        pnode_delete (fo->port);
                        g_free (fo);
                    }
                    break;
                default:
                    /* case PORT_SOURCE_TIMER: */
                    FK_W ("[kernel] unknown portev_source %d\n", pe[i].portev_source);
                }
            }
        } else {
            FK_W ("[kernel] port_getn %s\n", g_strerror (errno));
            nget = 0;
        }
    } while (nget == PE_ALLOC);

	/* Processing g_eventq */
    port_process_kevents ();
    
    if (pn->ref == 0) {
        pn->port_source_id = 0;
        ret = FALSE;
    }
    G_UNLOCK (fen_lock);
    /* FK_W ("OUT ========> %s\n", __func__); */
	return ret;
}

/*
 * ref - 1 if remove a watching file succeeded.
 */
static void
pnode_delete (pnode_t *pn)
{
    g_assert (pn->ref <= max_port_events);
    
	if (pn->ref == max_port_events) {
        FK_W ("PORT : move to visible queue - [pn] 0x%p [ref] %d\n", pn, pn->ref);
		pn_fq = g_list_remove (pn_fq, pn);
		pn_vq = g_list_prepend (pn_vq, pn);
	}
	if ((-- pn->ref) == 0) {
        /* Should dispatch the source */
	}
	FK_W ("%s [pn] 0x%p [ref] %d\n", __func__, pn, pn->ref);
}

/*
 * malloc pnode_t and port_create, start thread at pnode_ref.
 * if pnode_new succeeded, the pnode_t will never
 * be freed. So pnode_t can be freed only in pnode_new.
 * Note pnode_monitor_remove_all can also free pnode_t, but currently no one
 * invork it.
 */
static pnode_t *
pnode_new ()
{
	pnode_t *pn = NULL;

	if (pn_vq) {
		pn = (pnode_t*)pn_vq->data;
        g_assert (pn->ref < max_port_events);
	} else {
		pn = g_new0 (pnode_t, 1);
		if (pn != NULL) {
            if ((pn->port = port_create ()) >= 0) {
                g_assert (g_list_find (pn_vq, pn) == NULL);
                pn_vq = g_list_prepend (pn_vq, pn);
            } else {
                FK_W ("PORT_CREATE %s\n", g_strerror (errno));
                g_free (pn);
                pn = NULL;
			}
		}
	}
	if (pn) {
		FK_W ("%s [pn] 0x%p [ref] %d\n", __func__, pn, pn->ref);
        pn->ref++;
        if (pn->ref == max_port_events) {
            FK_W ("PORT : move to full queue - [pn] 0x%p [ref] %d\n", pn, pn->ref);
            pn_vq = g_list_remove (pn_vq, pn);
            pn_fq = g_list_prepend (pn_fq, pn);
            g_assert (g_list_find (pn_vq, pn) == NULL);
        }
        /* attach the source */
        if (pn->port_source_id == 0) {
            pn->port_source_id = g_timeout_add (PROCESS_PORT_EVENTS_TIME,
              port_fetch_event_cb,
              (void *)pn);
            g_assert (pn->port_source_id > 0);
        }
	}

	return pn;
}

/**
 * port_add_internal
 *
 * < private >
 * Unsafe, need lock fen_lock.
 */
static gboolean
port_add_internal (file_obj_t* fobj, off_t* len,
  gpointer f, gboolean need_stat)
{
    int ret;
    struct stat buf;
    _f* fo = NULL;

    g_assert (f && fobj);
    FK_W ("%s [0x%p] %s\n", __func__, f, fobj->fo_name);

    if ((fo = g_hash_table_lookup (_obj_fen_hash, f)) == NULL) {
        fo = g_new0 (_f, 1);
        fo->fobj = fobj;
        fo->user_data = f;
        g_assert (fo);
        FK_W ("[ NEW_FO ] [0x%p] %s\n", fo, F_NAME(fo));
        g_hash_table_insert (_obj_fen_hash, f, fo);
    }

    if (fo->is_active) {
        return TRUE;
    }

    if (fo->port == NULL) {
        fo->port = pnode_new ();
    }
    
    if (need_stat) {
        if (FN_STAT (F_NAME(fo), &buf) != 0) {
            FK_W ("LSTAT [%-20s] %s\n", F_NAME(fo), g_strerror (errno));
            goto L_exit;
        }
        g_assert (len);
        fo->fobj->fo_atime = buf.st_atim;
        fo->fobj->fo_mtime = buf.st_mtim;
        fo->fobj->fo_ctime = buf.st_ctim;
        *len = buf.st_size;
    }
    
    if (port_associate (F_PORT(fo),
          PORT_SOURCE_FILE,
          (uintptr_t)fo->fobj,
          FEN_ALL_EVENTS,
          (void *)fo) == 0) {
        fo->is_active = TRUE;
        FK_W ("%s %s\n", "PORT_ASSOCIATE", F_NAME(fo));
        return TRUE;
    } else {
        FK_W ("PORT_ASSOCIATE [%-20s] %s\n", F_NAME(fo), g_strerror (errno));
    L_exit:
        FK_W ("[ FREE_FO ] [0x%p]\n", fo);
        g_hash_table_remove (_obj_fen_hash, f);
        pnode_delete (fo->port);
        g_free (fo);
    }
    return FALSE;
}

gboolean
port_add (file_obj_t* fobj, off_t* len, gpointer f)
{
    return port_add_internal (fobj, len, f, TRUE);
}

gboolean
port_add_simple (file_obj_t* fobj, gpointer f)
{
    return port_add_internal (fobj, NULL, f, FALSE);
}

/**
 * port_remove
 *
 * < private >
 * Unsafe, need lock fen_lock.
 */
void
port_remove (gpointer f)
{
    _f* fo = NULL;

    FK_W ("%s\n", __func__);
    if ((fo = g_hash_table_lookup (_obj_fen_hash, f)) != NULL) {
        /* Marked */
        fo->user_data = NULL;
        g_hash_table_remove (_obj_fen_hash, f);
        
        if (port_dissociate (F_PORT(fo),
              PORT_SOURCE_FILE,
              (uintptr_t)fo->fobj) == 0) {
            /*
             * Note, we can run foode_delete if dissociating is failed,
             * because there may be some pending events (mostly like
             * FILE_DELETE) in the port_get. If we delete the foode
             * the fnode may be deleted, then port_get will run on an invalid
             * address.
             */
            FK_W ("[ FREE_FO ] [0x%p]\n", fo);
            pnode_delete (fo->port);
            g_free (fo);
        } else {
            FK_W ("PORT_DISSOCIATE [%-20s] %s\n", F_NAME(fo), g_strerror (errno));
        }
    }
}

const gchar *
_event_string (int event)
{
    switch (event) {
    case FILE_DELETE:
        return "FILE_DELETE";
    case FILE_RENAME_FROM:
        return "FILE_RENAME_FROM";
    case FILE_MODIFIED:
        return "FILE_MODIFIED";
    case FILE_RENAME_TO:
        return "FILE_RENAME_TO";
    case MOUNTEDOVER:
        return "MOUNTEDOVER";
    case FILE_ATTRIB:
        return "FILE_ATTRIB";
    case UNMOUNTED:
        return "UNMOUNTED";
    case FILE_ACCESS:
        return "FILE_ACCESS";
    default:
        return "EVENT_UNKNOWN";
    }
}

/**
 * Get Solaris resouce values.
 *
 */

extern gboolean
port_class_init (void (*user_add_event) (gpointer, fnode_event_t*))
{
	rctlblk_t *rblk;
    FK_W ("%s\n", __func__);
	if ((rblk = malloc (rctlblk_size ())) == NULL) {
        FK_W ("[kernel] rblk malloc %s\n", g_strerror (errno));
		return FALSE;
	}
	if (getrctl ("process.max-port-events", NULL, rblk, RCTL_FIRST) == -1) {
        FK_W ("[kernel] getrctl %s\n", g_strerror (errno));
        free (rblk);
        return FALSE;
	} else {
        max_port_events = rctlblk_get_value(rblk);
		FK_W ("[kernel] max_port_events = %u\n", max_port_events);
        free (rblk);
	}
    if ((_obj_fen_hash = g_hash_table_new(g_direct_hash,
           g_direct_equal)) == NULL) {
        FK_W ("[kernel] fobj hash initializing faild\n");
        return FALSE;
    }
    if ((g_eventq = g_queue_new ()) == NULL) {
		FK_W ("[kernel] FEN global event queue initializing faild\n");
    }
    if (user_add_event == NULL) {
        return FALSE;
    }
    add_event_cb = user_add_event;
	return TRUE;
}

fnode_event_t*
fnode_event_new (int event, gboolean has_twin, gpointer user_data)
{
    fnode_event_t *ev;
    
    if ((ev = g_new (fnode_event_t, 1)) != NULL) {
        g_assert (ev);
        ev->e = event;
        ev->user_data = user_data;
        ev->has_twin = has_twin;
        /* Default isn't a pending event. */
        ev->is_pending = FALSE;
    }
    return ev;
}

void
fnode_event_delete (fnode_event_t* ev)
{
    g_free (ev);
}
