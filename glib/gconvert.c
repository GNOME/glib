/* GLIB - Library of useful routines for C programming
 *
 * gconvert.c: Convert between character sets using iconv
 * Copyright Red Hat Inc., 2000
 * Authors: Havoc Pennington <hp@redhat.com>, Owen Taylor <otaylor@redhat.com
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
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <iconv.h>
#include <errno.h>
#include <string.h>

#include "glib.h"

GQuark 
g_convert_error_quark()
{
  static GQuark quark;
  if (!quark)
    quark = g_quark_from_static_string ("g_convert_error");
  return quark;
}

static iconv_t
open_converter (const gchar *to_codeset,
		const gchar *from_codeset,
		GError     **error)
{
  iconv_t cd = iconv_open (to_codeset, from_codeset);

  if (cd == (iconv_t) -1)
    {
      /* Something went wrong.  */
      if (errno == EINVAL)
	g_set_error (error, G_CONVERT_ERROR, G_CONVERT_ERROR_NO_CONVERSION,
		     "Conversion from character set `%s' to `%s' is not supported",
		     from_codeset, to_codeset);
      else
        g_set_error (error, G_CONVERT_ERROR, G_CONVERT_ERROR_OTHER,
		     "Could not open converter from `%s' to `%s': %s",
		     from_codeset, to_codeset, strerror (errno));
    }

  return cd;

}

/**
 * g_convert:
 * @str:          the string to convert
 * @len:          the length of the string
 * @to_codeset:   name of character set into which to convert @str
 * @from_codeset: character set of @str.
 * @bytes_read:   location to store the number of bytes in the
 *                input string that were successfully converted, or %NULL.
 *                Even if the conversion was succesful, this may be 
 *                less than len if there were partial characters
 *                at the end of the input. If the error
 *                G_CONVERT_ERROR_ILLEGAL_SEQUENCE occurs, the value
 *                stored will the byte fofset after the last valid
 *                input sequence.
 * @error:        location to store the error occuring, or %NULL to ignore
 *                errors. Any of the errors in #GConvertError may occur.
 *
 * Convert a string from one character set to another.
 *
 * Return value: If the conversion was successful, a newly allocated
 *               NUL-terminated string, which must be freed with
 *               g_free. Otherwise %NULL and @error will be set.
 **/
gchar*
g_convert (const gchar *str,
           gint         len,
           const gchar *to_codeset,
           const gchar *from_codeset,
           gint        *bytes_read,
	   gint        *bytes_written,
	   GError     **error)
{
  gchar *dest;
  gchar *outp;
  const gchar *p;
  size_t inbytes_remaining;
  size_t outbytes_remaining;
  size_t err;
  iconv_t cd;
  size_t outbuf_size;
  gboolean have_error = FALSE;
  
  g_return_val_if_fail (str != NULL, NULL);
  g_return_val_if_fail (to_codeset != NULL, NULL);
  g_return_val_if_fail (from_codeset != NULL, NULL);
     
  cd = open_converter (to_codeset, from_codeset, error);

  if (cd == (iconv_t) -1)
    {
      if (bytes_read)
        *bytes_read = 0;
      
      if (bytes_written)
        *bytes_written = 0;
      
      return NULL;
    }

  if (len < 0)
    len = strlen (str);

  p = str;
  inbytes_remaining = len;
  outbuf_size = len + 1; /* + 1 for nul in case len == 1 */
  outbytes_remaining = outbuf_size - 1; /* -1 for nul */
  outp = dest = g_malloc (outbuf_size);

 again:
  
  err = iconv (cd, &p, &inbytes_remaining, &outp, &outbytes_remaining);

  if (err == (size_t) -1)
    {
      switch (errno)
	{
	case EINVAL:
	  /* Incomplete text, do not report an error */
	  break;
	case E2BIG:
	  {
	    size_t used = outp - dest;
	    outbuf_size *= 2;
	    dest = g_realloc (dest, outbuf_size);

	    outp = dest + used;
	    outbytes_remaining = outbuf_size - used - 1; /* -1 for nul */

	    goto again;
	  }
	case EILSEQ:
	  g_set_error (error, G_CONVERT_ERROR, G_CONVERT_ERROR_ILLEGAL_SEQUENCE,
		       "Invalid byte sequence in conversion input");
	  have_error = TRUE;
	  break;
	default:
	  g_set_error (error, G_CONVERT_ERROR, G_CONVERT_ERROR_OTHER,
		       "Error during conversion: %s",
		       strerror (errno));
	  have_error = TRUE;
	  break;
	}
    }

  *outp = '\0';
  
  iconv_close (cd);

  if (bytes_read)
    *bytes_read = p - str;

  if (bytes_written)
    *bytes_written = outp - dest;	/* Doesn't include '\0' */

  if (have_error)
    {
      g_free (dest);
      return NULL;
    }
  else
    return dest;
}

/**
 * g_convert_with_fallback:
 * @str:          the string to convert
 * @len:          the length of the string
 * @to_codeset:   name of character set into which to convert @str
 * @from_codeset: character set of @str.
 * @fallback:     UTF-8 string to use in place of character not
 *                present in the target encoding. (This must be
 *                in the target encoding), if %NULL, characters
 *                not in the target encoding will be represented
 *                as Unicode escapes \x{XXXX} or \x{XXXXXX}.
 * @bytes_read:   location to store the number of bytes in the
 *                input string that were successfully converted, or %NULL.
 *                Even if the conversion was succesful, this may be 
 *                less than len if there were partial characters
 *                at the end of the input. If the error
 *                G_CONVERT_ERROR_ILLEGAL_SEQUENCE occurs, the value
 *                stored will the byte fofset after the last valid
 *                input sequence.
 * @error:        location to store the error occuring, or %NULL to ignore
 *                errors. Any of the errors in #GConvertError may occur.
 *
 * Convert a string from one character set to another, possibly
 * including fallback sequences for characters not representable
 * in the output. Note that it is not guaranteed that the specification
 * for the fallback sequences in @fallback will be honored. Some
 * systems may do a approximate conversion from @from_codeset
 * to @to_codeset in their iconv() functions, in which case GLib
 * will simply return that approximate conversion.
 *
 * Return value: If the conversion was successful, a newly allocated
 *               NUL-terminated string, which must be freed with
 *               g_free. Otherwise %NULL and @error will be set.
 **/
gchar*
g_convert_with_fallback (const gchar *str,
			 gint         len,
			 const gchar *to_codeset,
			 const gchar *from_codeset,
			 gchar       *fallback,
			 gint        *bytes_read,
			 gint        *bytes_written,
			 GError     **error)
{
  gchar *utf8;
  gchar *dest;
  gchar *outp;
  const gchar *insert_str = NULL;
  const gchar *p;
  size_t inbytes_remaining;
  const gchar *save_p = NULL;
  size_t save_inbytes = 0;
  size_t outbytes_remaining;
  size_t err;
  iconv_t cd;
  size_t outbuf_size;
  gboolean have_error = FALSE;
  gboolean done = FALSE;

  GError *local_error = NULL;
  
  g_return_val_if_fail (str != NULL, NULL);
  g_return_val_if_fail (to_codeset != NULL, NULL);
  g_return_val_if_fail (from_codeset != NULL, NULL);
     
  if (len < 0)
    len = strlen (str);
  
  /* Try an exact conversion; we only proceed if this fails
   * due to an illegal sequence in the input string.
   */
  dest = g_convert (str, len, to_codeset, from_codeset, 
		    bytes_read, bytes_written, &local_error);
  if (!local_error)
    return dest;

  if (!g_error_matches (local_error, G_CONVERT_ERROR, G_CONVERT_ERROR_ILLEGAL_SEQUENCE))
    {
      g_propagate_error (error, local_error);
      return NULL;
    }
  else
    g_error_free (local_error);

  /* No go; to proceed, we need a converter from "UTF-8" to
   * to_codeset, and the string as UTF-8.
   */
  cd = open_converter (to_codeset, "UTF-8", error);
  if (cd == (iconv_t) -1)
    {
      if (bytes_read)
        *bytes_read = 0;
      
      if (bytes_written)
        *bytes_written = 0;
      
      return NULL;
    }

  utf8 = g_convert (str, len, "UTF-8", from_codeset, 
		    bytes_read, &inbytes_remaining, error);
  if (!utf8)
    return NULL;

  /* Now the heart of the code. We loop through the UTF-8 string, and
   * whenever we hit an offending character, we form fallback, convert
   * the fallback to the target codeset, and then go back to
   * converting the original string after finishing with the fallback.
   *
   * The variables save_p and save_inbytes store the input state
   * for the original string while we are converting the fallback
   */
  p = utf8;
  outbuf_size = len + 1; /* + 1 for nul in case len == 1 */
  outbytes_remaining = outbuf_size - 1; /* -1 for nul */
  outp = dest = g_malloc (outbuf_size);

  while (!done && !have_error)
    {
      err = iconv (cd, &p, &inbytes_remaining, &outp, &outbytes_remaining);

      if (err == (size_t) -1)
	{
	  switch (errno)
	    {
	    case EINVAL:
	      g_assert_not_reached();
	      break;
	    case E2BIG:
	      {
		size_t used = outp - dest;
		outbuf_size *= 2;
		dest = g_realloc (dest, outbuf_size);
		
		outp = dest + used;
		outbytes_remaining = outbuf_size - used - 1; /* -1 for nul */
		
		break;
	      }
	    case EILSEQ:
	      if (save_p)
		{
		  /* Error converting fallback string - fatal
		   */
		  g_set_error (error, G_CONVERT_ERROR, G_CONVERT_ERROR_ILLEGAL_SEQUENCE,
			       "Cannot convert fallback '%s' to codeset '%s'",
			       insert_str, to_codeset);
		  have_error = TRUE;
		  break;
		}
	      else
		{
		  if (!fallback)
		    { 
		      gunichar ch = g_utf8_get_char (p);
		      insert_str = g_strdup_printf ("\\x{%0*X}",
						    (ch < 0x10000) ? 4 : 6,
						    ch);
		    }
		  else
		    insert_str = fallback;
		  
		  save_p = g_utf8_next_char (p);
		  save_inbytes = inbytes_remaining - (save_p - p);
		  p = insert_str;
		  inbytes_remaining = strlen (p);
		}
	      break;
	    default:
	      g_set_error (error, G_CONVERT_ERROR, G_CONVERT_ERROR_OTHER,
			   "Error during conversion: %s",
			   strerror (errno));
	      have_error = TRUE;
	      break;
	    }
	}
      else
	{
	  if (save_p)
	    {
	      if (!fallback)
		g_free ((gchar *)insert_str);
	      p = save_p;
	      inbytes_remaining = save_inbytes;
	      save_p = NULL;
	    }
	  else
	    done = TRUE;
	}
    }

  /* Cleanup
   */
  *outp = '\0';
  
  iconv_close (cd);

  if (bytes_written)
    *bytes_written = outp - str;	/* Doesn't include '\0' */

  g_free (utf8);

  if (have_error)
    {
      if (save_p && !fallback)
	g_free ((gchar *)insert_str);
      g_free (dest);
      return NULL;
    }
  else
    return dest;
}
