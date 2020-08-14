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

#include <fcntl.h>
#include <gio/gfileinfo.h>
#include <gio/gfile.h>
#include <glib/glib-private.h>
#include <glib/gstdio.h>
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

#ifdef G_OS_WIN32
/* We want 64-bit file size, file ID and symlink support */
#define GLocalFileStat GWin32PrivateStat
#else
#define GLocalFileStat struct stat
#endif

/* If the system doesn’t have statx() support, emulate it using traditional
 * stat(). It supports fields %G_LOCAL_FILE_STAT_FIELD_BASIC_STATS only, and
 * always returns all of them. */
typedef enum
{
  G_LOCAL_FILE_STAT_FIELD_TYPE = (1 << 0),
  G_LOCAL_FILE_STAT_FIELD_MODE = (1 << 1),
  G_LOCAL_FILE_STAT_FIELD_NLINK = (1 << 2),
  G_LOCAL_FILE_STAT_FIELD_UID = (1 << 3),
  G_LOCAL_FILE_STAT_FIELD_GID = (1 << 4),
  G_LOCAL_FILE_STAT_FIELD_ATIME = (1 << 5),
  G_LOCAL_FILE_STAT_FIELD_MTIME = (1 << 6),
  G_LOCAL_FILE_STAT_FIELD_CTIME = (1 << 7),
  G_LOCAL_FILE_STAT_FIELD_INO = (1 << 8),
  G_LOCAL_FILE_STAT_FIELD_SIZE = (1 << 9),
  G_LOCAL_FILE_STAT_FIELD_BLOCKS = (1 << 10),
  G_LOCAL_FILE_STAT_FIELD_BTIME = (1 << 11),
} GLocalFileStatField;

#define G_LOCAL_FILE_STAT_FIELD_BASIC_STATS \
  (G_LOCAL_FILE_STAT_FIELD_TYPE | G_LOCAL_FILE_STAT_FIELD_MODE | \
   G_LOCAL_FILE_STAT_FIELD_NLINK | G_LOCAL_FILE_STAT_FIELD_UID | \
   G_LOCAL_FILE_STAT_FIELD_GID | G_LOCAL_FILE_STAT_FIELD_ATIME | \
   G_LOCAL_FILE_STAT_FIELD_MTIME | G_LOCAL_FILE_STAT_FIELD_CTIME | \
   G_LOCAL_FILE_STAT_FIELD_INO | G_LOCAL_FILE_STAT_FIELD_SIZE | \
   G_LOCAL_FILE_STAT_FIELD_BLOCKS)
#define G_LOCAL_FILE_STAT_FIELD_ALL (G_LOCAL_FILE_STAT_FIELD_BASIC_STATS | G_LOCAL_FILE_STAT_FIELD_BTIME)

static inline int
g_local_file_fstat (int                  fd,
                    GLocalFileStatField  mask,
                    GLocalFileStatField  mask_required,
                    GLocalFileStat      *stat_buf)
{
  if ((G_LOCAL_FILE_STAT_FIELD_BASIC_STATS & (mask_required & mask)) != (mask_required & mask))
    {
      /* Only G_LOCAL_FILE_STAT_FIELD_BASIC_STATS are supported. */
      errno = ERANGE;
      return -1;
    }

#ifdef G_OS_WIN32
  return GLIB_PRIVATE_CALL (g_win32_fstat) (fd, stat_buf);
#else
  return fstat (fd, stat_buf);
#endif
}

static inline int
g_local_file_fstatat (int                  fd,
                      const char          *path,
                      int                  flags,
                      GLocalFileStatField  mask,
                      GLocalFileStatField  mask_required,
                      GLocalFileStat      *stat_buf)
{
  if ((G_LOCAL_FILE_STAT_FIELD_BASIC_STATS & (mask_required & mask)) != (mask_required & mask))
    {
      /* Only G_LOCAL_FILE_STAT_FIELD_BASIC_STATS are supported. */
      errno = ERANGE;
      return -1;
    }

#ifdef G_OS_WIN32
  /* Currently not supported on Windows */
  errno = ENOSYS;
  return -1;
#else
  return fstatat (fd, path, stat_buf, flags);
#endif
}

static inline int
g_local_file_lstat (const char          *path,
                    GLocalFileStatField  mask,
                    GLocalFileStatField  mask_required,
                    GLocalFileStat      *stat_buf)
{
  if ((G_LOCAL_FILE_STAT_FIELD_BASIC_STATS & (mask_required & mask)) != (mask_required & mask))
    {
      /* Only G_LOCAL_FILE_STAT_FIELD_BASIC_STATS are supported. */
      errno = ERANGE;
      return -1;
    }

#ifdef G_OS_WIN32
  return GLIB_PRIVATE_CALL (g_win32_lstat_utf8) (path, stat_buf);
#else
  return g_lstat (path, stat_buf);
#endif
}

static inline int
g_local_file_stat (const char          *path,
                   GLocalFileStatField  mask,
                   GLocalFileStatField  mask_required,
                   GLocalFileStat      *stat_buf)
{
  if ((G_LOCAL_FILE_STAT_FIELD_BASIC_STATS & (mask_required & mask)) != (mask_required & mask))
    {
      /* Only G_LOCAL_FILE_STAT_FIELD_BASIC_STATS are supported. */
      errno = ERANGE;
      return -1;
    }

#ifdef G_OS_WIN32
  return GLIB_PRIVATE_CALL (g_win32_stat_utf8) (path, stat_buf);
#else
  return stat (path, stat_buf);
#endif
}

inline static gboolean  _g_stat_has_field  (const GLocalFileStat *buf, GLocalFileStatField field) { return (G_LOCAL_FILE_STAT_FIELD_BASIC_STATS & field) == field; }

#ifndef G_OS_WIN32
inline static mode_t    _g_stat_mode      (const GLocalFileStat *buf) { return buf->st_mode; }
inline static nlink_t   _g_stat_nlink     (const GLocalFileStat *buf) { return buf->st_nlink; }
#else
inline static guint16   _g_stat_mode      (const GLocalFileStat *buf) { return buf->st_mode; }
inline static guint32   _g_stat_nlink     (const GLocalFileStat *buf) { return buf->st_nlink; }
#endif
inline static dev_t     _g_stat_dev       (const GLocalFileStat *buf) { return buf->st_dev; }
inline static ino_t     _g_stat_ino       (const GLocalFileStat *buf) { return buf->st_ino; }
inline static off_t     _g_stat_size      (const GLocalFileStat *buf) { return buf->st_size; }

#ifndef G_OS_WIN32
inline static uid_t     _g_stat_uid       (const GLocalFileStat *buf) { return buf->st_uid; }
inline static gid_t     _g_stat_gid       (const GLocalFileStat *buf) { return buf->st_gid; }
inline static dev_t     _g_stat_rdev      (const GLocalFileStat *buf) { return buf->st_rdev; }
inline static blksize_t _g_stat_blksize   (const GLocalFileStat *buf) { return buf->st_blksize; }
#else
inline static guint16   _g_stat_uid       (const GLocalFileStat *buf) { return buf->st_uid; }
inline static guint16   _g_stat_gid       (const GLocalFileStat *buf) { return buf->st_gid; }
#endif

#ifdef HAVE_STRUCT_STAT_ST_BLOCKS
inline static blkcnt_t  _g_stat_blocks    (const GLocalFileStat *buf) { return buf->st_blocks; }
#endif

#ifndef G_OS_WIN32
inline static time_t    _g_stat_atime     (const GLocalFileStat *buf) { return buf->st_atime; }
inline static time_t    _g_stat_ctime     (const GLocalFileStat *buf) { return buf->st_ctime; }
inline static time_t    _g_stat_mtime     (const GLocalFileStat *buf) { return buf->st_mtime; }
#endif
#ifdef HAVE_STRUCT_STAT_ST_MTIM_TV_NSEC
inline static guint32   _g_stat_atim_nsec (const GLocalFileStat *buf) { return buf->st_atim.tv_nsec; }
inline static guint32   _g_stat_ctim_nsec (const GLocalFileStat *buf) { return buf->st_ctim.tv_nsec; }
inline static guint32   _g_stat_mtim_nsec (const GLocalFileStat *buf) { return buf->st_mtim.tv_nsec; }
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
