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
 *         Руслан Ижбулатов <lrn1986@gmail.com>
 */

#include "config.h"
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "gcontenttype.h"
#include "gcontenttypeprivate.h"
#include "gthemedicon.h"
#include "gicon.h"
#include "glibintl.h"

#include <windows.h>

#define XDG_PREFIX _gio_xdg
#include "xdgmime/xdgmime.h"

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
      (key_type == REG_SZ || key_type == REG_EXPAND_SZ))
    {
      wchar_t *wc_temp = g_new (wchar_t, (nbytes+1)/2 + 1);
      RegQueryValueExW (reg_key, key_name, 0,
                        &key_type, (LPBYTE) wc_temp, &nbytes);
      wc_temp[nbytes/2] = '\0';
      if (key_type == REG_EXPAND_SZ)
        {
          wchar_t dummy[1];
          int len = ExpandEnvironmentStringsW (wc_temp, dummy, 1);
          if (len > 0)
            {
              wchar_t *wc_temp_expanded = g_new (wchar_t, len);
              if (ExpandEnvironmentStringsW (wc_temp, wc_temp_expanded, len) == len)
                value_utf8 = g_utf16_to_utf8 (wc_temp_expanded, -1, NULL, NULL, NULL);
              g_free (wc_temp_expanded);
            }
        }
      else
        {
          value_utf8 = g_utf16_to_utf8 (wc_temp, -1, NULL, NULL, NULL);
        }
      g_free (wc_temp);
      
    }
  g_free (wc_key);
  
  if (reg_key != NULL)
    RegCloseKey (reg_key);

  return value_utf8;
}

gboolean
g_content_type_equals (const gchar *type1,
                       const gchar *type2)
{
  char *progid1, *progid2;
  gboolean res;
  
  g_return_val_if_fail (type1 != NULL, FALSE);
  g_return_val_if_fail (type2 != NULL, FALSE);

  if (_g_generic_content_type_equals (type1, type2))
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
g_content_type_is_a (const gchar *type,
                     const gchar *supertype)
{
  gboolean res;
  char *value_utf8;

  g_return_val_if_fail (type != NULL, FALSE);
  g_return_val_if_fail (supertype != NULL, FALSE);

  if (g_content_type_equals (type, supertype) ||
      _g_generic_content_type_is_a (type, supertype))
    return TRUE;

  res = FALSE;
  value_utf8 = get_registry_classes_key (type, L"PerceivedType");
  if (value_utf8 && strcmp (value_utf8, supertype) == 0)
    res = TRUE;
  g_free (value_utf8);
  
  return res;
}

gboolean
g_content_type_is_mime_type (const gchar *type,
                             const gchar *mime_type)
{
  gchar *content_type;
  gboolean ret;

  g_return_val_if_fail (type != NULL, FALSE);
  g_return_val_if_fail (mime_type != NULL, FALSE);

  content_type = g_content_type_from_mime_type (mime_type);
  ret = g_content_type_is_a (type, content_type);
  g_free (content_type);

  return ret;
}

gboolean
g_content_type_is_unknown (const gchar *type)
{
  g_return_val_if_fail (type != NULL, FALSE);

  if (_g_generic_content_type_is_unknown (type))
    return TRUE;

  return strcmp ("*", type) == 0;
}

gchar *
g_content_type_get_description (const gchar *type)
{
  char *progid;
  char *description;

  g_return_val_if_fail (type != NULL, NULL);

  /* Unknown check is very specific, do it first */
  if (g_content_type_is_unknown (type))
    return g_strdup (_("Unknown type"));

  description = _g_generic_content_type_get_description (type);

  if (description)
    return description;

  progid = get_registry_classes_key (type, NULL);
  if (progid)
    {
      description = get_registry_classes_key (progid, NULL);
      g_free (progid);

      if (description)
        return description;
    }

  return g_strdup_printf (_("%s filetype"), type);
}

gchar *
g_content_type_get_mime_type (const gchar *type)
{
  char *mime;

  g_return_val_if_fail (type != NULL, NULL);

  if (type[0] != '.')
    return _g_generic_content_type_get_mime_type (type);

  mime = get_registry_classes_key (type, L"Content Type");
  if (mime)
    return mime;
  else if (g_content_type_is_unknown (type))
    return g_strdup ("application/octet-stream");
  else if (*type == '.')
    return g_strdup_printf ("application/x-ext-%s", type+1);

  return g_strdup ("application/octet-stream");
}

static gboolean
extension_can_be_executable (const gchar *type)
{
  if (strcmp (type, ".exe") == 0 ||
      strcmp (type, ".com") == 0 ||
      strcmp (type, ".cmd") == 0 ||
      strcmp (type, ".bat") == 0)
    return TRUE;

  return FALSE;
}

G_LOCK_DEFINE_STATIC (_type_icons);
static GHashTable *_type_icons = NULL;

GIcon *
g_content_type_get_icon (const gchar *type)
{
  GIcon *themed_icon;
  char *name = NULL;

  g_return_val_if_fail (type != NULL, NULL);

  /* In the Registry icons are the default value of
     HKEY_CLASSES_ROOT\<progid>\DefaultIcon with typical values like:
     <type>: <value>
     REG_EXPAND_SZ: %SystemRoot%\System32\Wscript.exe,3
     REG_SZ: shimgvw.dll,3
  */
  G_LOCK (_type_icons);
  if (!_type_icons)
    _type_icons = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
  name = g_hash_table_lookup (_type_icons, type);
  if (!name && type[0] == '.')
    {
      /* double lookup by extension */
      gchar *key = get_registry_classes_key (type, NULL);
      if (!key)
        key = g_strconcat (type+1, "file\\DefaultIcon", NULL);
      else
        {
          gchar *key2 = g_strconcat (key, "\\DefaultIcon", NULL);
          g_free (key);
          key = key2;
        }
      name = get_registry_classes_key (key, NULL);
      if (name && strcmp (name, "%1") == 0)
        {
          g_free (name);
          name = NULL;
        }
      if (name)
        {
          /* We can't free name later on, because it could be a string owned
           * by the hash table. Ensure that it IS always a string owned by
           * the hash table.
           */
          char *name2 = g_strdup (name);
          g_free (name);
          name = name2;
          g_hash_table_insert (_type_icons, g_strdup (type), name);
        }
      g_free (key);
    }

  themed_icon = NULL;

  if (!name && type[0] != '.')
    themed_icon = _g_generic_content_type_get_icon (type);

  if (!themed_icon && !name)
    {
      /* if no icon found, fall back to standard generic names */
      if (strcmp (type, "inode/directory") == 0)
        name = "folder";
      else if (g_content_type_can_be_executable (type))
        name = "system-run";
      else
        name = "text-x-generic";
      g_hash_table_insert (_type_icons, g_strdup (type), g_strdup (name));
    }

  if (!themed_icon)
    themed_icon = g_themed_icon_new (name);

  G_UNLOCK (_type_icons);

  return G_ICON (themed_icon);
}

GIcon *
g_content_type_get_symbolic_icon (const gchar *type)
{
  return g_content_type_get_icon (type);
}

gchar *
g_content_type_get_generic_icon_name (const gchar *type)
{
  return NULL;
}

gboolean
g_content_type_can_be_executable (const gchar *type)
{
  g_return_val_if_fail (type != NULL, FALSE);

  if (extension_can_be_executable (type))
    return TRUE;

  /* TODO: Also look at PATHEXT, which lists the extensions for
   * "scripts" in addition to those for true binary executables.
   *
   * (PATHEXT=.COM;.EXE;.BAT;.CMD;.VBS;.VBE;.JS;.JSE;.WSF;.WSH for me
   * right now, for instance). And in a sense, all associated file
   * types are "executable" on Windows... You can just type foo.jpg as
   * a command name in cmd.exe, and it will run the application
   * associated with .jpg. Hard to say what this API actually means
   * with "executable".
   *
   *
   * 1) non-binary files can only be run by ShellExecute*(), not by
   * CreateProcess*().
   * 2) _spawn*() is documented to run .exe, .com, .bat and .cmd (or a PE file
   * with any extension, including zero-length one (filename ends in ".\0");
   * for files with no extension (ends in "\0") it tries to append .exe,
   * .bat and .com, then fails).
   * This means that glib has no code (AFAIK) to run anything other than
   * things listed above.
   * 3) security-wise "executable" means "can be programmed". JPG files
   * are not executable, since opening any JPG file will only ever run
   * one kind of code (image viewer, usually), and the decision as to which
   * code to run is made by the user (or whoever sets file associations),
   * not by the author of the JPG file.
   * Unless the viewer is vulnerable and JPG is a specially-formed file
   * with executable code, that is ;)
   */

  if (_g_generic_content_type_can_be_executable (type))
    return TRUE;

  return FALSE;
}

gchar *
g_content_type_from_mime_type (const gchar *mime_type)
{
  char *key, *content_type;

  g_return_val_if_fail (mime_type != NULL, NULL);

  content_type = NULL;
  if (mime_type[0] != '.')
    content_type = _g_generic_content_type_from_mime_type (mime_type);

  if (content_type == NULL)
    {
      key = g_strconcat ("MIME\\DataBase\\Content Type\\", mime_type, NULL);
      content_type = get_registry_classes_key (key, L"Extension");
      g_free (key);
    }

  return content_type;
}

gchar *
g_content_type_guess (const gchar  *filename,
                      const guchar *data,
                      gsize         data_size,
                      gboolean     *result_uncertain)
{
  gchar *content_type;
  gboolean uncertain;

  if (result_uncertain)
    *result_uncertain = FALSE;

  /* our test suite and potentially other code used -1 in the past, which is
   * not documented and not allowed; guard against that */
  g_return_val_if_fail (data_size != (gsize) -1, g_strdup ("*"));

  content_type = _g_generic_content_type_guess (filename, data, data_size, &uncertain);

  if (content_type == NULL)
    return NULL;

  if (result_uncertain)
    *result_uncertain = uncertain;

  return content_type;
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
  
  types = g_list_reverse (types);

  return g_list_concat (types, _g_generic_content_types_get_registered ());
}

gchar **
g_content_type_guess_for_tree (GFile *root)
{
  return _g_generic_content_type_guess_for_tree (root);
}
