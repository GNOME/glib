/*
 * Copyright 2026 Christian Hergert
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "glistmodelview.h"

/**
 * GListModelView:
 *
 * `GListModelView` is a read-only [iface@Gio.ListModel] wrapper around another
 * `GListModel`.
 *
 * It forwards item lookups to the wrapped model and mirrors its
 * [property@Gio.ListModelView:n-items] property and
 * [signal@Gio.ListModel::items-changed] signal. This is useful for exposing a
 * model to API consumers without also exposing the concrete mutable type.
 *
 * The [property@Gio.ListModelView:item-type] property is fixed at construction
 * time and future models set with [method@Gio.ListModelView.set_model] must have
 * the same item type.
 *
 * Since: 2.88
 */

struct _GListModelView
{
  GObject parent_instance;

  GListModel *model;
  GType item_type;
  guint n_items;
  gulong items_changed_handler;
};

enum
{
  PROP_0,
  PROP_ITEM_TYPE,
  PROP_MODEL,
  PROP_N_ITEMS,
  N_PROPS
};

static void g_list_model_view_iface_init (GListModelInterface *iface);

G_DEFINE_TYPE_WITH_CODE (GListModelView, g_list_model_view, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_LIST_MODEL, g_list_model_view_iface_init));

static GParamSpec *properties[N_PROPS];

static void
g_list_model_view_clear_model (GListModelView *self)
{
  if (self->model == NULL)
    return;

  if (self->items_changed_handler != 0)
    {
      g_signal_handler_disconnect (self->model, self->items_changed_handler);
      self->items_changed_handler = 0;
    }

  g_clear_object (&self->model);
}

static void
g_list_model_view_items_changed_cb (GListModel     *model,
                                    guint           position,
                                    guint           removed,
                                    guint           added,
                                    GListModelView *self)
{
  self->n_items += added;
  self->n_items -= removed;

  g_list_model_items_changed (G_LIST_MODEL (self), position, removed, added);

  if (removed != added)
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_N_ITEMS]);
}

static void
g_list_model_view_dispose (GObject *object)
{
  GListModelView *self = G_LIST_MODEL_VIEW (object);

  g_list_model_view_clear_model (self);

  G_OBJECT_CLASS (g_list_model_view_parent_class)->dispose (object);
}

static void
g_list_model_view_get_property (GObject    *object,
                                guint       property_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  GListModelView *self = G_LIST_MODEL_VIEW (object);

  switch (property_id)
    {
    case PROP_ITEM_TYPE:
      g_value_set_gtype (value, self->item_type);
      break;

    case PROP_MODEL:
      g_value_set_object (value, self->model);
      break;

    case PROP_N_ITEMS:
      g_value_set_uint (value, self->n_items);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
g_list_model_view_set_property (GObject      *object,
                                guint         property_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GListModelView *self = G_LIST_MODEL_VIEW (object);

  switch (property_id)
    {
    case PROP_ITEM_TYPE:
      g_assert (g_type_is_a (g_value_get_gtype (value), G_TYPE_OBJECT));
      self->item_type = g_value_get_gtype (value);
      break;

    case PROP_MODEL:
      g_list_model_view_set_model (self, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
g_list_model_view_class_init (GListModelViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = g_list_model_view_dispose;
  object_class->get_property = g_list_model_view_get_property;
  object_class->set_property = g_list_model_view_set_property;

  /**
   * GListModelView:item-type:
   *
   * The type of items contained in this list model view.
   *
   * Since: 2.88
   */
  properties[PROP_ITEM_TYPE] =
    g_param_spec_gtype ("item-type", NULL, NULL, G_TYPE_OBJECT,
                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  /**
   * GListModelView:model:
   *
   * The wrapped list model.
   *
   * Since: 2.88
   */
  properties[PROP_MODEL] =
    g_param_spec_object ("model", NULL, NULL, G_TYPE_LIST_MODEL,
                         G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  /**
   * GListModelView:n-items:
   *
   * The number of items contained in this list model view.
   *
   * Since: 2.88
   */
  properties[PROP_N_ITEMS] =
    g_param_spec_uint ("n-items", NULL, NULL, 0, G_MAXUINT, 0,
                       G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
g_list_model_view_init (GListModelView *self)
{
  self->item_type = G_TYPE_OBJECT;
}

static GType
g_list_model_view_get_item_type (GListModel *list)
{
  GListModelView *self = G_LIST_MODEL_VIEW (list);

  return self->item_type;
}

static guint
g_list_model_view_get_n_items (GListModel *list)
{
  GListModelView *self = G_LIST_MODEL_VIEW (list);

  return self->n_items;
}

static gpointer
g_list_model_view_get_item (GListModel *list,
                            guint       position)
{
  GListModelView *self = G_LIST_MODEL_VIEW (list);

  if (self->model == NULL)
    return NULL;

  return g_list_model_get_item (self->model, position);
}

static void
g_list_model_view_iface_init (GListModelInterface *iface)
{
  iface->get_item_type = g_list_model_view_get_item_type;
  iface->get_n_items = g_list_model_view_get_n_items;
  iface->get_item = g_list_model_view_get_item;
}

/**
 * g_list_model_view_new:
 * @model: (transfer full) (nullable): a list model
 *
 * Creates a new `GListModelView` wrapping @model.
 *
 * If @model is `NULL`, then the item-type is assumed to be
 * [type@GObject.Object] and you may use [method@Gio.ListModeView.set_model]
 * to set a model after the instance is created.
 *
 * Returns: (transfer full): a new `GListModelView`
 * Since: 2.88
 */
GListModelView *
g_list_model_view_new (GListModel *model)
{
  GListModelView *self;
  GType item_type;

  g_return_val_if_fail (!model || G_IS_LIST_MODEL (model), NULL);

  if (model != NULL)
    item_type = g_list_model_get_item_type (model);
  else
    item_type = G_TYPE_OBJECT;

  self = g_object_new (G_TYPE_LIST_MODEL_VIEW,
                       "item-type", item_type,
                       "model", model,
                       NULL);

  g_clear_object (&model);

  return self;
}

/**
 * g_list_model_view_get_model:
 * @self: a `GListModelView`
 *
 * Gets the wrapped model of @self.
 *
 * Returns: (transfer none) (nullable): the wrapped model
 * Since: 2.88
 */
GListModel *
g_list_model_view_get_model (GListModelView *self)
{
  g_return_val_if_fail (G_IS_LIST_MODEL_VIEW (self), NULL);

  return self->model;
}

/**
 * g_list_model_view_set_model:
 * @self: a `GListModelView`
 * @model: (nullable): the new model
 *
 * Sets the wrapped model of @self.
 *
 * If @model is not %NULL, it must have the same item type as the current
 * model view.
 *
 * Since: 2.88
 */
void
g_list_model_view_set_model (GListModelView *self,
                             GListModel     *model)
{
  guint old_n_items;

  g_return_if_fail (G_IS_LIST_MODEL_VIEW (self));
  g_return_if_fail (model == NULL || G_IS_LIST_MODEL (model));
  g_return_if_fail (model == NULL ||
                    self->item_type == G_TYPE_OBJECT ||
                    g_list_model_get_item_type (model) == self->item_type);

  if (model == self->model)
    return;

  old_n_items = self->n_items;

  g_object_freeze_notify (G_OBJECT (self));

  g_list_model_view_clear_model (self);

  if (model != NULL)
    {
      if (self->item_type == G_TYPE_OBJECT)
        self->item_type = g_list_model_get_item_type (model);

      self->model = g_object_ref (model);
      self->items_changed_handler =
        g_signal_connect (self->model,
                          "items-changed",
                          G_CALLBACK (g_list_model_view_items_changed_cb),
                          self);
      self->n_items = g_list_model_get_n_items (self->model);
    }
  else
    {
      self->n_items = 0;
    }

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MODEL]);

  if (old_n_items != 0 || self->n_items != 0)
    g_list_model_items_changed (G_LIST_MODEL (self), 0, old_n_items, self->n_items);

  if (old_n_items != self->n_items)
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_N_ITEMS]);

  g_object_thaw_notify (G_OBJECT (self));
}
