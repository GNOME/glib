/* gmultibinding.h: Binding for object properties
 *
 * Copyright (C) 2015 Red Hat, Inc
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
 * Author: Matthias Clasen
 */

#ifndef __G_MULTI_BINDING_H__
#define __G_MULTI_BINDING_H__

#if !defined (__GLIB_GOBJECT_H_INSIDE__) && !defined (GOBJECT_COMPILATION)
#error "Only <glib-object.h> can be included directly."
#endif

#include <glib.h>
#include <gobject/gobject.h>

G_BEGIN_DECLS

#define G_TYPE_MULTI_BINDING          (g_multi_binding_get_type ())
#define G_MULTI_BINDING(obj)          (G_TYPE_CHECK_INSTANCE_CAST ((obj), G_TYPE_MULTI_BINDING, GMultiBinding))
#define G_IS_MULTI_BINDING(obj)       (G_TYPE_CHECK_INSTANCE_TYPE ((obj), G_TYPE_MULTI_BINDING))

typedef enum { /*< prefix=G_MULTI_BINDING >*/
  G_MULTI_BINDING_DEFAULT        = 0,
  G_MULTI_BINDING_SYNC_CREATE    = 1 << 1
} GMultiBindingFlags;

/**
 * GMultiBinding:
 *
 * GMultiBinding is an opaque structure whose members
 * cannot be accessed directly.
 *
 * Since: 2.44
 */
typedef struct _GMultiBinding   GMultiBinding;

typedef gboolean (* GMultiBindingTransformFunc) (GMultiBinding  *binding,
                                                 gint            notified,
                                                 const GValue    from_values[],
                                                 GValue          to_values[],
                                                 gpointer        user_data);

GLIB_AVAILABLE_IN_ALL
GType                 g_multi_binding_get_type            (void) G_GNUC_CONST;

GLIB_AVAILABLE_IN_ALL
gint                  g_multi_binding_get_n_sources       (GMultiBinding *binding);
GLIB_AVAILABLE_IN_ALL
GObject *             g_multi_binding_get_source          (GMultiBinding *binding,
                                                           gint           idx);
GLIB_AVAILABLE_IN_ALL
const gchar *         g_multi_binding_get_source_property (GMultiBinding *binding,
                                                           gint           idx);

GLIB_AVAILABLE_IN_ALL
gint                  g_multi_binding_get_n_targets       (GMultiBinding *binding);
GLIB_AVAILABLE_IN_ALL
GObject *             g_multi_binding_get_target          (GMultiBinding *binding,
                                                           gint           idx);
GLIB_AVAILABLE_IN_ALL
const gchar *         g_multi_binding_get_target_property (GMultiBinding *binding,
                                                           gint           idx);

GLIB_AVAILABLE_IN_ALL
void                  g_multi_binding_unbind              (GMultiBinding *binding);

GLIB_AVAILABLE_IN_ALL
GMultiBinding        *g_object_bind_properties_v          (gint                       n_sources,
                                                           GObject                   *sources[],
                                                           const gchar               *source_properties[],
                                                           gint                       n_targets,
                                                           GObject                   *targets[],
                                                           const gchar               *target_properties[],
                                                           GMultiBindingFlags         flags,
                                                           GMultiBindingTransformFunc transform,
                                                           gpointer                   user_data,
                                                           GDestroyNotify             notify);
GLIB_AVAILABLE_IN_ALL
GMultiBinding        *g_object_bind_properties            (GObject                   *source,
                                                           const gchar               *property,
                                                           ...
                                                           GObject                   *target,
                                                           const gchar               *property,
                                                           ...
                                                           GMultiBindingFlags         flags,
                                                           GMultiBindingTransformFunc transform,
                                                           gpointer                   user_data,
                                                           GDestroyNotify             notify);

#endif /* __G_MULTI_BINDING_H__ */
