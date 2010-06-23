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

#undef G_DISABLE_ASSERT
#undef G_LOG_DOMAIN

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "glib.h"

int array[10000];
gboolean failed = FALSE;

#define TEST(m,cond)    G_STMT_START { failed = !(cond); \
if (failed) \
  { if (!m) \
      g_print ("\n(%s:%d) failed for: %s\n", __FILE__, __LINE__, ( # cond )); \
    else \
      g_print ("\n(%s:%d) failed for: %s: (%s)\n", __FILE__, __LINE__, ( # cond ), (gchar*)m); \
      exit(1); \
  } \
} G_STMT_END

#define C2P(c)          ((gpointer) ((long) (c)))
#define P2C(p)          ((gchar) ((long) (p)))

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
traversal_test (void)
{
  GNode *root;
  GNode *node_B;
  GNode *node_D;
  GNode *node_F;
  GNode *node_G;
  GNode *node_J;
  gchar *tstring;

  root = g_node_new (C2P ('A'));
  node_B = g_node_new (C2P ('B'));
  g_node_append (root, node_B);
  g_node_append_data (node_B, C2P ('E'));
  g_node_prepend_data (node_B, C2P ('C'));
  node_D = g_node_new (C2P ('D'));
  g_node_insert (node_B, 1, node_D);
  node_F = g_node_new (C2P ('F'));
  g_node_append (root, node_F);
  node_G = g_node_new (C2P ('G'));
  g_node_append (node_F, node_G);
  node_J = g_node_new (C2P ('J'));
  g_node_prepend (node_G, node_J);
  g_node_insert (node_G, 42, g_node_new (C2P ('K')));
  g_node_insert_data (node_G, 0, C2P ('H'));
  g_node_insert (node_G, 1, g_node_new (C2P ('I')));

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

  g_node_traverse (root, G_PRE_ORDER, G_TRAVERSE_ALL, -1, node_build_string, &tstring);
  g_assert_cmpstr (tstring, ==,  "ABCDEFGHIJK");
  g_free (tstring); tstring = NULL;
  g_node_traverse (root, G_POST_ORDER, G_TRAVERSE_ALL, -1, node_build_string, &tstring);
  g_assert_cmpstr (tstring, ==, "CDEBHIJKGFA");
  g_free (tstring); tstring = NULL;
  g_node_traverse (root, G_IN_ORDER, G_TRAVERSE_ALL, -1, node_build_string, &tstring);
  g_assert_cmpstr (tstring, ==, "CBDEAHGIJKF");
  g_free (tstring); tstring = NULL;
  g_node_traverse (root, G_LEVEL_ORDER, G_TRAVERSE_ALL, -1, node_build_string, &tstring);
  g_assert_cmpstr (tstring, ==, "ABFCDEGHIJK");
  g_free (tstring); tstring = NULL;
  
  g_node_traverse (root, G_LEVEL_ORDER, G_TRAVERSE_LEAFS, -1, node_build_string, &tstring);
  g_assert_cmpstr (tstring, ==, "CDEHIJK");
  g_free (tstring); tstring = NULL;
  g_node_traverse (root, G_PRE_ORDER, G_TRAVERSE_NON_LEAFS, -1, node_build_string, &tstring);
  g_assert_cmpstr (tstring, ==, "ABFG");
  g_free (tstring); tstring = NULL;

  g_node_reverse_children (node_B);
  g_node_reverse_children (node_G);

  g_node_traverse (root, G_LEVEL_ORDER, G_TRAVERSE_ALL, -1, node_build_string, &tstring);
  g_assert_cmpstr (tstring, ==, "ABFEDCGKJIH");
  g_free (tstring); tstring = NULL;
  
  g_node_append (node_D, g_node_new (C2P ('L')));
  g_node_append (node_D, g_node_new (C2P ('M')));

  g_node_traverse (root, G_LEVEL_ORDER, G_TRAVERSE_ALL, -1, node_build_string, &tstring);
  g_assert_cmpstr (tstring, ==, "ABFEDCGLMKJIH");
  g_free (tstring); tstring = NULL;

  g_node_destroy (root);
}

static void
construct_test (void)
{
  GNode *root;
  GNode *node;
  GNode *node_B;
  GNode *node_D;
  GNode *node_F;
  GNode *node_G;
  GNode *node_J;
  guint i;

  root = g_node_new (C2P ('A'));
  g_assert_cmpint (g_node_depth (root), ==, 1);
  g_assert_cmpint (g_node_max_height (root), ==, 1);

  node_B = g_node_new (C2P ('B'));
  g_node_append (root, node_B);
  g_assert (root->children == node_B);

  g_node_append_data (node_B, C2P ('E'));
  g_node_prepend_data (node_B, C2P ('C'));
  node_D = g_node_new (C2P ('D'));
  g_node_insert (node_B, 1, node_D);

  node_F = g_node_new (C2P ('F'));
  g_node_append (root, node_F);
  g_assert (root->children->next == node_F);

  node_G = g_node_new (C2P ('G'));
  g_node_append (node_F, node_G);
  node_J = g_node_new (C2P ('J'));
  g_node_prepend (node_G, node_J);
  g_node_insert (node_G, 42, g_node_new (C2P ('K')));
  g_node_insert_data (node_G, 0, C2P ('H'));
  g_node_insert (node_G, 1, g_node_new (C2P ('I')));

  /* we have built:                    A
   *                                 /   \
   *                               B       F
   *                             / | \       \
   *                           C   D   E       G
   *                                         / /\ \
   *                                       H  I  J  K
   */
  g_assert_cmpint (g_node_depth (root), ==, 1);
  g_assert_cmpint (g_node_max_height (root), ==, 4);
  g_assert_cmpint (g_node_depth (node_G->children->next), ==, 4);
  g_assert_cmpint (g_node_n_nodes (root, G_TRAVERSE_LEAFS), ==, 7);
  g_assert_cmpint (g_node_n_nodes (root, G_TRAVERSE_NON_LEAFS), ==, 4);
  g_assert_cmpint (g_node_n_nodes (root, G_TRAVERSE_ALL), ==, 11);
  g_assert_cmpint (g_node_max_height (node_F), ==, 3);
  g_assert_cmpint (g_node_n_children (node_G), ==, 4);
  g_assert (g_node_find_child (root, G_TRAVERSE_ALL, C2P ('F')) == node_F);
  g_assert (g_node_find (root, G_LEVEL_ORDER, G_TRAVERSE_NON_LEAFS, C2P ('I')) == NULL);
  g_assert (g_node_find (root, G_IN_ORDER, G_TRAVERSE_LEAFS, C2P ('J')) == node_J);

  for (i = 0; i < g_node_n_children (node_B); i++)
    {
      node = g_node_nth_child (node_B, i);
      g_assert_cmpint (P2C (node->data), ==, ('C' + i));
    }

  for (i = 0; i < g_node_n_children (node_G); i++)
    g_assert_cmpint (g_node_child_position (node_G, g_node_nth_child (node_G, i)), ==, i);
}

static void
allocation_test (void)
{
  GNode *root;
  GNode *node;
  gint i;

  root = g_node_new (NULL);
  node = root;

  for (i = 0; i < 2048; i++)
    {
      g_node_append (node, g_node_new (NULL));
      if ((i % 5) == 4)
        node = node->children->next;
    }
  g_assert_cmpint (g_node_max_height (root), >, 100);
  g_assert_cmpint (g_node_n_nodes (root, G_TRAVERSE_ALL), ==, 1 + 2048);

  g_node_destroy (root);
}


int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/node/allocation", allocation_test);
  g_test_add_func ("/node/construction", construct_test);
  g_test_add_func ("/node/traversal", traversal_test);

  return g_test_run ();
}

