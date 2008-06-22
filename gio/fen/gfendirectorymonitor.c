/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set expandtab ts=4 shiftwidth=4: */
/* GIO - GLib Input, Output and Streaming Library
 * 
 * Copyright (C) 2006-2007 Red Hat, Inc.
 * Copyright (C) 2007 Sebastian Dröge.
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
 * Authors: Alexander Larsson <alexl@redhat.com>
 *          John McCutchan <john@johnmccutchan.com> 
 *          Sebastian Dröge <slomo@circular-chaos.org>
 *          Lin Ma <lin.ma@sun.com>
 */

#include "config.h"

#include "gfendirectorymonitor.h"
#include "giomodule.h"

#include "fen-helper.h"

#include "gioalias.h"

struct _GFenDirectoryMonitor
{
	GLocalDirectoryMonitor parent_instance;
    gboolean cancelled;
    fen_sub* sub;
};

static gboolean g_fen_directory_monitor_cancel (GFileMonitor* monitor);

#define g_fen_directory_monitor_get_type _g_fen_directory_monitor_get_type
G_DEFINE_TYPE_WITH_CODE (GFenDirectoryMonitor, g_fen_directory_monitor, G_TYPE_LOCAL_DIRECTORY_MONITOR,
  g_io_extension_point_implement (G_LOCAL_DIRECTORY_MONITOR_EXTENSION_POINT_NAME,
    g_define_type_id,
    "fen",
    20))

static void
g_fen_directory_monitor_finalize (GObject *object)
{
	GFenDirectoryMonitor *self = G_FEN_DIRECTORY_MONITOR (object);
    
    if (self->sub) {
        fen_remove (G_LOCAL_DIRECTORY_MONITOR (self)->dirname, self->sub, TRUE);
        fen_sub_delete (self->sub);
        self->sub = NULL;
    }

	if (G_OBJECT_CLASS (g_fen_directory_monitor_parent_class)->finalize)
		(*G_OBJECT_CLASS (g_fen_directory_monitor_parent_class)->finalize) (object);
}

static GObject *
g_fen_directory_monitor_constructor (GType type,
  guint n_construct_properties,
  GObjectConstructParam *construct_properties)
{
	GObject *obj;
	GFenDirectoryMonitorClass *klass;
	GObjectClass *parent_class;
	GFenDirectoryMonitor *self;
	const gchar *dirname = NULL;
  
	klass = G_FEN_DIRECTORY_MONITOR_CLASS (g_type_class_peek (G_TYPE_FEN_DIRECTORY_MONITOR));
	parent_class = g_fen_directory_monitor_parent_class;
	obj = parent_class->constructor (type,
      n_construct_properties,
      construct_properties);

	self = G_FEN_DIRECTORY_MONITOR (obj);

	dirname = G_LOCAL_DIRECTORY_MONITOR (self)->dirname;
	g_assert (dirname != NULL);

	/* Will never fail as is_supported() should be called before instanciating
	 * anyway */
    if (!fen_init ())
        g_assert_not_reached ();

	/* FIXME: what to do about errors here? we can't return NULL or another
	 * kind of error and an assertion is probably too hard */
    self->sub = fen_sub_new (self, TRUE);
    g_assert (self->sub);
    
    fen_add (dirname, self->sub, TRUE);

	return obj;
}

static gboolean
g_fen_directory_monitor_is_supported (void)
{
	return fen_init ();
}

static void
g_fen_directory_monitor_class_init (GFenDirectoryMonitorClass* klass)
{
	GObjectClass* gobject_class = G_OBJECT_CLASS (klass);
	GFileMonitorClass *directory_monitor_class = G_FILE_MONITOR_CLASS (klass);
	GLocalDirectoryMonitorClass *local_directory_monitor_class = G_LOCAL_DIRECTORY_MONITOR_CLASS (klass);
  
	gobject_class->finalize = g_fen_directory_monitor_finalize;
	gobject_class->constructor = g_fen_directory_monitor_constructor;
	directory_monitor_class->cancel = g_fen_directory_monitor_cancel;

	local_directory_monitor_class->mount_notify = TRUE;
	local_directory_monitor_class->is_supported = g_fen_directory_monitor_is_supported;
}

static void
g_fen_directory_monitor_init (GFenDirectoryMonitor* monitor)
{
}

static gboolean
g_fen_directory_monitor_cancel (GFileMonitor* monitor)
{
	GFenDirectoryMonitor *self = G_FEN_DIRECTORY_MONITOR (monitor);
    
    if (self->sub) {
        fen_remove (G_LOCAL_DIRECTORY_MONITOR (self)->dirname, self->sub, TRUE);
        fen_sub_delete (self->sub);
        self->sub = NULL;
    }
    
	if (G_FILE_MONITOR_CLASS (g_fen_directory_monitor_parent_class)->cancel)
		(*G_FILE_MONITOR_CLASS (g_fen_directory_monitor_parent_class)->cancel) (monitor);

	return TRUE;
}

