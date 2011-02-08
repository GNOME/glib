/*
 * Copyright © 2010 Codethink Limited
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
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: Ryan Lortie <desrt@desrt.ca>
 */

#if !defined (__GIO_GIO_H_INSIDE__) && !defined (GIO_COMPILATION)
#error "Only <gio/gio.h> can be included directly."
#endif

#ifndef __G_SIMPLE_ACTION_H__
#define __G_SIMPLE_ACTION_H__

#include <gio/giotypes.h>

G_BEGIN_DECLS

#define G_TYPE_SIMPLE_ACTION                                (g_simple_action_get_type ())
#define G_SIMPLE_ACTION(inst)                               (G_TYPE_CHECK_INSTANCE_CAST ((inst),                     \
                                                             G_TYPE_SIMPLE_ACTION, GSimpleAction))
#define G_SIMPLE_ACTION_CLASS(class)                        (G_TYPE_CHECK_CLASS_CAST ((class),                       \
                                                             G_TYPE_SIMPLE_ACTION, GSimpleActionClass))
#define G_IS_SIMPLE_ACTION(inst)                            (G_TYPE_CHECK_INSTANCE_TYPE ((inst),                     \
                                                             G_TYPE_SIMPLE_ACTION))
#define G_IS_SIMPLE_ACTION_CLASS(class)                     (G_TYPE_CHECK_CLASS_TYPE ((class),                       \
                                                             G_TYPE_SIMPLE_ACTION))
#define G_SIMPLE_ACTION_GET_CLASS(inst)                     (G_TYPE_INSTANCE_GET_CLASS ((inst),                      \
                                                             G_TYPE_SIMPLE_ACTION, GSimpleActionClass))

typedef struct _GSimpleActionPrivate                        GSimpleActionPrivate;
typedef struct _GSimpleActionClass                          GSimpleActionClass;

/**
 * GSimpleAction:
 *
 * The <structname>GSimpleAction</structname> structure contains private
 * data and should only be accessed using the provided API
 *
 * Since: 2.28
 */
struct _GSimpleAction
{
  /*< private >*/
  GObject parent_instance;

  GSimpleActionPrivate *priv;
};

/**
 * GSimpleActionClass:
 * @activate: the class closure for the activate signal
 *
 * Since: 2.28
 */
struct _GSimpleActionClass
{
  GObjectClass parent_class;

  /* signals */
  void  (* activate)  (GSimpleAction *simple,
                       GVariant      *parameter);
  /*< private >*/
  gpointer padding[6];
};

GType                   g_simple_action_get_type                        (void) G_GNUC_CONST;

GSimpleAction *         g_simple_action_new                             (const gchar        *name,
                                                                         const GVariantType *parameter_type);

GSimpleAction *         g_simple_action_new_stateful                    (const gchar        *name,
                                                                         const GVariantType *parameter_type,
                                                                         GVariant           *state);

void                    g_simple_action_set_enabled                     (GSimpleAction      *simple,
                                                                         gboolean            enabled);

G_END_DECLS

#endif /* __G_SIMPLE_ACTION_H__ */
