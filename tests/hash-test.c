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

static gboolean
node_build_string (GNode    *node,
		   gpointer  data)
{
  gchar **p = data;
  gchar *string;
  gchar c[2] = "_";

  c[0] = P2C (node->data);

  string = g_strconcat (*p ? *p : "", c, NULL);
  g_free (*p);
  *p = string;

  return FALSE;
}

static void
g_node_test (void)
{
  GNode *root;
  GNode *node;
  GNode *node_B;
  GNode *node_F;
  GNode *node_G;
  GNode *node_J;
  guint i;
  gchar *tstring;

  g_print ("checking n-way trees: ");
  failed = FALSE;

  root = g_node_new (C2P ('A'));
  TEST (NULL, g_node_depth (root) == 1 && g_node_max_height (root) == 1);

  node_B = g_node_new (C2P ('B'));
  g_node_append (root, node_B);
  TEST (NULL, root->children == node_B);

  g_node_append_data (node_B, C2P ('E'));
  g_node_prepend_data (node_B, C2P ('C'));
  g_node_insert (node_B, 1, g_node_new (C2P ('D')));

  node_F = g_node_new (C2P ('F'));
  g_node_append (root, node_F);
  TEST (NULL, root->children->next == node_F);

  node_G = g_node_new (C2P ('G'));
  g_node_append (node_F, node_G);
  node_J = g_node_new (C2P ('J'));
  g_node_prepend (node_G, node_J);
  g_node_insert (node_G, 42, g_node_new (C2P ('K')));
  g_node_insert_data (node_G, 0, C2P ('H'));
  g_node_insert (node_G, 1, g_node_new (C2P ('I')));

  TEST (NULL, g_node_depth (root) == 1);
  TEST (NULL, g_node_max_height (root) == 4);
  TEST (NULL, g_node_depth (node_G->children->next) == 4);
  TEST (NULL, g_node_n_nodes (root, G_TRAVERSE_LEAFS) == 7);
  TEST (NULL, g_node_n_nodes (root, G_TRAVERSE_NON_LEAFS) == 4);
  TEST (NULL, g_node_n_nodes (root, G_TRAVERSE_ALL) == 11);
  TEST (NULL, g_node_max_height (node_F) == 3);
  TEST (NULL, g_node_n_children (node_G) == 4);
  TEST (NULL, g_node_find_child (root, G_TRAVERSE_ALL, C2P ('F')) == node_F);
  TEST (NULL, g_node_find (root, G_LEVEL_ORDER, G_TRAVERSE_NON_LEAFS, C2P ('I')) == NULL);
  TEST (NULL, g_node_find (root, G_IN_ORDER, G_TRAVERSE_LEAFS, C2P ('J')) == node_J);

  for (i = 0; i < g_node_n_children (node_B); i++)
    {
      node = g_node_nth_child (node_B, i);
      TEST (NULL, P2C (node->data) == ('C' + i));
    }
  
  for (i = 0; i < g_node_n_children (node_G); i++)
    TEST (NULL, g_node_child_position (node_G, g_node_nth_child (node_G, i)) == i);

  /* we have built:                    A
   *                                 /   \
   *                               B       F
   *                             / | \       \
   *                           C   D   E       G
   *                                         / /\ \
   *                                       H  I  J  K
   *
   * for in-order traversal, 'G' is considered to be the "left"
   * child of 'F', which will cause 'F' to be the last node visited.
   */

  tstring = NULL;
  g_node_traverse (root, G_PRE_ORDER, G_TRAVERSE_ALL, -1, node_build_string, &tstring);
  TEST (tstring, strcmp (tstring, "ABCDEFGHIJK") == 0);
  g_free (tstring); tstring = NULL;
  g_node_traverse (root, G_POST_ORDER, G_TRAVERSE_ALL, -1, node_build_string, &tstring);
  TEST (tstring, strcmp (tstring, "CDEBHIJKGFA") == 0);
  g_free (tstring); tstring = NULL;
  g_node_traverse (root, G_IN_ORDER, G_TRAVERSE_ALL, -1, node_build_string, &tstring);
  TEST (tstring, strcmp (tstring, "CBDEAHGIJKF") == 0);
  g_free (tstring); tstring = NULL;
  g_node_traverse (root, G_LEVEL_ORDER, G_TRAVERSE_ALL, -1, node_build_string, &tstring);
  TEST (tstring, strcmp (tstring, "ABFCDEGHIJK") == 0);
  g_free (tstring); tstring = NULL;
  
  g_node_traverse (root, G_LEVEL_ORDER, G_TRAVERSE_LEAFS, -1, node_build_string, &tstring);
  TEST (tstring, strcmp (tstring, "CDEHIJK") == 0);
  g_free (tstring); tstring = NULL;
  g_node_traverse (root, G_PRE_ORDER, G_TRAVERSE_NON_LEAFS, -1, node_build_string, &tstring);
  TEST (tstring, strcmp (tstring, "ABFG") == 0);
  g_free (tstring); tstring = NULL;

  g_node_reverse_children (node_B);
  g_node_reverse_children (node_G);

  g_node_traverse (root, G_LEVEL_ORDER, G_TRAVERSE_ALL, -1, node_build_string, &tstring);
  TEST (tstring, strcmp (tstring, "ABFEDCGKJIH") == 0);
  g_free (tstring); tstring = NULL;
  
  g_node_destroy (root);

  /* allocation tests */

  root = g_node_new (NULL);
  node = root;

  for (i = 0; i < 2048; i++)
    {
      g_node_append (node, g_node_new (NULL));
      if ((i%5) == 4)
	node = node->children->next;
    }
  TEST (NULL, g_node_max_height (root) > 100);
  TEST (NULL, g_node_n_nodes (root, G_TRAVERSE_ALL) == 1 + 2048);

  g_node_destroy (root);
  
  if (!failed)
    g_print ("ok\n");
}

gboolean
my_hash_callback_remove (gpointer key,
			 gpointer value,
			 gpointer user_data)
{
  int *d = value;

  if ((*d) % 2)
    return TRUE;

  return FALSE;
}

void
my_hash_callback_remove_test (gpointer key,
			      gpointer value,
			      gpointer user_data)
{
  int *d = value;

  if ((*d) % 2)
    g_print ("bad!\n");
}

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


  hash_table = g_hash_table_new (my_hash, my_hash_compare);
  for (i = 0; i < 10000; i++)
    {
      array[i] = i;
      g_hash_table_insert (hash_table, &array[i], &array[i]);
    }
  g_hash_table_foreach (hash_table, my_hash_callback, NULL);

  for (i = 0; i < 10000; i++)
    if (array[i] == 0)
      g_assert_not_reached();

  for (i = 0; i < 10000; i++)
    g_hash_table_remove (hash_table, &array[i]);

  for (i = 0; i < 10000; i++)
    {
      array[i] = i;
      g_hash_table_insert (hash_table, &array[i], &array[i]);
    }

  if (g_hash_table_foreach_remove (hash_table, my_hash_callback_remove, NULL) != 5000 ||
      g_hash_table_size (hash_table) != 5000)
    g_assert_not_reached();

  g_hash_table_foreach (hash_table, my_hash_callback_remove_test, NULL);


  g_hash_table_destroy (hash_table);

  return 0;

}
