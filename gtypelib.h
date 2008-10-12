/* GObject introspection: struct definitions for the binary
 * typelib format, validation
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

#ifndef __G_TYPELIB_H__
#define __G_TYPELIB_H__

#include <gmodule.h>
#include "girepository.h"

G_BEGIN_DECLS

#define G_IR_MAGIC "GOBJ\nMETADATA\r\n\032"

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

  guint32 dependencies;

  guint32 size;
  guint32 namespace;
  guint32 nsversion;
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

  guint16 local    : 1;
  guint16 reserved :15;

  guint32 name;
  guint32 offset;
} DirEntry;


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

  guint16        may_return_null              : 1;
  guint16        caller_owns_return_value     : 1;
  guint16        caller_owns_return_container : 1;
  guint16        reserved                     :13;

  guint16        n_arguments;

  ArgBlob        arguments[];
} SignatureBlob;

typedef struct
{
  guint16 blob_type;  /* 1 */

  guint16 deprecated : 1;
  guint16 reserved   :15;

  guint32 name;
} CommonBlob;

typedef struct 
{
  guint16 blob_type;  /* 1 */

  guint16 deprecated  : 1;
  guint16 setter      : 1; 
  guint16 getter      : 1;
  guint16 constructor : 1;
  guint16 wraps_vfunc : 1;
  guint16 reserved    : 1;
  guint16 index       :10;

  guint32 name;
  guint32 symbol;
  guint32 signature;
} FunctionBlob;

typedef struct 
{
  guint16 blob_type;  /* 2 */

  guint16 deprecated : 1;
  guint16 reserved   :15;

  guint32 name;
  guint32 signature;
} CallbackBlob;

typedef struct 
{
  guint8  pointer  :1;
  guint8  reserved :2;
  guint8  tag      :5;    
  guint8  reserved2;
  guint16 interface;  
} InterfaceTypeBlob;

typedef struct
{
  guint16 pointer         :1;
  guint16 reserved        :2;
  guint16 tag             :5;    

  guint16 zero_terminated :1;
  guint16 has_length      :1;
  guint16 reserved2       :6;

  guint16 length;

  SimpleTypeBlob type;
} ArrayTypeBlob;

typedef struct
{
  guint8  	 pointer  :1;
  guint8  	 reserved :2;
  guint8  	 tag      :5;    

  guint8  	 reserved2;
  guint16 	 n_types;

  SimpleTypeBlob type[];
} ParamTypeBlob;

typedef struct
{
  guint8  pointer  :1;
  guint8  reserved :2;
  guint8  tag      :5;    

  guint8  reserved2;
  guint16 n_domains;

  guint16 domains[];
}  ErrorTypeBlob;

typedef struct
{
  guint16 blob_type;  /* 10 */

  guint16 deprecated : 1;
  guint16 reserved   :15;
  
  guint32 name;

  guint32 get_quark;
  guint16 error_codes;
  guint16 reserved2;
} ErrorDomainBlob;

typedef struct
{
  guint32 deprecated : 1;
  guint32 reserved   :31;
  guint32 name;
  guint32 value;
} ValueBlob;

typedef struct 
{
  guint32        name;

  guint8         readable :1; 
  guint8         writable :1;
  guint8         reserved :6;
  guint8         bits;

  guint16        struct_offset;      

  SimpleTypeBlob type;
} FieldBlob;

typedef struct
{
  guint16 blob_type;  
  guint16 deprecated   : 1; 
  guint16 unregistered :15;
  guint32 name; 

  guint32 gtype_name;
  guint32 gtype_init;
} RegisteredTypeBlob;

typedef struct
{
  guint16   blob_type;

  guint16   deprecated   : 1;
  guint16   unregistered : 1;
  guint16   reserved     :14;

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
  guint16      deprecated    : 1;
  guint16      unregistered  : 1;
  guint16      discriminated : 1;
  guint16      reserved      :13;
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

  guint16   deprecated   : 1; 
  guint16   unregistered : 1;
  guint16   reserved     :14;

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

  guint32        deprecated     : 1;
  guint32        readable       : 1;
  guint32        writable       : 1;
  guint32        construct      : 1;
  guint32        construct_only : 1;
  guint32        reserved       :27;

  SimpleTypeBlob type;

} PropertyBlob;

typedef struct
{
  guint16 deprecated        : 1;
  guint16 run_first         : 1;
  guint16 run_last          : 1;
  guint16 run_cleanup       : 1;
  guint16 no_recurse        : 1;
  guint16 detailed          : 1;
  guint16 action            : 1;
  guint16 no_hooks          : 1;
  guint16 has_class_closure : 1;
  guint16 true_stops_emit   : 1;
  guint16 reserved          : 6;

  guint16 class_closure;

  guint32 name;

  guint32 signature;
} SignalBlob;

typedef struct 
{
  guint32 name;

  guint16 must_chain_up           : 1;
  guint16 must_be_implemented     : 1;
  guint16 must_not_be_implemented : 1;
  guint16 class_closure           : 1;
  guint16 reserved                :12;
  guint16 signal;

  guint16 struct_offset;
  guint16 reserved2;
  guint32 signature;
} VFuncBlob;

typedef struct
{
  guint16   blob_type;  /* 7 */
  guint16   deprecated   : 1; 
  guint16   reserved     :15;
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
  guint16 deprecated   : 1; 
  guint16 reserved     :15;
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
  guint16        deprecated   : 1; 
  guint16        reserved     :15;
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


struct _GTypelib {
  guchar *data;
  gsize len;
  gboolean owns_memory;
  GMappedFile *mfile;
  GList *modules;
};

DirEntry *g_typelib_get_dir_entry (GTypelib *typelib,
				   guint16   index);

void      g_typelib_check_sanity (void);

#define   g_typelib_get_string(typelib,offset) ((const gchar*)&(typelib->data)[(offset)])


typedef enum
{
  G_TYPELIB_ERROR_INVALID,
  G_TYPELIB_ERROR_INVALID_HEADER,
  G_TYPELIB_ERROR_INVALID_DIRECTORY,
  G_TYPELIB_ERROR_INVALID_ENTRY,
  G_TYPELIB_ERROR_INVALID_BLOB
} GTypelibError;

#define G_TYPELIB_ERROR (g_typelib_error_quark ())

GQuark g_typelib_error_quark (void);

gboolean g_typelib_validate (GTypelib  *typelib,
			     GError    **error);


G_END_DECLS

#endif  /* __G_TYPELIB_H__ */

