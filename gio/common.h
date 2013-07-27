#include <glib.h>

typedef struct
{
  guint16 le;
} dfi_uint16;

typedef struct
{
  guint32 le;
} dfi_uint32;

typedef struct
{
  dfi_uint32 offset;
} dfi_pointer;

typedef struct
{
  dfi_uint32 offset;
} dfi_string;

typedef dfi_uint16 dfi_id;

struct dfi_id_list
{
  dfi_uint16 n_ids;
  dfi_uint16 ids[1];
};

struct dfi_string_list
{
  dfi_uint16 n_strings;
  dfi_uint16 padding;
  dfi_string strings[1];
};

struct dfi_text_index_item
{
  dfi_string key;

  union
    {
      dfi_id      pair[2];
      dfi_pointer pointer;
    } value;
};

struct dfi_text_index
{
  dfi_uint32                 n_items;
  struct dfi_text_index_item items[1];
};

struct dfi_keyfile
{
  dfi_uint16 n_groups;
  dfi_uint16 n_items;
};

struct dfi_keyfile_group
{
  dfi_id     name_id;
  dfi_uint16 items_index;
};

struct dfi_keyfile_item
{
  dfi_id     key_id;
  dfi_id     locale_id;
  dfi_string value;
};

struct dfi_pointer_array
{
  dfi_pointer associated_string_list;
  dfi_pointer pointers[1];
};

struct dfi_header
{
  dfi_pointer app_names;       /* string list */
  dfi_pointer key_names;       /* string list */
  dfi_pointer locale_names;    /* string list */
  dfi_pointer group_names;     /* string list */

  dfi_pointer implementors;    /* pointer array of id lists, associated with group_names */
  dfi_pointer text_indexes;    /* pointer array of text indexes, associated with locale_names */
  dfi_pointer desktop_files;   /* pointer array of desktop files, associated with app_names */

  dfi_pointer mime_types;      /* text index */
};
