/*-
 * Copyright (c) 2016 - 2018 Rozhuk Ivan <rozhuk.im@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Author: Rozhuk Ivan <rozhuk.im@gmail.com>
 *
 */

#include "config.h"

#include <glib-object.h>
#include <string.h>
#include <gio/gfilemonitor.h>
#include <gio/glocalfilemonitor.h>
#include <gio/giomodule.h>
#include "glib-private.h"
#include <glib-unix.h>
#include "kqueue_fnm.h"

/* Defaults. */
#ifndef KQUEUE_MON_RATE_LIMIT_TIME_INIT
#	define KQUEUE_MON_RATE_LIMIT_TIME_INIT		1000
#endif
#ifndef KQUEUE_MON_RATE_LIMIT_TIME_MAX
#	define KQUEUE_MON_RATE_LIMIT_TIME_MAX		8000
#endif
#ifndef KQUEUE_MON_RATE_LIMIT_TIME_MUL
#	define KQUEUE_MON_RATE_LIMIT_TIME_MUL		2
#endif
#ifndef KQUEUE_MON_MAX_DIR_FILES
#	define KQUEUE_MON_MAX_DIR_FILES			128
#endif
#ifndef KQUEUE_MON_LOCAL_SUBFILES
#	define KQUEUE_MON_LOCAL_SUBFILES		1
#endif
#ifndef KQUEUE_MON_LOCAL_SUBDIRS
#	define KQUEUE_MON_LOCAL_SUBDIRS			0
#endif


static GMutex			kqueue_lock;
static volatile kq_fnm_p	kqueue_fnm = NULL;
/* Exclude from file changes monitoring, watch only for dirs. */
static const char *non_local_fs[] = {
	"fusefs.sshfs",
	NULL
};

#define G_TYPE_KQUEUE_FILE_MONITOR      (g_kqueue_file_monitor_get_type())
#define G_KQUEUE_FILE_MONITOR(inst)     (G_TYPE_CHECK_INSTANCE_CAST((inst), \
					 G_TYPE_KQUEUE_FILE_MONITOR, GKqueueFileMonitor))

typedef GLocalFileMonitorClass	GKqueueFileMonitorClass;

typedef struct {
	GLocalFileMonitor	parent_instance;
	kq_fnmo_p		fnmo;
} GKqueueFileMonitor;

GType g_kqueue_file_monitor_get_type(void);
G_DEFINE_TYPE_WITH_CODE (GKqueueFileMonitor, g_kqueue_file_monitor, G_TYPE_LOCAL_FILE_MONITOR,
       g_io_extension_point_implement(G_LOCAL_FILE_MONITOR_EXTENSION_POINT_NAME,
               g_define_type_id,
               "kqueue",
               10))


static void
kqueue_event_handler(kq_fnm_p kfnm,
    kq_fnmo_p fnmo, void *udata, uint32_t event,
    const char *base, const char *filename, const char *new_filename) {
	static const uint32_t kfnm_to_glib_map[] = {
		0,				/* KF_EVENT_NOT_CHANGED */
		G_FILE_MONITOR_EVENT_CREATED,	/* KF_EVENT_CREATED */
		G_FILE_MONITOR_EVENT_DELETED,	/* KF_EVENT_DELETED */
		G_FILE_MONITOR_EVENT_RENAMED,	/* KF_EVENT_RENAMED */
		G_FILE_MONITOR_EVENT_CHANGED	/* KF_EVENT_CHANGED */
	};

	if (NULL == kfnm || NULL == filename ||
	    KF_EVENT_CREATED > event ||
	    KF_EVENT_CHANGED < event)
		return;
	g_file_monitor_source_handle_event(udata,
	    kfnm_to_glib_map[event],
	    filename, new_filename, NULL,
	    g_get_monotonic_time());
}

static gboolean
g_kqueue_file_monitor_is_supported(void) {
	kq_file_mon_settings_t kfms;

	if (NULL != kqueue_fnm)
		return (TRUE);
	/* Init only once. */
	g_mutex_lock(&kqueue_lock);
	if (NULL != kqueue_fnm) {
		g_mutex_unlock(&kqueue_lock);
		return (TRUE); /* Initialized while wait lock. */
	}

	memset(&kfms, 0x00, sizeof(kq_file_mon_settings_t));
	kfms.rate_limit_time_init = KQUEUE_MON_RATE_LIMIT_TIME_INIT;
	kfms.rate_limit_time_max = KQUEUE_MON_RATE_LIMIT_TIME_MAX;
	kfms.rate_limit_time_mul = KQUEUE_MON_RATE_LIMIT_TIME_MUL;
	kfms.max_dir_files = KQUEUE_MON_MAX_DIR_FILES;
	kfms.mon_local_subfiles = KQUEUE_MON_LOCAL_SUBFILES;
	kfms.mon_local_subdirs = KQUEUE_MON_LOCAL_SUBDIRS;
	kfms.local_fs = NULL;
	kfms.non_local_fs = non_local_fs;

	kqueue_fnm = kq_fnm_create(&kfms, kqueue_event_handler);
	if (NULL == kqueue_fnm) {
		g_mutex_unlock(&kqueue_lock);
		return (FALSE); /* Init fail. */
	}
	g_mutex_unlock(&kqueue_lock);

	return (TRUE);
}

static gboolean
g_kqueue_file_monitor_cancel(GFileMonitor *monitor) {
	GKqueueFileMonitor *gffm = G_KQUEUE_FILE_MONITOR(monitor);

	kq_fnm_del(kqueue_fnm, gffm->fnmo);
	gffm->fnmo = NULL;

	return (TRUE);
}

static void
g_kqueue_file_monitor_finalize(GObject *object) {
	//GKqueueFileMonitor *gffm = G_KQUEUE_FILE_MONITOR(object);

	//g_mutex_lock(&kqueue_lock);
	//kq_fnm_free(kqueue_fnm);
	//kqueue_fnm = NULL;
	//g_mutex_unlock(&kqueue_lock);
	//G_OBJECT_CLASS(g_kqueue_file_monitor_parent_class)->finalize(object);
}

static void
g_kqueue_file_monitor_start(GLocalFileMonitor *local_monitor,
    const gchar *dirname, const gchar *basename,
    const gchar *filename, GFileMonitorSource *source) {
	GKqueueFileMonitor *gffm = G_KQUEUE_FILE_MONITOR(local_monitor);

	g_assert(NULL != kqueue_fnm);
	//g_source_ref((GSource*)source);

	if (NULL == filename) {
		filename = dirname;
	}
	gffm->fnmo = kq_fnm_add(kqueue_fnm, filename, source);
}

static void
g_kqueue_file_monitor_init(GKqueueFileMonitor *monitor) {

}

static void
g_kqueue_file_monitor_class_init(GKqueueFileMonitorClass *class) {
	GObjectClass *gobject_class = G_OBJECT_CLASS(class);
	GFileMonitorClass *file_monitor_class = G_FILE_MONITOR_CLASS(class);

	class->is_supported = g_kqueue_file_monitor_is_supported;
	class->start = g_kqueue_file_monitor_start;
	class->mount_notify = TRUE; /* TODO: ??? */
	file_monitor_class->cancel = g_kqueue_file_monitor_cancel;
	gobject_class->finalize = g_kqueue_file_monitor_finalize;
}

static void
g_kqueue_file_monitor_class_finalize(GKqueueFileMonitorClass *class) {

}

void
g_io_module_load(GIOModule *module) {

	g_type_module_use(G_TYPE_MODULE(module));

	g_io_extension_point_implement(G_LOCAL_FILE_MONITOR_EXTENSION_POINT_NAME,
	    G_TYPE_KQUEUE_FILE_MONITOR, "kqueue", 10);
	g_io_extension_point_implement(G_NFS_FILE_MONITOR_EXTENSION_POINT_NAME,
	    G_TYPE_KQUEUE_FILE_MONITOR, "kqueue", 10);
}

void
g_io_module_unload(GIOModule *module) {

	g_assert_not_reached();
}
