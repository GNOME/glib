/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright (C) 2009 Red Hat, Inc.
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
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Alexander Larsson <alexl@redhat.com>
 *         Christian Persch <chpe@gnome.org>
 */

#pragma once

#if !defined (__GIO_GIO_H_INSIDE__) && !defined (GIO_COMPILATION)
#error "Only <gio/gio.h> can be included directly."
#endif

#include <gio/gconverter.h>

G_BEGIN_DECLS

#define G_TYPE_BASE64_DECODER         (g_base64_decoder_get_type ())

GIO_AVAILABLE_IN_2_82
G_DECLARE_FINAL_TYPE (GBase64Decoder, g_base64_decoder, G, BASE64_DECODER, GObject)

GIO_AVAILABLE_IN_2_82
GConverter *g_base64_decoder_new      (void);

G_END_DECLS
