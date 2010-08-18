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

#ifndef __G_ACTION_H__
#define __G_ACTION_H__

#include <gio/giotypes.h>

G_BEGIN_DECLS

#define G_TYPE_ACTION                                       (g_action_get_type ())
#define G_ACTION(inst)                                      (G_TYPE_CHECK_INSTANCE_CAST ((inst),                     \
                                                             G_TYPE_ACTION, GAction))
#define G_ACTION_CLASS(class)                               (G_TYPE_CHECK_CLASS_CAST ((class),                       \
                                                             G_TYPE_ACTION, GActionClass))
#define G_IS_ACTION(inst)                                   (G_TYPE_CHECK_INSTANCE_TYPE ((inst), G_TYPE_ACTION))
#define G_IS_ACTION_CLASS(class)                            (G_TYPE_CHECK_CLASS_TYPE ((class), G_TYPE_ACTION))
#define G_ACTION_GET_CLASS(inst)                            (G_TYPE_INSTANCE_GET_CLASS ((inst),                      \
                                                             G_TYPE_ACTION, GActionClass))

typedef struct _GActionPrivate                              GActionPrivate;
typedef struct _GActionClass                                GActionClass;

/**
 * GAction:
 *
 * The <structname>GAction</structname> structure contains private
 * data and should only be accessed using the provided API
 *
 * Since: 2.26
 */
struct _GAction
{
  /*< private >*/
  GObject parent_instance;

  GActionPrivate *priv;
};

/**
 * GActionClass:
 * @get_state_hint: the virtual function pointer for g_action_get_state_hint()
 * @set_state: the virtual function pointer for g_action_set_state().  The implementation should check the value
 *             for validity and then chain up to the handler in the base class in order to actually update the
 *             state.
 * @activate: the class closure for the activate signal
 *
 * Since: 2.26
 */
struct _GActionClass
{
  GObjectClass parent_class;

  /*< public >*/
  /* virtual functions */
  GVariant *           (* get_state_hint)       (GAction  *action);
  void                 (* set_state)            (GAction  *action,
                                                 GVariant *state);

  /*< private >*/
  gpointer vfunc_padding[6];

  /*< public >*/
  /* signals */
  void                 (* activate)             (GAction  *action,
                                                 GVariant *parameter);

  /*< private >*/
  gpointer signal_padding[6];
};

GType                   g_action_get_type                               (void) G_GNUC_CONST;

GAction *               g_action_new                                    (const gchar        *name,
                                                                         const GVariantType *parameter_type);

GAction *               g_action_new_stateful                           (const gchar        *name,
                                                                         const GVariantType *parameter_type,
                                                                         GVariant           *state);

const gchar *           g_action_get_name                               (GAction            *action);
const GVariantType *    g_action_get_parameter_type                     (GAction            *action);
const GVariantType *    g_action_get_state_type                         (GAction            *action);
GVariant *              g_action_get_state_hint                         (GAction            *action);

gboolean                g_action_get_enabled                            (GAction            *action);
void                    g_action_set_enabled                            (GAction            *action,
                                                                         gboolean            enabled);

GVariant *              g_action_get_state                              (GAction            *action);
void                    g_action_set_state                              (GAction            *action,
                                                                         GVariant           *value);

void                    g_action_activate                               (GAction            *action,
                                                                         GVariant           *parameter);
G_END_DECLS

#endif /* __G_ACTION_H__ */
