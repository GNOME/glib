/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright (C) 2014 Patrick Griffis
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
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "config.h"

#include "gcontenttype.h"
#include "gicon.h"
#include "gthemedicon.h"

#include <CoreServices/CoreServices.h>


/*< internal >
 * create_cfstring_from_cstr:
 * @cstr: a #gchar
 *
 * Converts a cstr to a utf8 cfstring
 * It must be CFReleased()'d.
 *
 */
static CFStringRef
create_cfstring_from_cstr (const gchar *cstr)
{
  return CFStringCreateWithCString (NULL, cstr, kCFStringEncodingUTF8);
}

/*< internal >
 * create_cstr_from_cfstring:
 * @str: a #CFStringRef
 *
 * Converts a cfstring to a utf8 cstring.
 * The incoming cfstring is released for you.
 * The returned string must be g_free()'d.
 *
 */
static gchar *
create_cstr_from_cfstring (CFStringRef str)
{
  const gchar *cstr;

  if (str == NULL)
    return NULL;

  cstr = CFStringGetCStringPtr (str, kCFStringEncodingUTF8);
  CFRelease (str);

  return g_strdup (cstr);
}

/*< internal >
 * create_cstr_from_cfstring_with_fallback:
 * @str: a #CFStringRef
 * @fallback: a #gchar
 *
 * Tries to convert a cfstring to a utf8 cstring.
 * If @str is NULL or conversion fails @fallback is returned.
 * The incoming cfstring is released for you.
 * The returned string must be g_free()'d.
 *
 */
static gchar *
create_cstr_from_cfstring_with_fallback (CFStringRef  str,
                                         const gchar *fallback)
{
  gchar *cstr;

  cstr = create_cstr_from_cfstring (str);
  if (!cstr)
    return g_strdup (fallback);

  return cstr;
}

gboolean
g_content_type_equals (const gchar *type1,
                       const gchar *type2)
{
  CFStringRef str1, str2;
  gboolean ret;

  g_return_val_if_fail (type1 != NULL, FALSE);
  g_return_val_if_fail (type2 != NULL, FALSE);

  if (g_ascii_strcasecmp (type1, type2) == 0)
    return TRUE;

  str1 = create_cfstring_from_cstr (type1);
  str2 = create_cfstring_from_cstr (type2);

  ret = UTTypeEqual (str1, str2);

  CFRelease (str1);
  CFRelease (str2);

  return ret;
}

gboolean
g_content_type_is_a (const gchar *ctype,
                     const gchar *csupertype)
{
  CFStringRef type, supertype;
  gboolean ret;

  g_return_val_if_fail (ctype != NULL, FALSE);
  g_return_val_if_fail (csupertype != NULL, FALSE);

  type = create_cfstring_from_cstr (ctype);
  supertype = create_cfstring_from_cstr (csupertype);

  ret = UTTypeConformsTo (type, supertype);

  CFRelease (type);
  CFRelease (supertype);

  return ret;
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

  /* Should dynamic types be considered "unknown"? */
  if (g_str_has_prefix (type, "dyn."))
    return TRUE;
  /* application/octet-stream */
  else if (g_strcmp0 (type, "public.data") == 0)
    return TRUE;

  return FALSE;
}

gchar *
g_content_type_get_description (const gchar *type)
{
  CFStringRef str;
  CFStringRef desc_str;

  g_return_val_if_fail (type != NULL, NULL);

  str = create_cfstring_from_cstr (type);
  desc_str = UTTypeCopyDescription (str);

  CFRelease (str);
  return create_cstr_from_cfstring_with_fallback (desc_str, "unknown");
}

static GIcon *
g_content_type_get_icon_internal (const gchar *type,
                                  gboolean     symbolic)
{
  GIcon *icon = NULL;
  gchar *name;

  g_return_val_if_fail (type != NULL, NULL);

  /* TODO: Show mimetype icons. */
  if (g_content_type_can_be_executable (type))
    name = "gtk-execute";
  else if (g_content_type_is_a (type, "public.directory"))
    name = symbolic ? "inode-directory-symbolic" : "inode-directory";
  else
    name = "gtk-file";

  icon = g_themed_icon_new_with_default_fallbacks (name);

  return icon;
}

GIcon *
g_content_type_get_icon (const gchar *type)
{
  return g_content_type_get_icon_internal (type, FALSE);
}

GIcon *
g_content_type_get_symbolic_icon (const gchar *type)
{
  return g_content_type_get_icon_internal (type, TRUE);
}

gchar *
g_content_type_get_generic_icon_name (const gchar *type)
{
  return NULL;
}

gboolean
g_content_type_can_be_executable (const gchar *type)
{
  CFStringRef uti;
  gboolean ret = FALSE;

  g_return_val_if_fail (type != NULL, FALSE);

  uti = create_cfstring_from_cstr (type);

  if (UTTypeConformsTo (uti, kUTTypeApplication))
    ret = TRUE;
  else if (UTTypeConformsTo (uti, CFSTR("public.executable")))
    ret = TRUE;
  else if (UTTypeConformsTo (uti, CFSTR("public.script")))
    ret = TRUE;
  /* Our tests assert that all text can be executable... */
  else if (UTTypeConformsTo (uti, CFSTR("public.text")))
      ret = TRUE;

  CFRelease (uti);
  return ret;
}

gchar *
g_content_type_from_mime_type (const gchar *mime_type)
{
  CFStringRef mime_str;
  CFStringRef uti_str;

  g_return_val_if_fail (mime_type != NULL, NULL);

  /* Their api does not handle globs but they are common. */
  if (g_str_has_suffix (mime_type, "*"))
    {
      if (g_str_has_prefix (mime_type, "audio"))
        return g_strdup ("public.audio");
      if (g_str_has_prefix (mime_type, "image"))
        return g_strdup ("public.image");
      if (g_str_has_prefix (mime_type, "text"))
        return g_strdup ("public.text");
      if (g_str_has_prefix (mime_type, "video"))
        return g_strdup ("public.movie");
    }

  /* Some exceptions are needed for gdk-pixbuf.
   * This list is not exhaustive.
   */
  if (g_str_has_prefix (mime_type, "image"))
    {
      if (g_str_has_suffix (mime_type, "x-icns"))
        return g_strdup ("com.apple.icns");
      if (g_str_has_suffix (mime_type, "x-tga"))
        return g_strdup ("com.truevision.tga-image");
      if (g_str_has_suffix (mime_type, "x-ico"))
        return g_strdup ("com.microsoft.ico ");
    }

  /* These are also not supported...
   * Used in glocalfileinfo.c
   */
  if (g_str_has_prefix (mime_type, "inode"))
    {
      if (g_str_has_suffix (mime_type, "directory"))
        return g_strdup ("public.folder");
      if (g_str_has_suffix (mime_type, "symlink"))
        return g_strdup ("public.symlink");
    }

  /* This is correct according to the Apple docs:
     https://developer.apple.com/library/content/documentation/Miscellaneous/Reference/UTIRef/Articles/System-DeclaredUniformTypeIdentifiers.html
  */
  if (strcmp (mime_type, "text/plain") == 0)
    return g_strdup ("public.text");

  /* Non standard type */
  if (strcmp (mime_type, "application/x-executable") == 0)
    return g_strdup ("public.executable");

  mime_str = create_cfstring_from_cstr (mime_type);
  uti_str = UTTypeCreatePreferredIdentifierForTag (kUTTagClassMIMEType, mime_str, NULL);

  CFRelease (mime_str);
  return create_cstr_from_cfstring_with_fallback (uti_str, "public.data");
}

gchar *
g_content_type_get_mime_type (const gchar *type)
{
  CFStringRef uti_str;
  CFStringRef mime_str;

  g_return_val_if_fail (type != NULL, NULL);

  /* We must match the additions above
   * so conversions back and forth work.
   */
  if (g_str_has_prefix (type, "public"))
    {
      if (g_str_has_suffix (type, ".image"))
        return g_strdup ("image/*");
      if (g_str_has_suffix (type, ".movie"))
        return g_strdup ("video/*");
      if (g_str_has_suffix (type, ".text"))
        return g_strdup ("text/*");
      if (g_str_has_suffix (type, ".audio"))
        return g_strdup ("audio/*");
      if (g_str_has_suffix (type, ".folder"))
        return g_strdup ("inode/directory");
      if (g_str_has_suffix (type, ".symlink"))
        return g_strdup ("inode/symlink");
      if (g_str_has_suffix (type, ".executable"))
        return g_strdup ("application/x-executable");
    }

  uti_str = create_cfstring_from_cstr (type);
  mime_str = UTTypeCopyPreferredTagWithClass(uti_str, kUTTagClassMIMEType);

  CFRelease (uti_str);
  return create_cstr_from_cfstring_with_fallback (mime_str, "application/octet-stream");
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
      if (g_ascii_iscntrl (c) && !g_ascii_isspace (c) && c != '\b')
        return FALSE;
    }
  return TRUE;
}

gchar *
g_content_type_guess (const gchar  *filename,
                      const guchar *data,
                      gsize         data_size,
                      gboolean     *result_uncertain)
{
  CFStringRef uti = NULL;
  gchar *cextension;
  CFStringRef extension;
  int uncertain = -1;

  g_return_val_if_fail (data_size != (gsize) -1, NULL);

  if (filename && *filename)
    {
      gchar *basename = g_path_get_basename (filename);
      gchar *dirname = g_path_get_dirname (filename);
      gsize i = strlen (filename);

      if (filename[i - 1] == '/')
        {
          if (g_strcmp0 (dirname, "/Volumes") == 0)
            {
              uti = CFStringCreateCopy (NULL, kUTTypeVolume);
            }
          else if ((cextension = strrchr (basename, '.')) != NULL)
            {
              cextension++;
              extension = create_cfstring_from_cstr (cextension);
              uti = UTTypeCreatePreferredIdentifierForTag (kUTTagClassFilenameExtension,
                                                           extension, NULL);
              CFRelease (extension);

              if (CFStringHasPrefix (uti, CFSTR ("dyn.")))
                {
                  CFRelease (uti);
                  uti = CFStringCreateCopy (NULL, kUTTypeFolder);
                  uncertain = TRUE;
                }
            }
          else
            {
              uti = CFStringCreateCopy (NULL, kUTTypeFolder);
              uncertain = TRUE; /* Matches Unix backend */
            }
        }
      else
        {
          /* GTK needs this... */
          if (g_str_has_suffix (basename, ".ui"))
            {
              uti = CFStringCreateCopy (NULL, kUTTypeXML);
            }
          else if (g_str_has_suffix (basename, ".txt"))
            {
              uti = CFStringCreateCopy (NULL, CFSTR ("public.text"));
            }
          else if ((cextension = strrchr (basename, '.')) != NULL)
            {
              cextension++;
              extension = create_cfstring_from_cstr (cextension);
              uti = UTTypeCreatePreferredIdentifierForTag (kUTTagClassFilenameExtension,
                                                           extension, NULL);
              CFRelease (extension);
            }
          g_free (basename);
          g_free (dirname);
        }
    }
  if (data && (!filename || !uti ||
               CFStringCompare (uti, CFSTR ("public.data"), 0) == kCFCompareEqualTo))
    {
      if (looks_like_text (data, data_size))
        {
          if (g_str_has_prefix ((const gchar*)data, "#!/"))
            uti = CFStringCreateCopy (NULL, CFSTR ("public.script"));
          else
            uti = CFStringCreateCopy (NULL, CFSTR ("public.text"));
        }
    }

  if (!uti)
    {
      /* Generic data type */
      uti = CFStringCreateCopy (NULL, CFSTR ("public.data"));
      if (result_uncertain)
        *result_uncertain = TRUE;
    }
  else if (result_uncertain)
    {
      *result_uncertain = uncertain == -1 ? FALSE : uncertain;
    }

  return create_cstr_from_cfstring (uti);
}

GList *
g_content_types_get_registered (void)
{
  /* TODO: UTTypeCreateAllIdentifiersForTag? */
  return NULL;
}

gchar **
g_content_type_guess_for_tree (GFile *root)
{
  return NULL;
}
