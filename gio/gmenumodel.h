/*
 * Copyright © 2011 Canonical Ltd.
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * licence, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
 * USA.
 *
 * Author: Ryan Lortie <desrt@desrt.ca>
 */

#ifndef __G_MENU_MODEL_H__
#define __G_MENU_MODEL_H__

#include <glib-object.h>

#include <gio/giotypes.h>

G_BEGIN_DECLS

#define G_MENU_ATTRIBUTE_ACTION "action"
#define G_MENU_ATTRIBUTE_TARGET "target"
#define G_MENU_ATTRIBUTE_LABEL "label"

#define G_MENU_LINK_SUBMENU "submenu"
#define G_MENU_LINK_SECTION "section"

#define G_TYPE_MENU_MODEL                                   (g_menu_model_get_type ())
#define G_MENU_MODEL(inst)                                  (G_TYPE_CHECK_INSTANCE_CAST ((inst),                     \
                                                             G_TYPE_MENU_MODEL, GMenuModel))
#define G_MENU_MODEL_CLASS(class)                           (G_TYPE_CHECK_CLASS_CAST ((class),                       \
                                                             G_TYPE_MENU_MODEL, GMenuModelClass))
#define G_IS_MENU_MODEL(inst)                               (G_TYPE_CHECK_INSTANCE_TYPE ((inst),                     \
                                                             G_TYPE_MENU_MODEL))
#define G_IS_MENU_MODEL_CLASS(class)                        (G_TYPE_CHECK_CLASS_TYPE ((class),                       \
                                                             G_TYPE_MENU_MODEL))
#define G_MENU_MODEL_GET_CLASS(inst)                        (G_TYPE_INSTANCE_GET_CLASS ((inst),                      \
                                                             G_TYPE_MENU_MODEL, GMenuModelClass))

typedef struct _GMenuModelPrivate                           GMenuModelPrivate;
typedef struct _GMenuModelClass                             GMenuModelClass;

typedef struct _GMenuAttributeIterPrivate                   GMenuAttributeIterPrivate;
typedef struct _GMenuAttributeIterClass                     GMenuAttributeIterClass;
typedef struct _GMenuAttributeIter                          GMenuAttributeIter;

typedef struct _GMenuLinkIterPrivate                        GMenuLinkIterPrivate;
typedef struct _GMenuLinkIterClass                          GMenuLinkIterClass;
typedef struct _GMenuLinkIter                               GMenuLinkIter;

struct _GMenuModel
{
  GObject            parent_instance;
  GMenuModelPrivate *priv;
};

struct _GMenuModelClass
{
  GObjectClass parent_class;

  gboolean              (*is_mutable)                       (GMenuModel          *model);
  gint                  (*get_n_items)                      (GMenuModel          *model);
  void                  (*get_item_attributes)              (GMenuModel          *model,
                                                             gint                 item_index,
                                                             GHashTable         **attributes);
  GMenuAttributeIter *  (*iterate_item_attributes)          (GMenuModel          *model,
                                                             gint                 item_index);
  GVariant *            (*get_item_attribute_value)         (GMenuModel          *model,
                                                             gint                 item_index,
                                                             const gchar         *attribute,
                                                             const GVariantType  *expected_type);
  void                  (*get_item_links)                   (GMenuModel          *model,
                                                             gint                 item_index,
                                                             GHashTable         **links);
  GMenuLinkIter *       (*iterate_item_links)               (GMenuModel          *model,
                                                             gint                 item_index);
  GMenuModel *          (*get_item_link)                    (GMenuModel          *model,
                                                             gint                 item_index,
                                                             const gchar         *link);
};

GType                   g_menu_model_get_type                           (void) G_GNUC_CONST;

gboolean                g_menu_model_is_mutable                         (GMenuModel         *model);
gint                    g_menu_model_get_n_items                        (GMenuModel         *model);

GMenuAttributeIter *    g_menu_model_iterate_item_attributes            (GMenuModel         *model,
                                                                         gint                item_index);
GVariant *              g_menu_model_get_item_attribute_value           (GMenuModel         *model,
                                                                         gint                item_index,
                                                                         const gchar        *attribute,
                                                                         const GVariantType *expected_type);
gboolean                g_menu_model_get_item_attribute                 (GMenuModel         *model,
                                                                         gint                item_index,
                                                                         const gchar        *attribute,
                                                                         const gchar        *format_string,
                                                                         ...);
GMenuLinkIter *         g_menu_model_iterate_item_links                 (GMenuModel         *model,
                                                                         gint                item_index);
GMenuModel *            g_menu_model_get_item_link                      (GMenuModel         *model,
                                                                         gint                item_index,
                                                                         const gchar        *link);

void                    g_menu_model_items_changed                      (GMenuModel         *model,
                                                                         gint                position,
                                                                         gint                removed,
                                                                         gint                added);


#define G_TYPE_MENU_ATTRIBUTE_ITER                          (g_menu_attribute_iter_get_type ())
#define G_MENU_ATTRIBUTE_ITER(inst)                         (G_TYPE_CHECK_INSTANCE_CAST ((inst),                     \
                                                             G_TYPE_MENU_ATTRIBUTE_ITER, GMenuAttributeIter))
#define G_MENU_ATTRIBUTE_ITER_CLASS(class)                  (G_TYPE_CHECK_CLASS_CAST ((class),                       \
                                                             G_TYPE_MENU_ATTRIBUTE_ITER, GMenuAttributeIterClass))
#define G_IS_MENU_ATTRIBUTE_ITER(inst)                      (G_TYPE_CHECK_INSTANCE_TYPE ((inst),                     \
                                                             G_TYPE_MENU_ATTRIBUTE_ITER))
#define G_IS_MENU_ATTRIBUTE_ITER_CLASS(class)               (G_TYPE_CHECK_CLASS_TYPE ((class),                       \
                                                             G_TYPE_MENU_ATTRIBUTE_ITER))
#define G_MENU_ATTRIBUTE_ITER_GET_CLASS(inst)               (G_TYPE_INSTANCE_GET_CLASS ((inst),                      \
                                                             G_TYPE_MENU_ATTRIBUTE_ITER, GMenuAttributeIterClass))

struct _GMenuAttributeIter
{
  GObject parent_instance;
  GMenuAttributeIterPrivate *priv;
};

struct _GMenuAttributeIterClass
{
  GObjectClass parent_class;

  gboolean      (*get_next) (GMenuAttributeIter  *iter,
                             const gchar        **out_type,
                             GVariant           **value);
};

GType                   g_menu_attribute_iter_get_type                  (void) G_GNUC_CONST;

gboolean                g_menu_attribute_iter_get_next                  (GMenuAttributeIter  *iter,
                                                                         const gchar        **out_name,
                                                                         GVariant           **value);
gboolean                g_menu_attribute_iter_next                      (GMenuAttributeIter  *iter);
const gchar *           g_menu_attribute_iter_get_name                  (GMenuAttributeIter  *iter);
GVariant *              g_menu_attribute_iter_get_value                 (GMenuAttributeIter  *iter);


#define G_TYPE_MENU_LINK_ITER                               (g_menu_link_iter_get_type ())
#define G_MENU_LINK_ITER(inst)                              (G_TYPE_CHECK_INSTANCE_CAST ((inst),                     \
                                                             G_TYPE_MENU_LINK_ITER, GMenuLinkIter))
#define G_MENU_LINK_ITER_CLASS(class)                       (G_TYPE_CHECK_CLASS_CAST ((class),                       \
                                                             G_TYPE_MENU_LINK_ITER, GMenuLinkIterClass))
#define G_IS_MENU_LINK_ITER(inst)                           (G_TYPE_CHECK_INSTANCE_TYPE ((inst),                     \
                                                             G_TYPE_MENU_LINK_ITER))
#define G_IS_MENU_LINK_ITER_CLASS(class)                    (G_TYPE_CHECK_CLASS_TYPE ((class),                       \
                                                             G_TYPE_MENU_LINK_ITER))
#define G_MENU_LINK_ITER_GET_CLASS(inst)                    (G_TYPE_INSTANCE_GET_CLASS ((inst),                      \
                                                             G_TYPE_MENU_LINK_ITER, GMenuLinkIterClass))

struct _GMenuLinkIter
{
  GObject parent_instance;
  GMenuLinkIterPrivate *priv;
};

struct _GMenuLinkIterClass
{
  GObjectClass parent_class;

  gboolean      (*get_next) (GMenuLinkIter  *iter,
                             const gchar   **out_name,
                             GMenuModel    **value);
};

GType                   g_menu_link_iter_get_type                       (void) G_GNUC_CONST;

gboolean                g_menu_link_iter_get_next                       (GMenuLinkIter  *iter,
                                                                         const gchar   **out_link,
                                                                         GMenuModel    **value);
gboolean                g_menu_link_iter_next                           (GMenuLinkIter  *iter);
const gchar *           g_menu_link_iter_get_name                       (GMenuLinkIter  *iter);
GMenuModel *            g_menu_link_iter_get_value                      (GMenuLinkIter  *iter);

G_END_DECLS

#endif /* __G_MENU_MODEL_H__ */
