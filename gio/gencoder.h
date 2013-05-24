/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright (C) 2013  Emmanuele Bassi <ebassi@gnome.org>
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
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __G_ENCODER_H__
#define __G_ENCODER_H__

#if !defined (__GIO_GIO_H_INSIDE__) && !defined (GIO_COMPILATION)
#error "Only <gio/gio.h> can be included directly."
#endif

#include <gio/giotypes.h>

G_BEGIN_DECLS

#define G_TYPE_ENCODER                  (g_encoder_get_type ())
#define G_ENCODER(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), G_TYPE_ENCODER, GEncoder))
#define G_IS_ENCODER(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), G_TYPE_ENCODER))
#define G_ENCODER_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), G_TYPE_ENCODER, GEncoderClass))
#define G_IS_ENCODER_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), G_TYPE_ENCODER))
#define G_ENCODER_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), G_TYPE_ENCODER, GEncoderClass))

typedef struct _GEncoderClass           GEncoderClass;

/**
 * GEncoder:
 *
 * Class for providing a way to encode and decode state into a buffer.
 *
 * Since: 2.38
 */
struct _GEncoder
{
  GObject parent_instance;
};

struct _GEncoderClass
{
  /*< private >*/
  GObjectClass parent_class;

  /*< public >*/
  gboolean (* read_from_bytes) (GEncoder    *encoder,
                                GBytes      *bytes,
                                GError     **error);
  GBytes * (* write_to_bytes)  (GEncoder    *encoder,
                                GError     **error);

  void     (* closed)          (GEncoder    *encoder,
                                GVariant    *variant);

  void     (* value_encoded)   (GEncoder    *encoder,
                                const char  *key,
                                GVariant    *variant);

  /*< private >*/
  gpointer _padding[8];
};

GLIB_AVAILABLE_IN_2_38
GType g_encoder_get_type (void) G_GNUC_CONST;

/* Encoding values */

GLIB_AVAILABLE_IN_2_38
gboolean        g_encoder_add_key               (GEncoder     *encoder,
                                                 const char   *key,
                                                 GVariant     *value);
GLIB_AVAILABLE_IN_2_38
gboolean        g_encoder_add_key_data          (GEncoder     *encoder,
                                                 const char   *key,
                                                 const guint8 *value,
                                                 gsize         value_len);
GLIB_AVAILABLE_IN_2_38
gboolean        g_encoder_add_key_string        (GEncoder     *encoder,
                                                 const char   *key,
                                                 const char   *value);
GLIB_AVAILABLE_IN_2_38
gboolean        g_encoder_add_key_int64         (GEncoder     *encoder,
                                                 const char   *key,
                                                 gint64        value);
GLIB_AVAILABLE_IN_2_38
gboolean        g_encoder_add_key_int32         (GEncoder     *encoder,
                                                 const char   *key,
                                                 gint32        value);
GLIB_AVAILABLE_IN_2_38
gboolean        g_encoder_add_key_double        (GEncoder     *encoder,
                                                 const char   *key,
                                                 double        value);
GLIB_AVAILABLE_IN_2_38
gboolean        g_encoder_add_key_bool          (GEncoder     *encoder,
                                                 const char   *key,
                                                 gboolean      value);

/* Decoding values */

GLIB_AVAILABLE_IN_2_38
gboolean        g_encoder_get_key_data          (GEncoder     *encoder,
                                                 const char   *key,
                                                 guint8      **res,
                                                 gsize        *res_len);
GLIB_AVAILABLE_IN_2_38
gboolean        g_encoder_get_key_string        (GEncoder     *encoder,
                                                 const char   *key,
                                                 char        **res);
GLIB_AVAILABLE_IN_2_38
gboolean        g_encoder_get_key_int64         (GEncoder     *encoder,
                                                 const char   *key,
                                                 gint64       *res);
GLIB_AVAILABLE_IN_2_38
gboolean        g_encoder_get_key_int32         (GEncoder     *encoder,
                                                 const char   *key,
                                                 gint32       *res);
GLIB_AVAILABLE_IN_2_38
gboolean        g_encoder_get_key_double        (GEncoder     *encoder,
                                                 const char   *key,
                                                 double       *res);
GLIB_AVAILABLE_IN_2_38
gboolean        g_encoder_get_key_bool          (GEncoder     *encoder,
                                                 const char   *key,
                                                 gboolean     *res);

GLIB_AVAILABLE_IN_2_38
gboolean        g_encoder_has_key               (GEncoder     *encoder,
                                                 const char   *key);
GLIB_AVAILABLE_IN_2_38
GVariant *      g_encoder_close                 (GEncoder     *encoder);

GLIB_AVAILABLE_IN_2_38
gboolean        g_encoder_read_from_bytes       (GEncoder     *encoder,
                                                 GBytes       *bytes,
                                                 GError      **error);
GLIB_AVAILABLE_IN_2_38
GBytes *        g_encoder_write_to_bytes        (GEncoder     *encoder,
                                                 GError      **error);

G_END_DECLS

#endif /* __G_ENCODER_H__ */
