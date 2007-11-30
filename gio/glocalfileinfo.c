/* GIO - GLib Input, Output and Streaming Library
 * 
 * Copyright (C) 2006-2007 Red Hat, Inc.
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
 * Author: Alexander Larsson <alexl@redhat.com>
 */

#include <config.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#ifdef HAVE_GRP_H
#include <grp.h>
#endif
#ifdef HAVE_PWD_H
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
#include "glibintl.h"

#include "glocalfileinfo.h"
#include "gioerror.h"
#include "gthemedicon.h"
#include "gcontenttype.h"
#include "gcontenttypeprivate.h"

#include "gioalias.h"

struct ThumbMD5Context {
	guint32 buf[4];
	guint32 bits[2];
	unsigned char in[64];
};

typedef struct {
  char *user_name;
  char *real_name;
} UidData;

G_LOCK_DEFINE_STATIC (uid_cache);
static GHashTable *uid_cache = NULL;

G_LOCK_DEFINE_STATIC (gid_cache);
static GHashTable *gid_cache = NULL;

static void thumb_md5 (const char *string, unsigned char digest[16]);

char *
_g_local_file_info_create_etag (struct stat *statbuf)
{
  GTimeVal tv;
  
  tv.tv_sec = statbuf->st_mtime;
#if defined (HAVE_STRUCT_STAT_ST_MTIMENSEC)
  tv.tv_usec = statbuf->st_mtimensec / 1000;
#elif defined (HAVE_STRUCT_STAT_ST_MTIM_TV_NSEC)
  tv.tv_usec = statbuf->st_mtim.tv_nsec / 1000;
#else
  tv.tv_usec = 0;
#endif

  return g_strdup_printf ("%lu:%lu", tv.tv_sec, tv.tv_usec);
}

static char *
_g_local_file_info_create_file_id (struct stat *statbuf)
{
  return g_strdup_printf ("l%" G_GUINT64_FORMAT ":%" G_GUINT64_FORMAT,
			  (guint64) statbuf->st_dev, 
			  (guint64) statbuf->st_ino);
}

static char *
_g_local_file_info_create_fs_id (struct stat *statbuf)
{
  return g_strdup_printf ("l%" G_GUINT64_FORMAT,
			  (guint64) statbuf->st_dev);
}


static gchar *
read_link (const gchar *full_name)
{
#ifdef HAVE_READLINK
  gchar *buffer;
  guint size;
  
  size = 256;
  buffer = g_malloc (size);
  
  while (1)
    {
      int read_size;
      
      read_size = readlink (full_name, buffer, size);
      if (read_size < 0)
	{
	  g_free (buffer);
	  return NULL;
	}
      if (read_size < size)
	{
	  buffer[read_size] = 0;
	  return buffer;
	}
      size *= 2;
      buffer = g_realloc (buffer, size);
    }
#else
  return NULL;
#endif
}

/* Get the SELinux security context */
static void
get_selinux_context (const char            *path,
		     GFileInfo             *info,
		     GFileAttributeMatcher *attribute_matcher,
		     gboolean               follow_symlinks)
{
#ifdef HAVE_SELINUX
  char *context;

  if (!g_file_attribute_matcher_matches (attribute_matcher, "selinux:context"))
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
	  g_file_info_set_attribute_string (info, "selinux:context", context);
	  freecon(context);
	}
    }
#endif
}

#ifdef HAVE_XATTR

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
hex_escape_string (const char *str, 
                   gboolean   *free_return)
{
  int num_invalid, i;
  char *escaped_str, *p;
  unsigned char c;
  static char *hex_digits = "0123456789abcdef";
  int len;

  len = strlen (str);
  
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
  *p++ = 0;

  *free_return = TRUE;
  return escaped_str;
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
  *p++ = 0;

  if (out_len)
    *out_len = p - unescaped_str;
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
  
  escaped_val = hex_escape_string (value, &free_escaped_val);
  
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
  ssize_t len;

  if (follow_symlinks)  
    len = getxattr (path, xattr, value, sizeof (value)-1);
  else
    len = lgetxattr (path, xattr,value, sizeof (value)-1);

  value_p = NULL;
  if (len >= 0)
    value_p = value;
  else if (len == -1 && errno == ERANGE)
    {
      if (follow_symlinks)  
	len = getxattr (path, xattr, NULL, 0);
      else
	len = lgetxattr (path, xattr, NULL, 0);

      if (len < 0)
	return;

      value_p = g_malloc (len+1);

      if (follow_symlinks)  
	len = getxattr (path, xattr, value_p, len);
      else
	len = lgetxattr (path, xattr, value_p, len);

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
  ssize_t list_res_size;
  size_t len;
  char *list;
  const char *attr, *attr2;

  if (user)
    all = g_file_attribute_matcher_enumerate_namespace (matcher, "xattr");
  else
    all = g_file_attribute_matcher_enumerate_namespace (matcher, "xattr_sys");

  if (all)
    {
      if (follow_symlinks)
	list_res_size = listxattr (path, NULL, 0);
      else
	list_res_size = llistxattr (path, NULL, 0);

      if (list_res_size == -1 ||
	  list_res_size == 0)
	return;

      list_size = list_res_size;
      list = g_malloc (list_size);

    retry:
      
      if (follow_symlinks)
	list_res_size = listxattr (path, list, list_size);
      else
	list_res_size = llistxattr (path, list, list_size);
      
      if (list_res_size == -1 && errno == ERANGE)
	{
	  list_size = list_size * 2;
	  list = g_realloc (list, list_size);
	  goto retry;
	}

      if (list_res_size == -1)
	return;

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
		  gio_attr = g_strconcat ("xattr:", escaped_attr, NULL);
		}
	      else
		{
		  escaped_attr = hex_escape_string (attr, &free_escaped_attr);
		  gio_attr = g_strconcat ("xattr_sys:", escaped_attr, NULL);
		}
	      
	      if (free_escaped_attr)
		g_free (escaped_attr);
	      
	      get_one_xattr (path, info, gio_attr, attr, follow_symlinks);
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
  ssize_t len;

  len = fgetxattr (fd, xattr, value, sizeof (value)-1);

  value_p = NULL;
  if (len >= 0)
    value_p = value;
  else if (len == -1 && errno == ERANGE)
    {
      len = fgetxattr (fd, xattr, NULL, 0);

      if (len < 0)
	return;

      value_p = g_malloc (len+1);

      len = fgetxattr (fd, xattr, value_p, len);

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
  ssize_t list_res_size;
  size_t len;
  char *list;
  const char *attr, *attr2;

  if (user)
    all = g_file_attribute_matcher_enumerate_namespace (matcher, "xattr");
  else
    all = g_file_attribute_matcher_enumerate_namespace (matcher, "xattr_sys");

  if (all)
    {
      list_res_size = flistxattr (fd, NULL, 0);

      if (list_res_size == -1 ||
	  list_res_size == 0)
	return;

      list_size = list_res_size;
      list = g_malloc (list_size);

    retry:
      
      list_res_size = flistxattr (fd, list, list_size);
      
      if (list_res_size == -1 && errno == ERANGE)
	{
	  list_size = list_size * 2;
	  list = g_realloc (list, list_size);
	  goto retry;
	}

      if (list_res_size == -1)
	return;

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
		  gio_attr = g_strconcat ("xattr:", escaped_attr, NULL);
		}
	      else
		{
		  escaped_attr = hex_escape_string (attr, &free_escaped_attr);
		  gio_attr = g_strconcat ("xattr_sys:", escaped_attr, NULL);
		}
	      
	      if (free_escaped_attr)
		g_free (escaped_attr);
	      
	      get_one_xattr_from_fd (fd, info, gio_attr, attr);
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
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                   _("Attribute value must be non-NULL"));
      return FALSE;
    }

  if (attr_value->type != G_FILE_ATTRIBUTE_TYPE_STRING)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                   _("Invalid attribute type (string expected)"));
      return FALSE;
    }

  if (!name_is_valid (escaped_attribute))
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
		   _("Invalid extended attribute name"));
      return FALSE;
    }

  if (g_str_has_prefix (escaped_attribute, "xattr:"))
    {
      escaped_attribute += 6;
      is_user = TRUE;
    }
  else
    {
      g_assert (g_str_has_prefix (escaped_attribute, "xattr_sys:"));
      escaped_attribute += 10;
      is_user = FALSE;
    }
  
  attribute = hex_unescape_string (escaped_attribute, NULL, &free_attribute);
  value = hex_unescape_string (attr_value->u.string, &val_len, &free_value);

  if (is_user)
    a = g_strconcat ("user.", attribute, NULL);
  else
    a = attribute;
  
  res = setxattr (filename, a, value, val_len, 0);
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
		   _("Error setting extended attribute '%s': %s"),
		   escaped_attribute, g_strerror (errno));
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
  struct stat statbuf;
  int res;
  
  parent_info->writable = FALSE;
  parent_info->is_sticky = FALSE;
  parent_info->device = 0;

  if (g_file_attribute_matcher_matches (attribute_matcher, G_FILE_ATTRIBUTE_ACCESS_CAN_RENAME) ||
      g_file_attribute_matcher_matches (attribute_matcher, G_FILE_ATTRIBUTE_ACCESS_CAN_DELETE) ||
      g_file_attribute_matcher_matches (attribute_matcher, G_FILE_ATTRIBUTE_ACCESS_CAN_TRASH) ||
      g_file_attribute_matcher_matches (attribute_matcher, G_FILE_ATTRIBUTE_UNIX_IS_MOUNTPOINT))
    {
      parent_info->writable = (g_access (dir, W_OK) == 0);
      
      res = g_stat (dir, &statbuf);

      /*
       * The sticky bit (S_ISVTX) on a directory means that a file in that directory can be
       * renamed or deleted only by the owner of the file, by the owner of the directory, and
       * by a privileged process.
       */
      if (res == 0)
	{
	  parent_info->is_sticky = (statbuf.st_mode & S_ISVTX) != 0;
	  parent_info->owner = statbuf.st_uid;
	  parent_info->device = statbuf.st_dev;
	}
    }
}

static void
get_access_rights (GFileAttributeMatcher *attribute_matcher,
		   GFileInfo             *info,
		   const gchar           *path,
		   struct stat           *statbuf,
		   GLocalParentFileInfo  *parent_info)
{
  if (g_file_attribute_matcher_matches (attribute_matcher,
					G_FILE_ATTRIBUTE_ACCESS_CAN_READ))
    g_file_info_set_attribute_boolean (info, G_FILE_ATTRIBUTE_ACCESS_CAN_READ,
				       g_access (path, R_OK) == 0);
  
  if (g_file_attribute_matcher_matches (attribute_matcher,
					G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE))
    g_file_info_set_attribute_boolean (info, G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE,
				       g_access (path, W_OK) == 0);
  
  if (g_file_attribute_matcher_matches (attribute_matcher,
					G_FILE_ATTRIBUTE_ACCESS_CAN_EXECUTE))
    g_file_info_set_attribute_boolean (info, G_FILE_ATTRIBUTE_ACCESS_CAN_EXECUTE,
				       g_access (path, X_OK) == 0);


  if (parent_info)
    {
      gboolean writable;

      writable = FALSE;
      if (parent_info->writable)
	{
	  if (parent_info->is_sticky)
	    {
	      uid_t uid = geteuid ();

	      if (uid == statbuf->st_uid ||
		  uid == parent_info->owner ||
		  uid == 0)
		writable = TRUE;
	    }
	  else
	    writable = TRUE;
	}

      if (g_file_attribute_matcher_matches (attribute_matcher, G_FILE_ATTRIBUTE_ACCESS_CAN_RENAME))
	g_file_info_set_attribute_boolean (info, G_FILE_ATTRIBUTE_ACCESS_CAN_RENAME,
					   writable);
      
      if (g_file_attribute_matcher_matches (attribute_matcher, G_FILE_ATTRIBUTE_ACCESS_CAN_DELETE))
	g_file_info_set_attribute_boolean (info, G_FILE_ATTRIBUTE_ACCESS_CAN_DELETE,
					   writable);

      /* TODO: This means we can move it, but we should also look for a trash dir */
      if (g_file_attribute_matcher_matches (attribute_matcher, G_FILE_ATTRIBUTE_ACCESS_CAN_TRASH))
	g_file_info_set_attribute_boolean (info, G_FILE_ATTRIBUTE_ACCESS_CAN_TRASH,
					   writable);
    }
}

static void
set_info_from_stat (GFileInfo             *info, 
                    struct stat           *statbuf,
		    GFileAttributeMatcher *attribute_matcher)
{
  GFileType file_type;

  file_type = G_FILE_TYPE_UNKNOWN;

  if (S_ISREG (statbuf->st_mode))
    file_type = G_FILE_TYPE_REGULAR;
  else if (S_ISDIR (statbuf->st_mode))
    file_type = G_FILE_TYPE_DIRECTORY;
  else if (S_ISCHR (statbuf->st_mode) ||
	   S_ISBLK (statbuf->st_mode) ||
	   S_ISFIFO (statbuf->st_mode)
#ifdef S_ISSOCK
	   || S_ISSOCK (statbuf->st_mode)
#endif
	   )
    file_type = G_FILE_TYPE_SPECIAL;
#ifdef S_ISLNK
  else if (S_ISLNK (statbuf->st_mode))
    file_type = G_FILE_TYPE_SYMBOLIC_LINK;
#endif

  g_file_info_set_file_type (info, file_type);
  g_file_info_set_size (info, statbuf->st_size);

  g_file_info_set_attribute_uint32 (info, G_FILE_ATTRIBUTE_UNIX_DEVICE, statbuf->st_dev);
  g_file_info_set_attribute_uint64 (info, G_FILE_ATTRIBUTE_UNIX_INODE, statbuf->st_ino);
  g_file_info_set_attribute_uint32 (info, G_FILE_ATTRIBUTE_UNIX_MODE, statbuf->st_mode);
  g_file_info_set_attribute_uint32 (info, G_FILE_ATTRIBUTE_UNIX_NLINK, statbuf->st_nlink);
  g_file_info_set_attribute_uint32 (info, G_FILE_ATTRIBUTE_UNIX_UID, statbuf->st_uid);
  g_file_info_set_attribute_uint32 (info, G_FILE_ATTRIBUTE_UNIX_GID, statbuf->st_gid);
  g_file_info_set_attribute_uint32 (info, G_FILE_ATTRIBUTE_UNIX_RDEV, statbuf->st_rdev);
#if defined (HAVE_STRUCT_STAT_BLKSIZE)
  g_file_info_set_attribute_uint32 (info, G_FILE_ATTRIBUTE_UNIX_BLOCK_SIZE, statbuf->st_blksize);
#endif
#if defined (HAVE_STRUCT_STAT_BLOCKS)
  g_file_info_set_attribute_uint64 (info, G_FILE_ATTRIBUTE_UNIX_BLOCKS, statbuf->st_blocks);
#endif
  
  g_file_info_set_attribute_uint64 (info, G_FILE_ATTRIBUTE_TIME_MODIFIED, statbuf->st_mtime);
#if defined (HAVE_STRUCT_STAT_ST_MTIMENSEC)
  g_file_info_set_attribute_uint32 (info, G_FILE_ATTRIBUTE_TIME_MODIFIED_USEC, statbuf->st_mtimensec / 1000);
#elif defined (HAVE_STRUCT_STAT_ST_MTIM_TV_NSEC)
  g_file_info_set_attribute_uint32 (info, G_FILE_ATTRIBUTE_TIME_MODIFIED_USEC, statbuf->st_mtim.tv_nsec / 1000);
#endif
  
  g_file_info_set_attribute_uint64 (info, G_FILE_ATTRIBUTE_TIME_ACCESS, statbuf->st_atime);
#if defined (HAVE_STRUCT_STAT_ST_ATIMENSEC)
  g_file_info_set_attribute_uint32 (info, G_FILE_ATTRIBUTE_TIME_ACCESS_USEC, statbuf->st_atimensec / 1000);
#elif defined (HAVE_STRUCT_STAT_ST_ATIM_TV_NSEC)
  g_file_info_set_attribute_uint32 (info, G_FILE_ATTRIBUTE_TIME_ACCESS_USEC, statbuf->st_atim.tv_nsec / 1000);
#endif
  
  g_file_info_set_attribute_uint64 (info, G_FILE_ATTRIBUTE_TIME_CHANGED, statbuf->st_ctime);
#if defined (HAVE_STRUCT_STAT_ST_CTIMENSEC)
  g_file_info_set_attribute_uint32 (info, G_FILE_ATTRIBUTE_TIME_CHANGED_USEC, statbuf->st_ctimensec / 1000);
#elif defined (HAVE_STRUCT_STAT_ST_CTIM_TV_NSEC)
  g_file_info_set_attribute_uint32 (info, G_FILE_ATTRIBUTE_TIME_CHANGED_USEC, statbuf->st_ctim.tv_nsec / 1000);
#endif

  if (g_file_attribute_matcher_matches (attribute_matcher,
					G_FILE_ATTRIBUTE_ETAG_VALUE))
    {
      char *etag = _g_local_file_info_create_etag (statbuf);
      g_file_info_set_attribute_string (info, G_FILE_ATTRIBUTE_ETAG_VALUE, etag);
      g_free (etag);
    }

  if (g_file_attribute_matcher_matches (attribute_matcher,
					G_FILE_ATTRIBUTE_ID_FILE))
    {
      char *id = _g_local_file_info_create_file_id (statbuf);
      g_file_info_set_attribute_string (info, G_FILE_ATTRIBUTE_ID_FILE, id);
      g_free (id);
    }

  if (g_file_attribute_matcher_matches (attribute_matcher,
					G_FILE_ATTRIBUTE_ID_FS))
    {
      char *id = _g_local_file_info_create_fs_id (statbuf);
      g_file_info_set_attribute_string (info, G_FILE_ATTRIBUTE_ID_FS, id);
      g_free (id);
    }
}

static char *
make_valid_utf8 (const char *name)
{
  GString *string;
  const gchar *remainder, *invalid;
  gint remaining_bytes, valid_bytes;
  
  string = NULL;
  remainder = name;
  remaining_bytes = strlen (name);
  
  while (remaining_bytes != 0) 
    {
      if (g_utf8_validate (remainder, remaining_bytes, &invalid)) 
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

  g_assert (g_utf8_validate (string->str, -1, NULL));
  
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
  char *gecos, *comma;
  
  if (uid_cache == NULL)
    uid_cache = g_hash_table_new_full (NULL, NULL, NULL, (GDestroyNotify)uid_data_free);

  data = g_hash_table_lookup (uid_cache, GINT_TO_POINTER (uid));

  if (data)
    return data;

  data = g_new0 (UidData, 1);

#if defined(HAVE_POSIX_GETPWUID_R)
  getpwuid_r (uid, &pwbuf, buffer, sizeof(buffer), &pwbufp);
#elif defined(HAVE_NONPOSIX_GETPWUID_R)
  pwbufp = getpwuid_r (uid, &pwbuf, buffer, sizeof(buffer));
#else
  pwbufp = getpwuid (uid);
#endif

  if (pwbufp != NULL)
    {
      if (pwbufp->pw_name != NULL && pwbufp->pw_name[0] != 0)
	data->user_name = convert_pwd_string_to_utf8 (pwbufp->pw_name);

      gecos = pwbufp->pw_gecos;

      if (gecos)
	{
	  comma = strchr (gecos, ',');
	  if (comma)
	    *comma = 0;
	  data->real_name = convert_pwd_string_to_utf8 (gecos);
	}
    }

  /* Default fallbacks */
  if (data->real_name == NULL)
    {
      if (data->user_name != NULL)
	data->real_name = g_strdup (data->user_name);
      else
	data->real_name = g_strdup_printf("user #%d", (int)uid);
    }
  
  if (data->user_name == NULL)
    data->user_name = g_strdup_printf("%d", (int)uid);
  
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
  char buffer[4096];
  struct group gbuf;
  struct group *gbufp;
  
  if (gid_cache == NULL)
    gid_cache = g_hash_table_new_full (NULL, NULL, NULL, (GDestroyNotify)g_free);

  name = g_hash_table_lookup (gid_cache, GINT_TO_POINTER (gid));

  if (name)
    return name;

#if defined (HAVE_POSIX_GETGRGID_R)
  getgrgid_r (gid, &gbuf, buffer, sizeof(buffer), &gbufp);
#elif defined (HAVE_NONPOSIX_GETGRGID_R)
  gbufp = getgrgid_r (gid, &gbuf, buffer, sizeof(buffer));
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

static char *
get_content_type (const char          *basename,
		  const char          *path,
		  struct stat         *statbuf,
		  gboolean             is_symlink,
		  gboolean             symlink_broken,
		  GFileQueryInfoFlags  flags,
		  gboolean             fast)
{
  if (is_symlink &&
      (symlink_broken || (flags & G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS)))
    return g_strdup  ("inode/symlink");
  else if (S_ISDIR(statbuf->st_mode))
    return g_strdup ("inode/directory");
  else if (S_ISCHR(statbuf->st_mode))
    return g_strdup ("inode/chardevice");
  else if (S_ISBLK(statbuf->st_mode))
    return g_strdup ("inode/blockdevice");
  else if (S_ISFIFO(statbuf->st_mode))
    return g_strdup ("inode/fifo");
#ifdef S_ISSOCK
  else if (S_ISSOCK(statbuf->st_mode))
    return g_strdup ("inode/socket");
#endif
  else
    {
      char *content_type;
      gboolean result_uncertain;
      
      content_type = g_content_type_guess (basename, NULL, 0, &result_uncertain);
      
#ifndef G_OS_WIN32
      if (!fast && result_uncertain && path != NULL)
	{
	  guchar sniff_buffer[4096];
	  gsize sniff_length;
	  int fd;

	  sniff_length = _g_unix_content_type_get_sniff_len ();
	  if (sniff_length > 4096)
	    sniff_length = 4096;
	  
	  fd = open (path, O_RDONLY);
	  if (fd != -1)
	    {
	      ssize_t res;
	      
	      res = read (fd, sniff_buffer, sniff_length);
	      close (fd);
	      if (res > 0)
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

static char *
thumb_digest_to_ascii (unsigned char  digest[16], 
                       const char    *suffix)
{
  static const char hex_digits[] = "0123456789abcdef";
  char *res;
  int i;
  
  res = g_malloc (33 + strlen (suffix));
  
  for (i = 0; i < 16; i++) 
    {
      res[2*i] = hex_digits[digest[i] >> 4];
      res[2*i+1] = hex_digits[digest[i] & 0xf];
    }
  
  res[32] = 0;

  strcat (res, suffix);
  
  return res;
}


static void
get_thumbnail_attributes (const char *path,
                          GFileInfo  *info)
{
  char *uri;
  unsigned char digest[16];
  char *filename;
  char *basename;

  uri = g_filename_to_uri (path, NULL, NULL);
  thumb_md5 (uri, digest);
  g_free (uri);

  basename = thumb_digest_to_ascii (digest, ".png");

  filename = g_build_filename (g_get_home_dir(), ".thumbnails", "normal", basename, NULL);

  if (g_file_test (filename, G_FILE_TEST_IS_REGULAR))
    g_file_info_set_attribute_byte_string (info, G_FILE_ATTRIBUTE_THUMBNAIL_PATH, filename);
  else
    {
      g_free (filename);
      filename = g_build_filename (g_get_home_dir(), ".thumbnails", "fail", "gnome-thumbnail-factory", basename, NULL);

      if (g_file_test (filename, G_FILE_TEST_IS_REGULAR))
	g_file_info_set_attribute_boolean (info, G_FILE_ATTRIBUTE_THUMBNAILING_FAILED, TRUE);
    }
  g_free (basename);
  g_free (filename);
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
  struct stat statbuf;
  struct stat statbuf2;
  int res;
  gboolean is_symlink, symlink_broken;

  info = g_file_info_new ();

  /* Make sure we don't set any unwanted attributes */
  g_file_info_set_attribute_mask (info, attribute_matcher);
  
  g_file_info_set_name (info, basename);

  /* Avoid stat in trivial case */
  if (attribute_matcher == NULL)
    return info;

  res = g_lstat (path, &statbuf);
  if (res == -1)
    {
      g_object_unref (info);
      g_set_error (error, G_IO_ERROR,
		   g_io_error_from_errno (errno),
		   _("Error stating file '%s': %s"),
		   path, g_strerror (errno));
      return NULL;
    }
  
#ifdef S_ISLNK
  is_symlink = S_ISLNK (statbuf.st_mode);
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
	  res = stat (path, &statbuf2);

	    /* Report broken links as symlinks */
	  if (res != -1)
	    statbuf = statbuf2;
	  else
	    symlink_broken = TRUE;
	}
    }

  set_info_from_stat (info, &statbuf, attribute_matcher);
  
  if (basename != NULL && basename[0] == '.')
    g_file_info_set_is_hidden (info, TRUE);

  if (basename != NULL && basename[strlen (basename) -1] == '~')
    g_file_info_set_attribute_boolean (info, G_FILE_ATTRIBUTE_STD_IS_BACKUP, TRUE);

  if (is_symlink &&
      g_file_attribute_matcher_matches (attribute_matcher,
					G_FILE_ATTRIBUTE_STD_SYMLINK_TARGET))
    {
      char *link = read_link (path);
      g_file_info_set_symlink_target (info, link);
      g_free (link);
    }

  if (g_file_attribute_matcher_matches (attribute_matcher,
					G_FILE_ATTRIBUTE_STD_DISPLAY_NAME))
    {
      char *display_name = g_filename_display_basename (path);
      
      if (strstr (display_name, "\357\277\275") != NULL)
	{
	  char *p = display_name;
	  display_name = g_strconcat (display_name, _(" (invalid encoding)"), NULL);
	  g_free (p);
	}
      g_file_info_set_display_name (info, display_name);
      g_free (display_name);
    }
  
  if (g_file_attribute_matcher_matches (attribute_matcher,
					G_FILE_ATTRIBUTE_STD_EDIT_NAME))
    {
      char *edit_name = g_filename_display_basename (path);
      g_file_info_set_edit_name (info, edit_name);
      g_free (edit_name);
    }

  
  if (g_file_attribute_matcher_matches (attribute_matcher,
					G_FILE_ATTRIBUTE_STD_COPY_NAME))
    {
      char *copy_name = g_filename_to_utf8 (basename, -1, NULL, NULL, NULL);
      if (copy_name)
	g_file_info_set_attribute_string (info, G_FILE_ATTRIBUTE_STD_COPY_NAME, copy_name);
      g_free (copy_name);
    }

  if (g_file_attribute_matcher_matches (attribute_matcher,
					G_FILE_ATTRIBUTE_STD_CONTENT_TYPE) ||
      g_file_attribute_matcher_matches (attribute_matcher,
					G_FILE_ATTRIBUTE_STD_ICON))
    {
      char *content_type = get_content_type (basename, path, &statbuf, is_symlink, symlink_broken, flags, FALSE);

      if (content_type)
	{
	  g_file_info_set_content_type (info, content_type);

	  if (g_file_attribute_matcher_matches (attribute_matcher,
						G_FILE_ATTRIBUTE_STD_ICON))
	    {
	      char *mimetype_icon, *generic_mimetype_icon, *type_icon, *p;
	      char *icon_names[3];
	      GIcon *icon;
	      int i;

	      mimetype_icon = g_strdup (content_type);
	      
	      while ((p = strchr(mimetype_icon, '/')) != NULL)
		*p = '-';

	      p = strchr (content_type, '/');
	      if (p == NULL)
		p = content_type + strlen (content_type);

	      generic_mimetype_icon = g_malloc (p - content_type + strlen ("-x-generic") + 1);
	      memcpy (generic_mimetype_icon, content_type, p - content_type);
	      memcpy (generic_mimetype_icon + (p - content_type), "-x-generic", strlen ("-x-generic"));
	      generic_mimetype_icon[(p - content_type) + strlen ("-x-generic")] = 0;

	      /* TODO: Special case desktop dir? That could be expensive with xdg dirs... */
	      if (strcmp (path, g_get_home_dir ()) == 0)
		type_icon = "user-home";
	      else if (S_ISDIR (statbuf.st_mode)) 
		type_icon = "folder";
	      else if (statbuf.st_mode & S_IXUSR)
		type_icon = "application-x-executable";
	      else
		type_icon = "text-x-generic";

	      i = 0;
	      icon_names[i++] = mimetype_icon;
	      icon_names[i++] = generic_mimetype_icon;
	      if (strcmp (generic_mimetype_icon, type_icon) != 0 &&
		  strcmp (mimetype_icon, type_icon) != 0) 
		icon_names[i++] = type_icon;
	      
	      icon = g_themed_icon_new_from_names (icon_names, i);
	      g_file_info_set_icon (info, icon);
	      
	      g_object_unref (icon);
	      g_free (mimetype_icon);
	      g_free (generic_mimetype_icon);
	    }
	  
	  g_free (content_type);
	}
    }

  if (g_file_attribute_matcher_matches (attribute_matcher,
					G_FILE_ATTRIBUTE_STD_FAST_CONTENT_TYPE))
    {
      char *content_type = get_content_type (basename, path, &statbuf, is_symlink, symlink_broken, flags, TRUE);
      
      if (content_type)
	{
	  g_file_info_set_attribute_string (info, G_FILE_ATTRIBUTE_STD_FAST_CONTENT_TYPE, content_type);
	  g_free (content_type);
	}
    }

  if (g_file_attribute_matcher_matches (attribute_matcher,
					G_FILE_ATTRIBUTE_OWNER_USER))
    {
      char *name;
      
      name = get_username_from_uid (statbuf.st_uid);
      if (name)
	g_file_info_set_attribute_string (info, G_FILE_ATTRIBUTE_OWNER_USER, name);
      g_free (name);
    }

  if (g_file_attribute_matcher_matches (attribute_matcher,
					G_FILE_ATTRIBUTE_OWNER_USER_REAL))
    {
      char *name;
      
      name = get_realname_from_uid (statbuf.st_uid);
      if (name)
	g_file_info_set_attribute_string (info, G_FILE_ATTRIBUTE_OWNER_USER_REAL, name);
      g_free (name);
    }
  
  if (g_file_attribute_matcher_matches (attribute_matcher,
					G_FILE_ATTRIBUTE_OWNER_GROUP))
    {
      char *name;
      
      name = get_groupname_from_gid (statbuf.st_gid);
      if (name)
	g_file_info_set_attribute_string (info, G_FILE_ATTRIBUTE_OWNER_GROUP, name);
      g_free (name);
    }

  if (parent_info && parent_info->device != 0 &&
      g_file_attribute_matcher_matches (attribute_matcher, G_FILE_ATTRIBUTE_UNIX_IS_MOUNTPOINT) &&
      statbuf.st_dev != parent_info->device) 
    g_file_info_set_attribute_boolean (info, G_FILE_ATTRIBUTE_UNIX_IS_MOUNTPOINT, TRUE);
  
  get_access_rights (attribute_matcher, info, path, &statbuf, parent_info);
  
  get_selinux_context (path, info, attribute_matcher, (flags & G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS) == 0);
  get_xattrs (path, TRUE, info, attribute_matcher, (flags & G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS) == 0);
  get_xattrs (path, FALSE, info, attribute_matcher, (flags & G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS) == 0);

  if (g_file_attribute_matcher_matches (attribute_matcher,
					G_FILE_ATTRIBUTE_THUMBNAIL_PATH))
    get_thumbnail_attributes (path, info);
  
  g_file_info_unset_attribute_mask (info);

  return info;
}

GFileInfo *
_g_local_file_info_get_from_fd (int      fd,
				char    *attributes,
				GError **error)
{
  struct stat stat_buf;
  GFileAttributeMatcher *matcher;
  GFileInfo *info;
  
  if (fstat (fd, &stat_buf) == -1)
    {
      g_set_error (error, G_IO_ERROR,
		   g_io_error_from_errno (errno),
		   _("Error stating file descriptor: %s"),
		   g_strerror (errno));
      return NULL;
    }

  info = g_file_info_new ();

  matcher = g_file_attribute_matcher_new (attributes);

  /* Make sure we don't set any unwanted attributes */
  g_file_info_set_attribute_mask (info, matcher);
  
  set_info_from_stat (info, &stat_buf, matcher);
  
#ifdef HAVE_SELINUX
  if (g_file_attribute_matcher_matches (matcher, "selinux:context") &&
      is_selinux_enabled ())
    {
      char *context;
      if (fgetfilecon_raw (fd, &context) >= 0)
	{
	  g_file_info_set_attribute_string (info, "selinux:context", context);
	  freecon(context);
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
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
		   _("Invalid attribute type (uint32 expected)"));
      return FALSE;
    }

  *val_out = value->u.uint32;
  
  return TRUE;
}

static gboolean
get_uint64 (const GFileAttributeValue  *value,
	    guint64                    *val_out,
	    GError                    **error)
{
  if (value->type != G_FILE_ATTRIBUTE_TYPE_UINT64)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
		   _("Invalid attribute type (uint64 expected)"));
      return FALSE;
    }

  *val_out = value->u.uint64;
  
  return TRUE;
}

#if defined(HAVE_SYMLINK)
static gboolean
get_byte_string (const GFileAttributeValue  *value,
		 const char                **val_out,
		 GError                    **error)
{
  if (value->type != G_FILE_ATTRIBUTE_TYPE_BYTE_STRING)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
		   _("Invalid attribute type (byte string expected)"));
      return FALSE;
    }

  *val_out = value->u.string;
  
  return TRUE;
}
#endif

static gboolean
set_unix_mode (char                       *filename,
	       const GFileAttributeValue  *value,
	       GError                    **error)
{
  guint32 val;
  
  if (!get_uint32 (value, &val, error))
    return FALSE;
  
  if (g_chmod (filename, val) == -1)
    {
      g_set_error (error, G_IO_ERROR,
		   g_io_error_from_errno (errno),
		   _("Error setting permissions: %s"),
		   g_strerror (errno));
      return FALSE;
    }
  return TRUE;
}

#ifdef HAVE_CHOWN
static gboolean
set_unix_uid_gid (char                       *filename,
		  const GFileAttributeValue  *uid_value,
		  const GFileAttributeValue  *gid_value,
		  GFileQueryInfoFlags         flags,
		  GError                    **error)
{
  int res;
  guint32 val;
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
  
  if (flags & G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS)
    res = lchown (filename, uid, gid);
  else
    res = chown (filename, uid, gid);
  
  if (res == -1)
    {
      g_set_error (error, G_IO_ERROR,
		   g_io_error_from_errno (errno),
		   _("Error setting owner: %s"),
		   g_strerror (errno));
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
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
		   _("symlink must be non-NULL"));
      return FALSE;
    }
  
  if (g_lstat (filename, &statbuf))
    {
      g_set_error (error, G_IO_ERROR,
		   g_io_error_from_errno (errno),
		   _("Error setting symlink: %s"),
		   g_strerror (errno));
      return FALSE;
    }
  
  if (!S_ISLNK (statbuf.st_mode))
    {
      g_set_error (error, G_IO_ERROR,
		   G_IO_ERROR_NOT_SYMBOLIC_LINK,
		   _("Error setting symlink: file is not a symlink"));
      return FALSE;
    }
  
  if (g_unlink (filename))
    {
      g_set_error (error, G_IO_ERROR,
		   g_io_error_from_errno (errno),
		   _("Error setting symlink: %s"),
		   g_strerror (errno));
      return FALSE;
    }
  
  if (symlink (filename, val) != 0)
    {
      g_set_error (error, G_IO_ERROR,
		   g_io_error_from_errno (errno),
		   _("Error setting symlink: %s"),
		   g_strerror (errno));
      return FALSE;
    }
  
  return TRUE;
}
#endif

static int
lazy_stat (char        *filename, 
           struct stat *statbuf, 
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


#ifdef HAVE_UTIMES
static gboolean
set_mtime_atime (char                       *filename,
		 const GFileAttributeValue  *mtime_value,
		 const GFileAttributeValue  *mtime_usec_value,
		 const GFileAttributeValue  *atime_value,
		 const GFileAttributeValue  *atime_usec_value,
		 GError                    **error)
{
  int res;
  guint64 val;
  guint32 val_usec;
  struct stat statbuf;
  gboolean got_stat = FALSE;
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
	  times[0].tv_sec = statbuf.st_mtime;
#if defined (HAVE_STRUCT_STAT_ST_ATIMENSEC)
	  times[0].tv_usec = statbuf.st_atimensec / 1000;
#elif defined (HAVE_STRUCT_STAT_ST_ATIM_TV_NSEC)
	  times[0].tv_usec = statbuf.st_atim.tv_nsec / 1000;
#endif
	}
    }
  
  if (atime_usec_value)
    {
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
      if (!get_uint32 (mtime_usec_value, &val_usec, error))
	return FALSE;
      times[1].tv_usec = val_usec;
    }
  
  res = utimes(filename, times);
  if (res == -1)
    {
      g_set_error (error, G_IO_ERROR,
		   g_io_error_from_errno (errno),
		   _("Error setting owner: %s"),
		   g_strerror (errno));
	  return FALSE;
    }
  return TRUE;
}
#endif

gboolean
_g_local_file_info_set_attribute (char                       *filename,
				  const char                 *attribute,
				  const GFileAttributeValue  *value,
				  GFileQueryInfoFlags         flags,
				  GCancellable               *cancellable,
				  GError                    **error)
{
  if (strcmp (attribute, G_FILE_ATTRIBUTE_UNIX_MODE) == 0)
    return set_unix_mode (filename, value, error);
  
#ifdef HAVE_CHOWN
  else if (strcmp (attribute, G_FILE_ATTRIBUTE_UNIX_UID) == 0)
    return set_unix_uid_gid (filename, value, NULL, flags, error);
  else if (strcmp (attribute, G_FILE_ATTRIBUTE_UNIX_GID) == 0)
    return set_unix_uid_gid (filename, NULL, value, flags, error);
#endif
  
#ifdef HAVE_SYMLINK
  else if (strcmp (attribute, G_FILE_ATTRIBUTE_STD_SYMLINK_TARGET) == 0)
    return set_symlink (filename, value, error);
#endif

#ifdef HAVE_UTIMES
  else if (strcmp (attribute, G_FILE_ATTRIBUTE_TIME_MODIFIED) == 0)
    return set_mtime_atime (filename, value, NULL, NULL, NULL, error);
  else if (strcmp (attribute, G_FILE_ATTRIBUTE_TIME_MODIFIED_USEC) == 0)
    return set_mtime_atime (filename, NULL, value, NULL, NULL, error);
  else if (strcmp (attribute, G_FILE_ATTRIBUTE_TIME_ACCESS) == 0)
    return set_mtime_atime (filename, NULL, NULL, value, NULL, error);
  else if (strcmp (attribute, G_FILE_ATTRIBUTE_TIME_ACCESS_USEC) == 0)
    return set_mtime_atime (filename, NULL, NULL, NULL, value, error);
#endif

#ifdef HAVE_XATTR
  else if (g_str_has_prefix (attribute, "xattr:"))
    return set_xattr (filename, attribute, value, error);
  else if (g_str_has_prefix (attribute, "xattr_sys:"))
    return set_xattr (filename, attribute, value, error);
#endif
  
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
  GFileAttributeValue *value, *uid, *gid;
  GFileAttributeValue *mtime, *mtime_usec, *atime, *atime_usec;
  GFileAttributeStatus status;
  gboolean res;
  
  /* Handles setting multiple specified data in a single set, and takes care
     of ordering restrictions when setting attributes */

  res = TRUE;

  /* Set symlink first, since this recreates the file */
#ifdef HAVE_SYMLINK
  value = g_file_info_get_attribute (info, G_FILE_ATTRIBUTE_STD_SYMLINK_TARGET);
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

#ifdef HAVE_CHOWN
  /* Group uid and gid setting into one call
   * Change ownership before permissions, since ownership changes can
     change permissions (e.g. setuid)
   */
  uid = g_file_info_get_attribute (info, G_FILE_ATTRIBUTE_UNIX_UID);
  gid = g_file_info_get_attribute (info, G_FILE_ATTRIBUTE_UNIX_GID);
  
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
  
  value = g_file_info_get_attribute (info, G_FILE_ATTRIBUTE_UNIX_MODE);
  if (value)
    {
      if (!set_unix_mode (filename, value, error))
	{
	  value->status = G_FILE_ATTRIBUTE_STATUS_ERROR_SETTING;
	  res = FALSE;
	  /* Don't set error multiple times */
	  error = NULL;
	}
      else
	value->status = G_FILE_ATTRIBUTE_STATUS_SET;
	
    }

#ifdef HAVE_UTIMES
  /* Group all time settings into one call
   * Change times as the last thing to avoid it changing due to metadata changes
   */
  
  mtime = g_file_info_get_attribute (info, G_FILE_ATTRIBUTE_TIME_MODIFIED);
  mtime_usec = g_file_info_get_attribute (info, G_FILE_ATTRIBUTE_TIME_MODIFIED_USEC);
  atime = g_file_info_get_attribute (info, G_FILE_ATTRIBUTE_TIME_ACCESS);
  atime_usec = g_file_info_get_attribute (info, G_FILE_ATTRIBUTE_TIME_ACCESS_USEC);

  if (mtime || mtime_usec || atime || atime_usec)
    {
      if (!set_mtime_atime (filename, mtime, mtime_usec, atime, atime_usec, error))
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
      if (atime)
	atime->status = status;
      if (atime_usec)
	atime_usec->status = status;
    }
#endif

  /* xattrs are handled by default callback */

  return res;
}


/*
 * This code implements the MD5 message-digest algorithm.
 * The algorithm is due to Ron Rivest.  This code was
 * written by Colin Plumb in 1993, no copyright is claimed.
 * This code is in the public domain; do with it what you wish.
 *
 * Equivalent code is available from RSA Data Security, Inc.
 * This code has been tested against that, and is equivalent,
 * except that you don't need to include two pages of legalese
 * with every copy.
 *
 * To compute the message digest of a chunk of bytes, declare an
 * ThumbMD5Context structure, pass it to thumb_md5_init, call
 * thumb_md5_update as needed on buffers full of bytes, and then call
 * thumb_md5_final, which will fill a supplied 32-byte array with the
 * digest in ascii form. 
 *
 */

static void thumb_md5_init      (struct ThumbMD5Context *context);
static void thumb_md5_update    (struct ThumbMD5Context *context,
				 unsigned char const    *buf,
				 unsigned                len);
static void thumb_md5_final     (unsigned char           digest[16],
				 struct ThumbMD5Context *context);
static void thumb_md5_transform (guint32                 buf[4],
				 guint32 const           in[16]);


static void
thumb_md5 (const char *string, unsigned char digest[16])
{
  struct ThumbMD5Context md5_context;
  
  thumb_md5_init (&md5_context);
  thumb_md5_update (&md5_context, (unsigned char *)string, strlen (string));
  thumb_md5_final (digest, &md5_context);
}

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
#define byteReverse(buf, len)	/* Nothing */
#else

/*
 * Note: this code is harmless on little-endian machines.
 */
static void
byteReverse (unsigned char *buf, 
             unsigned       longs)
{
    guint32 t;
    do {
	t = (guint32) ((unsigned) buf[3] << 8 | buf[2]) << 16 |
	    ((unsigned) buf[1] << 8 | buf[0]);
	*(guint32 *) buf = t;
	buf += 4;
    } while (--longs);
}

#endif

/*
 * Start MD5 accumulation.  Set bit count to 0 and buffer to mysterious
 * initialization constants.
 */
static void 
thumb_md5_init (struct ThumbMD5Context *ctx)
{
    ctx->buf[0] = 0x67452301;
    ctx->buf[1] = 0xefcdab89;
    ctx->buf[2] = 0x98badcfe;
    ctx->buf[3] = 0x10325476;

    ctx->bits[0] = 0;
    ctx->bits[1] = 0;
}

/*
 * Update context to reflect the concatenation of another buffer full
 * of bytes.
 */
static void 
thumb_md5_update (struct ThumbMD5Context *ctx,
		  unsigned char const    *buf,
		  unsigned                len)
{
    guint32 t;

    /* Update bitcount */

    t = ctx->bits[0];
    if ((ctx->bits[0] = t + ((guint32) len << 3)) < t)
	ctx->bits[1]++;		/* Carry from low to high */
    ctx->bits[1] += len >> 29;

    t = (t >> 3) & 0x3f;	/* Bytes already in shsInfo->data */

    /* Handle any leading odd-sized chunks */

    if (t) {
	unsigned char *p = (unsigned char *) ctx->in + t;

	t = 64 - t;
	if (len < t) {
	    memcpy (p, buf, len);
	    return;
	}
	memcpy (p, buf, t);
	byteReverse (ctx->in, 16);
	thumb_md5_transform (ctx->buf, (guint32 *) ctx->in);
	buf += t;
	len -= t;
    }

    /* Process data in 64-byte chunks */

    while (len >= 64) {
	memcpy (ctx->in, buf, 64);
	byteReverse (ctx->in, 16);
	thumb_md5_transform (ctx->buf, (guint32 *) ctx->in);
	buf += 64;
	len -= 64;
    }

    /* Handle any remaining bytes of data. */

    memcpy(ctx->in, buf, len);
}

/*
 * Final wrapup - pad to 64-byte boundary with the bit pattern 
 * 1 0* (64-bit count of bits processed, MSB-first)
 */
static void 
thumb_md5_final (unsigned char           digest[16], 
                 struct ThumbMD5Context *ctx)
{
    unsigned count;
    unsigned char *p;

    /* Compute number of bytes mod 64 */
    count = (ctx->bits[0] >> 3) & 0x3F;

    /* Set the first char of padding to 0x80.  This is safe since there is
       always at least one byte free */
    p = ctx->in + count;
    *p++ = 0x80;

    /* Bytes of padding needed to make 64 bytes */
    count = 64 - 1 - count;

    /* Pad out to 56 mod 64 */
    if (count < 8) {
	/* Two lots of padding:  Pad the first block to 64 bytes */
	memset (p, 0, count);
	byteReverse (ctx->in, 16);
	thumb_md5_transform (ctx->buf, (guint32 *) ctx->in);

	/* Now fill the next block with 56 bytes */
	memset(ctx->in, 0, 56);
    } else {
	/* Pad block to 56 bytes */
	memset(p, 0, count - 8);
    }
    byteReverse(ctx->in, 14);

    /* Append length in bits and transform */
    ((guint32 *) ctx->in)[14] = ctx->bits[0];
    ((guint32 *) ctx->in)[15] = ctx->bits[1];

    thumb_md5_transform (ctx->buf, (guint32 *) ctx->in);
    byteReverse ((unsigned char *) ctx->buf, 4);
    memcpy (digest, ctx->buf, 16);
    memset (ctx, 0, sizeof(ctx));	/* In case it's sensitive */
}


/* The four core functions - F1 is optimized somewhat */

#define F1(x, y, z) (z ^ (x & (y ^ z)))
#define F2(x, y, z) F1 (z, x, y)
#define F3(x, y, z) (x ^ y ^ z)
#define F4(x, y, z) (y ^ (x | ~z))

/* This is the central step in the MD5 algorithm. */
#define thumb_md5_step(f, w, x, y, z, data, s) \
	( w += f(x, y, z) + data,  w = w<<s | w>>(32-s),  w += x )

/*
 * The core of the MD5 algorithm, this alters an existing MD5 hash to
 * reflect the addition of 16 longwords of new data.  ThumbMD5Update blocks
 * the data and converts bytes into longwords for this routine.
 */
static void 
thumb_md5_transform (guint32       buf[4], 
                     guint32 const in[16])
{
    register guint32 a, b, c, d;

    a = buf[0];
    b = buf[1];
    c = buf[2];
    d = buf[3];

    thumb_md5_step(F1, a, b, c, d, in[0] + 0xd76aa478, 7);
    thumb_md5_step(F1, d, a, b, c, in[1] + 0xe8c7b756, 12);
    thumb_md5_step(F1, c, d, a, b, in[2] + 0x242070db, 17);
    thumb_md5_step(F1, b, c, d, a, in[3] + 0xc1bdceee, 22);
    thumb_md5_step(F1, a, b, c, d, in[4] + 0xf57c0faf, 7);
    thumb_md5_step(F1, d, a, b, c, in[5] + 0x4787c62a, 12);
    thumb_md5_step(F1, c, d, a, b, in[6] + 0xa8304613, 17);
    thumb_md5_step(F1, b, c, d, a, in[7] + 0xfd469501, 22);
    thumb_md5_step(F1, a, b, c, d, in[8] + 0x698098d8, 7);
    thumb_md5_step(F1, d, a, b, c, in[9] + 0x8b44f7af, 12);
    thumb_md5_step(F1, c, d, a, b, in[10] + 0xffff5bb1, 17);
    thumb_md5_step(F1, b, c, d, a, in[11] + 0x895cd7be, 22);
    thumb_md5_step(F1, a, b, c, d, in[12] + 0x6b901122, 7);
    thumb_md5_step(F1, d, a, b, c, in[13] + 0xfd987193, 12);
    thumb_md5_step(F1, c, d, a, b, in[14] + 0xa679438e, 17);
    thumb_md5_step(F1, b, c, d, a, in[15] + 0x49b40821, 22);
		
    thumb_md5_step(F2, a, b, c, d, in[1] + 0xf61e2562, 5);
    thumb_md5_step(F2, d, a, b, c, in[6] + 0xc040b340, 9);
    thumb_md5_step(F2, c, d, a, b, in[11] + 0x265e5a51, 14);
    thumb_md5_step(F2, b, c, d, a, in[0] + 0xe9b6c7aa, 20);
    thumb_md5_step(F2, a, b, c, d, in[5] + 0xd62f105d, 5);
    thumb_md5_step(F2, d, a, b, c, in[10] + 0x02441453, 9);
    thumb_md5_step(F2, c, d, a, b, in[15] + 0xd8a1e681, 14);
    thumb_md5_step(F2, b, c, d, a, in[4] + 0xe7d3fbc8, 20);
    thumb_md5_step(F2, a, b, c, d, in[9] + 0x21e1cde6, 5);
    thumb_md5_step(F2, d, a, b, c, in[14] + 0xc33707d6, 9);
    thumb_md5_step(F2, c, d, a, b, in[3] + 0xf4d50d87, 14);
    thumb_md5_step(F2, b, c, d, a, in[8] + 0x455a14ed, 20);
    thumb_md5_step(F2, a, b, c, d, in[13] + 0xa9e3e905, 5);
    thumb_md5_step(F2, d, a, b, c, in[2] + 0xfcefa3f8, 9);
    thumb_md5_step(F2, c, d, a, b, in[7] + 0x676f02d9, 14);
    thumb_md5_step(F2, b, c, d, a, in[12] + 0x8d2a4c8a, 20);
		
    thumb_md5_step(F3, a, b, c, d, in[5] + 0xfffa3942, 4);
    thumb_md5_step(F3, d, a, b, c, in[8] + 0x8771f681, 11);
    thumb_md5_step(F3, c, d, a, b, in[11] + 0x6d9d6122, 16);
    thumb_md5_step(F3, b, c, d, a, in[14] + 0xfde5380c, 23);
    thumb_md5_step(F3, a, b, c, d, in[1] + 0xa4beea44, 4);
    thumb_md5_step(F3, d, a, b, c, in[4] + 0x4bdecfa9, 11);
    thumb_md5_step(F3, c, d, a, b, in[7] + 0xf6bb4b60, 16);
    thumb_md5_step(F3, b, c, d, a, in[10] + 0xbebfbc70, 23);
    thumb_md5_step(F3, a, b, c, d, in[13] + 0x289b7ec6, 4);
    thumb_md5_step(F3, d, a, b, c, in[0] + 0xeaa127fa, 11);
    thumb_md5_step(F3, c, d, a, b, in[3] + 0xd4ef3085, 16);
    thumb_md5_step(F3, b, c, d, a, in[6] + 0x04881d05, 23);
    thumb_md5_step(F3, a, b, c, d, in[9] + 0xd9d4d039, 4);
    thumb_md5_step(F3, d, a, b, c, in[12] + 0xe6db99e5, 11);
    thumb_md5_step(F3, c, d, a, b, in[15] + 0x1fa27cf8, 16);
    thumb_md5_step(F3, b, c, d, a, in[2] + 0xc4ac5665, 23);
		
    thumb_md5_step(F4, a, b, c, d, in[0] + 0xf4292244, 6);
    thumb_md5_step(F4, d, a, b, c, in[7] + 0x432aff97, 10);
    thumb_md5_step(F4, c, d, a, b, in[14] + 0xab9423a7, 15);
    thumb_md5_step(F4, b, c, d, a, in[5] + 0xfc93a039, 21);
    thumb_md5_step(F4, a, b, c, d, in[12] + 0x655b59c3, 6);
    thumb_md5_step(F4, d, a, b, c, in[3] + 0x8f0ccc92, 10);
    thumb_md5_step(F4, c, d, a, b, in[10] + 0xffeff47d, 15);
    thumb_md5_step(F4, b, c, d, a, in[1] + 0x85845dd1, 21);
    thumb_md5_step(F4, a, b, c, d, in[8] + 0x6fa87e4f, 6);
    thumb_md5_step(F4, d, a, b, c, in[15] + 0xfe2ce6e0, 10);
    thumb_md5_step(F4, c, d, a, b, in[6] + 0xa3014314, 15);
    thumb_md5_step(F4, b, c, d, a, in[13] + 0x4e0811a1, 21);
    thumb_md5_step(F4, a, b, c, d, in[4] + 0xf7537e82, 6);
    thumb_md5_step(F4, d, a, b, c, in[11] + 0xbd3af235, 10);
    thumb_md5_step(F4, c, d, a, b, in[2] + 0x2ad7d2bb, 15);
    thumb_md5_step(F4, b, c, d, a, in[9] + 0xeb86d391, 21);

    buf[0] += a;
    buf[1] += b;
    buf[2] += c;
    buf[3] += d;
}
