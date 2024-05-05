/* GIO - GLib Input, Output and Streaming Library
 *
 * SPDX-FileCopyrightText: 2009 Red Hat, Inc.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#if !defined (__GIO_GIO_H_INSIDE__) && !defined (GIO_COMPILATION)
#error "Only <gio/gio.h> can be included directly."
#endif

#include <gio/gconverter.h>

G_BEGIN_DECLS

#define G_TYPE_BASE64_DECODER (g_base64_decoder_get_type ())

GIO_AVAILABLE_IN_2_82
G_DECLARE_FINAL_TYPE (GBase64Decoder, g_base64_decoder, G, BASE64_DECODER, GObject)

GIO_AVAILABLE_IN_2_82
GConverter *g_base64_decoder_new (void);

G_END_DECLS
