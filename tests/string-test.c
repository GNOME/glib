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
#undef G_LOG_DOMAIN

#include <stdio.h>
#include <string.h>
#include "glib.h"

int array[10000];
gboolean failed = FALSE;

#define	TEST(m,cond)	G_STMT_START { failed = !(cond); \
if (failed) \
  { if (!m) \
      g_print ("\n(%s:%d) failed for: %s\n", __FILE__, __LINE__, ( # cond )); \
    else \
      g_print ("\n(%s:%d) failed for: %s: (%s)\n", __FILE__, __LINE__, ( # cond ), (gchar*)m); \
  } \
else \
  g_print ("."); fflush (stdout); \
} G_STMT_END

#define	C2P(c)		((gpointer) ((long) (c)))
#define	P2C(p)		((gchar) ((long) (p)))

#define GLIB_TEST_STRING "el dorado "
#define GLIB_TEST_STRING_5 "el do"

typedef struct {
	guint age;
	gchar name[40];
} GlibTestInfo;

int
main (int   argc,
      char *argv[])
{
  GList *list, *t;
  GSList *slist, *st;
  GHashTable *hash_table;
  GMemChunk *mem_chunk;
  GStringChunk *string_chunk;
  GTimer *timer;
  gint nums[10] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
  gint morenums[10] = { 8, 9, 7, 0, 3, 2, 5, 1, 4, 6};
  gchar *string;

  gchar *mem[10000], *tmp_string, *tmp_string_2;
  gint i, j;
  GArray *garray;
  GPtrArray *gparray;
  GByteArray *gbarray;
  GString *string1, *string2;
  GTree *tree;
  char chars[62];
  GRelation *relation;
  GTuples *tuples;
  gint data [1024];
  GlibTestInfo *gti;
  struct {
    gchar *filename;
    gchar *dirname;
  } dirname_checks[] = {
#ifndef NATIVE_WIN32
    { "/", "/" },
    { "////", "/" },
    { ".////", "." },
    { ".", "." },
    { "..", "." },
    { "../", ".." },
    { "..////", ".." },
    { "", "." },
    { "a/b", "a" },
    { "a/b/", "a/b" },
    { "c///", "c" },
#else
    { "\\", "\\" },
    { ".\\\\\\\\", "." },
    { ".", "." },
    { "..", "." },
    { "..\\", ".." },
    { "..\\\\\\\\", ".." },
    { "", "." },
    { "a\\b", "a" },
    { "a\\b\\", "a\\b" },
    { "c\\\\\\", "c" },
#endif
  };
  guint n_dirname_checks = sizeof (dirname_checks) / sizeof (dirname_checks[0]);
  guint16 gu16t1 = 0x44afU, gu16t2 = 0xaf44U;
  guint32 gu32t1 = 0x02a7f109U, gu32t2 = 0x09f1a702U;
#ifdef G_HAVE_GINT64
  guint64 gu64t1 = G_GINT64_CONSTANT(0x1d636b02300a7aa7U),
	  gu64t2 = G_GINT64_CONSTANT(0xa77a0a30026b631dU);
#endif


  string_chunk = g_string_chunk_new (1024);

  for (i = 0; i < 100000; i ++)
    {
      tmp_string = g_string_chunk_insert (string_chunk, "hi pete");

      if (strcmp ("hi pete", tmp_string) != 0)
	g_error ("string chunks are broken.\n");
    }

  tmp_string_2 = g_string_chunk_insert_const (string_chunk, tmp_string);

  g_assert (tmp_string_2 != tmp_string &&
	    strcmp(tmp_string_2, tmp_string) == 0);

  tmp_string = g_string_chunk_insert_const (string_chunk, tmp_string);

  g_assert (tmp_string_2 == tmp_string);

  g_string_chunk_free (string_chunk);

  string1 = g_string_new ("hi pete!");
  string2 = g_string_new ("");

  g_assert (string1 != NULL);
  g_assert (string2 != NULL);
  g_assert (strcmp ("hi pete!", string1->str) == 0);
  g_assert (strcmp ("", string2->str) == 0);

  for (i = 0; i < 10000; i++)
    g_string_append_c (string1, 'a'+(i%26));

  g_assert((strlen("hi pete!") + 10000) == string1->len);
  g_assert((strlen("hi pete!") + 10000) == strlen(string1->str));

#if !(defined (_MSC_VER) || defined (__LCC__))
  /* MSVC and LCC use the same run-time C library, which doesn't like
     the %10000.10000f format... */
  g_string_sprintf (string2, "%s|%0100d|%s|%s|%0*d|%*.*f|%10000.10000f",
		    "this pete guy sure is a wuss, like he's the number ",
		    1,
		    " wuss.  everyone agrees.\n",
		    string1->str,
		    10, 666, 15, 15, 666.666666666, 666.666666666);
#else
  g_string_sprintf (string2, "%s|%0100d|%s|%s|%0*d|%*.*f|%100.100f",
		    "this pete guy sure is a wuss, like he's the number ",
		    1,
		    " wuss.  everyone agrees.\n",
		    string1->str,
		    10, 666, 15, 15, 666.666666666, 666.666666666);
#endif

  g_assert (g_strcasecmp ("FroboZZ", "frobozz") == 0);
  g_assert (g_strcasecmp ("frobozz", "frobozz") == 0);
  g_assert (g_strcasecmp ("frobozz", "FROBOZZ") == 0);
  g_assert (g_strcasecmp ("FROBOZZ", "froboz") != 0);
  g_assert (g_strcasecmp ("", "") == 0);
  g_assert (g_strcasecmp ("!#%&/()", "!#%&/()") == 0);
  g_assert (g_strcasecmp ("a", "b") < 0);
  g_assert (g_strcasecmp ("a", "B") < 0);
  g_assert (g_strcasecmp ("A", "b") < 0);
  g_assert (g_strcasecmp ("A", "B") < 0);
  g_assert (g_strcasecmp ("b", "a") > 0);
  g_assert (g_strcasecmp ("b", "A") > 0);
  g_assert (g_strcasecmp ("B", "a") > 0);
  g_assert (g_strcasecmp ("B", "A") > 0);

  g_assert(g_strdup(NULL) == NULL);
  string = g_strdup(GLIB_TEST_STRING);
  g_assert(string != NULL);
  g_assert(strcmp(string, GLIB_TEST_STRING) == 0);
  g_free(string);

  string = g_strconcat(GLIB_TEST_STRING, NULL);
  g_assert(string != NULL);
  g_assert(strcmp(string, GLIB_TEST_STRING) == 0);
  g_free(string);

  string = g_strconcat(GLIB_TEST_STRING, GLIB_TEST_STRING, 
  		       GLIB_TEST_STRING, NULL);
  g_assert(string != NULL);
  g_assert(strcmp(string, GLIB_TEST_STRING GLIB_TEST_STRING
  			  GLIB_TEST_STRING) == 0);
  g_free(string);
  
  string = g_strdup_printf ("%05d %-5s", 21, "test");
  g_assert (string != NULL);
  g_assert (strcmp(string, "00021 test ") == 0);
  g_free (string);

  return 0;
}

