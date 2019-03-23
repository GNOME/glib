/* -*- mode: C; c-file-style: "gnu" -*- */
/* xdgmimealias.c: Private file.  Datastructure for storing the aliases.
 *
 * More info can be found at http://www.freedesktop.org/standards/
 *
 * Copyright (C) 2004  Red Hat, Inc.
 * Copyright (C) 2004  Matthias Clasen <mclasen@redhat.com>
 *
 * Licensed under the Academic Free License version 2.0
 * Or under the following terms:
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "xdgmimealias.h"
#include "xdgmimeint.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <glib.h>

#ifndef	FALSE
#define	FALSE	(0)
#endif

#ifndef	TRUE
#define	TRUE	(!FALSE)
#endif

typedef struct XdgAlias XdgAlias;

struct XdgAlias 
{
  char *alias;
  char *mime_type;
};

struct XdgAliasList
{
  struct XdgAlias *aliases;
  int n_aliases;
};

XdgAliasList *
_xdg_mime_alias_list_new (void)
{
  XdgAliasList *list;

  list = malloc (sizeof (XdgAliasList));

  list->aliases = NULL;
  list->n_aliases = 0;

  return list;
}

void         
_xdg_mime_alias_list_free (XdgAliasList *list)
{
  int i;

  if (list->aliases)
    {
      for (i = 0; i < list->n_aliases; i++)
	{
	  free (list->aliases[i].alias);
	  free (list->aliases[i].mime_type);
	}
      free (list->aliases);
    }
  free (list);
}

static int
alias_entry_cmp (const void *v1, const void *v2)
{
  return _xdg_mime_mime_type_cmp_ext (((XdgAlias *)v1)->alias, ((XdgAlias *)v2)->alias);
}

const char  *
_xdg_mime_alias_list_lookup (XdgAliasList *list,
			     const char   *alias)
{
  XdgAlias *entry;
  XdgAlias key;

  if (list->n_aliases > 0)
    {
      key.alias = (char *)alias;
      key.mime_type = NULL;

      entry = bsearch (&key, list->aliases, list->n_aliases,
		       sizeof (XdgAlias), alias_entry_cmp);
      if (entry)
        return entry->mime_type;
    }

  return NULL;
}

void
_xdg_mime_alias_read_from_file (XdgAliasList *list,
				const char   *file_name)
{
  char *contents, *contents_end;
  gsize contents_len;
  char *line, *next_line, *eol;
  int alloc;

  if (!g_file_get_contents (file_name, &contents, &contents_len, NULL))
    return;

  alloc = list->n_aliases + 16;
  list->aliases = realloc (list->aliases, alloc * sizeof (XdgAlias));

  contents_end = contents + contents_len;
  next_line = contents;
  while (next_line != NULL)
    {
      size_t contents_left;
      char *sep, *mime;
      line = next_line;
      contents_left = contents_end - line;

      eol = memchr (line, '\n', contents_left);
      if (eol != NULL)
        {
          next_line = eol + 1;
        }
      else
        {
          next_line = NULL;
          eol = contents_end;
        }

      if (line[0] == '#')
        continue;

      sep = memchr (line, ' ', (size_t) (eol - line));
      if (sep == NULL)
        continue;

      *(sep++) = '\000';

      if (list->n_aliases == alloc)
        {
          alloc <<= 1;
          list->aliases = realloc (list->aliases, alloc * sizeof (XdgAlias));
        }

      list->aliases[list->n_aliases].alias = strdup (line);
      mime = malloc ((size_t) (eol - sep) + 1);
      memcpy (mime, sep, (size_t) (eol - sep));
      mime[(size_t) (eol - sep)] = '\000';
      list->aliases[list->n_aliases].mime_type = mime;
      list->n_aliases++;
    }

  list->aliases = realloc (list->aliases, list->n_aliases * sizeof (XdgAlias));

  g_free (contents);

  if (list->n_aliases > 1)
    qsort (list->aliases, list->n_aliases, 
           sizeof (XdgAlias), alias_entry_cmp);
}


#ifdef NOT_USED_IN_GIO

void
_xdg_mime_alias_list_dump (XdgAliasList *list)
{
  int i;

  if (list->aliases)
    {
      for (i = 0; i < list->n_aliases; i++)
	{
	  printf ("%s %s\n", 
		  list->aliases[i].alias,
		  list->aliases[i].mime_type);
	}
    }
}

#endif
