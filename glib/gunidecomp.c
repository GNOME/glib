/* decomp.c - Character decomposition.
 *
 *  Copyright (C) 1999, 2000 Tom Tromey
 *  Copyright 2000 Red Hat, Inc.
 *
 * The Gnome Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * The Gnome Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with the Gnome Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 *   Boston, MA 02111-1307, USA.
 */

#include "glib.h"
#include "gunidecomp.h"

#include <config.h>

#include <stdlib.h>

/* We cheat a bit and cast type values to (char *).  We detect these
   using the &0xff trick.  */
#define CC(Page, Char) \
  (((((int) (combining_class_table[Page])) & 0xff) \
    == ((int) combining_class_table[Page])) \
   ? ((int) combining_class_table[Page]) \
   : (combining_class_table[Page][Char]))

#define COMBINING_CLASS(Char) \
     (((Char) > (UNICODE_LAST_CHAR)) ? 0 : CC((Char) >> 8, (Char) & 0xff))

/* Compute the canonical ordering of a string in-place.  */
void
g_unicode_canonical_ordering (gunichar *string,
			      size_t len)
{
  size_t i;
  int swap = 1;

  while (swap)
    {
      int last;
      swap = 0;
      last = COMBINING_CLASS (string[0]);
      for (i = 0; i < len - 1; ++i)
	{
	  int next = COMBINING_CLASS (string[i + 1]);
	  if (next != 0 && last > next)
	    {
	      size_t j;
	      /* Percolate item leftward through string.  */
	      for (j = i; j > 0; --j)
		{
		  gunichar t;
		  if (COMBINING_CLASS (string[j]) <= next)
		    break;
		  t = string[j + 1];
		  string[j + 1] = string[j];
		  string[j] = t;
		  swap = 1;
		}
	      /* We're re-entering the loop looking at the old
		 character again.  */
	      next = last;
	    }
	  last = next;
	}
    }
}

gunichar *
g_unicode_canonical_decomposition (gunichar ch,
				   size_t *result_len)
{
  gunichar *r = NULL;

  if (ch <= 0xffff)
    {
      int start = 0;
      int end = G_N_ELEMENTS (decomp_table);
      while (start != end)
	{
	  int half = (start + end) / 2;
	  if (ch == decomp_table[half].ch)
	    {
	      /* Found it.  */
	      int i, len;
	      /* We store as a double-nul terminated string.  */
	      for (len = 0; (decomp_table[half].expansion[len]
			     || decomp_table[half].expansion[len + 1]);
		   len += 2)
		;

	      /* We've counted twice as many bytes as there are
		 characters.  */
	      *result_len = len / 2;
	      r = malloc (len / 2 * sizeof (gunichar));

	      for (i = 0; i < len; i += 2)
		{
		  r[i / 2] = (decomp_table[half].expansion[i] << 8
			      | decomp_table[half].expansion[i + 1]);
		}
	      break;
	    }
	  else if (ch > decomp_table[half].ch)
	    start = half;
	  else
	    end = half;
	}
    }

  if (r == NULL)
    {
      /* Not in our table.  */
      r = malloc (sizeof (gunichar));
      *r = ch;
      *result_len = 1;
    }

  /* Supposedly following the Unicode 2.1.9 table means that the
     decompositions come out in canonical order.  I haven't tested
     this, but we rely on it here.  */
  return r;
}
