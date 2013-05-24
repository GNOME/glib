#ifndef __G_KEYFILE_ENCODER_H__
#define __G_KEYFILE_ENCODER_H__

#if !defined (__GIO_GIO_H_INSIDE__) && !defined (GIO_COMPILATION)
#error "Only <gio/gio.h> can be included directly."
#endif

#include <gio/giotypes.h>

G_BEGIN_DECLS

#define G_TYPE_KEYFILE_ENCODER                  (g_keyfile_encoder_get_type ())
#define G_KEYFILE_ENCODER(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), G_TYPE_KEYFILE_ENCODER, GKeyfileEncoder))
#define G_IS_KEYFILE_ENCODER(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), G_TYPE_KEYFILE_ENCODER))
#define G_KEYFILE_ENCODER_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), G_TYPE_KEYFILE_ENCODER, GKeyfileEncoderClass))
#define G_IS_KEYFILE_ENCODER_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), G_TYPE_KEYFILE_ENCODER))
#define G_KEYFILE_ENCODER_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), G_TYPE_KEYFILE_ENCODER, GKeyfileEncoderClass))

typedef struct _GKeyfileEncoderClass            GKeyfileEncoderClass;

GLIB_AVAILABLE_IN_2_38
GType g_keyfile_encoder_get_type (void) G_GNUC_CONST;

GLIB_AVAILABLE_IN_2_38
GEncoder *      g_keyfile_encoder_new                   (void);

GLIB_AVAILABLE_IN_2_38
void            g_keyfile_encoder_set_section_name      (GKeyfileEncoder *encoder,
                                                         const char      *section_name);
GLIB_AVAILABLE_IN_2_38
const char *    g_keyfile_encoder_get_section_name      (GKeyfileEncoder *encoder);

G_END_DECLS

#endif /* __G_KEYFILE_ENCODER_H__ */
