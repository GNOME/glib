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

#include "config.h"

#include <stddef.h>
#include <string.h>
#include <locale.h>

#include "glib.h"
#include "gunichartables.h"
#include "gunicodeprivate.h"

#define ATTR_TABLE(Page) (((Page) <= G_UNICODE_LAST_PAGE_PART1) \
                          ? attr_table_part1[Page] \
                          : attr_table_part2[(Page) - 0xe00])

#define ATTTABLE(Page, Char) \
  ((ATTR_TABLE(Page) == G_UNICODE_MAX_TABLE_INDEX) ? 0 : (attr_data[ATTR_TABLE(Page)][Char]))

#define TTYPE_PART1(Page, Char) \
  ((type_table_part1[Page] >= G_UNICODE_MAX_TABLE_INDEX) \
   ? (type_table_part1[Page] - G_UNICODE_MAX_TABLE_INDEX) \
   : (type_data[type_table_part1[Page]][Char]))

#define TTYPE_PART2(Page, Char) \
  ((type_table_part2[Page] >= G_UNICODE_MAX_TABLE_INDEX) \
   ? (type_table_part2[Page] - G_UNICODE_MAX_TABLE_INDEX) \
   : (type_data[type_table_part2[Page]][Char]))

#define TYPE(Char) \
  (((Char) <= G_UNICODE_LAST_CHAR_PART1) \
   ? TTYPE_PART1 ((Char) >> 8, (Char) & 0xff) \
   : (((Char) >= 0xe0000 && (Char) <= G_UNICODE_LAST_CHAR) \
      ? TTYPE_PART2 (((Char) - 0xe0000) >> 8, (Char) & 0xff) \
      : G_UNICODE_UNASSIGNED))


#define ISDIGIT(Type) ((Type) == G_UNICODE_DECIMAL_NUMBER	\
		       || (Type) == G_UNICODE_LETTER_NUMBER	\
		       || (Type) == G_UNICODE_OTHER_NUMBER)

#define ISALPHA(Type) ((Type) == G_UNICODE_LOWERCASE_LETTER	\
		       || (Type) == G_UNICODE_UPPERCASE_LETTER	\
		       || (Type) == G_UNICODE_TITLECASE_LETTER	\
		       || (Type) == G_UNICODE_MODIFIER_LETTER	\
		       || (Type) == G_UNICODE_OTHER_LETTER)

#define ISMARK(Type) ((Type) == G_UNICODE_NON_SPACING_MARK ||	\
		      (Type) == G_UNICODE_COMBINING_MARK ||	\
		      (Type) == G_UNICODE_ENCLOSING_MARK)
		      

/**
 * g_unichar_isalnum:
 * @c: a Unicode character
 * 
 * Determines whether a character is alphanumeric.
 * Given some UTF-8 text, obtain a character value
 * with g_utf8_get_char().
 * 
 * Return value: %TRUE if @c is an alphanumeric character
 **/
gboolean
g_unichar_isalnum (gunichar c)
{
  int t = TYPE (c);
  return ISDIGIT (t) || ISALPHA (t);
}

/**
 * g_unichar_isalpha:
 * @c: a Unicode character
 * 
 * Determines whether a character is alphabetic (i.e. a letter).
 * Given some UTF-8 text, obtain a character value with
 * g_utf8_get_char().
 * 
 * Return value: %TRUE if @c is an alphabetic character
 **/
gboolean
g_unichar_isalpha (gunichar c)
{
  int t = TYPE (c);
  return ISALPHA (t);
}


/**
 * g_unichar_iscntrl:
 * @c: a Unicode character
 * 
 * Determines whether a character is a control character.
 * Given some UTF-8 text, obtain a character value with
 * g_utf8_get_char().
 * 
 * Return value: %TRUE if @c is a control character
 **/
gboolean
g_unichar_iscntrl (gunichar c)
{
  return TYPE (c) == G_UNICODE_CONTROL;
}

/**
 * g_unichar_isdigit:
 * @c: a Unicode character
 * 
 * Determines whether a character is numeric (i.e. a digit).  This
 * covers ASCII 0-9 and also digits in other languages/scripts.  Given
 * some UTF-8 text, obtain a character value with g_utf8_get_char().
 * 
 * Return value: %TRUE if @c is a digit
 **/
gboolean
g_unichar_isdigit (gunichar c)
{
  return TYPE (c) == G_UNICODE_DECIMAL_NUMBER;
}


/**
 * g_unichar_isgraph:
 * @c: a Unicode character
 * 
 * Determines whether a character is printable and not a space
 * (returns %FALSE for control characters, format characters, and
 * spaces). g_unichar_isprint() is similar, but returns %TRUE for
 * spaces. Given some UTF-8 text, obtain a character value with
 * g_utf8_get_char().
 * 
 * Return value: %TRUE if @c is printable unless it's a space
 **/
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

/**
 * g_unichar_islower:
 * @c: a Unicode character
 * 
 * Determines whether a character is a lowercase letter.
 * Given some UTF-8 text, obtain a character value with
 * g_utf8_get_char().
 * 
 * Return value: %TRUE if @c is a lowercase letter
 **/
gboolean
g_unichar_islower (gunichar c)
{
  return TYPE (c) == G_UNICODE_LOWERCASE_LETTER;
}


/**
 * g_unichar_isprint:
 * @c: a Unicode character
 * 
 * Determines whether a character is printable.
 * Unlike g_unichar_isgraph(), returns %TRUE for spaces.
 * Given some UTF-8 text, obtain a character value with
 * g_utf8_get_char().
 * 
 * Return value: %TRUE if @c is printable
 **/
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

/**
 * g_unichar_ispunct:
 * @c: a Unicode character
 * 
 * Determines whether a character is punctuation or a symbol.
 * Given some UTF-8 text, obtain a character value with
 * g_utf8_get_char().
 * 
 * Return value: %TRUE if @c is a punctuation or symbol character
 **/
gboolean
g_unichar_ispunct (gunichar c)
{
  int t = TYPE (c);
  return (t == G_UNICODE_CONNECT_PUNCTUATION || t == G_UNICODE_DASH_PUNCTUATION
	  || t == G_UNICODE_CLOSE_PUNCTUATION || t == G_UNICODE_FINAL_PUNCTUATION
	  || t == G_UNICODE_INITIAL_PUNCTUATION || t == G_UNICODE_OTHER_PUNCTUATION
	  || t == G_UNICODE_OPEN_PUNCTUATION || t == G_UNICODE_CURRENCY_SYMBOL
	  || t == G_UNICODE_MODIFIER_SYMBOL || t == G_UNICODE_MATH_SYMBOL
	  || t == G_UNICODE_OTHER_SYMBOL);
}

/**
 * g_unichar_isspace:
 * @c: a Unicode character
 * 
 * Determines whether a character is a space, tab, or line separator
 * (newline, carriage return, etc.).  Given some UTF-8 text, obtain a
 * character value with g_utf8_get_char().
 *
 * (Note: don't use this to do word breaking; you have to use
 * Pango or equivalent to get word breaking right, the algorithm
 * is fairly complex.)
 *  
 * Return value: %TRUE if @c is a punctuation character
 **/
gboolean
g_unichar_isspace (gunichar c)
{
  switch (c)
    {
      /* special-case these since Unicode thinks they are not spaces */
    case '\t':
    case '\n':
    case '\r':
    case '\f':
      return TRUE;
      break;
      
    default:
      {
        int t = TYPE (c);
        return (t == G_UNICODE_SPACE_SEPARATOR || t == G_UNICODE_LINE_SEPARATOR
                || t == G_UNICODE_PARAGRAPH_SEPARATOR);
      }
      break;
    }
}

/**
 * g_unichar_isupper:
 * @c: a Unicode character
 * 
 * Determines if a character is uppercase.
 * 
 * Return value: %TRUE if @c is an uppercase character
 **/
gboolean
g_unichar_isupper (gunichar c)
{
  return TYPE (c) == G_UNICODE_UPPERCASE_LETTER;
}

/**
 * g_unichar_istitle:
 * @c: a Unicode character
 * 
 * Determines if a character is titlecase. Some characters in
 * Unicode which are composites, such as the DZ digraph
 * have three case variants instead of just two. The titlecase
 * form is used at the beginning of a word where only the
 * first letter is capitalized. The titlecase form of the DZ
 * digraph is U+01F2 LATIN CAPITAL LETTTER D WITH SMALL LETTER Z.
 * 
 * Return value: %TRUE if the character is titlecase
 **/
gboolean
g_unichar_istitle (gunichar c)
{
  unsigned int i;
  for (i = 0; i < G_N_ELEMENTS (title_table); ++i)
    if (title_table[i][0] == c)
      return 1;
  return 0;
}

/**
 * g_unichar_isxdigit:
 * @c: a Unicode character.
 * 
 * Determines if a character is a hexidecimal digit.
 * 
 * Return value: %TRUE if the character is a hexadecimal digit
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
 * @c: a Unicode character
 * 
 * Determines if a given character is assigned in the Unicode
 * standard.
 *
 * Return value: %TRUE if the character has an assigned value
 **/
gboolean
g_unichar_isdefined (gunichar c)
{
  int t = TYPE (c);
  return t != G_UNICODE_UNASSIGNED;
}

/**
 * g_unichar_iswide:
 * @c: a Unicode character
 * 
 * Determines if a character is typically rendered in a double-width
 * cell.
 * 
 * Return value: %TRUE if the character is wide
 **/
/* This function stolen from Markus Kuhn <Markus.Kuhn@cl.cam.ac.uk>.  */
gboolean
g_unichar_iswide (gunichar c)
{
  if (c < 0x1100)
    return FALSE;

  return (c <= 0x115f  /* Hangul Jamo init. consonants */ 
          || c == 0x2329 || c == 0x232a     /* angle brackets */
          || (c >= 0x2e80 && c <= 0xa4cf && (c < 0x302a || c > 0x302f) 
              && c != 0x303f && c != 0x3099 && c!= 0x309a) /* CJK ... Yi */
          || (c >= 0xac00 && c <= 0xd7a3)   /* Hangul Syllables */
          || (c >= 0xf900 && c <= 0xfaff)   /* CJK Compatibility Ideographs */
          || (c >= 0xfe30 && c <= 0xfe6f)   /* CJK Compatibility Forms */
          || (c >= 0xff00 && c <= 0xff60)   /* Fullwidth Forms */
          || (c >= 0xffe0 && c <= 0xffe6)   /* Fullwidth Forms */
          || (c >= 0x20000 && c <= 0x2fffd) /* CJK extra stuff */
          || (c >= 0x30000 && c <= 0x3fffd));
}

/**
 * g_unichar_toupper:
 * @c: a Unicode character
 * 
 * Converts a character to uppercase.
 * 
 * Return value: the result of converting @c to uppercase.
 *               If @c is not an lowercase or titlecase character,
 *               or has no upper case equivalent @c is returned unchanged.
 **/
gunichar
g_unichar_toupper (gunichar c)
{
  int t = TYPE (c);
  if (t == G_UNICODE_LOWERCASE_LETTER)
    {
      gunichar val = ATTTABLE (c >> 8, c & 0xff);
      if (val >= 0x1000000)
	{
	  const gchar *p = special_case_table + val - 0x1000000;
	  return g_utf8_get_char (p);
	}
      else
	return val ? val : c;
    }
  else if (t == G_UNICODE_TITLECASE_LETTER)
    {
      unsigned int i;
      for (i = 0; i < G_N_ELEMENTS (title_table); ++i)
	{
	  if (title_table[i][0] == c)
	    return title_table[i][1];
	}
    }
  return c;
}

/**
 * g_unichar_tolower:
 * @c: a Unicode character.
 * 
 * Converts a character to lower case.
 * 
 * Return value: the result of converting @c to lower case.
 *               If @c is not an upperlower or titlecase character,
 *               or has no lowercase equivalent @c is returned unchanged.
 **/
gunichar
g_unichar_tolower (gunichar c)
{
  int t = TYPE (c);
  if (t == G_UNICODE_UPPERCASE_LETTER)
    {
      gunichar val = ATTTABLE (c >> 8, c & 0xff);
      if (val >= 0x1000000)
	{
	  const gchar *p = special_case_table + val - 0x1000000;
	  return g_utf8_get_char (p);
	}
      else
	return val ? val : c;
    }
  else if (t == G_UNICODE_TITLECASE_LETTER)
    {
      unsigned int i;
      for (i = 0; i < G_N_ELEMENTS (title_table); ++i)
	{
	  if (title_table[i][0] == c)
	    return title_table[i][2];
	}
    }
  return c;
}

/**
 * g_unichar_totitle:
 * @c: a Unicode character
 * 
 * Converts a character to the titlecase.
 * 
 * Return value: the result of converting @c to titlecase.
 *               If @c is not an uppercase or lowercase character,
 *               @c is returned unchanged.
 **/
gunichar
g_unichar_totitle (gunichar c)
{
  unsigned int i;
  for (i = 0; i < G_N_ELEMENTS (title_table); ++i)
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
 * g_unichar_digit_value:
 * @c: a Unicode character
 *
 * Determines the numeric value of a character as a decimal
 * digit.
 *
 * Return value: If @c is a decimal digit (according to
 * g_unichar_isdigit()), its numeric value. Otherwise, -1.
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
 * @c: a Unicode character
 *
 * Determines the numeric value of a character as a hexidecimal
 * digit.
 *
 * Return value: If @c is a hex digit (according to
 * g_unichar_isxdigit()), its numeric value. Otherwise, -1.
 **/
int
g_unichar_xdigit_value (gunichar c)
{
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  if (TYPE (c) == G_UNICODE_DECIMAL_NUMBER)
    return ATTTABLE (c >> 8, c & 0xff);
  return -1;
}

/**
 * g_unichar_type:
 * @c: a Unicode character
 * 
 * Classifies a Unicode character by type.
 * 
 * Return value: the type of the character.
 **/
GUnicodeType
g_unichar_type (gunichar c)
{
  return TYPE (c);
}

/*
 * Case mapping functions
 */

typedef enum {
  LOCALE_NORMAL,
  LOCALE_TURKIC,
  LOCALE_LITHUANIAN
} LocaleType;

static LocaleType
get_locale_type (void)
{
  const char *locale = setlocale (LC_CTYPE, NULL);

  switch (locale[0])
    {
   case 'a':
      if (locale[1] == 'z')
	return LOCALE_TURKIC;
      break;
    case 'l':
      if (locale[1] == 't')
	return LOCALE_LITHUANIAN;
      break;
    case 't':
      if (locale[1] == 'r')
	return LOCALE_TURKIC;
      break;
    }

  return LOCALE_NORMAL;
}

static gint
output_marks (const char **p_inout,
	      char        *out_buffer,
	      gboolean     remove_dot)
{
  const char *p = *p_inout;
  gint len = 0;
  
  while (*p)
    {
      gunichar c = g_utf8_get_char (p);
      int t = TYPE(c);
      
      if (ISMARK(t))
	{
	  if (!remove_dot || c != 0x307 /* COMBINING DOT ABOVE */)
	    len += g_unichar_to_utf8 (c, out_buffer ? out_buffer + len : NULL);
	  p = g_utf8_next_char (p);
	}
      else
	break;
    }

  *p_inout = p;
  return len;
}

static gint
output_special_case (gchar *out_buffer,
		     int    offset,
		     int    type,
		     int    which)
{
  const gchar *p = special_case_table + offset;
  gint len;

  if (type != G_UNICODE_TITLECASE_LETTER)
    p = g_utf8_next_char (p);

  if (which == 1)
    p += strlen (p) + 1;

  len = strlen (p);
  if (out_buffer)
    memcpy (out_buffer, p, len);

  return len;
}

static gsize
real_toupper (const gchar *str,
	      gssize       max_len,
	      gchar       *out_buffer,
	      LocaleType   locale_type)
{
  const gchar *p = str;
  const char *last = NULL;
  gsize len = 0;
  gboolean last_was_i = FALSE;

  while ((max_len < 0 || p < str + max_len) && *p)
    {
      gunichar c = g_utf8_get_char (p);
      int t = TYPE (c);
      gunichar val;

      last = p;
      p = g_utf8_next_char (p);

      if (locale_type == LOCALE_LITHUANIAN)
	{
	  if (c == 'i')
	    last_was_i = TRUE;
	  else 
	    {
	      if (last_was_i)
		{
		  /* Nasty, need to remove any dot above. Though
		   * I think only E WITH DOT ABOVE occurs in practice
		   * which could simplify this considerably.
		   */
		  gsize decomp_len, i;
		  gunichar *decomp;

		  decomp = g_unicode_canonical_decomposition (c, &decomp_len);
		  for (i=0; i < decomp_len; i++)
		    {
		      if (decomp[i] != 0x307 /* COMBINING DOT ABOVE */)
			len += g_unichar_to_utf8 (g_unichar_toupper (decomp[i]), out_buffer ? out_buffer + len : NULL);
		    }
		  g_free (decomp);
		  
		  len += output_marks (&p, out_buffer ? out_buffer + len : NULL, TRUE);

		  continue;
		}

	      if (!ISMARK(t))
		last_was_i = FALSE;
	    }
	}
      
      if (locale_type == LOCALE_TURKIC && c == 'i')
	{
	  /* i => LATIN CAPITAL LETTER I WITH DOT ABOVE */
	  len += g_unichar_to_utf8 (0x130, out_buffer ? out_buffer + len : NULL); 
	}
      else if (c == 0x0345)	/* COMBINING GREEK YPOGEGRAMMENI */
	{
	  /* Nasty, need to move it after other combining marks .. this would go away if
	   * we normalized first.
	   */
	  len += output_marks (&p, out_buffer ? out_buffer + len : NULL, FALSE);

	  /* And output as GREEK CAPITAL LETTER IOTA */
	  len += g_unichar_to_utf8 (0x399, out_buffer ? out_buffer + len : NULL); 	  
	}
      else if (t == G_UNICODE_LOWERCASE_LETTER || t == G_UNICODE_TITLECASE_LETTER)
	{
	  val = ATTTABLE (c >> 8, c & 0xff);

	  if (val >= 0x1000000)
	    {
	      len += output_special_case (out_buffer ? out_buffer + len : NULL, val - 0x1000000, t,
					  t == G_UNICODE_LOWERCASE_LETTER ? 0 : 1);
	    }
	  else
	    {
	      if (t == G_UNICODE_TITLECASE_LETTER)
		{
		  unsigned int i;
		  for (i = 0; i < G_N_ELEMENTS (title_table); ++i)
		    {
		      if (title_table[i][0] == c)
			val = title_table[i][1];
		    }
		}

	      len += g_unichar_to_utf8 (val, out_buffer ? out_buffer + len : NULL);
	    }
	}
      else
	{
	  gsize char_len = g_utf8_skip[*(guchar *)last];

	  if (out_buffer)
	    memcpy (out_buffer + len, last, char_len);

	  len += char_len;
	}

    }

  return len;
}

/**
 * g_utf8_strup:
 * @str: a UTF-8 encoded string
 * @len: length of @str, in bytes, or -1 if @str is nul-terminated.
 * 
 * Converts all Unicode characters in the string that have a case
 * to uppercase. The exact manner that this is done depends
 * on the current locale, and may result in the number of
 * characters in the string increasing. (For instance, the
 * German ess-zet will be changed to SS.)
 * 
 * Return value: a newly allocated string, with all characters
 *    converted to uppercase.  
 **/
gchar *
g_utf8_strup (const gchar *str,
	      gssize       len)
{
  gsize result_len;
  LocaleType locale_type;
  gchar *result;

  g_return_val_if_fail (str != NULL, NULL);

  locale_type = get_locale_type ();
  
  /*
   * We use a two pass approach to keep memory management simple
   */
  result_len = real_toupper (str, len, NULL, locale_type);
  result = g_malloc (result_len + 1);
  real_toupper (str, len, result, locale_type);
  result[result_len] = '\0';

  return result;
}

/* traverses the string checking for characters with combining class == 230
 * until a base character is found */
static gboolean
has_more_above (const gchar *str)
{
  const gchar *p = str;
  gint combining_class;

  while (*p)
    {
      combining_class = _g_unichar_combining_class (g_utf8_get_char (p));
      if (combining_class == 230)
        return TRUE;
      else if (combining_class == 0)
        break;

      p = g_utf8_next_char (p);
    }

  return FALSE;
}

static gsize
real_tolower (const gchar *str,
	      gssize       max_len,
	      gchar       *out_buffer,
	      LocaleType   locale_type)
{
  const gchar *p = str;
  const char *last = NULL;
  gsize len = 0;

  while ((max_len < 0 || p < str + max_len) && *p)
    {
      gunichar c = g_utf8_get_char (p);
      int t = TYPE (c);
      gunichar val;

      last = p;
      p = g_utf8_next_char (p);

      if (locale_type == LOCALE_TURKIC && c == 'I')
	{
          if (g_utf8_get_char (p) == 0x0307)
            {
              /* I + COMBINING DOT ABOVE => i (U+0069) */
              len += g_unichar_to_utf8 (0x0069, out_buffer ? out_buffer + len : NULL); 
              p = g_utf8_next_char (p);
            }
          else
            {
              /* I => LATIN SMALL LETTER DOTLESS I */
              len += g_unichar_to_utf8 (0x131, out_buffer ? out_buffer + len : NULL); 
            }
        }
      /* Introduce an explicit dot above when lowercasing capital I's and J's
       * whenever there are more accents above. [SpecialCasing.txt] */
      else if (locale_type == LOCALE_LITHUANIAN && 
               (c == 0x00cc || c == 0x00cd || c == 0x0128))
        {
          len += g_unichar_to_utf8 (0x0069, out_buffer ? out_buffer + len : NULL); 
          len += g_unichar_to_utf8 (0x0307, out_buffer ? out_buffer + len : NULL); 

          switch (c)
            {
            case 0x00cc: 
              len += g_unichar_to_utf8 (0x0300, out_buffer ? out_buffer + len : NULL); 
              break;
            case 0x00cd: 
              len += g_unichar_to_utf8 (0x0301, out_buffer ? out_buffer + len : NULL); 
              break;
            case 0x0128: 
              len += g_unichar_to_utf8 (0x0303, out_buffer ? out_buffer + len : NULL); 
              break;
            }
        }
      else if (locale_type == LOCALE_LITHUANIAN && 
               (c == 'I' || c == 'J' || c == 0x012e) && 
               has_more_above (p))
        {
          len += g_unichar_to_utf8 (g_unichar_tolower (c), out_buffer ? out_buffer + len : NULL); 
          len += g_unichar_to_utf8 (0x0307, out_buffer ? out_buffer + len : NULL); 
        }
      else if (c == 0x03A3)	/* GREEK CAPITAL LETTER SIGMA */
	{
	  if ((max_len < 0 || p < str + max_len) && *p)
	    {
	      gunichar next_c = g_utf8_get_char (p);
	      int next_type = TYPE(next_c);

	      /* SIGMA mapps differently depending on whether it is
	       * final or not. The following simplified test would
	       * fail in the case of combining marks following the
	       * sigma, but I don't think that occurs in real text.
	       * The test here matches that in ICU.
	       */
	      if (ISALPHA(next_type)) /* Lu,Ll,Lt,Lm,Lo */
		val = 0x3c3;	/* GREEK SMALL SIGMA */
	      else
		val = 0x3c2;	/* GREEK SMALL FINAL SIGMA */
	    }
	  else
	    val = 0x3c2;	/* GREEK SMALL FINAL SIGMA */

	  len += g_unichar_to_utf8 (val, out_buffer ? out_buffer + len : NULL);
	}
      else if (t == G_UNICODE_UPPERCASE_LETTER || t == G_UNICODE_TITLECASE_LETTER)
	{
	  val = ATTTABLE (c >> 8, c & 0xff);

	  if (val >= 0x1000000)
	    {
	      len += output_special_case (out_buffer ? out_buffer + len : NULL, val - 0x1000000, t, 0);
	    }
	  else
	    {
	      if (t == G_UNICODE_TITLECASE_LETTER)
		{
		  unsigned int i;
		  for (i = 0; i < G_N_ELEMENTS (title_table); ++i)
		    {
		      if (title_table[i][0] == c)
			val = title_table[i][2];
		    }
		}

	      len += g_unichar_to_utf8 (val, out_buffer ? out_buffer + len : NULL);
	    }
	}
      else
	{
	  gsize char_len = g_utf8_skip[*(guchar *)last];

	  if (out_buffer)
	    memcpy (out_buffer + len, last, char_len);

	  len += char_len;
	}

    }

  return len;
}

/**
 * g_utf8_strdown:
 * @str: a UTF-8 encoded string
 * @len: length of @str, in bytes, or -1 if @str is nul-terminated.
 * 
 * Converts all Unicode characters in the string that have a case
 * to lowercase. The exact manner that this is done depends
 * on the current locale, and may result in the number of
 * characters in the string changing.
 * 
 * Return value: a newly allocated string, with all characters
 *    converted to lowercase.  
 **/
gchar *
g_utf8_strdown (const gchar *str,
		gssize       len)
{
  gsize result_len;
  LocaleType locale_type;
  gchar *result;

  g_return_val_if_fail (str != NULL, NULL);

  locale_type = get_locale_type ();
  
  /*
   * We use a two pass approach to keep memory management simple
   */
  result_len = real_tolower (str, len, NULL, locale_type);
  result = g_malloc (result_len + 1);
  real_tolower (str, len, result, locale_type);
  result[result_len] = '\0';

  return result;
}

/**
 * g_utf8_casefold:
 * @str: a UTF-8 encoded string
 * @len: length of @str, in bytes, or -1 if @str is nul-terminated.
 * 
 * Converts a string into a form that is independent of case. The
 * result will not correspond to any particular case, but can be
 * compared for equality or ordered with the results of calling
 * g_utf8_casefold() on other strings.
 * 
 * Note that calling g_utf8_casefold() followed by g_utf8_collate() is
 * only an approximation to the correct linguistic case insensitive
 * ordering, though it is a fairly good one. Getting this exactly
 * right would require a more sophisticated collation function that
 * takes case sensitivity into account. GLib does not currently
 * provide such a function.
 * 
 * Return value: a newly allocated string, that is a
 *   case independent form of @str.
 **/
gchar *
g_utf8_casefold (const gchar *str,
		 gssize       len)
{
  GString *result;
  const char *p;

  g_return_val_if_fail (str != NULL, NULL);

  result = g_string_new (NULL);
  p = str;
  while ((len < 0 || p < str + len) && *p)
    {
      gunichar ch = g_utf8_get_char (p);

      int start = 0;
      int end = G_N_ELEMENTS (casefold_table);

      if (ch >= casefold_table[start].ch &&
          ch <= casefold_table[end - 1].ch)
	{
	  while (TRUE)
	    {
	      int half = (start + end) / 2;
	      if (ch == casefold_table[half].ch)
		{
		  g_string_append (result, casefold_table[half].data);
		  goto next;
		}
	      else if (half == start)
		break;
	      else if (ch > casefold_table[half].ch)
		start = half;
	      else
		end = half;
	    }
	}

      g_string_append_unichar (result, g_unichar_tolower (ch));
      
    next:
      p = g_utf8_next_char (p);
    }

  return g_string_free (result, FALSE); 
}

/**
 * g_unichar_get_mirror_char:
 * @ch: a unicode character
 * @mirrored_ch: location to store the mirrored character
 * 
 * In Unicode, some characters are <firstterm>mirrored</firstterm>. This
 * means that their images are mirrored horizontally in text that is laid
 * out from right to left. For instance, "(" would become its mirror image,
 * ")", in right-to-left text.
 *
 * If @ch has the Unicode mirrored property and there is another unicode
 * character that typically has a glyph that is the mirror image of @ch's
 * glyph, puts that character in the address pointed to by @mirrored_ch.
 *
 * Return value: %TRUE if @ch has a mirrored character and @mirrored_ch is
 * filled in, %FALSE otherwise
 *
 * Since: 2.4
 **/
/* This code is adapted from FriBidi (http://fribidi.sourceforge.net/). 
 * FriBidi is: Copyright (C) 1999,2000 Dov Grobgeld, and
 *             Copyright (C) 2001,2002 Behdad Esfahbod.
 */
gboolean
g_unichar_get_mirror_char (gunichar ch,
                           gunichar *mirrored_ch)
{
  gint pos, step, size;
  gboolean found;

  size = G_N_ELEMENTS (bidi_mirroring_table);
  pos = step = (size / 2) + 1;

  while (step > 1)
    {
      gunichar cmp_ch = bidi_mirroring_table[pos].ch;
      step = (step + 1) / 2;

      if (cmp_ch < ch)
        {
          pos += step;
          if (pos > size - 1)
            pos = size - 1;
        }
      else if (cmp_ch > ch)
        {
          pos -= step;
          if (pos < 0)
            pos = 0;
        }
      else
        break;
    }
  found = bidi_mirroring_table[pos].ch == ch;
  if (mirrored_ch)
    *mirrored_ch = found ? bidi_mirroring_table[pos].mirrored_ch : ch;

  return found;

}
