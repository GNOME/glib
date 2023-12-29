/**
 * GITypeTag:
 * @GI_TYPE_TAG_VOID: void
 * @GI_TYPE_TAG_BOOLEAN: boolean
 * @GI_TYPE_TAG_INT8: 8-bit signed integer
 * @GI_TYPE_TAG_UINT8: 8-bit unsigned integer
 * @GI_TYPE_TAG_INT16: 16-bit signed integer
 * @GI_TYPE_TAG_UINT16: 16-bit unsigned integer
 * @GI_TYPE_TAG_INT32: 32-bit signed integer
 * @GI_TYPE_TAG_UINT32: 32-bit unsigned integer
 * @GI_TYPE_TAG_INT64: 64-bit signed integer
 * @GI_TYPE_TAG_UINT64: 64-bit unsigned integer
 * @GI_TYPE_TAG_FLOAT: float
 * @GI_TYPE_TAG_DOUBLE: double floating point
 * @GI_TYPE_TAG_GTYPE: a [type@GObject.Type]
 * @GI_TYPE_TAG_UTF8: a UTF-8 encoded string
 * @GI_TYPE_TAG_FILENAME: a filename, encoded in the same encoding
 *   as the native filesystem is using.
 * @GI_TYPE_TAG_ARRAY: an array
 * @GI_TYPE_TAG_INTERFACE: an extended interface object
 * @GI_TYPE_TAG_GLIST: a [type@GLib.List]
 * @GI_TYPE_TAG_GSLIST: a [type@GLib.SList]
 * @GI_TYPE_TAG_GHASH: a [type@GLib.HashTable]
 * @GI_TYPE_TAG_ERROR: a [type@GLib.Error]
 * @GI_TYPE_TAG_UNICHAR: Unicode character
 *
 * The type tag of a [class@GIRepository.TypeInfo].
 *
 * Since: 2.80
 */
typedef enum {
  /* Basic types */
  GI_TYPE_TAG_VOID      =  0,
  GI_TYPE_TAG_BOOLEAN   =  1,
  GI_TYPE_TAG_INT8      =  2,  /* Start of GI_TYPE_TAG_IS_NUMERIC types */
  GI_TYPE_TAG_UINT8     =  3,
  GI_TYPE_TAG_INT16     =  4,
  GI_TYPE_TAG_UINT16    =  5,
  GI_TYPE_TAG_INT32     =  6,
  GI_TYPE_TAG_UINT32    =  7,
  GI_TYPE_TAG_INT64     =  8,
  GI_TYPE_TAG_UINT64    =  9,
  GI_TYPE_TAG_FLOAT     = 10,
  GI_TYPE_TAG_DOUBLE    = 11,  /* End of numeric types */
  GI_TYPE_TAG_GTYPE     = 12,
  GI_TYPE_TAG_UTF8      = 13,
  GI_TYPE_TAG_FILENAME  = 14,
  /* Non-basic types; compare with GI_TYPE_TAG_IS_BASIC */
  GI_TYPE_TAG_ARRAY     = 15,  /* container (see GI_TYPE_TAG_IS_CONTAINER) */
  GI_TYPE_TAG_INTERFACE = 16,
  GI_TYPE_TAG_GLIST     = 17,  /* container */
  GI_TYPE_TAG_GSLIST    = 18,  /* container */
  GI_TYPE_TAG_GHASH     = 19,  /* container */
  GI_TYPE_TAG_ERROR     = 20,
  /* Another basic type */
  GI_TYPE_TAG_UNICHAR   = 21
  /* Note - there is currently only room for 32 tags */
} GITypeTag;

/**
 * GI_TYPE_TAG_N_TYPES:
 *
 * Number of entries in [enum@GIRepository.TypeTag].
 *
 * Since: 2.80
 */
#define GI_TYPE_TAG_N_TYPES (GI_TYPE_TAG_UNICHAR+1)

/**
 * GI_TYPE_TAG_IS_BASIC:
 * @tag: a type tag
 *
 * Checks if @tag is a basic type.
 *
 * Since: 2.80
 */
#define GI_TYPE_TAG_IS_BASIC(tag) ((tag) < GI_TYPE_TAG_ARRAY || (tag) == GI_TYPE_TAG_UNICHAR)
