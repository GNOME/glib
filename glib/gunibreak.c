/* gunibreak.c - line break properties
 *
 *  Copyright 2000 Red Hat, Inc.
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
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <stdlib.h>

#ifndef HAVE_LIBICU

#include "gunibreak.h"

#define TPROP_PART1(Page, Char) \
  ((break_property_table_part1[Page] >= G_UNICODE_MAX_TABLE_INDEX) \
   ? (break_property_table_part1[Page] - G_UNICODE_MAX_TABLE_INDEX) \
   : (break_property_data[break_property_table_part1[Page]][Char]))

#define TPROP_PART2(Page, Char) \
  ((break_property_table_part2[Page] >= G_UNICODE_MAX_TABLE_INDEX) \
   ? (break_property_table_part2[Page] - G_UNICODE_MAX_TABLE_INDEX) \
   : (break_property_data[break_property_table_part2[Page]][Char]))

#define PROP(Char) \
  (((Char) <= G_UNICODE_LAST_CHAR_PART1) \
   ? TPROP_PART1 ((Char) >> 8, (Char) & 0xff) \
   : (((Char) >= 0xe0000 && (Char) <= G_UNICODE_LAST_CHAR) \
      ? TPROP_PART2 (((Char) - 0xe0000) >> 8, (Char) & 0xff) \
      : G_UNICODE_BREAK_UNKNOWN))

#else /* HAVE_LIBICU */

#include <unicode/uchar.h>
#include "gunicode.h"

static GUnicodeBreakType
u_line_break_to_g_unicode_break_type (ULineBreak code)
{
  switch (code)
    {
    case U_LB_UNKNOWN: /*[XX]*/
      return G_UNICODE_BREAK_UNKNOWN;
    case U_LB_AMBIGUOUS: /*[AI]*/
      return G_UNICODE_BREAK_AMBIGUOUS;
    case U_LB_ALPHABETIC: /*[AL]*/
      return G_UNICODE_BREAK_ALPHABETIC;
    case U_LB_BREAK_BOTH: /*[B2]*/
      return G_UNICODE_BREAK_BEFORE_AND_AFTER;
    case U_LB_BREAK_AFTER: /*[BA]*/
      return G_UNICODE_BREAK_AFTER;
    case U_LB_BREAK_BEFORE: /*[BB]*/
      return G_UNICODE_BREAK_BEFORE;
    case U_LB_MANDATORY_BREAK: /*[BK]*/
      return G_UNICODE_BREAK_MANDATORY;
    case U_LB_CONTINGENT_BREAK: /*[CB]*/
      return G_UNICODE_BREAK_CONTINGENT;
    case U_LB_CLOSE_PUNCTUATION: /*[CL]*/
      return G_UNICODE_BREAK_CLOSE_PUNCTUATION;
    case U_LB_COMBINING_MARK: /*[CM]*/
      return G_UNICODE_BREAK_COMBINING_MARK;
    case U_LB_CARRIAGE_RETURN: /*[CR]*/
      return G_UNICODE_BREAK_CARRIAGE_RETURN;
    case U_LB_EXCLAMATION: /*[EX]*/
      return G_UNICODE_BREAK_EXCLAMATION;
    case U_LB_GLUE: /*[GL]*/
      return G_UNICODE_BREAK_NON_BREAKING_GLUE;
    case U_LB_HYPHEN: /*[HY]*/
      return G_UNICODE_BREAK_HYPHEN;
    case U_LB_IDEOGRAPHIC: /*[ID]*/
      return G_UNICODE_BREAK_IDEOGRAPHIC;
    case U_LB_INSEPARABLE: /*[IN]*/
      return G_UNICODE_BREAK_INSEPARABLE;
    case U_LB_INFIX_NUMERIC: /*[IS]*/
      return G_UNICODE_BREAK_INFIX_SEPARATOR;
    case U_LB_LINE_FEED: /*[LF]*/
      return G_UNICODE_BREAK_LINE_FEED;
    case U_LB_NONSTARTER: /*[NS]*/
      return G_UNICODE_BREAK_NON_STARTER;
    case U_LB_NUMERIC: /*[NU]*/
      return G_UNICODE_BREAK_NUMERIC;
    case U_LB_OPEN_PUNCTUATION: /*[OP]*/
      return G_UNICODE_BREAK_OPEN_PUNCTUATION;
    case U_LB_POSTFIX_NUMERIC: /*[PO]*/
      return G_UNICODE_BREAK_POSTFIX;
    case U_LB_PREFIX_NUMERIC: /*[PR]*/
      return G_UNICODE_BREAK_PREFIX;
    case U_LB_QUOTATION: /*[QU]*/
      return G_UNICODE_BREAK_QUOTATION;
    case U_LB_COMPLEX_CONTEXT: /*[SA]*/
      return G_UNICODE_BREAK_COMPLEX_CONTEXT;
    case U_LB_SURROGATE: /*[SG]*/
      return G_UNICODE_BREAK_SURROGATE;
    case U_LB_SPACE: /*[SP]*/
      return G_UNICODE_BREAK_SPACE;
    case U_LB_BREAK_SYMBOLS: /*[SY]*/
      return G_UNICODE_BREAK_SYMBOL;
    case U_LB_ZWSPACE: /*[ZW]*/
      return G_UNICODE_BREAK_ZERO_WIDTH_SPACE;
    case U_LB_NEXT_LINE: /*[NL]*/
      return G_UNICODE_BREAK_NEXT_LINE;
    case U_LB_WORD_JOINER: /*[WJ]*/
      return G_UNICODE_BREAK_WORD_JOINER;
    case U_LB_H2: /*[H2]*/
      return G_UNICODE_BREAK_HANGUL_LV_SYLLABLE;
    case U_LB_H3: /*[H3]*/
      return G_UNICODE_BREAK_HANGUL_LVT_SYLLABLE;
    case U_LB_JL: /*[JL]*/
      return G_UNICODE_BREAK_HANGUL_L_JAMO;
    case U_LB_JT: /*[JT]*/
      return G_UNICODE_BREAK_HANGUL_T_JAMO;
    case U_LB_JV: /*[JV]*/
      return G_UNICODE_BREAK_HANGUL_V_JAMO;
    case U_LB_CLOSE_PARENTHESIS: /*[CP]*/
      return G_UNICODE_BREAK_CLOSE_PARANTHESIS;
    case U_LB_CONDITIONAL_JAPANESE_STARTER: /*[CJ]*/
      return G_UNICODE_BREAK_CONDITIONAL_JAPANESE_STARTER;
    case U_LB_HEBREW_LETTER: /*[HL]*/
      return G_UNICODE_BREAK_HEBREW_LETTER;
    case U_LB_REGIONAL_INDICATOR: /*[RI]*/
      return G_UNICODE_BREAK_REGIONAL_INDICATOR;
    case U_LB_E_BASE: /*[EB]*/
      return G_UNICODE_BREAK_EMOJI_BASE;
    case U_LB_E_MODIFIER: /*[EM]*/
      return G_UNICODE_BREAK_EMOJI_MODIFIER;
    case U_LB_ZWJ: /*[ZWJ]*/
      return G_UNICODE_BREAK_ZERO_WIDTH_JOINER;
    case U_LB_COUNT:
      break;
    }
  return G_UNICODE_BREAK_UNKNOWN;
}
#endif /* HAVE_LIBICU */

/**
 * g_unichar_break_type:
 * @c: a Unicode character
 * 
 * Determines the break type of @c. @c should be a Unicode character
 * (to derive a character from UTF-8 encoded text, use
 * g_utf8_get_char()). The break type is used to find word and line
 * breaks ("text boundaries"), Pango implements the Unicode boundary
 * resolution algorithms and normally you would use a function such
 * as pango_break() instead of caring about break types yourself.
 * 
 * Returns: the break type of @c
 **/
GUnicodeBreakType
g_unichar_break_type (gunichar c)
{
#ifdef HAVE_LIBICU
  gint32 line_break;

  line_break = u_getIntPropertyValue (c, UCHAR_LINE_BREAK);
  return u_line_break_to_g_unicode_break_type (line_break);
#else /* !HAVE_LIBICU */
  return PROP (c);
#endif
}
