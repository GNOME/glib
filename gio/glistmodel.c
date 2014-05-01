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

#include "glistmodel.h"

G_DEFINE_INTERFACE (GListModel, g_list_model, G_TYPE_OBJECT);

/**
 * SECTION:glistmodel
 * @title: GListModel
 * @short_description: An interface describing a dynamic list of objects
 * @include: gio/gio.h
 *
 * #GListModel is a dynamic list of #GObjects.
 */

/**
 * GListModel:
 * @get_item_type: the virtual function pointer for g_list_model_get_item_type()
 * @get_n_items: the virtual function pointer for g_list_model_get_n_items()
 * @get_item: the virtual function pointer for g_list_model_get_item()
 *
 * The virtual function table for #GListModel.
 *
 * Since: 2.42
 */

static guint g_list_model_changed_signal;

static void
g_list_model_default_init (GListModelInterface *iface)
{
  /**
   * GListModel::items-changed
   * @list: the #GListModel that changed
   * @position: the position at which @list changed
   * @removed: the number of items removed
   * @added: the number of items added
   *
   * This signal is emitted whenever items were added or removed to
   * @list. At @position, @removed items were removed and @added items
   * were added in their place.
   *
   * Since: 2.42
   */
  g_list_model_changed_signal = g_signal_new ("items-changed",
                                              G_TYPE_LIST_MODEL,
                                              G_SIGNAL_RUN_LAST,
                                              0,
                                              NULL, NULL,
                                              g_cclosure_marshal_generic,
                                              G_TYPE_NONE,
                                              3, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_UINT);
}

/**
 * g_list_model_get_item_type:
 * @list: a @GListModel
 *
 * Gets the type of the items in @list. All items returned from
 * g_list_model_get_type() are of that type.
 *
 * Returns: the #GType of the items contained in @list.
 *
 * Since: 2.42
 */
GType
g_list_model_get_item_type (GListModel *list)
{
  g_return_val_if_fail (G_IS_LIST_MODEL (list), G_TYPE_NONE);

  return G_LIST_MODEL_GET_IFACE (list)->get_item_type (list);
}

/**
 * g_list_model_get_n_items:
 * @list: a @GListModel
 *
 * Gets the number of items in @list.
 *
 * Returns: the number of items in @list.
 *
 * Since: 2.42
 */
guint
g_list_model_get_n_items (GListModel *list)
{
  g_return_if_fail (G_IS_LIST_MODEL (list));

  return G_LIST_MODEL_GET_IFACE (list)->get_n_items (list);
}

/**
 * g_list_model_get_item:
 * @list: a @GListModel
 * @position: the position of the item to fetch
 *
 * Get the item at @position. If @position is greater than the number of
 * items in @list, %NULL is returned.
 *
 * %NULL is never returned for an index that is smaller than the length
 * of the list.
 *
 * Returns: (transfer full) (allow-none) (type GObject): the item at
 * @position.
 *
 * Since: 2.42
 */
gpointer
g_list_model_get_item (GListModel *list,
                       guint       position)
{
  g_return_val_if_fail (G_IS_LIST_MODEL (list), NULL);

  return G_LIST_MODEL_GET_IFACE (list)->get_item (list, position);
}

/**
 * g_list_model_items_changed:
 * @list: a @GListModel
 * @position: the position at which @list changed
 * @removed: the number of items removed
 * @added: the number of items added
 *
 * Emits the #GListModel::items-changed signal on @list.
 *
 * This function should only be called by classes implementing
 * #GListModel. It has to be called after the internal representation
 * of @list has been updated, because handlers connected to this signal
 * might query the new state of the list.
 *
 * Implementations may not emit this signal in response to a call to the
 * #GListModel API.
 *
 * Since: 2.42
 */
void
g_list_model_items_changed (GListModel *list,
                            guint       position,
                            guint       removed,
                            guint       added)
{
  g_return_if_fail (G_IS_LIST_MODEL (list));

  g_signal_emit (list, g_list_model_changed_signal, 0, position, removed, added);
}
