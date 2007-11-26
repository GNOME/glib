/* GIO - GLib Input, Output and Streaming Library
 * 
 * Copyright (C) 2006-2007 Red Hat, Inc.
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
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Alexander Larsson <alexl@redhat.com>
 */

#include <config.h>
#include "gurifuncs.h"
#include "string.h"

static int
unescape_character (const char *scanner)
{
  int first_digit;
  int second_digit;
  
  first_digit = g_ascii_xdigit_value (*scanner++);
  if (first_digit < 0)
    return -1;

  second_digit = g_ascii_xdigit_value (*scanner++);
  if (second_digit < 0)
    return -1;

  return (first_digit << 4) | second_digit;
}

/**
 * g_uri_unescape_segment:
 * @escaped_string: a string.
 * @escaped_string_end: a string.
 * @illegal_characters: a string of illegal characters not to be allowed.
 * 
 * Returns: an unescaped version of @escaped_string or %NULL on error.
 * The returned string should be freed when no longer needed.
 **/
char *
g_uri_unescape_segment (const char *escaped_string,
			const char *escaped_string_end,
			const char *illegal_characters)
{
  const char *in;
  char *out, *result;
  gint character;
  
  if (escaped_string == NULL)
    return NULL;
  
  if (escaped_string_end == NULL)
    escaped_string_end = escaped_string + strlen (escaped_string);
  
  result = g_malloc (escaped_string_end - escaped_string + 1);
  
  out = result;
  for (in = escaped_string; in < escaped_string_end; in++)
    {
      character = *in;
      
      if (*in == '%')
	{
	  in++;
	  
	  if (escaped_string_end - in < 2)
	    {
	      /* Invalid escaped char (to short) */
	      g_free (result);
	      return NULL;
	    }
	  
	  character = unescape_character (in);
	  
	  /* Check for an illegal character. We consider '\0' illegal here. */
	  if (character <= 0 ||
	      (illegal_characters != NULL &&
	       strchr (illegal_characters, (char)character) != NULL))
	    {
	      g_free (result);
	      return NULL;
	    }
	  
	  in++; /* The other char will be eaten in the loop header */
	}
      *out++ = (char)character;
    }
  
  *out = '\0';
  
  return result;
}

/**
 * g_uri_unescape_string:
 * @escaped_string: an escaped string to be unescaped.
 * @illegal_characters: a string of illegal characters not to be allowed.
 * 
 * Returns: an unescaped version of @escaped_string. 
 *
 * The returned string should be freed when no longer needed
 *
 **/
char *
g_uri_unescape_string (const char *escaped_string,
		       const char *illegal_characters)
{
  return g_uri_unescape_segment (escaped_string, NULL, illegal_characters);
}

/**
 * g_uri_get_scheme:
 * @uri: a valid URI.
 * 
 * Returns: The "Scheme" component of the URI, or %NULL on error. 
 * RFC 3986 decodes the scheme as:
 * URI = scheme ":" hier-part [ "?" query ] [ "#" fragment ] 
 * Popular schemes include "file", "http", "svn", etc.
 *
 * The returned string should be freed when no longer needed.
 *
 **/
char *
g_uri_get_scheme (const char  *uri)
{
  const char *p;
  char c;

  g_return_val_if_fail (uri != NULL, NULL);

  /* From RFC 3986 Decodes:
   * URI         = scheme ":" hier-part [ "?" query ] [ "#" fragment ]
   */ 

  p = uri;
  
  /* Decode scheme:
     scheme      = ALPHA *( ALPHA / DIGIT / "+" / "-" / "." )
  */

  if (!g_ascii_isalpha (*p))
    return NULL;
  
  while (1)
    {
      c = *p++;
      
      if (c == ':')
	break;
      
      if (!(g_ascii_isalnum(c) ||
	    c == '+' ||
	    c == '-' ||
	    c == '.'))
	return NULL;
    }
  
  return g_strndup (uri, p - uri - 1);
}

#define SUB_DELIM_CHARS  "!$&'()*+,;="

static gboolean
is_valid (char c, const char *reserved_chars_allowed)
{
  if (g_ascii_isalnum (c) ||
      c == '-' ||
      c == '.' ||
      c == '_' ||
      c == '~')
    return TRUE;

  if (reserved_chars_allowed &&
      strchr (reserved_chars_allowed, c) != NULL)
    return TRUE;
  
  return FALSE;
}

static gboolean 
gunichar_ok (gunichar c)
{
  return
    (c != (gunichar) -2) &&
    (c != (gunichar) -1);
}

/**
 * g_string_append_uri_escaped:
 * @string: a #GString to append to.
 * @unescaped: the input C string of unescaped URI data.
 * @reserved_chars_allowed: a string of reserve characters allowed to be used.
 * @allow_utf8: set %TRUE if the return value may include UTF8 characters.
 * 
 * Returns a #GString with the escaped URI appended.
 *
 **/
GString *
g_string_append_uri_escaped (GString *string,
			     const char *unescaped,
			     const char *reserved_chars_allowed,
			     gboolean allow_utf8)
{
  unsigned char c;
  const char *end;
  static const gchar hex[16] = "0123456789ABCDEF";

  g_return_val_if_fail (string != NULL, NULL);
  g_return_val_if_fail (unescaped != NULL, NULL);

  end = unescaped + strlen (unescaped);
  
  while ((c = *unescaped) != 0)
    {
      if (c >= 0x80 && allow_utf8 &&
	  gunichar_ok (g_utf8_get_char_validated (unescaped, end - unescaped)))
	{
	  int len = g_utf8_skip [c];
	  g_string_append_len (string, unescaped, len);
	  unescaped += len;
	}
      else if (is_valid (c, reserved_chars_allowed))
	{
	  g_string_append_c (string, c);
	  unescaped++;
	}
      else
	{
	  g_string_append_c (string, '%');
	  g_string_append_c (string, hex[((guchar)c) >> 4]);
	  g_string_append_c (string, hex[((guchar)c) & 0xf]);
	  unescaped++;
	}
    }

  return string;
}

/**
 * g_uri_escape_string:
 * @unescaped: the unescaped input string.
 * @reserved_chars_allowed: a string of reserve characters allowed to be used.
 * @allow_utf8: set to %TRUE if string can include UTF8 characters.
 * 
 * Returns an escaped version of @unescaped. 
 * 
 * The returned string should be freed when no longer needed.
 **/
char *
g_uri_escape_string (const char *unescaped,
		     const char  *reserved_chars_allowed,
		     gboolean     allow_utf8)
{
  GString *s;

  g_return_val_if_fail (unescaped != NULL, NULL);

  s = g_string_sized_new (strlen (unescaped) + 10);
  
  g_string_append_uri_escaped (s, unescaped, reserved_chars_allowed, allow_utf8);
  
  return g_string_free (s, FALSE);
}
