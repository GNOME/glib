/* gutf8.c - Operations on UTF-8 strings.
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

#include <config.h>

#include <stdlib.h>
#ifdef HAVE_CODESET
#include <langinfo.h>
#endif
#include <string.h>

#include "glib.h"

#define UTF8_COMPUTE(Char, Mask, Len)					      \
  if (Char < 128)							      \
    {									      \
      Len = 1;								      \
      Mask = 0x7f;							      \
    }									      \
  else if ((Char & 0xe0) == 0xc0)					      \
    {									      \
      Len = 2;								      \
      Mask = 0x1f;							      \
    }									      \
  else if ((Char & 0xf0) == 0xe0)					      \
    {									      \
      Len = 3;								      \
      Mask = 0x0f;							      \
    }									      \
  else if ((Char & 0xf8) == 0xf0)					      \
    {									      \
      Len = 4;								      \
      Mask = 0x07;							      \
    }									      \
  else if ((Char & 0xfc) == 0xf8)					      \
    {									      \
      Len = 5;								      \
      Mask = 0x03;							      \
    }									      \
  else if ((Char & 0xfe) == 0xfc)					      \
    {									      \
      Len = 6;								      \
      Mask = 0x01;							      \
    }									      \
  else									      \
    Len = -1;

#define UTF8_GET(Result, Chars, Count, Mask, Len)			      \
  (Result) = (Chars)[0] & (Mask);					      \
  for ((Count) = 1; (Count) < (Len); ++(Count))				      \
    {									      \
      if (((Chars)[(Count)] & 0xc0) != 0x80)				      \
	{								      \
	  (Result) = -1;						      \
	  break;							      \
	}								      \
      (Result) <<= 6;							      \
      (Result) |= ((Chars)[(Count)] & 0x3f);				      \
    }
gchar g_utf8_skip[256] = {
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
  3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,4,4,4,4,4,4,4,4,5,5,5,5,6,6,0,0
};

/**
 * g_utf8_find_prev_char:
 * @str: pointer to the beginning of a UTF-8 string
 * @p: pointer to some position within @str
 * 
 * Given a position @p with a UTF-8 encoded string @str, find the start
 * of the previous UTF-8 character starting before @p. Returns %NULL if no
 * UTF-8 characters are present in @p before @str.
 *
 * @p does not have to be at the beginning of a UTF-8 chracter. No check
 * is made to see if the character found is actually valid other than
 * it starts with an appropriate byte.
 *
 * Return value: a pointer to the found character or %NULL.
 **/
gchar *
g_utf8_find_prev_char (const char *str,
		       const char *p)
{
  for (--p; p > str; --p)
    {
      if ((*p & 0xc0) != 0x80)
	return (gchar *)p;
    }
  return NULL;
}

/**
 * g_utf8_find_next_char:
 * @p: a pointer to a position within a UTF-8 encoded string
 * @end: a pointer to the end of the string, or %NULL to indicate
 *        that the string is NULL terminated, in which case
 *        the returned value will be 
 *
 * Find the start of the next utf-8 character in the string after @p
 *
 * @p does not have to be at the beginning of a UTF-8 chracter. No check
 * is made to see if the character found is actually valid other than
 * it starts with an appropriate byte.
 * 
 * Return value: a pointer to the found character or %NULL
 **/
gchar *
g_utf8_find_next_char (const gchar *p,
		       const gchar *end)
{
  if (*p)
    {
      if (end)
	for (++p; p < end && (*p & 0xc0) == 0x80; ++p)
	  ;
      else
	for (++p; (*p & 0xc0) == 0x80; ++p)
	  ;
    }
  return (p == end) ? NULL : (gchar *)p;
}

/**
 * g_utf8_prev_char:
 * @p: a pointer to a position within a UTF-8 encoded string
 *
 * Find the previous UTF-8 character in the string before @p
 *
 * @p does not have to be at the beginning of a UTF-8 character. No check
 * is made to see if the character found is actually valid other than
 * it starts with an appropriate byte. If @p might be the first
 * character of the string, you must use g_utf8_find_prev_char instead.
 * 
 * Return value: a pointer to the found character.
 **/
gchar *
g_utf8_prev_char (const gchar *p)
{
  while (TRUE)
    {
      p--;
      if ((*p & 0xc0) != 0x80)
	return (gchar *)p;
    }
}

/**
 * g_utf8_strlen:
 * @p: pointer to the start of a UTF-8 string.
 * @max: the maximum number of bytes to examine. If @max
 *       is less than 0, then the string is assumed to be
 *       nul-terminated.
 * 
 * Return value: the length of the string in characters
 */
gint
g_utf8_strlen (const gchar *p, gint max)
{
  int len = 0;
  const gchar *start = p;
  /* special case for the empty string */
  if (!*p) 
    return 0;
  /* Note that the test here and the test in the loop differ subtly.
     In the loop we want to see if we've passed the maximum limit --
     for instance if the buffer ends mid-character.  Here at the top
     of the loop we want to see if we've just reached the last byte.  */
  while (max < 0 || p - start < max)
    {
      p = g_utf8_next_char (p);
      ++len;
      if (! *p || (max > 0 && p - start > max))
	break;
    }
  return len;
}

/**
 * g_utf8_get_char:
 * @p: a pointer to unicode character encoded as UTF-8
 * 
 * Convert a sequence of bytes encoded as UTF-8 to a unicode character.
 * 
 * Return value: the resulting character or (gunichar)-1 if @p does
 *               not point to a valid UTF-8 encoded unicode character
 **/
gunichar
g_utf8_get_char (const gchar *p)
{
  int i, mask = 0, len;
  gunichar result;
  unsigned char c = (unsigned char) *p;

  UTF8_COMPUTE (c, mask, len);
  if (len == -1)
    return (gunichar)-1;
  UTF8_GET (result, p, i, mask, len);

  return result;
}

/**
 * g_utf8_offset_to_pointer:
 * @str: a UTF-8 encoded string
 * @offset: a character offset within the string.
 * 
 * Converts from an integer character offset to a pointer to a position
 * within the string.
 * 
 * Return value: the resulting pointer
 **/
gchar *
g_utf8_offset_to_pointer  (const gchar *str,
			   gint         offset)
{
  const gchar *s = str;
  while (offset--)
    s = g_utf8_next_char (s);
  
  return (gchar *)s;
}

/**
 * g_utf8_pointer_to_offset:
 * @str: a UTF-8 encoded string
 * @pos: a pointer to a position within @str
 * 
 * Converts from a pointer to position within a string to a integer
 * character offset
 * 
 * Return value: the resulting character offset
 **/
gint
g_utf8_pointer_to_offset (const gchar *str,
			  const gchar *pos)
{
  const gchar *s = str;
  gint offset = 0;
  
  while (s < pos)
    {
      s = g_utf8_next_char (s);
      offset++;
    }

  return offset;
}


gchar *
g_utf8_strncpy (gchar *dest, const gchar *src, size_t n)
{
  const gchar *s = src;
  while (n && *s)
    {
      s = g_utf8_next_char(s);
      n--;
    }
  strncpy(dest, src, s - src);
  dest[s - src] = 0;
  return dest;
}

static gboolean
g_utf8_get_charset_internal (char **a)
{
  char *charset = getenv("CHARSET");

  if (charset && a && ! *a)
    *a = charset;

  if (charset && strstr (charset, "UTF-8"))
      return TRUE;

#ifdef HAVE_CODESET
  charset = nl_langinfo(CODESET);
  if (charset)
    {
      if (a && ! *a)
	*a = charset;
      if (strcmp (charset, "UTF-8") == 0)
	return TRUE;
    }
#endif
  
#if 0 /* #ifdef _NL_CTYPE_CODESET_NAME */
  charset = nl_langinfo (_NL_CTYPE_CODESET_NAME);
  if (charset)
    {
      if (a && ! *a)
	*a = charset;
      if (strcmp (charset, "UTF-8") == 0)
	return TRUE;
    }
#endif

  if (a && ! *a) 
    *a = "US-ASCII";
  /* Assume this for compatibility at present.  */
  return FALSE;
}

static int utf8_locale_cache = -1;
static char *utf8_charset_cache = NULL;

gboolean
g_get_charset (char **charset) 
{
  if (utf8_locale_cache != -1)
    {
      if (charset)
	*charset = utf8_charset_cache;
      return utf8_locale_cache;
    }
  utf8_locale_cache = g_utf8_get_charset_internal (&utf8_charset_cache);
  if (charset) 
    *charset = utf8_charset_cache;
  return utf8_locale_cache;
}

/* unicode_strchr */

/**
 * g_unichar_to_utf8:
 * @c: a ISO10646 character code
 * @outbuf: output buffer, must have at least 6 bytes of space.
 *       If %NULL, the length will be computed and returned
 *       and nothing will be written to @out.
 * 
 * Convert a single character to utf8
 * 
 * Return value: number of bytes written
 **/
int
g_unichar_to_utf8 (gunichar c, gchar *outbuf)
{
  size_t len = 0;
  int first;
  int i;

  if (c < 0x80)
    {
      first = 0;
      len = 1;
    }
  else if (c < 0x800)
    {
      first = 0xc0;
      len = 2;
    }
  else if (c < 0x10000)
    {
      first = 0xe0;
      len = 3;
    }
   else if (c < 0x200000)
    {
      first = 0xf0;
      len = 4;
    }
  else if (c < 0x4000000)
    {
      first = 0xf8;
      len = 5;
    }
  else
    {
      first = 0xfc;
      len = 6;
    }

  if (outbuf)
    {
      for (i = len - 1; i > 0; --i)
	{
	  outbuf[i] = (c & 0x3f) | 0x80;
	  c >>= 6;
	}
      outbuf[0] = c | first;
    }

  return len;
}

/**
 * g_utf8_strchr:
 * @p: a nul-terminated utf-8 string
 * @c: a iso-10646 character/
 * 
 * Find the leftmost occurence of the given iso-10646 character
 * in a UTF-8 string.
 * 
 * Return value: NULL if the string does not contain the character, otherwise, a
 *               a pointer to the start of the leftmost of the character in the string.
 **/
gchar *
g_utf8_strchr (const char *p, gunichar c)
{
  gchar ch[10];

  gint len = g_unichar_to_utf8 (c, ch);
  ch[len] = '\0';
  
  return strstr(p, ch);
}

#if 0
/**
 * g_utf8_strrchr:
 * @p: a nul-terminated utf-8 string
 * @c: a iso-10646 character/
 * 
 * Find the rightmost occurence of the given iso-10646 character
 * in a UTF-8 string.
 * 
 * Return value: NULL if the string does not contain the character, otherwise, a
 *               a pointer to the start of the rightmost of the character in the string.
 **/

/* This is ifdefed out atm as there is no strrstr function in libc.
 */
gchar *
unicode_strrchr (const char *p, gunichar c)
{
  gchar ch[10];

  len = g_unichar_to_utf8 (c, ch);
  ch[len] = '\0';
  
  return strrstr(p, ch);
}
#endif


/**
 * g_utf8_to_ucs4:
 * @str: a UTF-8 encoded strnig
 * @len: the length of @
 * 
 * Convert a string from UTF-8 to a 32-bit fixed width
 * representation as UCS-4.
 * 
 * Return value: a pointer to a newly allocated UCS-4 string.
 *               This value must be freed with g_free()
 **/
gunichar *
g_utf8_to_ucs4 (const char *str, int len)
{
  gunichar *result;
  gint n_chars, i;
  const gchar *p;
  
  n_chars = g_utf8_strlen (str, len);
  result = g_new (gunichar, n_chars);
  
  p = str;
  for (i=0; i < n_chars; i++)
    {
      result[i] = g_utf8_get_char (p);
      p = g_utf8_next_char (p);
    }

  return result;
}

/**
 * g_ucs4_to_utf8:
 * @str: a UCS-4 encoded string
 * @len: the length of @
 * 
 * Convert a string from a 32-bit fixed width representation as UCS-4.
 * to UTF-8.
 * 
 * Return value: a pointer to a newly allocated UTF-8 string.
 *               This value must be freed with g_free()
 **/
gchar *
g_ucs4_to_utf8 (const gunichar *str, int len)
{
  gint result_length;
  gchar *result, *p;
  gint i;

  result_length = 0;
  for (i = 0; i < len ; i++)
    result_length += g_unichar_to_utf8 (str[i], NULL);

  result_length++;

  result = g_malloc (result_length + 1);
  p = result;

  for (i = 0; i < len ; i++)
    p += g_unichar_to_utf8 (str[i], p);
  
  *p = '\0';

  return result;
}

/**
 * g_utf8_validate:
 * @str: a pointer to character data
 * @max_len: max bytes to validate, or -1 to go until nul
 * @end: return location for end of valid data
 * 
 * Validates UTF-8 encoded text. @str is the text to validate;
 * if @str is nul-terminated, then @max_len can be -1, otherwise
 * @max_len should be the number of bytes to validate.
 * If @end is non-NULL, then the end of the valid range
 * will be stored there (i.e. the address of the first invalid byte
 * if some bytes were invalid, or the end of the text being validated
 * otherwise).
 *
 * Returns TRUE if all of @str was valid. Many GLib and GTK+
 * routines <emphasis>require</emphasis> valid UTF8 as input;
 * so data read from a file or the network should be checked
 * with g_utf8_validate() before doing anything else with it.
 * 
 * Return value: TRUE if the text was valid UTF-8.
 **/
gboolean
g_utf8_validate (const gchar  *str,
                 gint          max_len,
                 const gchar **end)
{

  const gchar *p;
  gboolean retval = TRUE;
  
  if (end)
    *end = str;
  
  p = str;
  
  while ((max_len < 0 || (p - str) < max_len) && *p)
    {
      int i, mask = 0, len;
      gunichar result;
      unsigned char c = (unsigned char) *p;
      
      UTF8_COMPUTE (c, mask, len);

      if (len == -1)
        {
          retval = FALSE;
          break;
        }

      /* check that the expected number of bytes exists in str */
      if (max_len >= 0 &&
          ((max_len - (p - str)) < len))
        {
          retval = FALSE;
          break;
        }
        
      UTF8_GET (result, p, i, mask, len);

      if (result == (gunichar)-1)
        {
          retval = FALSE;
          break;
        }
      
      p += len;
    }

  if (end)
    *end = p;
  
  return retval;
}


