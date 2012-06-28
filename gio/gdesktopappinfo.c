/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright (C) 2006-2007 Red Hat, Inc.
 * Copyright © 2007 Ryan Lortie
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

#include "config.h"

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#ifdef HAVE_CRT_EXTERNS_H
#include <crt_externs.h>
#endif

#include "gcontenttypeprivate.h"
#include "gdesktopappinfo.h"
#include "gfile.h"
#include "gioerror.h"
#include "gthemedicon.h"
#include "gfileicon.h"
#include <glib/gstdio.h>
#include "glibintl.h"
#include "giomodule-priv.h"
#include "gappinfo.h"


/**
 * SECTION:gdesktopappinfo
 * @title: GDesktopAppInfo
 * @short_description: Application information from desktop files
 * @include: gio/gdesktopappinfo.h
 *
 * #GDesktopAppInfo is an implementation of #GAppInfo based on
 * desktop files.
 *
 * Note that <filename>&lt;gio/gdesktopappinfo.h&gt;</filename> belongs to
 * the UNIX-specific GIO interfaces, thus you have to use the
 * <filename>gio-unix-2.0.pc</filename> pkg-config file when using it.
 */

#define DEFAULT_APPLICATIONS_GROUP  "Default Applications"
#define ADDED_ASSOCIATIONS_GROUP    "Added Associations"
#define REMOVED_ASSOCIATIONS_GROUP  "Removed Associations"
#define MIME_CACHE_GROUP            "MIME Cache"
#define GENERIC_NAME_KEY            "GenericName"
#define FULL_NAME_KEY               "X-GNOME-FullName"
#define KEYWORDS_KEY                "Keywords"

enum {
  PROP_0,
  PROP_FILENAME
};

static void     g_desktop_app_info_iface_init         (GAppInfoIface    *iface);
static GList *  get_all_desktop_entries_for_mime_type (const char       *base_mime_type,
						       const char      **except,
						       gboolean          include_fallback,
                                                       char            **explicit_default);
static void     mime_info_cache_reload                (const char       *dir);
static gboolean g_desktop_app_info_ensure_saved       (GDesktopAppInfo  *info,
						       GError          **error);

/**
 * GDesktopAppInfo:
 * 
 * Information about an installed application from a desktop file.
 */
struct _GDesktopAppInfo
{
  GObject parent_instance;

  char *desktop_id;
  char *filename;

  char *name;
  char *generic_name;
  char *fullname;
  char *comment;
  char *icon_name;
  GIcon *icon;
  char **keywords;
  char **only_show_in;
  char **not_show_in;
  char *try_exec;
  char *exec;
  char *binary;
  char *path;
  char *categories;

  guint nodisplay       : 1;
  guint hidden          : 1;
  guint terminal        : 1;
  guint startup_notify  : 1;
  guint no_fuse         : 1;
  /* FIXME: what about StartupWMClass ? */
};

typedef enum {
  UPDATE_MIME_NONE = 1 << 0,
  UPDATE_MIME_SET_DEFAULT = 1 << 1,
  UPDATE_MIME_SET_NON_DEFAULT = 1 << 2,
  UPDATE_MIME_REMOVE = 1 << 3,
  UPDATE_MIME_SET_LAST_USED = 1 << 4,
} UpdateMimeFlags;

G_DEFINE_TYPE_WITH_CODE (GDesktopAppInfo, g_desktop_app_info, G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (G_TYPE_APP_INFO,
						g_desktop_app_info_iface_init))

G_LOCK_DEFINE_STATIC (g_desktop_env);
static gchar *g_desktop_env = NULL;

static gpointer
search_path_init (gpointer data)
{
  char **args = NULL;
  const char * const *data_dirs;
  const char *user_data_dir;
  int i, length, j;

  data_dirs = g_get_system_data_dirs ();
  length = g_strv_length ((char **) data_dirs);
  
  args = g_new (char *, length + 2);
  
  j = 0;
  user_data_dir = g_get_user_data_dir ();
  args[j++] = g_build_filename (user_data_dir, "applications", NULL);
  for (i = 0; i < length; i++)
    args[j++] = g_build_filename (data_dirs[i],
				  "applications", NULL);
  args[j++] = NULL;
  
  return args;
}
  
static const char * const *
get_applications_search_path (void)
{
  static GOnce once_init = G_ONCE_INIT;
  return g_once (&once_init, search_path_init, NULL);
}

static void
g_desktop_app_info_finalize (GObject *object)
{
  GDesktopAppInfo *info;

  info = G_DESKTOP_APP_INFO (object);

  g_free (info->desktop_id);
  g_free (info->filename);
  g_free (info->name);
  g_free (info->generic_name);
  g_free (info->fullname);
  g_free (info->comment);
  g_free (info->icon_name);
  if (info->icon)
    g_object_unref (info->icon);
  g_strfreev (info->keywords);
  g_strfreev (info->only_show_in);
  g_strfreev (info->not_show_in);
  g_free (info->try_exec);
  g_free (info->exec);
  g_free (info->binary);
  g_free (info->path);
  g_free (info->categories);
  
  G_OBJECT_CLASS (g_desktop_app_info_parent_class)->finalize (object);
}

static void
g_desktop_app_info_set_property(GObject         *object,
				guint            prop_id,
				const GValue    *value,
				GParamSpec      *pspec)
{
  GDesktopAppInfo *self = G_DESKTOP_APP_INFO (object);

  switch (prop_id)
    {
    case PROP_FILENAME:
      self->filename = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
g_desktop_app_info_get_property(GObject         *object,
				guint            prop_id,
				GValue          *value,
				GParamSpec      *pspec)
{
  GDesktopAppInfo *self = G_DESKTOP_APP_INFO (object);

  switch (prop_id)
    {
    case PROP_FILENAME:
      g_value_set_string (value, self->filename);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
g_desktop_app_info_class_init (GDesktopAppInfoClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  
  gobject_class->get_property = g_desktop_app_info_get_property;
  gobject_class->set_property = g_desktop_app_info_set_property;
  gobject_class->finalize = g_desktop_app_info_finalize;

  /**
   * GDesktopAppInfo:filename:
   *
   * The origin filename of this #GDesktopAppInfo
   */
  g_object_class_install_property (gobject_class,
                                   PROP_FILENAME,
                                   g_param_spec_string ("filename", "Filename", "",
							NULL,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

static void
g_desktop_app_info_init (GDesktopAppInfo *local)
{
}

static char *
binary_from_exec (const char *exec)
{
  const char *p, *start;
  
  p = exec;
  while (*p == ' ')
    p++;
  start = p;
  while (*p != ' ' && *p != 0)
    p++;
  
  return g_strndup (start, p - start);
  
}

static gboolean
g_desktop_app_info_load_from_keyfile (GDesktopAppInfo *info, 
				      GKeyFile        *key_file)
{
  char *start_group;
  char *type;
  char *try_exec;

  start_group = g_key_file_get_start_group (key_file);
  if (start_group == NULL || strcmp (start_group, G_KEY_FILE_DESKTOP_GROUP) != 0)
    {
      g_free (start_group);
      return FALSE;
    }
  g_free (start_group);

  type = g_key_file_get_string (key_file,
                                G_KEY_FILE_DESKTOP_GROUP,
                                G_KEY_FILE_DESKTOP_KEY_TYPE,
                                NULL);
  if (type == NULL || strcmp (type, G_KEY_FILE_DESKTOP_TYPE_APPLICATION) != 0)
    {
      g_free (type);
      return FALSE;
    }
  g_free (type);

  try_exec = g_key_file_get_string (key_file,
                                    G_KEY_FILE_DESKTOP_GROUP,
                                    G_KEY_FILE_DESKTOP_KEY_TRY_EXEC,
                                    NULL);
  if (try_exec && try_exec[0] != '\0')
    {
      char *t;
      t = g_find_program_in_path (try_exec);
      if (t == NULL)
	{
	  g_free (try_exec);
	  return FALSE;
	}
      g_free (t);
    }

  info->name = g_key_file_get_locale_string (key_file, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_NAME, NULL, NULL);
  info->generic_name = g_key_file_get_locale_string (key_file, G_KEY_FILE_DESKTOP_GROUP, GENERIC_NAME_KEY, NULL, NULL);
  info->fullname = g_key_file_get_locale_string (key_file, G_KEY_FILE_DESKTOP_GROUP, FULL_NAME_KEY, NULL, NULL);
  info->keywords = g_key_file_get_locale_string_list (key_file, G_KEY_FILE_DESKTOP_GROUP, KEYWORDS_KEY, NULL, NULL, NULL);
  info->comment = g_key_file_get_locale_string (key_file, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_COMMENT, NULL, NULL);
  info->nodisplay = g_key_file_get_boolean (key_file, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_NO_DISPLAY, NULL) != FALSE;
  info->icon_name =  g_key_file_get_locale_string (key_file, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_ICON, NULL, NULL);
  info->only_show_in = g_key_file_get_string_list (key_file, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_ONLY_SHOW_IN, NULL, NULL);
  info->not_show_in = g_key_file_get_string_list (key_file, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_NOT_SHOW_IN, NULL, NULL);
  info->try_exec = try_exec;
  info->exec = g_key_file_get_string (key_file, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_EXEC, NULL);
  info->path = g_key_file_get_string (key_file, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_PATH, NULL);
  info->terminal = g_key_file_get_boolean (key_file, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_TERMINAL, NULL) != FALSE;
  info->startup_notify = g_key_file_get_boolean (key_file, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_STARTUP_NOTIFY, NULL) != FALSE;
  info->no_fuse = g_key_file_get_boolean (key_file, G_KEY_FILE_DESKTOP_GROUP, "X-GIO-NoFuse", NULL) != FALSE;
  info->hidden = g_key_file_get_boolean (key_file, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_HIDDEN, NULL) != FALSE;
  info->categories = g_key_file_get_string (key_file, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_CATEGORIES, NULL);
  
  info->icon = NULL;
  if (info->icon_name)
    {
      if (g_path_is_absolute (info->icon_name))
	{
	  GFile *file;
	  
	  file = g_file_new_for_path (info->icon_name);
	  info->icon = g_file_icon_new (file);
	  g_object_unref (file);
	}
      else
        {
          char *p;

          /* Work around a common mistake in desktop files */    
          if ((p = strrchr (info->icon_name, '.')) != NULL &&
              (strcmp (p, ".png") == 0 ||
               strcmp (p, ".xpm") == 0 ||
               strcmp (p, ".svg") == 0)) 
            *p = 0;

	  info->icon = g_themed_icon_new (info->icon_name);
        }
    }
  
  if (info->exec)
    info->binary = binary_from_exec (info->exec);
  
  if (info->path && info->path[0] == '\0')
    {
      g_free (info->path);
      info->path = NULL;
    }

  return TRUE;
}

static gboolean
g_desktop_app_info_load_file (GDesktopAppInfo *self)
{
  GKeyFile *key_file;
  gboolean retval = FALSE;
  
  g_return_val_if_fail (self->filename != NULL, FALSE);

  key_file = g_key_file_new ();
  
  if (g_key_file_load_from_file (key_file,
				 self->filename,
				 G_KEY_FILE_NONE,
				 NULL))
    {
      retval = g_desktop_app_info_load_from_keyfile (self, key_file);
    }

  g_key_file_free (key_file);
  return retval;
}

/**
 * g_desktop_app_info_new_from_keyfile:
 * @key_file: an opened #GKeyFile
 * 
 * Creates a new #GDesktopAppInfo.
 *
 * Returns: a new #GDesktopAppInfo or %NULL on error.
 *
 * Since: 2.18
 **/
GDesktopAppInfo *
g_desktop_app_info_new_from_keyfile (GKeyFile *key_file)
{
  GDesktopAppInfo *info;

  info = g_object_new (G_TYPE_DESKTOP_APP_INFO, NULL);
  info->filename = NULL;
  if (!g_desktop_app_info_load_from_keyfile (info, key_file))
    {
      g_object_unref (info);
      return NULL;
    }
  return info;
}

/**
 * g_desktop_app_info_new_from_filename:
 * @filename: the path of a desktop file, in the GLib filename encoding
 * 
 * Creates a new #GDesktopAppInfo.
 *
 * Returns: a new #GDesktopAppInfo or %NULL on error.
 **/
GDesktopAppInfo *
g_desktop_app_info_new_from_filename (const char *filename)
{
  GDesktopAppInfo *info = NULL;

  info = g_object_new (G_TYPE_DESKTOP_APP_INFO, "filename", filename, NULL);
  if (!g_desktop_app_info_load_file (info))
    {
      g_object_unref (info);
      return NULL;
    }
  return info;
}

/**
 * g_desktop_app_info_new:
 * @desktop_id: the desktop file id
 * 
 * Creates a new #GDesktopAppInfo based on a desktop file id. 
 *
 * A desktop file id is the basename of the desktop file, including the 
 * .desktop extension. GIO is looking for a desktop file with this name 
 * in the <filename>applications</filename> subdirectories of the XDG data
 * directories (i.e. the directories specified in the 
 * <envar>XDG_DATA_HOME</envar> and <envar>XDG_DATA_DIRS</envar> environment 
 * variables). GIO also supports the prefix-to-subdirectory mapping that is
 * described in the <ulink url="http://standards.freedesktop.org/menu-spec/latest/">Menu Spec</ulink> 
 * (i.e. a desktop id of kde-foo.desktop will match
 * <filename>/usr/share/applications/kde/foo.desktop</filename>).
 * 
 * Returns: a new #GDesktopAppInfo, or %NULL if no desktop file with that id
 */
GDesktopAppInfo *
g_desktop_app_info_new (const char *desktop_id)
{
  GDesktopAppInfo *appinfo;
  const char * const *dirs;
  char *basename;
  int i;

  dirs = get_applications_search_path ();

  basename = g_strdup (desktop_id);
  
  for (i = 0; dirs[i] != NULL; i++)
    {
      char *filename;
      char *p;

      filename = g_build_filename (dirs[i], desktop_id, NULL);
      appinfo = g_desktop_app_info_new_from_filename (filename);
      g_free (filename);
      if (appinfo != NULL)
	goto found;

      p = basename;
      while ((p = strchr (p, '-')) != NULL)
	{
	  *p = '/';
	  
	  filename = g_build_filename (dirs[i], basename, NULL);
	  appinfo = g_desktop_app_info_new_from_filename (filename);
	  g_free (filename);
	  if (appinfo != NULL)
	    goto found;
	  *p = '-';
	  p++;
	}
    }
  
  g_free (basename);
  return NULL;

 found:
  g_free (basename);
  
  appinfo->desktop_id = g_strdup (desktop_id);

  if (g_desktop_app_info_get_is_hidden (appinfo))
    {
      g_object_unref (appinfo);
      appinfo = NULL;
    }
  
  return appinfo;
}

static GAppInfo *
g_desktop_app_info_dup (GAppInfo *appinfo)
{
  GDesktopAppInfo *info = G_DESKTOP_APP_INFO (appinfo);
  GDesktopAppInfo *new_info;
  
  new_info = g_object_new (G_TYPE_DESKTOP_APP_INFO, NULL);

  new_info->filename = g_strdup (info->filename);
  new_info->desktop_id = g_strdup (info->desktop_id);
  
  new_info->name = g_strdup (info->name);
  new_info->generic_name = g_strdup (info->generic_name);
  new_info->fullname = g_strdup (info->fullname);
  new_info->keywords = g_strdupv (info->keywords);
  new_info->comment = g_strdup (info->comment);
  new_info->nodisplay = info->nodisplay;
  new_info->icon_name = g_strdup (info->icon_name);
  if (info->icon)
    new_info->icon = g_object_ref (info->icon);
  new_info->only_show_in = g_strdupv (info->only_show_in);
  new_info->not_show_in = g_strdupv (info->not_show_in);
  new_info->try_exec = g_strdup (info->try_exec);
  new_info->exec = g_strdup (info->exec);
  new_info->binary = g_strdup (info->binary);
  new_info->path = g_strdup (info->path);
  new_info->hidden = info->hidden;
  new_info->terminal = info->terminal;
  new_info->startup_notify = info->startup_notify;
  
  return G_APP_INFO (new_info);
}

static gboolean
g_desktop_app_info_equal (GAppInfo *appinfo1,
			  GAppInfo *appinfo2)
{
  GDesktopAppInfo *info1 = G_DESKTOP_APP_INFO (appinfo1);
  GDesktopAppInfo *info2 = G_DESKTOP_APP_INFO (appinfo2);

  if (info1->desktop_id == NULL ||
      info2->desktop_id == NULL)
    return info1 == info2;

  return strcmp (info1->desktop_id, info2->desktop_id) == 0;
}

static const char *
g_desktop_app_info_get_id (GAppInfo *appinfo)
{
  GDesktopAppInfo *info = G_DESKTOP_APP_INFO (appinfo);

  return info->desktop_id;
}

static const char *
g_desktop_app_info_get_name (GAppInfo *appinfo)
{
  GDesktopAppInfo *info = G_DESKTOP_APP_INFO (appinfo);

  if (info->name == NULL)
    return _("Unnamed");
  return info->name;
}

static const char *
g_desktop_app_info_get_display_name (GAppInfo *appinfo)
{
  GDesktopAppInfo *info = G_DESKTOP_APP_INFO (appinfo);

  if (info->fullname == NULL)
    return g_desktop_app_info_get_name (appinfo);
  return info->fullname;
}

/**
 * g_desktop_app_info_get_is_hidden:
 * @info: a #GDesktopAppInfo.
 *
 * A desktop file is hidden if the Hidden key in it is
 * set to True.
 *
 * Returns: %TRUE if hidden, %FALSE otherwise. 
 **/
gboolean
g_desktop_app_info_get_is_hidden (GDesktopAppInfo *info)
{
  return info->hidden;
}

/**
 * g_desktop_app_info_get_filename:
 * @info: a #GDesktopAppInfo
 *
 * When @info was created from a known filename, return it.  In some
 * situations such as the #GDesktopAppInfo returned from
 * g_desktop_app_info_new_from_keyfile(), this function will return %NULL.
 *
 * Returns: The full path to the file for @info, or %NULL if not known.
 * Since: 2.24
 */
const char *
g_desktop_app_info_get_filename (GDesktopAppInfo *info)
{
  return info->filename;
}

static const char *
g_desktop_app_info_get_description (GAppInfo *appinfo)
{
  GDesktopAppInfo *info = G_DESKTOP_APP_INFO (appinfo);
  
  return info->comment;
}

static const char *
g_desktop_app_info_get_executable (GAppInfo *appinfo)
{
  GDesktopAppInfo *info = G_DESKTOP_APP_INFO (appinfo);
  
  return info->binary;
}

static const char *
g_desktop_app_info_get_commandline (GAppInfo *appinfo)
{
  GDesktopAppInfo *info = G_DESKTOP_APP_INFO (appinfo);
  
  return info->exec;
}

static GIcon *
g_desktop_app_info_get_icon (GAppInfo *appinfo)
{
  GDesktopAppInfo *info = G_DESKTOP_APP_INFO (appinfo);

  return info->icon;
}

/**
 * g_desktop_app_info_get_categories:
 * @info: a #GDesktopAppInfo
 *
 * Gets the categories from the desktop file.
 *
 * Returns: The unparsed Categories key from the desktop file;
 *     i.e. no attempt is made to split it by ';' or validate it.
 */
const char *
g_desktop_app_info_get_categories (GDesktopAppInfo *info)
{
  return info->categories;
}

/**
 * g_desktop_app_info_get_keywords:
 * @info: a #GDesktopAppInfo
 *
 * Gets the keywords from the desktop file.
 *
 * Returns: (transfer none): The value of the Keywords key
 *
 * Since: 2.32
 */
const char * const *
g_desktop_app_info_get_keywords (GDesktopAppInfo *info)
{
  return (const char * const *)info->keywords;
}

/**
 * g_desktop_app_info_get_generic_name:
 * @info: a #GDesktopAppInfo
 *
 * Gets the generic name from the destkop file.
 *
 * Returns: The value of the GenericName key
 */
const char *
g_desktop_app_info_get_generic_name (GDesktopAppInfo *info)
{
  return info->generic_name;
}

/**
 * g_desktop_app_info_get_nodisplay:
 * @info: a #GDesktopAppInfo
 *
 * Gets the value of the NoDisplay key, which helps determine if the
 * application info should be shown in menus. See
 * #G_KEY_FILE_DESKTOP_KEY_NO_DISPLAY and g_app_info_should_show().
 *
 * Returns: The value of the NoDisplay key
 *
 * Since: 2.30
 */
gboolean
g_desktop_app_info_get_nodisplay (GDesktopAppInfo *info)
{
  return info->nodisplay;
}

/**
 * g_desktop_app_info_get_show_in:
 * @info: a #GDesktopAppInfo
 * @desktop_env: a string specifying a desktop name
 *
 * Checks if the application info should be shown in menus that list available
 * applications for a specific name of the desktop, based on the
 * <literal>OnlyShowIn</literal> and <literal>NotShowIn</literal> keys.
 *
 * If @desktop_env is %NULL, then the name of the desktop set with
 * g_desktop_app_info_set_desktop_env() is used.
 *
 * Note that g_app_info_should_show() for @info will include this check (with
 * %NULL for @desktop_env) as well as additional checks.
 *
 * Returns: %TRUE if the @info should be shown in @desktop_env according to the
 * <literal>OnlyShowIn</literal> and <literal>NotShowIn</literal> keys, %FALSE
 * otherwise.
 *
 * Since: 2.30
 */
gboolean
g_desktop_app_info_get_show_in (GDesktopAppInfo *info,
                                const gchar     *desktop_env)
{
  gboolean found;
  int i;

  g_return_val_if_fail (G_IS_DESKTOP_APP_INFO (info), FALSE);

  if (!desktop_env) {
    G_LOCK (g_desktop_env);
    desktop_env = g_desktop_env;
    G_UNLOCK (g_desktop_env);
  }

  if (info->only_show_in)
    {
      if (desktop_env == NULL)
	return FALSE;

      found = FALSE;
      for (i = 0; info->only_show_in[i] != NULL; i++)
	{
	  if (strcmp (info->only_show_in[i], desktop_env) == 0)
	    {
	      found = TRUE;
	      break;
	    }
	}
      if (!found)
	return FALSE;
    }

  if (info->not_show_in && desktop_env)
    {
      for (i = 0; info->not_show_in[i] != NULL; i++)
	{
	  if (strcmp (info->not_show_in[i], desktop_env) == 0)
	    return FALSE;
	}
    }

  return TRUE;
}

static char *
expand_macro_single (char macro, char *uri)
{
  GFile *file;
  char *result = NULL;
  char *path, *name;

  file = g_file_new_for_uri (uri);
  path = g_file_get_path (file);
  g_object_unref (file);
  
  switch (macro)
    {
    case 'u':
    case 'U':	
      result = g_shell_quote (uri);
      break;
    case 'f':
    case 'F':
      if (path)
	result = g_shell_quote (path);
      break;
    case 'd':
    case 'D':
      if (path)
        {
          name = g_path_get_dirname (path);
	  result = g_shell_quote (name);
          g_free (name);
        }
      break;
    case 'n':
    case 'N':
      if (path)
        {
          name = g_path_get_basename (path);
	  result = g_shell_quote (name);
          g_free (name);
        }
      break;
    }

  g_free (path);
  
  return result;
}

static void
expand_macro (char              macro, 
              GString          *exec, 
              GDesktopAppInfo  *info, 
              GList           **uri_list)
{
  GList *uris = *uri_list;
  char *expanded;
  gboolean force_file_uri;
  char force_file_uri_macro;
  char *uri;

  g_return_if_fail (exec != NULL);

  /* On %u and %U, pass POSIX file path pointing to the URI via
   * the FUSE mount in ~/.gvfs. Note that if the FUSE daemon isn't
   * running or the URI doesn't have a POSIX file path via FUSE
   * we'll just pass the URI.
   */
  force_file_uri_macro = macro;
  force_file_uri = FALSE;
  if (!info->no_fuse)
    {
      switch (macro)
	{
	case 'u':
	  force_file_uri_macro = 'f';
	  force_file_uri = TRUE;
	  break;
	case 'U':
	  force_file_uri_macro = 'F';
	  force_file_uri = TRUE;
	  break;
	default:
	  break;
	}
    }

  switch (macro)
    {
    case 'u':
    case 'f':
    case 'd':
    case 'n':
      if (uris)
	{
	  uri = uris->data;
          if (!force_file_uri ||
	      /* Pass URI if it contains an anchor */
	      strchr (uri, '#') != NULL)
            {
              expanded = expand_macro_single (macro, uri);
            }
          else
            {
              expanded = expand_macro_single (force_file_uri_macro, uri);
              if (expanded == NULL)
                expanded = expand_macro_single (macro, uri);
            }

	  if (expanded)
	    {
	      g_string_append (exec, expanded);
	      g_free (expanded);
	    }
	  uris = uris->next;
	}

      break;

    case 'U':	
    case 'F':
    case 'D':
    case 'N':
      while (uris)
	{
	  uri = uris->data;
	  
          if (!force_file_uri ||
	      /* Pass URI if it contains an anchor */
	      strchr (uri, '#') != NULL)
            {
              expanded = expand_macro_single (macro, uri);
            }
          else
            {
              expanded = expand_macro_single (force_file_uri_macro, uri);
              if (expanded == NULL)
                expanded = expand_macro_single (macro, uri);
            }

	  if (expanded)
	    {
	      g_string_append (exec, expanded);
	      g_free (expanded);
	    }
	  
	  uris = uris->next;
	  
	  if (uris != NULL && expanded)
	    g_string_append_c (exec, ' ');
	}

      break;

    case 'i':
      if (info->icon_name)
	{
	  g_string_append (exec, "--icon ");
          expanded = g_shell_quote (info->icon_name);
	  g_string_append (exec, expanded);
          g_free (expanded);
	}
      break;

    case 'c':
      if (info->name) 
        {
          expanded = g_shell_quote (info->name);
	  g_string_append (exec, expanded);
          g_free (expanded);
        }
      break;

    case 'k':
      if (info->filename) 
        {
          expanded = g_shell_quote (info->filename);
	  g_string_append (exec, expanded);
          g_free (expanded);
        }
      break;

    case 'm': /* deprecated */
      break;

    case '%':
      g_string_append_c (exec, '%');
      break;
    }
  
  *uri_list = uris;
}

static gboolean
expand_application_parameters (GDesktopAppInfo   *info,
			       GList            **uris,
			       int               *argc,
			       char            ***argv,
			       GError           **error)
{
  GList *uri_list = *uris;
  const char *p = info->exec;
  GString *expanded_exec;
  gboolean res;

  if (info->exec == NULL)
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                           _("Desktop file didn't specify Exec field"));
      return FALSE;
    }

  expanded_exec = g_string_new (NULL);

  while (*p)
    {
      if (p[0] == '%' && p[1] != '\0')
	{
	  expand_macro (p[1], expanded_exec, info, uris);
	  p++;
	}
      else
	g_string_append_c (expanded_exec, *p);

      p++;
    }

  /* No file substitutions */
  if (uri_list == *uris && uri_list != NULL)
    {
      /* If there is no macro default to %f. This is also what KDE does */
      g_string_append_c (expanded_exec, ' ');
      expand_macro ('f', expanded_exec, info, uris);
    }

  res = g_shell_parse_argv (expanded_exec->str, argc, argv, error);
  g_string_free (expanded_exec, TRUE);
  return res;
}

static gboolean
prepend_terminal_to_vector (int    *argc,
			    char ***argv)
{
#ifndef G_OS_WIN32
  char **real_argv;
  int real_argc;
  int i, j;
  char **term_argv = NULL;
  int term_argc = 0;
  char *check;
  char **the_argv;
  
  g_return_val_if_fail (argc != NULL, FALSE);
  g_return_val_if_fail (argv != NULL, FALSE);
	
  /* sanity */
  if(*argv == NULL)
    *argc = 0;
  
  the_argv = *argv;

  /* compute size if not given */
  if (*argc < 0)
    {
      for (i = 0; the_argv[i] != NULL; i++)
	;
      *argc = i;
    }
  
  term_argc = 2;
  term_argv = g_new0 (char *, 3);

  check = g_find_program_in_path ("gnome-terminal");
  if (check != NULL)
    {
      term_argv[0] = check;
      /* Note that gnome-terminal takes -x and
       * as -e in gnome-terminal is broken we use that. */
      term_argv[1] = g_strdup ("-x");
    }
  else
    {
      if (check == NULL)
	check = g_find_program_in_path ("nxterm");
      if (check == NULL)
	check = g_find_program_in_path ("color-xterm");
      if (check == NULL)
	check = g_find_program_in_path ("rxvt");
      if (check == NULL)
	check = g_find_program_in_path ("xterm");
      if (check == NULL)
	check = g_find_program_in_path ("dtterm");
      if (check == NULL)
	{
	  check = g_strdup ("xterm");
	  g_warning ("couldn't find a terminal, falling back to xterm");
	}
      term_argv[0] = check;
      term_argv[1] = g_strdup ("-e");
    }

  real_argc = term_argc + *argc;
  real_argv = g_new (char *, real_argc + 1);
  
  for (i = 0; i < term_argc; i++)
    real_argv[i] = term_argv[i];
  
  for (j = 0; j < *argc; j++, i++)
    real_argv[i] = (char *)the_argv[j];
  
  real_argv[i] = NULL;
  
  g_free (*argv);
  *argv = real_argv;
  *argc = real_argc;
  
  /* we use g_free here as we sucked all the inner strings
   * out from it into real_argv */
  g_free (term_argv);
  return TRUE;
#else
  return FALSE;
#endif /* G_OS_WIN32 */
}

static GList *
create_files_for_uris (GList *uris)
{
  GList *res;
  GList *iter;

  res = NULL;

  for (iter = uris; iter; iter = iter->next)
    {
      GFile *file = g_file_new_for_uri ((char *)iter->data);
      res = g_list_prepend (res, file);
    }

  return g_list_reverse (res);
}

typedef struct
{
  GSpawnChildSetupFunc user_setup;
  gpointer user_setup_data;

  char *pid_envvar;
} ChildSetupData;

static void
child_setup (gpointer user_data)
{
  ChildSetupData *data = user_data;

  if (data->pid_envvar)
    {
      pid_t pid = getpid ();
      char buf[20];
      int i;

      /* Write the pid into the space already reserved for it in the
       * environment array. We can't use sprintf because it might
       * malloc, so we do it by hand. It's simplest to write the pid
       * out backwards first, then copy it over.
       */
      for (i = 0; pid; i++, pid /= 10)
        buf[i] = (pid % 10) + '0';
      for (i--; i >= 0; i--)
        *(data->pid_envvar++) = buf[i];
      *data->pid_envvar = '\0';
    }

  if (data->user_setup)
    data->user_setup (data->user_setup_data);
}

static void
notify_desktop_launch (GDBusConnection  *session_bus,
		       GDesktopAppInfo  *info,
		       long              pid,
		       const char       *display,
		       const char       *sn_id,
		       GList            *uris)
{
  GDBusMessage *msg;
  GVariantBuilder uri_variant;
  GVariantBuilder extras_variant;
  GList *iter;
  const char *desktop_file_id;
  const char *gio_desktop_file;

  if (session_bus == NULL)
    return;

  g_variant_builder_init (&uri_variant, G_VARIANT_TYPE ("as"));
  for (iter = uris; iter; iter = iter->next)
    g_variant_builder_add (&uri_variant, "s", iter->data);

  g_variant_builder_init (&extras_variant, G_VARIANT_TYPE ("a{sv}"));
  if (sn_id != NULL && g_utf8_validate (sn_id, -1, NULL))
    g_variant_builder_add (&extras_variant, "{sv}",
			   "startup-id",
			   g_variant_new ("s",
					  sn_id));
  gio_desktop_file = g_getenv ("GIO_LAUNCHED_DESKTOP_FILE");
  if (gio_desktop_file != NULL)
    g_variant_builder_add (&extras_variant, "{sv}",
			   "origin-desktop-file",
			   g_variant_new_bytestring (gio_desktop_file));
  if (g_get_prgname () != NULL)
    g_variant_builder_add (&extras_variant, "{sv}",
			   "origin-prgname",
			   g_variant_new_bytestring (g_get_prgname ()));
  g_variant_builder_add (&extras_variant, "{sv}",
			 "origin-pid",
			 g_variant_new ("x",
					(gint64)getpid ()));

  if (info->filename)
    desktop_file_id = info->filename;
  else if (info->desktop_id)
    desktop_file_id = info->desktop_id;
  else
    desktop_file_id = "";
  
  msg = g_dbus_message_new_signal ("/org/gtk/gio/DesktopAppInfo",
				   "org.gtk.gio.DesktopAppInfo",
				   "Launched");
  g_dbus_message_set_body (msg, g_variant_new ("(@aysxasa{sv})",
					       g_variant_new_bytestring (desktop_file_id),
					       display ? display : "",
					       (gint64)pid,
					       &uri_variant,
					       &extras_variant));
  g_dbus_connection_send_message (session_bus,
				  msg, 0,
				  NULL,
				  NULL);
  g_object_unref (msg);
}

#define _SPAWN_FLAGS_DEFAULT (G_SPAWN_SEARCH_PATH)

static gboolean
_g_desktop_app_info_launch_uris_internal (GAppInfo                   *appinfo,
					  GList                      *uris,
					  GAppLaunchContext          *launch_context,
					  GSpawnFlags                 spawn_flags,
					  GSpawnChildSetupFunc        user_setup,
					  gpointer                    user_setup_data,
					  GDesktopAppLaunchCallback   pid_callback,
					  gpointer                    pid_callback_data,
					  GError                     **error)
{
  GDesktopAppInfo *info = G_DESKTOP_APP_INFO (appinfo);
  GDBusConnection *session_bus;
  gboolean completed = FALSE;
  GList *old_uris;
  char **argv, **envp;
  int argc;
  ChildSetupData data;

  g_return_val_if_fail (appinfo != NULL, FALSE);

  argv = NULL;

  session_bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);

  if (launch_context)
    envp = g_app_launch_context_get_environment (launch_context);
  else
    envp = g_get_environ ();

  do
    {
      GPid pid;
      GList *launched_uris;
      GList *iter;
      char *display, *sn_id;

      old_uris = uris;
      if (!expand_application_parameters (info, &uris,
                                          &argc, &argv, error))
        goto out;

      /* Get the subset of URIs we're launching with this process */
      launched_uris = NULL;
      for (iter = old_uris; iter != NULL && iter != uris; iter = iter->next)
        launched_uris = g_list_prepend (launched_uris, iter->data);
      launched_uris = g_list_reverse (launched_uris);

      if (info->terminal && !prepend_terminal_to_vector (&argc, &argv))
        {
          g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                               _("Unable to find terminal required for application"));
          goto out;
        }

      data.user_setup = user_setup;
      data.user_setup_data = user_setup_data;

      if (info->filename)
        {
          envp = g_environ_setenv (envp,
                                   "GIO_LAUNCHED_DESKTOP_FILE",
                                   info->filename,
                                   TRUE);
          envp = g_environ_setenv (envp,
                                   "GIO_LAUNCHED_DESKTOP_FILE_PID",
                                   "XXXXXXXXXXXXXXXXXXXX", /* filled in child_setup */
                                   TRUE);
          data.pid_envvar = (char *)g_environ_getenv (envp, "GIO_LAUNCHED_DESKTOP_FILE_PID");
        }
      else
        {
          data.pid_envvar = NULL;
        }

      display = NULL;
      sn_id = NULL;
      if (launch_context)
        {
          GList *launched_files = create_files_for_uris (launched_uris);

          display = g_app_launch_context_get_display (launch_context,
                                                      appinfo,
                                                      launched_files);
          if (display)
            envp = g_environ_setenv (envp, "DISPLAY", display, TRUE);

          if (info->startup_notify)
            {
              sn_id = g_app_launch_context_get_startup_notify_id (launch_context,
                                                                  appinfo,
                                                                  launched_files);
              envp = g_environ_setenv (envp, "DESKTOP_STARTUP_ID", sn_id, TRUE);
            }

          g_list_free_full (launched_files, g_object_unref);
        }

      if (!g_spawn_async (info->path,
                          argv,
                          envp,
                          spawn_flags,
                          child_setup,
                          &data,
                          &pid,
                          error))
        {
	  if (sn_id)
	    g_app_launch_context_launch_failed (launch_context, sn_id);

	  g_free (display);
	  g_free (sn_id);
	  g_list_free (launched_uris);

	  goto out;
	}

      if (pid_callback != NULL)
	pid_callback (info, pid, pid_callback_data);

      notify_desktop_launch (session_bus,
			     info,
			     pid,
			     display,
			     sn_id,
			     launched_uris);

      g_free (display);
      g_free (sn_id);
      g_list_free (launched_uris);

      g_strfreev (argv);
      argv = NULL;
    }
  while (uris != NULL);

  /* TODO - need to handle the process exiting immediately
   * after launching an app.  See http://bugzilla.gnome.org/606960
   */
  if (session_bus != NULL)
    {
      /* This asynchronous flush holds a reference until it completes,
       * which ensures that the following unref won't immediately kill
       * the connection if we were the initial owner.
       */
      g_dbus_connection_flush (session_bus, NULL, NULL, NULL);
      g_object_unref (session_bus);
    }

  completed = TRUE;

 out:
  g_strfreev (argv);
  g_strfreev (envp);

  return completed;
}

static gboolean
g_desktop_app_info_launch_uris (GAppInfo           *appinfo,
				GList              *uris,
				GAppLaunchContext  *launch_context,
				GError            **error)
{
  return _g_desktop_app_info_launch_uris_internal (appinfo, uris,
						   launch_context,
						   _SPAWN_FLAGS_DEFAULT,
						   NULL, NULL, NULL, NULL,
						   error);
}

static gboolean
g_desktop_app_info_supports_uris (GAppInfo *appinfo)
{
  GDesktopAppInfo *info = G_DESKTOP_APP_INFO (appinfo);
 
  return info->exec && 
    ((strstr (info->exec, "%u") != NULL) ||
     (strstr (info->exec, "%U") != NULL));
}

static gboolean
g_desktop_app_info_supports_files (GAppInfo *appinfo)
{
  GDesktopAppInfo *info = G_DESKTOP_APP_INFO (appinfo);
 
  return info->exec && 
    ((strstr (info->exec, "%f") != NULL) ||
     (strstr (info->exec, "%F") != NULL));
}

static gboolean
g_desktop_app_info_launch (GAppInfo           *appinfo,
			   GList              *files,
			   GAppLaunchContext  *launch_context,
			   GError            **error)
{
  GList *uris;
  char *uri;
  gboolean res;

  uris = NULL;
  while (files)
    {
      uri = g_file_get_uri (files->data);
      uris = g_list_prepend (uris, uri);
      files = files->next;
    }
  
  uris = g_list_reverse (uris);
  
  res = g_desktop_app_info_launch_uris (appinfo, uris, launch_context, error);
  
  g_list_free_full (uris, g_free);
  
  return res;
}

/**
 * g_desktop_app_info_launch_uris_as_manager:
 * @appinfo: a #GDesktopAppInfo
 * @uris: (element-type utf8): List of URIs
 * @launch_context: a #GAppLaunchContext
 * @spawn_flags: #GSpawnFlags, used for each process
 * @user_setup: (scope call): a #GSpawnChildSetupFunc, used once for
 *     each process.
 * @user_setup_data: (closure user_setup): User data for @user_setup
 * @pid_callback: (scope call): Callback for child processes
 * @pid_callback_data: (closure pid_callback): User data for @callback
 * @error: return location for a #GError, or %NULL
 *
 * This function performs the equivalent of g_app_info_launch_uris(),
 * but is intended primarily for operating system components that
 * launch applications.  Ordinary applications should use
 * g_app_info_launch_uris().
 *
 * In contrast to g_app_info_launch_uris(), all processes created will
 * always be run directly as children as if by the UNIX fork()/exec()
 * calls.
 *
 * This guarantee allows additional control over the exact environment
 * of the child processes, which is provided via a setup function
 * @user_setup, as well as the process identifier of each child process
 * via @pid_callback. See g_spawn_async() for more information about the
 * semantics of the @user_setup function.
 *
 * Returns: %TRUE on successful launch, %FALSE otherwise.
 */
gboolean
g_desktop_app_info_launch_uris_as_manager (GDesktopAppInfo            *appinfo,
					   GList                      *uris,
					   GAppLaunchContext          *launch_context,
					   GSpawnFlags                 spawn_flags,
					   GSpawnChildSetupFunc        user_setup,
					   gpointer                    user_setup_data,
					   GDesktopAppLaunchCallback   pid_callback,
					   gpointer                    pid_callback_data,
					   GError                    **error)
{
  return _g_desktop_app_info_launch_uris_internal ((GAppInfo*)appinfo,
						   uris,
						   launch_context,
						   spawn_flags,
						   user_setup,
						   user_setup_data,
						   pid_callback,
						   pid_callback_data,
						   error);
}

/**
 * g_desktop_app_info_set_desktop_env:
 * @desktop_env: a string specifying what desktop this is
 *
 * Sets the name of the desktop that the application is running in.
 * This is used by g_app_info_should_show() and
 * g_desktop_app_info_get_show_in() to evaluate the
 * <literal>OnlyShowIn</literal> and <literal>NotShowIn</literal>
 * desktop entry fields.
 *
 * The <ulink url="http://standards.freedesktop.org/menu-spec/latest/">Desktop
 * Menu specification</ulink> recognizes the following:
 * <simplelist>
 *   <member>GNOME</member>
 *   <member>KDE</member>
 *   <member>ROX</member>
 *   <member>XFCE</member>
 *   <member>LXDE</member>
 *   <member>Unity</member>
 *   <member>Old</member>
 * </simplelist>
 *
 * Should be called only once; subsequent calls are ignored.
 */
void
g_desktop_app_info_set_desktop_env (const gchar *desktop_env)
{
  G_LOCK (g_desktop_env);
  if (!g_desktop_env)
    g_desktop_env = g_strdup (desktop_env);
  G_UNLOCK (g_desktop_env);
}

static gboolean
g_desktop_app_info_should_show (GAppInfo *appinfo)
{
  GDesktopAppInfo *info = G_DESKTOP_APP_INFO (appinfo);

  if (info->nodisplay)
    return FALSE;

  return g_desktop_app_info_get_show_in (info, NULL);
}

typedef enum {
  APP_DIR,
  MIMETYPE_DIR
} DirType;

static char *
ensure_dir (DirType   type,
            GError  **error)
{
  char *path, *display_name;
  int errsv;

  if (type == APP_DIR)
    path = g_build_filename (g_get_user_data_dir (), "applications", NULL);
  else
    path = g_build_filename (g_get_user_data_dir (), "mime", "packages", NULL);

  errno = 0;
  if (g_mkdir_with_parents (path, 0700) == 0)
    return path;

  errsv = errno;
  display_name = g_filename_display_name (path);
  if (type == APP_DIR)
    g_set_error (error, G_IO_ERROR, g_io_error_from_errno (errsv),
                 _("Can't create user application configuration folder %s: %s"),
                 display_name, g_strerror (errsv));
  else
    g_set_error (error, G_IO_ERROR, g_io_error_from_errno (errsv),
                 _("Can't create user MIME configuration folder %s: %s"),
                 display_name, g_strerror (errsv));

  g_free (display_name);
  g_free (path);

  return NULL;
}

static gboolean
update_mimeapps_list (const char  *desktop_id, 
		      const char  *content_type,
                      UpdateMimeFlags flags,
		      GError     **error)
{
  char *dirname, *filename, *string;
  GKeyFile *key_file;
  gboolean load_succeeded, res;
  char **old_list, **list;
  gsize length, data_size;
  char *data;
  int i, j, k;
  char **content_types;

  /* Don't add both at start and end */
  g_assert (!((flags & UPDATE_MIME_SET_DEFAULT) &&
              (flags & UPDATE_MIME_SET_NON_DEFAULT)));

  dirname = ensure_dir (APP_DIR, error);
  if (!dirname)
    return FALSE;

  filename = g_build_filename (dirname, "mimeapps.list", NULL);
  g_free (dirname);

  key_file = g_key_file_new ();
  load_succeeded = g_key_file_load_from_file (key_file, filename, G_KEY_FILE_NONE, NULL);
  if (!load_succeeded ||
      (!g_key_file_has_group (key_file, ADDED_ASSOCIATIONS_GROUP) &&
       !g_key_file_has_group (key_file, REMOVED_ASSOCIATIONS_GROUP) &&
       !g_key_file_has_group (key_file, DEFAULT_APPLICATIONS_GROUP)))
    {
      g_key_file_free (key_file);
      key_file = g_key_file_new ();
    }

  if (content_type)
    {
      content_types = g_new (char *, 2);
      content_types[0] = g_strdup (content_type);
      content_types[1] = NULL;
    }
  else
    {
      content_types = g_key_file_get_keys (key_file, DEFAULT_APPLICATIONS_GROUP, NULL, NULL);
    }

  for (k = 0; content_types && content_types[k]; k++)
    {
      /* set as default, if requested so */
      string = g_key_file_get_string (key_file,
                                      DEFAULT_APPLICATIONS_GROUP,
                                      content_types[k],
                                      NULL);

      if (g_strcmp0 (string, desktop_id) != 0 &&
          (flags & UPDATE_MIME_SET_DEFAULT))
        {
          g_free (string);
          string = g_strdup (desktop_id);

          /* add in the non-default list too, if it's not already there */
          flags |= UPDATE_MIME_SET_NON_DEFAULT;
        }

      if (string == NULL || desktop_id == NULL)
        g_key_file_remove_key (key_file,
                               DEFAULT_APPLICATIONS_GROUP,
                               content_types[k],
                               NULL);
      else
        g_key_file_set_string (key_file,
                               DEFAULT_APPLICATIONS_GROUP,
                               content_types[k],
                               string);

      g_free (string);
    }

  if (content_type)
    {
      /* reuse the list from above */
    }
  else
    {
      g_strfreev (content_types);
      content_types = g_key_file_get_keys (key_file, ADDED_ASSOCIATIONS_GROUP, NULL, NULL);
    }
  
  for (k = 0; content_types && content_types[k]; k++)
    { 
      /* Add to the right place in the list */
  
      length = 0;
      old_list = g_key_file_get_string_list (key_file, ADDED_ASSOCIATIONS_GROUP,
					     content_types[k], &length, NULL);

      list = g_new (char *, 1 + length + 1);

      i = 0;

      /* if we're adding a last-used hint, just put the application in front of the list */
      if (flags & UPDATE_MIME_SET_LAST_USED)
        {
          /* avoid adding this again as non-default later */
          if (flags & UPDATE_MIME_SET_NON_DEFAULT)
            flags ^= UPDATE_MIME_SET_NON_DEFAULT;

          list[i++] = g_strdup (desktop_id);
        }

      if (old_list)
        {
          for (j = 0; old_list[j] != NULL; j++)
	    {
	      if (g_strcmp0 (old_list[j], desktop_id) != 0)
                {
                  /* rewrite other entries if they're different from the new one */
                  list[i++] = g_strdup (old_list[j]);
                }
	      else if (flags & UPDATE_MIME_SET_NON_DEFAULT)
		{
                  /* we encountered an old entry which is equal to the one we're adding as non-default,
                   * don't change its position in the list.
                   */
		  flags ^= UPDATE_MIME_SET_NON_DEFAULT;
		  list[i++] = g_strdup (old_list[j]);
		}
	    }
        }

      /* add it at the end of the list */
      if (flags & UPDATE_MIME_SET_NON_DEFAULT)
        list[i++] = g_strdup (desktop_id);

      list[i] = NULL;
  
      g_strfreev (old_list);

      if (list[0] == NULL || desktop_id == NULL)
        g_key_file_remove_key (key_file,
			       ADDED_ASSOCIATIONS_GROUP,
			       content_types[k],
			       NULL);
      else
        g_key_file_set_string_list (key_file,
                                    ADDED_ASSOCIATIONS_GROUP,
                                    content_types[k],
                                    (const char * const *)list, i);

      g_strfreev (list);
    }
  
  if (content_type)
    {
      /* reuse the list from above */
    }
  else
    {
      g_strfreev (content_types);
      content_types = g_key_file_get_keys (key_file, REMOVED_ASSOCIATIONS_GROUP, NULL, NULL);
    }

  for (k = 0; content_types && content_types[k]; k++) 
    {
      /* Remove from removed associations group (unless remove) */
  
      length = 0;
      old_list = g_key_file_get_string_list (key_file, REMOVED_ASSOCIATIONS_GROUP,
					     content_types[k], &length, NULL);

      list = g_new (char *, 1 + length + 1);

      i = 0;
      if (flags & UPDATE_MIME_REMOVE)
        list[i++] = g_strdup (desktop_id);
      if (old_list)
        {
          for (j = 0; old_list[j] != NULL; j++)
	    {
	      if (g_strcmp0 (old_list[j], desktop_id) != 0)
	        list[i++] = g_strdup (old_list[j]);
	    }
        }
      list[i] = NULL;
  
      g_strfreev (old_list);

      if (list[0] == NULL || desktop_id == NULL)
        g_key_file_remove_key (key_file,
			       REMOVED_ASSOCIATIONS_GROUP,
			       content_types[k],
			       NULL);
      else
        g_key_file_set_string_list (key_file,
				    REMOVED_ASSOCIATIONS_GROUP,
				    content_types[k],
				    (const char * const *)list, i);

      g_strfreev (list);
    }

  g_strfreev (content_types);

  data = g_key_file_to_data (key_file, &data_size, error);
  g_key_file_free (key_file);

  res = g_file_set_contents (filename, data, data_size, error);

  mime_info_cache_reload (NULL);

  g_free (filename);
  g_free (data);

  return res;
}

static gboolean
g_desktop_app_info_set_as_last_used_for_type (GAppInfo    *appinfo,
                                              const char  *content_type,
                                              GError     **error)
{
  GDesktopAppInfo *info = G_DESKTOP_APP_INFO (appinfo);

  if (!g_desktop_app_info_ensure_saved (info, error))
    return FALSE;

  if (!info->desktop_id)
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                           _("Application information lacks an identifier"));
      return FALSE;
    }

  /* both add support for the content type and set as last used */
  return update_mimeapps_list (info->desktop_id, content_type,
                               UPDATE_MIME_SET_NON_DEFAULT |
                               UPDATE_MIME_SET_LAST_USED,
                               error);
}

static gboolean
g_desktop_app_info_set_as_default_for_type (GAppInfo    *appinfo,
					    const char  *content_type,
					    GError     **error)
{
  GDesktopAppInfo *info = G_DESKTOP_APP_INFO (appinfo);

  if (!g_desktop_app_info_ensure_saved (info, error))
    return FALSE;

  if (!info->desktop_id)
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                           _("Application information lacks an identifier"));
      return FALSE;
    }

  return update_mimeapps_list (info->desktop_id, content_type,
                               UPDATE_MIME_SET_DEFAULT,
                               error);
}

static void
update_program_done (GPid     pid,
		     gint     status,
		     gpointer data)
{
  /* Did the application exit correctly */
  if (WIFEXITED (status) &&
      WEXITSTATUS (status) == 0)
    {
      /* Here we could clean out any caches in use */
    }
}

static void
run_update_command (char *command,
		    char *subdir)
{
	char *argv[3] = {
		NULL,
		NULL,
		NULL,
	};
	GPid pid = 0;
	GError *error = NULL;

	argv[0] = command;
	argv[1] = g_build_filename (g_get_user_data_dir (), subdir, NULL);

	if (g_spawn_async ("/", argv,
			   NULL,       /* envp */
			   G_SPAWN_SEARCH_PATH |
			   G_SPAWN_STDOUT_TO_DEV_NULL |
			   G_SPAWN_STDERR_TO_DEV_NULL |
			   G_SPAWN_DO_NOT_REAP_CHILD,
			   NULL, NULL, /* No setup function */
			   &pid,
			   &error)) 
	  g_child_watch_add (pid, update_program_done, NULL);
	else
	  {
	    /* If we get an error at this point, it's quite likely the user doesn't
	     * have an installed copy of either 'update-mime-database' or
	     * 'update-desktop-database'.  I don't think we want to popup an error
	     * dialog at this point, so we just do a g_warning to give the user a
	     * chance of debugging it.
	     */
	    g_warning ("%s", error->message);
	  }
	
	g_free (argv[1]);
}

static gboolean
g_desktop_app_info_set_as_default_for_extension (GAppInfo    *appinfo,
						 const char  *extension,
						 GError     **error)
{
  char *filename, *basename, *mimetype;
  char *dirname;
  gboolean res;

  if (!g_desktop_app_info_ensure_saved (G_DESKTOP_APP_INFO (appinfo), error))
    return FALSE;  
  
  dirname = ensure_dir (MIMETYPE_DIR, error);
  if (!dirname)
    return FALSE;
  
  basename = g_strdup_printf ("user-extension-%s.xml", extension);
  filename = g_build_filename (dirname, basename, NULL);
  g_free (basename);
  g_free (dirname);

  mimetype = g_strdup_printf ("application/x-extension-%s", extension);
  
  if (!g_file_test (filename, G_FILE_TEST_EXISTS)) 
    {
      char *contents;

      contents =
        g_strdup_printf ("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                         "<mime-info xmlns=\"http://www.freedesktop.org/standards/shared-mime-info\">\n"
                         " <mime-type type=\"%s\">\n"
                         "  <comment>%s document</comment>\n"
                         "  <glob pattern=\"*.%s\"/>\n"
                         " </mime-type>\n"
                         "</mime-info>\n", mimetype, extension, extension);

      g_file_set_contents (filename, contents, -1, NULL);
      g_free (contents);

      run_update_command ("update-mime-database", "mime");
    }
  g_free (filename);
  
  res = g_desktop_app_info_set_as_default_for_type (appinfo,
						    mimetype,
						    error);

  g_free (mimetype);
  
  return res;
}

static gboolean
g_desktop_app_info_add_supports_type (GAppInfo    *appinfo,
				      const char  *content_type,
				      GError     **error)
{
  GDesktopAppInfo *info = G_DESKTOP_APP_INFO (appinfo);

  if (!g_desktop_app_info_ensure_saved (G_DESKTOP_APP_INFO (info), error))
    return FALSE;  
  
  return update_mimeapps_list (info->desktop_id, content_type,
                               UPDATE_MIME_SET_NON_DEFAULT,
                               error);
}

static gboolean
g_desktop_app_info_can_remove_supports_type (GAppInfo *appinfo)
{
  return TRUE;
}

static gboolean
g_desktop_app_info_remove_supports_type (GAppInfo    *appinfo,
					 const char  *content_type,
					 GError     **error)
{
  GDesktopAppInfo *info = G_DESKTOP_APP_INFO (appinfo);

  if (!g_desktop_app_info_ensure_saved (G_DESKTOP_APP_INFO (info), error))
    return FALSE;
  
  return update_mimeapps_list (info->desktop_id, content_type,
                               UPDATE_MIME_REMOVE,
                               error);
}

static gboolean
g_desktop_app_info_ensure_saved (GDesktopAppInfo  *info,
				 GError          **error)
{
  GKeyFile *key_file;
  char *dirname;
  char *filename;
  char *data, *desktop_id;
  gsize data_size;
  int fd;
  gboolean res;
  
  if (info->filename != NULL)
    return TRUE;

  /* This is only used for object created with
   * g_app_info_create_from_commandline. All other
   * object should have a filename
   */
  
  dirname = ensure_dir (APP_DIR, error);
  if (!dirname)
    return FALSE;
  
  key_file = g_key_file_new ();

  g_key_file_set_string (key_file, G_KEY_FILE_DESKTOP_GROUP,
			 "Encoding", "UTF-8");
  g_key_file_set_string (key_file, G_KEY_FILE_DESKTOP_GROUP,
			 G_KEY_FILE_DESKTOP_KEY_VERSION, "1.0");
  g_key_file_set_string (key_file, G_KEY_FILE_DESKTOP_GROUP,
			 G_KEY_FILE_DESKTOP_KEY_TYPE,
                         G_KEY_FILE_DESKTOP_TYPE_APPLICATION);
  if (info->terminal) 
    g_key_file_set_boolean (key_file, G_KEY_FILE_DESKTOP_GROUP,
			    G_KEY_FILE_DESKTOP_KEY_TERMINAL, TRUE);
  if (info->nodisplay)
    g_key_file_set_boolean (key_file, G_KEY_FILE_DESKTOP_GROUP,
			    G_KEY_FILE_DESKTOP_KEY_NO_DISPLAY, TRUE);

  g_key_file_set_string (key_file, G_KEY_FILE_DESKTOP_GROUP,
			 G_KEY_FILE_DESKTOP_KEY_EXEC, info->exec);

  g_key_file_set_string (key_file, G_KEY_FILE_DESKTOP_GROUP,
			 G_KEY_FILE_DESKTOP_KEY_NAME, info->name);

  if (info->generic_name != NULL)
    g_key_file_set_string (key_file, G_KEY_FILE_DESKTOP_GROUP,
			   GENERIC_NAME_KEY, info->generic_name);

  if (info->fullname != NULL)
    g_key_file_set_string (key_file, G_KEY_FILE_DESKTOP_GROUP,
			   FULL_NAME_KEY, info->fullname);

  g_key_file_set_string (key_file, G_KEY_FILE_DESKTOP_GROUP,
			 G_KEY_FILE_DESKTOP_KEY_COMMENT, info->comment);
  
  g_key_file_set_boolean (key_file, G_KEY_FILE_DESKTOP_GROUP,
			  G_KEY_FILE_DESKTOP_KEY_NO_DISPLAY, TRUE);

  data = g_key_file_to_data (key_file, &data_size, NULL);
  g_key_file_free (key_file);

  desktop_id = g_strdup_printf ("userapp-%s-XXXXXX.desktop", info->name);
  filename = g_build_filename (dirname, desktop_id, NULL);
  g_free (desktop_id);
  g_free (dirname);
  
  fd = g_mkstemp (filename);
  if (fd == -1)
    {
      char *display_name;

      display_name = g_filename_display_name (filename);
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
		   _("Can't create user desktop file %s"), display_name);
      g_free (display_name);
      g_free (filename);
      g_free (data);
      return FALSE;
    }

  desktop_id = g_path_get_basename (filename);

  close (fd);
  
  res = g_file_set_contents (filename, data, data_size, error);
  if (!res)
    {
      g_free (desktop_id);
      g_free (filename);
      return FALSE;
    }

  info->filename = filename;
  info->desktop_id = desktop_id;
  
  run_update_command ("update-desktop-database", "applications");
  
  return TRUE;
}

static gboolean
g_desktop_app_info_can_delete (GAppInfo *appinfo)
{
  GDesktopAppInfo *info = G_DESKTOP_APP_INFO (appinfo);

  if (info->filename)
    {
      if (strstr (info->filename, "/userapp-"))
        return g_access (info->filename, W_OK) == 0;
    }

  return FALSE;
}

static gboolean
g_desktop_app_info_delete (GAppInfo *appinfo)
{
  GDesktopAppInfo *info = G_DESKTOP_APP_INFO (appinfo);
  
  if (info->filename)
    { 
      if (g_remove (info->filename) == 0)
        {
          update_mimeapps_list (info->desktop_id, NULL,
                                UPDATE_MIME_NONE,
                                NULL);

          g_free (info->filename);
          info->filename = NULL;
          g_free (info->desktop_id);
          info->desktop_id = NULL;

          return TRUE;
        }
    }

  return FALSE;
}

/**
 * g_app_info_create_from_commandline:
 * @commandline: the commandline to use
 * @application_name: (allow-none): the application name, or %NULL to use @commandline
 * @flags: flags that can specify details of the created #GAppInfo
 * @error: a #GError location to store the error occurring, %NULL to ignore.
 *
 * Creates a new #GAppInfo from the given information.
 *
 * Note that for @commandline, the quoting rules of the Exec key of the
 * <ulink url="http://freedesktop.org/Standards/desktop-entry-spec">freedesktop.org Desktop
 * Entry Specification</ulink> are applied. For example, if the @commandline contains
 * percent-encoded URIs, the percent-character must be doubled in order to prevent it from
 * being swallowed by Exec key unquoting. See the specification for exact quoting rules.
 *
 * Returns: (transfer full): new #GAppInfo for given command.
 **/
GAppInfo *
g_app_info_create_from_commandline (const char           *commandline,
				    const char           *application_name,
				    GAppInfoCreateFlags   flags,
				    GError              **error)
{
  char **split;
  char *basename;
  GDesktopAppInfo *info;

  g_return_val_if_fail (commandline, NULL);

  info = g_object_new (G_TYPE_DESKTOP_APP_INFO, NULL);

  info->filename = NULL;
  info->desktop_id = NULL;
  
  info->terminal = (flags & G_APP_INFO_CREATE_NEEDS_TERMINAL) != 0;
  info->startup_notify = (flags & G_APP_INFO_CREATE_SUPPORTS_STARTUP_NOTIFICATION) != 0;
  info->hidden = FALSE;
  if ((flags & G_APP_INFO_CREATE_SUPPORTS_URIS) != 0)
    info->exec = g_strconcat (commandline, " %u", NULL);
  else
    info->exec = g_strconcat (commandline, " %f", NULL);
  info->nodisplay = TRUE;
  info->binary = binary_from_exec (info->exec);
  
  if (application_name)
    info->name = g_strdup (application_name);
  else
    {
      /* FIXME: this should be more robust. Maybe g_shell_parse_argv and use argv[0] */
      split = g_strsplit (commandline, " ", 2);
      basename = split[0] ? g_path_get_basename (split[0]) : NULL;
      g_strfreev (split);
      info->name = basename;
      if (info->name == NULL)
	info->name = g_strdup ("custom");
    }
  info->comment = g_strdup_printf (_("Custom definition for %s"), info->name);
  
  return G_APP_INFO (info);
}

static void
g_desktop_app_info_iface_init (GAppInfoIface *iface)
{
  iface->dup = g_desktop_app_info_dup;
  iface->equal = g_desktop_app_info_equal;
  iface->get_id = g_desktop_app_info_get_id;
  iface->get_name = g_desktop_app_info_get_name;
  iface->get_description = g_desktop_app_info_get_description;
  iface->get_executable = g_desktop_app_info_get_executable;
  iface->get_icon = g_desktop_app_info_get_icon;
  iface->launch = g_desktop_app_info_launch;
  iface->supports_uris = g_desktop_app_info_supports_uris;
  iface->supports_files = g_desktop_app_info_supports_files;
  iface->launch_uris = g_desktop_app_info_launch_uris;
  iface->should_show = g_desktop_app_info_should_show;
  iface->set_as_default_for_type = g_desktop_app_info_set_as_default_for_type;
  iface->set_as_default_for_extension = g_desktop_app_info_set_as_default_for_extension;
  iface->add_supports_type = g_desktop_app_info_add_supports_type;
  iface->can_remove_supports_type = g_desktop_app_info_can_remove_supports_type;
  iface->remove_supports_type = g_desktop_app_info_remove_supports_type;
  iface->can_delete = g_desktop_app_info_can_delete;
  iface->do_delete = g_desktop_app_info_delete;
  iface->get_commandline = g_desktop_app_info_get_commandline;
  iface->get_display_name = g_desktop_app_info_get_display_name;
  iface->set_as_last_used_for_type = g_desktop_app_info_set_as_last_used_for_type;
}

static gboolean
app_info_in_list (GAppInfo *info, 
                  GList    *list)
{
  while (list != NULL)
    {
      if (g_app_info_equal (info, list->data))
	return TRUE;
      list = list->next;
    }
  return FALSE;
}

/**
 * g_app_info_get_recommended_for_type:
 * @content_type: the content type to find a #GAppInfo for
 * 
 * Gets a list of recommended #GAppInfos for a given content type, i.e.
 * those applications which claim to support the given content type exactly,
 * and not by MIME type subclassing.
 * Note that the first application of the list is the last used one, i.e.
 * the last one for which g_app_info_set_as_last_used_for_type() has been
 * called.
 *
 * Returns: (element-type GAppInfo) (transfer full): #GList of #GAppInfos
 *     for given @content_type or %NULL on error.
 *
 * Since: 2.28
 **/
GList *
g_app_info_get_recommended_for_type (const gchar *content_type)
{
  GList *desktop_entries, *l;
  GList *infos;
  GDesktopAppInfo *info;

  g_return_val_if_fail (content_type != NULL, NULL);

  desktop_entries = get_all_desktop_entries_for_mime_type (content_type, NULL, FALSE, NULL);

  infos = NULL;
  for (l = desktop_entries; l != NULL; l = l->next)
    {
      char *desktop_entry = l->data;

      info = g_desktop_app_info_new (desktop_entry);
      if (info)
	{
	  if (app_info_in_list (G_APP_INFO (info), infos))
	    g_object_unref (info);
	  else
	    infos = g_list_prepend (infos, info);
	}
      g_free (desktop_entry);
    }

  g_list_free (desktop_entries);

  return g_list_reverse (infos);
}

/**
 * g_app_info_get_fallback_for_type:
 * @content_type: the content type to find a #GAppInfo for
 * 
 * Gets a list of fallback #GAppInfos for a given content type, i.e.
 * those applications which claim to support the given content type
 * by MIME type subclassing and not directly.
 *
 * Returns: (element-type GAppInfo) (transfer full): #GList of #GAppInfos
 *     for given @content_type or %NULL on error.
 *
 * Since: 2.28
 **/
GList *
g_app_info_get_fallback_for_type (const gchar *content_type)
{
  GList *desktop_entries, *l;
  GList *infos, *recommended_infos;
  GDesktopAppInfo *info;

  g_return_val_if_fail (content_type != NULL, NULL);

  desktop_entries = get_all_desktop_entries_for_mime_type (content_type, NULL, TRUE, NULL);
  recommended_infos = g_app_info_get_recommended_for_type (content_type);

  infos = NULL;
  for (l = desktop_entries; l != NULL; l = l->next)
    {
      char *desktop_entry = l->data;

      info = g_desktop_app_info_new (desktop_entry);
      if (info)
	{
	  if (app_info_in_list (G_APP_INFO (info), infos) ||
	      app_info_in_list (G_APP_INFO (info), recommended_infos))
	    g_object_unref (info);
	  else
	    infos = g_list_prepend (infos, info);
	}
      g_free (desktop_entry);
    }

  g_list_free (desktop_entries);
  g_list_free_full (recommended_infos, g_object_unref);

  return g_list_reverse (infos);
}

/**
 * g_app_info_get_all_for_type:
 * @content_type: the content type to find a #GAppInfo for
 *
 * Gets a list of all #GAppInfos for a given content type,
 * including the recommended and fallback #GAppInfos. See
 * g_app_info_get_recommended_for_type() and
 * g_app_info_get_fallback_for_type().
 *
 * Returns: (element-type GAppInfo) (transfer full): #GList of #GAppInfos
 *     for given @content_type or %NULL on error.
 **/
GList *
g_app_info_get_all_for_type (const char *content_type)
{
  GList *desktop_entries, *l;
  GList *infos;
  char *user_default = NULL;
  GDesktopAppInfo *info;

  g_return_val_if_fail (content_type != NULL, NULL);
  
  desktop_entries = get_all_desktop_entries_for_mime_type (content_type, NULL, TRUE, &user_default);
  infos = NULL;

  /* put the user default in front of the list, for compatibility */
  if (user_default != NULL)
    {
      info = g_desktop_app_info_new (user_default);

      if (info != NULL)
        infos = g_list_prepend (infos, info);
    }

  g_free (user_default);

  for (l = desktop_entries; l != NULL; l = l->next)
    {
      char *desktop_entry = l->data;

      info = g_desktop_app_info_new (desktop_entry);
      if (info)
	{
	  if (app_info_in_list (G_APP_INFO (info), infos))
	    g_object_unref (info);
	  else
	    infos = g_list_prepend (infos, info);
	}
      g_free (desktop_entry);
    }

  g_list_free (desktop_entries);
  
  return g_list_reverse (infos);
}

/**
 * g_app_info_reset_type_associations:
 * @content_type: a content type
 *
 * Removes all changes to the type associations done by
 * g_app_info_set_as_default_for_type(),
 * g_app_info_set_as_default_for_extension(),
 * g_app_info_add_supports_type() or
 * g_app_info_remove_supports_type().
 *
 * Since: 2.20
 */
void
g_app_info_reset_type_associations (const char *content_type)
{
  update_mimeapps_list (NULL, content_type,
                        UPDATE_MIME_NONE,
                        NULL);
}

/**
 * g_app_info_get_default_for_type:
 * @content_type: the content type to find a #GAppInfo for
 * @must_support_uris: if %TRUE, the #GAppInfo is expected to
 *     support URIs
 *
 * Gets the default #GAppInfo for a given content type.
 *
 * Returns: (transfer full): #GAppInfo for given @content_type or
 *     %NULL on error.
 */
GAppInfo *
g_app_info_get_default_for_type (const char *content_type,
				 gboolean    must_support_uris)
{
  GList *desktop_entries, *l;
  char *user_default = NULL;
  GAppInfo *info;

  g_return_val_if_fail (content_type != NULL, NULL);
  
  desktop_entries = get_all_desktop_entries_for_mime_type (content_type, NULL, TRUE, &user_default);

  info = NULL;

  if (user_default != NULL)
    {
      info = (GAppInfo *) g_desktop_app_info_new (user_default);

      if (info)
        {
	  if (must_support_uris && !g_app_info_supports_uris (info))
	    {
	      g_object_unref (info);
	      info = NULL;
	    }
        }
    }

  g_free (user_default);

  if (info != NULL)
    {
      g_list_free_full (desktop_entries, g_free);
      return info;
    }

  /* pick the first from the other list that matches our URI
   * requirements.
   */
  for (l = desktop_entries; l != NULL; l = l->next)
    {
      char *desktop_entry = l->data;

      info = (GAppInfo *)g_desktop_app_info_new (desktop_entry);
      if (info)
	{
	  if (must_support_uris && !g_app_info_supports_uris (info))
	    {
	      g_object_unref (info);
	      info = NULL;
	    }
	  else
	    break;
	}
    }
  
  g_list_free_full (desktop_entries, g_free);

  return info;
}

/**
 * g_app_info_get_default_for_uri_scheme:
 * @uri_scheme: a string containing a URI scheme.
 *
 * Gets the default application for handling URIs with
 * the given URI scheme. A URI scheme is the initial part
 * of the URI, up to but not including the ':', e.g. "http",
 * "ftp" or "sip".
 *
 * Returns: (transfer full): #GAppInfo for given @uri_scheme or %NULL on error.
 */
GAppInfo *
g_app_info_get_default_for_uri_scheme (const char *uri_scheme)
{
  GAppInfo *app_info;
  char *content_type, *scheme_down;

  scheme_down = g_ascii_strdown (uri_scheme, -1);
  content_type = g_strdup_printf ("x-scheme-handler/%s", scheme_down);
  g_free (scheme_down);
  app_info = g_app_info_get_default_for_type (content_type, FALSE);
  g_free (content_type);

  return app_info;
}

static void
get_apps_from_dir (GHashTable *apps, 
                   const char *dirname, 
                   const char *prefix)
{
  GDir *dir;
  const char *basename;
  char *filename, *subprefix, *desktop_id;
  gboolean hidden;
  GDesktopAppInfo *appinfo;
  
  dir = g_dir_open (dirname, 0, NULL);
  if (dir)
    {
      while ((basename = g_dir_read_name (dir)) != NULL)
	{
	  filename = g_build_filename (dirname, basename, NULL);
	  if (g_str_has_suffix (basename, ".desktop"))
	    {
	      desktop_id = g_strconcat (prefix, basename, NULL);

	      /* Use _extended so we catch NULLs too (hidden) */
	      if (!g_hash_table_lookup_extended (apps, desktop_id, NULL, NULL))
		{
		  appinfo = g_desktop_app_info_new_from_filename (filename);
                  hidden = FALSE;

		  if (appinfo && g_desktop_app_info_get_is_hidden (appinfo))
		    {
		      g_object_unref (appinfo);
		      appinfo = NULL;
		      hidden = TRUE;
		    }
				      
		  if (appinfo || hidden)
		    {
		      g_hash_table_insert (apps, g_strdup (desktop_id), appinfo);

		      if (appinfo)
			{
			  /* Reuse instead of strdup here */
			  appinfo->desktop_id = desktop_id;
			  desktop_id = NULL;
			}
		    }
		}
	      g_free (desktop_id);
	    }
	  else
	    {
	      if (g_file_test (filename, G_FILE_TEST_IS_DIR))
		{
		  subprefix = g_strconcat (prefix, basename, "-", NULL);
		  get_apps_from_dir (apps, filename, subprefix);
		  g_free (subprefix);
		}
	    }
	  g_free (filename);
	}
      g_dir_close (dir);
    }
}


/**
 * g_app_info_get_all:
 *
 * Gets a list of all of the applications currently registered 
 * on this system.
 * 
 * For desktop files, this includes applications that have 
 * <literal>NoDisplay=true</literal> set or are excluded from 
 * display by means of <literal>OnlyShowIn</literal> or
 * <literal>NotShowIn</literal>. See g_app_info_should_show().
 * The returned list does not include applications which have
 * the <literal>Hidden</literal> key set. 
 * 
 * Returns: (element-type GAppInfo) (transfer full): a newly allocated #GList of references to #GAppInfo<!---->s.
 **/
GList *
g_app_info_get_all (void)
{
  const char * const *dirs;
  GHashTable *apps;
  GHashTableIter iter;
  gpointer value;
  int i;
  GList *infos;

  dirs = get_applications_search_path ();

  apps = g_hash_table_new_full (g_str_hash, g_str_equal,
				g_free, NULL);

  
  for (i = 0; dirs[i] != NULL; i++)
    get_apps_from_dir (apps, dirs[i], "");


  infos = NULL;
  g_hash_table_iter_init (&iter, apps);
  while (g_hash_table_iter_next (&iter, NULL, &value))
    {
      if (value)
        infos = g_list_prepend (infos, value);
    }

  g_hash_table_destroy (apps);

  return g_list_reverse (infos);
}

/* Cacheing of mimeinfo.cache and defaults.list files */

typedef struct {
  char *path;
  GHashTable *mime_info_cache_map;
  GHashTable *defaults_list_map;
  GHashTable *mimeapps_list_added_map;
  GHashTable *mimeapps_list_removed_map;
  GHashTable *mimeapps_list_defaults_map;
  time_t mime_info_cache_timestamp;
  time_t defaults_list_timestamp;
  time_t mimeapps_list_timestamp;
} MimeInfoCacheDir;

typedef struct {
  GList *dirs;                       /* mimeinfo.cache and defaults.list */
  GHashTable *global_defaults_cache; /* global results of defaults.list lookup and validation */
  time_t last_stat_time;
  guint should_ping_mime_monitor : 1;
} MimeInfoCache;

static MimeInfoCache *mime_info_cache = NULL;
G_LOCK_DEFINE_STATIC (mime_info_cache);

static void mime_info_cache_dir_add_desktop_entries (MimeInfoCacheDir  *dir,
						     const char        *mime_type,
						     char             **new_desktop_file_ids);

static MimeInfoCache * mime_info_cache_new (void);

static void
destroy_info_cache_value (gpointer  key, 
                          GList    *value, 
                          gpointer  data)
{
  g_list_free_full (value, g_free);
}

static void
destroy_info_cache_map (GHashTable *info_cache_map)
{
  g_hash_table_foreach (info_cache_map, (GHFunc)destroy_info_cache_value, NULL);
  g_hash_table_destroy (info_cache_map);
}

static gboolean
mime_info_cache_dir_out_of_date (MimeInfoCacheDir *dir,
				 const char       *cache_file,
				 time_t           *timestamp)
{
  struct stat buf;
  char *filename;
  
  filename = g_build_filename (dir->path, cache_file, NULL);
  
  if (g_stat (filename, &buf) < 0)
    {
      g_free (filename);
      return TRUE;
    }
  g_free (filename);

  if (buf.st_mtime != *timestamp) 
    return TRUE;
  
  return FALSE;
}

/* Call with lock held */
static gboolean
remove_all (gpointer  key,
	    gpointer  value,
	    gpointer  user_data)
{
  return TRUE;
}


static void
mime_info_cache_blow_global_cache (void)
{
  g_hash_table_foreach_remove (mime_info_cache->global_defaults_cache,
			       remove_all, NULL);
}

static void
mime_info_cache_dir_init (MimeInfoCacheDir *dir)
{
  GError *load_error;
  GKeyFile *key_file;
  gchar *filename, **mime_types;
  int i;
  struct stat buf;
  
  load_error = NULL;
  mime_types = NULL;
  
  if (dir->mime_info_cache_map != NULL &&
      !mime_info_cache_dir_out_of_date (dir, "mimeinfo.cache",
					&dir->mime_info_cache_timestamp))
    return;
  
  if (dir->mime_info_cache_map != NULL)
    destroy_info_cache_map (dir->mime_info_cache_map);
  
  dir->mime_info_cache_map = g_hash_table_new_full (g_str_hash, g_str_equal,
						    (GDestroyNotify) g_free,
						    NULL);
  
  key_file = g_key_file_new ();
  
  filename = g_build_filename (dir->path, "mimeinfo.cache", NULL);
  
  if (g_stat (filename, &buf) < 0)
    goto error;
  
  if (dir->mime_info_cache_timestamp > 0) 
    mime_info_cache->should_ping_mime_monitor = TRUE;
  
  dir->mime_info_cache_timestamp = buf.st_mtime;
  
  g_key_file_load_from_file (key_file, filename, G_KEY_FILE_NONE, &load_error);
  
  g_free (filename);
  filename = NULL;
  
  if (load_error != NULL)
    goto error;
  
  mime_types = g_key_file_get_keys (key_file, MIME_CACHE_GROUP,
				    NULL, &load_error);
  
  if (load_error != NULL)
    goto error;
  
  for (i = 0; mime_types[i] != NULL; i++)
    {
      gchar **desktop_file_ids;
      char *unaliased_type;
      desktop_file_ids = g_key_file_get_string_list (key_file,
						     MIME_CACHE_GROUP,
						     mime_types[i],
						     NULL,
						     NULL);
      
      if (desktop_file_ids == NULL)
	continue;

      unaliased_type = _g_unix_content_type_unalias (mime_types[i]);
      mime_info_cache_dir_add_desktop_entries (dir,
					       unaliased_type,
					       desktop_file_ids);
      g_free (unaliased_type);
    
      g_strfreev (desktop_file_ids);
    }
  
  g_strfreev (mime_types);
  g_key_file_free (key_file);
  
  return;
 error:
  g_free (filename);
  g_key_file_free (key_file);
  
  if (mime_types != NULL)
    g_strfreev (mime_types);
  
  if (load_error)
    g_error_free (load_error);
}

static void
mime_info_cache_dir_init_defaults_list (MimeInfoCacheDir *dir)
{
  GKeyFile *key_file;
  GError *load_error;
  gchar *filename, **mime_types;
  char *unaliased_type;
  char **desktop_file_ids;
  int i;
  struct stat buf;

  load_error = NULL;
  mime_types = NULL;

  if (dir->defaults_list_map != NULL &&
      !mime_info_cache_dir_out_of_date (dir, "defaults.list",
					&dir->defaults_list_timestamp))
    return;
  
  if (dir->defaults_list_map != NULL)
    g_hash_table_destroy (dir->defaults_list_map);
  dir->defaults_list_map = g_hash_table_new_full (g_str_hash, g_str_equal,
						  g_free, (GDestroyNotify)g_strfreev);
  

  key_file = g_key_file_new ();
  
  filename = g_build_filename (dir->path, "defaults.list", NULL);
  if (g_stat (filename, &buf) < 0)
    goto error;

  if (dir->defaults_list_timestamp > 0) 
    mime_info_cache->should_ping_mime_monitor = TRUE;

  dir->defaults_list_timestamp = buf.st_mtime;

  g_key_file_load_from_file (key_file, filename, G_KEY_FILE_NONE, &load_error);
  g_free (filename);
  filename = NULL;

  if (load_error != NULL)
    goto error;

  mime_types = g_key_file_get_keys (key_file, DEFAULT_APPLICATIONS_GROUP,
				    NULL, NULL);
  if (mime_types != NULL)
    {
      for (i = 0; mime_types[i] != NULL; i++)
	{
	  desktop_file_ids = g_key_file_get_string_list (key_file,
							 DEFAULT_APPLICATIONS_GROUP,
							 mime_types[i],
							 NULL,
							 NULL);
	  if (desktop_file_ids == NULL)
	    continue;
	  
	  unaliased_type = _g_unix_content_type_unalias (mime_types[i]);
	  g_hash_table_replace (dir->defaults_list_map,
				unaliased_type,
				desktop_file_ids);
	}
      
      g_strfreev (mime_types);
    }

  g_key_file_free (key_file);
  return;
  
 error:
  g_free (filename);
  g_key_file_free (key_file);
  
  if (mime_types != NULL)
    g_strfreev (mime_types);
  
  if (load_error)
    g_error_free (load_error);
}

static void
mime_info_cache_dir_init_mimeapps_list (MimeInfoCacheDir *dir)
{
  GKeyFile *key_file;
  GError *load_error;
  gchar *filename, **mime_types;
  char *unaliased_type;
  char **desktop_file_ids;
  char *desktop_id;
  int i;
  struct stat buf;

  load_error = NULL;
  mime_types = NULL;

  if (dir->mimeapps_list_added_map != NULL &&
      !mime_info_cache_dir_out_of_date (dir, "mimeapps.list",
					&dir->mimeapps_list_timestamp))
    return;
  
  if (dir->mimeapps_list_added_map != NULL)
    g_hash_table_destroy (dir->mimeapps_list_added_map);
  dir->mimeapps_list_added_map = g_hash_table_new_full (g_str_hash, g_str_equal,
							g_free, (GDestroyNotify)g_strfreev);
  
  if (dir->mimeapps_list_removed_map != NULL)
    g_hash_table_destroy (dir->mimeapps_list_removed_map);
  dir->mimeapps_list_removed_map = g_hash_table_new_full (g_str_hash, g_str_equal,
							  g_free, (GDestroyNotify)g_strfreev);

  if (dir->mimeapps_list_defaults_map != NULL)
    g_hash_table_destroy (dir->mimeapps_list_defaults_map);
  dir->mimeapps_list_defaults_map = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                           g_free, g_free);

  key_file = g_key_file_new ();
  
  filename = g_build_filename (dir->path, "mimeapps.list", NULL);
  if (g_stat (filename, &buf) < 0)
    goto error;

  if (dir->mimeapps_list_timestamp > 0) 
    mime_info_cache->should_ping_mime_monitor = TRUE;

  dir->mimeapps_list_timestamp = buf.st_mtime;

  g_key_file_load_from_file (key_file, filename, G_KEY_FILE_NONE, &load_error);
  g_free (filename);
  filename = NULL;

  if (load_error != NULL)
    goto error;

  mime_types = g_key_file_get_keys (key_file, ADDED_ASSOCIATIONS_GROUP,
				    NULL, NULL);
  if (mime_types != NULL)
    {
      for (i = 0; mime_types[i] != NULL; i++)
	{
	  desktop_file_ids = g_key_file_get_string_list (key_file,
							 ADDED_ASSOCIATIONS_GROUP,
							 mime_types[i],
							 NULL,
							 NULL);
	  if (desktop_file_ids == NULL)
	    continue;
	  
	  unaliased_type = _g_unix_content_type_unalias (mime_types[i]);
	  g_hash_table_replace (dir->mimeapps_list_added_map,
				unaliased_type,
				desktop_file_ids);
	}
      
      g_strfreev (mime_types);
    }

  mime_types = g_key_file_get_keys (key_file, REMOVED_ASSOCIATIONS_GROUP,
				    NULL, NULL);
  if (mime_types != NULL)
    {
      for (i = 0; mime_types[i] != NULL; i++)
	{
	  desktop_file_ids = g_key_file_get_string_list (key_file,
							 REMOVED_ASSOCIATIONS_GROUP,
							 mime_types[i],
							 NULL,
							 NULL);
	  if (desktop_file_ids == NULL)
	    continue;
	  
	  unaliased_type = _g_unix_content_type_unalias (mime_types[i]);
	  g_hash_table_replace (dir->mimeapps_list_removed_map,
				unaliased_type,
				desktop_file_ids);
	}
      
      g_strfreev (mime_types);
    }

  mime_types = g_key_file_get_keys (key_file, DEFAULT_APPLICATIONS_GROUP,
                                    NULL, NULL);
  if (mime_types != NULL)
    {
      for (i = 0; mime_types[i] != NULL; i++)
        {
          desktop_id = g_key_file_get_string (key_file,
                                              DEFAULT_APPLICATIONS_GROUP,
                                              mime_types[i],
                                              NULL);
          if (desktop_id == NULL)
            continue;

          unaliased_type = _g_unix_content_type_unalias (mime_types[i]);
          g_hash_table_replace (dir->mimeapps_list_defaults_map,
                                unaliased_type,
                                desktop_id);
        }

      g_strfreev (mime_types);
    }

  g_key_file_free (key_file);
  return;
  
 error:
  g_free (filename);
  g_key_file_free (key_file);
  
  if (mime_types != NULL)
    g_strfreev (mime_types);
  
  if (load_error)
    g_error_free (load_error);
}

static MimeInfoCacheDir *
mime_info_cache_dir_new (const char *path)
{
  MimeInfoCacheDir *dir;

  dir = g_new0 (MimeInfoCacheDir, 1);
  dir->path = g_strdup (path);
  
  return dir;
}

static void
mime_info_cache_dir_free (MimeInfoCacheDir *dir)
{
  if (dir == NULL)
    return;
  
  if (dir->mime_info_cache_map != NULL)
    {
      destroy_info_cache_map (dir->mime_info_cache_map);
      dir->mime_info_cache_map = NULL;
      
  }
  
  if (dir->defaults_list_map != NULL)
    {
      g_hash_table_destroy (dir->defaults_list_map);
      dir->defaults_list_map = NULL;
    }
  
  if (dir->mimeapps_list_added_map != NULL)
    {
      g_hash_table_destroy (dir->mimeapps_list_added_map);
      dir->mimeapps_list_added_map = NULL;
    }
  
  if (dir->mimeapps_list_removed_map != NULL)
    {
      g_hash_table_destroy (dir->mimeapps_list_removed_map);
      dir->mimeapps_list_removed_map = NULL;
    }

  if (dir->mimeapps_list_defaults_map != NULL)
    {
      g_hash_table_destroy (dir->mimeapps_list_defaults_map);
      dir->mimeapps_list_defaults_map = NULL;
    }

  g_free (dir);
}

static void
mime_info_cache_dir_add_desktop_entries (MimeInfoCacheDir  *dir,
					 const char        *mime_type,
					 char             **new_desktop_file_ids)
{
  GList *desktop_file_ids;
  int i;
  
  desktop_file_ids = g_hash_table_lookup (dir->mime_info_cache_map,
					  mime_type);
  
  for (i = 0; new_desktop_file_ids[i] != NULL; i++)
    {
      if (!g_list_find_custom (desktop_file_ids, new_desktop_file_ids[i], (GCompareFunc) strcmp))
	desktop_file_ids = g_list_append (desktop_file_ids,
					  g_strdup (new_desktop_file_ids[i]));
    }
  
  g_hash_table_insert (dir->mime_info_cache_map, g_strdup (mime_type), desktop_file_ids);
}

static void
mime_info_cache_init_dir_lists (void)
{
  const char * const *dirs;
  int i;
  
  mime_info_cache = mime_info_cache_new ();
  
  dirs = get_applications_search_path ();
  
  for (i = 0; dirs[i] != NULL; i++)
    {
      MimeInfoCacheDir *dir;
      
      dir = mime_info_cache_dir_new (dirs[i]);
      
      if (dir != NULL)
	{
	  mime_info_cache_dir_init (dir);
	  mime_info_cache_dir_init_defaults_list (dir);
	  mime_info_cache_dir_init_mimeapps_list (dir);
	  
	  mime_info_cache->dirs = g_list_append (mime_info_cache->dirs, dir);
	}
    }
}

static void
mime_info_cache_update_dir_lists (void)
{
  GList *tmp;
  
  tmp = mime_info_cache->dirs;
  
  while (tmp != NULL)
    {
      MimeInfoCacheDir *dir = (MimeInfoCacheDir *) tmp->data;

      /* No need to do this if we had file monitors... */
      mime_info_cache_blow_global_cache ();
      mime_info_cache_dir_init (dir);
      mime_info_cache_dir_init_defaults_list (dir);
      mime_info_cache_dir_init_mimeapps_list (dir);
      
      tmp = tmp->next;
    }
}

static void
mime_info_cache_init (void)
{
  G_LOCK (mime_info_cache);
  if (mime_info_cache == NULL)
    mime_info_cache_init_dir_lists ();
  else
    {
      time_t now;
      
      time (&now);
      if (now >= mime_info_cache->last_stat_time + 10)
	{
	  mime_info_cache_update_dir_lists ();
	  mime_info_cache->last_stat_time = now;
	}
    }
  
  if (mime_info_cache->should_ping_mime_monitor)
    {
      /* g_idle_add (emit_mime_changed, NULL); */
      mime_info_cache->should_ping_mime_monitor = FALSE;
    }
  
  G_UNLOCK (mime_info_cache);
}

static MimeInfoCache *
mime_info_cache_new (void)
{
  MimeInfoCache *cache;
  
  cache = g_new0 (MimeInfoCache, 1);
  
  cache->global_defaults_cache = g_hash_table_new_full (g_str_hash, g_str_equal,
							(GDestroyNotify) g_free,
							(GDestroyNotify) g_free);
  return cache;
}

static void
mime_info_cache_free (MimeInfoCache *cache)
{
  if (cache == NULL)
    return;
  
  g_list_free_full (cache->dirs, (GDestroyNotify) mime_info_cache_dir_free);
  g_hash_table_destroy (cache->global_defaults_cache);
  g_free (cache);
}

/**
 * mime_info_cache_reload:
 * @dir: directory path which needs reloading.
 * 
 * Reload the mime information for the @dir.
 */
static void
mime_info_cache_reload (const char *dir)
{
  /* FIXME: just reload the dir that needs reloading,
   * don't blow the whole cache
   */
  if (mime_info_cache != NULL)
    {
      G_LOCK (mime_info_cache);
      mime_info_cache_free (mime_info_cache);
      mime_info_cache = NULL;
      G_UNLOCK (mime_info_cache);
    }
}

static GList *
append_desktop_entry (GList      *list, 
                      const char *desktop_entry,
		      GList      *removed_entries)
{
  /* Add if not already in list, and valid */
  if (!g_list_find_custom (list, desktop_entry, (GCompareFunc) strcmp) &&
      !g_list_find_custom (removed_entries, desktop_entry, (GCompareFunc) strcmp))
    list = g_list_prepend (list, g_strdup (desktop_entry));
  
  return list;
}

/**
 * get_all_desktop_entries_for_mime_type:
 * @mime_type: a mime type.
 * @except: NULL or a strv list
 *
 * Returns all the desktop ids for @mime_type. The desktop files
 * are listed in an order so that default applications are listed before
 * non-default ones, and handlers for inherited mimetypes are listed
 * after the base ones.
 *
 * Optionally doesn't list the desktop ids given in the @except 
 *
 * Return value: a #GList containing the desktop ids which claim
 *    to handle @mime_type.
 */
static GList *
get_all_desktop_entries_for_mime_type (const char  *base_mime_type,
				       const char **except,
				       gboolean     include_fallback,
                                       char       **explicit_default)
{
  GList *desktop_entries, *removed_entries, *list, *dir_list, *tmp;
  MimeInfoCacheDir *dir;
  char *mime_type, *default_entry = NULL;
  char *old_default_entry = NULL;
  const char *entry;
  char **mime_types;
  char **default_entries;
  char **removed_associations;
  gboolean already_found_handler;
  int i, j, k;
  GPtrArray *array;
  char **anc;
  
  mime_info_cache_init ();

  if (include_fallback)
    {
      /* collect all ancestors */
      mime_types = _g_unix_content_type_get_parents (base_mime_type);
      array = g_ptr_array_new ();
      for (i = 0; mime_types[i]; i++)
	g_ptr_array_add (array, mime_types[i]);
      g_free (mime_types);
      for (i = 0; i < array->len; i++)
	{
	  anc = _g_unix_content_type_get_parents (g_ptr_array_index (array, i));
	  for (j = 0; anc[j]; j++)
	    {
	      for (k = 0; k < array->len; k++)
		{
		  if (strcmp (anc[j], g_ptr_array_index (array, k)) == 0)
		    break;
		}
	      if (k == array->len) /* not found */
		g_ptr_array_add (array, g_strdup (anc[j]));
	    }
	  g_strfreev (anc);
	}
      g_ptr_array_add (array, NULL);
      mime_types = (char **)g_ptr_array_free (array, FALSE);
    }
  else
    {
      mime_types = g_malloc0 (2 * sizeof (gchar *));
      mime_types[0] = g_strdup (base_mime_type);
      mime_types[1] = NULL;
    }

  G_LOCK (mime_info_cache);
  
  removed_entries = NULL;
  desktop_entries = NULL;

  for (i = 0; except != NULL && except[i] != NULL; i++)
    removed_entries = g_list_prepend (removed_entries, g_strdup (except[i]));
  
  for (i = 0; mime_types[i] != NULL; i++)
    {
      mime_type = mime_types[i];

      /* This is true if we already found a handler for a more specific
         mimetype. If its set we ignore any defaults for the less specific
         mimetypes. */
      already_found_handler = (desktop_entries != NULL);

      /* Go through all apps listed in user and system dirs */
      for (dir_list = mime_info_cache->dirs;
	   dir_list != NULL;
	   dir_list = dir_list->next)
	{
	  dir = dir_list->data;

          /* Pick the explicit default application if we got no result earlier
           * (ie, for more specific mime types)
           */
          if (!already_found_handler)
            {
              entry = g_hash_table_lookup (dir->mimeapps_list_defaults_map, mime_type);

              if (entry != NULL)
                {
                  /* Save the default entry if it's the first one we encounter */
                  if (default_entry == NULL)
                    default_entry = g_strdup (entry);
                }
            }

	  /* Then added associations from mimeapps.list */
	  default_entries = g_hash_table_lookup (dir->mimeapps_list_added_map, mime_type);
	  for (j = 0; default_entries != NULL && default_entries[j] != NULL; j++)
            desktop_entries = append_desktop_entry (desktop_entries, default_entries[j], removed_entries);

	  /* Then removed associations from mimeapps.list */
	  removed_associations = g_hash_table_lookup (dir->mimeapps_list_removed_map, mime_type);
	  for (j = 0; removed_associations != NULL && removed_associations[j] != NULL; j++)
	    removed_entries = append_desktop_entry (removed_entries, removed_associations[j], NULL);

	  /* Then system defaults (or old per-user config) (using removed associations from this dir or earlier) */
	  default_entries = g_hash_table_lookup (dir->defaults_list_map, mime_type);
	  for (j = 0; default_entries != NULL && default_entries[j] != NULL; j++)
            {
              if (default_entry == NULL && old_default_entry == NULL && !already_found_handler)
                old_default_entry = g_strdup (default_entries[j]);

              desktop_entries = append_desktop_entry (desktop_entries, default_entries[j], removed_entries);
            }
	}

      /* Go through all entries that support the mimetype */
      for (dir_list = mime_info_cache->dirs;
	   dir_list != NULL;
	   dir_list = dir_list->next) 
        {
	  dir = dir_list->data;
	
	  list = g_hash_table_lookup (dir->mime_info_cache_map, mime_type);
	  for (tmp = list; tmp != NULL; tmp = tmp->next)
	    desktop_entries = append_desktop_entry (desktop_entries, tmp->data, removed_entries);
        }
    }
  
  G_UNLOCK (mime_info_cache);

  g_strfreev (mime_types);

  /* If we have no default from mimeapps.list, take it from
   * defaults.list intead.
   *
   * If we do have a default from mimeapps.list, free any one that came
   * from defaults.list.
   */
  if (default_entry == NULL)
    default_entry = old_default_entry;
  else
    g_free (old_default_entry);

  if (explicit_default != NULL)
    *explicit_default = default_entry;
  else
    g_free (default_entry);

  g_list_free_full (removed_entries, g_free);

  desktop_entries = g_list_reverse (desktop_entries);
  
  return desktop_entries;
}

/* GDesktopAppInfoLookup interface: */

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

typedef GDesktopAppInfoLookupIface GDesktopAppInfoLookupInterface;
G_DEFINE_INTERFACE (GDesktopAppInfoLookup, g_desktop_app_info_lookup, G_TYPE_OBJECT)

static void
g_desktop_app_info_lookup_default_init (GDesktopAppInfoLookupInterface *iface)
{
}

/**
 * g_desktop_app_info_lookup_get_default_for_uri_scheme:
 * @lookup: a #GDesktopAppInfoLookup
 * @uri_scheme: a string containing a URI scheme.
 *
 * Gets the default application for launching applications 
 * using this URI scheme for a particular GDesktopAppInfoLookup
 * implementation.
 *
 * The GDesktopAppInfoLookup interface and this function is used
 * to implement g_app_info_get_default_for_uri_scheme() backends
 * in a GIO module. There is no reason for applications to use it
 * directly. Applications should use g_app_info_get_default_for_uri_scheme().
 * 
 * Returns: (transfer full): #GAppInfo for given @uri_scheme or %NULL on error.
 *
 * Deprecated: The #GDesktopAppInfoLookup interface is deprecated and unused by gio.
 */
GAppInfo *
g_desktop_app_info_lookup_get_default_for_uri_scheme (GDesktopAppInfoLookup *lookup,
						      const char            *uri_scheme)
{
  GDesktopAppInfoLookupIface *iface;

  g_return_val_if_fail (G_IS_DESKTOP_APP_INFO_LOOKUP (lookup), NULL);

  iface = G_DESKTOP_APP_INFO_LOOKUP_GET_IFACE (lookup);

  return (* iface->get_default_for_uri_scheme) (lookup, uri_scheme);
}

G_GNUC_END_IGNORE_DEPRECATIONS
