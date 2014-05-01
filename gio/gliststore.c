/*
 * Copyright © 2014 Lars Uebernickel
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
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Lars Uebernickel <lars@uebernic.de>
 */

#include "config.h"

#include "gliststore.h"
#include "glistmodel.h"

/**
 * SECTION:glistmodel
 * @title: GListStore
 * @short_description: A simple implementation of #GListModel
 * @include: gio/gio.h
 *
 * #GListStore is a simple implementation of #GListModel that stores all
 * items in memory.
 */

typedef GObjectClass GListStoreClass;

struct _GListStore
{
  GObject parent;

  GType item_type;
  GSequence *items;
};

enum
{
  PROP_0,
  PROP_ITEM_TYPE,
  N_PROPERTIES
};

static void g_list_store_iface_init (GListModelInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GListStore, g_list_store, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, g_list_store_iface_init));

static void
g_list_store_dispose (GObject *object)
{
  GListStore *store = G_LIST_STORE (object);

  g_sequence_free (store->items);

  G_OBJECT_CLASS (g_list_store_parent_class)->dispose (object);
}

static void
g_list_store_get_property (GObject    *object,
                           guint       property_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  GListStore *store = G_LIST_STORE (object);

  switch (property_id)
    {
    case PROP_ITEM_TYPE:
      g_value_set_gtype (value, store->item_type);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
g_list_store_set_property (GObject      *object,
                           guint         property_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
  GListStore *store = G_LIST_STORE (object);

  switch (property_id)
    {
    case PROP_ITEM_TYPE: /* construct-only */
      store->item_type = g_value_get_gtype (value);
      if (!g_type_is_a (store->item_type, G_TYPE_OBJECT))
        g_critical ("GListStore cannot store items of type '%s'. Items must be GObjects.",
                    g_type_name (store->item_type));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
g_list_store_class_init (GListStoreClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = g_list_store_dispose;
  object_class->get_property = g_list_store_get_property;
  object_class->set_property = g_list_store_set_property;

  /**
   * GListStore:item-type:
   *
   * The type of items contained in this list store.
   *
   * Since: 2.42
   **/
  g_object_class_install_property (object_class, PROP_ITEM_TYPE,
    g_param_spec_gtype ("item-type", "", "", G_TYPE_OBJECT,
                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static GType
g_list_store_get_item_type (GListModel *list)
{
  GListStore *store = G_LIST_STORE (list);

  return store->item_type;
}

static guint
g_list_store_get_n_items (GListModel *list)
{
  GListStore *store = G_LIST_STORE (list);

  return g_sequence_get_length (store->items);
}

static gpointer
g_list_store_get_item (GListModel *list,
                       guint       position)
{
  GListStore *store = G_LIST_STORE (list);
  GSequenceIter *it;

  it = g_sequence_get_iter_at_pos (store->items, position);

  if (g_sequence_iter_is_end (it))
    return NULL;
  else
    return g_object_ref (g_sequence_get (it));
}

static void
g_list_store_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = g_list_store_get_item_type;
  iface->get_n_items = g_list_store_get_n_items;
  iface->get_item = g_list_store_get_item;
}

static void
g_list_store_init (GListStore *store)
{
  store->items = g_sequence_new (g_object_unref);
}

/**
 * g_list_store_new:
 * @item_type: the #Gtype of the items that will be stored in the list
 *
 * Creates a new #GListStore with items of type @item_type.
 *
 * Returns: a new #GListStore
 * Since: 2.42
 */
GListStore *
g_list_store_new (GType item_type)
{
  /* We only allow GObjects as item types right now. This might change
   * in the future.
   */
  g_return_val_if_fail (g_type_is_a (item_type, G_TYPE_OBJECT), NULL);

  return g_object_new (G_TYPE_LIST_STORE,
                       "item-type", item_type,
                       NULL);
}

/**
 * g_list_store_insert:
 * @store: a #GListStore
 * @position: the position at which to insert the new item
 * @item: the new item
 *
 * Inserts @item into @store at @position. @item must be of type
 * #GListStore:item-type.
 *
 * Use g_list_store_splice() to insert multiple items at the same time
 * efficiently.
 *
 * Since: 2.42
 */
void
g_list_store_insert (GListStore *store,
                     guint       position,
                     gpointer    item)
{
  GSequenceIter *it;

  g_return_if_fail (G_IS_LIST_STORE (store));
  g_return_if_fail (g_type_is_a (G_OBJECT_TYPE (item), store->item_type));
  g_return_if_fail (position <= g_sequence_get_length (store->items));

  it = g_sequence_get_iter_at_pos (store->items, position);
  g_sequence_insert_before (it, g_object_ref (item));

  g_list_model_items_changed (G_LIST_MODEL (store), position, 0, 1);
}

/**
 * g_list_store_append:
 * @store: a #GListStore
 * @item: the new item
 *
 * Appends @item to @store. @item must be of type #GListStore:item-type.
 *
 * Use g_list_store_splice() to append multiple items at the same time
 * efficiently.
 *
 * Since: 2.42
 */
void
g_list_store_append (GListStore *store,
                     gpointer    item)
{
  guint n_items;

  g_return_if_fail (G_IS_LIST_STORE (store));
  g_return_if_fail (g_type_is_a (G_OBJECT_TYPE (item), store->item_type));

  n_items = g_sequence_get_length (store->items);
  g_sequence_append (store->items, g_object_ref (item));

  g_list_model_items_changed (G_LIST_MODEL (store), n_items, 0, 1);
}

/**
 * g_list_store_remove:
 * @store: a #GListStore
 * @position: the position of the item that is to be removed
 *
 * Removes the item from @store that is at @position.
 *
 * Use g_list_store_splice() to remove multiple items at the same time
 * efficiently.
 *
 * Since: 2.42
 */
void
g_list_store_remove (GListStore *store,
                     guint       position)
{
  GSequenceIter *it;

  g_return_if_fail (G_IS_LIST_STORE (store));

  it = g_sequence_get_iter_at_pos (store->items, position);
  g_return_if_fail (!g_sequence_iter_is_end (it));

  g_sequence_remove (it);
  g_list_model_items_changed (G_LIST_MODEL (store), position, 1, 0);
}

/**
 * g_list_store_remove_all:
 * @store: a #GListStore
 *
 * Removes all items from @store.
 *
 * Since: 2.42
 */
void
g_list_store_remove_all (GListStore *store)
{
  guint n_items;

  g_return_if_fail (G_IS_LIST_STORE (store));

  n_items = g_sequence_get_length (store->items);
  g_sequence_remove_range (g_sequence_get_begin_iter (store->items),
                           g_sequence_get_end_iter (store->items));

  g_list_model_items_changed (G_LIST_MODEL (store), 0, n_items, 0);
}

/**
 * g_list_store_splice:
 * @store: a #GListStore
 * @position: the position at which to make the change
 * @removed: the number of items to remove
 * @added: the number of items to add
 * @items: the items to add
 *
 * Changes @store by removing @removed items and adding @added items to
 * store. @items must contain @added items of type
 * #GListStore::item-type.
 *
 * This function is more efficient than g_list_store_insert() and
 * g_list_store_remove(), because it only emits
 * #GListModel::items-changed once for the change.
 *
 * Since: 2.42
 */
void
g_list_store_splice (GListStore *store,
                     guint       position,
                     guint       removed,
                     guint       added,
                     gpointer   *items)
{
  GSequenceIter *it;
  guint n_items;

  g_return_if_fail (G_IS_LIST_STORE (store));

  n_items = g_sequence_get_length (store->items);
  g_return_if_fail (position < n_items || (removed == 0 && position <= n_items));
  g_return_if_fail (removed <= n_items - position);

  it = g_sequence_get_iter_at_pos (store->items, position);

  if (removed)
    {
      GSequenceIter *end;

      end = g_sequence_iter_move (it, removed);
      g_sequence_remove_range (it, end);

      it = end;
    }

  if (added)
    {
      gint i;

      it = g_sequence_iter_next (it);
      for (i = 0; i < added; i++)
        {
          if (g_type_is_a (G_OBJECT_TYPE (items[i]), store->item_type))
            it = g_sequence_insert_before (it, g_object_ref (items[i]));
          else
            g_critical ("%s: item %d is a %s instead of a %s",
                        G_STRFUNC, i, G_OBJECT_TYPE_NAME (items[i]),
                        g_type_name (store->item_type));
        }
    }

  g_list_model_items_changed (G_LIST_MODEL (store), position, removed, added);
}
