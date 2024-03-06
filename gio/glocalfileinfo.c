/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* GIO - GLib Input, Output and Streaming Library
 * 
 * Copyright (C) 2006-2007 Red Hat, Inc.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
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

#include "config.h"

#include <glib.h>

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#ifdef G_OS_UNIX
#include <grp.h>
#include <pwd.h>
#endif
#ifdef HAVE_SELINUX
#include <selinux/selinux.h>
#endif

#ifdef HAVE_XATTR

#if defined HAVE_SYS_XATTR_H
  #include <sys/xattr.h>
#elif defined HAVE_ATTR_XATTR_H
  #include <attr/xattr.h>
#else
  #error "Neither <sys/xattr.h> nor <attr/xattr.h> is present but extended attribute support is enabled."
#endif /* defined HAVE_SYS_XATTR_H || HAVE_ATTR_XATTR_H */

#endif /* HAVE_XATTR */

#include <glib/gstdio.h>
#include <glib/gstdioprivate.h>
#include <gfileattribute-priv.h>
#include <gfileinfo-priv.h>
#include <gvfs.h>

#ifdef G_OS_UNIX
#include <unistd.h>
#include "glib-unix.h"
#endif

#include "glib-private.h"

#include "thumbnail-verify.h"

#ifdef G_OS_WIN32
#include <windows.h>
#include <io.h>
#ifndef W_OK
#define W_OK 2
#endif
#ifndef R_OK
#define R_OK 4
#endif
#ifndef X_OK
#define X_OK 0 /* not really */
#endif
#ifndef S_ISREG
#define S_ISREG(m) (((m) & _S_IFMT) == _S_IFREG)
#endif
#ifndef S_ISDIR
#define S_ISDIR(m) (((m) & _S_IFMT) == _S_IFDIR)
#endif
#ifndef S_IXUSR
#define S_IXUSR _S_IEXEC
#endif
#endif

#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#endif

#include "glocalfileinfo.h"
#include "gioerror.h"
#include "gthemedicon.h"
#include "gcontenttypeprivate.h"
#include "glibintl.h"


struct ThumbMD5Context {
	guint32 buf[4];
	guint32 bits[2];
	unsigned char in[64];
};

#ifndef G_OS_WIN32

typedef struct {
  char *user_name;
  char *real_name;
} UidData;

G_LOCK_DEFINE_STATIC (uid_cache);
static GHashTable *uid_cache = NULL;

G_LOCK_DEFINE_STATIC (gid_cache);
static GHashTable *gid_cache = NULL;

#endif  /* !G_OS_WIN32 */

char *
_g_local_file_info_create_etag (GLocalFileStat *statbuf)
{
  glong sec, usec, nsec;

  g_return_val_if_fail (_g_stat_has_field (statbuf, G_LOCAL_FILE_STAT_FIELD_MTIME), NULL);

  sec = _g_stat_mtime (statbuf);
  usec = _g_stat_mtim_nsec (statbuf) / 1000;
  nsec = _g_stat_mtim_nsec (statbuf);

  return g_strdup_printf ("%lu:%lu:%lu", sec, usec, nsec);
}

static char *
_g_local_file_info_create_file_id (GLocalFileStat *statbuf)
{
  guint64 ino;
#ifdef G_OS_WIN32
  ino = statbuf->file_index;
#else
  ino = _g_stat_ino (statbuf);
#endif
  return g_strdup_printf ("l%" G_GUINT64_FORMAT ":%" G_GUINT64_FORMAT,
			  (guint64) _g_stat_dev (statbuf),
			  ino);
}

static char *
_g_local_file_info_create_fs_id (GLocalFileStat *statbuf)
{
  return g_strdup_printf ("l%" G_GUINT64_FORMAT,
			  (guint64) _g_stat_dev (statbuf));
}

#if defined (S_ISLNK) || defined (G_OS_WIN32)

static gchar *
read_link (const gchar *full_name)
{
#if defined (HAVE_READLINK)
  gchar *buffer;
  gsize size;
  
  size = 256;
  buffer = g_malloc (size);
  
  while (1)
    {
      gssize read_size;

      read_size = readlink (full_name, buffer, size);
      if (read_size < 0)
	{
	  g_free (buffer);
	  return NULL;
	}
      if ((gsize) read_size < size)
	{
	  buffer[read_size] = 0;
	  return buffer;
	}
      size *= 2;
      buffer = g_realloc (buffer, size);
    }
#elif defined (G_OS_WIN32)
  gchar *buffer;
  int read_size;

  read_size = GLIB_PRIVATE_CALL (g_win32_readlink_utf8) (full_name, NULL, 0, &buffer, TRUE);
  if (read_size < 0)
    return NULL;
  else if (read_size == 0)
    return strdup ("");
  else
    return buffer;
#else
  return NULL;
#endif
}

#endif  /* S_ISLNK || G_OS_WIN32 */

#ifdef HAVE_SELINUX
/* Get the SELinux security context */
static void
get_selinux_context (const char            *path,
		     GFileInfo             *info,
		     GFileAttributeMatcher *attribute_matcher,
		     gboolean               follow_symlinks)
{
  char *context;

  if (!_g_file_attribute_matcher_matches_id (attribute_matcher, G_FILE_ATTRIBUTE_ID_SELINUX_CONTEXT))
    return;
  
  if (is_selinux_enabled ())
    {
      if (follow_symlinks)
	{
	  if (lgetfilecon_raw (path, &context) < 0)
	    return;
	}
      else
	{
	  if (getfilecon_raw (path, &context) < 0)
	    return;
	}

      if (context)
	{
	  _g_file_info_set_attribute_string_by_id (info, G_FILE_ATTRIBUTE_ID_SELINUX_CONTEXT, context);
	  freecon (context);
	}
    }
}
#endif

#ifdef HAVE_XATTR

/* Wrappers to hide away differences between (Linux) getxattr/lgetxattr and
 * (Mac) getxattr(..., XATTR_NOFOLLOW)
 */
#ifdef HAVE_XATTR_NOFOLLOW
#define g_fgetxattr(fd,name,value,size)  fgetxattr(fd,name,value,size,0,0)
#define g_flistxattr(fd,name,size)       flistxattr(fd,name,size,0)
#define g_setxattr(path,name,value,size) setxattr(path,name,value,size,0,0)
#define g_removexattr(path,name) removexattr(path,name,0)
#else
#define g_fgetxattr     fgetxattr
#define g_flistxattr    flistxattr
#define g_setxattr(path,name,value,size) setxattr(path,name,value,size,0)
#define g_removexattr(path,name) removexattr(path,name)
#endif

static gssize
g_getxattr (const char *path, const char *name, void *value, size_t size,
            gboolean follow_symlinks)
{
#ifdef HAVE_XATTR_NOFOLLOW
  return getxattr (path, name, value, size, 0, follow_symlinks ? 0 : XATTR_NOFOLLOW);
#else
  if (follow_symlinks)
    return getxattr (path, name, value, size);
  else
    return lgetxattr (path, name, value, size);
#endif
}

static gssize
g_listxattr(const char *path, char *namebuf, size_t size,
            gboolean follow_symlinks)
{
#ifdef HAVE_XATTR_NOFOLLOW
  return listxattr (path, namebuf, size, follow_symlinks ? 0 : XATTR_NOFOLLOW);
#else
  if (follow_symlinks)
    return listxattr (path, namebuf, size);
  else
    return llistxattr (path, namebuf, size);
#endif
}

static gboolean
valid_char (char c)
{
  return c >= 32 && c <= 126 && c != '\\';
}

static gboolean
name_is_valid (const char *str)
{
  while (*str)
    {
      if (!valid_char (*str++))
	return FALSE;
    }
  return TRUE;
}

static char *
hex_escape_buffer (const char *str,
                   size_t      len,
                   gboolean   *free_return)
{
  size_t num_invalid, i;
  char *escaped_str, *p;
  unsigned char c;
  static char *hex_digits = "0123456789abcdef";

  num_invalid = 0;
  for (i = 0; i < len; i++)
    {
      if (!valid_char (str[i]))
	num_invalid++;
    }

  if (num_invalid == 0)
    {
      *free_return = FALSE;
      return (char *)str;
    }

  escaped_str = g_malloc (len + num_invalid*3 + 1);

  p = escaped_str;
  for (i = 0; i < len; i++)
    {
      if (valid_char (str[i]))
	*p++ = str[i];
      else
	{
	  c = str[i];
	  *p++ = '\\';
	  *p++ = 'x';
	  *p++ = hex_digits[(c >> 4) & 0xf];
	  *p++ = hex_digits[c & 0xf];
	}
    }
  *p = 0;

  *free_return = TRUE;
  return escaped_str;
}

static char *
hex_escape_string (const char *str,
                   gboolean   *free_return)
{
  return hex_escape_buffer (str, strlen (str), free_return);
}

static char *
hex_unescape_string (const char *str, 
                     int        *out_len, 
                     gboolean   *free_return)
{
  int i;
  char *unescaped_str, *p;
  unsigned char c;
  int len;

  len = strlen (str);
  
  if (strchr (str, '\\') == NULL)
    {
      if (out_len)
	*out_len = len;
      *free_return = FALSE;
      return (char *)str;
    }
  
  unescaped_str = g_malloc (len + 1);

  p = unescaped_str;
  for (i = 0; i < len; i++)
    {
      if (str[i] == '\\' &&
	  str[i+1] == 'x' &&
	  len - i >= 4)
	{
	  c =
	    (g_ascii_xdigit_value (str[i+2]) << 4) |
	    g_ascii_xdigit_value (str[i+3]);
	  *p++ = c;
	  i += 3;
	}
      else
	*p++ = str[i];
    }
  if (out_len)
    *out_len = p - unescaped_str;
  *p++ = 0;

  *free_return = TRUE;
  return unescaped_str;
}

static void
escape_xattr (GFileInfo  *info,
	      const char *gio_attr, /* gio attribute name */
	      const char *value, /* Is zero terminated */
	      size_t      len /* not including zero termination */)
{
  char *escaped_val;
  gboolean free_escaped_val;
  
  escaped_val = hex_escape_buffer (value, len, &free_escaped_val);
  
  g_file_info_set_attribute_string (info, gio_attr, escaped_val);
  
  if (free_escaped_val)
    g_free (escaped_val);
}

static void
get_one_xattr (const char *path,
	       GFileInfo  *info,
	       const char *gio_attr,
	       const char *xattr,
	       gboolean    follow_symlinks)
{
  char value[64];
  char *value_p;
  gssize len;
  int errsv;

  len = g_getxattr (path, xattr, value, sizeof (value)-1, follow_symlinks);
  errsv = errno;

  value_p = NULL;
  if (len >= 0)
    value_p = value;
  else if (len == -1 && errsv == ERANGE)
    {
      len = g_getxattr (path, xattr, NULL, 0, follow_symlinks);

      if (len < 0)
	return;

      value_p = g_malloc (len+1);

      len = g_getxattr (path, xattr, value_p, len, follow_symlinks);

      if (len < 0)
	{
	  g_free (value_p);
	  return;
	}
    }
  else
    return;
  
  /* Null terminate */
  value_p[len] = 0;

  escape_xattr (info, gio_attr, value_p, len);
  
  if (value_p != value)
    g_free (value_p);
}

#endif /* defined HAVE_XATTR */

static void
get_xattrs (const char            *path,
	    gboolean               user,
	    GFileInfo             *info,
	    GFileAttributeMatcher *matcher,
	    gboolean               follow_symlinks)
{
#ifdef HAVE_XATTR
  gboolean all;
  gsize list_size;
  gssize list_res_size;
  size_t len;
  char *list;
  const char *attr, *attr2;

  if (user)
    all = g_file_attribute_matcher_enumerate_namespace (matcher, "xattr");
  else
    all = g_file_attribute_matcher_enumerate_namespace (matcher, "xattr-sys");

  if (all)
    {
      int errsv;

      list_res_size = g_listxattr (path, NULL, 0, follow_symlinks);

      if (list_res_size == -1 ||
	  list_res_size == 0)
	return;

      list_size = list_res_size;
      list = g_malloc (list_size);

    retry:
      
      list_res_size = g_listxattr (path, list, list_size, follow_symlinks);
      errsv = errno;
      
      if (list_res_size == -1 && errsv == ERANGE)
	{
	  list_size = list_size * 2;
	  list = g_realloc (list, list_size);
	  goto retry;
	}

      if (list_res_size == -1)
        {
          g_free (list);
          return;
        }

      attr = list;
      while (list_res_size > 0)
	{
	  if ((user && g_str_has_prefix (attr, "user.")) ||
	      (!user && !g_str_has_prefix (attr, "user.")))
	    {
	      char *escaped_attr, *gio_attr;
	      gboolean free_escaped_attr;
	      
	      if (user)
		{
		  escaped_attr = hex_escape_string (attr + 5, &free_escaped_attr);
		  gio_attr = g_strconcat ("xattr::", escaped_attr, NULL);
		}
	      else
		{
		  escaped_attr = hex_escape_string (attr, &free_escaped_attr);
		  gio_attr = g_strconcat ("xattr-sys::", escaped_attr, NULL);
		}
	      
	      if (free_escaped_attr)
		g_free (escaped_attr);
	      
	      get_one_xattr (path, info, gio_attr, attr, follow_symlinks);

	      g_free (gio_attr);
	    }
	      
	  len = strlen (attr) + 1;
	  attr += len;
	  list_res_size -= len;
	}

      g_free (list);
    }
  else
    {
      while ((attr = g_file_attribute_matcher_enumerate_next (matcher)) != NULL)
	{
	  char *unescaped_attribute, *a;
	  gboolean free_unescaped_attribute;

	  attr2 = strchr (attr, ':');
	  if (attr2)
	    {
	      attr2 += 2; /* Skip '::' */
	      unescaped_attribute = hex_unescape_string (attr2, NULL, &free_unescaped_attribute);
	      if (user)
		a = g_strconcat ("user.", unescaped_attribute, NULL);
	      else
		a = unescaped_attribute;
	      
	      get_one_xattr (path, info, attr, a, follow_symlinks);

	      if (user)
		g_free (a);
	      
	      if (free_unescaped_attribute)
		g_free (unescaped_attribute);
	    }
	}
    }
#endif /* defined HAVE_XATTR */
}

#ifdef HAVE_XATTR
static void
get_one_xattr_from_fd (int         fd,
		       GFileInfo  *info,
		       const char *gio_attr,
		       const char *xattr)
{
  char value[64];
  char *value_p;
  gssize len;
  int errsv;

  len = g_fgetxattr (fd, xattr, value, sizeof (value) - 1);
  errsv = errno;

  value_p = NULL;
  if (len >= 0)
    value_p = value;
  else if (len == -1 && errsv == ERANGE)
    {
      len = g_fgetxattr (fd, xattr, NULL, 0);

      if (len < 0)
	return;

      value_p = g_malloc (len + 1);

      len = g_fgetxattr (fd, xattr, value_p, len);

      if (len < 0)
	{
	  g_free (value_p);
	  return;
	}
    }
  else
    return;
  
  /* Null terminate */
  value_p[len] = 0;

  escape_xattr (info, gio_attr, value_p, len);
  
  if (value_p != value)
    g_free (value_p);
}
#endif /* defined HAVE_XATTR */

static void
get_xattrs_from_fd (int                    fd,
		    gboolean               user,
		    GFileInfo             *info,
		    GFileAttributeMatcher *matcher)
{
#ifdef HAVE_XATTR
  gboolean all;
  gsize list_size;
  gssize list_res_size;
  size_t len;
  char *list;
  const char *attr, *attr2;

  if (user)
    all = g_file_attribute_matcher_enumerate_namespace (matcher, "xattr");
  else
    all = g_file_attribute_matcher_enumerate_namespace (matcher, "xattr-sys");

  if (all)
    {
      int errsv;

      list_res_size = g_flistxattr (fd, NULL, 0);

      if (list_res_size == -1 ||
	  list_res_size == 0)
	return;

      list_size = list_res_size;
      list = g_malloc (list_size);

    retry:
      
      list_res_size = g_flistxattr (fd, list, list_size);
      errsv = errno;
      
      if (list_res_size == -1 && errsv == ERANGE)
	{
	  list_size = list_size * 2;
	  list = g_realloc (list, list_size);
	  goto retry;
	}

      if (list_res_size == -1)
        {
          g_free (list);
          return;
        }

      attr = list;
      while (list_res_size > 0)
	{
	  if ((user && g_str_has_prefix (attr, "user.")) ||
	      (!user && !g_str_has_prefix (attr, "user.")))
	    {
	      char *escaped_attr, *gio_attr;
	      gboolean free_escaped_attr;
	      
	      if (user)
		{
		  escaped_attr = hex_escape_string (attr + 5, &free_escaped_attr);
		  gio_attr = g_strconcat ("xattr::", escaped_attr, NULL);
		}
	      else
		{
		  escaped_attr = hex_escape_string (attr, &free_escaped_attr);
		  gio_attr = g_strconcat ("xattr-sys::", escaped_attr, NULL);
		}
	      
	      if (free_escaped_attr)
		g_free (escaped_attr);
	      
	      get_one_xattr_from_fd (fd, info, gio_attr, attr);
	      g_free (gio_attr);
	    }
	  
	  len = strlen (attr) + 1;
	  attr += len;
	  list_res_size -= len;
	}

      g_free (list);
    }
  else
    {
      while ((attr = g_file_attribute_matcher_enumerate_next (matcher)) != NULL)
	{
	  char *unescaped_attribute, *a;
	  gboolean free_unescaped_attribute;

	  attr2 = strchr (attr, ':');
	  if (attr2)
	    {
	      attr2++; /* Skip ':' */
	      unescaped_attribute = hex_unescape_string (attr2, NULL, &free_unescaped_attribute);
	      if (user)
		a = g_strconcat ("user.", unescaped_attribute, NULL);
	      else
		a = unescaped_attribute;
	      
	      get_one_xattr_from_fd (fd, info, attr, a);

	      if (user)
		g_free (a);
	      
	      if (free_unescaped_attribute)
		g_free (unescaped_attribute);
	    }
	}
    }
#endif /* defined HAVE_XATTR */
}

#ifdef HAVE_XATTR
static gboolean
set_xattr (char                       *filename,
	   const char                 *escaped_attribute,
	   const GFileAttributeValue  *attr_value,
	   GError                    **error)
{
  char *attribute, *value;
  gboolean free_attribute, free_value;
  int val_len, res, errsv;
  gboolean is_user;
  char *a;

  if (attr_value == NULL)
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                           _("Attribute value must be non-NULL"));
      return FALSE;
    }

  if (attr_value->type != G_FILE_ATTRIBUTE_TYPE_STRING && attr_value->type != G_FILE_ATTRIBUTE_TYPE_INVALID)
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                           _("Invalid attribute type (string or invalid expected)"));
      return FALSE;
    }

  if (!name_is_valid (escaped_attribute))
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                           _("Invalid extended attribute name"));
      return FALSE;
    }

  if (g_str_has_prefix (escaped_attribute, "xattr::"))
    {
      escaped_attribute += strlen ("xattr::");
      is_user = TRUE;
    }
  else
    {
      g_warn_if_fail (g_str_has_prefix (escaped_attribute, "xattr-sys::"));
      escaped_attribute += strlen ("xattr-sys::");
      is_user = FALSE;
    }

  attribute = hex_unescape_string (escaped_attribute, NULL, &free_attribute);

  if (is_user)
    a = g_strconcat ("user.", attribute, NULL);
  else
    a = attribute;

  if (attr_value->type == G_FILE_ATTRIBUTE_TYPE_STRING)
    {
      value = hex_unescape_string (attr_value->u.string, &val_len, &free_value);
      res = g_setxattr (filename, a, value, val_len);
    }
  else
    {
      value = NULL;
      val_len = 0;
      free_value = FALSE;
      res = g_removexattr (filename, a);
    }

  errsv = errno;
  
  if (is_user)
    g_free (a);
  
  if (free_attribute)
    g_free (attribute);
  
  if (free_value)
    g_free (value);

  if (res == -1)
    {
      g_set_error (error, G_IO_ERROR,
		   g_io_error_from_errno (errsv),
		   _("Error setting extended attribute “%s”: %s"),
		   escaped_attribute, g_strerror (errsv));
      return FALSE;
    }
  
  return TRUE;
}

#endif


void
_g_local_file_info_get_parent_info (const char            *dir,
				    GFileAttributeMatcher *attribute_matcher,
				    GLocalParentFileInfo  *parent_info)
{
  GStatBuf statbuf;
  int res;

  parent_info->extra_data = NULL;
  parent_info->free_extra_data = NULL;
  parent_info->writable = FALSE;
  parent_info->is_sticky = FALSE;
  parent_info->has_trash_dir = FALSE;
  parent_info->device = 0;
  parent_info->inode = 0;

  if (_g_file_attribute_matcher_matches_id (attribute_matcher, G_FILE_ATTRIBUTE_ID_ACCESS_CAN_RENAME) ||
      _g_file_attribute_matcher_matches_id (attribute_matcher, G_FILE_ATTRIBUTE_ID_ACCESS_CAN_DELETE) ||
      _g_file_attribute_matcher_matches_id (attribute_matcher, G_FILE_ATTRIBUTE_ID_ACCESS_CAN_TRASH) ||
      _g_file_attribute_matcher_matches_id (attribute_matcher, G_FILE_ATTRIBUTE_ID_UNIX_IS_MOUNTPOINT))
    {
      /* FIXME: Windows: The underlying _waccess() call in the C
       * library is mostly pointless as it only looks at the READONLY
       * FAT-style attribute of the file, it doesn't check the ACL at
       * all.
       */
      parent_info->writable = (g_access (dir, W_OK) == 0);
      
      res = g_stat (dir, &statbuf);

      /*
       * The sticky bit (S_ISVTX) on a directory means that a file in that directory can be
       * renamed or deleted only by the owner of the file, by the owner of the directory, and
       * by a privileged process.
       */
      if (res == 0)
	{
#ifdef S_ISVTX
	  parent_info->is_sticky = (statbuf.st_mode & S_ISVTX) != 0;
#else
	  parent_info->is_sticky = FALSE;
#endif
	  parent_info->owner = statbuf.st_uid;
	  parent_info->device = statbuf.st_dev;
	  parent_info->inode = statbuf.st_ino;
          /* No need to find trash dir if it's not writable anyway */
          if (parent_info->writable &&
              _g_file_attribute_matcher_matches_id (attribute_matcher, G_FILE_ATTRIBUTE_ID_ACCESS_CAN_TRASH))
            parent_info->has_trash_dir = _g_local_file_has_trash_dir (dir, statbuf.st_dev);
	}
    }
}

void
_g_local_file_info_free_parent_info (GLocalParentFileInfo *parent_info)
{
  if (parent_info->extra_data &&
      parent_info->free_extra_data)
    parent_info->free_extra_data (parent_info->extra_data);
}

static void
get_access_rights (GFileAttributeMatcher *attribute_matcher,
		   GFileInfo             *info,
		   const gchar           *path,
		   GLocalFileStat        *statbuf,
		   GLocalParentFileInfo  *parent_info)
{
  /* FIXME: Windows: The underlyin _waccess() is mostly pointless */
  if (_g_file_attribute_matcher_matches_id (attribute_matcher,
					    G_FILE_ATTRIBUTE_ID_ACCESS_CAN_READ))
    _g_file_info_set_attribute_boolean_by_id (info, G_FILE_ATTRIBUTE_ID_ACCESS_CAN_READ,
				             g_access (path, R_OK) == 0);
  
  if (_g_file_attribute_matcher_matches_id (attribute_matcher,
					    G_FILE_ATTRIBUTE_ID_ACCESS_CAN_WRITE))
    _g_file_info_set_attribute_boolean_by_id (info, G_FILE_ATTRIBUTE_ID_ACCESS_CAN_WRITE,
				             g_access (path, W_OK) == 0);
  
  if (_g_file_attribute_matcher_matches_id (attribute_matcher,
					    G_FILE_ATTRIBUTE_ID_ACCESS_CAN_EXECUTE))
    _g_file_info_set_attribute_boolean_by_id (info, G_FILE_ATTRIBUTE_ID_ACCESS_CAN_EXECUTE,
				             g_access (path, X_OK) == 0);


  if (parent_info)
    {
      gboolean writable;

      writable = FALSE;
      if (parent_info->writable)
	{
#ifdef G_OS_WIN32
	  writable = TRUE;
#else
	  if (parent_info->is_sticky)
	    {
	      uid_t uid = geteuid ();

	      if (uid == _g_stat_uid (statbuf) ||
		  uid == (uid_t) parent_info->owner ||
		  uid == 0)
		writable = TRUE;
	    }
	  else
	    writable = TRUE;
#endif
	}

      if (_g_file_attribute_matcher_matches_id (attribute_matcher, G_FILE_ATTRIBUTE_ID_ACCESS_CAN_RENAME))
	_g_file_info_set_attribute_boolean_by_id (info, G_FILE_ATTRIBUTE_ID_ACCESS_CAN_RENAME,
					         writable);
      
      if (_g_file_attribute_matcher_matches_id (attribute_matcher, G_FILE_ATTRIBUTE_ID_ACCESS_CAN_DELETE))
	_g_file_info_set_attribute_boolean_by_id (info, G_FILE_ATTRIBUTE_ID_ACCESS_CAN_DELETE,
					         writable);

      if (_g_file_attribute_matcher_matches_id (attribute_matcher, G_FILE_ATTRIBUTE_ID_ACCESS_CAN_TRASH))
        _g_file_info_set_attribute_boolean_by_id (info, G_FILE_ATTRIBUTE_ID_ACCESS_CAN_TRASH,
                                                 writable && parent_info->has_trash_dir);
    }
}

static void
set_info_from_stat (GFileInfo             *info, 
                    GLocalFileStat        *statbuf,
		    GFileAttributeMatcher *attribute_matcher)
{
  GFileType file_type;

  file_type = G_FILE_TYPE_UNKNOWN;

  if (S_ISREG (_g_stat_mode (statbuf)))
    file_type = G_FILE_TYPE_REGULAR;
  else if (S_ISDIR (_g_stat_mode (statbuf)))
    file_type = G_FILE_TYPE_DIRECTORY;
#ifndef G_OS_WIN32
  else if (S_ISCHR (_g_stat_mode (statbuf)) ||
	   S_ISBLK (_g_stat_mode (statbuf)) ||
	   S_ISFIFO (_g_stat_mode (statbuf))
#ifdef S_ISSOCK
	   || S_ISSOCK (_g_stat_mode (statbuf))
#endif
	   )
    file_type = G_FILE_TYPE_SPECIAL;
#endif
#ifdef S_ISLNK
  else if (S_ISLNK (_g_stat_mode (statbuf)))
    file_type = G_FILE_TYPE_SYMBOLIC_LINK;
#elif defined (G_OS_WIN32)
  else if (statbuf->reparse_tag == IO_REPARSE_TAG_SYMLINK ||
           statbuf->reparse_tag == IO_REPARSE_TAG_MOUNT_POINT)
    file_type = G_FILE_TYPE_SYMBOLIC_LINK;
#endif

  g_file_info_set_file_type (info, file_type);
  g_file_info_set_size (info, _g_stat_size (statbuf));

  _g_file_info_set_attribute_uint32_by_id (info, G_FILE_ATTRIBUTE_ID_UNIX_DEVICE, _g_stat_dev (statbuf));
  _g_file_info_set_attribute_uint32_by_id (info, G_FILE_ATTRIBUTE_ID_UNIX_NLINK, _g_stat_nlink (statbuf));
#ifndef G_OS_WIN32
  /* Pointless setting these on Windows even if they exist in the struct */
  _g_file_info_set_attribute_uint64_by_id (info, G_FILE_ATTRIBUTE_ID_UNIX_INODE, _g_stat_ino (statbuf));
  _g_file_info_set_attribute_uint32_by_id (info, G_FILE_ATTRIBUTE_ID_UNIX_UID, _g_stat_uid (statbuf));
  _g_file_info_set_attribute_uint32_by_id (info, G_FILE_ATTRIBUTE_ID_UNIX_GID, _g_stat_gid (statbuf));
  _g_file_info_set_attribute_uint32_by_id (info, G_FILE_ATTRIBUTE_ID_UNIX_RDEV, _g_stat_rdev (statbuf));
#endif
  /* Mostly pointless on Windows.
   * Still, it allows for S_ISREG/S_ISDIR and IWRITE (read-only) checks.
   */
  _g_file_info_set_attribute_uint32_by_id (info, G_FILE_ATTRIBUTE_ID_UNIX_MODE, _g_stat_mode (statbuf));
#if defined (HAVE_STRUCT_STAT_ST_BLKSIZE)
  _g_file_info_set_attribute_uint32_by_id (info, G_FILE_ATTRIBUTE_ID_UNIX_BLOCK_SIZE, _g_stat_blksize (statbuf));
#endif
#if defined (HAVE_STRUCT_STAT_ST_BLOCKS)
  _g_file_info_set_attribute_uint64_by_id (info, G_FILE_ATTRIBUTE_ID_UNIX_BLOCKS, _g_stat_blocks (statbuf));
  _g_file_info_set_attribute_uint64_by_id (info, G_FILE_ATTRIBUTE_ID_STANDARD_ALLOCATED_SIZE,
                                           _g_stat_blocks (statbuf) * G_GUINT64_CONSTANT (512));
#elif defined (G_OS_WIN32)
  _g_file_info_set_attribute_uint64_by_id (info, G_FILE_ATTRIBUTE_ID_STANDARD_ALLOCATED_SIZE,
                                           statbuf->allocated_size);

#endif

  _g_file_info_set_attribute_uint64_by_id (info, G_FILE_ATTRIBUTE_ID_TIME_MODIFIED, _g_stat_mtime (statbuf));
  _g_file_info_set_attribute_uint32_by_id (info, G_FILE_ATTRIBUTE_ID_TIME_MODIFIED_USEC, _g_stat_mtim_nsec (statbuf) / 1000);
  _g_file_info_set_attribute_uint32_by_id (info, G_FILE_ATTRIBUTE_ID_TIME_MODIFIED_NSEC, _g_stat_mtim_nsec (statbuf));

  if (_g_stat_has_field (statbuf, G_LOCAL_FILE_STAT_FIELD_ATIME))
    {
      _g_file_info_set_attribute_uint64_by_id (info, G_FILE_ATTRIBUTE_ID_TIME_ACCESS, _g_stat_atime (statbuf));
      _g_file_info_set_attribute_uint32_by_id (info, G_FILE_ATTRIBUTE_ID_TIME_ACCESS_USEC, _g_stat_atim_nsec (statbuf) / 1000);
      _g_file_info_set_attribute_uint32_by_id (info, G_FILE_ATTRIBUTE_ID_TIME_ACCESS_NSEC, _g_stat_atim_nsec (statbuf));
    }

#ifndef G_OS_WIN32
  /* Microsoft uses st_ctime for file creation time,
   * instead of file change time:
   * https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/stat-functions#generic-text-routine-mappings
   * Thank you, Microsoft!
   */
  _g_file_info_set_attribute_uint64_by_id (info, G_FILE_ATTRIBUTE_ID_TIME_CHANGED, _g_stat_ctime (statbuf));
  _g_file_info_set_attribute_uint32_by_id (info, G_FILE_ATTRIBUTE_ID_TIME_CHANGED_USEC, _g_stat_ctim_nsec (statbuf) / 1000);
  _g_file_info_set_attribute_uint32_by_id (info, G_FILE_ATTRIBUTE_ID_TIME_CHANGED_NSEC, _g_stat_ctim_nsec (statbuf));
#endif

#if defined (HAVE_STATX)
  if (_g_stat_has_field (statbuf, G_LOCAL_FILE_STAT_FIELD_BTIME))
    {
      _g_file_info_set_attribute_uint64_by_id (info, G_FILE_ATTRIBUTE_ID_TIME_CREATED, statbuf->stx_btime.tv_sec);
      _g_file_info_set_attribute_uint32_by_id (info, G_FILE_ATTRIBUTE_ID_TIME_CREATED_USEC, statbuf->stx_btime.tv_nsec / 1000);
      _g_file_info_set_attribute_uint32_by_id (info, G_FILE_ATTRIBUTE_ID_TIME_CREATED_NSEC, statbuf->stx_btime.tv_nsec);
    }
#elif defined (HAVE_STRUCT_STAT_ST_BIRTHTIME) && defined (HAVE_STRUCT_STAT_ST_BIRTHTIMENSEC)
  _g_file_info_set_attribute_uint64_by_id (info, G_FILE_ATTRIBUTE_ID_TIME_CREATED, statbuf->st_birthtime);
  _g_file_info_set_attribute_uint32_by_id (info, G_FILE_ATTRIBUTE_ID_TIME_CREATED_USEC, statbuf->st_birthtimensec / 1000);
  _g_file_info_set_attribute_uint32_by_id (info, G_FILE_ATTRIBUTE_ID_TIME_CREATED_NSEC, statbuf->st_birthtimensec);
#elif defined (HAVE_STRUCT_STAT_ST_BIRTHTIM) && defined (HAVE_STRUCT_STAT_ST_BIRTHTIM_TV_NSEC)
  _g_file_info_set_attribute_uint64_by_id (info, G_FILE_ATTRIBUTE_ID_TIME_CREATED, statbuf->st_birthtim.tv_sec);
  _g_file_info_set_attribute_uint32_by_id (info, G_FILE_ATTRIBUTE_ID_TIME_CREATED_USEC, statbuf->st_birthtim.tv_nsec / 1000);
  _g_file_info_set_attribute_uint32_by_id (info, G_FILE_ATTRIBUTE_ID_TIME_CREATED_NSEC, statbuf->st_birthtim.tv_nsec);
#elif defined (HAVE_STRUCT_STAT_ST_BIRTHTIME)
  _g_file_info_set_attribute_uint64_by_id (info, G_FILE_ATTRIBUTE_ID_TIME_CREATED, statbuf->st_birthtime);
#elif defined (HAVE_STRUCT_STAT_ST_BIRTHTIM)
  _g_file_info_set_attribute_uint64_by_id (info, G_FILE_ATTRIBUTE_ID_TIME_CREATED, statbuf->st_birthtim);
#elif defined (G_OS_WIN32)
  _g_file_info_set_attribute_uint64_by_id (info, G_FILE_ATTRIBUTE_ID_TIME_CREATED, statbuf->st_ctim.tv_sec);
  _g_file_info_set_attribute_uint32_by_id (info, G_FILE_ATTRIBUTE_ID_TIME_CREATED_USEC, statbuf->st_ctim.tv_nsec / 1000);
  _g_file_info_set_attribute_uint32_by_id (info, G_FILE_ATTRIBUTE_ID_TIME_CREATED_NSEC, statbuf->st_ctim.tv_nsec);
#endif

  if (_g_file_attribute_matcher_matches_id (attribute_matcher,
					    G_FILE_ATTRIBUTE_ID_ETAG_VALUE))
    {
      char *etag = _g_local_file_info_create_etag (statbuf);
      _g_file_info_set_attribute_string_by_id (info, G_FILE_ATTRIBUTE_ID_ETAG_VALUE, etag);
      g_free (etag);
    }

  if (_g_file_attribute_matcher_matches_id (attribute_matcher,
					    G_FILE_ATTRIBUTE_ID_ID_FILE))
    {
      char *id = _g_local_file_info_create_file_id (statbuf);
      _g_file_info_set_attribute_string_by_id (info, G_FILE_ATTRIBUTE_ID_ID_FILE, id);
      g_free (id);
    }

  if (_g_file_attribute_matcher_matches_id (attribute_matcher,
					    G_FILE_ATTRIBUTE_ID_ID_FILESYSTEM))
    {
      char *id = _g_local_file_info_create_fs_id (statbuf);
      _g_file_info_set_attribute_string_by_id (info, G_FILE_ATTRIBUTE_ID_ID_FILESYSTEM, id);
      g_free (id);
    }
}

#ifndef G_OS_WIN32

static char *
make_valid_utf8 (const char *name)
{
  GString *string;
  const gchar *remainder, *invalid;
  gsize remaining_bytes, valid_bytes;
  
  string = NULL;
  remainder = name;
  remaining_bytes = strlen (name);
  
  while (remaining_bytes != 0) 
    {
      if (g_utf8_validate_len (remainder, remaining_bytes, &invalid))
	break;
      valid_bytes = invalid - remainder;
    
      if (string == NULL) 
	string = g_string_sized_new (remaining_bytes);

      g_string_append_len (string, remainder, valid_bytes);
      /* append U+FFFD REPLACEMENT CHARACTER */
      g_string_append (string, "\357\277\275");
      
      remaining_bytes -= valid_bytes + 1;
      remainder = invalid + 1;
    }
  
  if (string == NULL)
    return g_strdup (name);
  
  g_string_append (string, remainder);

  g_warn_if_fail (g_utf8_validate (string->str, -1, NULL));
  
  return g_string_free (string, FALSE);
}

static char *
convert_pwd_string_to_utf8 (char *pwd_str)
{
  char *utf8_string;
  
  if (!g_utf8_validate (pwd_str, -1, NULL))
    {
      utf8_string = g_locale_to_utf8 (pwd_str, -1, NULL, NULL, NULL);
      if (utf8_string == NULL)
	utf8_string = make_valid_utf8 (pwd_str);
    }
  else 
    utf8_string = g_strdup (pwd_str);
  
  return utf8_string;
}

static void
uid_data_free (UidData *data)
{
  g_free (data->user_name);
  g_free (data->real_name);
  g_free (data);
}

/* called with lock held */
static UidData *
lookup_uid_data (uid_t uid)
{
  UidData *data;
  char buffer[4096];
  struct passwd pwbuf;
  struct passwd *pwbufp;
#ifndef __BIONIC__
  char *gecos, *comma;
#endif

  if (uid_cache == NULL)
    uid_cache = g_hash_table_new_full (NULL, NULL, NULL, (GDestroyNotify)uid_data_free);

  data = g_hash_table_lookup (uid_cache, GINT_TO_POINTER (uid));

  if (data)
    return data;

  data = g_new0 (UidData, 1);

#if defined(HAVE_GETPWUID_R)
  getpwuid_r (uid, &pwbuf, buffer, sizeof(buffer), &pwbufp);
#else
  pwbufp = getpwuid (uid);
#endif

  if (pwbufp != NULL)
    {
      if (pwbufp->pw_name != NULL && pwbufp->pw_name[0] != 0)
	data->user_name = convert_pwd_string_to_utf8 (pwbufp->pw_name);

#ifndef __BIONIC__
      gecos = pwbufp->pw_gecos;

      if (gecos)
	{
	  comma = strchr (gecos, ',');
	  if (comma)
	    *comma = 0;
	  data->real_name = convert_pwd_string_to_utf8 (gecos);
	}
#endif
    }

  /* Default fallbacks */
  if (data->real_name == NULL)
    {
      if (data->user_name != NULL)
	data->real_name = g_strdup (data->user_name);
      else
	data->real_name = g_strdup_printf ("user #%d", (int)uid);
    }
  
  if (data->user_name == NULL)
    data->user_name = g_strdup_printf ("%d", (int)uid);
  
  g_hash_table_replace (uid_cache, GINT_TO_POINTER (uid), data);
  
  return data;
}

static char *
get_username_from_uid (uid_t uid)
{
  char *res;
  UidData *data;
  
  G_LOCK (uid_cache);
  data = lookup_uid_data (uid);
  res = g_strdup (data->user_name);  
  G_UNLOCK (uid_cache);

  return res;
}

static char *
get_realname_from_uid (uid_t uid)
{
  char *res;
  UidData *data;
  
  G_LOCK (uid_cache);
  data = lookup_uid_data (uid);
  res = g_strdup (data->real_name);  
  G_UNLOCK (uid_cache);
  
  return res;
}

/* called with lock held */
static char *
lookup_gid_name (gid_t gid)
{
  char *name;
#if defined (HAVE_GETGRGID_R)
  char buffer[4096];
  struct group gbuf;
#endif
  struct group *gbufp;

  if (gid_cache == NULL)
    gid_cache = g_hash_table_new_full (NULL, NULL, NULL, (GDestroyNotify)g_free);

  name = g_hash_table_lookup (gid_cache, GINT_TO_POINTER (gid));

  if (name)
    return name;

#if defined (HAVE_GETGRGID_R)
  getgrgid_r (gid, &gbuf, buffer, sizeof(buffer), &gbufp);
#else
  gbufp = getgrgid (gid);
#endif

  if (gbufp != NULL &&
      gbufp->gr_name != NULL &&
      gbufp->gr_name[0] != 0)
    name = convert_pwd_string_to_utf8 (gbufp->gr_name);
  else
    name = g_strdup_printf("%d", (int)gid);
  
  g_hash_table_replace (gid_cache, GINT_TO_POINTER (gid), name);
  
  return name;
}

static char *
get_groupname_from_gid (gid_t gid)
{
  char *res;
  char *name;
  
  G_LOCK (gid_cache);
  name = lookup_gid_name (gid);
  res = g_strdup (name);  
  G_UNLOCK (gid_cache);
  return res;
}

#endif /* !G_OS_WIN32 */

static char *
get_content_type (const char          *basename,
		  const char          *path,
		  GLocalFileStat      *statbuf,
		  gboolean             is_symlink,
		  gboolean             symlink_broken,
		  GFileQueryInfoFlags  flags,
		  gboolean             fast)
{
  if (is_symlink &&
      (symlink_broken || (flags & G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS)))
    return g_content_type_from_mime_type ("inode/symlink");
  else if (statbuf != NULL && S_ISDIR(_g_stat_mode (statbuf)))
    return g_content_type_from_mime_type ("inode/directory");
#ifndef G_OS_WIN32
  else if (statbuf != NULL && S_ISCHR(_g_stat_mode (statbuf)))
    return g_content_type_from_mime_type ("inode/chardevice");
  else if (statbuf != NULL && S_ISBLK(_g_stat_mode (statbuf)))
    return g_content_type_from_mime_type ("inode/blockdevice");
  else if (statbuf != NULL && S_ISFIFO(_g_stat_mode (statbuf)))
    return g_content_type_from_mime_type ("inode/fifo");
  else if (statbuf != NULL && S_ISREG(_g_stat_mode (statbuf)) && _g_stat_size (statbuf) == 0)
    {
      /* Don't sniff zero-length files in order to avoid reading files
       * that appear normal but are not (eg: files in /proc and /sys)
       */
      return g_content_type_from_mime_type ("application/x-zerosize");
    }
#endif
#ifdef S_ISSOCK
  else if (statbuf != NULL && S_ISSOCK(_g_stat_mode (statbuf)))
    return g_content_type_from_mime_type ("inode/socket");
#endif
  else
    {
      char *content_type;
      gboolean result_uncertain;

      content_type = g_content_type_guess (basename, NULL, 0, &result_uncertain);
      
#if !defined(G_OS_WIN32) && !defined(__APPLE__)
      if (!fast && result_uncertain && path != NULL)
	{
          /* Sniff the first 16KiB of the file (sometimes less, if xdgmime
           * says it doesn’t need so much). Most files need less than 4KiB of
           * sniffing, but some disk images need more (see
           * https://gitlab.gnome.org/GNOME/glib/-/issues/3186). */
	  guchar sniff_buffer[16384];
	  gsize sniff_length;
#ifdef O_NOATIME
	  int errsv;
#endif
	  int fd;

	  sniff_length = _g_unix_content_type_get_sniff_len ();
	  if (sniff_length == 0 || sniff_length > sizeof (sniff_buffer))
	    sniff_length = sizeof (sniff_buffer);

#ifdef O_NOATIME	  
          fd = g_open (path, O_RDONLY | O_NOATIME | O_CLOEXEC, 0);
          errsv = errno;
          if (fd < 0 && errsv == EPERM)
#endif
	    fd = g_open (path, O_RDONLY | O_CLOEXEC, 0);

	  if (fd != -1)
	    {
	      gssize res;
	      
	      res = read (fd, sniff_buffer, sniff_length);
	      (void) g_close (fd, NULL);
	      if (res >= 0)
		{
		  g_free (content_type);
		  content_type = g_content_type_guess (basename, sniff_buffer, res, NULL);
		}
	    }
	}
#endif
      
      return content_type;
    }
  
}

typedef enum {
  THUMBNAIL_SIZE_AUTO,
  THUMBNAIL_SIZE_NORMAL,
  THUMBNAIL_SIZE_LARGE,
  THUMBNAIL_SIZE_XLARGE,
  THUMBNAIL_SIZE_XXLARGE,
  THUMBNAIL_SIZE_LAST,
} ThumbnailSize;

static const char *
get_thumbnail_dirname_from_size (ThumbnailSize size)
{
  switch (size)
    {
    case THUMBNAIL_SIZE_AUTO:
      return NULL;
      break;
    case THUMBNAIL_SIZE_NORMAL:
      return "normal";
      break;
    case THUMBNAIL_SIZE_LARGE:
      return "large";
      break;
    case THUMBNAIL_SIZE_XLARGE:
      return "x-large";
      break;
    case THUMBNAIL_SIZE_XXLARGE:
      return "xx-large";
      break;
    default:
      g_assert_not_reached ();
    }

  g_return_val_if_reached (NULL);
}

/* @stat_buf is the pre-calculated result of stat(path), or %NULL if that failed. */
static void
get_thumbnail_attributes (const char     *path,
                          GFileInfo      *info,
                          const GLocalFileStat *stat_buf,
                          ThumbnailSize   size)
{
  GChecksum *checksum;
  const char *dirname;
  char *uri;
  char *filename = NULL;
  char *basename;
  guint32 failed_attr_id;
  guint32 is_valid_attr_id;
  guint32 path_attr_id;

  switch (size)
    {
    case THUMBNAIL_SIZE_AUTO:
      failed_attr_id = G_FILE_ATTRIBUTE_ID_THUMBNAILING_FAILED;
      is_valid_attr_id = G_FILE_ATTRIBUTE_ID_THUMBNAIL_IS_VALID;
      path_attr_id = G_FILE_ATTRIBUTE_ID_THUMBNAIL_PATH;
      break;
    case THUMBNAIL_SIZE_NORMAL:
      failed_attr_id = G_FILE_ATTRIBUTE_ID_THUMBNAILING_FAILED_NORMAL;
      is_valid_attr_id = G_FILE_ATTRIBUTE_ID_THUMBNAIL_IS_VALID_NORMAL;
      path_attr_id = G_FILE_ATTRIBUTE_ID_THUMBNAIL_PATH_NORMAL;
      break;
    case THUMBNAIL_SIZE_LARGE:
      failed_attr_id = G_FILE_ATTRIBUTE_ID_THUMBNAILING_FAILED_LARGE;
      is_valid_attr_id = G_FILE_ATTRIBUTE_ID_THUMBNAIL_IS_VALID_LARGE;
      path_attr_id = G_FILE_ATTRIBUTE_ID_THUMBNAIL_PATH_LARGE;
      break;
    case THUMBNAIL_SIZE_XLARGE:
      failed_attr_id = G_FILE_ATTRIBUTE_ID_THUMBNAILING_FAILED_XLARGE;
      is_valid_attr_id = G_FILE_ATTRIBUTE_ID_THUMBNAIL_IS_VALID_XLARGE;
      path_attr_id = G_FILE_ATTRIBUTE_ID_THUMBNAIL_PATH_XLARGE;
      break;
    case THUMBNAIL_SIZE_XXLARGE:
      failed_attr_id = G_FILE_ATTRIBUTE_ID_THUMBNAILING_FAILED_XXLARGE;
      is_valid_attr_id = G_FILE_ATTRIBUTE_ID_THUMBNAIL_IS_VALID_XXLARGE;
      path_attr_id = G_FILE_ATTRIBUTE_ID_THUMBNAIL_PATH_XXLARGE;
      break;
    default:
      g_assert_not_reached ();
    }

  dirname = get_thumbnail_dirname_from_size (size);
  uri = g_filename_to_uri (path, NULL, NULL);

  checksum = g_checksum_new (G_CHECKSUM_MD5);
  g_checksum_update (checksum, (const guchar *) uri, strlen (uri));

  basename = g_strconcat (g_checksum_get_string (checksum), ".png", NULL);
  g_checksum_free (checksum);

  if (dirname)
    {
      filename = g_build_filename (g_get_user_cache_dir (),
                                   "thumbnails", dirname, basename,
                                   NULL);

      if (!g_file_test (filename, G_FILE_TEST_IS_REGULAR))
        g_clear_pointer (&filename, g_free);
    }
  else
    {
      gssize i;

      for (i = THUMBNAIL_SIZE_LAST - 1; i >= 0 ; i--)
        {
          filename = g_build_filename (g_get_user_cache_dir (),
                                       "thumbnails",
                                       get_thumbnail_dirname_from_size (i),
                                       basename,
                                      NULL);
          if (g_file_test (filename, G_FILE_TEST_IS_REGULAR))
            break;

          g_clear_pointer (&filename, g_free);
        }
    }

  if (filename)
    {
      _g_file_info_set_attribute_byte_string_by_id (info, path_attr_id, filename);
      _g_file_info_set_attribute_boolean_by_id (info, is_valid_attr_id,
                                                thumbnail_verify (filename, uri, stat_buf));
    }
  else
    {
      filename = g_build_filename (g_get_user_cache_dir (),
                                   "thumbnails", "fail",
                                   "gnome-thumbnail-factory",
                                   basename,
                                   NULL);

      if (g_file_test (filename, G_FILE_TEST_IS_REGULAR))
        {
          _g_file_info_set_attribute_boolean_by_id (info, failed_attr_id, TRUE);
          _g_file_info_set_attribute_boolean_by_id (info, is_valid_attr_id,
                                                    thumbnail_verify (filename, uri, stat_buf));
        }
    }

  g_free (basename);
  g_free (filename);
  g_free (uri);
}

#ifdef G_OS_WIN32
static void
win32_get_file_user_info (const gchar  *filename,
			  gchar       **group_name, 
			  gchar       **user_name, 
			  gchar       **real_name)
{
  PSECURITY_DESCRIPTOR psd = NULL;
  DWORD sd_size = 0; /* first call calculates the size required */
  
  wchar_t *wfilename = g_utf8_to_utf16 (filename, -1, NULL, NULL, NULL);
  if ((GetFileSecurityW (wfilename, 
                        GROUP_SECURITY_INFORMATION | OWNER_SECURITY_INFORMATION,
			NULL,
			sd_size,
			&sd_size) || (ERROR_INSUFFICIENT_BUFFER == GetLastError())) &&
     (psd = g_try_malloc (sd_size)) != NULL &&
     GetFileSecurityW (wfilename, 
                       GROUP_SECURITY_INFORMATION | OWNER_SECURITY_INFORMATION,
		       psd,
		       sd_size,
		       &sd_size))
    {
      PSID psid = 0;
      BOOL defaulted;
      SID_NAME_USE name_use = 0; /* don't care? */
      wchar_t *name = NULL;
      wchar_t *domain = NULL;
      DWORD name_len = 0;
      DWORD domain_len = 0;
      /* get the user name */
      do {
        if (!user_name)
	  break;
	if (!GetSecurityDescriptorOwner (psd, &psid, &defaulted))
	  break;
	if (!LookupAccountSidW (NULL, /* local machine */
                                psid, 
			        name, &name_len,
			        domain, &domain_len, /* no domain info yet */
			        &name_use)  && (ERROR_INSUFFICIENT_BUFFER != GetLastError()))
	  break;
	name = g_try_malloc (name_len * sizeof (wchar_t));
	domain = g_try_malloc (domain_len * sizeof (wchar_t));
	if (name && domain &&
            LookupAccountSidW (NULL, /* local machine */
                               psid, 
			       name, &name_len,
			       domain, &domain_len, /* no domain info yet */
			       &name_use))
	  {
	    *user_name = g_utf16_to_utf8 (name, -1, NULL, NULL, NULL);
	  }
	g_free (name);
	g_free (domain);
      } while (FALSE);

      /* get the group name */
      do {
        if (!group_name)
	  break;
	if (!GetSecurityDescriptorGroup (psd, &psid, &defaulted))
	  break;
	if (!LookupAccountSidW (NULL, /* local machine */
                                psid, 
			        name, &name_len,
			        domain, &domain_len, /* no domain info yet */
			        &name_use)  && (ERROR_INSUFFICIENT_BUFFER != GetLastError()))
	  break;
	name = g_try_malloc (name_len * sizeof (wchar_t));
	domain = g_try_malloc (domain_len * sizeof (wchar_t));
	if (name && domain &&
            LookupAccountSidW (NULL, /* local machine */
                               psid, 
			       name, &name_len,
			       domain, &domain_len, /* no domain info yet */
			       &name_use))
	  {
	    *group_name = g_utf16_to_utf8 (name, -1, NULL, NULL, NULL);
	  }
	g_free (name);
	g_free (domain);
      } while (FALSE);

      /* TODO: get real name */

      g_free (psd);
    }
  g_free (wfilename);
}
#endif /* G_OS_WIN32 */

#ifndef G_OS_WIN32
/* support for '.hidden' files */
G_LOCK_DEFINE_STATIC (hidden_cache);
static GHashTable *hidden_cache;
static GSource *hidden_cache_source = NULL; /* Under the hidden_cache lock */
static guint hidden_cache_ttl_secs = 5;
static guint hidden_cache_ttl_jitter_secs = 2;

typedef struct
{
  GHashTable *hidden_files;
  gint64 timestamp_secs;
} HiddenCacheData;

static gboolean
remove_from_hidden_cache (gpointer user_data)
{
  HiddenCacheData *data;
  GHashTableIter iter;
  gboolean retval;
  gint64 timestamp_secs;

  G_LOCK (hidden_cache);
  timestamp_secs = g_source_get_time (hidden_cache_source) / G_USEC_PER_SEC;

  g_hash_table_iter_init (&iter, hidden_cache);
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &data))
    {
      if (timestamp_secs > data->timestamp_secs + hidden_cache_ttl_secs)
        g_hash_table_iter_remove (&iter);
    }

  if (g_hash_table_size (hidden_cache) == 0)
    {
      g_clear_pointer (&hidden_cache_source, g_source_unref);
      retval = G_SOURCE_REMOVE;
    }
  else
    retval = G_SOURCE_CONTINUE;

  G_UNLOCK (hidden_cache);

  return retval;
}

static GHashTable *
read_hidden_file (const gchar *dirname)
{
  gchar *contents = NULL;
  gchar *filename;

  filename = g_build_path ("/", dirname, ".hidden", NULL);
  (void) g_file_get_contents (filename, &contents, NULL, NULL);
  g_free (filename);

  if (contents != NULL)
    {
      GHashTable *table;
      gchar **lines;
      gint i;

      table = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

      lines = g_strsplit (contents, "\n", 0);
      g_free (contents);

      for (i = 0; lines[i]; i++)
        /* hash table takes the individual strings... */
        g_hash_table_add (table, lines[i]);

      /* ... so we only free the container. */
      g_free (lines);

      return table;
    }
  else
    return NULL;
}

static void
free_hidden_file_data (gpointer user_data)
{
  HiddenCacheData *data = user_data;

  g_clear_pointer (&data->hidden_files, g_hash_table_unref);
  g_free (data);
}

static gboolean
file_is_hidden (const gchar *path,
                const gchar *basename)
{
  HiddenCacheData *data;
  gboolean result;
  gchar *dirname;
  gpointer table;

  dirname = g_path_get_dirname (path);

  G_LOCK (hidden_cache);

  if G_UNLIKELY (hidden_cache == NULL)
    hidden_cache = g_hash_table_new_full (g_str_hash, g_str_equal,
                                          g_free, free_hidden_file_data);

  if (!g_hash_table_lookup_extended (hidden_cache, dirname,
                                     NULL, (gpointer *) &data))
    {
      data = g_new0 (HiddenCacheData, 1);
      data->hidden_files = table = read_hidden_file (dirname);
      data->timestamp_secs = g_get_monotonic_time () / G_USEC_PER_SEC;

      g_hash_table_insert (hidden_cache,
                           g_strdup (dirname),
                           data);

      if (!hidden_cache_source)
        {
          hidden_cache_source =
            g_timeout_source_new_seconds (hidden_cache_ttl_secs +
                                          hidden_cache_ttl_jitter_secs);
          g_source_set_priority (hidden_cache_source, G_PRIORITY_DEFAULT);
          g_source_set_static_name (hidden_cache_source,
                                    "[gio] remove_from_hidden_cache");
          g_source_set_callback (hidden_cache_source,
                                 remove_from_hidden_cache,
                                 NULL, NULL);
          g_source_attach (hidden_cache_source,
                           GLIB_PRIVATE_CALL (g_get_worker_context) ());
        }
    }
  else
    table = data->hidden_files;

  result = table != NULL && g_hash_table_contains (table, basename);

  G_UNLOCK (hidden_cache);

  g_free (dirname);

  return result;
}
#endif /* !G_OS_WIN32 */

void
_g_local_file_info_get_nostat (GFileInfo              *info,
                               const char             *basename,
			       const char             *path,
                               GFileAttributeMatcher  *attribute_matcher)
{
  g_file_info_set_name (info, basename);

  if (_g_file_attribute_matcher_matches_id (attribute_matcher,
					    G_FILE_ATTRIBUTE_ID_STANDARD_DISPLAY_NAME))
    {
      char *display_name = g_filename_display_basename (path);
     
      /* look for U+FFFD REPLACEMENT CHARACTER */ 
      if (strstr (display_name, "\357\277\275") != NULL)
	{
	  char *p = display_name;
	  display_name = g_strconcat (display_name, _(" (invalid encoding)"), NULL);
	  g_free (p);
	}
      g_file_info_set_display_name (info, display_name);
      g_free (display_name);
    }
  
  if (_g_file_attribute_matcher_matches_id (attribute_matcher,
					    G_FILE_ATTRIBUTE_ID_STANDARD_EDIT_NAME))
    {
      char *edit_name = g_filename_display_basename (path);
      g_file_info_set_edit_name (info, edit_name);
      g_free (edit_name);
    }

  
  if (_g_file_attribute_matcher_matches_id (attribute_matcher,
					    G_FILE_ATTRIBUTE_ID_STANDARD_COPY_NAME))
    {
      char *copy_name = g_filename_to_utf8 (basename, -1, NULL, NULL, NULL);
      if (copy_name)
	_g_file_info_set_attribute_string_by_id (info, G_FILE_ATTRIBUTE_ID_STANDARD_COPY_NAME, copy_name);
      g_free (copy_name);
    }
}

static const char *
get_icon_name (const char *path,
               gboolean    use_symbolic,
               gboolean   *with_fallbacks_out)
{
  const char *name = NULL;
  gboolean with_fallbacks = TRUE;

  if (g_strcmp0 (path, g_get_home_dir ()) == 0)
    {
      name = use_symbolic ? "user-home-symbolic" : "user-home";
      with_fallbacks = FALSE;
    }
  else if (g_strcmp0 (path, g_get_user_special_dir (G_USER_DIRECTORY_DESKTOP)) == 0)
    {
      name = use_symbolic ? "user-desktop-symbolic" : "user-desktop";
      with_fallbacks = FALSE;
    }
  else if (g_strcmp0 (path, g_get_user_special_dir (G_USER_DIRECTORY_DOCUMENTS)) == 0)
    {
      name = use_symbolic ? "folder-documents-symbolic" : "folder-documents";
    }
  else if (g_strcmp0 (path, g_get_user_special_dir (G_USER_DIRECTORY_DOWNLOAD)) == 0)
    {
      name = use_symbolic ? "folder-download-symbolic" : "folder-download";
    }
  else if (g_strcmp0 (path, g_get_user_special_dir (G_USER_DIRECTORY_MUSIC)) == 0)
    {
      name = use_symbolic ? "folder-music-symbolic" : "folder-music";
    }
  else if (g_strcmp0 (path, g_get_user_special_dir (G_USER_DIRECTORY_PICTURES)) == 0)
    {
      name = use_symbolic ? "folder-pictures-symbolic" : "folder-pictures";
    }
  else if (g_strcmp0 (path, g_get_user_special_dir (G_USER_DIRECTORY_PUBLIC_SHARE)) == 0)
    {
      name = use_symbolic ? "folder-publicshare-symbolic" : "folder-publicshare";
    }
  else if (g_strcmp0 (path, g_get_user_special_dir (G_USER_DIRECTORY_TEMPLATES)) == 0)
    {
      name = use_symbolic ? "folder-templates-symbolic" : "folder-templates";
    }
  else if (g_strcmp0 (path, g_get_user_special_dir (G_USER_DIRECTORY_VIDEOS)) == 0)
    {
      name = use_symbolic ? "folder-videos-symbolic" : "folder-videos";
    }
  else
    {
      name = NULL;
    }

  if (with_fallbacks_out != NULL)
    *with_fallbacks_out = with_fallbacks;

  return name;
}

static GIcon *
get_icon (const char *path,
          const char *content_type,
          gboolean    use_symbolic)
{
  GIcon *icon = NULL;
  const char *icon_name;
  gboolean with_fallbacks;

  icon_name = get_icon_name (path, use_symbolic, &with_fallbacks);
  if (icon_name != NULL)
    {
      if (with_fallbacks)
        icon = g_themed_icon_new_with_default_fallbacks (icon_name);
      else
        icon = g_themed_icon_new (icon_name);
    }
  else
    {
      if (use_symbolic)
        icon = g_content_type_get_symbolic_icon (content_type);
      else
        icon = g_content_type_get_icon (content_type);
    }

  return icon;
}

GFileInfo *
_g_local_file_info_get (const char             *basename,
			const char             *path,
			GFileAttributeMatcher  *attribute_matcher,
			GFileQueryInfoFlags     flags,
			GLocalParentFileInfo   *parent_info,
			GError                **error)
{
  GFileInfo *info;
  GLocalFileStat statbuf;
  GLocalFileStat statbuf2;
  int res;
  gboolean stat_ok;
  gboolean is_symlink, symlink_broken;
  char *symlink_target;
  GVfs *vfs;
  GVfsClass *class;
  guint64 device;
#ifdef G_OS_WIN32
  DWORD attributes;
  gboolean attributes_set;
#endif

  info = g_file_info_new ();

  /* Make sure we don't set any unwanted attributes */
  g_file_info_set_attribute_mask (info, attribute_matcher);
  
  _g_local_file_info_get_nostat (info, basename, path, attribute_matcher);

  if (attribute_matcher == NULL)
    {
      g_file_info_unset_attribute_mask (info);
      return info;
    }

  res = g_local_file_lstat (path,
                            G_LOCAL_FILE_STAT_FIELD_BASIC_STATS | G_LOCAL_FILE_STAT_FIELD_BTIME,
                            G_LOCAL_FILE_STAT_FIELD_ALL & (~G_LOCAL_FILE_STAT_FIELD_BTIME) & (~G_LOCAL_FILE_STAT_FIELD_ATIME),
                            &statbuf);

  if (res == -1)
    {
      int errsv = errno;

      /* Don't bail out if we get Permission denied (SELinux?) */
      if (errsv != EACCES)
        {
          char *display_name = g_filename_display_name (path);
          g_object_unref (info);
          g_set_error (error, G_IO_ERROR,
		       g_io_error_from_errno (errsv),
		       _("Error when getting information for file “%s”: %s"),
		       display_name, g_strerror (errsv));
          g_free (display_name);
          return NULL;
        }
    }

  /* Even if stat() fails, try to get as much as other attributes possible */
  stat_ok = res != -1;

  if (stat_ok)
    device = _g_stat_dev (&statbuf);
  else
    device = 0;

#ifdef S_ISLNK
  is_symlink = stat_ok && S_ISLNK (_g_stat_mode (&statbuf));
#elif defined (G_OS_WIN32)
  /* glib already checked the FILE_ATTRIBUTE_REPARSE_POINT for us */
  is_symlink = stat_ok &&
      (statbuf.reparse_tag == IO_REPARSE_TAG_SYMLINK ||
       statbuf.reparse_tag == IO_REPARSE_TAG_MOUNT_POINT);
#else
  is_symlink = FALSE;
#endif
  symlink_broken = FALSE;

  if (is_symlink)
    {
      g_file_info_set_is_symlink (info, TRUE);

      /* Unless NOFOLLOW was set we default to following symlinks */
      if (!(flags & G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS))
	{
          res = g_local_file_stat (path,
                                   G_LOCAL_FILE_STAT_FIELD_BASIC_STATS | G_LOCAL_FILE_STAT_FIELD_BTIME,
                                   G_LOCAL_FILE_STAT_FIELD_ALL & (~G_LOCAL_FILE_STAT_FIELD_BTIME) & (~G_LOCAL_FILE_STAT_FIELD_ATIME),
                                   &statbuf2);

	  /* Report broken links as symlinks */
	  if (res != -1)
	    {
	      statbuf = statbuf2;
	      stat_ok = TRUE;
	    }
	  else
	    symlink_broken = TRUE;
	}
    }
  else
    g_file_info_set_is_symlink (info, FALSE);

  if (stat_ok)
    set_info_from_stat (info, &statbuf, attribute_matcher);

#ifndef G_OS_WIN32
  if (_g_file_attribute_matcher_matches_id (attribute_matcher,
					    G_FILE_ATTRIBUTE_ID_STANDARD_IS_HIDDEN))
    {
      g_file_info_set_is_hidden (info,
                                 (basename != NULL &&
                                  (basename[0] == '.' ||
                                   file_is_hidden (path, basename) ||
                                   (stat_ok &&
                                    _g_local_file_is_lost_found_dir (path, _g_stat_dev (&statbuf))))));
    }

  _g_file_info_set_attribute_boolean_by_id (info,
                                            G_FILE_ATTRIBUTE_ID_STANDARD_IS_BACKUP,
                                            basename != NULL && basename[strlen (basename) - 1] == '~' &&
                                                (stat_ok && S_ISREG (_g_stat_mode (&statbuf))));
#else
  _g_file_info_set_attribute_boolean_by_id (info, G_FILE_ATTRIBUTE_ID_STANDARD_IS_BACKUP, FALSE);

  attributes_set = FALSE;

  if (stat_ok)
    {
      attributes = statbuf.attributes;
      attributes_set = TRUE;
    }
  else
    {
      wchar_t *path_utf16 = g_utf8_to_utf16 (path, -1, NULL, NULL, NULL);

      if (path_utf16)
        {
          attributes = GetFileAttributes (path_utf16);
          attributes_set = (attributes != INVALID_FILE_ATTRIBUTES);
        }

      g_free (path_utf16);
    }

  if (attributes_set)
    {
      g_file_info_set_is_hidden (info, (attributes & FILE_ATTRIBUTE_HIDDEN));

      _g_file_info_set_attribute_boolean_by_id (info, G_FILE_ATTRIBUTE_ID_DOS_IS_ARCHIVE,
                                                (attributes & FILE_ATTRIBUTE_ARCHIVE));

      _g_file_info_set_attribute_boolean_by_id (info, G_FILE_ATTRIBUTE_ID_DOS_IS_SYSTEM,
                                                (attributes & FILE_ATTRIBUTE_SYSTEM));
    }

  _g_file_info_set_attribute_boolean_by_id (info, G_FILE_ATTRIBUTE_ID_DOS_IS_MOUNTPOINT,
                                            (statbuf.reparse_tag == IO_REPARSE_TAG_MOUNT_POINT));

  if (statbuf.reparse_tag != 0)
    _g_file_info_set_attribute_uint32_by_id (info, G_FILE_ATTRIBUTE_ID_DOS_REPARSE_POINT_TAG, statbuf.reparse_tag);
#endif

  symlink_target = NULL;
  if (is_symlink)
    {
#if defined (S_ISLNK) || defined (G_OS_WIN32)
      symlink_target = read_link (path);
#endif
      if (symlink_target &&
          _g_file_attribute_matcher_matches_id (attribute_matcher,
                                                G_FILE_ATTRIBUTE_ID_STANDARD_SYMLINK_TARGET))
        g_file_info_set_symlink_target (info, symlink_target);
    }

  if (_g_file_attribute_matcher_matches_id (attribute_matcher,
					    G_FILE_ATTRIBUTE_ID_STANDARD_CONTENT_TYPE) ||
      _g_file_attribute_matcher_matches_id (attribute_matcher,
					    G_FILE_ATTRIBUTE_ID_STANDARD_ICON) ||
      _g_file_attribute_matcher_matches_id (attribute_matcher,
					    G_FILE_ATTRIBUTE_ID_STANDARD_SYMBOLIC_ICON))
    {
      char *content_type = get_content_type (basename, path, stat_ok ? &statbuf : NULL, is_symlink, symlink_broken, flags, FALSE);

      if (content_type)
	{
	  g_file_info_set_content_type (info, content_type);

	  if (_g_file_attribute_matcher_matches_id (attribute_matcher,
                                                     G_FILE_ATTRIBUTE_ID_STANDARD_ICON)
               || _g_file_attribute_matcher_matches_id (attribute_matcher,
                                                        G_FILE_ATTRIBUTE_ID_STANDARD_SYMBOLIC_ICON))
	    {
	      GIcon *icon;

              /* non symbolic icon */
              icon = get_icon (path, content_type, FALSE);
              if (icon != NULL)
                {
                  g_file_info_set_icon (info, icon);
                  g_object_unref (icon);
                }

              /* symbolic icon */
              icon = get_icon (path, content_type, TRUE);
              if (icon != NULL)
                {
                  g_file_info_set_symbolic_icon (info, icon);
                  g_object_unref (icon);
                }

	    }
	  
	  g_free (content_type);
	}
    }

  if (_g_file_attribute_matcher_matches_id (attribute_matcher,
					    G_FILE_ATTRIBUTE_ID_STANDARD_FAST_CONTENT_TYPE))
    {
      char *content_type = get_content_type (basename, path, stat_ok ? &statbuf : NULL, is_symlink, symlink_broken, flags, TRUE);
      
      if (content_type)
	{
	  _g_file_info_set_attribute_string_by_id (info, G_FILE_ATTRIBUTE_ID_STANDARD_FAST_CONTENT_TYPE, content_type);
	  g_free (content_type);
	}
    }

  if (_g_file_attribute_matcher_matches_id (attribute_matcher,
					    G_FILE_ATTRIBUTE_ID_OWNER_USER))
    {
      char *name = NULL;
      
#ifdef G_OS_WIN32
      win32_get_file_user_info (path, NULL, &name, NULL);
#else
      if (stat_ok)
        name = get_username_from_uid (_g_stat_uid (&statbuf));
#endif
      if (name)
	_g_file_info_set_attribute_string_by_id (info, G_FILE_ATTRIBUTE_ID_OWNER_USER, name);
      g_free (name);
    }

  if (_g_file_attribute_matcher_matches_id (attribute_matcher,
					    G_FILE_ATTRIBUTE_ID_OWNER_USER_REAL))
    {
      char *name = NULL;
#ifdef G_OS_WIN32
      win32_get_file_user_info (path, NULL, NULL, &name);
#else
      if (stat_ok)
        name = get_realname_from_uid (_g_stat_uid (&statbuf));
#endif
      if (name)
	_g_file_info_set_attribute_string_by_id (info, G_FILE_ATTRIBUTE_ID_OWNER_USER_REAL, name);
      g_free (name);
    }
  
  if (_g_file_attribute_matcher_matches_id (attribute_matcher,
					    G_FILE_ATTRIBUTE_ID_OWNER_GROUP))
    {
      char *name = NULL;
#ifdef G_OS_WIN32
      win32_get_file_user_info (path, &name, NULL, NULL);
#else
      if (stat_ok)
        name = get_groupname_from_gid (_g_stat_gid (&statbuf));
#endif
      if (name)
	_g_file_info_set_attribute_string_by_id (info, G_FILE_ATTRIBUTE_ID_OWNER_GROUP, name);
      g_free (name);
    }

  if (stat_ok && parent_info && parent_info->device != 0 &&
      _g_file_attribute_matcher_matches_id (attribute_matcher, G_FILE_ATTRIBUTE_ID_UNIX_IS_MOUNTPOINT))
    _g_file_info_set_attribute_boolean_by_id (info, G_FILE_ATTRIBUTE_ID_UNIX_IS_MOUNTPOINT,
                                              (_g_stat_dev (&statbuf) != parent_info->device || _g_stat_ino (&statbuf) == parent_info->inode));
  
  if (stat_ok)
    get_access_rights (attribute_matcher, info, path, &statbuf, parent_info);
  
#ifdef HAVE_SELINUX
  get_selinux_context (path, info, attribute_matcher, (flags & G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS) == 0);
#endif
  get_xattrs (path, TRUE, info, attribute_matcher, (flags & G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS) == 0);
  get_xattrs (path, FALSE, info, attribute_matcher, (flags & G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS) == 0);

  if (_g_file_attribute_matcher_matches_id (attribute_matcher,
                                            G_FILE_ATTRIBUTE_ID_THUMBNAIL_PATH) ||
      _g_file_attribute_matcher_matches_id (attribute_matcher,
                                            G_FILE_ATTRIBUTE_ID_THUMBNAIL_IS_VALID) ||
      _g_file_attribute_matcher_matches_id (attribute_matcher,
                                            G_FILE_ATTRIBUTE_ID_THUMBNAILING_FAILED))
    {
      get_thumbnail_attributes (path, info, stat_ok ? &statbuf : NULL, THUMBNAIL_SIZE_AUTO);
    }

  if (_g_file_attribute_matcher_matches_id (attribute_matcher,
                                            G_FILE_ATTRIBUTE_ID_THUMBNAIL_PATH_NORMAL) ||
      _g_file_attribute_matcher_matches_id (attribute_matcher,
                                            G_FILE_ATTRIBUTE_ID_THUMBNAIL_IS_VALID_NORMAL) ||
      _g_file_attribute_matcher_matches_id (attribute_matcher,
                                            G_FILE_ATTRIBUTE_ID_THUMBNAILING_FAILED_NORMAL))
    {
      get_thumbnail_attributes (path, info, stat_ok ? &statbuf : NULL, THUMBNAIL_SIZE_NORMAL);
    }

  if (_g_file_attribute_matcher_matches_id (attribute_matcher,
                                            G_FILE_ATTRIBUTE_ID_THUMBNAIL_PATH_LARGE) ||
      _g_file_attribute_matcher_matches_id (attribute_matcher,
                                            G_FILE_ATTRIBUTE_ID_THUMBNAIL_IS_VALID_LARGE) ||
      _g_file_attribute_matcher_matches_id (attribute_matcher,
                                            G_FILE_ATTRIBUTE_ID_THUMBNAILING_FAILED_LARGE))
    {
      get_thumbnail_attributes (path, info, stat_ok ? &statbuf : NULL, THUMBNAIL_SIZE_LARGE);
    }

  if (_g_file_attribute_matcher_matches_id (attribute_matcher,
                                            G_FILE_ATTRIBUTE_ID_THUMBNAIL_PATH_XLARGE) ||
      _g_file_attribute_matcher_matches_id (attribute_matcher,
                                            G_FILE_ATTRIBUTE_ID_THUMBNAIL_IS_VALID_XLARGE) ||
      _g_file_attribute_matcher_matches_id (attribute_matcher,
                                            G_FILE_ATTRIBUTE_ID_THUMBNAILING_FAILED_XLARGE))
    {
      get_thumbnail_attributes (path, info, stat_ok ? &statbuf : NULL, THUMBNAIL_SIZE_XLARGE);
    }

  if (_g_file_attribute_matcher_matches_id (attribute_matcher,
                                            G_FILE_ATTRIBUTE_ID_THUMBNAIL_PATH_XXLARGE) ||
      _g_file_attribute_matcher_matches_id (attribute_matcher,
                                            G_FILE_ATTRIBUTE_ID_THUMBNAIL_IS_VALID_XXLARGE) ||
      _g_file_attribute_matcher_matches_id (attribute_matcher,
                                            G_FILE_ATTRIBUTE_ID_THUMBNAILING_FAILED_XXLARGE))
    {
      get_thumbnail_attributes (path, info, stat_ok ? &statbuf : NULL, THUMBNAIL_SIZE_XXLARGE);
    }

  vfs = g_vfs_get_default ();
  class = G_VFS_GET_CLASS (vfs);
  if (class->local_file_add_info)
    {
      class->local_file_add_info (vfs,
                                  path,
                                  device,
                                  attribute_matcher,
                                  info,
                                  NULL,
                                  &parent_info->extra_data,
                                  &parent_info->free_extra_data);
    }

  g_file_info_unset_attribute_mask (info);

  g_free (symlink_target);

  return info;
}

GFileInfo *
_g_local_file_info_get_from_fd (int         fd,
				const char *attributes,
				GError    **error)
{
  GLocalFileStat stat_buf;
  GFileAttributeMatcher *matcher;
  GFileInfo *info;

  if (g_local_file_fstat (fd,
                          G_LOCAL_FILE_STAT_FIELD_BASIC_STATS | G_LOCAL_FILE_STAT_FIELD_BTIME,
                          G_LOCAL_FILE_STAT_FIELD_ALL & (~G_LOCAL_FILE_STAT_FIELD_BTIME) & (~G_LOCAL_FILE_STAT_FIELD_ATIME),
                          &stat_buf) == -1)
    {
      int errsv = errno;

      g_set_error (error, G_IO_ERROR,
		   g_io_error_from_errno (errsv),
		   _("Error when getting information for file descriptor: %s"),
		   g_strerror (errsv));
      return NULL;
    }

  info = g_file_info_new ();

  matcher = g_file_attribute_matcher_new (attributes);

  /* Make sure we don't set any unwanted attributes */
  g_file_info_set_attribute_mask (info, matcher);
  
  set_info_from_stat (info, &stat_buf, matcher);
  
#ifdef HAVE_SELINUX
  if (_g_file_attribute_matcher_matches_id (matcher, G_FILE_ATTRIBUTE_ID_SELINUX_CONTEXT) &&
      is_selinux_enabled ())
    {
      char *context;
      if (fgetfilecon_raw (fd, &context) >= 0)
	{
	  _g_file_info_set_attribute_string_by_id (info, G_FILE_ATTRIBUTE_ID_SELINUX_CONTEXT, context);
	  freecon (context);
	}
    }
#endif

  get_xattrs_from_fd (fd, TRUE, info, matcher);
  get_xattrs_from_fd (fd, FALSE, info, matcher);
  
  g_file_attribute_matcher_unref (matcher);

  g_file_info_unset_attribute_mask (info);
  
  return info;
}

static gboolean
get_uint32 (const GFileAttributeValue  *value,
	    guint32                    *val_out,
	    GError                    **error)
{
  if (value->type != G_FILE_ATTRIBUTE_TYPE_UINT32)
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                           _("Invalid attribute type (uint32 expected)"));
      return FALSE;
    }

  *val_out = value->u.uint32;
  
  return TRUE;
}

#if defined (HAVE_UTIMES) || defined (HAVE_UTIMENSAT) || defined (G_OS_WIN32)
static gboolean
get_uint64 (const GFileAttributeValue  *value,
	    guint64                    *val_out,
	    GError                    **error)
{
  if (value->type != G_FILE_ATTRIBUTE_TYPE_UINT64)
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                           _("Invalid attribute type (uint64 expected)"));
      return FALSE;
    }

  *val_out = value->u.uint64;
  
  return TRUE;
}
#endif

#if defined(HAVE_SYMLINK)
static gboolean
get_byte_string (const GFileAttributeValue  *value,
		 const char                **val_out,
		 GError                    **error)
{
  if (value->type != G_FILE_ATTRIBUTE_TYPE_BYTE_STRING)
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                           _("Invalid attribute type (byte string expected)"));
      return FALSE;
    }

  *val_out = value->u.string;
  
  return TRUE;
}
#endif

#ifdef HAVE_SELINUX
static gboolean
get_string (const GFileAttributeValue  *value,
	    const char                **val_out,
	    GError                    **error)
{
  if (value->type != G_FILE_ATTRIBUTE_TYPE_STRING)
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                           _("Invalid attribute type (byte string expected)"));
      return FALSE;
    }

  *val_out = value->u.string;
  
  return TRUE;
}
#endif

static gboolean
set_unix_mode (char                       *filename,
               GFileQueryInfoFlags         flags,
	       const GFileAttributeValue  *value,
	       GError                    **error)
{
  guint32 val = 0;
  int res = 0;
  
  if (!get_uint32 (value, &val, error))
    return FALSE;

#if defined (HAVE_SYMLINK) || defined (G_OS_WIN32)
  if (flags & G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS) {
#ifdef HAVE_LCHMOD
    res = lchmod (filename, val);
#else
    gboolean is_symlink;
#ifndef G_OS_WIN32
    struct stat statbuf;
    /* Calling chmod on a symlink changes permissions on the symlink.
     * We don't want to do this, so we need to check for a symlink */
    res = g_lstat (filename, &statbuf);
    is_symlink = (res == 0 && S_ISLNK (statbuf.st_mode));
#else
    /* FIXME: implement lchmod for W32, should be doable */
    GWin32PrivateStat statbuf;

    res = GLIB_PRIVATE_CALL (g_win32_lstat_utf8) (filename, &statbuf);
    is_symlink = (res == 0 &&
                  (statbuf.reparse_tag == IO_REPARSE_TAG_SYMLINK ||
                   statbuf.reparse_tag == IO_REPARSE_TAG_MOUNT_POINT));
#endif
    if (is_symlink)
      {
        g_set_error_literal (error, G_IO_ERROR,
                             G_IO_ERROR_NOT_SUPPORTED,
                             _("Cannot set permissions on symlinks"));
        return FALSE;
      }
    else if (res == 0)
      res = g_chmod (filename, val);
#endif
  } else
#endif
    res = g_chmod (filename, val);

  if (res == -1)
    {
      int errsv = errno;

      g_set_error (error, G_IO_ERROR,
		   g_io_error_from_errno (errsv),
		   _("Error setting permissions: %s"),
		   g_strerror (errsv));
      return FALSE;
    }
  return TRUE;
}

#ifdef G_OS_UNIX
static gboolean
set_unix_uid_gid (char                       *filename,
		  const GFileAttributeValue  *uid_value,
		  const GFileAttributeValue  *gid_value,
		  GFileQueryInfoFlags         flags,
		  GError                    **error)
{
  int res;
  guint32 val = 0;
  uid_t uid;
  gid_t gid;
  
  if (uid_value)
    {
      if (!get_uint32 (uid_value, &val, error))
	return FALSE;
      uid = val;
    }
  else
    uid = -1;
  
  if (gid_value)
    {
      if (!get_uint32 (gid_value, &val, error))
	return FALSE;
      gid = val;
    }
  else
    gid = -1;
  
#ifdef HAVE_LCHOWN
  if (flags & G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS)
    res = lchown (filename, uid, gid);
  else
#endif
    res = chown (filename, uid, gid);
  
  if (res == -1)
    {
      int errsv = errno;

      g_set_error (error, G_IO_ERROR,
		   g_io_error_from_errno (errsv),
		   _("Error setting owner: %s"),
		   g_strerror (errsv));
	  return FALSE;
    }
  return TRUE;
}
#endif

#ifdef HAVE_SYMLINK
static gboolean
set_symlink (char                       *filename,
	     const GFileAttributeValue  *value,
	     GError                    **error)
{
  const char *val;
  struct stat statbuf;
  
  if (!get_byte_string (value, &val, error))
    return FALSE;
  
  if (val == NULL)
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                           _("symlink must be non-NULL"));
      return FALSE;
    }
  
  if (g_lstat (filename, &statbuf))
    {
      int errsv = errno;

      g_set_error (error, G_IO_ERROR,
		   g_io_error_from_errno (errsv),
		   _("Error setting symlink: %s"),
		   g_strerror (errsv));
      return FALSE;
    }
  
  if (!S_ISLNK (statbuf.st_mode))
    {
      g_set_error_literal (error, G_IO_ERROR,
                           G_IO_ERROR_NOT_SYMBOLIC_LINK,
                           _("Error setting symlink: file is not a symlink"));
      return FALSE;
    }
  
  if (g_unlink (filename))
    {
      int errsv = errno;

      g_set_error (error, G_IO_ERROR,
		   g_io_error_from_errno (errsv),
		   _("Error setting symlink: %s"),
		   g_strerror (errsv));
      return FALSE;
    }
  
  if (symlink (filename, val) != 0)
    {
      int errsv = errno;

      g_set_error (error, G_IO_ERROR,
		   g_io_error_from_errno (errsv),
		   _("Error setting symlink: %s"),
		   g_strerror (errsv));
      return FALSE;
    }
  
  return TRUE;
}
#endif

#if defined (HAVE_UTIMES) || defined (HAVE_UTIMENSAT) || defined(G_OS_WIN32)
static int
lazy_stat (const char  *filename,
           GStatBuf    *statbuf,
           gboolean    *called_stat)
{
  int res;

  if (*called_stat)
    return 0;

  res = g_stat (filename, statbuf);

  if (res == 0)
    *called_stat = TRUE;

  return res;
}
#endif

#if defined (G_OS_WIN32)
/* From
 * https://support.microsoft.com/en-ca/help/167296/how-to-convert-a-unix-time-t-to-a-win32-filetime-or-systemtime
 * FT = UT * 10000000 + 116444736000000000.
 * Converts unix epoch time (a signed 64-bit integer) to FILETIME.
 * Can optionally use a more precise timestamp that has
 * a fraction of a second expressed in nanoseconds.
 * UT must be between January 1st of year 1601 and December 31st of year 30827.
 * nsec must be non-negative and < 1000000000.
 * Returns TRUE if conversion succeeded, FALSE otherwise.
 *
 * The function that does the reverse can be found in
 * glib/gstdio.c.
 */
static gboolean
_g_win32_unix_time_to_filetime (gint64     ut,
                                gint32     nsec,
                                FILETIME  *ft,
                                GError   **error)
{
  gint64 result;
  /* 1 unit of FILETIME is 100ns */
  const gint64 hundreds_of_nsec_per_sec = 10000000;
  /* The difference between January 1, 1601 UTC (FILETIME epoch) and UNIX epoch
   * in hundreds of nanoseconds.
   */
  const gint64 filetime_unix_epoch_offset = 116444736000000000;
  /* This is the maximum timestamp that SYSTEMTIME can
   * represent (last millisecond of the year 30827).
   * Since FILETIME and SYSTEMTIME are both used on Windows,
   * we use this as a limit (FILETIME can support slightly
   * larger interval, up to year 30828).
   */
  const gint64 max_systemtime = 0x7fff35f4f06c58f0;

  g_return_val_if_fail (ft != NULL, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  if (nsec < 0)
    {
      g_set_error (error, G_IO_ERROR,
                   G_IO_ERROR_INVALID_DATA,
                   _("Extra nanoseconds %d for UNIX timestamp %lld are negative"),
                   nsec, ut);
      return FALSE;
    }

  if (nsec >= hundreds_of_nsec_per_sec * 100)
    {
      g_set_error (error, G_IO_ERROR,
                   G_IO_ERROR_INVALID_DATA,
                   _("Extra nanoseconds %d for UNIX timestamp %lld reach 1 second"),
                   nsec, ut);
      return FALSE;
    }

  if (ut >= (G_MAXINT64 / hundreds_of_nsec_per_sec) ||
      (ut * hundreds_of_nsec_per_sec) >= (G_MAXINT64 - filetime_unix_epoch_offset))
    {
      g_set_error (error, G_IO_ERROR,
                   G_IO_ERROR_INVALID_DATA,
                   _("UNIX timestamp %lld does not fit into 64 bits"),
                   ut);
      return FALSE;
    }

  result = ut * hundreds_of_nsec_per_sec + filetime_unix_epoch_offset + nsec / 100;

  if (result >= max_systemtime || result < 0)
    {
      g_set_error (error, G_IO_ERROR,
                   G_IO_ERROR_INVALID_DATA,
                   _("UNIX timestamp %lld is outside of the range supported by Windows"),
                   ut);
      return FALSE;
    }

  ft->dwLowDateTime = (DWORD) (result);
  ft->dwHighDateTime = (DWORD) (result >> 32);

  return TRUE;
}

static gboolean
set_mtime_atime (const char                 *filename,
		 const GFileAttributeValue  *mtime_value,
		 const GFileAttributeValue  *mtime_usec_value,
		 const GFileAttributeValue  *mtime_nsec_value,
		 const GFileAttributeValue  *atime_value,
		 const GFileAttributeValue  *atime_usec_value,
		 const GFileAttributeValue  *atime_nsec_value,
		 GError                    **error)
{
  BOOL res;
  guint64 val = 0;
  guint32 val_usec = 0;
  guint32 val_nsec = 0;
  gunichar2 *filename_utf16;
  SECURITY_ATTRIBUTES sec = { sizeof (SECURITY_ATTRIBUTES), NULL, FALSE };
  HANDLE file_handle;
  FILETIME mtime;
  FILETIME atime;
  FILETIME *p_mtime = NULL;
  FILETIME *p_atime = NULL;
  DWORD gle;
  GStatBuf statbuf;
  gboolean got_stat = FALSE;

  /* ATIME */
  if (atime_value)
    {
      if (!get_uint64 (atime_value, &val, error))
        return FALSE;
      val_usec = 0;
      val_nsec = 0;
    }
  else
    {
      if (lazy_stat (filename, &statbuf, &got_stat) == 0)
	{
          val = statbuf.st_atime;
#if defined (HAVE_STRUCT_STAT_ST_ATIMENSEC)
          val_nsec = statbuf.st_atimensec;
#elif defined (HAVE_STRUCT_STAT_ST_ATIM_TV_NSEC)
          val_nsec = statbuf.st_atim.tv_nsec;
#endif
	}
    }

  if (atime_usec_value &&
      !get_uint32 (atime_usec_value, &val_usec, error))
    return FALSE;

  /* Convert to nanoseconds. Clamp the usec value if it’s going to overflow,
   * as %G_MAXINT32 will trigger a ‘too big’ error in
   * _g_win32_unix_time_to_filetime() anyway. */
  val_nsec = (val_usec > G_MAXINT32 / 1000) ? G_MAXINT32 : (val_usec * 1000);

  if (atime_nsec_value &&
      !get_uint32 (atime_nsec_value, &val_nsec, error))
    return FALSE;
  if (val_nsec > 0)
    {
      if (!_g_win32_unix_time_to_filetime (val, val_nsec, &atime, error))
        return FALSE;
    }
  else
    {
      if (!_g_win32_unix_time_to_filetime (val, val_usec, &atime, error))
        return FALSE;
    }

  p_atime = &atime;

  /* MTIME */
  if (mtime_value)
    {
      if (!get_uint64 (mtime_value, &val, error))
	return FALSE;
      val_usec = 0;
      val_nsec = 0;
    }
  else
    {
      if (lazy_stat (filename, &statbuf, &got_stat) == 0)
	{
          val = statbuf.st_mtime;
#if defined (HAVE_STRUCT_STAT_ST_MTIMENSEC)
          val_nsec = statbuf.st_mtimensec;
#elif defined (HAVE_STRUCT_STAT_ST_MTIM_TV_NSEC)
          val_nsec = statbuf.st_mtim.tv_nsec;
#endif
	}
    }

  if (mtime_usec_value &&
      !get_uint32 (mtime_usec_value, &val_usec, error))
    return FALSE;

  /* Convert to nanoseconds. Clamp the usec value if it’s going to overflow,
   * as %G_MAXINT32 will trigger a ‘too big’ error in
   * _g_win32_unix_time_to_filetime() anyway. */
  val_nsec = (val_usec > G_MAXINT32 / 1000) ? G_MAXINT32 : (val_usec * 1000);

  if (mtime_nsec_value &&
      !get_uint32 (mtime_nsec_value, &val_nsec, error))
    return FALSE;
  if (val_nsec > 0)
    {
      if (!_g_win32_unix_time_to_filetime (val, val_nsec, &mtime, error))
        return FALSE;
    }
  else
    {
      if (!_g_win32_unix_time_to_filetime (val, val_usec, &mtime, error))
        return FALSE;
    }
  p_mtime = &mtime;

  filename_utf16 = g_utf8_to_utf16 (filename, -1, NULL, NULL, error);

  if (filename_utf16 == NULL)
    {
      g_prefix_error (error,
                      _("File name “%s” cannot be converted to UTF-16"),
                      filename);
      return FALSE;
    }

  file_handle = CreateFileW (filename_utf16,
                             FILE_WRITE_ATTRIBUTES,
                             FILE_SHARE_WRITE | FILE_SHARE_READ | FILE_SHARE_DELETE,
                             &sec,
                             OPEN_EXISTING,
                             FILE_FLAG_BACKUP_SEMANTICS,
                             NULL);
  gle = GetLastError ();
  g_clear_pointer (&filename_utf16, g_free);

  if (file_handle == INVALID_HANDLE_VALUE)
    {
      g_set_error (error, G_IO_ERROR,
                   g_io_error_from_errno (gle),
                   _("File “%s” cannot be opened: Windows Error %lu"),
                   filename, gle);

      return FALSE;
    }

  res = SetFileTime (file_handle, NULL, p_atime, p_mtime);
  gle = GetLastError ();
  CloseHandle (file_handle);

  if (!res)
    g_set_error (error, G_IO_ERROR,
                 g_io_error_from_errno (gle),
                 _("Error setting modification or access time for file “%s”: %lu"),
                 filename, gle);

  return res;
}
#elif defined (HAVE_UTIMES) || defined (HAVE_UTIMENSAT)
static gboolean
set_mtime_atime (char                       *filename,
		 const GFileAttributeValue  *mtime_value,
		 const GFileAttributeValue  *mtime_usec_value,
		 const GFileAttributeValue  *mtime_nsec_value,
		 const GFileAttributeValue  *atime_value,
		 const GFileAttributeValue  *atime_usec_value,
		 const GFileAttributeValue  *atime_nsec_value,
		 GError                    **error)
{
  int res;
  guint64 val = 0;
  GStatBuf statbuf;
  gboolean got_stat = FALSE;
#ifdef HAVE_UTIMENSAT
  struct timespec times_n[2] = { {0, 0}, {0, 0} };
  /* ATIME */
  if (atime_value)
    {
      if (!get_uint64 (atime_value, &val, error))
	return FALSE;
      times_n[0].tv_sec = val;
    }
  else
    {
      if (lazy_stat (filename, &statbuf, &got_stat) == 0)
	{
          times_n[0].tv_sec = statbuf.st_atime;
#if defined (HAVE_STRUCT_STAT_ST_ATIMENSEC)
          times_n[0].tv_nsec = statbuf.st_atimensec;
#elif defined (HAVE_STRUCT_STAT_ST_ATIM_TV_NSEC)
          times_n[0].tv_nsec = statbuf.st_atim.tv_nsec;
#endif
	}
    }

  if (atime_usec_value)
    {
      guint32 val_usec = 0;

      if (!get_uint32 (atime_usec_value, &val_usec, error))
        return FALSE;

      times_n[0].tv_nsec = val_usec * 1000;
    }

  if (atime_nsec_value)
    {
      guint32 val_nsec = 0;

      if (!get_uint32 (atime_nsec_value, &val_nsec, error))
        return FALSE;
      times_n[0].tv_nsec = val_nsec;
    }

  /* MTIME */
  if (mtime_value)
    {
      if (!get_uint64 (mtime_value, &val, error))
	return FALSE;
      times_n[1].tv_sec = val;
    }
  else
    {
      if (lazy_stat (filename, &statbuf, &got_stat) == 0)
	{
          times_n[1].tv_sec = statbuf.st_mtime;
#if defined (HAVE_STRUCT_STAT_ST_MTIMENSEC)
          times_n[1].tv_nsec = statbuf.st_mtimensec;
#elif defined (HAVE_STRUCT_STAT_ST_MTIM_TV_NSEC)
          times_n[1].tv_nsec = statbuf.st_mtim.tv_nsec;
#endif
	}
    }

  if (mtime_usec_value)
    {
      guint32 val_usec = 0;

      if (!get_uint32 (mtime_usec_value, &val_usec, error))
        return FALSE;

      times_n[1].tv_nsec = val_usec * 1000;
    }

  if (mtime_nsec_value)
    {
      guint32 val_nsec = 0;

      if (!get_uint32 (mtime_nsec_value, &val_nsec, error))
        return FALSE;
      times_n[1].tv_nsec = val_nsec;
    }

  res = utimensat (AT_FDCWD, filename, times_n, 0);

#else /* HAVE_UTIMES */

  struct timeval times[2] = { {0, 0}, {0, 0} };

  /* ATIME */
  if (atime_value)
    {
      if (!get_uint64 (atime_value, &val, error))
        return FALSE;

      times[0].tv_sec = val;
    }
  else
    {
      if (lazy_stat (filename, &statbuf, &got_stat) == 0)
        {
          times[0].tv_sec = statbuf.st_atime;
#if defined (HAVE_STRUCT_STAT_ST_ATIMENSEC)
          times[0].tv_usec = statbuf.st_atimensec / 1000;
#elif defined (HAVE_STRUCT_STAT_ST_ATIM_TV_NSEC)
          times[0].tv_usec = statbuf.st_atim.tv_nsec / 1000;
#endif
        }
    }

  if (atime_usec_value)
    {
      guint32 val_usec = 0;

      if (!get_uint32 (atime_usec_value, &val_usec, error))
        return FALSE;

      times[0].tv_usec = val_usec;
    }

  /* MTIME */
  if (mtime_value)
    {
      if (!get_uint64 (mtime_value, &val, error))
        return FALSE;

      times[1].tv_sec = val;
    }
  else
    {
      if (lazy_stat (filename, &statbuf, &got_stat) == 0)
        {
          times[1].tv_sec = statbuf.st_mtime;
#if defined (HAVE_STRUCT_STAT_ST_MTIMENSEC)
          times[1].tv_usec = statbuf.st_mtimensec / 1000;
#elif defined (HAVE_STRUCT_STAT_ST_MTIM_TV_NSEC)
          times[1].tv_usec = statbuf.st_mtim.tv_nsec / 1000;
#endif
        }
    }

  if (mtime_usec_value)
    {
      guint32 val_usec = 0;

      if (!get_uint32 (mtime_usec_value, &val_usec, error))
        return FALSE;

      times[1].tv_usec = val_usec;
    }

  res = utimes (filename, times);
#endif

  if (res == -1)
    {
      int errsv = errno;

      g_set_error (error, G_IO_ERROR,
                   g_io_error_from_errno (errsv),
                   _("Error setting modification or access time: %s"),
                   g_strerror (errsv));
      return FALSE;
    }
  return TRUE;
}
#endif


#ifdef HAVE_SELINUX
static gboolean
set_selinux_context (char                       *filename,
                     const GFileAttributeValue  *value,
                     GError                    **error)
{
  const char *val;

  if (!get_string (value, &val, error))
    return FALSE;

  if (val == NULL)
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                           _("SELinux context must be non-NULL"));
      return FALSE;
    }

  if (!is_selinux_enabled ())
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                           _("SELinux is not enabled on this system"));
      return FALSE;
    }

  if (setfilecon_raw (filename, val) < 0)
    {
      int errsv = errno;
            
      g_set_error (error, G_IO_ERROR,
                   g_io_error_from_errno (errsv),
                   _("Error setting SELinux context: %s"),
                   g_strerror (errsv));
      return FALSE;
    }

  return TRUE;
}
#endif 


gboolean
_g_local_file_info_set_attribute (char                 *filename,
				  const char           *attribute,
				  GFileAttributeType    type,
				  gpointer              value_p,
				  GFileQueryInfoFlags   flags,
				  GCancellable         *cancellable,
				  GError              **error)
{
  GFileAttributeValue value = { 0 };
  GVfsClass *class;
  GVfs *vfs;

  _g_file_attribute_value_set_from_pointer (&value, type, value_p, FALSE);
  
  if (strcmp (attribute, G_FILE_ATTRIBUTE_UNIX_MODE) == 0)
    return set_unix_mode (filename, flags, &value, error);
  
#ifdef G_OS_UNIX
  else if (strcmp (attribute, G_FILE_ATTRIBUTE_UNIX_UID) == 0)
    return set_unix_uid_gid (filename, &value, NULL, flags, error);
  else if (strcmp (attribute, G_FILE_ATTRIBUTE_UNIX_GID) == 0)
    return set_unix_uid_gid (filename, NULL, &value, flags, error);
#endif
  
#ifdef HAVE_SYMLINK
  else if (strcmp (attribute, G_FILE_ATTRIBUTE_STANDARD_SYMLINK_TARGET) == 0)
    return set_symlink (filename, &value, error);
#endif

#if defined (HAVE_UTIMES) || defined (HAVE_UTIMENSAT) || defined (G_OS_WIN32)
  else if (strcmp (attribute, G_FILE_ATTRIBUTE_TIME_MODIFIED) == 0)
    return set_mtime_atime (filename, &value, NULL, NULL, NULL, NULL, NULL, error);
  else if (strcmp (attribute, G_FILE_ATTRIBUTE_TIME_MODIFIED_USEC) == 0)
    return set_mtime_atime (filename, NULL, &value, NULL, NULL, NULL, NULL, error);
  else if (strcmp (attribute, G_FILE_ATTRIBUTE_TIME_MODIFIED_NSEC) == 0)
    return set_mtime_atime (filename, NULL, NULL, &value, NULL, NULL, NULL, error);
  else if (strcmp (attribute, G_FILE_ATTRIBUTE_TIME_ACCESS) == 0)
    return set_mtime_atime (filename, NULL, NULL, NULL, &value, NULL, NULL, error);
  else if (strcmp (attribute, G_FILE_ATTRIBUTE_TIME_ACCESS_USEC) == 0)
    return set_mtime_atime (filename, NULL, NULL, NULL, NULL, &value, NULL, error);
  else if (strcmp (attribute, G_FILE_ATTRIBUTE_TIME_ACCESS_NSEC) == 0)
    return set_mtime_atime (filename, NULL, NULL, NULL, NULL, NULL, &value, error);
#endif

#ifdef HAVE_XATTR
  else if (g_str_has_prefix (attribute, "xattr::"))
    return set_xattr (filename, attribute, &value, error);
  else if (g_str_has_prefix (attribute, "xattr-sys::"))
    return set_xattr (filename, attribute, &value, error);
#endif

#ifdef HAVE_SELINUX 
  else if (strcmp (attribute, G_FILE_ATTRIBUTE_SELINUX_CONTEXT) == 0)
    return set_selinux_context (filename, &value, error);
#endif

  vfs = g_vfs_get_default ();
  class = G_VFS_GET_CLASS (vfs);
  if (class->local_file_set_attributes)
    {
      GFileInfo *info;

      info = g_file_info_new ();
      g_file_info_set_attribute (info,
                                 attribute,
                                 type,
                                 value_p);
      if (!class->local_file_set_attributes (vfs, filename,
                                             info,
                                             flags, cancellable,
                                             error))
        {
          g_object_unref (info);
	  return FALSE;
        }

      if (g_file_info_get_attribute_status (info, attribute) == G_FILE_ATTRIBUTE_STATUS_SET)
        {
          g_object_unref (info);
          return TRUE;
        }

      g_object_unref (info);
    }

  g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
	       _("Setting attribute %s not supported"), attribute);
  return FALSE;
}

gboolean
_g_local_file_info_set_attributes  (char                 *filename,
				    GFileInfo            *info,
				    GFileQueryInfoFlags   flags,
				    GCancellable         *cancellable,
				    GError              **error)
{
  GFileAttributeValue *value;
#ifdef G_OS_UNIX
  GFileAttributeValue *uid, *gid;
#endif
#if defined (HAVE_UTIMES) || defined (HAVE_UTIMENSAT) || defined (G_OS_WIN32)
  GFileAttributeValue *mtime, *mtime_usec, *mtime_nsec, *atime, *atime_usec, *atime_nsec;
#endif
#if defined (G_OS_UNIX) || defined (G_OS_WIN32)
  GFileAttributeStatus status;
#endif
  gboolean res;
  GVfsClass *class;
  GVfs *vfs;
  
  /* Handles setting multiple specified data in a single set, and takes care
     of ordering restrictions when setting attributes */

  res = TRUE;

  /* Set symlink first, since this recreates the file */
#ifdef HAVE_SYMLINK
  value = _g_file_info_get_attribute_value (info, G_FILE_ATTRIBUTE_STANDARD_SYMLINK_TARGET);
  if (value)
    {
      if (!set_symlink (filename, value, error))
	{
	  value->status = G_FILE_ATTRIBUTE_STATUS_ERROR_SETTING;
	  res = FALSE;
	  /* Don't set error multiple times */
	  error = NULL;
	}
      else
	value->status = G_FILE_ATTRIBUTE_STATUS_SET;
	
    }
#endif

#ifdef G_OS_UNIX
  /* Group uid and gid setting into one call
   * Change ownership before permissions, since ownership changes can
     change permissions (e.g. setuid)
   */
  uid = _g_file_info_get_attribute_value (info, G_FILE_ATTRIBUTE_UNIX_UID);
  gid = _g_file_info_get_attribute_value (info, G_FILE_ATTRIBUTE_UNIX_GID);
  
  if (uid || gid)
    {
      if (!set_unix_uid_gid (filename, uid, gid, flags, error))
	{
	  status = G_FILE_ATTRIBUTE_STATUS_ERROR_SETTING;
	  res = FALSE;
	  /* Don't set error multiple times */
	  error = NULL;
	}
      else
	status = G_FILE_ATTRIBUTE_STATUS_SET;
      if (uid)
	uid->status = status;
      if (gid)
	gid->status = status;
    }
#endif
  
  value = _g_file_info_get_attribute_value (info, G_FILE_ATTRIBUTE_UNIX_MODE);
  if (value)
    {
      if (!set_unix_mode (filename, flags, value, error))
	{
	  value->status = G_FILE_ATTRIBUTE_STATUS_ERROR_SETTING;
	  res = FALSE;
	  /* Don't set error multiple times */
	  error = NULL;
	}
      else
	value->status = G_FILE_ATTRIBUTE_STATUS_SET;
	
    }

#if defined (HAVE_UTIMES) || defined (HAVE_UTIMENSAT) || defined (G_OS_WIN32)
  /* Group all time settings into one call
   * Change times as the last thing to avoid it changing due to metadata changes
   */
  
  mtime = _g_file_info_get_attribute_value (info, G_FILE_ATTRIBUTE_TIME_MODIFIED);
  mtime_usec = _g_file_info_get_attribute_value (info, G_FILE_ATTRIBUTE_TIME_MODIFIED_USEC);
  mtime_nsec = _g_file_info_get_attribute_value (info, G_FILE_ATTRIBUTE_TIME_MODIFIED_NSEC);
  atime = _g_file_info_get_attribute_value (info, G_FILE_ATTRIBUTE_TIME_ACCESS);
  atime_usec = _g_file_info_get_attribute_value (info, G_FILE_ATTRIBUTE_TIME_ACCESS_USEC);
  atime_nsec = _g_file_info_get_attribute_value (info, G_FILE_ATTRIBUTE_TIME_ACCESS_NSEC);

  if (mtime || mtime_usec || mtime_nsec || atime || atime_usec || atime_nsec)
    {
      if (!set_mtime_atime (filename, mtime, mtime_usec, mtime_nsec, atime, atime_usec, atime_nsec, error))
	{
	  status = G_FILE_ATTRIBUTE_STATUS_ERROR_SETTING;
	  res = FALSE;
	  /* Don't set error multiple times */
	  error = NULL;
	}
      else
	status = G_FILE_ATTRIBUTE_STATUS_SET;
      
      if (mtime)
	mtime->status = status;
      if (mtime_usec)
	mtime_usec->status = status;
      if (mtime_nsec)
	mtime_nsec->status = status;
      if (atime)
	atime->status = status;
      if (atime_usec)
	atime_usec->status = status;
      if (atime_nsec)
	atime_nsec->status = status;
    }
#endif

  /* xattrs are handled by default callback */


  /*  SELinux context */
#ifdef HAVE_SELINUX 
  if (is_selinux_enabled ()) {
    value = _g_file_info_get_attribute_value (info, G_FILE_ATTRIBUTE_SELINUX_CONTEXT);
    if (value)
    {
      if (!set_selinux_context (filename, value, error))
        {
          value->status = G_FILE_ATTRIBUTE_STATUS_ERROR_SETTING;
          res = FALSE;
          /* Don't set error multiple times */
          error = NULL;
        }
      else
        value->status = G_FILE_ATTRIBUTE_STATUS_SET;
    }
  }
#endif

  vfs = g_vfs_get_default ();
  class = G_VFS_GET_CLASS (vfs);
  if (class->local_file_set_attributes)
    {
      if (!class->local_file_set_attributes (vfs, filename,
                                             info,
                                             flags, cancellable,
                                             error))
        {
	  res = FALSE;
	  /* Don't set error multiple times */
	  error = NULL;
        }
    }

  return res;
}
