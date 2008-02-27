/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

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
#include <sys/types.h>
#include <string.h>
#include <stdio.h>
#include "gcontenttypeprivate.h"
#include "gthemedicon.h"
#include "glibintl.h"

#include "gioalias.h"

/**
 * SECTION:gcontenttype
 * @short_description: Platform-specific content typing
 * @include: gio/gio.h
 *
 * A content type is a platform specific string that defines the type
 * of a file. On unix it is a mime type, on win32 it is an extension string
 * like ".doc", ".txt" or a percieved string like "audio". Such strings
 * can be looked up in the registry at HKEY_CLASSES_ROOT.
 **/

#ifdef G_OS_WIN32

#include <windows.h>

static char *
get_registry_classes_key (const char    *subdir,
			  const wchar_t *key_name)
{
  wchar_t *wc_key;
  HKEY reg_key = NULL;
  DWORD key_type;
  DWORD nbytes;
  char *value_utf8;

  value_utf8 = NULL;
  
  nbytes = 0;
  wc_key = g_utf8_to_utf16 (subdir, -1, NULL, NULL, NULL);
  if (RegOpenKeyExW (HKEY_CLASSES_ROOT, wc_key, 0,
		     KEY_QUERY_VALUE, &reg_key) == ERROR_SUCCESS &&
      RegQueryValueExW (reg_key, key_name, 0,
			&key_type, NULL, &nbytes) == ERROR_SUCCESS &&
      key_type == REG_SZ)
    {
      wchar_t *wc_temp = g_new (wchar_t, (nbytes+1)/2 + 1);
      RegQueryValueExW (reg_key, key_name, 0,
			&key_type, (LPBYTE) wc_temp, &nbytes);
      wc_temp[nbytes/2] = '\0';
      value_utf8 = g_utf16_to_utf8 (wc_temp, -1, NULL, NULL, NULL);
      g_free (wc_temp);
    }
  g_free (wc_key);
  
  if (reg_key != NULL)
    RegCloseKey (reg_key);

  return value_utf8;
}

gboolean
g_content_type_equals (const char *type1,
		       const char *type2)
{
  char *progid1, *progid2;
  gboolean res;
  
  g_return_val_if_fail (type1 != NULL, FALSE);
  g_return_val_if_fail (type2 != NULL, FALSE);

  if (g_ascii_strcasecmp (type1, type2) == 0)
    return TRUE;

  res = FALSE;
  progid1 = get_registry_classes_key (type1, NULL);
  progid2 = get_registry_classes_key (type2, NULL);
  if (progid1 != NULL && progid2 != NULL &&
      strcmp (progid1, progid2) == 0)
    res = TRUE;
  g_free (progid1);
  g_free (progid2);
  
  return res;
}

gboolean
g_content_type_is_a (const char *type,
		     const char *supertype)
{
  gboolean res;
  char *value_utf8;

  g_return_val_if_fail (type != NULL, FALSE);
  g_return_val_if_fail (supertype != NULL, FALSE);

  if (g_content_type_equals (type, supertype))
    return TRUE;

  res = FALSE;
  value_utf8 = get_registry_classes_key (type, L"PerceivedType");
  if (value_utf8 && strcmp (value_utf8, supertype) == 0)
    res = TRUE;
  g_free (value_utf8);
  
  return res;
}

gboolean
g_content_type_is_unknown (const char *type)
{
  g_return_val_if_fail (type != NULL, FALSE);

  return strcmp ("*", type) == 0;
}

char *
g_content_type_get_description (const char *type)
{
  char *progid;
  char *description;

  g_return_val_if_fail (type != NULL, NULL);

  progid = get_registry_classes_key (type, NULL);
  if (progid)
    {
      description = get_registry_classes_key (progid, NULL);
      g_free (progid);

      if (description)
	return description;
    }

  if (g_content_type_is_unknown (type))
    return g_strdup (_("Unknown type"));
  return g_strdup_printf (_("%s filetype"), type);
}

char *
g_content_type_get_mime_type (const char *type)
{
  char *mime;

  g_return_val_if_fail (type != NULL, NULL);

  mime = get_registry_classes_key (type, L"Content Type");
  if (mime)
    return mime;
  else if (g_content_type_is_unknown (type))
    return g_strdup ("application/octet-stream");
  else if (*type == '.')
    return g_strdup_printf ("application/x-ext-%s", type+1);
  /* TODO: Map "image" to "image/ *", etc? */

  return g_strdup ("application/octet-stream");
}

GIcon *
g_content_type_get_icon (const char *type)
{
  g_return_val_if_fail (type != NULL, NULL);

  /* TODO: How do we represent icons???
     In the registry they are the default value of
     HKEY_CLASSES_ROOT\<progid>\DefaultIcon with typical values like:
     <type>: <value>
     REG_EXPAND_SZ: %SystemRoot%\System32\Wscript.exe,3
     REG_SZ: shimgvw.dll,3
  */
  return NULL;
}

gboolean
g_content_type_can_be_executable (const char *type)
{
  g_return_val_if_fail (type != NULL, FALSE);

  if (strcmp (type, ".exe") == 0 ||
      strcmp (type, ".com") == 0 ||
      strcmp (type, ".bat") == 0)
    return TRUE;
  return FALSE;
}

static gboolean
looks_like_text (const guchar *data, 
                 gsize         data_size)
{
  gsize i;
  guchar c;
  for (i = 0; i < data_size; i++)
    {
      c = data[i];
      if (g_ascii_iscntrl (c) && !g_ascii_isspace (c))
	return FALSE;
    }
  return TRUE;
}

char *
g_content_type_guess (const char   *filename,
		      const guchar *data,
		      gsize         data_size,
		      gboolean     *result_uncertain)
{
  char *basename;
  char *type;
  char *dot;

  type = NULL;

  if (filename)
    {
      basename = g_path_get_basename (filename);
      dot = strrchr (basename, '.');
      if (dot)
	type = g_strdup (dot);
      g_free (basename);
    }

  if (type)
    return type;

  if (data && looks_like_text (data, data_size))
    return g_strdup (".txt");

  return g_strdup ("*");
}

GList *
g_content_types_get_registered (void)
{
  DWORD index;
  wchar_t keyname[256];
  DWORD key_len;
  char *key_utf8;
  GList *types;

  types = NULL;
  index = 0;
  key_len = 256;
  while (RegEnumKeyExW(HKEY_CLASSES_ROOT,
		       index,
		       keyname,
		       &key_len,
		       NULL,
		       NULL,
		       NULL,
		       NULL) == ERROR_SUCCESS)
    {
      key_utf8 = g_utf16_to_utf8 (keyname, -1, NULL, NULL, NULL);
      if (key_utf8)
	{
	  if (*key_utf8 == '.')
	    types = g_list_prepend (types, key_utf8);
	  else
	    g_free (key_utf8);
	}
      index++;
      key_len = 256;
    }
  
  return g_list_reverse (types);
}

#else /* !G_OS_WIN32 - Unix specific version */

#include <dirent.h>

#define XDG_PREFIX _gio_xdg
#include "xdgmime/xdgmime.h"

/* We lock this mutex whenever we modify global state in this module.  */
G_LOCK_DEFINE_STATIC (gio_xdgmime);

gsize
_g_unix_content_type_get_sniff_len (void)
{
  gsize size;

  G_LOCK (gio_xdgmime);
  size = xdg_mime_get_max_buffer_extents ();
  G_UNLOCK (gio_xdgmime);

  return size;
}

char *
_g_unix_content_type_unalias (const char *type)
{
  char *res;
  
  G_LOCK (gio_xdgmime);
  res = g_strdup (xdg_mime_unalias_mime_type (type));
  G_UNLOCK (gio_xdgmime);
  
  return res;
}

char **
_g_unix_content_type_get_parents (const char *type)
{
  const char *umime;
  char **parents;
  GPtrArray *array;
  int i;

  array = g_ptr_array_new ();
  
  G_LOCK (gio_xdgmime);
  
  umime = xdg_mime_unalias_mime_type (type);
  
  g_ptr_array_add (array, g_strdup (umime));
  
  parents = xdg_mime_list_mime_parents (umime);
  for (i = 0; parents && parents[i] != NULL; i++)
    g_ptr_array_add (array, g_strdup (parents[i]));
  
  free (parents);
  
  G_UNLOCK (gio_xdgmime);
  
  g_ptr_array_add (array, NULL);
  
  return (char **)g_ptr_array_free (array, FALSE);
}

/**
 * g_content_type_equals:
 * @type1: a content type string.
 * @type2: a content type string.
 *
 * Compares two content types for equality.
 *
 * Returns: %TRUE if the two strings are identical or equivalent,
 * %FALSE otherwise.
 **/  
gboolean
g_content_type_equals (const char *type1,
		       const char *type2)
{
  gboolean res;
  
  g_return_val_if_fail (type1 != NULL, FALSE);
  g_return_val_if_fail (type2 != NULL, FALSE);
  
  G_LOCK (gio_xdgmime);
  res = xdg_mime_mime_type_equal (type1, type2);
  G_UNLOCK (gio_xdgmime);
	
  return res;
}

/**
 * g_content_type_is_a:
 * @type: a content type string. 
 * @supertype: a string.
 *
 * Determines if @type is a subset of @supertype.  
 *
 * Returns: %TRUE if @type is a kind of @supertype,
 * %FALSE otherwise. 
 **/  
gboolean
g_content_type_is_a (const char *type,
		     const char *supertype)
{
  gboolean res;
    
  g_return_val_if_fail (type != NULL, FALSE);
  g_return_val_if_fail (supertype != NULL, FALSE);
  
  G_LOCK (gio_xdgmime);
  res = xdg_mime_mime_type_subclass (type, supertype);
  G_UNLOCK (gio_xdgmime);
	
  return res;
}

/**
 * g_content_type_is_unknown:
 * @type: a content type string. 
 * 
 * Checks if the content type is the generic "unknown" type.
 * On unix this is the "application/octet-stream" mimetype,
 * while on win32 it is "*".
 * 
 * Returns: %TRUE if the type is the unknown type.
 **/  
gboolean
g_content_type_is_unknown (const char *type)
{
  g_return_val_if_fail (type != NULL, FALSE);

  return strcmp (XDG_MIME_TYPE_UNKNOWN, type) == 0;
}


typedef enum {
  MIME_TAG_TYPE_OTHER,
  MIME_TAG_TYPE_COMMENT
} MimeTagType;

typedef struct {
  int current_type;
  int current_lang_level;
  int comment_lang_level;
  char *comment;
} MimeParser;


static int
language_level (const char *lang)
{
  const char * const *lang_list;
  int i;
  
  /* The returned list is sorted from most desirable to least
     desirable and always contains the default locale "C". */
  lang_list = g_get_language_names ();
  
  for (i = 0; lang_list[i]; i++)
    if (strcmp (lang_list[i], lang) == 0)
      return 1000-i;
  
  return 0;
}

static void
mime_info_start_element (GMarkupParseContext  *context,
			 const gchar          *element_name,
			 const gchar         **attribute_names,
			 const gchar         **attribute_values,
			 gpointer              user_data,
			 GError              **error)
{
  int i;
  const char *lang;
  MimeParser *parser = user_data;
  
  if (strcmp (element_name, "comment") == 0)
    {
      lang = "C";
      for (i = 0; attribute_names[i]; i++)
	if (strcmp (attribute_names[i], "xml:lang") == 0)
	  {
	    lang = attribute_values[i];
	    break;
	  }
      
      parser->current_lang_level = language_level (lang);
      parser->current_type = MIME_TAG_TYPE_COMMENT;
    }
  else
    parser->current_type = MIME_TAG_TYPE_OTHER;
  
}

static void
mime_info_end_element (GMarkupParseContext  *context,
		       const gchar          *element_name,
		       gpointer              user_data,
		       GError              **error)
{
  MimeParser *parser = user_data;
  
  parser->current_type = MIME_TAG_TYPE_OTHER;
}

static void
mime_info_text (GMarkupParseContext  *context,
		const gchar          *text,
		gsize                 text_len,  
		gpointer              user_data,
		GError              **error)
{
  MimeParser *parser = user_data;

  if (parser->current_type == MIME_TAG_TYPE_COMMENT &&
      parser->current_lang_level > parser->comment_lang_level)
    {
      g_free (parser->comment);
      parser->comment = g_strndup (text, text_len);
      parser->comment_lang_level = parser->current_lang_level;
    }
}

static char *
load_comment_for_mime_helper (const char *dir, 
                              const char *basename)
{
  GMarkupParseContext *context;
  char *filename, *data;
  gsize len;
  gboolean res;
  MimeParser parse_data = {0};
  GMarkupParser parser = {
    mime_info_start_element,
    mime_info_end_element,
    mime_info_text
  };

  filename = g_build_filename (dir, "mime", basename, NULL);
  
  res = g_file_get_contents (filename,  &data,  &len,  NULL);
  g_free (filename);
  if (!res)
    return NULL;
  
  context = g_markup_parse_context_new   (&parser, 0, &parse_data, NULL);
  res = g_markup_parse_context_parse (context, data, len, NULL);
  g_free (data);
  g_markup_parse_context_free (context);
  
  if (!res)
    return NULL;

  return parse_data.comment;
}


static char *
load_comment_for_mime (const char *mimetype)
{
  const char * const* dirs;
  char *basename;
  char *comment;
  int i;

  basename = g_strdup_printf ("%s.xml", mimetype);

  comment = load_comment_for_mime_helper (g_get_user_data_dir (), basename);
  if (comment)
    {
      g_free (basename);
      return comment;
    }
  
  dirs = g_get_system_data_dirs ();

  for (i = 0; dirs[i] != NULL; i++)
    {
      comment = load_comment_for_mime_helper (dirs[i], basename);
      if (comment)
	{
	  g_free (basename);
	  return comment;
	}
    }
  g_free (basename);
  
  return g_strdup_printf (_("%s type"), mimetype);
}

/**
 * g_content_type_get_description:
 * @type: a content type string. 
 * 
 * Gets the human readable description of the content type.
 * 
 * Returns: a short description of the content type @type. 
 **/  
char *
g_content_type_get_description (const char *type)
{
  static GHashTable *type_comment_cache = NULL;
  char *comment;

  g_return_val_if_fail (type != NULL, NULL);
  
  G_LOCK (gio_xdgmime);
  type = xdg_mime_unalias_mime_type (type);

  if (type_comment_cache == NULL)
    type_comment_cache = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

  comment = g_hash_table_lookup (type_comment_cache, type);
  comment = g_strdup (comment);
  G_UNLOCK (gio_xdgmime);
  
  if (comment != NULL)
    return comment;

  comment = load_comment_for_mime (type);
  
  G_LOCK (gio_xdgmime);
  g_hash_table_insert (type_comment_cache,
		       g_strdup (type),
		       g_strdup (comment));
  G_UNLOCK (gio_xdgmime);

  return comment;
}

/**
 * g_content_type_get_mime_type:
 * @type: a content type string. 
 * 
 * Gets the mime-type for the content type. If one is registered
 * 
 * Returns: the registered mime-type for the given @type, or NULL if unknown.
 **/  
char *
g_content_type_get_mime_type (const char *type)
{
  g_return_val_if_fail (type != NULL, NULL);

  return g_strdup (type);
}

/**
 * g_content_type_get_icon:
 * @type: a content type string.
 * 
 * Gets the icon for a content type.
 * 
 * Returns: #GIcon corresponding to the content type.
 **/  
GIcon *
g_content_type_get_icon (const char *type)
{
  char *mimetype_icon, *generic_mimetype_icon, *q;
  char *icon_names[3];
  const char *p;
  GIcon *themed_icon;
  
  g_return_val_if_fail (type != NULL, NULL);
  
  mimetype_icon = g_strdup (type);
  
  while ((q = strchr (mimetype_icon, '/')) != NULL)
    *q = '-';
  
  p = strchr (type, '/');
  if (p == NULL)
    p = type + strlen (type);
  
  generic_mimetype_icon = g_malloc (p - type + strlen ("-x-generic") + 1);
  memcpy (generic_mimetype_icon, type, p - type);
  memcpy (generic_mimetype_icon + (p - type), "-x-generic", strlen ("-x-generic"));
  generic_mimetype_icon[(p - type) + strlen ("-x-generic")] = 0;
  
  icon_names[0] = mimetype_icon;
  /* Not all icons have migrated to the new icon theme spec, look for old names too */
  icon_names[1] = g_strconcat ("gnome-mime-", mimetype_icon, NULL);
  icon_names[2] = generic_mimetype_icon;
  
  themed_icon = g_themed_icon_new_from_names (icon_names, 3);
  
  g_free (mimetype_icon);
  g_free (icon_names[1]);
  g_free (generic_mimetype_icon);
  
  return themed_icon;
}

/**
 * g_content_type_can_be_executable:
 * @type: a content type string.
 * 
 * Checks if a content type can be executable. Note that for instance
 * things like text files can be executables (i.e. scripts and batch files).
 * 
 * Returns: %TRUE if the file type corresponds to a type that
 * can be executable, %FALSE otherwise. 
 **/  
gboolean
g_content_type_can_be_executable (const char *type)
{
  g_return_val_if_fail (type != NULL, FALSE);

  if (g_content_type_is_a (type, "application/x-executable")  ||
      g_content_type_is_a (type, "text/plain"))
    return TRUE;

  return FALSE;
}

static gboolean
looks_like_text (const guchar *data, gsize data_size)
{
  gsize i;
  char c;
  
  for (i = 0; i < data_size; i++)
    {
      c = data[i];
      
      if (g_ascii_iscntrl (c) &&
	  !g_ascii_isspace (c))
	return FALSE;
    }
  return TRUE;
}

/**
 * g_content_type_guess:
 * @filename: a string.
 * @data: a stream of data.
 * @data_size: the size of @data.
 * @result_uncertain: a flag indicating the certainty of the 
 * result.
 * 
 * Guesses the content type based on example data. If the function is 
 * uncertain, @result_uncertain will be set to %TRUE.
 * 
 * Returns: a string indicating a guessed content type for the 
 * given data. 
 **/  
char *
g_content_type_guess (const char   *filename,
		      const guchar *data,
		      gsize         data_size,
		      gboolean     *result_uncertain)
{
  char *basename;
  const char *name_mimetypes[10], *sniffed_mimetype;
  char *mimetype;
  int i;
  int n_name_mimetypes;
  int sniffed_prio;

  sniffed_prio = 0;
  n_name_mimetypes = 0;
  sniffed_mimetype = XDG_MIME_TYPE_UNKNOWN;

  if (result_uncertain)
    *result_uncertain = FALSE;
  
  G_LOCK (gio_xdgmime);
  
  if (filename)
    {
      basename = g_path_get_basename (filename);
      n_name_mimetypes = xdg_mime_get_mime_types_from_file_name (basename, name_mimetypes, 10);
      g_free (basename);
    }

  /* Got an extension match, and no conflicts. This is it. */
  if (n_name_mimetypes == 1)
    {
      G_UNLOCK (gio_xdgmime);
      return g_strdup (name_mimetypes[0]);
    }
  
  if (data)
    {
      sniffed_mimetype = xdg_mime_get_mime_type_for_data (data, data_size, &sniffed_prio);
      if (sniffed_mimetype == XDG_MIME_TYPE_UNKNOWN &&
	  data &&
	  looks_like_text (data, data_size))
	sniffed_mimetype = "text/plain";
    }
  
  if (n_name_mimetypes == 0)
    {
      if (sniffed_mimetype == XDG_MIME_TYPE_UNKNOWN &&
	  result_uncertain)
	*result_uncertain = TRUE;
      
      mimetype = g_strdup (sniffed_mimetype);
    }
  else
    {
      mimetype = NULL;
      if (sniffed_mimetype != XDG_MIME_TYPE_UNKNOWN)
	{
	  if (sniffed_prio >= 80) /* High priority sniffing match, use that */
	    mimetype = g_strdup (sniffed_mimetype);
	  else
	    {
	      /* There are conflicts between the name matches and we have a sniffed
		 type, use that as a tie breaker. */
	      
	      for (i = 0; i < n_name_mimetypes; i++)
		{
		  if ( xdg_mime_mime_type_subclass (name_mimetypes[i], sniffed_mimetype))
		    {
		      /* This nametype match is derived from (or the same as) the sniffed type).
			 This is probably it. */
		      mimetype = g_strdup (name_mimetypes[i]);
		      break;
		    }
		}
	    }
	}
      
      if (mimetype == NULL)
	{
	  /* Conflicts, and sniffed type was no help or not there. guess on the first one */
	  mimetype = g_strdup (name_mimetypes[0]);
	  if (result_uncertain)
	    *result_uncertain = TRUE;
	}
    }
  
  G_UNLOCK (gio_xdgmime);

  return mimetype;
}

static void
enumerate_mimetypes_subdir (const char *dir, 
                            const char *prefix, 
                            GHashTable *mimetypes)
{
  DIR *d;
  struct dirent *ent;
  char *mimetype;

  d = opendir (dir);
  if (d)
    {
      while ((ent = readdir (d)) != NULL)
	{
	  if (g_str_has_suffix (ent->d_name, ".xml"))
	    {
	      mimetype = g_strdup_printf ("%s/%.*s", prefix, (int) strlen (ent->d_name) - 4, ent->d_name);
	      g_hash_table_insert (mimetypes, mimetype, NULL);
	    }
	}
      closedir (d);
    }
}

static void
enumerate_mimetypes_dir (const char *dir, 
                         GHashTable *mimetypes)
{
  DIR *d;
  struct dirent *ent;
  char *mimedir;
  char *name;

  mimedir = g_build_filename (dir, "mime", NULL);
  
  d = opendir (mimedir);
  if (d)
    {
      while ((ent = readdir (d)) != NULL)
	{
	  if (strcmp (ent->d_name, "packages") != 0)
	    {
	      name = g_build_filename (mimedir, ent->d_name, NULL);
	      if (g_file_test (name, G_FILE_TEST_IS_DIR))
		enumerate_mimetypes_subdir (name, ent->d_name, mimetypes);
	      g_free (name);
	    }
	}
      closedir (d);
    }
  
  g_free (mimedir);
}

/**
 * g_content_types_get_registered:
 * 
 * Gets a list of strings containing all the registered content types
 * known to the system. The list and its data should be freed using 
 * @g_list_foreach(list, g_free, NULL) and @g_list_free(list)
 * Returns: #GList of the registered content types.
 **/  
GList *
g_content_types_get_registered (void)
{
  const char * const* dirs;
  GHashTable *mimetypes;
  GHashTableIter iter;
  gpointer key;
  int i;
  GList *l;

  mimetypes = g_hash_table_new (g_str_hash, g_str_equal);

  enumerate_mimetypes_dir (g_get_user_data_dir (), mimetypes);
  dirs = g_get_system_data_dirs ();

  for (i = 0; dirs[i] != NULL; i++)
    enumerate_mimetypes_dir (dirs[i], mimetypes);

  l = NULL;
  g_hash_table_iter_init (&iter, mimetypes);
  while (g_hash_table_iter_next (&iter, &key, NULL))
    l = g_list_prepend (l, key);

  g_hash_table_destroy (mimetypes);

  return l;
}

#endif /* Unix version */

#define __G_CONTENT_TYPE_C__
#include "gioaliasdef.c"
