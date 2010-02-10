/*
 * Copyright © 2007, 2008 Ryan Lortie
 * Copyright © 2009, 2010 Codethink Limited
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the licence, or (at your option) any later version.
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
 *
 * Author: Ryan Lortie <desrt@desrt.ca>
 */

#ifndef __G_VARIANT_H__
#define __G_VARIANT_H__

#include <glib/gvarianttype.h>

G_BEGIN_DECLS

typedef struct _GVariant        GVariant;

void                            g_variant_unref                         (GVariant             *value);
GVariant *                      g_variant_ref                           (GVariant             *value);
GVariant *                      g_variant_ref_sink                      (GVariant             *value);

gsize                           g_variant_n_children                    (GVariant             *value);
GVariant *                      g_variant_get_child_value               (GVariant             *value,
                                                                         gsize                 index);

gsize                           g_variant_get_size                      (GVariant             *value);
gconstpointer                   g_variant_get_data                      (GVariant             *value);
void                            g_variant_store                         (GVariant             *value,
                                                                         gpointer              data);

G_END_DECLS

#endif /* __G_VARIANT_H__ */
