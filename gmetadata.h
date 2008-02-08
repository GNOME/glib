/* GObject introspection: struct definitions for the binary
 * metadata format, validation
 *
 * Copyright (C) 2005 Matthias Clasen
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
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __G_METADATA_H__
#define __G_METADATA_H__

#include <gmodule.h>
#include "girepository.h"

G_BEGIN_DECLS

#define G_IDL_MAGIC "GOBJ\nMETADATA\r\n\032"

enum 
{
  BLOB_TYPE_INVALID,
  BLOB_TYPE_FUNCTION,
  BLOB_TYPE_CALLBACK,
  BLOB_TYPE_STRUCT,
  BLOB_TYPE_BOXED,
  BLOB_TYPE_ENUM,
  BLOB_TYPE_FLAGS,
  BLOB_TYPE_OBJECT,
  BLOB_TYPE_INTERFACE,
  BLOB_TYPE_CONSTANT,
  BLOB_TYPE_ERROR_DOMAIN,
  BLOB_TYPE_UNION
};

typedef struct
{
  gchar   magic[16];
  guint8  major_version;
  guint8  minor_version;
  guint16 reserved;
  guint16 n_entries;
  guint16 n_local_entries;
  guint32 directory;
  guint32 n_annotations;
  guint32 annotations;

  guint32 size;
  guint32 namespace;
  guint32 shared_library;

  guint16 entry_blob_size;
  guint16 function_blob_size;
  guint16 callback_blob_size;
  guint16 signal_blob_size;
  guint16 vfunc_blob_size;
  guint16 arg_blob_size;
  guint16 property_blob_size;
  guint16 field_blob_size;
  guint16 value_blob_size;
  guint16 annotation_blob_size;
  guint16 constant_blob_size;
  guint16 error_domain_blob_size;

  guint16 signature_blob_size;
  guint16 enum_blob_size;
  guint16 struct_blob_size;
  guint16 object_blob_size;
  guint16 interface_blob_size;
  guint16 union_blob_size;
  
  guint16 padding[7];
} Header;

typedef struct
{
  guint16 blob_type;

  guint   local    : 1;
  guint   reserved :15;

  guint32 name;
  guint32 offset;
} DirEntry;


#define TYPE_POINTER_MASK 1 << 7
#define TYPE_TAG_MASK         63

typedef enum 
{
  TYPE_TAG_VOID      =  0,
  TYPE_TAG_BOOLEAN   =  1,
  TYPE_TAG_INT8      =  2,
  TYPE_TAG_UINT8     =  3,
  TYPE_TAG_INT16     =  4,
  TYPE_TAG_UINT16    =  5,  
  TYPE_TAG_INT32     =  6,
  TYPE_TAG_UINT32    =  7,
  TYPE_TAG_INT64     =  8,
  TYPE_TAG_UINT64    =  9,
  TYPE_TAG_INT       = 10,
  TYPE_TAG_UINT      = 11,
  TYPE_TAG_LONG      = 12,
  TYPE_TAG_ULONG     = 13,
  TYPE_TAG_SSIZE     = 14,
  TYPE_TAG_SIZE      = 15,
  TYPE_TAG_FLOAT     = 16,
  TYPE_TAG_DOUBLE    = 17,
  TYPE_TAG_UTF8      = 18,
  TYPE_TAG_FILENAME  = 19,
  TYPE_TAG_ARRAY     = 20,
  TYPE_TAG_INTERFACE = 21,
  TYPE_TAG_LIST      = 22,
  TYPE_TAG_SLIST     = 23,
  TYPE_TAG_HASH      = 24,
  TYPE_TAG_ERROR     = 25
} TypeTag;

typedef union
{
  struct 
  {
    guint reserved   : 8;
    guint reserved2  :16;
    guint pointer    : 1;
    guint reserved3  : 2;
    guint tag        : 5;    
  };
  guint32    offset;
} SimpleTypeBlob;


typedef struct
{
  guint32        name;

  guint          in                           : 1;
  guint          out                          : 1;
  guint          dipper                       : 1;
  guint          null_ok                      : 1;
  guint          optional                     : 1;
  guint          transfer_ownership           : 1;
  guint          transfer_container_ownership : 1;
  guint          return_value                 : 1;
  guint          reserved                     :24;

  SimpleTypeBlob arg_type;
} ArgBlob;

typedef struct 
{
  SimpleTypeBlob return_type;

  guint          may_return_null              : 1;
  guint          caller_owns_return_value     : 1;
  guint          caller_owns_return_container : 1;
  guint          reserved                     :13;

  guint16        n_arguments;

  ArgBlob        arguments[];
} SignatureBlob;

typedef struct
{
  guint16 blob_type;  /* 1 */

  guint   deprecated     : 1;
  guint   reserved       :15;

  guint32 name;
} CommonBlob;

typedef struct 
{
  guint16 blob_type;  /* 1 */

  guint   deprecated     : 1;
  guint   setter         : 1; 
  guint   getter         : 1;
  guint   constructor    : 1;
  guint   wraps_vfunc    : 1;
  guint   reserved       : 1;
  guint   index          :10;

  guint32 name;
  guint32 symbol;
  guint32 signature;
} FunctionBlob;

typedef struct 
{
  guint16 blob_type;  /* 2 */

  guint   deprecated     : 1;
  guint   reserved       :15;

  guint32 name;
  guint32 signature;
} CallbackBlob;

typedef struct 
{
  guint pointer    :1;
  guint reserved   :2;
  guint tag        :5;    
  guint8     reserved2;
  guint16    interface;  
} InterfaceTypeBlob;

typedef struct
{
  guint pointer    :1;
  guint reserved   :2;
  guint tag        :5;    

  guint          zero_terminated :1;
  guint          has_length      :1;
  guint          reserved2       :6;

  guint16        length;

  SimpleTypeBlob type;
} ArrayTypeBlob;

typedef struct
{
  guint pointer    :1;
  guint reserved   :2;
  guint tag        :5;    

  guint8         reserved2;
  guint16        n_types;

  SimpleTypeBlob type[];
} ParamTypeBlob;

typedef struct
{
  guint pointer    :1;
  guint reserved   :2;
  guint tag        :5;    

  guint8     reserved2;
  guint16    n_domains;

  guint16    domains[];
}  ErrorTypeBlob;

typedef struct
{
  guint16 blob_type;  /* 10 */

  guint   deprecated     : 1;
  guint   reserved       :15;
  
  guint32 name;

  guint32 get_quark;
  guint16 error_codes;
  guint16 reserved2;
} ErrorDomainBlob;

typedef struct
{
  guint   deprecated : 1;
  guint   reserved   :31;
  guint32 name;
  guint32 value;
} ValueBlob;

typedef struct 
{
  guint32        name;

  guint          readable : 1; 
  guint          writable : 1;
  guint          reserved : 6;
  guint8         bits;

  guint16        struct_offset;      

  SimpleTypeBlob type;
} FieldBlob;

typedef struct
{
  guint16 blob_type;  
  guint   deprecated   : 1; 
  guint   unregistered :15;
  guint32 name; 

  guint32 gtype_name;
  guint32 gtype_init;
} RegisteredTypeBlob;

typedef struct
{
  guint16   blob_type;

  guint     deprecated   : 1;
  guint     unregistered : 1;
  guint     reserved     :14;

  guint32   name;

  guint32   gtype_name;
  guint32   gtype_init;

  guint16   n_fields;
  guint16   n_methods;

#if 0
  /* variable-length parts of the blob */
  FieldBlob    fields[];   
  FunctionBlob methods[];
#endif
} StructBlob;

typedef struct 
{  
  guint16      blob_type; 
  guint        deprecated    : 1;
  guint        unregistered  : 1;
  guint        discriminated : 1;
  guint        reserved      :13;
  guint32      name;

  guint32      gtype_name;
  guint32      gtype_init;

  guint16      n_fields;
  guint16      n_functions;

  gint32       discriminator_offset; 
  SimpleTypeBlob discriminator_type;

#if 0
  FieldBlob    fields[];   
  FunctionBlob functions[];  
  ConstantBlob discriminator_values[]
#endif
} UnionBlob;

typedef struct
{
  guint16   blob_type;

  guint     deprecated   : 1; 
  guint     unregistered : 1;
  guint     reserved     :14;

  guint32   name; 

  guint32   gtype_name;
  guint32   gtype_init;

  guint16   n_values;
  guint16   reserved2;

  ValueBlob values[];    
} EnumBlob;

typedef struct
{
  guint32        name;

  guint          deprecated     : 1;
  guint          readable       : 1;
  guint          writable       : 1;
  guint          construct      : 1;
  guint          construct_only : 1;
  guint          reserved       :27;

  SimpleTypeBlob type;

} PropertyBlob;

typedef struct
{
  guint   deprecated        : 1;
  guint   run_first         : 1;
  guint   run_last          : 1;
  guint   run_cleanup       : 1;
  guint   no_recurse        : 1;
  guint   detailed          : 1;
  guint   action            : 1;
  guint   no_hooks          : 1;
  guint   has_class_closure : 1;
  guint   true_stops_emit   : 1;
  guint   reserved          : 6;

  guint16 class_closure;

  guint32 name;

  guint32 signature;
} SignalBlob;

typedef struct 
{
  guint32 name;

  guint   must_chain_up           : 1;
  guint   must_be_implemented     : 1;
  guint   must_not_be_implemented : 1;
  guint   class_closure           : 1;
  guint   reserved                :12;
  guint16 signal;

  guint16 struct_offset;
  guint16 reserved2;
  guint32 signature;
} VFuncBlob;

typedef struct
{
  guint16   blob_type;  /* 7 */
  guint     deprecated   : 1; 
  guint     reserved     :15;
  guint32   name; 

  guint32   gtype_name;
  guint32   gtype_init;

  guint16   parent;

  guint16   n_interfaces;
  guint16   n_fields;
  guint16   n_properties;
  guint16   n_methods;
  guint16   n_signals;
  guint16   n_vfuncs;
  guint16   n_constants;

  guint16   interfaces[];
 
#if 0
  /* variable-length parts of the blob */
  FieldBlob           fields[];
  PropertyBlob        properties[];
  FunctionBlob        methods[];
  SignalBlob          signals[];
  VFuncBlob           vfuncs[];
  ConstantBlob        constants[];
#endif
} ObjectBlob;

typedef struct 
{
  guint16 blob_type;  
  guint   deprecated   : 1; 
  guint   reserved     :15;
  guint32 name; 

  guint32 gtype_name;
  guint32 gtype_init;

  guint16 n_prerequisites;
  guint16 n_properties;
  guint16 n_methods;
  guint16 n_signals;
  guint16 n_vfuncs;
  guint16 n_constants;  

  guint16 prerequisites[];

#if 0 
  /* variable-length parts of the blob */
  PropertyBlob        properties[];
  FunctionBlob        methods[];
  SignalBlob          signals[];
  VFuncBlob           vfuncs[];
  ConstantBlob        constants[];
#endif
} InterfaceBlob;


typedef struct
{
  guint16        blob_type;
  guint          deprecated   : 1; 
  guint          reserved     :15;
  guint32        name; 

  SimpleTypeBlob type;

  guint32        size;
  guint32        offset;
} ConstantBlob;

typedef struct
{ 
  guint32 offset;
  guint32 name;
  guint32 value;
} AnnotationBlob;


struct _GMetadata {
  guchar *data;
  gsize len;
  gboolean owns_memory;
  GMappedFile *mfile;
  GModule *module;
};

DirEntry *g_metadata_get_dir_entry (GMetadata *metadata,
				    guint16            index);

void      g_metadata_check_sanity (void);

#define   g_metadata_get_string(metadata,offset) ((const gchar*)&(metadata->data)[(offset)])


typedef enum
{
  G_METADATA_ERROR_INVALID,
  G_METADATA_ERROR_INVALID_HEADER,
  G_METADATA_ERROR_INVALID_DIRECTORY,
  G_METADATA_ERROR_INVALID_ENTRY,
  G_METADATA_ERROR_INVALID_BLOB
} GMetadataError;

#define G_METADATA_ERROR (g_metadata_error_quark ())

GQuark g_metadata_error_quark (void);

gboolean g_metadata_validate (GMetadata  *metadata,
			      GError    **error);


G_END_DECLS

#endif  /* __G_METADATA_H__ */

