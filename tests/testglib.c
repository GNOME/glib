/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
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

/*
 * Modified by the GLib Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GLib Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GLib at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "config.h"

#undef G_DISABLE_ASSERT
#undef G_LOG_DOMAIN

#ifdef GLIB_COMPILATION
#undef GLIB_COMPILATION
#endif

#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "glib.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef G_OS_WIN32
#include <io.h>			/* For read(), write() etc */
#endif

static int array[10000];
static gboolean failed = FALSE;

/* We write (m ? m : "") even in the m != NULL case to suppress a warning with GCC-3.1
 */
#define	TEST(m,cond)	G_STMT_START { failed = !(cond); \
if (failed) \
  { if (!m) \
      g_print ("\n(%s:%d) failed for: %s\n", __FILE__, __LINE__, ( # cond )); \
    else \
      g_print ("\n(%s:%d) failed for: %s: (%s)\n", __FILE__, __LINE__, ( # cond ), (gchar*)(m ? m : "")); \
  } \
else \
  g_print ("."); fflush (stdout); \
} G_STMT_END

#define	C2P(c)		((gpointer) ((long) (c)))
#define	P2C(p)		((gchar) ((long) (p)))

#define GLIB_TEST_STRING "el dorado "
#define GLIB_TEST_STRING_5 "el do"

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
  gchar *tstring, *cstring;

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

  cstring = NULL;
  node = g_node_copy (root);
  TEST (NULL, g_node_n_nodes (root, G_TRAVERSE_ALL) == g_node_n_nodes (node, G_TRAVERSE_ALL));
  TEST (NULL, g_node_max_height (root) == g_node_max_height (node));
  g_node_traverse (root, G_IN_ORDER, G_TRAVERSE_ALL, -1, node_build_string, &tstring);
  g_node_traverse (node, G_IN_ORDER, G_TRAVERSE_ALL, -1, node_build_string, &cstring);
  TEST (cstring, strcmp (tstring, cstring) == 0);
  g_free (tstring); tstring = NULL;
  g_free (cstring); cstring = NULL;
  g_node_destroy (node);

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

static gboolean
my_hash_callback_remove (gpointer key,
			 gpointer value,
			 gpointer user_data)
{
  int *d = value;

  if ((*d) % 2)
    return TRUE;

  return FALSE;
}

static void
my_hash_callback_remove_test (gpointer key,
			      gpointer value,
			      gpointer user_data)
{
  int *d = value;

  if ((*d) % 2)
    g_print ("bad!\n");
}

static void
my_hash_callback (gpointer key,
		  gpointer value,
		  gpointer user_data)
{
  int *d = value;
  *d = 1;
}

static guint
my_hash (gconstpointer key)
{
  return (guint) *((const gint*) key);
}

static gboolean
my_hash_equal (gconstpointer a,
	       gconstpointer b)
{
  return *((const gint*) a) == *((const gint*) b);
}

static gint
my_list_compare_one (gconstpointer a, gconstpointer b)
{
  gint one = *((const gint*)a);
  gint two = *((const gint*)b);
  return one-two;
}

static gint
my_list_compare_two (gconstpointer a, gconstpointer b)
{
  gint one = *((const gint*)a);
  gint two = *((const gint*)b);
  return two-one;
}

/* static void
my_list_print (gpointer a, gpointer b)
{
  gint three = *((gint*)a);
  g_print("%d", three);
}; */

static gint
my_compare (gconstpointer a,
	    gconstpointer b)
{
  const char *cha = a;
  const char *chb = b;

  return *cha - *chb;
}

static gint
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
  GTimer *timer, *timer2;
  gint nums[10] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
  gint morenums[10] = { 8, 9, 7, 0, 3, 2, 5, 1, 4, 6};
  gchar *string;

  gchar *mem[10000], *tmp_string = NULL, *tmp_string_2;
  gint i, j;
  GArray *garray;
  GPtrArray *gparray;
  GByteArray *gbarray;
  GString *string1, *string2;
  const gchar *charset;
  GTree *tree;
  char chars[62];
  GRelation *relation;
  GTuples *tuples;
  gint data [1024];
  struct {
    gchar *filename;
    gchar *dirname;
  } dirname_checks[] = {
    { "/", "/" },
    { "////", "/" },
    { ".////", "." },
    { "../", ".." },
    { "..////", ".." },
    { "a/b", "a" },
    { "a/b/", "a/b" },
    { "c///", "c" },
#ifdef G_OS_WIN32
    { "\\", "\\" },
    { ".\\\\\\\\", "." },
    { "..\\", ".." },
    { "..\\\\\\\\", ".." },
    { "a\\b", "a" },
    { "a\\b/", "a\\b" },
    { "a/b\\", "a/b" },
    { "c\\\\/", "c" },
    { "//\\", "/" },
#endif
#ifdef G_WITH_CYGWIN
    { "//server/share///x", "//server/share" },
#endif
    { ".", "." },
    { "..", "." },
    { "", "." },
  };
  guint n_dirname_checks = G_N_ELEMENTS (dirname_checks);

  struct {
    gchar *filename;
    gchar *without_root;
  } skip_root_checks[] = {
    { "/", "" },
    { "//", "" },
    { "/foo", "foo" },
    { "//foo", "foo" },
    { "a/b", NULL },
#ifdef G_OS_WIN32
    { "\\", "" },
    { "\\foo", "foo" },
    { "\\\\server\\foo", "" },
    { "\\\\server\\foo\\bar", "bar" },
    { "a\\b", NULL },
#endif
#ifdef G_WITH_CYGWIN
    { "//server/share///x", "//x" },
#endif
    { ".", NULL },
    { "", NULL },
  };
  guint n_skip_root_checks = G_N_ELEMENTS (skip_root_checks);

#ifndef G_DISABLE_ASSERT
  guint16 gu16t1 = 0x44afU, gu16t2 = 0xaf44U;
  guint32 gu32t1 = 0x02a7f109U, gu32t2 = 0x09f1a702U;
  guint64 gu64t1 = G_GINT64_CONSTANT(0x1d636b02300a7aa7U),
	  gu64t2 = G_GINT64_CONSTANT(0xa77a0a30026b631dU);
#endif
  const char hello[] = "Hello, World";
  const int hellolen = sizeof (hello) - 1;
  int fd;
  char template[32];
  GError *error;
  char *name_used;
#ifdef G_OS_WIN32
  /* Can't calculate GLib DLL name at runtime. */
  gchar *glib_dll = "libglib-2.0-0.dll";
#endif
#ifdef G_WITH_CYGWIN
  gchar *glib_dll = "cygglib-2.0-0.dll";
#endif

  g_print ("TestGLib v%u.%u.%u (i:%u b:%u)\n",
	   glib_major_version,
	   glib_minor_version,
	   glib_micro_version,
	   glib_interface_age,
	   glib_binary_age);

  string = g_get_current_dir ();
  g_print ("cwd: %s\n", string);
  g_free (string);
  g_print ("user: %s\n", g_get_user_name ());
  g_print ("real: %s\n", g_get_real_name ());
  g_print ("home: %s\n", g_get_home_dir ());
  g_print ("tmp-dir: %s\n", g_get_tmp_dir ());

  /* type sizes */
  g_print ("checking size of gint8: %"    G_GSIZE_FORMAT, sizeof (gint8));
  TEST (NULL, sizeof (gint8) == 1);
  g_print ("\nchecking size of gint16: %" G_GSIZE_FORMAT, sizeof (gint16));
  TEST (NULL, sizeof (gint16) == 2);
  g_print ("\nchecking size of gint32: %" G_GSIZE_FORMAT, sizeof (gint32));
  TEST (NULL, sizeof (gint32) == 4);
  g_print ("\nchecking size of gsize: %"  G_GSIZE_FORMAT, sizeof (gsize));
  g_print ("\nchecking size of gint64: %" G_GSIZE_FORMAT, sizeof (gint64));
  TEST (NULL, sizeof (gint64) == 8);
  g_print ("\n");

  g_print ("checking g_path_get_basename()...");
  string = g_path_get_basename (G_DIR_SEPARATOR_S "foo" G_DIR_SEPARATOR_S "dir" G_DIR_SEPARATOR_S);
  g_assert (strcmp (string, "dir") == 0);
  g_free (string);
  string = g_path_get_basename (G_DIR_SEPARATOR_S "foo" G_DIR_SEPARATOR_S "file");
  g_assert (strcmp (string, "file") == 0);
  g_free (string);
  g_print ("ok\n");
#ifdef G_OS_WIN32
  string = g_path_get_basename ("/foo/dir/");
  g_assert (strcmp (string, "dir") == 0);
  g_free (string);
  string = g_path_get_basename ("/foo/file");
  g_assert (strcmp (string, "file") == 0);
  g_free (string);
  g_print ("ok\n");
#endif

  g_print ("checking g_path_get_dirname()...");
  for (i = 0; i < n_dirname_checks; i++)
    {
      gchar *dirname;

      dirname = g_path_get_dirname (dirname_checks[i].filename);
      if (strcmp (dirname, dirname_checks[i].dirname) != 0)
	{
	  g_print ("\nfailed for \"%s\"==\"%s\" (returned: \"%s\")\n",
		   dirname_checks[i].filename,
		   dirname_checks[i].dirname,
		   dirname);
	  n_dirname_checks = 0;
	}
      g_free (dirname);
    }
  if (n_dirname_checks)
    g_print ("ok\n");

  g_print ("checking g_path_skip_root()...");
  for (i = 0; i < n_skip_root_checks; i++)
    {
      const gchar *skipped;

      skipped = g_path_skip_root (skip_root_checks[i].filename);
      if ((skipped && !skip_root_checks[i].without_root) ||
	  (!skipped && skip_root_checks[i].without_root) ||
	  ((skipped && skip_root_checks[i].without_root) &&
	   strcmp (skipped, skip_root_checks[i].without_root)))
	{
	  g_print ("\nfailed for \"%s\"==\"%s\" (returned: \"%s\")\n",
		   skip_root_checks[i].filename,
		   (skip_root_checks[i].without_root ?
		    skip_root_checks[i].without_root : "<NULL>"),
		   (skipped ? skipped : "<NULL>"));
	  n_skip_root_checks = 0;
	}
    }
  if (n_skip_root_checks)
    g_print ("ok\n");

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
  list = NULL;

  for (i = 0; i < 10; i++)
    list = g_list_prepend (list, &morenums[i]);

  list = g_list_sort (list, my_list_compare_two);

  /*
  g_print("\n");
  g_list_foreach (list, my_list_print, NULL);
  */

  for (i = 0; i < 10; i++)
    {
      t = g_list_nth (list, i);
      if (*((gint*) t->data) != (9 - i))
         g_error ("Merge sort failed");
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
  slist = NULL;

  for (i = 0; i < 10; i++)
    slist = g_slist_prepend (slist, &morenums[i]);

  slist = g_slist_sort (slist, my_list_compare_two);

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


  g_print ("checking binary trees...\n");

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
  g_tree_foreach (tree, my_traverse, NULL);
  g_print ("\n");

  for (i = 0; i < 10; i++)
    g_tree_remove (tree, &chars[i]);

  g_print ("tree height: %d\n", g_tree_height (tree));
  g_print ("tree nnodes: %d\n", g_tree_nnodes (tree));

  g_print ("tree: ");
  g_tree_foreach (tree, my_traverse, NULL);
  g_print ("\n");

  g_print ("ok\n");


  /* check n-way trees */
  g_node_test ();

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

  hash_table = g_hash_table_new (my_hash, my_hash_equal);
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

  for (i = 0; i < 10000; i++)
    {
      array[i] = i;
      g_hash_table_insert (hash_table, &array[i], &array[i]);
    }

  if (g_hash_table_foreach_remove (hash_table, my_hash_callback_remove, NULL) != 5000 ||
      g_hash_table_size (hash_table) != 5000)
    g_print ("bad!\n");

  g_hash_table_foreach (hash_table, my_hash_callback_remove_test, NULL);


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

  garray = g_array_new (FALSE, FALSE, sizeof (gint));
  for (i = 0; i < 10000; i++)
    g_array_append_val (garray, i);

  for (i = 0; i < 10000; i++)
    if (g_array_index (garray, gint, i) != i)
      g_print ("uh oh: %d ( %d )\n", g_array_index (garray, gint, i), i);

  g_array_free (garray, TRUE);

  garray = g_array_new (FALSE, FALSE, sizeof (gint));
  for (i = 0; i < 100; i++)
    g_array_prepend_val (garray, i);

  for (i = 0; i < 100; i++)
    if (g_array_index (garray, gint, i) != (100 - i - 1))
      g_print ("uh oh: %d ( %d )\n", g_array_index (garray, gint, i), 100 - i - 1);

  g_array_free (garray, TRUE);

  g_print ("ok\n");


  g_print ("checking strings...");

  string1 = g_string_new ("hi pete!");
  string2 = g_string_new ("");

  g_assert (strcmp ("hi pete!", string1->str) == 0);

  for (i = 0; i < 10000; i++)
    g_string_append_c (string1, 'a'+(i%26));

#ifndef G_OS_WIN32
  /* MSVC, mingw32 and LCC use the same run-time C library, which doesn't like
     the %10000.10000f format... */
  g_string_printf (string2, "%s|%0100d|%s|%s|%0*d|%*.*f|%10000.10000f",
		   "this pete guy sure is a wuss, like he's the number ",
		   1,
		   " wuss.  everyone agrees.\n",
		   string1->str,
		   10, 666, 15, 15, 666.666666666, 666.666666666);
#else
  g_string_printf (string2, "%s|%0100d|%s|%s|%0*d|%*.*f|%100.100f",
		   "this pete guy sure is a wuss, like he's the number ",
		   1,
		   " wuss.  everyone agrees.\n",
		   string1->str,
		   10, 666, 15, 15, 666.666666666, 666.666666666);
#endif

  g_print ("string2 length = %lu...\n", (gulong)string2->len);
  string2->str[70] = '\0';
  g_print ("first 70 chars:\n%s\n", string2->str);
  string2->str[141] = '\0';
  g_print ("next 70 chars:\n%s\n", string2->str+71);
  string2->str[212] = '\0';
  g_print ("and next 70:\n%s\n", string2->str+142);
  g_print ("last 70 chars:\n%s\n", string2->str+string2->len - 70);

  g_string_free (string1, TRUE);
  g_string_free (string2, TRUE);

  /* append */
  string1 = g_string_new ("firsthalf");
  g_string_append (string1, "lasthalf");
  g_assert (strcmp (string1->str, "firsthalflasthalf") == 0);
  g_string_free (string1, TRUE);

  /* append_len */

  string1 = g_string_new ("firsthalf");
  g_string_append_len (string1, "lasthalfjunkjunk", strlen ("lasthalf"));
  g_assert (strcmp (string1->str, "firsthalflasthalf") == 0);
  g_string_free (string1, TRUE);  
  
  /* prepend */
  string1 = g_string_new ("lasthalf");
  g_string_prepend (string1, "firsthalf");
  g_assert (strcmp (string1->str, "firsthalflasthalf") == 0);
  g_string_free (string1, TRUE);

  /* prepend_len */
  string1 = g_string_new ("lasthalf");
  g_string_prepend_len (string1, "firsthalfjunkjunk", strlen ("firsthalf"));
  g_assert (strcmp (string1->str, "firsthalflasthalf") == 0);
  g_string_free (string1, TRUE);
  
  /* insert */
  string1 = g_string_new ("firstlast");
  g_string_insert (string1, 5, "middle");
  g_assert (strcmp (string1->str, "firstmiddlelast") == 0);
  g_string_free (string1, TRUE);

  /* insert with pos == end of the string */
  string1 = g_string_new ("firstmiddle");
  g_string_insert (string1, strlen ("firstmiddle"), "last");
  g_assert (strcmp (string1->str, "firstmiddlelast") == 0);
  g_string_free (string1, TRUE);
  
  /* insert_len */

  string1 = g_string_new ("firstlast");
  g_string_insert_len (string1, 5, "middlejunkjunk", strlen ("middle"));
  g_assert (strcmp (string1->str, "firstmiddlelast") == 0);
  g_string_free (string1, TRUE);

  /* insert_len with magic -1 pos for append */
  string1 = g_string_new ("first");
  g_string_insert_len (string1, -1, "lastjunkjunk", strlen ("last"));
  g_assert (strcmp (string1->str, "firstlast") == 0);
  g_string_free (string1, TRUE);
  
  /* insert_len with magic -1 len for strlen-the-string */
  string1 = g_string_new ("first");
  g_string_insert_len (string1, 5, "last", -1);
  g_assert (strcmp (string1->str, "firstlast") == 0);
  g_string_free (string1, TRUE);
  
  g_print ("ok\n");

  /* g_string_equal */
  string1 = g_string_new ("test");
  string2 = g_string_new ("te");
  g_assert (! g_string_equal(string1, string2));
  g_string_append (string2, "st");
  g_assert (g_string_equal(string1, string2));
  g_string_free (string1, TRUE);
  g_string_free (string2, TRUE);
  
  /* Check handling of embedded ASCII 0 (NUL) characters in GString. */
  string1 = g_string_new ("fiddle");
  string2 = g_string_new ("fiddle");
  g_assert (g_string_equal(string1, string2));
  g_string_append_c(string1, '\0');
  g_assert (! g_string_equal(string1, string2));
  g_string_append_c(string2, '\0');
  g_assert (g_string_equal(string1, string2));
  g_string_append_c(string1, 'x');
  g_string_append_c(string2, 'y');
  g_assert (! g_string_equal(string1, string2));
  g_assert (string1->len == 8);
  g_string_append(string1, "yzzy");
  g_assert (string1->len == 12);
  g_assert ( memcmp(string1->str, "fiddle\0xyzzy", 13) == 0);
  g_string_insert(string1, 1, "QED");
  g_assert ( memcmp(string1->str, "fQEDiddle\0xyzzy", 16) == 0);
  g_string_free (string1, TRUE);
  g_string_free (string2, TRUE);
  
  g_print ("test positional printf formats (not supported): ");
  string = g_strdup_printf ("%.*s%s", 5, "a", "b");
  tmp_string = g_strdup_printf ("%2$*1$s", 5, "c");
  g_print ("%s%s\n", string, tmp_string);
  g_free (tmp_string);
  g_free (string);

  g_print ("checking timers...\n");
  
  timer = g_timer_new ();
  g_print ("  spinning for 3 seconds...\n");

  g_timer_start (timer);
  while (g_timer_elapsed (timer, NULL) < 3)
    ;

  g_timer_stop (timer);
  g_timer_destroy (timer);

  g_print ("ok\n");

  g_print ("checking g_timer_continue...\n");

  timer2 = g_timer_new ();

  g_print ("\trun for 1 second...\n");
  timer = g_timer_new();
  g_usleep(G_USEC_PER_SEC); /* run timer for 1 second */
  g_timer_stop(timer);

  g_print ("\tstop for 1 second...\n");
  g_usleep(G_USEC_PER_SEC); /* wait for 1 second */
  g_print ("\trun for 2 seconds...\n");

  g_timer_continue(timer);
  g_usleep(2*G_USEC_PER_SEC); /* run timer for 2 seconds */
  g_timer_stop(timer);

  g_print ("\tstop for 1.5 seconds...\n");
  g_usleep((3*G_USEC_PER_SEC)/2); /* wait for 1.5 seconds */
  g_print ("\trun for 0.2 seconds...\n");

  g_timer_continue(timer);
  g_usleep(G_USEC_PER_SEC/5); /* run timer for 0.2 seconds */
  g_timer_stop(timer);

  g_print ("\tstop for 4 seconds...\n");
  g_usleep(4*G_USEC_PER_SEC); /* wait for 4 seconds */
  g_print ("\trun for 5.8 seconds...\n");

  g_timer_continue(timer);
  g_usleep((29*G_USEC_PER_SEC)/5); /* run timer for 5.8 seconds */
  g_timer_stop(timer);

  g_print ("\t=> total elapsed = %.2f seconds (should be: 9.00 seconds)\n\n", g_timer_elapsed(timer, NULL));

  if (g_timer_elapsed(timer, NULL) > 8.8 && g_timer_elapsed(timer, NULL) < 9.2)
    g_print ("g_timer_continue ... ok\n\n");
  else
    g_print ("g_timer_continue ... ***** FAILED *****\n\n");

  g_timer_stop(timer2);

  if (g_timer_elapsed(timer2, NULL) > (8.8+6.5) && g_timer_elapsed(timer2, NULL) < (9.2+6.5))
    g_print ("timer2 ... ok\n\n");
  else
    g_print ("timer2 ... ***** FAILED *****\n\n");

  g_timer_destroy(timer);
  g_timer_destroy(timer2);

  g_print ("checking g_ascii_strcasecmp...");
  g_assert (g_ascii_strcasecmp ("FroboZZ", "frobozz") == 0);
  g_assert (g_ascii_strcasecmp ("frobozz", "frobozz") == 0);
  g_assert (g_ascii_strcasecmp ("frobozz", "FROBOZZ") == 0);
  g_assert (g_ascii_strcasecmp ("FROBOZZ", "froboz") > 0);
  g_assert (g_ascii_strcasecmp ("", "") == 0);
  g_assert (g_ascii_strcasecmp ("!#%&/()", "!#%&/()") == 0);
  g_assert (g_ascii_strcasecmp ("a", "b") < 0);
  g_assert (g_ascii_strcasecmp ("a", "B") < 0);
  g_assert (g_ascii_strcasecmp ("A", "b") < 0);
  g_assert (g_ascii_strcasecmp ("A", "B") < 0);
  g_assert (g_ascii_strcasecmp ("b", "a") > 0);
  g_assert (g_ascii_strcasecmp ("b", "A") > 0);
  g_assert (g_ascii_strcasecmp ("B", "a") > 0);
  g_assert (g_ascii_strcasecmp ("B", "A") > 0);

  g_print ("ok\n");

  g_print ("checking g_strdup...");
  g_assert(g_strdup(NULL) == NULL);
  string = g_strdup(GLIB_TEST_STRING);
  g_assert(string != NULL);
  g_assert(strcmp(string, GLIB_TEST_STRING) == 0);
  g_free(string);

  g_print ("ok\n");

  g_print ("checking g_strconcat...");
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
  g_print ("ok\n");
  

  g_print("checking g_strlcpy/g_strlcat...");
  /* The following is a torture test for strlcpy/strlcat, with lots of
   * checking; normal users wouldn't use them this way!
   */
  string = g_malloc (6);
  *(string + 5) = 'Z'; /* guard value, shouldn't change during test */
  *string = 'q';
  g_assert (g_strlcpy(string, "" , 5) == 0);
  g_assert ( *string == '\0' );
  *string = 'q';
  g_assert (g_strlcpy(string, "abc" , 5) == 3);
  g_assert ( *(string + 3) == '\0' );
  g_assert (g_str_equal(string, "abc"));
  g_assert (g_strlcpy(string, "abcd" , 5) == 4);
  g_assert ( *(string + 4) == '\0' );
  g_assert ( *(string + 5) == 'Z' );
  g_assert (g_str_equal(string, "abcd"));
  g_assert (g_strlcpy(string, "abcde" , 5) == 5);
  g_assert ( *(string + 4) == '\0' );
  g_assert ( *(string + 5) == 'Z' );
  g_assert (g_str_equal(string, "abcd"));
  g_assert (g_strlcpy(string, "abcdef" , 5) == 6);
  g_assert ( *(string + 4) == '\0' );
  g_assert ( *(string + 5) == 'Z' );
  g_assert (g_str_equal(string, "abcd"));
  *string = 'Y';
  *(string + 1)= '\0';
  g_assert (g_strlcpy(string, "Hello" , 0) == 5);
  g_assert (*string == 'Y');
  *string = '\0';
  g_assert (g_strlcat(string, "123" , 5) == 3);
  g_assert ( *(string + 3) == '\0' );
  g_assert (g_str_equal(string, "123"));
  g_assert (g_strlcat(string, "" , 5) == 3);
  g_assert ( *(string + 3) == '\0' );
  g_assert (g_str_equal(string, "123"));
  g_assert (g_strlcat(string, "4", 5) == 4);
  g_assert (g_str_equal(string, "1234"));
  g_assert (g_strlcat(string, "5", 5) == 5);
  g_assert ( *(string + 4) == '\0' );
  g_assert (g_str_equal(string, "1234"));
  g_assert ( *(string + 5) == 'Z' );
  *string = 'Y';
  *(string + 1)= '\0';
  g_assert (g_strlcat(string, "123" , 0) == 3);
  g_assert (*string == 'Y');
  
  /* A few more tests, demonstrating more "normal" use  */
  g_assert (g_strlcpy(string, "hi", 5) == 2);
  g_assert (g_str_equal(string, "hi"));
  g_assert (g_strlcat(string, "t", 5) == 3);
  g_assert (g_str_equal(string, "hit"));
  g_free(string);

  g_print ("ok\n");
  
  
  g_print ("checking g_strdup_printf...");
  string = g_strdup_printf ("%05d %-5s", 21, "test");
  g_assert (string != NULL);
  g_assert (strcmp(string, "00021 test ") == 0);
  g_free (string);

  g_print ("ok\n");

  /* g_debug (argv[0]); */

  /* Relation tests */

  g_print ("checking relations...");

  relation = g_relation_new (2);

  g_relation_index (relation, 0, g_int_hash, g_int_equal);
  g_relation_index (relation, 1, g_int_hash, g_int_equal);

  for (i = 0; i < 1024; i += 1)
    data[i] = i;

  for (i = 1; i < 1023; i += 1)
    {
      g_relation_insert (relation, data + i, data + i + 1);
      g_relation_insert (relation, data + i, data + i - 1);
    }

  for (i = 2; i < 1022; i += 1)
    {
      g_assert (! g_relation_exists (relation, data + i, data + i));
      g_assert (! g_relation_exists (relation, data + i, data + i + 2));
      g_assert (! g_relation_exists (relation, data + i, data + i - 2));
    }

  for (i = 1; i < 1023; i += 1)
    {
      g_assert (g_relation_exists (relation, data + i, data + i + 1));
      g_assert (g_relation_exists (relation, data + i, data + i - 1));
    }

  for (i = 2; i < 1022; i += 1)
    {
      g_assert (g_relation_count (relation, data + i, 0) == 2);
      g_assert (g_relation_count (relation, data + i, 1) == 2);
    }

  g_assert (g_relation_count (relation, data, 0) == 0);

  g_assert (g_relation_count (relation, data + 42, 0) == 2);
  g_assert (g_relation_count (relation, data + 43, 1) == 2);
  g_assert (g_relation_count (relation, data + 41, 1) == 2);
  g_relation_delete (relation, data + 42, 0);
  g_assert (g_relation_count (relation, data + 42, 0) == 0);
  g_assert (g_relation_count (relation, data + 43, 1) == 1);
  g_assert (g_relation_count (relation, data + 41, 1) == 1);

  tuples = g_relation_select (relation, data + 200, 0);

  g_assert (tuples->len == 2);

#if 0
  for (i = 0; i < tuples->len; i += 1)
    {
      printf ("%d %d\n",
	      *(gint*) g_tuples_index (tuples, i, 0),
	      *(gint*) g_tuples_index (tuples, i, 1));
    }
#endif

  g_assert (g_relation_exists (relation, data + 300, data + 301));
  g_relation_delete (relation, data + 300, 0);
  g_assert (!g_relation_exists (relation, data + 300, data + 301));

  g_tuples_destroy (tuples);

  g_relation_destroy (relation);

  relation = NULL;

  g_print ("ok\n");

  g_print ("checking pointer arrays...");

  gparray = g_ptr_array_new ();
  for (i = 0; i < 10000; i++)
    g_ptr_array_add (gparray, GINT_TO_POINTER (i));

  for (i = 0; i < 10000; i++)
    if (g_ptr_array_index (gparray, i) != GINT_TO_POINTER (i))
      g_print ("array fails: %p ( %p )\n", g_ptr_array_index (gparray, i), GINT_TO_POINTER (i));

  g_ptr_array_free (gparray, TRUE);

  g_print ("ok\n");


  g_print ("checking byte arrays...");

  gbarray = g_byte_array_new ();
  for (i = 0; i < 10000; i++)
    g_byte_array_append (gbarray, (guint8*) "abcd", 4);

  for (i = 0; i < 10000; i++)
    {
      g_assert (gbarray->data[4*i] == 'a');
      g_assert (gbarray->data[4*i+1] == 'b');
      g_assert (gbarray->data[4*i+2] == 'c');
      g_assert (gbarray->data[4*i+3] == 'd');
    }

  g_byte_array_free (gbarray, TRUE);
  g_print ("ok\n");

  g_printerr ("g_log tests:");
  g_warning ("harmless warning with parameters: %d %s %#x", 42, "Boo", 12345);
  g_message ("the next warning is a test:");
  string = NULL;
  g_print (string);

  g_print ("checking endian macros (host is ");
#if G_BYTE_ORDER == G_BIG_ENDIAN
  g_print ("big endian)...");
#else
  g_print ("little endian)...");
#endif
  g_assert (GUINT16_SWAP_LE_BE (gu16t1) == gu16t2);  
  g_assert (GUINT32_SWAP_LE_BE (gu32t1) == gu32t2);  
  g_assert (GUINT64_SWAP_LE_BE (gu64t1) == gu64t2);  

  g_print ("ok\n");

  if (g_get_charset (&charset))
    g_print ("current charset is UTF-8: %s\n", charset);
  else
    g_print ("current charset is not UTF-8: %s\n", charset);

#ifdef G_PLATFORM_WIN32
  g_print ("current locale: %s\n", g_win32_getlocale ());
  g_print ("GLib DLL name tested for: %s\n", glib_dll);

  g_print ("GLib installation directory, from Registry entry for %s if available: %s\n",
	   GETTEXT_PACKAGE,
	   g_win32_get_package_installation_directory (GETTEXT_PACKAGE, NULL));
  g_print ("Ditto, or from GLib DLL name: %s\n",
	   g_win32_get_package_installation_directory (GETTEXT_PACKAGE, glib_dll));
  g_print ("Ditto, only from GLib DLL name: %s\n",
	   g_win32_get_package_installation_directory (NULL, glib_dll));
  g_print ("locale subdirectory of GLib installation directory: %s\n",
	   g_win32_get_package_installation_subdirectory (NULL, glib_dll, "lib\\locale"));
  g_print ("GTK+ 2.0 installation directory, if available: %s\n",
	   g_win32_get_package_installation_directory ("gtk20", NULL));

  g_print ("found more.com as %s\n", g_find_program_in_path ("more.com"));
  g_print ("found regedit as %s\n", g_find_program_in_path ("regedit"));

#endif

  g_print ("checking file functions...\n");

  strcpy (template, "foobar");
  fd = g_mkstemp (template);
  if (fd != -1)
    g_print ("g_mkstemp works even if template doesn't end in XXXXXX\n");
  close (fd);
  strcpy (template, "fooXXXXXX");
  fd = g_mkstemp (template);
  if (fd == -1)
    g_print ("g_mkstemp didn't work for template %s\n", template);
  i = write (fd, hello, hellolen);
  if (i == -1)
    g_print ("write() failed: %s\n", g_strerror (errno));
  else if (i != hellolen)
    g_print ("write() should have written %d bytes, wrote %d\n", hellolen, i);

  lseek (fd, 0, 0);
  i = read (fd, chars, sizeof (chars));
  if (i == -1)
    g_print ("read() failed: %s\n", g_strerror (errno));
  else if (i != hellolen)
    g_print ("read() should have read %d bytes, got %d\n", hellolen, i);

  chars[i] = 0;
  if (strcmp (chars, hello) != 0)
    g_print ("wrote '%s', but got '%s'\n", hello, chars);

  close (fd);
  remove (template);

  error = NULL;
  strcpy (template, "zap" G_DIR_SEPARATOR_S "barXXXXXX");
  fd = g_file_open_tmp (template, &name_used, &error);
  if (fd != -1)
    g_print ("g_file_open_tmp works even if template contains '%s'\n",
	     G_DIR_SEPARATOR_S);
  else
    g_print ("g_file_open_tmp correctly returns error: %s\n",
	     error->message);
  close (fd);
  g_clear_error (&error);

#ifdef G_OS_WIN32
  strcpy (template, "zap/barXXXXXX");
  fd = g_file_open_tmp (template, &name_used, &error);
  if (fd != -1)
    g_print ("g_file_open_tmp works even if template contains '/'\n");
  else
    g_print ("g_file_open_tmp correctly returns error: %s\n",
	     error->message);
  close (fd);
  g_clear_error (&error);
#endif

  strcpy (template, "zapXXXXXX");
  fd = g_file_open_tmp (template, &name_used, &error);
  if (fd == -1)
    g_print ("g_file_open_tmp didn't work for template '%s': %s\n",
	     template, error->message);
  else
    g_print ("g_file_open_tmp for template '%s' used name '%s'\n",
	     template, name_used);
  close (fd);
  g_clear_error (&error);
  remove (name_used);

  fd = g_file_open_tmp (NULL, &name_used, &error);
  if (fd == -1)
    g_print ("g_file_open_tmp didn't work for a NULL template: %s\n",
	     error->message);
  else
    g_print ("g_file_open_tmp for NULL template used name '%s'\n",
	     name_used);
  close (fd);
  g_clear_error (&error);
  remove (name_used);
  
  return 0;
}

