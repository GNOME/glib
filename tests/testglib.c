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

#include <stdio.h>
#include <string.h>
#include "glib.h"

int array[10000];

void
my_hash_callback (gpointer key,
		  gpointer value,
		  gpointer user_data)
{
  int *d = value;
  *d = 1;
}

guint
my_hash (gconstpointer key)
{
  return (guint) *((const gint*) key);
}

gint
my_hash_compare (gconstpointer a,
		 gconstpointer b)
{
  return *((const gint*) a) == *((const gint*) b);
}

gint 
my_list_compare_one (gconstpointer a, gconstpointer b)
{
  gint one = *((const gint*)a);
  gint two = *((const gint*)b);
  return one-two;
}

gint 
my_list_compare_two (gconstpointer a, gconstpointer b)
{
  gint one = *((const gint*)a);
  gint two = *((const gint*)b);
  return two-one;
}

/* void
my_list_print (gpointer a, gpointer b)
{
  gint three = *((gint*)a);
  g_print("%d", three);
}; */

gint
my_compare (gconstpointer a,
	    gconstpointer b)
{
  const char *cha = a;
  const char *chb = b;

  return *cha - *chb;
}

gint
my_traverse (gpointer key,
	     gpointer value,
	     gpointer data)
{
  char *ch = key;
  g_print ("%c ", *ch);
  return FALSE;
}

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

  gchar *mem[10000], *tmp_string, *tmp_string_2;
  gint i, j;
  GArray *garray;
  GString *string1, *string2;
  GTree *tree;
  char chars[62];

  g_print ("checking size of gint8...%ld (should be 1)\n", (glong)sizeof (gint8));
  g_print ("checking size of gint16...%ld (should be 2)\n", (glong)sizeof (gint16));
  g_print ("checking size of gint32...%ld (should be 4)\n", (glong)sizeof (gint32));

  g_print ("checking doubly linked lists...");

  list = NULL;
  for (i = 0; i < 10; i++)
    list = g_list_append (list, &nums[i]);
  list = g_list_reverse (list);

  for (i = 0; i < 10; i++)
    {
      t = g_list_nth (list, i);
      if (*((gint*) t->data) != (9 - i))
	g_error ("Regular insert failed");
    }

  for (i = 0; i < 10; i++)
    if(g_list_position(list, g_list_nth (list, i)) != i)
      g_error("g_list_position does not seem to be the inverse of g_list_nth\n");

  g_list_free (list);
  list = NULL;
  
  for (i = 0; i < 10; i++)
    list = g_list_insert_sorted (list, &morenums[i], my_list_compare_one);

  /*
  g_print("\n");
  g_list_foreach (list, my_list_print, NULL);
  */

  for (i = 0; i < 10; i++)
    {
      t = g_list_nth (list, i);
      if (*((gint*) t->data) != i)
         g_error ("Sorted insert failed");
    }
    
  g_list_free (list);
  list = NULL;
  
  for (i = 0; i < 10; i++)
    list = g_list_insert_sorted (list, &morenums[i], my_list_compare_two);

  /*
  g_print("\n");
  g_list_foreach (list, my_list_print, NULL);
  */

  for (i = 0; i < 10; i++)
    {
      t = g_list_nth (list, i);
      if (*((gint*) t->data) != (9 - i))
         g_error ("Sorted insert failed");
    }
    
  g_list_free (list);

  g_print ("ok\n");


  g_print ("checking singly linked lists...");

  slist = NULL;
  for (i = 0; i < 10; i++)
    slist = g_slist_append (slist, &nums[i]);
  slist = g_slist_reverse (slist);

  for (i = 0; i < 10; i++)
    {
      st = g_slist_nth (slist, i);
      if (*((gint*) st->data) != (9 - i))
	g_error ("failed");
    }

  g_slist_free (slist);
  slist = NULL;

  for (i = 0; i < 10; i++)
    slist = g_slist_insert_sorted (slist, &morenums[i], my_list_compare_one);

  /*
  g_print("\n");
  g_slist_foreach (slist, my_list_print, NULL);
  */

  for (i = 0; i < 10; i++)
    {
      st = g_slist_nth (slist, i);
      if (*((gint*) st->data) != i)
         g_error ("Sorted insert failed");
    }
     
  g_slist_free(slist);
  slist = NULL;
   
  for (i = 0; i < 10; i++)
    slist = g_slist_insert_sorted (slist, &morenums[i], my_list_compare_two);

  /*
  g_print("\n");
  g_slist_foreach (slist, my_list_print, NULL);
  */

  for (i = 0; i < 10; i++)
    {
      st = g_slist_nth (slist, i);
      if (*((gint*) st->data) != (9 - i))
         g_error("Sorted insert failed");
    }
    
  g_slist_free(slist);

  g_print ("ok\n");


  g_print ("checking trees...\n");

  tree = g_tree_new (my_compare);
  i = 0;
  for (j = 0; j < 10; j++, i++)
    {
      chars[i] = '0' + j;
      g_tree_insert (tree, &chars[i], &chars[i]);
    }
  for (j = 0; j < 26; j++, i++)
    {
      chars[i] = 'A' + j;
      g_tree_insert (tree, &chars[i], &chars[i]);
    }
  for (j = 0; j < 26; j++, i++)
    {
      chars[i] = 'a' + j;
      g_tree_insert (tree, &chars[i], &chars[i]);
    }

  g_print ("tree height: %d\n", g_tree_height (tree));
  g_print ("tree nnodes: %d\n", g_tree_nnodes (tree));

  g_print ("tree: ");
  g_tree_traverse (tree, my_traverse, G_IN_ORDER, NULL);
  g_print ("\n");

  for (i = 0; i < 10; i++)
    g_tree_remove (tree, &chars[i]);

  g_print ("tree height: %d\n", g_tree_height (tree));
  g_print ("tree nnodes: %d\n", g_tree_nnodes (tree));

  g_print ("tree: ");
  g_tree_traverse (tree, my_traverse, G_IN_ORDER, NULL);
  g_print ("\n");

  g_print ("ok\n");


  g_print ("checking mem chunks...");

  mem_chunk = g_mem_chunk_new ("test mem chunk", 50, 100, G_ALLOC_AND_FREE);

  for (i = 0; i < 10000; i++)
    {
      mem[i] = g_chunk_new (gchar, mem_chunk);

      for (j = 0; j < 50; j++)
	mem[i][j] = i * j;
    }

  for (i = 0; i < 10000; i++)
    {
      g_mem_chunk_free (mem_chunk, mem[i]);
    }

  g_print ("ok\n");


  g_print ("checking hash tables...");

  hash_table = g_hash_table_new (my_hash, my_hash_compare);
  for (i = 0; i < 10000; i++)
    {
      array[i] = i;
      g_hash_table_insert (hash_table, &array[i], &array[i]);
    }
  g_hash_table_foreach (hash_table, my_hash_callback, NULL);

  for (i = 0; i < 10000; i++)
    if (array[i] == 0)
      g_print ("%d\n", i);

  for (i = 0; i < 10000; i++)
    g_hash_table_remove (hash_table, &array[i]);

  g_hash_table_destroy (hash_table);

  g_print ("ok\n");


  g_print ("checking string chunks...");

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

  g_print ("ok\n");


  g_print ("checking arrays...");

  garray = g_array_new (FALSE);
  for (i = 0; i < 10000; i++)
    g_array_append_val (garray, gint, i);

  for (i = 0; i < 10000; i++)
    if (g_array_index (garray, gint, i) != i)
      g_print ("uh oh: %d ( %d )\n", g_array_index (garray, gint, i), i);

  g_array_free (garray, TRUE);

  garray = g_array_new (FALSE);
  for (i = 0; i < 10000; i++)
    g_array_prepend_val (garray, gint, i);

  for (i = 0; i < 10000; i++)
    if (g_array_index (garray, gint, i) != (10000 - i - 1))
      g_print ("uh oh: %d ( %d )\n", g_array_index (garray, gint, i), 10000 - i - 1);

  g_array_free (garray, TRUE);

  g_print ("ok\n");


  g_print ("checking strings...");

  string1 = g_string_new ("hi pete!");
  string2 = g_string_new ("");

  g_assert (strcmp ("hi pete!", string1->str) == 0);

  for (i = 0; i < 10000; i++)
    g_string_append_c (string1, 'a'+(i%26));

  g_string_sprintf (string2, "%s|%0100d|%s|%s|%0*d|%*.*f|%10000.10000f",
		    "this pete guy sure is a wuss, like he's the number ",
		    1,
		    " wuss.  everyone agrees.\n",
		    string1->str,
		    10, 666, 15, 15, 666.666666666, 666.666666666);

  g_print ("ok\n");

  g_print ("checking timers...\n");

  timer = g_timer_new ();
  g_print ("  spinning for 3 seconds...\n");

  g_timer_start (timer);
  while (g_timer_elapsed (timer, NULL) < 3)
    ;

  g_timer_stop (timer);
  g_timer_destroy (timer);

  g_print ("ok\n");

  g_print ("checking g_strcasecmp...\n");

  /* g_debug (argv[0]); */


  return 0;
}
