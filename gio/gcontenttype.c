/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

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

#include "config.h"
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "gcontenttypeprivate.h"
#include "gthemedicon.h"
#include "gicon.h"
#include "gfile.h"
#include "gfileenumerator.h"
#include "gfileinfo.h"
#include "glibintl.h"


#include <dirent.h>

#define XDG_PREFIX _gio_xdg
#include "xdgmime/xdgmime.h"

static void tree_magic_schedule_reload (void);

/* We lock this mutex whenever we modify global state in this module.  */
G_LOCK_DEFINE_STATIC (gio_xdgmime);

gsize
_g_generic_content_type_get_sniff_len (void)
{
  gsize size;

  G_LOCK (gio_xdgmime);
  size = xdg_mime_get_max_buffer_extents ();
  G_UNLOCK (gio_xdgmime);

  return size;
}

gchar *
_g_generic_content_type_unalias (const gchar *type)
{
  gchar *res;

  G_LOCK (gio_xdgmime);
  res = g_strdup (xdg_mime_unalias_mime_type (type));
  G_UNLOCK (gio_xdgmime);

  return res;
}

gchar **
_g_generic_content_type_get_parents (const gchar *type)
{
  const gchar *umime;
  gchar **parents;
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

  return (gchar **)g_ptr_array_free (array, FALSE);
}

G_LOCK_DEFINE_STATIC (global_mime_dirs);
static gchar **global_mime_dirs = NULL;

static void
_g_generic_content_type_set_mime_dirs_locked (const char * const *dirs)
{
  g_clear_pointer (&global_mime_dirs, g_strfreev);

  if (dirs != NULL)
    {
      global_mime_dirs = g_strdupv ((gchar **) dirs);
    }
  else
    {
      GPtrArray *mime_dirs = g_ptr_array_new_with_free_func (g_free);
      const gchar * const *system_dirs = g_get_system_data_dirs ();

      g_ptr_array_add (mime_dirs, g_build_filename (g_get_user_data_dir (), "mime", NULL));
      for (; *system_dirs != NULL; system_dirs++)
        g_ptr_array_add (mime_dirs, g_build_filename (*system_dirs, "mime", NULL));
      g_ptr_array_add (mime_dirs, NULL);  /* NULL terminator */

      global_mime_dirs = (gchar **) g_ptr_array_free (mime_dirs, FALSE);
    }

  xdg_mime_set_dirs ((const gchar * const *) global_mime_dirs);
  tree_magic_schedule_reload ();
}

void
_g_generic_content_type_set_mime_dirs (const gchar * const *dirs)
{
  G_LOCK (global_mime_dirs);
  _g_generic_content_type_set_mime_dirs_locked (dirs);
  G_UNLOCK (global_mime_dirs);
}

const gchar * const *
_g_generic_content_type_get_mime_dirs (void)
{
  const gchar * const *mime_dirs;

  G_LOCK (global_mime_dirs);

  if (global_mime_dirs == NULL)
    _g_generic_content_type_set_mime_dirs_locked (NULL);

  mime_dirs = (const gchar * const *) global_mime_dirs;

  G_UNLOCK (global_mime_dirs);

  g_assert (mime_dirs != NULL);
  return mime_dirs;
}

gboolean
_g_generic_content_type_equals (const gchar *type1,
                       const gchar *type2)
{
  gboolean res;

  g_return_val_if_fail (type1 != NULL, FALSE);
  g_return_val_if_fail (type2 != NULL, FALSE);

  G_LOCK (gio_xdgmime);
  res = xdg_mime_mime_type_equal (type1, type2);
  G_UNLOCK (gio_xdgmime);

  return res;
}

gboolean
_g_generic_content_type_is_a (const gchar *type,
                              const gchar *supertype)
{
  gboolean res;

  g_return_val_if_fail (type != NULL, FALSE);
  g_return_val_if_fail (supertype != NULL, FALSE);

  G_LOCK (gio_xdgmime);
  res = xdg_mime_mime_type_subclass (type, supertype);
  G_UNLOCK (gio_xdgmime);

  return res;
}

gboolean
_g_generic_content_type_is_mime_type (const gchar *type,
                                      const gchar *mime_type)
{
  return _g_generic_content_type_is_a (type, mime_type);
}

gboolean
_g_generic_content_type_is_unknown (const gchar *type)
{
  g_return_val_if_fail (type != NULL, FALSE);

  return _xdg_mime_mime_type_cmp_ext (XDG_MIME_TYPE_UNKNOWN, type) == 0;
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

  filename = g_build_filename (dir, basename, NULL);

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
  const char * const *dirs;
  char *basename;
  char *comment;
  gsize i;

  basename = g_strdup_printf ("%s.xml", mimetype);

  dirs = _g_generic_content_type_get_mime_dirs ();
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

gchar *
_g_generic_content_type_get_description (const gchar *type)
{
  static GHashTable *type_comment_cache = NULL;
  gchar *comment;

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

char *
_g_generic_content_type_get_mime_type (const char *type)
{
  g_return_val_if_fail (type != NULL, NULL);

  return g_strdup (type);
}

static GIcon *
_g_generic_content_type_get_icon_internal (const gchar *type,
                                           gboolean     symbolic)
{
  char *mimetype_icon;
  char *generic_mimetype_icon = NULL;
  char *q;
  char *icon_names[6];
  int n = 0;
  GIcon *themed_icon;
  const char  *xdg_icon;
  int i;

  g_return_val_if_fail (type != NULL, NULL);

  G_LOCK (gio_xdgmime);
  xdg_icon = xdg_mime_get_icon (type);
  G_UNLOCK (gio_xdgmime);

  if (xdg_icon)
    icon_names[n++] = g_strdup (xdg_icon);

  mimetype_icon = g_strdup (type);
  while ((q = strchr (mimetype_icon, '/')) != NULL)
    *q = '-';

  icon_names[n++] = mimetype_icon;

  generic_mimetype_icon = _g_generic_content_type_get_generic_icon_name (type);
  if (generic_mimetype_icon)
    icon_names[n++] = generic_mimetype_icon;

  if (symbolic)
    {
      for (i = 0; i < n; i++)
        {
          icon_names[n + i] = icon_names[i];
          icon_names[i] = g_strconcat (icon_names[i], "-symbolic", NULL);
        }

      n += n;
    }

  themed_icon = g_themed_icon_new_from_names (icon_names, n);

  for (i = 0; i < n; i++)
    g_free (icon_names[i]);

  return themed_icon;
}

GIcon *
_g_generic_content_type_get_icon (const gchar *type)
{
  return _g_generic_content_type_get_icon_internal (type, FALSE);
}

GIcon *
_g_generic_content_type_get_symbolic_icon (const gchar *type)
{
  return _g_generic_content_type_get_icon_internal (type, TRUE);
}

gchar *
_g_generic_content_type_get_generic_icon_name (const gchar *type)
{
  const gchar *xdg_icon_name;
  gchar *icon_name;

  G_LOCK (gio_xdgmime);
  xdg_icon_name = xdg_mime_get_generic_icon (type);
  G_UNLOCK (gio_xdgmime);

  /* TODO: an opportunity here. Define icon-name="..." content
   * type parameter, and store icon name right with the content
   * type value! This will ensure that any content type that comes
   * with an icon name will retain it while being processed
   * by GContentType APIs.
   */
  if (!xdg_icon_name)
    {
      const char *p;
      const char *suffix = "-x-generic";

      p = strchr (type, '/');
      if (p == NULL)
        {
          p = strchr (type, ';');
          if (p == NULL)
            p = type + strlen (type);
        }

      icon_name = g_malloc (p - type + strlen (suffix) + 1);
      memcpy (icon_name, type, p - type);
      memcpy (icon_name + (p - type), suffix, strlen (suffix));
      icon_name[(p - type) + strlen (suffix)] = 0;
    }
  else
    {
      icon_name = g_strdup (xdg_icon_name);
    }

  return icon_name;
}

gboolean
_g_generic_content_type_can_be_executable (const gchar *type)
{
  g_return_val_if_fail (type != NULL, FALSE);

  if (_g_generic_content_type_is_a (type, "application/x-executable")  ||
      _g_generic_content_type_is_a (type, "text/plain"))
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
          !g_ascii_isspace (c) &&
          c != '\b')
        return FALSE;
    }
  return TRUE;
}

gchar *
_g_generic_content_type_from_mime_type (const gchar *mime_type)
{
  char *umime;

  g_return_val_if_fail (mime_type != NULL, NULL);

  G_LOCK (gio_xdgmime);
  /* mime type and content type are same on unixes */
  umime = g_strdup (xdg_mime_unalias_mime_type (mime_type));
  G_UNLOCK (gio_xdgmime);

  return umime;
}

/* Return a copy of @sniffed_mimetype, possibly
 * with "; ext=.foo" added, where "foo" is
 * either @actual_extension or the default extension
 * that mime/type database has stored for @sniffed_mimetype.
 * The point is to avoid losing the file extension
 * when guessing file mime/type, because file extension
 * has significance on some OSes (i.e. Windows).
 */
static gchar *
maybe_add_ext (const gchar *sniffed_mimetype,
               const gchar *actual_extension)
{
  gchar *ext = NULL;
  int ext_n = 0;
  const gchar *semicolon;
  gchar *sniffed_ext_mimetype;

  /* Get default extension for this mime/type */
  ext_n = xdg_mime_get_file_exts_from_mime_type (sniffed_mimetype, &ext, 1);

  if (ext_n == 1 && actual_extension != NULL)
    {
      /* See if this extension matches the actual extension */
      if (strcmp (actual_extension, ext) != 0)
        {
          /* Extension of the sniffed type doesn't match actual extension.
           * Disregard the sniffed one.
           */
          ext_n = 0;
        }
    }

  semicolon = strchr (sniffed_mimetype, ';');

  /* Only add the ext=... parameter if it's impossible to
   * infer current file extension from the mime/type alone.
   * Otherwise we end up with redundant things like
   * application/x-extension-foo; ext=foo
   * application/pdf; ext=pdf
   * and so on.
   * ext_n == 0 means that this mime/type either has
   * no associated extension (so we have to add ext=...,
   * otherwise we'll lose the extension), or its associated
   * extension does not match the actual extension (thus we
   * also have to add ext=..., otherwise we'll lose the
   * non-standard extension).
   */
  if (ext_n == 0 && actual_extension != NULL)
    {
      /* This assumes that xdg sniffer (which gave us sniffed_mimetype) doesn't
       * return a type with ext parameter.
       * If it does, we'll end up with "mime/type; ext=.returned ext=.actual",
       * and .returned takes precedence.
       */
      sniffed_ext_mimetype = g_strdup_printf ("%s%s ext=\"%s\"",
                                              sniffed_mimetype,
                                              semicolon ? "" : ";",
                                              actual_extension);
    }
  else
    {
      sniffed_ext_mimetype = g_strdup (sniffed_mimetype);
    }

  g_free (ext);

  return sniffed_ext_mimetype;
}

gchar *
_g_generic_content_type_guess (const gchar  *filename,
                               const guchar *data,
                               gsize         data_size,
                               gboolean     *result_uncertain)
{
  const char *name_mimetypes[10], *sniffed_mimetype;
  char *mimetype;
  int i;
  int n_name_mimetypes;
  int sniffed_prio;
  gchar *actual_extension;
  gchar *sniffed_ext_mimetype;

  sniffed_prio = 0;
  n_name_mimetypes = 0;
  sniffed_mimetype = XDG_MIME_TYPE_UNKNOWN;

  if (result_uncertain)
    *result_uncertain = FALSE;

  /* our test suite and potentially other code used -1 in the past, which is
   * not documented and not allowed; guard against that */
  g_return_val_if_fail (data_size != (gsize) -1, g_strdup (XDG_MIME_TYPE_UNKNOWN));

  G_LOCK (gio_xdgmime);

  actual_extension = NULL;

  if (filename)
    {
      i = strlen (filename);
      if (filename[i - 1] == '/')
        {
          name_mimetypes[0] = "inode/directory";
          name_mimetypes[1] = NULL;
          n_name_mimetypes = 1;
          if (result_uncertain)
            *result_uncertain = TRUE;
        }
      else
        {
          gchar *basename;
          gchar *basename_utf8 = NULL;

          basename = g_path_get_basename (filename);
          if (basename)
            basename_utf8 = g_filename_to_utf8 (basename, -1, NULL, NULL, NULL);

          if (basename_utf8)
            n_name_mimetypes = xdg_mime_get_mime_types_from_file_name (basename_utf8, name_mimetypes, 10);
          else
            n_name_mimetypes = 0;

          if (strrchr (basename_utf8, '.') != NULL)
            actual_extension = g_strdup (strrchr (basename_utf8, '.'));

          g_free (basename_utf8);
          g_free (basename);
        }
    }

#define APP_X_EXT_ "application/x-extension-"

  /* Got an extension match, not a synthetic, and no conflicts. This is it. */
  /* TODO: Is this correct? We're guessing based on filename *only*, even
   * though we might have some data to sniff too. The caller already spent
   * resources on I/O to get us that data, doing a mime/type data-matching
   * should be cheap by comparison - this isn't about performance anymore.
   */
  if (n_name_mimetypes == 1 &&
      strncmp (name_mimetypes[0],
               APP_X_EXT_,
               strlen (APP_X_EXT_)) != 0)
    {
      gchar *s = g_strdup (name_mimetypes[0]);
      G_UNLOCK (gio_xdgmime);
      g_free (actual_extension);
      return s;
    }

  sniffed_ext_mimetype = NULL;

  if (data)
    {
      sniffed_mimetype = xdg_mime_get_mime_type_for_data (data, data_size, &sniffed_prio);
      if (sniffed_mimetype == XDG_MIME_TYPE_UNKNOWN &&
          data &&
          looks_like_text (data, data_size))
        sniffed_mimetype = "text/plain";

      /* For security reasons we don't ever want to sniff desktop files
       * where we know the filename and it doesn't have a .desktop extension.
       * This is because desktop files allow executing any application and
       * we don't want to make it possible to hide them looking like something
       * else.
       */
      if (filename != NULL &&
          _xdg_mime_mime_type_cmp_ext (sniffed_mimetype, "application/x-desktop") == 0)
        {
          sniffed_mimetype = "text/plain";
          sniffed_ext_mimetype = g_strdup (sniffed_mimetype);
        }
      else
        {
          /* Warn, because this will produce things like "foo/bar; ext=.one ext=.two" */
          if (strstr (sniffed_mimetype, " ext="))
            g_warning ("Trying to add extension \"%s\" to content type \"%s\"",
                       actual_extension, sniffed_mimetype);
          sniffed_ext_mimetype = maybe_add_ext (sniffed_mimetype, actual_extension);
        }
    }

  if (n_name_mimetypes == 0)
    {
      if (sniffed_mimetype == XDG_MIME_TYPE_UNKNOWN &&
          result_uncertain)
        *result_uncertain = TRUE;

      mimetype = sniffed_ext_mimetype ? g_strdup (sniffed_ext_mimetype) : g_strdup (sniffed_mimetype);
    }
  else
    {
      mimetype = NULL;
      if (sniffed_mimetype != XDG_MIME_TYPE_UNKNOWN)
        {
          if (sniffed_prio >= 80) /* High priority sniffing match, use that */
            mimetype = sniffed_ext_mimetype ? g_strdup (sniffed_ext_mimetype) : g_strdup (sniffed_mimetype);
          else
            {
              /* There are conflicts between the name matches and we
               * have a sniffed type, use that as a tie breaker.
               * Ignore synthetic mime/types.
               */
              for (i = 0; i < n_name_mimetypes; i++)
                {
                  if (strncmp (name_mimetypes[i], APP_X_EXT_, strlen (APP_X_EXT_)) != 0 &&
                      xdg_mime_mime_type_subclass (name_mimetypes[i], sniffed_mimetype))
                    {
                      if (strstr (name_mimetypes[i], " ext="))
                        g_warning ("Trying to add extension \"%s\" to content type \"%s\"",
                                   actual_extension, name_mimetypes[i]);
                      /* This nametype match is derived from (or the same as)
                       * the sniffed type). This is probably it.
                       */
                      mimetype = maybe_add_ext (name_mimetypes[i], actual_extension);
                      break;
                    }
                }
            }
        }

      if (mimetype == NULL)
        {
          if (strstr (name_mimetypes[0], " ext="))
            g_warning ("Trying to add extension \"%s\" to content type \"%s\"",
                       actual_extension, name_mimetypes[0]);
          /* Conflicts, and sniffed type was no help or not there.
           * Guess on the first one
           */
          mimetype = maybe_add_ext (name_mimetypes[0], actual_extension);
          if (result_uncertain)
            *result_uncertain = TRUE;
        }
    }

  g_free (sniffed_ext_mimetype);
  g_free (actual_extension);

  G_UNLOCK (gio_xdgmime);

  return mimetype;
}

static void
enumerate_mimetypes_subdir (const char *dir,
                            const char *prefix,
                            GHashTable *mimetypes)
{
  GDir *d;
  const gchar *d_name;
  char *mimetype;

  d = g_dir_open (dir, 0, NULL);
  if (d)
    {
      while ((d_name = g_dir_read_name (d)) != NULL)
        {
          if (g_str_has_suffix (d_name, ".xml"))
            {
              mimetype = g_strdup_printf ("%s/%.*s", prefix, (int) strlen (d_name) - 4, d_name);
              g_hash_table_replace (mimetypes, mimetype, NULL);
            }
        }
      g_dir_close (d);
    }
}

static void
enumerate_mimetypes_dir (const char *dir,
                         GHashTable *mimetypes)
{
  GDir *d;
  const gchar *d_name;
  const char *mimedir;
  char *name;

  mimedir = dir;

  d = g_dir_open (mimedir, 0, NULL);
  if (d)
    {
      while ((d_name = g_dir_read_name (d)) != NULL)
        {
          if (strcmp (d_name, "packages") != 0)
            {
              name = g_build_filename (mimedir, d_name, NULL);
              if (g_file_test (name, G_FILE_TEST_IS_DIR))
                enumerate_mimetypes_subdir (name, d_name, mimetypes);
              g_free (name);
            }
        }
      g_dir_close (d);
    }
}

GList *
_g_generic_content_types_get_registered (void)
{
  const char * const *dirs;
  GHashTable *mimetypes;
  GHashTableIter iter;
  gpointer key;
  gsize i;
  GList *l;

  mimetypes = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  dirs = _g_generic_content_type_get_mime_dirs ();
  for (i = 0; dirs[i] != NULL; i++)
    enumerate_mimetypes_dir (dirs[i], mimetypes);

  l = NULL;
  g_hash_table_iter_init (&iter, mimetypes);
  while (g_hash_table_iter_next (&iter, &key, NULL))
    {
      l = g_list_prepend (l, key);
      g_hash_table_iter_steal (&iter);
    }

  g_hash_table_destroy (mimetypes);

  return l;
}


/* tree magic data */
static GList *tree_matches = NULL;
static gboolean need_reload = FALSE;

G_LOCK_DEFINE_STATIC (gio_treemagic);

typedef struct
{
  gchar *path;
  GFileType type;
  guint match_case : 1;
  guint executable : 1;
  guint non_empty  : 1;
  guint on_disc    : 1;
  gchar *mimetype;
  GList *matches;
} TreeMatchlet;

typedef struct
{
  gchar *contenttype;
  gint priority;
  GList *matches;
} TreeMatch;


static void
tree_matchlet_free (TreeMatchlet *matchlet)
{
  g_list_free_full (matchlet->matches, (GDestroyNotify) tree_matchlet_free);
  g_free (matchlet->path);
  g_free (matchlet->mimetype);
  g_slice_free (TreeMatchlet, matchlet);
}

static void
tree_match_free (TreeMatch *match)
{
  g_list_free_full (match->matches, (GDestroyNotify) tree_matchlet_free);
  g_free (match->contenttype);
  g_slice_free (TreeMatch, match);
}

static TreeMatch *
parse_header (gchar *line)
{
  gint len;
  gchar *s;
  TreeMatch *match;

  len = strlen (line);

  if (line[0] != '[' || line[len - 1] != ']')
    return NULL;

  line[len - 1] = 0;
  s = strchr (line, ':');

  match = g_slice_new0 (TreeMatch);
  match->priority = atoi (line + 1);
  match->contenttype = g_strdup (s + 1);

  return match;
}

static TreeMatchlet *
parse_match_line (gchar *line,
                  gint  *depth)
{
  gchar *s, *p;
  TreeMatchlet *matchlet;
  gchar **parts;
  gint i;

  matchlet = g_slice_new0 (TreeMatchlet);

  if (line[0] == '>')
    {
      *depth = 0;
      s = line;
    }
  else
    {
      *depth = atoi (line);
      s = strchr (line, '>');
    }
  s += 2;
  p = strchr (s, '"');
  *p = 0;

  matchlet->path = g_strdup (s);
  s = p + 1;
  parts = g_strsplit (s, ",", 0);
  if (strcmp (parts[0], "=file") == 0)
    matchlet->type = G_FILE_TYPE_REGULAR;
  else if (strcmp (parts[0], "=directory") == 0)
    matchlet->type = G_FILE_TYPE_DIRECTORY;
  else if (strcmp (parts[0], "=link") == 0)
    matchlet->type = G_FILE_TYPE_SYMBOLIC_LINK;
  else
    matchlet->type = G_FILE_TYPE_UNKNOWN;
  for (i = 1; parts[i]; i++)
    {
      if (strcmp (parts[i], "executable") == 0)
        matchlet->executable = 1;
      else if (strcmp (parts[i], "match-case") == 0)
        matchlet->match_case = 1;
      else if (strcmp (parts[i], "non-empty") == 0)
        matchlet->non_empty = 1;
      else if (strcmp (parts[i], "on-disc") == 0)
        matchlet->on_disc = 1;
      else
        matchlet->mimetype = g_strdup (parts[i]);
    }

  g_strfreev (parts);

  return matchlet;
}

static gint
cmp_match (gconstpointer a, gconstpointer b)
{
  const TreeMatch *aa = (const TreeMatch *)a;
  const TreeMatch *bb = (const TreeMatch *)b;

  return bb->priority - aa->priority;
}

static void
insert_match (TreeMatch *match)
{
  tree_matches = g_list_insert_sorted (tree_matches, match, cmp_match);
}

static void
insert_matchlet (TreeMatch    *match,
                 TreeMatchlet *matchlet,
                 gint          depth)
{
  if (depth == 0)
    match->matches = g_list_append (match->matches, matchlet);
  else
    {
      GList *last;
      TreeMatchlet *m;

      last = g_list_last (match->matches);
      if (!last)
        {
          tree_matchlet_free (matchlet);
          g_warning ("can't insert tree matchlet at depth %d", depth);
          return;
        }

      m = (TreeMatchlet *) last->data;
      while (--depth > 0)
        {
          last = g_list_last (m->matches);
          if (!last)
            {
              tree_matchlet_free (matchlet);
              g_warning ("can't insert tree matchlet at depth %d", depth);
              return;
            }

          m = (TreeMatchlet *) last->data;
        }
      m->matches = g_list_append (m->matches, matchlet);
    }
}

static void
read_tree_magic_from_directory (const gchar *prefix)
{
  gchar *filename;
  gchar *text;
  gsize len;
  gchar **lines;
  gint i;
  TreeMatch *match;
  TreeMatchlet *matchlet;
  gint depth;

  filename = g_build_filename (prefix, "treemagic", NULL);

  if (g_file_get_contents (filename, &text, &len, NULL))
    {
      if (strcmp (text, "MIME-TreeMagic") == 0)
        {
          lines = g_strsplit (text + strlen ("MIME-TreeMagic") + 2, "\n", 0);
          match = NULL;
          for (i = 0; lines[i] && lines[i][0]; i++)
            {
              if (lines[i][0] == '[')
                {
                  match = parse_header (lines[i]);
                  insert_match (match);
                }
              else if (match != NULL)
                {
                  matchlet = parse_match_line (lines[i], &depth);
                  insert_matchlet (match, matchlet, depth);
                }
              else
                {
                  g_warning ("%s: header corrupt; skipping", filename);
                  break;
                }
            }

          g_strfreev (lines);
        }
      else
        g_warning ("%s: header not found, skipping", filename);

      g_free (text);
    }

  g_free (filename);
}

static void
tree_magic_schedule_reload (void)
{
  need_reload = TRUE;
}

static void
xdg_mime_reload (void *user_data)
{
  tree_magic_schedule_reload ();
}

static void
tree_magic_shutdown (void)
{
  g_list_free_full (tree_matches, (GDestroyNotify) tree_match_free);
  tree_matches = NULL;
}

static void
tree_magic_init (void)
{
  static gboolean initialized = FALSE;
  gsize i;

  if (!initialized)
    {
      initialized = TRUE;

      xdg_mime_register_reload_callback (xdg_mime_reload, NULL, NULL);
      need_reload = TRUE;
    }

  if (need_reload)
    {
      const char * const *dirs;

      need_reload = FALSE;

      tree_magic_shutdown ();

      dirs = _g_generic_content_type_get_mime_dirs ();
      for (i = 0; dirs[i] != NULL; i++)
        read_tree_magic_from_directory (dirs[i]);
    }
}

/* a filtering enumerator */

typedef struct
{
  gchar *path;
  gint depth;
  gboolean ignore_case;
  gchar **components;
  gchar **case_components;
  GFileEnumerator **enumerators;
  GFile **children;
} Enumerator;

static gboolean
component_match (Enumerator  *e,
                 gint         depth,
                 const gchar *name)
{
  gchar *case_folded, *key;
  gboolean found;

  if (strcmp (name, e->components[depth]) == 0)
    return TRUE;

  if (!e->ignore_case)
    return FALSE;

  case_folded = g_utf8_casefold (name, -1);
  key = g_utf8_collate_key (case_folded, -1);

  found = strcmp (key, e->case_components[depth]) == 0;

  g_free (case_folded);
  g_free (key);

  return found;
}

static GFile *
next_match_recurse (Enumerator *e,
                    gint        depth)
{
  GFile *file;
  GFileInfo *info;
  const gchar *name;

  while (TRUE)
    {
      if (e->enumerators[depth] == NULL)
        {
          if (depth > 0)
            {
              file = next_match_recurse (e, depth - 1);
              if (file)
                {
                  e->children[depth] = file;
                  e->enumerators[depth] = g_file_enumerate_children (file,
                                                                     G_FILE_ATTRIBUTE_STANDARD_NAME,
                                                                     G_FILE_QUERY_INFO_NONE,
                                                                     NULL,
                                                                     NULL);
                }
            }
          if (e->enumerators[depth] == NULL)
            return NULL;
        }

      while ((info = g_file_enumerator_next_file (e->enumerators[depth], NULL, NULL)))
        {
          name = g_file_info_get_name (info);
          if (component_match (e, depth, name))
            {
              file = g_file_get_child (e->children[depth], name);
              g_object_unref (info);
              return file;
            }
          g_object_unref (info);
        }

      g_object_unref (e->enumerators[depth]);
      e->enumerators[depth] = NULL;
      g_object_unref (e->children[depth]);
      e->children[depth] = NULL;
    }
}

static GFile *
enumerator_next (Enumerator *e)
{
  return next_match_recurse (e, e->depth - 1);
}

static Enumerator *
enumerator_new (GFile      *root,
                const char *path,
                gboolean    ignore_case)
{
  Enumerator *e;
  gint i;
  gchar *case_folded;

  e = g_new0 (Enumerator, 1);
  e->path = g_strdup (path);
  e->ignore_case = ignore_case;

  e->components = g_strsplit (e->path, G_DIR_SEPARATOR_S, -1);
  e->depth = g_strv_length (e->components);
  if (e->ignore_case)
    {
      e->case_components = g_new0 (char *, e->depth + 1);
      for (i = 0; e->components[i]; i++)
        {
          case_folded = g_utf8_casefold (e->components[i], -1);
          e->case_components[i] = g_utf8_collate_key (case_folded, -1);
          g_free (case_folded);
        }
    }

  e->children = g_new0 (GFile *, e->depth);
  e->children[0] = g_object_ref (root);
  e->enumerators = g_new0 (GFileEnumerator *, e->depth);
  e->enumerators[0] = g_file_enumerate_children (root,
                                                 G_FILE_ATTRIBUTE_STANDARD_NAME,
                                                 G_FILE_QUERY_INFO_NONE,
                                                 NULL,
                                                 NULL);

  return e;
}

static void
enumerator_free (Enumerator *e)
{
  gint i;

  for (i = 0; i < e->depth; i++)
    {
      if (e->enumerators[i])
        g_object_unref (e->enumerators[i]);
      if (e->children[i])
        g_object_unref (e->children[i]);
    }

  g_free (e->enumerators);
  g_free (e->children);
  g_strfreev (e->components);
  if (e->case_components)
    g_strfreev (e->case_components);
  g_free (e->path);
  g_free (e);
}

static gboolean
matchlet_match (TreeMatchlet *matchlet,
                GFile        *root)
{
  GFile *file;
  GFileInfo *info;
  gboolean result;
  const gchar *attrs;
  Enumerator *e;
  GList *l;

  e = enumerator_new (root, matchlet->path, !matchlet->match_case);

  do
    {
      file = enumerator_next (e);
      if (!file)
        {
          enumerator_free (e);
          return FALSE;
        }

      if (matchlet->mimetype)
        attrs = G_FILE_ATTRIBUTE_STANDARD_TYPE ","
                G_FILE_ATTRIBUTE_ACCESS_CAN_EXECUTE ","
                G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE;
      else
        attrs = G_FILE_ATTRIBUTE_STANDARD_TYPE ","
                G_FILE_ATTRIBUTE_ACCESS_CAN_EXECUTE;
      info = g_file_query_info (file,
                                attrs,
                                G_FILE_QUERY_INFO_NONE,
                                NULL,
                                NULL);
      if (info)
        {
          result = TRUE;

          if (matchlet->type != G_FILE_TYPE_UNKNOWN &&
              g_file_info_get_file_type (info) != matchlet->type)
            result = FALSE;

          if (matchlet->executable &&
              !g_file_info_get_attribute_boolean (info, G_FILE_ATTRIBUTE_ACCESS_CAN_EXECUTE))
            result = FALSE;
        }
      else
        result = FALSE;

      if (result && matchlet->non_empty)
        {
          GFileEnumerator *child_enum;
          GFileInfo *child_info;

          child_enum = g_file_enumerate_children (file,
                                                  G_FILE_ATTRIBUTE_STANDARD_NAME,
                                                  G_FILE_QUERY_INFO_NONE,
                                                  NULL,
                                                  NULL);

          if (child_enum)
            {
              child_info = g_file_enumerator_next_file (child_enum, NULL, NULL);
              if (child_info)
                g_object_unref (child_info);
              else
                result = FALSE;
              g_object_unref (child_enum);
            }
          else
            result = FALSE;
        }

      if (result && matchlet->mimetype)
        {
          if (strcmp (matchlet->mimetype, g_file_info_get_content_type (info)) != 0)
            result = FALSE;
        }

      if (info)
        g_object_unref (info);
      g_object_unref (file);
    }
  while (!result);

  enumerator_free (e);

  if (!matchlet->matches)
    return TRUE;

  for (l = matchlet->matches; l; l = l->next)
    {
      TreeMatchlet *submatchlet;

      submatchlet = l->data;
      if (matchlet_match (submatchlet, root))
        return TRUE;
    }

  return FALSE;
}

static void
match_match (TreeMatch    *match,
             GFile        *root,
             GPtrArray    *types)
{
  GList *l;

  for (l = match->matches; l; l = l->next)
    {
      TreeMatchlet *matchlet = l->data;
      if (matchlet_match (matchlet, root))
        {
          g_ptr_array_add (types, g_strdup (match->contenttype));
          break;
        }
    }
}

gchar **
_g_generic_content_type_guess_for_tree (GFile *root)
{
  GPtrArray *types;
  GList *l;

  types = g_ptr_array_new ();

  G_LOCK (gio_treemagic);

  tree_magic_init ();
  for (l = tree_matches; l; l = l->next)
    {
      TreeMatch *match = l->data;
      match_match (match, root, types);
    }

  G_UNLOCK (gio_treemagic);

  g_ptr_array_add (types, NULL);

  return (gchar **)g_ptr_array_free (types, FALSE);
}
