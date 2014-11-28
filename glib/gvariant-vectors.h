
#ifndef __G_VARIANT_VECTORS_H__
#define __G_VARIANT_VECTORS_H__

#include <glib/garray.h>

typedef struct
{
  GByteArray *extra_bytes;
  GArray     *vectors;
  GByteArray *offsets;
} GVariantVectors;


/* If ->bytes is NULL then offset/size point inside of extra_bytes,
 * otherwise pointer/size point to memory owned by the GBytes.
 */
typedef struct
{
  GBytes *gbytes;
  union {
    gconstpointer pointer;
    gsize         offset;
  } data;
  gsize    size;
} GVariantVector;

void                    g_variant_vectors_init                          (GVariantVectors *vectors);


gsize                   g_variant_vectors_append_pad                    (GVariantVectors *vectors,
                                                                         gsize            padding);


void                    g_variant_vectors_append_copy                   (GVariantVectors *vectors,
                                                                         gconstpointer    data,
                                                                         gsize            size);


void                    g_variant_vectors_append_gbytes                 (GVariantVectors *vectors,
                                                                         GBytes          *gbytes,
                                                                         gconstpointer    data,
                                                                         gsize            size);


gsize                   g_variant_vectors_reserve_offsets               (GVariantVectors *vectors,
                                                                         guint            n_offsets,
                                                                         guint            offset_size);


void                    g_variant_vectors_write_to_offsets              (GVariantVectors *vectors,
                                                                         gsize            offset,
                                                                         gsize            value,
                                                                         gsize            offset_key);


void                    g_variant_vectors_commit_offsets                (GVariantVectors *vectors,
                                                                         gsize            offset_key);

#endif /* __G_GVARIANT_VECTORS_H__ */
