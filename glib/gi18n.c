/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1998  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GLib Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GLib Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GLib at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "gi18n.h"
#include "galias.h"
#include "gmessages.h"
#include "galloca.h"
#include <string.h>
#include <locale.h>

static gboolean g_should_translate = TRUE;

void
g_i18n_init (void)
{
  gchar *domain, *default_domain;

  setlocale (LC_ALL, "");
  domain = g_strdup (textdomain (NULL));
  default_domain = g_strdup (textdomain (""));
  textdomain (domain);

  if (!strcmp (domain, default_domain))
    g_warning ("textdomain() must be called before glib i18n initialization");

  g_free (domain);
  g_free (default_domain);

  if (!*gettext (""))
    {
      g_should_translate = FALSE;
      g_warning ("No translation is available for the requested locale.");
    }
}

/**
 * g_gettext:
 * @msgid: message to translate
 *
 * This function is a wrapper of gettext() which does not not translate
 * the message if the application who initialized glib i18n is not translated.
 *
 * Applications should normally not use this function directly,
 * but use the _() macro for translations.
 *
 * Returns: The translated string
 *
 * Since: 2.16
 */
const gchar *
g_gettext (const gchar *msgid)
{
  if (g_should_translate)
    return gettext (msgid);
  else
    return msgid;
}

/**
 * g_dgettext:
 * @domain: the translation domain to use, or %NULL to use
 *   the domain set with textdomain()
 * @msgid: message to translate
 *
 * This function is a wrapper of dgettext() which does not not translate
 * the message if the application who initialized glib i18n is not translated.
 *
 * Applications should normally not use this function directly,
 * but use the _() macro for translations.
 *
 * Returns: The translated string
 *
 * Since: 2.16
 */
const gchar *
g_dgettext (const gchar *domain,
            const gchar *msgid)
{
  if (g_should_translate)
    return dgettext (domain, msgid);
  else
    return msgid;
}

/**
 * g_dpgettext:
 * @domain: the translation domain to use, or %NULL to use
 *   the domain set with textdomain()
 * @msgctxtid: a combined message context and message id, separated
 *   by a \004 character
 * @msgidoffset: the offset of the message id in @msgctxid
 *
 * This function is a variant of dgettext() which supports
 * a disambiguating message context. GNU gettext uses the
 * '\004' character to separate the message context and
 * message id in @msgctxtid.
 * If 0 is passed as @msgidoffset, this function will fall back to
 * trying to use the deprecated convention of using "|" as a separation
 * character.
 *
 * Applications should normally not use this function directly,
 * but use the C_() macro for translations with context.
 *
 * Returns: The translated string
 *
 * Since: 2.16
 */
const gchar *
g_dpgettext (const gchar *domain, 
             const gchar *msgctxtid, 
             gsize        msgidoffset)
{
  const gchar *translation;
  gchar *sep;

  translation = g_dgettext (domain, msgctxtid);

  if (translation == msgctxtid)
    {
      if (msgidoffset > 0)
        return msgctxtid + msgidoffset;

      sep = strchr (msgctxtid, '|');
 
      if (sep)
        {
          /* try with '\004' instead of '|', in case
           * xgettext -kQ_:1g was used
           */
          gchar *tmp = g_alloca (strlen (msgctxtid) + 1);
          strcpy (tmp, msgctxtid);
          tmp[sep - msgctxtid] = '\004';

          translation = g_dgettext (domain, tmp);
   
          if (translation == tmp)
            return sep + 1; 
        }
    }

  return translation;
}

#define __G_I18N_C__
#include "galiasdef.c"
