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

#ifndef __KQUEUE_FILE_NOTIFY_MONITOR_H__
#define __KQUEUE_FILE_NOTIFY_MONITOR_H__

#include <sys/param.h>
#include <sys/types.h>
#include <inttypes.h>


typedef struct kq_file_nonify_monitor_s		*kq_fnm_p;
typedef struct kq_file_nmon_obj_s		*kq_fnmo_p;

typedef void (*kfnm_event_handler_cb)(kq_fnm_p kfnm,
					kq_fnmo_p fnmo, void *udata,
					uint32_t event,
					const char *base,
					const char *filename,
					const char *new_filename);
#define KF_EVENT_NOT_CHANGED	0 /* Internal use. */
#define KF_EVENT_CREATED	1
#define KF_EVENT_DELETED	2
#define KF_EVENT_RENAMED	3
#define KF_EVENT_CHANGED	4


typedef struct kq_file_nonify_mon_settings_s {
	uint32_t	rate_limit_time_init;	/* Fire events for dir min interval, mseconds. */
	uint32_t	rate_limit_time_max;	/* Fire events for dir max interval, mseconds. */
	uint32_t	rate_limit_time_mul;	/* Fire events time increment, mseconds. */
	size_t		max_dir_files;		/* If dir contain more than n files - do not mon files changes. */
	int		mon_local_subfiles;	/* Enable monitoring files changes on local file systems. */
	int		mon_local_subdirs;	/* Also mon for subdirs changes . */
	const char 	**local_fs;		/* NULL terminated fs names list that threat as local. Keep utill kq_fnm_free() return. */
	const char	**non_local_fs;		/* NULL terminated fs names list that threat as not local. Keep utill kq_fnm_free() return. */
} kq_file_mon_settings_t, *kq_file_mon_settings_p;

kq_fnm_p	kq_fnm_create(kq_file_mon_settings_p s,
		    kfnm_event_handler_cb cb_func);
void		kq_fnm_free(kq_fnm_p kfnm);

kq_fnmo_p	kq_fnm_add(kq_fnm_p kfnm,
			    const char *path, void *udata);
void		kq_fnm_del(kq_fnm_p kfnm, kq_fnmo_p fnmo);


#endif /* __KQUEUE_FILE_NOTIFY_MONITOR_H__ */
