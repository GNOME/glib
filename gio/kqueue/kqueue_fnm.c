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

#include <sys/param.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/fcntl.h> /* open, fcntl */

#include <inttypes.h>
#include <stdlib.h> /* malloc, exit */
#include <unistd.h> /* close, write, sysconf */
#include <string.h> /* bcopy, bzero, memcpy, memmove, memset, strerror... */
#include <dirent.h> /* opendir, readdir */
#include <errno.h>
#include <pthread.h>

#include "kqueue_fnm.h"


/* Preallocate items count. */
#ifndef FILES_ALLOC_BLK_SIZE
#	define FILES_ALLOC_BLK_SIZE	32
#endif


typedef struct readdir_context_s {
	int		fd;
	uint8_t		*buf;
	size_t		buf_size;
	size_t		buf_used;
	size_t		buf_pos;
} readdir_ctx_t, *readdir_ctx_p;


typedef struct file_info_s { /* Directory file. */
	int		fd;		/* For notify kqueue(). */
	struct dirent 	de;		/* d_reclen used for action. */
	struct stat	sb;
} file_info_t, *file_info_p;


typedef struct kq_file_nmon_obj_s {
	int		fd;		/* For notify kqueue(). */
	int		is_dir;
	int		is_local;	/* Is file system local. */
	struct stat	sb;
	char		path[(PATH_MAX + 2)];
	size_t		path_size;
	size_t		name_offset;	/* Parent path size. */
	uint32_t	rate_lim_cur_interval;	/* From rate_limit_time_init to rate_limit_time_max. 0 disabled. */
	size_t		rate_lim_ev_cnt;	/* Events count then rate_lim_cur_interval != 0 since last report. */
	sbintime_t	rate_lim_ev_last;	/* Last event time. */
	void		*udata;
	kq_fnm_p	kfnm;
	/* For dir. */
	file_info_p	files;
	volatile size_t	files_count;
	size_t		files_allocated;
} kq_fnmo_t;


typedef struct kq_file_nonify_monitor_s {
	int		fd;		/* kqueue() fd. */
	int		pfd[2];		/* pipe queue specific. */
	kfnm_event_handler_cb cb_func;	/* Callback on dir/file change. */
	kq_file_mon_settings_t s;
	sbintime_t	rate_lim_time_init; /* rate_limit_time_init */
	pthread_t	tid;
} kq_fnm_t;


typedef void (*kq_msg_cb)(void *arg);

typedef struct kq_file_mon_msg_pkt_s {
	size_t		magic;
	kq_msg_cb	msg_cb;
	void		*arg;
	size_t		chk_sum;
} kq_fnm_msg_pkt_t, *kq_fnm_msg_pkt_p;

#define KF_MSG_PKT_MAGIC	0xffddaa00


#ifndef O_NOATIME
#	define O_NOATIME	0
#endif
#ifndef O_EVTONLY
#	define O_EVTONLY	O_RDONLY
#endif
#define OPEN_FILE_FLAGS		(O_EVTONLY | O_NONBLOCK | O_NOFOLLOW | O_NOATIME | O_CLOEXEC)

#define EVFILT_VNODE_SUB_FLAGS	(NOTE_WRITE |				\
				NOTE_EXTEND |				\
				NOTE_ATTRIB |				\
				NOTE_LINK |				\
				NOTE_CLOSE_WRITE)

#define EVFILT_VNODE_FLAGS_ALL	(NOTE_DELETE |				\
				EVFILT_VNODE_SUB_FLAGS |		\
				NOTE_RENAME |				\
				NOTE_REVOKE)

#ifndef _GENERIC_DIRSIZ
#	define _GENERIC_DIRSIZ(__de)	MIN((__de)->d_reclen, sizeof(struct dirent))
#endif

#define IS_NAME_DOTS(__name)	('.' == (__name)[0] &&			\
				 ('\0' == (__name)[1] || 		\
				  ('.' == (__name)[1] && '\0' == (__name)[2])))
#define IS_DE_NAME_EQ(__de1, __de2)  (0 == mem_cmpn((__de1)->d_name,	\
						    (__de1)->d_namlen,	\
						    (__de2)->d_name,	\
						    (__de2)->d_namlen))
#define zalloc(__size)		calloc(1, (__size))

#if (!defined(HAVE_REALLOCARRAY) && (!defined(__FreeBSD_version) || __FreeBSD_version < 1100000))
#	define reallocarray(__mem, __size, __count)	realloc((__mem), ((__size) * (__count)))
#endif

/* To not depend from compiller version. */
#define MSTOSBT(__ms)		((sbintime_t)((((int64_t)(__ms)) * (int64_t)(((uint64_t)1 << 63) / 500) >> 32)))

#ifndef CLOCK_MONOTONIC_FAST
#	define CLOCK_MONOTONIC_FAST	CLOCK_MONOTONIC
#endif


void 	*kq_fnm_proccess_events_proc(void *data);

static inline int
mem_cmpn(const void *buf1, const size_t buf1_size,
    const void *buf2, const size_t buf2_size) {

	if (buf1_size != buf2_size)
		return (((buf1_size > buf2_size) ? 127 : -127));
	if (0 == buf1_size || buf1 == buf2)
		return (0);
	if (NULL == buf1)
		return (-127);
	if (NULL == buf2)
		return (127);
	return (memcmp(buf1, buf2, buf1_size));
}

static int
realloc_items(void **items, const size_t item_size,
    size_t *allocated, const size_t alloc_blk_cnt, const size_t count) {
	size_t allocated_prev, allocated_new;
	uint8_t *items_new;

	if (NULL == items || 0 == item_size || NULL == allocated ||
	    0 == alloc_blk_cnt)
		return (EINVAL);
	allocated_prev = (*allocated);
	if (NULL != (*items) &&
	    allocated_prev > count &&
	    allocated_prev <= (count + alloc_blk_cnt))
		return (0);
	allocated_new = (((count / alloc_blk_cnt) + 1) * alloc_blk_cnt);
	items_new = (uint8_t*)reallocarray((*items), item_size, allocated_new);
	if (NULL == items_new) /* Realloc fail! */
		return (ENOMEM);

	if (allocated_new > allocated_prev) { /* Init new mem. */
		memset((items_new + (allocated_prev * item_size)), 0x00,
		    ((allocated_new - allocated_prev) * item_size));
	}
	(*items) = items_new;
	(*allocated) = allocated_new;

	return (0);
}


static int
readdir_start(int fd, struct stat *sb, size_t exp_count, readdir_ctx_p rdd) {
	size_t buf_size;

	if (-1 == fd || NULL == sb || NULL == rdd)
		return (EINVAL);
	if (-1 == lseek(fd, 0, SEEK_SET))
		return (errno);
	/* Calculate buf size for getdents(). */
	buf_size = MAX((size_t)sb->st_size, (exp_count * sizeof(struct dirent)));
	if (0 == buf_size) {
		buf_size = (16 * PAGE_SIZE);
	}
	/* Make buf size well aligned. */
	if (0 != sb->st_blksize) {
		if (powerof2(sb->st_blksize)) {
			buf_size = roundup2(buf_size, sb->st_blksize);
		} else {
			buf_size = roundup(buf_size, sb->st_blksize);
		}
	} else {
		buf_size = round_page(buf_size);
	}
	/* Init. */
	memset(rdd, 0x00, sizeof(readdir_ctx_t));
	rdd->buf = malloc(buf_size);
	if (NULL == rdd->buf)
		return (ENOMEM);
	rdd->buf_size = buf_size;
	rdd->fd = fd;

	return (0);
}

static void
readdir_free(readdir_ctx_p rdd) {

	if (NULL == rdd || NULL == rdd->buf)
		return;
	free(rdd->buf);
	memset(rdd, 0x00, sizeof(readdir_ctx_t));
}

static int
readdir_next(readdir_ctx_p rdd, struct dirent *de) {
	int error = 0, ios;
	uint8_t *ptr;

	if (NULL == rdd || NULL == rdd->buf || NULL == de)
		return (EINVAL);

	for (;;) {
		if (rdd->buf_used <= rdd->buf_pos) {
			/* Called once if buf size calculated ok. */
			ios = getdents(rdd->fd, (char*)rdd->buf, (int)rdd->buf_size);
			if (-1 == ios) {
				error = errno;
				break;
			}
			if (0 == ios) {
				error = ESPIPE; /* EOF. */
				break;
			}
			rdd->buf_used = (size_t)ios;
			rdd->buf_pos = 0;
		}
		/* Keep data aligned. */
		ptr = (rdd->buf + rdd->buf_pos);
		memcpy(de, ptr, (sizeof(struct dirent) - sizeof(de->d_name)));
		if (0 == de->d_reclen) {
			error = ESPIPE; /* EOF. */
			break;
		}
		rdd->buf_pos += de->d_reclen;
#ifdef DT_WHT
		if (DT_WHT == de->d_type)
			continue;
#endif
		memcpy(de, ptr, _GENERIC_DIRSIZ(de));
		if (!IS_NAME_DOTS(de->d_name))
			return (0); /* OK. */
	}

	/* Err or no more files. */
	readdir_free(rdd);

	return (error);
}


static int
file_info_find_ni(file_info_p files, size_t files_count,
    file_info_p fi, size_t *idx) {
	size_t i;
	mode_t st_ftype;

	if (NULL == files || NULL == fi || NULL == idx)
		return (0);
	st_ftype = (S_IFMT & fi->sb.st_mode);
	for (i = 0; i < files_count; i ++) {
		if ((S_IFMT & files[i].sb.st_mode) != st_ftype)
			continue;
		if ((fi->sb.st_ino != files[i].sb.st_ino ||
		     fi->de.d_fileno != files[i].de.d_fileno) &&
		    0 == IS_DE_NAME_EQ(&fi->de, &files[i].de))
			continue;
		(*idx) = i;
		return (1);
	}
	(*idx) = files_count;
	return (0);
}

static int
file_info_find_ino(file_info_p files, size_t files_count,
    file_info_p fi, size_t *idx) {
	size_t i;
	mode_t st_ftype;

	if (NULL == files || NULL == fi || NULL == idx)
		return (0);
	st_ftype = (S_IFMT & fi->sb.st_mode);
	for (i = 0; i < files_count; i ++) {
		if ((S_IFMT & files[i].sb.st_mode) != st_ftype ||
		    fi->sb.st_ino != files[i].sb.st_ino ||
		    fi->de.d_fileno != files[i].de.d_fileno)
			continue;
		(*idx) = i;
		return (1);
	}
	(*idx) = files_count;
	return (0);
}

static int
file_info_find_name(file_info_p files, size_t files_count,
    file_info_p fi, size_t *idx) {
	size_t i;
	mode_t st_ftype;

	if (NULL == files || NULL == fi || NULL == idx)
		return (0);
	st_ftype = (S_IFMT & fi->sb.st_mode);
	for (i = 0; i < files_count; i ++) {
		if ((S_IFMT & files[i].sb.st_mode) != st_ftype ||
		    0 == IS_DE_NAME_EQ(&fi->de, &files[i].de))
			continue;
		(*idx) = i;
		return (1);
	}
	(*idx) = files_count;
	return (0);
}

static void
file_info_fd_close(file_info_p files, size_t files_count) {
	size_t i;

	if (NULL == files || 0 == files_count)
		return;
	for (i = 0; i < files_count; i ++) {
		if (-1 == files[i].fd)
			continue;
		close(files[i].fd);
		files[i].fd = -1;
	}
}


static int
is_fs_local(struct statfs *stfs, const char **local_fs, const char **non_local_fs) {
	size_t i;

	if (NULL == stfs)
		return (0);
	if (NULL != local_fs) {
		for (i = 0; NULL != local_fs[i]; i ++) {
			if (0 == strncmp(stfs->f_fstypename, local_fs[i],
			    sizeof(stfs->f_fstypename)))
				return (1);
		}
	}
	if (0 == (MNT_LOCAL & stfs->f_flags))
		return (0);
	if (NULL != non_local_fs) {
		for (i = 0; NULL != non_local_fs[i]; i ++) {
			if (0 == strncmp(stfs->f_fstypename, non_local_fs[i],
			    sizeof(stfs->f_fstypename)))
				return (0);
		}
	}
	return (1);
}


static void
kq_fnmo_rate_lim_stop(kq_fnmo_p fnmo) {
	struct kevent kev;

	if (NULL == fnmo || -1 == fnmo->fd || 0 == fnmo->rate_lim_cur_interval)
		return;
	fnmo->rate_lim_cur_interval = 0;
	fnmo->rate_lim_ev_cnt = 0;
	EV_SET(&kev, fnmo->fd, EVFILT_TIMER, EV_DELETE, 0, 0, NULL);
	kevent(fnmo->kfnm->fd, &kev, 1, NULL, 0, NULL);
}

static int
kq_fnmo_rate_lim_shedule_next(kq_fnmo_p fnmo) {
	u_short flags = (EV_ADD | EV_CLEAR | EV_ONESHOT);
	struct kevent kev;

	if (NULL == fnmo || -1 == fnmo->fd || 0 == fnmo->kfnm->s.rate_limit_time_init)
		return (EINVAL);
	if (0 == fnmo->rate_lim_cur_interval) { /* First call. */
		fnmo->rate_lim_cur_interval = fnmo->kfnm->s.rate_limit_time_init;
	} else {
		if (fnmo->rate_lim_cur_interval == fnmo->kfnm->s.rate_limit_time_max)
			return (0); /* No need to modify timer. */
		/* Increase rate limit interval. */
		fnmo->rate_lim_cur_interval *= fnmo->kfnm->s.rate_limit_time_mul;
	}
	if (fnmo->rate_lim_cur_interval >= fnmo->kfnm->s.rate_limit_time_max) {
		/* Check upper limit and shedule periodic timer with upper rate limit time. */
		flags &= ~EV_ONESHOT;
		fnmo->rate_lim_cur_interval = fnmo->kfnm->s.rate_limit_time_max;
	}
	EV_SET(&kev, fnmo->fd, EVFILT_TIMER, flags,
	    NOTE_MSECONDS, fnmo->rate_lim_cur_interval, fnmo);
	if (-1 == kevent(fnmo->kfnm->fd, &kev, 1, NULL, 0, NULL)) {
		fnmo->rate_lim_cur_interval = 0;
		return (errno);
	}
	if (0 != (EV_ERROR & kev.flags)) {
		fnmo->rate_lim_cur_interval = 0;
		return ((int)kev.data);
	}
	return (0);
}

/* Return:
 * 0 for events that not handled
 * 1 for handled = rate limited
 * -1 on error.
 */
static int
kq_fnmo_rate_lim_check(kq_fnmo_p fnmo) {
	sbintime_t sbt, sbt_now;
	struct timespec ts;

	if (NULL == fnmo)
		return (-1);
	if (-1 == fnmo->fd ||
	    0 == fnmo->kfnm->s.rate_limit_time_init)
		return (0);
	if (0 != fnmo->rate_lim_cur_interval) {
		fnmo->rate_lim_ev_cnt ++; /* Count event, timer is active. */
		return (1);
	}

	/* Do we need to enable rate limit? */
	if (0 != clock_gettime(CLOCK_MONOTONIC_FAST, &ts))
		return (-1);
	sbt_now = tstosbt(ts);
	sbt = (fnmo->rate_lim_ev_last + fnmo->kfnm->rate_lim_time_init);
	fnmo->rate_lim_ev_last = sbt_now;
	if (sbt < sbt_now) /* Events rate to low. */
		return (0);
	/* Try to enable rate limit. */
	if (0 != kq_fnmo_rate_lim_shedule_next(fnmo))
		return (-1);
	/* Ok. */
	fnmo->rate_lim_ev_cnt ++;

	return (1);
}

static void
kq_fnmo_clean(kq_fnmo_p fnmo) {

	if (NULL == fnmo)
		return;
	if (-1 != fnmo->fd) {
		kq_fnmo_rate_lim_stop(fnmo);
		close(fnmo->fd);
		fnmo->fd = -1;
	}
	if (0 != fnmo->is_local) { /* Stop monitoring files/dirs. */
		file_info_fd_close(fnmo->files, fnmo->files_count);
	}
	free(fnmo->files);
	fnmo->files = NULL;
	fnmo->files_count = 0;
	fnmo->files_allocated = 0;
}

static void
kq_fnmo_free(void *arg) {
	kq_fnmo_p fnmo = arg;

	if (NULL == fnmo)
		return;
	kq_fnmo_clean(fnmo);
	free(fnmo);
}

static kq_fnmo_p
kq_fnmo_alloc(kq_fnm_p kfnm, const char *path, void *udata) {
	kq_fnmo_p fnmo;

	if (NULL == kfnm || NULL == path)
		return (NULL);
	fnmo = zalloc(sizeof(kq_fnmo_t));
	if (NULL == fnmo)
		return (NULL);
	/* Remember args. */
	fnmo->path_size = strlcpy(fnmo->path, path, PATH_MAX);
	fnmo->name_offset = fnmo->path_size;
	fnmo->udata = udata;
	fnmo->kfnm = kfnm;

	return (fnmo);
}

static int
kq_fnmo_readdir(kq_fnmo_p fnmo, size_t exp_count) {
	int error;
	struct dirent *de;
	file_info_p tmfi;
	readdir_ctx_t rdd;

	if (NULL == fnmo || 0 == fnmo->is_dir)
		return (EINVAL);

	free(fnmo->files);
	fnmo->files = NULL;
	fnmo->files_count = 0;
	fnmo->files_allocated = 0;
	/* Pre allocate. */
	if (0 != realloc_items((void**)&fnmo->files,
	    sizeof(file_info_t), &fnmo->files_allocated,
	    FILES_ALLOC_BLK_SIZE, (exp_count + 1)))
		return (ENOMEM);

	error = readdir_start(fnmo->fd, &fnmo->sb, exp_count, &rdd);
	if (0 != error)
		return (error);
	for (;;) {
		if (0 != realloc_items((void**)&fnmo->files,
		    sizeof(file_info_t), &fnmo->files_allocated,
		    FILES_ALLOC_BLK_SIZE, fnmo->files_count)) {
			free(fnmo->files);
			fnmo->files = NULL;
			fnmo->files_count = 0;
			fnmo->files_allocated = 0;
			readdir_free(&rdd);
			return (ENOMEM);
		}
		de = &fnmo->files[fnmo->files_count].de; /* Use short name. */
		/* Get file name from folder. */
		if (0 != readdir_next(&rdd, de))
			break;
		/* Get file attrs. */
		if (0 != fstatat(fnmo->fd, de->d_name,
		    &fnmo->files[fnmo->files_count].sb,
		    AT_SYMLINK_NOFOLLOW)) {
			memset(&fnmo->files[fnmo->files_count].sb, 0x00,
			    sizeof(struct stat));
		}
		fnmo->files[fnmo->files_count].fd = -1;
		fnmo->files_count ++;
	}
	/* Mem compact. */
	tmfi = reallocarray(fnmo->files, sizeof(file_info_t), (fnmo->files_count + 1));
	if (NULL != tmfi) { /* realloc ok. */
		fnmo->files = tmfi;
		fnmo->files_allocated = (fnmo->files_count + 1);
	}

	readdir_free(&rdd);

	return (0); /* OK. */
}


static void
kq_fnmo_fi_start(kq_fnmo_p fnmo, file_info_p fi) {
	struct kevent kev;

	if (NULL == fnmo || NULL == fi)
		return;
	fi->fd = openat(fnmo->fd, fi->de.d_name, OPEN_FILE_FLAGS);
	if (-1 == fi->fd)
		return;
	EV_SET(&kev, fi->fd, EVFILT_VNODE,
	    (EV_ADD | EV_CLEAR),
	    EVFILT_VNODE_SUB_FLAGS, 0, fnmo);
	kevent(fnmo->kfnm->fd, &kev, 1, NULL, 0, NULL);
}

static int
kq_fnmo_is_fi_monitored(kq_fnmo_p fnmo, file_info_p fi) {

	if (NULL == fnmo)
		return (0);
	if (0 == fnmo->is_local ||
	    (0 != fnmo->kfnm->s.max_dir_files &&
	     fnmo->kfnm->s.max_dir_files < fnmo->files_count))
		return (0);
	if (NULL != fi &&
	    0 == fnmo->kfnm->s.mon_local_subdirs &&
	    S_ISDIR(fi->sb.st_mode))
		return (0);
	return (1);
}

static void
kq_fnmo_init(void *arg) {
	kq_fnmo_p fnmo = arg;
	size_t i;
	struct statfs stfs;
	struct kevent kev;

	if (NULL == fnmo)
		return;
	fnmo->fd = open(fnmo->path, OPEN_FILE_FLAGS);
	if (-1 == fnmo->fd)
		return;
	if (0 != fstat(fnmo->fd, &fnmo->sb))
		goto err_out;
	
	/* Get parent folder name. */
	if (S_ISDIR(fnmo->sb.st_mode)) {
		fnmo->is_dir = 1;
		/* Be sure that folder contain trailing '/'. */
		if ('/' != fnmo->path[(fnmo->path_size - 1)]) {
			fnmo->path[fnmo->path_size] = '/';
			fnmo->path_size ++;
			fnmo->path[fnmo->path_size] = 0;
		}
		/* Skip last '/' for parent dir search. */
		fnmo->name_offset = (fnmo->path_size - 1);
	}

	/* Is file system local? */
	if (0 != fnmo->is_dir &&
	    0 != fnmo->kfnm->s.mon_local_subfiles &&
	    0 == fstatfs(fnmo->fd, &stfs)) {
		fnmo->is_local = is_fs_local(&stfs, fnmo->kfnm->s.local_fs,
		    fnmo->kfnm->s.non_local_fs);
	}

	/* Find parent dir path size. */
	while (0 < fnmo->name_offset && '/' != fnmo->path[(fnmo->name_offset - 1)]) {
		fnmo->name_offset --;
	}

	/* Dir special processing. */
	if (0 != fnmo->is_dir) {
		/* Read and remember dir content. */
		if (0 != kq_fnmo_readdir(fnmo, 0))
			goto err_out;
	}
	/* Add to kqueue. */
	EV_SET(&kev, fnmo->fd, EVFILT_VNODE, (EV_ADD | EV_CLEAR),
	    EVFILT_VNODE_FLAGS_ALL, 0, fnmo);
	if (-1 == kevent(fnmo->kfnm->fd, &kev, 1, NULL, 0, NULL) ||
	    0 != (EV_ERROR & kev.flags))
		goto err_out;
	/* Add monitor sub files/dirs, ignory errors. */
	/* Check twice for performance reason. */
	if (0 != kq_fnmo_is_fi_monitored(fnmo, NULL)) {
		for (i = 0; i < fnmo->files_count; i ++) {
			if (0 != kq_fnmo_is_fi_monitored(fnmo, &fnmo->files[i])) {
				kq_fnmo_fi_start(fnmo, &fnmo->files[i]);
			}
		}
	}

	return; /* OK. */

err_out:
	kq_fnmo_clean(fnmo);
}

static void
kq_handle_changes(kq_fnm_p kfnm, kq_fnmo_p fnmo) {
	size_t i, k, files_count;
	file_info_p files;

	if (NULL == kfnm || NULL == fnmo)
		return;
	if (0 != fstat(fnmo->fd, &fnmo->sb) ||
	    0 == fnmo->sb.st_nlink) {
		kq_fnmo_clean(fnmo);
		kfnm->cb_func(kfnm, fnmo, fnmo->udata, KF_EVENT_DELETED,
		    fnmo->path, "", NULL);
		return;
	}
	if (0 == fnmo->is_dir) {
		fnmo->path[fnmo->name_offset] = 0;
		kfnm->cb_func(kfnm, fnmo, fnmo->udata, KF_EVENT_CHANGED,
		    fnmo->path, (fnmo->path + fnmo->name_offset), NULL);
		fnmo->path[fnmo->name_offset] = '/';
		return;
	}

	/* Dir processing. */

	/* Save prev. */
	files = fnmo->files;
	files_count = fnmo->files_count;
	fnmo->files = NULL;
	fnmo->files_count = 0;
	/* Update dir. */
	if (0 != kq_fnmo_readdir(fnmo, files_count)) {
		/* Restore prev state on fail. */
		fnmo->files = files;
		fnmo->files_count = files_count;
		return;
	}
	/* Notify removed first. */
	for (i = 0; i < files_count; i ++) {
		if (0 != file_info_find_ni(fnmo->files, fnmo->files_count, &files[i], &k)) /* Deleted? */
			continue;
		if (-1 != files[i].fd) {
			close(files[i].fd);
			files[i].fd = -1;
		}
		kfnm->cb_func(kfnm, fnmo, fnmo->udata, KF_EVENT_DELETED,
		    fnmo->path, files[i].de.d_name, NULL);
	}
	/* Notify. */
	for (i = 0; i < fnmo->files_count; i ++) {
		/* Is new file/folder? */
		if (0 == file_info_find_ino(files, files_count, &fnmo->files[i], &k) &&
		    0 == file_info_find_name(files, files_count, &fnmo->files[i], &k)) { /* Add new. */
			/* Add monitor sub files/dirs, ignory errors. */
			if (0 != kq_fnmo_is_fi_monitored(fnmo, &fnmo->files[i])) {
				kq_fnmo_fi_start(fnmo, &fnmo->files[i]);
			}
			kfnm->cb_func(kfnm, fnmo, fnmo->udata, KF_EVENT_CREATED,
			    fnmo->path, fnmo->files[i].de.d_name, NULL);
			continue;
		}
		/* Keep file fd. */
		fnmo->files[i].fd = files[k].fd;
		files[k].fd = -1;
		/* Is renamed? */
		if (0 == IS_DE_NAME_EQ(&files[k].de, &fnmo->files[i].de)) {
			kfnm->cb_func(kfnm, fnmo, fnmo->udata, KF_EVENT_RENAMED,
			    fnmo->path, files[k].de.d_name, fnmo->files[i].de.d_name);
			continue;
		}
		/* Is modified? */
		if (0 != memcmp(&fnmo->files[i].sb, &files[k].sb, sizeof(struct stat))) {
			kfnm->cb_func(kfnm, fnmo, fnmo->udata, KF_EVENT_CHANGED,
			    fnmo->path, fnmo->files[i].de.d_name, NULL);
			continue;
		}
		/* Not changed. */
	}

	/* Prevent FD leak die to race conditions.
	 * All fd must be -1, check this while debuging.
	 */
	file_info_fd_close(files, files_count);
	free(files);
}

static void
kq_handle_rename(kq_fnm_p kfnm, kq_fnmo_p fnmo) {
	int up_dir_fd, found = 0;
	readdir_ctx_t rdd;
	struct dirent de;
	struct stat sb;
	char old_filename[(MAXNAMLEN + 2)];
	size_t old_filename_size;

	if (NULL == kfnm || NULL == fnmo)
		return;
	if (0 != fstat(fnmo->fd, &fnmo->sb) ||
	    0 == fnmo->sb.st_nlink) {
notify_removed:
		kq_fnmo_clean(fnmo);
		kfnm->cb_func(kfnm, fnmo, fnmo->udata, KF_EVENT_DELETED,
		    fnmo->path, "", NULL);
		return;
	}
	/* Save old file name. */
	old_filename_size = (fnmo->path_size - fnmo->name_offset - (size_t)fnmo->is_dir);
	memcpy(old_filename,
	    (fnmo->path + fnmo->name_offset),
	    old_filename_size);
	old_filename[old_filename_size] = 0;

	/* Get parent folder name. */
	fnmo->path[fnmo->name_offset] = 0;
	/* Try to open. */
	up_dir_fd = open(fnmo->path, (OPEN_FILE_FLAGS | O_DIRECTORY));
	/* Restore '/' after parent folder. */
	fnmo->path[fnmo->name_offset] = '/';
	if (-1 == up_dir_fd ||
	    0 != fstat(up_dir_fd, &sb) ||
	    0 != readdir_start(up_dir_fd, &sb, 0, &rdd)) {
		close(up_dir_fd);
		return;
	}
	/* Find new name by inode. */
	while (0 == readdir_next(&rdd, &de)) {
		if (0 == fstatat(up_dir_fd, de.d_name, &sb, AT_SYMLINK_NOFOLLOW) &&
		    0 == memcmp(&fnmo->sb, &sb, sizeof(struct stat))) {
			found ++;
			break;
		}
	}
	close(up_dir_fd);
	if (0 == found)
		goto notify_removed; /* Not found. */
	/* Update name. */
	if (PATH_MAX <= (fnmo->name_offset + de.d_namlen))
		return; /* Too long. */
	memcpy((fnmo->path + fnmo->name_offset), de.d_name, de.d_namlen);
	fnmo->path_size = (fnmo->name_offset + de.d_namlen);
	/* Add last '/' for dir. */
	fnmo->path[fnmo->path_size] = '/';
	fnmo->path_size += (size_t)fnmo->is_dir;
	fnmo->path[fnmo->path_size] = 0;
	/* Notify. */
	kfnm->cb_func(kfnm, fnmo, fnmo->udata, KF_EVENT_RENAMED,
	    fnmo->path, old_filename, de.d_name);
}


static void
kq_fnm_delay_call_process(kq_fnm_p kfnm, kq_msg_cb forced_msg_cb) {
	ssize_t ios;
	kq_fnm_msg_pkt_t msg;

	for (;;) {
		ios = read(kfnm->pfd[0], &msg, sizeof(msg));
		if (0 >= ios)
			return;
		if (sizeof(msg) != ios ||
		    KF_MSG_PKT_MAGIC != msg.magic ||
		    (((size_t)msg.msg_cb) ^ ((size_t)msg.arg)) != msg.chk_sum)
			continue;
		if (NULL != forced_msg_cb) {
			forced_msg_cb(msg.arg);
			continue;
		}
		if (NULL == msg.msg_cb)
			continue;
		msg.msg_cb(msg.arg);
	}
}

static int
kq_fnm_delay_call(kq_fnm_p kfnm, kq_msg_cb msg_cb,
    void *arg) {
	kq_fnm_msg_pkt_t msg;

	if (NULL == kfnm || NULL == arg)
		return (EINVAL);
	msg.magic = KF_MSG_PKT_MAGIC;
	msg.msg_cb = msg_cb;
	msg.arg = arg;
	msg.chk_sum = (((size_t)msg.msg_cb) ^ ((size_t)msg.arg));
	if (sizeof(msg) == write(kfnm->pfd[1], &msg, sizeof(msg)))
		return (0);
	return (errno);
}


static void
kq_fnm_free_cb(void *arg) {
	kq_fnm_p kfnm = arg;

	if (NULL == kfnm)
		return;
	close(kfnm->fd);
	kfnm->fd = -1;
}
void
kq_fnm_free(kq_fnm_p kfnm) {

	if (NULL == kfnm)
		return;
	kq_fnm_delay_call(kfnm, kq_fnm_free_cb, kfnm);
	pthread_join(kfnm->tid, NULL);
	/* Free all in delay calls queue. */
	close(kfnm->pfd[1]);
	kq_fnm_delay_call_process(kfnm, kq_fnmo_free);
	close(kfnm->pfd[0]);
	free(kfnm);
}

kq_fnm_p
kq_fnm_create(kq_file_mon_settings_p s, kfnm_event_handler_cb cb_func) {
	kq_fnm_p kfnm;
	struct kevent kev;

	if (NULL == s || NULL == cb_func)
		return (NULL);
	kfnm = zalloc(sizeof(kq_fnm_t));
	if (NULL == kfnm)
		return (NULL);
	kfnm->fd = kqueue();
	if (-1 == kfnm->fd)
		goto err_out;
	if (-1 == pipe2(kfnm->pfd, O_NONBLOCK))
		goto err_out;
	kfnm->cb_func = cb_func;
	memcpy(&kfnm->s, s, sizeof(kq_file_mon_settings_t));
	if (kfnm->s.rate_limit_time_init >= kfnm->s.rate_limit_time_max) {
		kfnm->s.rate_limit_time_max = kfnm->s.rate_limit_time_init;
	}
	if (0 == kfnm->s.rate_limit_time_mul) {
		kfnm->s.rate_limit_time_mul ++;
	}
	kfnm->rate_lim_time_init = MSTOSBT(kfnm->s.rate_limit_time_init);

	EV_SET(&kev, kfnm->pfd[0], EVFILT_READ, EV_ADD, 0, 0, NULL);
	if (-1 == kevent(kfnm->fd, &kev, 1, NULL, 0, NULL) ||
	    0 != (EV_ERROR & kev.flags))
		goto err_out;
	if (0 != pthread_create(&kfnm->tid, NULL,
	    kq_fnm_proccess_events_proc, kfnm))
		goto err_out;

	return (kfnm);

err_out:
	kq_fnm_free(kfnm);
	return (NULL);
}

kq_fnmo_p
kq_fnm_add(kq_fnm_p kfnm, const char *path, void *udata) {
	int error;
	kq_fnmo_p fnmo;

	if (NULL == kfnm || NULL == path)
		return (NULL);
	fnmo = kq_fnmo_alloc(kfnm, path, udata);
	if (NULL == fnmo)
		return (NULL);
	/* Shedule delay call to init. */
	error = kq_fnm_delay_call(kfnm, kq_fnmo_init, fnmo);
	if (0 != error) { /* Error, do no directly init to avoid freezes. */
		kq_fnmo_free(fnmo);
		return (NULL);
	}
	return (fnmo);
}

void
kq_fnm_del(kq_fnm_p kfnm, kq_fnmo_p fnmo) {
	int error;

	if (NULL == kfnm || NULL == fnmo)
		return;
	/* Cancel notifications. */
	kq_fnmo_rate_lim_stop(fnmo);
	close(fnmo->fd);
	fnmo->fd = -1;
	/* Shedule delay call to free. */
	error = kq_fnm_delay_call(kfnm, kq_fnmo_free, fnmo);
	if (0 == error)
		return;
	/* Error, free directly. */
	kq_fnmo_free(fnmo);
}


static void
kq_fnm_proccess_event(kq_fnm_p kfnm, struct kevent *kev) {
	kq_fnmo_p fnmo;
	file_info_p fi;
	size_t i;
	int is_rate_lim_checked = 0;
	struct stat sb;

	if (NULL == kfnm || NULL == kev)
		return;

	/* Handle delay calls. */
	if (kev->ident == (uintptr_t)kfnm->pfd[0]) {
		if (kev->filter == EVFILT_READ) {
			kq_fnm_delay_call_process(kfnm, NULL);
		}
		return;
	}

	if (0 == kev->udata)
		return; /* No associated data, skip. */
	fnmo = (kq_fnmo_p)kev->udata;

	/* FS delayed notifications. */
	if (EVFILT_TIMER == kev->filter) {
		if (0 == fnmo->rate_lim_ev_cnt) {
			/* No delayed events, disable rate limit polling. */
			kq_fnmo_rate_lim_stop(fnmo);
			return;
		}
		fnmo->rate_lim_ev_cnt = 0; /* Reset counter. */
		kq_fnmo_rate_lim_shedule_next(fnmo);
		kq_handle_changes(kfnm, fnmo);
		return;
	}

	/* FS notifications. */
	if (EVFILT_VNODE != kev->filter)
		return; /* Unknown event, skip. */
	/* Subdir/file */
	if (kev->ident != (uintptr_t)fnmo->fd) {
		/* Is files changes rate limited? */
		if (1 == kq_fnmo_rate_lim_check(fnmo))
			return;
		is_rate_lim_checked ++;
		/* Try to find file and check it, without call kq_handle_changes(). */
		fi = NULL;
		for (i = 0; i < fnmo->files_count; i ++) {
			if (kev->ident != (uintptr_t)fnmo->files[i].fd)
				continue;
			fi = &fnmo->files[i];
			break;
		}
		if (NULL != fi) {
			/* Get file attrs. */
			if (0 != fstat(fi->fd, &sb)) {
				memset(&sb, 0x00, sizeof(struct stat));
			}
			/* Is modified? */
			if (0 != memcmp(&fi->sb, &sb, sizeof(struct stat))) {
				memcpy(&fi->sb, &sb, sizeof(struct stat));
				kfnm->cb_func(kfnm, fnmo, fnmo->udata, KF_EVENT_CHANGED,
				    fnmo->path, fi->de.d_name, NULL);
				return;
			}
		}
		/* fd not found or changes not found, rescan dir. */
		kev->fflags = NOTE_WRITE;
	}
	/* Monitored object. */
	/* All flags from EVFILT_VNODE_FLAGS_ALL must be handled here. */
	if (EV_ERROR & kev->flags) {
		kev->fflags |= NOTE_REVOKE; /* Treat error as unmount. */
	}
	if (NOTE_RENAME & kev->fflags) {
		kq_handle_rename(kfnm, fnmo);
	}
	if ((NOTE_WRITE | NOTE_EXTEND | NOTE_ATTRIB | NOTE_LINK | NOTE_CLOSE_WRITE) & kev->fflags) {
		/* Only count changes, do not prevent NOTE_DELETE event handling. */
		if (0 == is_rate_lim_checked &&
		    1 != kq_fnmo_rate_lim_check(fnmo)) {
			kq_handle_changes(kfnm, fnmo);
		}
	}
	if ((NOTE_DELETE | NOTE_REVOKE) & kev->fflags) {
		/* No report about childs. */
		kq_fnmo_clean(fnmo);
		kfnm->cb_func(kfnm, fnmo, fnmo->udata, KF_EVENT_DELETED,
		    fnmo->path, "", NULL);
	}
}

void *
kq_fnm_proccess_events_proc(void *data) {
	struct kevent kev;
	kq_fnm_p kfnm = data;

	if (NULL == kfnm)
		return (NULL);
	/* Get and proccess events, no wait. */
	while (0 < kevent(kfnm->fd, NULL, 0, &kev, 1, NULL)) {
		kq_fnm_proccess_event(kfnm, &kev);
	}
	return (NULL);
}
