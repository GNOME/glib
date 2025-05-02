/*
 * Copyright © 2025 Luca Bacci
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
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Luca Bacci <luca.bacci@outlook.com>
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>

#include "gprintprivate.h"
#include "gprintfint.h"

#define CHAR_IS_SAFE(wc) (!((wc < 0x20 && wc != '\t' && wc != '\n' && wc != '\r') || \
                            (wc == 0x7f) || \
                            (wc >= 0x80 && wc < 0xa0)))

char *
g_print_convert (const char *string,
                 const char *charset)
{
  if (!g_utf8_validate (string, -1, NULL))
    {
      GString *gstring = g_string_new ("[Invalid UTF-8] ");
      guchar *p;

      for (p = (guchar *)string; *p; p++)
        {
          if (CHAR_IS_SAFE(*p) &&
              !(*p == '\r' && *(p + 1) != '\n') &&
              *p < 0x80)
            g_string_append_c (gstring, *p);
          else
            g_string_append_printf (gstring, "\\x%02x", (guint)(guchar)*p);
        }

      return g_string_free (gstring, FALSE);
    }
  else
    {
      GError *err = NULL;

      gchar *result = g_convert_with_fallback (string, -1, charset, "UTF-8", "?", NULL, NULL, &err);
      if (result)
        return result;
      else
        {
          /* Not thread-safe, but doesn't matter if we print the warning twice
           */
          static gboolean warned = FALSE;
          if (!warned)
            {
              warned = TRUE;
              _g_fprintf (stderr, "GLib: Cannot convert message: %s\n", err->message);
            }
          g_error_free (err);

          return g_strdup (string);
        }
    }
}
