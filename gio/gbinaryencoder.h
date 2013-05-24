#ifndef __G_BINARY_ENCODER_H__
#define __G_BINARY_ENCODER_H__

#if !defined (__GIO_GIO_H_INSIDE__) && !defined (GIO_COMPILATION)
#error "Only <gio/gio.h> can be included directly."
#endif

#include <gio/giotypes.h>

G_BEGIN_DECLS

#define G_TYPE_BINARY_ENCODER                   (g_binary_encoder_get_type ())
#define G_BINARY_ENCODER(obj)                   (G_TYPE_CHECK_INSTANCE_CAST ((obj), G_TYPE_BINARY_ENCODER, GBinaryEncoder))
#define G_IS_BINARY_ENCODER(obj)                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), G_TYPE_BINARY_ENCODER))
#define G_BINARY_ENCODER_CLASS(klass)           (G_TYPE_CHECK_CLASS_CAST ((klass), G_TYPE_BINARY_ENCODER, GBinaryEncoderClass))
#define G_IS_BINARY_ENCODER_CLASS(klass)        (G_TYPE_CHECK_CLASS_TYPE ((klass), G_TYPE_BINARY_ENCODER))
#define G_BINARY_ENCODER_GET_CLASS(obj)         (G_TYPE_INSTANCE_GET_CLASS ((obj), G_TYPE_BINARY_ENCODER, GBinaryEncoderClass))

typedef struct _GBinaryEncoderClass             GBinaryEncoderClass;

GLIB_AVAILABLE_IN_2_38
GType g_binary_encoder_get_type (void) G_GNUC_CONST;

GLIB_AVAILABLE_IN_2_38
GEncoder *      g_binary_encoder_new            (void);

G_END_DECLS

#endif /* __G_BINARY_ENCODER_H__ */
