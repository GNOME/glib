#include "config.h"

#include "gvariant-vectors.h"
#include "gtestutils.h"

static void
append_zeros (GByteArray *array,
              guint       n)
{
  guchar zeros[8] = "";

  g_byte_array_append (array, zeros, n);
}

void
g_variant_vectors_init (GVariantVectors *vectors)
{

  /* The first 8 bytes of 'extra_bytes' is always 0.  We use this for
   * inserting padding in between two GBytes records.
   */
  vectors->extra_bytes = g_byte_array_new ();
  append_zeros (vectors->extra_bytes, 8);

  vectors->vectors = g_array_new (FALSE, FALSE, sizeof (GVariantVector));

  vectors->offsets = g_byte_array_new ();
}

gsize
g_variant_vectors_append_pad (GVariantVectors *vectors,
                              gsize            padding)
{
  /* If the last vector that we stored was 'pad' or 'copy' then we will
   * expand it instead of adding a new one.
   */
  if (vectors->vectors->len)
    {
      GVariantVector *expand_vector = &g_array_index (vectors->vectors, GVariantVector, vectors->vectors->len - 1);

      if (expand_vector->gbytes == NULL)
        {
          expand_vector->size += padding;

          /* If the vector points to data, we need to add the padding to
           * the end of that data.  If it points to the zero bytes at
           * the start then we can just grow it (but we must ensure that
           * it doesn't get too large).
           */
          if (expand_vector->data.offset)
            append_zeros (vectors->extra_bytes, padding);
          else
            g_assert (expand_vector->size < 8);

          return padding;
        }

      /* If the last vector was a GBytes then fall through */
    }

  /* Otherwise, record a new vector pointing to the padding bytes at the
   * start.
   */
  {
    GVariantVector v;

    v.gbytes = NULL;
    v.data.offset = 0;
    v.size = padding;

    g_array_append_val (vectors->vectors, v);
  }

  return padding;
}

void
g_variant_vectors_append_copy (GVariantVectors *vectors,
                               gconstpointer    data,
                               gsize            size)
{
  /* If the last vector that we stored was 'pad' or 'copy' then we will
   * expand it instead of adding a new one.
   */
  if (vectors->vectors->len)
    {
      GVariantVector *expand_vector = &g_array_index (vectors->vectors, GVariantVector, vectors->vectors->len - 1);

      if (expand_vector->gbytes == NULL)
        {
          /* If this was a padding vector then we must convert it to
           * data first.
           */
          if (expand_vector->data.offset == 0)
            {
              expand_vector->data.offset = vectors->extra_bytes->len;
              append_zeros (vectors->extra_bytes, expand_vector->size);
            }

          /* We have a vector pointing to data at the end of the
           * extra_bytes array, so just append there and grow the
           * vector.
           */
          g_byte_array_append (vectors->extra_bytes, data, size);
          expand_vector->size += size;
          return;
        }

      /* If the last vector was a GBytes then fall through */
    }

  /* Otherwise, copy the data and record a new vector. */
  {
    GVariantVector v;

    v.gbytes = NULL;
    v.data.offset = vectors->extra_bytes->len;
    v.size = size;

    g_byte_array_append (vectors->extra_bytes, data, size);
    g_array_append_val (vectors->vectors, v);
  }
}

void
g_variant_vectors_append_gbytes (GVariantVectors *vectors,
                                 GBytes          *gbytes,
                                 gconstpointer    data,
                                 gsize            size)
{
  GVariantVector v;

  /* Some very rough profiling has indicated that the trade-off for
   * overhead on the atomic operations involved in the ref/unref on the
   * GBytes is larger than the cost of the copy at ~128 bytes.
   */
  if (size < 128)
    {
      g_variant_vectors_append_copy (vectors, data, size);
      return;
    }

  v.gbytes = g_bytes_ref (gbytes);
  v.data.pointer = data;
  v.size = size;

  g_array_append_val (vectors->vectors, v);
}

typedef void (* WriteFunction) (gpointer base, gsize offset, gsize value);
static void write_1 (gpointer base, gsize offset, gsize value) { ((guint8 *) base)[offset] = value; }
static void write_2 (gpointer base, gsize offset, gsize value) { ((guint16 *) base)[offset] = GUINT16_TO_LE (value); }
static void write_4 (gpointer base, gsize offset, gsize value) { ((guint32 *) base)[offset] = GUINT32_TO_LE (value); }
static void write_8 (gpointer base, gsize offset, gsize value) { ((guint64 *) base)[offset] = GUINT64_TO_LE (value); }

typedef struct
{
  gsize         size;
  WriteFunction func;
} OffsetsHeader;

gsize
g_variant_vectors_reserve_offsets (GVariantVectors *vectors,
                                   guint            n_offsets,
                                   guint            offset_size)
{
  OffsetsHeader *header;
  gsize total_size;
  gsize add_size;
  guint key;

  total_size = n_offsets * offset_size;

  /* Add room for the metadata and round up to multiple of 8 */
  add_size = (sizeof (OffsetsHeader) + total_size + 7) & ~7ull;
  key = vectors->offsets->len;
  g_byte_array_set_size (vectors->offsets, key + add_size);
  header = (OffsetsHeader *) (vectors->offsets->data + key);
  key += sizeof (OffsetsHeader);
  header->size = total_size;

  switch (offset_size)
    {
    case 1:
      header->func = write_1;
      break;

    case 2:
      header->func = write_2;
      break;

    case 4:
      header->func = write_4;
      break;

    case 8:
      header->func = write_8;
      break;

    default:
      g_assert_not_reached ();
    }

  return key;
}

void
g_variant_vectors_write_to_offsets (GVariantVectors *vectors,
                                    gsize            offset,
                                    gsize            value,
                                    gsize            key)
{
  OffsetsHeader *header;
  guchar *offsets;

  offsets = vectors->offsets->data + key;
  header = (OffsetsHeader *) (offsets - sizeof (OffsetsHeader));

  header->func (offsets, offset, value);
}

void
g_variant_vectors_commit_offsets (GVariantVectors *vectors,
                                  gsize            key)
{
  OffsetsHeader *header;
  guchar *offsets;

  offsets = vectors->offsets->data + key;
  header = (OffsetsHeader *) (offsets - sizeof (OffsetsHeader));

  g_variant_vectors_append_copy (vectors, offsets, header->size);
  g_byte_array_set_size (vectors->offsets, key - sizeof (OffsetsHeader));
}
