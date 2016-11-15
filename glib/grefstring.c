/* GLib - Library of useful routines for C programming
 * Copyright 2016  Emmanuele Bassi
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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

/*
 * MT safe
 */

#include "config.h"

#include "grefstring.h"

#include <string.h>

#include "gmessages.h"
#include "grefcount.h"

char *
g_ref_string_new (const char *str)
{
  gsize len;
  char *res;

  g_return_val_if_fail (str != NULL || *str != '\0', NULL);

  len = strlen (str);
  res = g_ref_alloc (len + 1, NULL);

  memcpy (res, str, len);
  res[len] = '\0';

  return res;
}

char *
g_ref_string_ref (char *str)
{
  g_return_val_if_fail (str != NULL, NULL);

  g_ref_acquire (str);

  return str;
}

void
g_ref_string_unref (char *str)
{
  g_return_if_fail (str != NULL);

  g_ref_release (str);
}
