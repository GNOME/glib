/* guniprop.c - Unicode character properties.
 *
 * Copyright (C) 1999 Tom Tromey
 * Copyright (C) 2000 Red Hat, Inc.
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

#include "glib.h"
#include "gunichartables.h"

#include <config.h>

#include <stddef.h>

#define asize(x)  ((sizeof (x)) / sizeof (x[0]))

#define ATTTABLE(Page, Char) \
  ((attr_table[Page] == 0) ? 0 : (attr_table[Page][Char]))

/* We cheat a bit and cast type values to (char *).  We detect these
   using the &0xff trick.  */
#define TTYPE(Page, Char) \
  (((((int) type_table[Page]) & 0xff) == ((int) type_table[Page])) \
   ? ((int) (type_table[Page])) \
   : (type_table[Page][Char]))

#define TYPE(Char) (((Char) > (G_UNICODE_LAST_CHAR)) ? G_UNICODE_UNASSIGNED : TTYPE ((Char) >> 8, (Char) & 0xff))

#define ISDIGIT(Type) ((Type) == G_UNICODE_DECIMAL_NUMBER \
		       || (Type) == G_UNICODE_LETTER_NUMBER \
		       || (Type) == G_UNICODE_OTHER_NUMBER)

#define ISALPHA(Type) ((Type) == G_UNICODE_LOWERCASE_LETTER \
		       || (Type) == G_UNICODE_UPPERCASE_LETTER \
		       || (Type) == G_UNICODE_TITLECASE_LETTER \
		       || (Type) == G_UNICODE_MODIFIER_LETTER \
		       || (Type) == G_UNICODE_OTHER_LETTER)

gboolean
g_unichar_isalnum (gunichar c)
{
  int t = TYPE (c);
  return ISDIGIT (t) || ISALPHA (t);
}

gboolean
g_unichar_isalpha (gunichar c)
{
  int t = TYPE (c);
  return ISALPHA (t);
}

gboolean
g_unichar_iscntrl (gunichar c)
{
  return TYPE (c) == G_UNICODE_CONTROL;
}

gboolean
g_unichar_isdigit (gunichar c)
{
  return TYPE (c) == G_UNICODE_DECIMAL_NUMBER;
}

gboolean
g_unichar_isgraph (gunichar c)
{
  int t = TYPE (c);
  return (t != G_UNICODE_CONTROL
	  && t != G_UNICODE_FORMAT
	  && t != G_UNICODE_UNASSIGNED
	  && t != G_UNICODE_PRIVATE_USE
	  && t != G_UNICODE_SURROGATE
	  && t != G_UNICODE_SPACE_SEPARATOR);
}

gboolean
g_unichar_islower (gunichar c)
{
  return TYPE (c) == G_UNICODE_LOWERCASE_LETTER;
}

gboolean
g_unichar_isprint (gunichar c)
{
  int t = TYPE (c);
  return (t != G_UNICODE_CONTROL
	  && t != G_UNICODE_FORMAT
	  && t != G_UNICODE_UNASSIGNED
	  && t != G_UNICODE_PRIVATE_USE
	  && t != G_UNICODE_SURROGATE);
}

gboolean
g_unichar_ispunct (gunichar c)
{
  int t = TYPE (c);
  return (t == G_UNICODE_CONNECT_PUNCTUATION || t == G_UNICODE_DASH_PUNCTUATION
	  || t == G_UNICODE_CLOSE_PUNCTUATION || t == G_UNICODE_FINAL_PUNCTUATION
	  || t == G_UNICODE_INITIAL_PUNCTUATION || t == G_UNICODE_OTHER_PUNCTUATION
	  || t == G_UNICODE_OPEN_PUNCTUATION);
}

gboolean
g_unichar_isspace (gunichar c)
{
  int t = TYPE (c);
  return (t == G_UNICODE_SPACE_SEPARATOR || t == G_UNICODE_LINE_SEPARATOR
	  || t == G_UNICODE_PARAGRAPH_SEPARATOR);
}

/**
 * g_unichar_isupper:
 * @c: a unicode character
 * 
 * Determines if a character is uppercase.
 * 
 * Return value: 
 **/
gboolean
g_unichar_isupper (gunichar c)
{
  return TYPE (c) == G_UNICODE_UPPERCASE_LETTER;
}

/**
 * g_unichar_istitle:
 * @c: a unicode character
 * 
 * Determines if a character is titlecase. Some characters in
 * Unicode which are composites, such as the DZ digraph
 * have three case variants instead of just two. The titlecase
 * form is used at the beginning of a word where only the
 * first letter is capitalized. The titlecase form of the DZ
 * digraph is U+01F2 LATIN CAPITAL LETTTER D WITH SMALL LETTER Z
 * 
 * Return value: %TRUE if the character is titlecase.
 **/
gboolean
g_unichar_istitle (gunichar c)
{
  unsigned int i;
  for (i = 0; i < asize (title_table); ++i)
    if (title_table[i][0] == c)
      return 1;
  return 0;
}

/**
 * g_unichar_isxdigit:
 * @c: a unicode character.
 * 
 * Determines if a characters is a hexidecimal digit
 * 
 * Return value: %TRUE if the character is a hexidecimal digit.
 **/
gboolean
g_unichar_isxdigit (gunichar c)
{
  int t = TYPE (c);
  return ((c >= 'a' && c <= 'f')
	  || (c >= 'A' && c <= 'F')
	  || ISDIGIT (t));
}

/**
 * g_unichar_isdefined:
 * @c: a unicode character
 * 
 * Determines if a given character is assigned in the Unicode
 * standard
 *
 * Return value: %TRUE if the character has an assigned value.
 **/
gboolean
g_unichar_isdefined (gunichar c)
{
  int t = TYPE (c);
  return t != G_UNICODE_UNASSIGNED;
}

/**
 * g_unichar_iswide:
 * @c: a unicode character
 * 
 * Determines if a character is typically rendered in a double-width
 * cell.
 * 
 * Return value: %TRUE if the character is wide.
 **/
/* This function stolen from Markus Kuhn <Markus.Kuhn@cl.cam.ac.uk>.  */
gboolean
g_unichar_iswide (gunichar c)
{
  if (c < 0x1100)
    return 0;

  return ((c >= 0x1100 && c <= 0x115f)	   /* Hangul Jamo */
	  || (c >= 0x2e80 && c <= 0xa4cf && (c & ~0x0011) != 0x300a &&
	      c != 0x303f)		   /* CJK ... Yi */
	  || (c >= 0xac00 && c <= 0xd7a3)  /* Hangul Syllables */
	  || (c >= 0xf900 && c <= 0xfaff)  /* CJK Compatibility Ideographs */
	  || (c >= 0xfe30 && c <= 0xfe6f)  /* CJK Compatibility Forms */
	  || (c >= 0xff00 && c <= 0xff5f)  /* Fullwidth Forms */
	  || (c >= 0xffe0 && c <= 0xffe6));
}

/**
 * g_unichar_toupper:
 * @c: a unicode character
 * 
 * Convert a character to uppercase.
 * 
 * Return value: the result of converting @c to uppercase.
 *               If @c is not an lowercase or titlecase character,
 *               @c is returned unchanged.
 **/
gunichar
g_unichar_toupper (gunichar c)
{
  int t = TYPE (c);
  if (t == G_UNICODE_LOWERCASE_LETTER)
    return ATTTABLE (c >> 8, c & 0xff);
  else if (t == G_UNICODE_TITLECASE_LETTER)
    {
      unsigned int i;
      for (i = 0; i < asize (title_table); ++i)
	{
	  if (title_table[i][0] == c)
	    return title_table[i][1];
	}
    }
  return c;
}

/**
 * g_unichar_tolower:
 * @c: a unicode character.
 * 
 * Convert a character to lower case
 * 
e * Return value: the result of converting @c to lower case.
 *               If @c is not an upperlower or titlecase character,
 *               @c is returned unchanged.
 **/
gunichar
g_unichar_tolower (gunichar c)
{
  int t = TYPE (c);
  if (t == G_UNICODE_UPPERCASE_LETTER)
    return ATTTABLE (c >> 8, c & 0xff);
  else if (t == G_UNICODE_TITLECASE_LETTER)
    {
      unsigned int i;
      for (i = 0; i < asize (title_table); ++i)
	{
	  if (title_table[i][0] == c)
	    return title_table[i][2];
	}
    }
  return c;
}

/**
 * g_unichar_totitle:
 * @c: a unicode character
 * 
 * Convert a character to the titlecase
 * 
 * Return value: the result of converting @c to titlecase.
 *               If @c is not an uppercase or lowercase character,
 *               @c is returned unchanged.
 **/
gunichar
g_unichar_totitle (gunichar c)
{
  unsigned int i;
  for (i = 0; i < asize (title_table); ++i)
    {
      if (title_table[i][0] == c || title_table[i][1] == c
	  || title_table[i][2] == c)
	return title_table[i][0];
    }
  return (TYPE (c) == G_UNICODE_LOWERCASE_LETTER
	  ? ATTTABLE (c >> 8, c & 0xff)
	  : c);
}

/**
 * g_unichar_xdigit_value:
 * @c: a unicode character
 *
 * Determines the numeric value of a character as a decimal
 * degital.
 *
 * Return value: If @c is a decimal digit (according to
 * `g_unichar_isdigit'), its numeric value. Otherwise, -1.
 **/
int
g_unichar_digit_value (gunichar c)
{
  if (TYPE (c) == G_UNICODE_DECIMAL_NUMBER)
    return ATTTABLE (c >> 8, c & 0xff);
  return -1;
}

/**
 * g_unichar_xdigit_value:
 * @c: a unicode character
 *
 * Determines the numeric value of a character as a hexidecimal
 * degital.
 *
 * Return value: If @c is a hex digit (according to
 * `g_unichar_isxdigit'), its numeric value. Otherwise, -1.
 **/
int
g_unichar_xdigit_value (gunichar c)
{
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 1;
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 1;
  if (TYPE (c) == G_UNICODE_DECIMAL_NUMBER)
    return ATTTABLE (c >> 8, c & 0xff);
  return -1;
}

/**
 * g_unichar_type:
 * @c: a unicode character
 * 
 * Classifies a unicode character by type.
 * 
 * Return value: the typ of the character.
 **/
GUnicodeType
g_unichar_type (gunichar c)
{
  return TYPE (c);
}
