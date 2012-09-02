/* gunibreak.c - line break properties
 *
 *  Copyright 2000 Red Hat, Inc.
 *
 * The Gnome Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * The Gnome Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the Gnome Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 *   Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <stdlib.h>

#include "gunigraphemebreak.h"

#define TPROP_PART1(Page, Char) \
  ((grapheme_break_property_table_part1[Page] >= G_UNICODE_MAX_TABLE_INDEX) \
   ? (grapheme_break_property_table_part1[Page] - G_UNICODE_MAX_TABLE_INDEX) \
   : (grapheme_break_property_data[grapheme_break_property_table_part1[Page]][Char]))

#define TPROP_PART2(Page, Char) \
  ((grapheme_break_property_table_part2[Page] >= G_UNICODE_MAX_TABLE_INDEX) \
   ? (grapheme_break_property_table_part2[Page] - G_UNICODE_MAX_TABLE_INDEX) \
   : (grapheme_break_property_data[grapheme_break_property_table_part2[Page]][Char]))

#define PROP(Char) \
  (((Char) <= G_UNICODE_LAST_CHAR_PART1) \
   ? TPROP_PART1 ((Char) >> 8, (Char) & 0xff) \
   : (((Char) >= 0xe0000 && (Char) <= G_UNICODE_LAST_CHAR) \
      ? TPROP_PART2 (((Char) - 0xe0000) >> 8, (Char) & 0xff) \
      : G_UNICODE_BREAK_UNKNOWN))

/**
 * g_unichar_grapheme_cluster_break_type:
 * @c: a Unicode character
 *
 * Determines the grapeme cluster break type of @c.
 *
 * Return value: the grapheme cluster break type of @c
 *
 * Since: 2.36
 **/
GUnicodeGraphemeClusterBreakType
g_unichar_grapheme_cluster_break_type (gunichar c)
{
  return PROP (c);
}
