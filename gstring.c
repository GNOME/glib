/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#include <glib.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>


typedef struct _GRealStringChunk GRealStringChunk;
typedef struct _GRealString      GRealString;

struct _GRealStringChunk
{
  GHashTable *const_table;
  GSList     *storage_list;
  gint        storage_next;
  gint        this_size;
  gint        default_size;
};

struct _GRealString
{
  gchar *str;
  gint   len;
  gint   alloc;
};


static GMemChunk *string_mem_chunk = NULL;

/* Hash Functions.
 */

gint
g_str_equal (gconstpointer v, gconstpointer v2)
{
  return strcmp ((const gchar*) v, (const gchar*)v2) == 0;
}

/* a char* hash function from ASU */
guint
g_str_hash (gconstpointer v)
{
  const char *s = (char*)v;
  const char *p;
  guint h=0, g;

  for(p = s; *p != '\0'; p += 1) {
    h = ( h << 4 ) + *p;
    if ( ( g = h & 0xf0000000 ) ) {
      h = h ^ (g >> 24);
      h = h ^ g;
    }
  }

  return h /* % M */;
}


/* String Chunks.
 */

GStringChunk*
g_string_chunk_new (gint default_size)
{
  GRealStringChunk *new_chunk = g_new (GRealStringChunk, 1);
  gint size = 1;

  while (size < default_size)
    size <<= 1;

  new_chunk->const_table       = NULL;
  new_chunk->storage_list      = NULL;
  new_chunk->storage_next      = size;
  new_chunk->default_size      = size;
  new_chunk->this_size         = size;

  return (GStringChunk*) new_chunk;
}

void
g_string_chunk_free (GStringChunk *fchunk)
{
  GRealStringChunk *chunk = (GRealStringChunk*) fchunk;
  GSList *tmp_list;

  g_return_if_fail (chunk != NULL);

  if (chunk->storage_list)
    {
      GListAllocator *tmp_allocator = g_slist_set_allocator (NULL);

      for (tmp_list = chunk->storage_list; tmp_list; tmp_list = tmp_list->next)
	g_free (tmp_list->data);

      g_slist_free (chunk->storage_list);

      g_slist_set_allocator (tmp_allocator);
    }

  if (chunk->const_table)
    g_hash_table_destroy (chunk->const_table);

  g_free (chunk);
}

gchar*
g_string_chunk_insert (GStringChunk *fchunk,
		       const gchar  *string)
{
  GRealStringChunk *chunk = (GRealStringChunk*) fchunk;
  gint len = strlen (string);
  char* pos;

  g_return_val_if_fail (chunk != NULL, NULL);

  if ((chunk->storage_next + len + 1) > chunk->this_size)
    {
      GListAllocator *tmp_allocator = g_slist_set_allocator (NULL);
      gint new_size = chunk->default_size;

      while (new_size < len+1)
	new_size <<= 1;

      chunk->storage_list = g_slist_prepend (chunk->storage_list,
					     g_new (char, new_size));

      chunk->this_size = new_size;
      chunk->storage_next = 0;

      g_slist_set_allocator (tmp_allocator);
    }

  pos = ((char*)chunk->storage_list->data) + chunk->storage_next;

  strcpy (pos, string);

  chunk->storage_next += len + 1;

  return pos;
}

gchar*
g_string_chunk_insert_const (GStringChunk *fchunk,
			     const gchar  *string)
{
  GRealStringChunk *chunk = (GRealStringChunk*) fchunk;
  char* lookup;

  g_return_val_if_fail (chunk != NULL, NULL);

  if (!chunk->const_table)
    chunk->const_table = g_hash_table_new (g_str_hash, g_str_equal);

  lookup = (char*) g_hash_table_lookup (chunk->const_table, (gchar *)string);

  if (!lookup)
    {
      lookup = g_string_chunk_insert (fchunk, string);
      g_hash_table_insert (chunk->const_table, lookup, lookup);
    }

  return lookup;
}

/* Strings.
 */
static gint
nearest_pow (gint num)
{
  gint n = 1;

  while (n < num)
    n <<= 1;

  return n;
}

static void
g_string_maybe_expand (GRealString* string, gint len)
{
  if (string->len + len >= string->alloc)
    {
      string->alloc = nearest_pow (string->len + len + 1);
      string->str = g_realloc (string->str, string->alloc);
    }
}

GString*
g_string_sized_new (guint dfl_size)
{
  GRealString *string;

  if (!string_mem_chunk)
    string_mem_chunk = g_mem_chunk_new ("string mem chunk",
					sizeof (GRealString),
					1024, G_ALLOC_AND_FREE);

  string = g_chunk_new (GRealString, string_mem_chunk);

  string->alloc = 0;
  string->len   = 0;
  string->str   = NULL;

  g_string_maybe_expand (string, MAX (dfl_size, 2));
  string->str[0] = 0;

  return (GString*) string;
}

GString*
g_string_new (const gchar *init)
{
  GString *string;

  string = g_string_sized_new (2);

  if (init)
    g_string_append (string, init);

  return string;
}

void
g_string_free (GString *string,
	       gint free_segment)
{
  g_return_if_fail (string != NULL);

  if (free_segment)
    g_free (string->str);

  g_mem_chunk_free (string_mem_chunk, string);
}

GString*
g_string_assign (GString *lval,
		 const gchar *rval)
{
  g_string_truncate (lval, 0);
  g_string_append (lval, rval);

  return lval;
}

GString*
g_string_truncate (GString* fstring,
		   gint len)
{
  GRealString *string = (GRealString*)fstring;

  g_return_val_if_fail (string != NULL, NULL);

  string->len = len;

  string->str[len] = 0;

  return fstring;
}

GString*
g_string_append (GString *fstring,
		 const gchar *val)
{
  GRealString *string = (GRealString*)fstring;
  int len;

  g_return_val_if_fail (string != NULL, NULL);
  g_return_val_if_fail (val != NULL, fstring);
  
  len = strlen (val);
  g_string_maybe_expand (string, len);

  strcpy (string->str + string->len, val);

  string->len += len;

  return fstring;
}

GString*
g_string_append_c (GString *fstring,
		   gchar c)
{
  GRealString *string = (GRealString*)fstring;

  g_return_val_if_fail (string != NULL, NULL);
  g_string_maybe_expand (string, 1);

  string->str[string->len++] = c;
  string->str[string->len] = 0;

  return fstring;
}

GString*
g_string_prepend (GString *fstring,
		  const gchar *val)
{
  GRealString *string = (GRealString*)fstring;
  gint len;

  g_return_val_if_fail (string != NULL, NULL);
  g_return_val_if_fail (val != NULL, fstring);

  len = strlen (val);
  g_string_maybe_expand (string, len);

  g_memmove (string->str + len, string->str, string->len);

  strncpy (string->str, val, len);

  string->len += len;

  string->str[string->len] = 0;

  return fstring;
}

GString*
g_string_prepend_c (GString *fstring,
		    gchar    c)
{
  GRealString *string = (GRealString*)fstring;

  g_return_val_if_fail (string != NULL, NULL);
  g_string_maybe_expand (string, 1);

  g_memmove (string->str + 1, string->str, string->len);

  string->str[0] = c;

  string->len += 1;

  string->str[string->len] = 0;

  return fstring;
}

GString*
g_string_insert (GString     *fstring,
		 gint         pos,
		 const gchar *val)
{
  GRealString *string = (GRealString*)fstring;
  gint len;

  g_return_val_if_fail (string != NULL, NULL);
  g_return_val_if_fail (val != NULL, fstring);
  g_return_val_if_fail (pos >= 0, fstring);
  g_return_val_if_fail (pos <= string->len, fstring);

  len = strlen (val);
  g_string_maybe_expand (string, len);

  g_memmove (string->str + pos + len, string->str + pos, string->len - pos);

  strncpy (string->str + pos, val, len);

  string->len += len;

  string->str[string->len] = 0;

  return fstring;
}

GString *
g_string_insert_c (GString *fstring,
		   gint     pos,
		   gchar    c)
{
  GRealString *string = (GRealString*)fstring;

  g_return_val_if_fail (string != NULL, NULL);
  g_return_val_if_fail (pos <= string->len, fstring);

  g_string_maybe_expand (string, 1);

  g_memmove (string->str + pos + 1, string->str + pos, string->len - pos);

  string->str[pos] = c;

  string->len += 1;

  string->str[string->len] = 0;

  return fstring;
}

GString*
g_string_erase (GString *fstring,
		gint pos,
		gint len)
{
  GRealString *string = (GRealString*)fstring;

  g_return_val_if_fail (string != NULL, NULL);
  g_return_val_if_fail (len >= 0, fstring);
  g_return_val_if_fail (pos >= 0, fstring);
  g_return_val_if_fail (pos <= string->len, fstring);
  g_return_val_if_fail (pos + len <= string->len, fstring);

  if (pos + len < string->len)
    g_memmove (string->str + pos, string->str + pos + len, string->len - (pos + len));

  string->len -= len;
  
  string->str[string->len] = 0;

  return fstring;
}

GString*
g_string_down (GString *fstring)
{
  GRealString *string = (GRealString*)fstring;
  gchar *s;

  g_return_val_if_fail (string != NULL, NULL);

  s = string->str;

  while (*s)
    {
      *s = tolower (*s);
      s++;
    }

  return fstring;
}

GString*
g_string_up (GString *fstring)
{
  GRealString *string = (GRealString*)fstring;
  gchar *s;

  g_return_val_if_fail (string != NULL, NULL);

  s = string->str;

  while (*s)
    {
      *s = toupper (*s);
      s++;
    }

  return fstring;
}

static int
get_length_upper_bound (const gchar* fmt, va_list *args)
{
  int len = 0;
  int short_int;
  int long_int;
  int done;
  char *tmp;

  while (*fmt)
    {
      char c = *fmt++;

      short_int = FALSE;
      long_int = FALSE;

      if (c == '%')
	{
	  done = FALSE;
	  while (*fmt && !done)
	    {
	      switch (*fmt++)
		{
		case '*':
		  len += va_arg(*args, int);
		  break;
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
		  fmt -= 1;
		  len += strtol (fmt, (char **)&fmt, 10);
		  break;
		case 'h':
		  short_int = TRUE;
		  break;
		case 'l':
		  long_int = TRUE;
		  break;

		  /* I ignore 'q' and 'L', they're not portable anyway. */

		case 's':
		  tmp = va_arg(*args, char *);
		  if(tmp)
		    len += strlen (tmp);
		  else
		    len += strlen ("(null)");
		  done = TRUE;
		  break;
		case 'd':
		case 'i':
		case 'o':
		case 'u':
		case 'x':
		case 'X':
		  if (long_int)
		    (void)va_arg (*args, long);
		  else if (short_int)
		    (void)va_arg (*args, int);
		  else
		    (void)va_arg (*args, int);
		  len += 32;
		  done = TRUE;
		  break;
		case 'D':
		case 'O':
		case 'U':
		  (void)va_arg (*args, long);
		  len += 32;
		  done = TRUE;
		  break;
		case 'e':
		case 'E':
		case 'f':
		case 'g':
		  (void)va_arg (*args, double);
		  len += 32;
		  done = TRUE;
		  break;
		case 'c':
		  (void)va_arg (*args, int);
		  len += 1;
		  done = TRUE;
		  break;
		case 'p':
		case 'n':
		  (void)va_arg (*args, void*);
		  len += 32;
		  done = TRUE;
		  break;
		case '%':
		  len += 1;
		  done = TRUE;
		  break;
		default:
		  break;
		}
	    }
	}
      else
	len += 1;
    }

  return len;
}

char*
g_vsprintf (const gchar *fmt,
	    va_list *args,
	    va_list *args2)
{
  static gchar *buf = NULL;
  static gint   alloc = 0;

  gint len = get_length_upper_bound (fmt, args);

  if (len >= alloc)
    {
      if (buf)
	g_free (buf);

      alloc = nearest_pow (MAX(len + 1, 1024));

      buf = g_new (char, alloc);
    }

  vsprintf (buf, fmt, *args2);

  return buf;
}

static void
g_string_sprintfa_int (GString *string,
		       const gchar *fmt,
		       va_list *args,
		       va_list *args2)
{
  g_string_append (string, g_vsprintf (fmt, args, args2));
}

void
g_string_sprintf (GString *string,
		  const gchar *fmt,
		  ...)
{
  va_list args, args2;

  va_start(args, fmt);
  va_start(args2, fmt);

  g_string_truncate (string, 0);

  g_string_sprintfa_int (string, fmt, &args, &args2);

  va_end(args);
  va_end(args2);
}

void
g_string_sprintfa (GString *string,
		   const gchar *fmt,
		   ...)
{
  va_list args, args2;

  va_start(args, fmt);
  va_start(args2, fmt);

  g_string_sprintfa_int (string, fmt, &args, &args2);

  va_end(args);
  va_end(args2);
}
