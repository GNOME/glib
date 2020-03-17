/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright (C) 2006-2007 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Alexander Larsson <alexl@redhat.com>
 */

#ifndef __G_LOCAL_FILE_INFO_H__
#define __G_LOCAL_FILE_INFO_H__

#include <gio/gfileinfo.h>
#include <gio/gfile.h>
#include <glib/gstdioprivate.h>
#include <sys/stat.h>
#include <sys/types.h>

G_BEGIN_DECLS

typedef struct
{
  gboolean writable;
  gboolean is_sticky;
  gboolean has_trash_dir;
  int      owner;
  dev_t    device;
  ino_t    inode;
  gpointer extra_data;
  GDestroyNotify free_extra_data;
} GLocalParentFileInfo;

#ifdef HAVE_STATX
#define GLocalFileStat struct statx

#define FSTAT(fd, stat)   statx (fd, "", AT_EMPTY_PATH, STATX_BASIC_STATS, stat)
#define LSTAT(path, stat) statx (AT_FDCWD, path, \
                                 AT_NO_AUTOMOUNT | AT_SYMLINK_NOFOLLOW | AT_STATX_SYNC_AS_STAT, \
                                 STATX_BASIC_STATS | STATX_BTIME, stat)
#define STAT(path, stat)  statx (AT_FDCWD, path, \
                                 AT_NO_AUTOMOUNT | AT_STATX_SYNC_AS_STAT, \
                                 STATX_BASIC_STATS | STATX_BTIME, stat)

inline static guint16 _g_stat_mode        (const GLocalFileStat *buf) { return buf->stx_mode; }
inline static guint32 _g_stat_nlink       (const GLocalFileStat *buf) { return buf->stx_nlink; }
inline static guint32 _g_stat_dev         (const GLocalFileStat *buf) { return buf->stx_dev_major; }
inline static guint64 _g_stat_ino         (const GLocalFileStat *buf) { return buf->stx_ino; }
inline static guint64 _g_stat_size        (const GLocalFileStat *buf) { return buf->stx_size; }

inline static guint32 _g_stat_uid         (const GLocalFileStat *buf) { return buf->stx_uid; }
inline static guint32 _g_stat_gid         (const GLocalFileStat *buf) { return buf->stx_gid; }
inline static guint32 _g_stat_rdev        (const GLocalFileStat *buf) { return buf->stx_rdev_major; }
inline static guint32 _g_stat_blksize     (const GLocalFileStat *buf) { return buf->stx_blksize; }

inline static guint64 _g_stat_blocks      (const GLocalFileStat *buf) { return buf->stx_blocks; }

inline static gint64  _g_stat_atime       (const GLocalFileStat *buf) { return buf->stx_atime.tv_sec; }
inline static gint64  _g_stat_ctime       (const GLocalFileStat *buf) { return buf->stx_ctime.tv_sec; }
inline static gint64  _g_stat_mtime       (const GLocalFileStat *buf) { return buf->stx_mtime.tv_sec; }
inline static guint32 _g_stat_atim_nsec   (const GLocalFileStat *buf) { return buf->stx_atime.tv_nsec; }
inline static guint32 _g_stat_ctim_nsec   (const GLocalFileStat *buf) { return buf->stx_ctime.tv_nsec; }
inline static guint32 _g_stat_mtim_nsec   (const GLocalFileStat *buf) { return buf->stx_mtime.tv_nsec; }

#else
#if defined (G_OS_WIN32)
/* We want 64-bit file size, file ID and symlink support */
#define GLocalFileStat GWin32PrivateStat
#define FSTAT(fd, stat)   GLIB_PRIVATE_CALL (g_win32_fstat)      (fd, stat)
#define LSTAT(path, stat) GLIB_PRIVATE_CALL (g_win32_lstat_utf8) (path, stat)
#define STAT(path, stat)  GLIB_PRIVATE_CALL (g_win32_stat_utf8)  (path, stat)
#else
#define GLocalFileStat struct stat
#define FSTAT(fd, stat)   fstat   (fd, stat)
#define LSTAT(path, stat) g_lstat (path, stat)
#define STAT(path, stat)  stat    (path, stat)
#endif

inline static mode_t    _g_stat_mode      (const GLocalFileStat *buf) { return buf->st_mode; }
inline static nlink_t   _g_stat_nlink     (const GLocalFileStat *buf) { return buf->st_nlink; }
inline static dev_t     _g_stat_dev       (const GLocalFileStat *buf) { return buf->st_dev; }
inline static ino_t     _g_stat_ino       (const GLocalFileStat *buf) { return buf->st_ino; }
inline static off_t     _g_stat_size      (const GLocalFileStat *buf) { return buf->st_size; }

#ifndef G_OS_WIN32
inline static uid_t     _g_stat_uid       (const GLocalFileStat *buf) { return buf->st_uid; }
inline static gid_t     _g_stat_gid       (const GLocalFileStat *buf) { return buf->st_gid; }
inline static dev_t     _g_stat_rdev      (const GLocalFileStat *buf) { return buf->st_rdev; }
inline static blksize_t _g_stat_blksize   (const GLocalFileStat *buf) { return buf->st_blksize; }
#endif

#ifdef HAVE_STRUCT_STAT_ST_BLOCKS
inline static blkcnt_t  _g_stat_blocks    (const GLocalFileStat *buf) { return buf->st_blocks; }
#endif

inline static time_t    _g_stat_atime     (const GLocalFileStat *buf) { return buf->st_atime; }
inline static time_t    _g_stat_ctime     (const GLocalFileStat *buf) { return buf->st_ctime; }
inline static time_t    _g_stat_mtime     (const GLocalFileStat *buf) { return buf->st_mtime; }
#ifdef HAVE_STRUCT_STAT_ST_MTIM_TV_NSEC
inline static guint32   _g_stat_atim_nsec (const GLocalFileStat *buf) { return buf->st_atim.tv_nsec; }
inline static guint32   _g_stat_ctim_nsec (const GLocalFileStat *buf) { return buf->st_ctim.tv_nsec; }
inline static guint32   _g_stat_mtim_nsec (const GLocalFileStat *buf) { return buf->st_mtim.tv_nsec; }
#endif

#endif

#define G_LOCAL_FILE_INFO_NOSTAT_ATTRIBUTES \
    G_FILE_ATTRIBUTE_STANDARD_NAME "," \
    G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME "," \
    G_FILE_ATTRIBUTE_STANDARD_EDIT_NAME "," \
    G_FILE_ATTRIBUTE_STANDARD_COPY_NAME

gboolean   _g_local_file_has_trash_dir        (const char             *dirname,
                                               dev_t                   dir_dev);
#ifdef G_OS_UNIX
gboolean   _g_local_file_is_lost_found_dir    (const char             *path,
                                               dev_t                   path_dev);
#endif
void       _g_local_file_info_get_parent_info (const char             *dir,
                                               GFileAttributeMatcher  *attribute_matcher,
                                               GLocalParentFileInfo   *parent_info);
void       _g_local_file_info_free_parent_info (GLocalParentFileInfo   *parent_info);
void       _g_local_file_info_get_nostat      (GFileInfo              *info,
                                               const char             *basename,
                                               const char             *path,
                                               GFileAttributeMatcher  *attribute_matcher);
GFileInfo *_g_local_file_info_get             (const char             *basename,
                                               const char             *path,
                                               GFileAttributeMatcher  *attribute_matcher,
                                               GFileQueryInfoFlags     flags,
                                               GLocalParentFileInfo   *parent_info,
                                               GError                **error);
GFileInfo *_g_local_file_info_get_from_fd     (int                     fd,
                                               const char             *attributes,
                                               GError                **error);
char *     _g_local_file_info_create_etag     (GLocalFileStat         *statbuf);
gboolean   _g_local_file_info_set_attribute   (char                   *filename,
                                               const char             *attribute,
                                               GFileAttributeType      type,
                                               gpointer                value_p,
                                               GFileQueryInfoFlags     flags,
                                               GCancellable           *cancellable,
                                               GError                **error);
gboolean   _g_local_file_info_set_attributes  (char                   *filename,
                                               GFileInfo              *info,
                                               GFileQueryInfoFlags     flags,
                                               GCancellable           *cancellable,
                                               GError                **error);

G_END_DECLS

#endif /* __G_FILE_LOCAL_FILE_INFO_H__ */


