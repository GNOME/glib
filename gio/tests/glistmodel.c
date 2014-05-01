/*
 * Copyright © 2014 Lars Uebernickel
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2 of the licence or (at
 * your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Lars Uebernickel <lars@uebernic.de>
 */

#include <gio/gio.h>

static void
test_store_boundaries (void)
{
  GListStore *store;
  GMenuItem *item;

  store = g_list_store_new (G_TYPE_MENU_ITEM);

  item = g_menu_item_new (NULL, NULL);
  g_object_add_weak_pointer (G_OBJECT (item), (gpointer *) &item);

  /* remove an item from an empty list */
  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL, "*g_sequence*");
  g_list_store_remove (store, 0);
  g_test_assert_expected_messages ();

  /* don't allow inserting an item past the end ... */
  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL, "*g_sequence*");
  g_list_store_insert (store, 1, item);
  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (store)), ==, 0);
  g_test_assert_expected_messages ();

  /* ... except exactly at the end */
  g_list_store_insert (store, 0, item);
  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (store)), ==, 1);

  /* remove a non-existing item at exactly the end of the list */
  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL, "*g_sequence*");
  g_list_store_remove (store, 1);
  g_test_assert_expected_messages ();

  g_list_store_remove (store, 0);
  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (store)), ==, 0);

  /* splice beyond the end of the list */
  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL, "*position*");
  g_list_store_splice (store, 1, 0, 0, NULL);
  g_test_assert_expected_messages ();

  /* remove items from an empty list */
  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL, "*position*");
  g_list_store_splice (store, 0, 1, 0, NULL);
  g_test_assert_expected_messages ();

  g_list_store_append (store, item);
  g_list_store_splice (store, 0, 1, 1, (gpointer *) &item);
  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (store)), ==, 1);

  /* remove more items than exist */
  g_test_expect_message (G_LOG_DOMAIN, G_LOG_LEVEL_CRITICAL, "*position*");
  g_list_store_splice (store, 0, 5, 0, NULL);
  g_test_assert_expected_messages ();
  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (store)), ==, 1);

  g_object_unref (store);
  g_object_unref (item);
  g_assert_null (item);
}

static void
test_store_refcounts (void)
{
  GListStore *store;
  GMenuItem *items[10];
  GMenuItem *tmp;
  guint i;
  guint n_items;

  store = g_list_store_new (G_TYPE_MENU_ITEM);

  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (store)), ==, 0);
  g_assert_null (g_list_model_get_item (G_LIST_MODEL (store), 0));

  n_items = G_N_ELEMENTS (items);
  for (i = 0; i < n_items; i++)
    {
      items[i] = g_menu_item_new (NULL, NULL);
      g_object_add_weak_pointer (G_OBJECT (items[i]), (gpointer *) &items[i]);
      g_list_store_append (store, items[i]);

      g_object_unref (items[i]);
      g_assert_nonnull (items[i]);
    }

  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (store)), ==, n_items);
  g_assert_null (g_list_model_get_item (G_LIST_MODEL (store), n_items));

  tmp = g_list_model_get_item (G_LIST_MODEL (store), 3);
  g_assert (tmp == items[3]);
  g_object_unref (tmp);

  g_list_store_remove (store, 4);
  g_assert_null (items[4]);
  n_items--;
  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (store)), ==, n_items);
  g_assert_null (g_list_model_get_item (G_LIST_MODEL (store), n_items));

  g_object_unref (store);
  for (i = 0; i < G_N_ELEMENTS (items); i++)
    g_assert_null (items[i]);
}

int main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/glistmodel/store/boundaries", test_store_boundaries);
  g_test_add_func ("/glistmodel/store/refcounts", test_store_refcounts);

  return g_test_run ();
}
