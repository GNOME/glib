/* GObject introspection: typelib validation, auxiliary functions 
 * related to the binary typelib format
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

#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include "config.h"
#include "gtypelib.h"

typedef struct {
  GTypelib *typelib;
  GSList *context_stack;
} ValidateContext;

#define ALIGN_VALUE(this, boundary) \
  (( ((unsigned long)(this)) + (((unsigned long)(boundary)) -1)) & (~(((unsigned long)(boundary))-1)))

static void
push_context (ValidateContext *ctx, const char *name)
{
  ctx->context_stack = g_slist_prepend (ctx->context_stack, (char*)name);
}

static void
pop_context (ValidateContext *ctx)
{
  g_assert (ctx->context_stack != NULL);
  ctx->context_stack = g_slist_delete_link (ctx->context_stack, 
					    ctx->context_stack);
}

static gboolean
validate_interface_blob (ValidateContext *ctx,
			 guint32        offset,
			 GError       **error);

static DirEntry *
get_dir_entry_checked (GTypelib *typelib,
		       guint16    index,
		       GError   **error)
{
  Header *header = (Header *)typelib->data;
  guint32 offset;

  if (index == 0 || index > header->n_entries)
    {
      g_set_error (error,
		   G_TYPELIB_ERROR,
		   G_TYPELIB_ERROR_INVALID_BLOB,
		   "Invalid directory index %d", index);
      return FALSE;	        
    }

  offset = header->directory + (index - 1) * header->entry_blob_size;

  if (typelib->len < offset + sizeof (DirEntry))
    {
      g_set_error (error,
		   G_TYPELIB_ERROR,
		   G_TYPELIB_ERROR_INVALID,
		   "The buffer is too short");
      return FALSE;
    }

  return (DirEntry *)&typelib->data[offset];
}


static CommonBlob *
get_blob (GTypelib *typelib,
	  guint32   offset,
	  GError  **error)
{
  if (typelib->len < offset + sizeof (CommonBlob))
    {
      g_set_error (error,
		   G_TYPELIB_ERROR,
		   G_TYPELIB_ERROR_INVALID,
		   "The buffer is too short");
      return FALSE;
    }
  return (CommonBlob *)&typelib->data[offset];
}

static InterfaceTypeBlob *
get_type_blob (GTypelib *typelib,
	       SimpleTypeBlob *simple,
	       GError  **error)
{
  if (simple->offset == 0)
    {
      g_set_error (error,
		   G_TYPELIB_ERROR,
		   G_TYPELIB_ERROR_INVALID,
		   "Expected blob for type");
      return FALSE;
    }

  if (simple->flags.reserved == 0 && simple->flags.reserved2 == 0)
    {
      g_set_error (error,
		   G_TYPELIB_ERROR,
		   G_TYPELIB_ERROR_INVALID,
		   "Expected non-basic type but got %d",
		   simple->flags.tag);
      return FALSE;
    }

  return (InterfaceTypeBlob*) get_blob (typelib, simple->offset, error);
}

DirEntry *
g_typelib_get_dir_entry (GTypelib *typelib,
			  guint16    index)
{
  Header *header = (Header *)typelib->data;

  return (DirEntry *)&typelib->data[header->directory + (index - 1) * header->entry_blob_size];
}

void    
g_typelib_check_sanity (void)
{
  /* Check that struct layout is as we expect */

  gboolean size_check_ok = TRUE;

#define CHECK_SIZE(s,n) \
  if (sizeof(s) != n) \
    { \
      g_printerr ("sizeof("#s") is expected to be %d but is %"G_GSIZE_FORMAT".\n", \
		  n, sizeof (s));					\
      size_check_ok = FALSE; \
    }

  /* When changing the size of a typelib structure, you are required to update
   * the hardcoded size here.  Do NOT change these to use sizeof(); these
   * should match whatever is defined in the text specification and serve as
   * a sanity check on structure modifications.
   *
   * Everything else in the code however should be using sizeof().
   */
  
  CHECK_SIZE (Header, 112);
  CHECK_SIZE (DirEntry, 12);
  CHECK_SIZE (SimpleTypeBlob, 4);
  CHECK_SIZE (ArgBlob, 16);
  CHECK_SIZE (SignatureBlob, 8);
  CHECK_SIZE (CommonBlob, 8);
  CHECK_SIZE (FunctionBlob, 20);
  CHECK_SIZE (CallbackBlob, 12);
  CHECK_SIZE (InterfaceTypeBlob, 4);
  CHECK_SIZE (ArrayTypeBlob, 8);
  CHECK_SIZE (ParamTypeBlob, 4);
  CHECK_SIZE (ErrorTypeBlob, 4);
  CHECK_SIZE (ErrorDomainBlob, 16);
  CHECK_SIZE (ValueBlob, 12);
  CHECK_SIZE (FieldBlob, 16);
  CHECK_SIZE (RegisteredTypeBlob, 16);
  CHECK_SIZE (StructBlob, 32);
  CHECK_SIZE (EnumBlob, 24);
  CHECK_SIZE (PropertyBlob, 16);
  CHECK_SIZE (SignalBlob, 16);
  CHECK_SIZE (VFuncBlob, 20);
  CHECK_SIZE (ObjectBlob, 44);
  CHECK_SIZE (InterfaceBlob, 40);
  CHECK_SIZE (ConstantBlob, 24);
  CHECK_SIZE (AttributeBlob, 12);
  CHECK_SIZE (UnionBlob, 40);
#undef CHECK_SIZE

  g_assert (size_check_ok);
}


static gboolean
is_aligned (guint32 offset)
{
  return offset == ALIGN_VALUE (offset, 4);
}

#define MAX_NAME_LEN 200

static const char *
get_string (GTypelib *typelib, guint32 offset, GError **error)
{
  if (typelib->len < offset)
    {
      g_set_error (error,
		   G_TYPELIB_ERROR,
		   G_TYPELIB_ERROR_INVALID,
		   "Buffer is too short while looking up name");
      return NULL;
    }

  return (const char*)&typelib->data[offset];
}

static const char *
get_string_nofail (GTypelib *typelib, guint32 offset)
{
  const char *ret = get_string (typelib, offset, NULL);
  g_assert (ret);
  return ret;
}

static gboolean
validate_name (GTypelib   *typelib,
	       const char *msg,
	       const guchar *data, guint32 offset,
	       GError **error)
{
  const char *name;

  name = get_string (typelib, offset, error);
  if (!name)
    return FALSE;

  if (!memchr (name, '\0', MAX_NAME_LEN)) 
    {
      g_set_error (error,
		   G_TYPELIB_ERROR,
		   G_TYPELIB_ERROR_INVALID,
		   "The %s is too long: %s",
		   msg, name);
      return FALSE;
    }
  
  if (strspn (name, G_CSET_a_2_z G_CSET_A_2_Z G_CSET_DIGITS "-_") < strlen (name)) 
    {
      g_set_error (error,
		   G_TYPELIB_ERROR,
		   G_TYPELIB_ERROR_INVALID,
		   "The %s is contains invalid characters: %s",
		   msg, name);
      return FALSE;
    }
  
  return TRUE;
}

static gboolean 
validate_header (ValidateContext  *ctx,
		 GError          **error)
{
  GTypelib *typelib = ctx->typelib;
  Header *header;

  if (typelib->len < sizeof (Header))
    {
      g_set_error (error,
		   G_TYPELIB_ERROR,
		   G_TYPELIB_ERROR_INVALID,
		   "The buffer is too short");
      return FALSE;
    }

  header = (Header *)typelib->data;

  if (strncmp (header->magic, G_IR_MAGIC, 16) != 0)
    {
      g_set_error (error,
		   G_TYPELIB_ERROR,
		   G_TYPELIB_ERROR_INVALID_HEADER,
		   "Magic string not found");
      return FALSE;
      
    }

  if (header->major_version != 2 || header->minor_version != 0)
    {
      g_set_error (error,
		   G_TYPELIB_ERROR,
		   G_TYPELIB_ERROR_INVALID_HEADER,
		   "Version mismatch");
      return FALSE;
      
    }

  if (header->n_entries < header->n_local_entries)
    {
      g_set_error (error,
		   G_TYPELIB_ERROR,
		   G_TYPELIB_ERROR_INVALID_HEADER,
		   "Inconsistent entry counts");
      return FALSE; 
    }

  if (header->size != typelib->len)
    {
      g_set_error (error,
		   G_TYPELIB_ERROR,
		   G_TYPELIB_ERROR_INVALID_HEADER,
		   "Typelib size mismatch");
      return FALSE; 
    }
  
  /* This is a sanity check for a specific typelib; it
   * prevents us from loading an incompatible typelib.
   * 
   * The hardcoded checks in g_typelib_check_sanity to
   * protect against inadvertent or buggy changes to the typelib format
   * itself.
   */

  if (header->entry_blob_size != sizeof (DirEntry) ||
      header->function_blob_size != sizeof (FunctionBlob) ||
      header->callback_blob_size != sizeof (CallbackBlob) ||
      header->signal_blob_size != sizeof (SignalBlob) ||
      header->vfunc_blob_size != sizeof (VFuncBlob) ||
      header->arg_blob_size != sizeof (ArgBlob) ||
      header->property_blob_size != sizeof (PropertyBlob) ||
      header->field_blob_size != sizeof (FieldBlob) ||
      header->value_blob_size != sizeof (ValueBlob) ||
      header->constant_blob_size != sizeof (ConstantBlob) ||
      header->error_domain_blob_size != sizeof (ErrorDomainBlob) ||
      header->attribute_blob_size != sizeof (AttributeBlob) ||
      header->signature_blob_size != sizeof (SignatureBlob) ||
      header->enum_blob_size != sizeof (EnumBlob) ||
      header->struct_blob_size != sizeof (StructBlob) ||
      header->object_blob_size != sizeof(ObjectBlob) ||
      header->interface_blob_size != sizeof (InterfaceBlob) ||
      header->union_blob_size != sizeof (UnionBlob))
    {
      g_set_error (error,
		   G_TYPELIB_ERROR,
		   G_TYPELIB_ERROR_INVALID_HEADER,
		   "Blob size mismatch");
      return FALSE; 
    }

  if (!is_aligned (header->directory))
    {
      g_set_error (error,
		   G_TYPELIB_ERROR,
		   G_TYPELIB_ERROR_INVALID_HEADER,
		   "Misaligned directory");
      return FALSE; 
    }

  if (!is_aligned (header->attributes))
    {
      g_set_error (error,
		   G_TYPELIB_ERROR,
		   G_TYPELIB_ERROR_INVALID_HEADER,
		   "Misaligned attributes");
      return FALSE; 
    }

  if (header->attributes == 0 && header->n_attributes > 0)
    {
      g_set_error (error,
		   G_TYPELIB_ERROR,
		   G_TYPELIB_ERROR_INVALID_HEADER,
		   "Wrong number of attributes");
      return FALSE; 
    }

  if (!validate_name (typelib, "namespace", typelib->data, header->namespace, error))
    return FALSE; 

  return TRUE;
}

static gboolean validate_type_blob (GTypelib     *typelib,
				    guint32        offset,
				    guint32        signature_offset,
				    gboolean       return_type,
				    GError       **error);

static gboolean
validate_array_type_blob (GTypelib     *typelib,
			  guint32        offset,
			  guint32        signature_offset,
			  gboolean       return_type,
			  GError       **error)
{
  ArrayTypeBlob *blob;

  blob = (ArrayTypeBlob*)&typelib->data[offset];

  if (!blob->pointer)
    {
      g_set_error (error,
		   G_TYPELIB_ERROR,
		   G_TYPELIB_ERROR_INVALID_BLOB,
		   "Pointer type exected for tag %d", blob->tag);
      return FALSE;	  
    }

  /* FIXME validate length */

  if (!validate_type_blob (typelib,
			   offset + G_STRUCT_OFFSET (ArrayTypeBlob, type),
			   0, FALSE, error))
    return FALSE;

  return TRUE;
}

static gboolean
validate_iface_type_blob (GTypelib     *typelib,
			  guint32        offset,
			  guint32        signature_offset,
			  gboolean       return_type,
			  GError       **error)
{
  InterfaceTypeBlob *blob;
  InterfaceBlob *target;

  blob = (InterfaceTypeBlob*)&typelib->data[offset];

  target = (InterfaceBlob*) get_dir_entry_checked (typelib, blob->interface, error);

  if (!target)
    return FALSE;
  if (target->blob_type == 0) /* non-local */
    return TRUE;

  return TRUE;
}

static gboolean
validate_param_type_blob (GTypelib     *typelib,
			  guint32        offset,
			  guint32        signature_offset,
			  gboolean       return_type,
			  gint           n_params,
			  GError       **error)
{
  ParamTypeBlob *blob;
  gint i;

  blob = (ParamTypeBlob*)&typelib->data[offset];

  if (!blob->pointer)
    {
      g_set_error (error,
		   G_TYPELIB_ERROR,
		   G_TYPELIB_ERROR_INVALID_BLOB,
		   "Pointer type exected for tag %d", blob->tag);
      return FALSE;	  
    }
  
  if (blob->n_types != n_params)
    {
      g_set_error (error,
		   G_TYPELIB_ERROR,
		   G_TYPELIB_ERROR_INVALID_BLOB,
		   "Parameter type number mismatch");
      return FALSE;	        
    }
  
  for (i = 0; i < n_params; i++)
    {
      if (!validate_type_blob (typelib,
			       offset + sizeof (ParamTypeBlob) +
			       i * sizeof (SimpleTypeBlob),
			       0, FALSE, error))
	return FALSE;
    }

  return TRUE;
}

static gboolean
validate_error_type_blob (GTypelib     *typelib,
			  guint32        offset,
			  guint32        signature_offset,
			  gboolean       return_type,
			  GError       **error)
{
  ErrorTypeBlob *blob;
  Header *header;
  gint i;
  DirEntry *entry;

  blob = (ErrorTypeBlob*)&typelib->data[offset];

  header = (Header *)typelib->data;

  if (!blob->pointer)
    {
      g_set_error (error,
		   G_TYPELIB_ERROR,
		   G_TYPELIB_ERROR_INVALID_BLOB,
		   "Pointer type exected for tag %d", blob->tag);
      return FALSE;	  
    }
  
  for (i = 0; i < blob->n_domains; i++)
    {
      if (blob->domains[i] == 0 || blob->domains[i] > header->n_entries)
	{
	  g_set_error (error,
		       G_TYPELIB_ERROR,
		       G_TYPELIB_ERROR_INVALID_BLOB,
		       "Invalid directory index %d", blob->domains[i]);
	  return FALSE;	        
	}

      entry = g_typelib_get_dir_entry (typelib, blob->domains[i]);

      if (entry->blob_type != BLOB_TYPE_ERROR_DOMAIN &&
	  (entry->local || entry->blob_type != BLOB_TYPE_INVALID))
	{
	  g_set_error (error,
		       G_TYPELIB_ERROR,
		       G_TYPELIB_ERROR_INVALID_BLOB,
		       "Wrong blob type");
	  return FALSE;	        
	}
    }

  return TRUE;
}

static gboolean
validate_type_blob (GTypelib     *typelib,
		    guint32        offset,
		    guint32        signature_offset,
		    gboolean       return_type,
		    GError       **error)
{
  SimpleTypeBlob *simple;
  InterfaceTypeBlob *iface;
  
  simple = (SimpleTypeBlob *)&typelib->data[offset];

  if (simple->flags.reserved == 0 && 
      simple->flags.reserved2 == 0)
    {
      if (simple->flags.tag >= GI_TYPE_TAG_ARRAY)
	{
	  g_set_error (error,
		       G_TYPELIB_ERROR,
		       G_TYPELIB_ERROR_INVALID_BLOB,
		       "Wrong tag in simple type");
	  return FALSE;
	}
      
      if (simple->flags.tag >= GI_TYPE_TAG_UTF8 &&
	  !simple->flags.pointer)
	{
	  g_set_error (error,
		       G_TYPELIB_ERROR,
		       G_TYPELIB_ERROR_INVALID_BLOB,
		       "Pointer type exected for tag %d", simple->flags.tag);
	  return FALSE;	  
	}

      return TRUE;
    }

  iface = (InterfaceTypeBlob*)&typelib->data[simple->offset];

  switch (iface->tag)
    {
    case GI_TYPE_TAG_ARRAY:
      if (!validate_array_type_blob (typelib, simple->offset, 
				     signature_offset, return_type, error))
	return FALSE;
      break;
    case GI_TYPE_TAG_INTERFACE:
      if (!validate_iface_type_blob (typelib, simple->offset, 
				     signature_offset, return_type, error))
	return FALSE;
      break;
    case GI_TYPE_TAG_GLIST:
    case GI_TYPE_TAG_GSLIST:
      if (!validate_param_type_blob (typelib, simple->offset, 
				     signature_offset, return_type, 1, error))
	return FALSE;
      break;
    case GI_TYPE_TAG_GHASH:
      if (!validate_param_type_blob (typelib, simple->offset, 
				     signature_offset, return_type, 2, error))
	return FALSE;
      break;
    case GI_TYPE_TAG_ERROR:
      if (!validate_error_type_blob (typelib, simple->offset, 
				     signature_offset, return_type, error))
	return FALSE;
      break;
    default:
      g_set_error (error,
		   G_TYPELIB_ERROR,
		   G_TYPELIB_ERROR_INVALID_BLOB,
		   "Wrong tag in complex type");
      return FALSE;
    }

  return TRUE;
}

static gboolean
validate_arg_blob (GTypelib     *typelib,
		   guint32        offset,
		   guint32        signature_offset,
		   GError       **error)
{
  ArgBlob *blob;

  if (typelib->len < offset + sizeof (ArgBlob))
    {
      g_set_error (error,
		   G_TYPELIB_ERROR,
		   G_TYPELIB_ERROR_INVALID,
		   "The buffer is too short");
      return FALSE;
    }

  blob = (ArgBlob*) &typelib->data[offset];

  if (!validate_name (typelib, "argument", typelib->data, blob->name, error))
    return FALSE; 
 
  if (!validate_type_blob (typelib, 
			   offset + G_STRUCT_OFFSET (ArgBlob, arg_type), 
			   signature_offset, FALSE, error))
    return FALSE;

  return TRUE;
}

static SimpleTypeBlob *
return_type_from_signature (GTypelib *typelib,
			    guint32   offset,
			    GError  **error)
{
  SignatureBlob *blob;
  if (typelib->len < offset + sizeof (SignatureBlob))
    {
      g_set_error (error,
		   G_TYPELIB_ERROR,
		   G_TYPELIB_ERROR_INVALID,
		   "The buffer is too short");
      return NULL;
    }

  blob = (SignatureBlob*) &typelib->data[offset];
  if (blob->return_type.offset == 0)
    {
      g_set_error (error,
		   G_TYPELIB_ERROR,
		   G_TYPELIB_ERROR_INVALID,
		   "No return type found in signature");
      return NULL;
    }

  return (SimpleTypeBlob *)&typelib->data[offset + G_STRUCT_OFFSET (SignatureBlob, return_type)];
}

static gboolean
validate_signature_blob (GTypelib     *typelib,
			 guint32        offset,
			 GError       **error)
{
  SignatureBlob *blob;
  gint i;

  if (typelib->len < offset + sizeof (SignatureBlob))
    {
      g_set_error (error,
		   G_TYPELIB_ERROR,
		   G_TYPELIB_ERROR_INVALID,
		   "The buffer is too short");
      return FALSE;
    }

  blob = (SignatureBlob*) &typelib->data[offset];

  if (blob->return_type.offset != 0)
    {
      if (!validate_type_blob (typelib, 
			       offset + G_STRUCT_OFFSET (SignatureBlob, return_type), 
			       offset, TRUE, error))
	return FALSE;
    }

  for (i = 0; i < blob->n_arguments; i++)
    {
      if (!validate_arg_blob (typelib, 
			      offset + sizeof (SignatureBlob) + 
			      i * sizeof (ArgBlob), 
			      offset, 
			      error))
	return FALSE;
    }

  /* FIXME check constraints on return_value */
  /* FIXME check array-length pairs */
  return TRUE;
}

static gboolean
validate_function_blob (ValidateContext *ctx,
			guint32        offset,
			guint16        container_type,
			GError       **error)
{
  GTypelib *typelib = ctx->typelib;
  FunctionBlob *blob;
  SignatureBlob *sigblob;
  gboolean is_method;

  if (typelib->len < offset + sizeof (FunctionBlob))
    {
      g_set_error (error,
		   G_TYPELIB_ERROR,
		   G_TYPELIB_ERROR_INVALID,
		   "The buffer is too short");
      return FALSE;
    }

  blob = (FunctionBlob*) &typelib->data[offset];

  if (blob->blob_type != BLOB_TYPE_FUNCTION)
    {
      g_set_error (error,
		   G_TYPELIB_ERROR,
		   G_TYPELIB_ERROR_INVALID_BLOB,
		   "Wrong blob type %d, expected function", blob->blob_type);
      return FALSE;
    }

  if (!validate_name (typelib, "function", typelib->data, blob->name, error))
    return FALSE; 

  push_context (ctx, get_string_nofail (typelib, blob->name));
  
  if (!validate_name (typelib, "function symbol", typelib->data, blob->symbol, error))
    return FALSE; 

  switch (container_type)
    {
    case BLOB_TYPE_BOXED:
    case BLOB_TYPE_STRUCT:
    case BLOB_TYPE_UNION:
    case BLOB_TYPE_OBJECT:
    case BLOB_TYPE_INTERFACE:
      is_method = !(blob->constructor || blob->setter || blob->getter || blob->wraps_vfunc);
      break;
    default:
      is_method = FALSE;
    }
  
  if (blob->constructor)
    {
      switch (container_type)
	{
	case BLOB_TYPE_BOXED:
	case BLOB_TYPE_STRUCT:
	case BLOB_TYPE_UNION:
	case BLOB_TYPE_OBJECT:
	case BLOB_TYPE_INTERFACE:
	  break;
	default:
	  g_set_error (error,
		       G_TYPELIB_ERROR,
		       G_TYPELIB_ERROR_INVALID_BLOB,
		       "Constructor not allowed");
	  return FALSE;
	}
    }

  if (blob->setter || blob->getter || blob->wraps_vfunc)
    {
      switch (container_type)
	{
	case BLOB_TYPE_OBJECT:
	case BLOB_TYPE_INTERFACE:
	  break;
	default:
	  g_set_error (error,
		       G_TYPELIB_ERROR,
		       G_TYPELIB_ERROR_INVALID_BLOB,
		       "Setter, getter or wrapper not allowed");
	  return FALSE;
	}
    }

  if (blob->index)
    {
      if (!(blob->setter || blob->getter || blob->wraps_vfunc))
	{
	  g_set_error (error,
		       G_TYPELIB_ERROR,
		       G_TYPELIB_ERROR_INVALID_BLOB,
		       "Must be setter, getter or wrapper");
	  return FALSE;
	}
    }

  /* FIXME: validate index range */

  if (!validate_signature_blob (typelib, blob->signature, error))
    return FALSE;

  sigblob = (SignatureBlob*) &typelib->data[blob->signature];

  if (blob->constructor) 
    {
      SimpleTypeBlob *simple = return_type_from_signature (typelib,
							   blob->signature,
							   error);
      InterfaceTypeBlob *iface_type;

      if (!simple)
	return FALSE;
      iface_type = get_type_blob (typelib, simple, error);
      if (!iface_type)
	return FALSE;
      if (!(iface_type->tag == GI_TYPE_TAG_INTERFACE))
	{
	  g_set_error (error,
		       G_TYPELIB_ERROR,
		       G_TYPELIB_ERROR_INVALID,
		       "Invalid return type %d for constructor",
		       iface_type->tag);
	  return FALSE;
	}
    }

  pop_context (ctx);

  return TRUE;
}

static gboolean
validate_callback_blob (ValidateContext *ctx,
			guint32        offset,
			GError       **error)
{
  GTypelib *typelib = ctx->typelib;
  CallbackBlob *blob;

  if (typelib->len < offset + sizeof (CallbackBlob))
    {
      g_set_error (error,
		   G_TYPELIB_ERROR,
		   G_TYPELIB_ERROR_INVALID,
		   "The buffer is too short");
      return FALSE;
    }

  blob = (CallbackBlob*) &typelib->data[offset];

  if (blob->blob_type != BLOB_TYPE_CALLBACK)
    {
      g_set_error (error,
		   G_TYPELIB_ERROR,
		   G_TYPELIB_ERROR_INVALID_BLOB,
		   "Wrong blob type");
      return FALSE;
    }

  if (!validate_name (typelib, "callback", typelib->data, blob->name, error))
    return FALSE; 

  push_context (ctx, get_string_nofail (typelib, blob->name));
  
  if (!validate_signature_blob (typelib, blob->signature, error))
    return FALSE;

  pop_context (ctx);
	
  return TRUE;
}

static gboolean
validate_constant_blob (GTypelib     *typelib,
			guint32        offset,
			GError       **error)
{
  guint value_size[] = {
    0, /* VOID */
    4, /* BOOLEAN */
    1, /* INT8 */
    1, /* UINT8 */
    2, /* INT16 */
    2, /* UINT16 */
    4, /* INT32 */
    4, /* UINT32 */
    8, /* INT64 */
    8, /* UINT64 */
    sizeof (gshort),
    sizeof (gushort),
    sizeof (gint),
    sizeof (guint),
    sizeof (glong),
    sizeof (gulong),
    sizeof (gssize),
    sizeof (gsize),
    sizeof (gfloat),
    sizeof (gdouble),
    sizeof (time_t),
    0, /* GTYPE */
    0, /* UTF8 */
    0, /* FILENAME */
    0, /* ARRAY */
    0, /* INTERFACE */
    0, /* GLIST */
    0, /* GSLIST */
    0, /* GHASH */
    0, /* ERROR */
  };
  ConstantBlob *blob;
  SimpleTypeBlob *type;

  if (typelib->len < offset + sizeof (ConstantBlob))
    {
      g_set_error (error,
		   G_TYPELIB_ERROR,
		   G_TYPELIB_ERROR_INVALID,
		   "The buffer is too short");
      return FALSE;
    }

  blob = (ConstantBlob*) &typelib->data[offset];

  if (blob->blob_type != BLOB_TYPE_CONSTANT)
    {
      g_set_error (error,
		   G_TYPELIB_ERROR,
		   G_TYPELIB_ERROR_INVALID_BLOB,
		   "Wrong blob type");
      return FALSE;
    }

  if (!validate_name (typelib, "constant", typelib->data, blob->name, error))
    return FALSE; 
  
  if (!validate_type_blob (typelib, offset + G_STRUCT_OFFSET (ConstantBlob, type), 
			   0, FALSE, error))
    return FALSE;

  if (!is_aligned (blob->offset))
    {
      g_set_error (error,
		   G_TYPELIB_ERROR,
		   G_TYPELIB_ERROR_INVALID_BLOB,
		   "Misaligned constant value");
      return FALSE;
    }
  
  type = (SimpleTypeBlob *)&typelib->data[offset + G_STRUCT_OFFSET (ConstantBlob, type)];
  if (type->flags.reserved == 0 && type->flags.reserved2 == 0)
    {
      if (type->flags.tag == 0)
	{
	  g_set_error (error,
		       G_TYPELIB_ERROR,
		       G_TYPELIB_ERROR_INVALID_BLOB,
		       "Constant value type void");
	  return FALSE;
	}

      if (value_size[type->flags.tag] != 0 &&
	  blob->size != value_size[type->flags.tag])
	{
	  g_set_error (error,
		       G_TYPELIB_ERROR,
		       G_TYPELIB_ERROR_INVALID_BLOB,
		       "Constant value size mismatch");
	  return FALSE;
	}
      /* FIXME check string values */
    }

  return TRUE;
}

static gboolean
validate_value_blob (GTypelib     *typelib,
		     guint32        offset,
		     GError       **error)
{
  ValueBlob *blob;

  if (typelib->len < offset + sizeof (ValueBlob))
    {
      g_set_error (error,
		   G_TYPELIB_ERROR,
		   G_TYPELIB_ERROR_INVALID,
		   "The buffer is too short");
      return FALSE;
    }

  blob = (ValueBlob*) &typelib->data[offset];

  if (!validate_name (typelib, "value", typelib->data, blob->name, error))
    return FALSE; 
  
  return TRUE;
}

static gboolean
validate_field_blob (GTypelib     *typelib,
		     guint32        offset,
		     GError       **error)
{
  FieldBlob *blob;

  if (typelib->len < offset + sizeof (FieldBlob))
    {
      g_set_error (error,
		   G_TYPELIB_ERROR,
		   G_TYPELIB_ERROR_INVALID,
		   "The buffer is too short");
      return FALSE;
    }

  blob = (FieldBlob*) &typelib->data[offset];
  
  if (!validate_name (typelib, "field", typelib->data, blob->name, error))
    return FALSE; 
  
  if (!validate_type_blob (typelib,
			   offset + G_STRUCT_OFFSET (FieldBlob, type), 
			   0, FALSE, error))
    return FALSE;

  return TRUE;
}

static gboolean
validate_property_blob (GTypelib     *typelib,
			guint32        offset,
			GError       **error)
{
  PropertyBlob *blob;

  if (typelib->len < offset + sizeof (PropertyBlob))
    {
      g_set_error (error,
		   G_TYPELIB_ERROR,
		   G_TYPELIB_ERROR_INVALID,
		   "The buffer is too short");
      return FALSE;
    }

  blob = (PropertyBlob*) &typelib->data[offset];
  
  if (!validate_name (typelib, "property", typelib->data, blob->name, error))
    return FALSE; 
    
  if (!validate_type_blob (typelib,
			   offset + G_STRUCT_OFFSET (PropertyBlob, type), 
			   0, FALSE, error))
    return FALSE;

  return TRUE;
}

static gboolean
validate_signal_blob (GTypelib     *typelib,
		      guint32        offset,
		      guint32        container_offset,
		      GError       **error)
{
  SignalBlob *blob;
  gint n_signals;

  if (typelib->len < offset + sizeof (SignalBlob))
    {
      g_set_error (error,
		   G_TYPELIB_ERROR,
		   G_TYPELIB_ERROR_INVALID,
		   "The buffer is too short");
      return FALSE;
    }

  blob = (SignalBlob*) &typelib->data[offset];

  if (!validate_name (typelib, "signal", typelib->data, blob->name, error))
    return FALSE; 
  
  if ((blob->run_first != 0) + 
      (blob->run_last != 0) + 
      (blob->run_cleanup != 0) != 1)
    {
      g_set_error (error,
		   G_TYPELIB_ERROR,
		   G_TYPELIB_ERROR_INVALID_BLOB,
		   "Invalid signal run flags");
      return FALSE; 
    }

  if (blob->has_class_closure)
    {
      if (((CommonBlob*)&typelib->data[container_offset])->blob_type == BLOB_TYPE_OBJECT)
	{
	  ObjectBlob *object;

	  object = (ObjectBlob*)&typelib->data[container_offset];
	  
	  n_signals = object->n_signals;
	}
      else
	{
	  InterfaceBlob *iface;
	  
	  iface = (InterfaceBlob*)&typelib->data[container_offset];
	  
	  n_signals = iface->n_signals;
	}

      if (blob->class_closure >= n_signals)
	{
	  g_set_error (error,
		       G_TYPELIB_ERROR,
		       G_TYPELIB_ERROR_INVALID_BLOB,
		       "Invalid class closure index");
	  return FALSE; 
	}
    }

  if (!validate_signature_blob (typelib, blob->signature, error))
    return FALSE;
  
  return TRUE;
}

static gboolean
validate_vfunc_blob (GTypelib     *typelib,
		     guint32        offset,
		     guint32        container_offset,
		     GError       **error)
{
  VFuncBlob *blob;
  gint n_vfuncs;

  if (typelib->len < offset + sizeof (VFuncBlob))
    {
      g_set_error (error,
		   G_TYPELIB_ERROR,
		   G_TYPELIB_ERROR_INVALID,
		   "The buffer is too short");
      return FALSE;
    }

  blob = (VFuncBlob*) &typelib->data[offset];

  if (!validate_name (typelib, "vfunc", typelib->data, blob->name, error))
    return FALSE; 
  
  if (blob->class_closure)
    {
      if (((CommonBlob*)&typelib->data[container_offset])->blob_type == BLOB_TYPE_OBJECT)
	{
	  ObjectBlob *object;

	  object = (ObjectBlob*)&typelib->data[container_offset];
	  
	  n_vfuncs = object->n_vfuncs;
	}
      else
	{
	  InterfaceBlob *iface;
	  
	  iface = (InterfaceBlob*)&typelib->data[container_offset];
	  
	  n_vfuncs = iface->n_vfuncs;
	}

      if (blob->class_closure >= n_vfuncs)
	{
	  g_set_error (error,
		       G_TYPELIB_ERROR,
		       G_TYPELIB_ERROR_INVALID_BLOB,
		       "Invalid class closure index");
	  return FALSE; 
	}
    }

  if (!validate_signature_blob (typelib, blob->signature, error))
    return FALSE;
  
  return TRUE;
}

static gboolean
validate_struct_blob (ValidateContext *ctx,
		      guint32        offset,
		      guint16        blob_type,
		      GError       **error)
{
  GTypelib *typelib = ctx->typelib;
  StructBlob *blob;
  gint i;

  if (typelib->len < offset + sizeof (StructBlob))
    {
      g_set_error (error,
		   G_TYPELIB_ERROR,
		   G_TYPELIB_ERROR_INVALID,
		   "The buffer is too short");
      return FALSE;
    }

  blob = (StructBlob*) &typelib->data[offset];

  if (blob->blob_type != blob_type)
    {
      g_set_error (error,
		   G_TYPELIB_ERROR,
		   G_TYPELIB_ERROR_INVALID_BLOB,
		   "Wrong blob type");
      return FALSE;
    }

  if (!validate_name (typelib, "struct", typelib->data, blob->name, error))
    return FALSE; 

  push_context (ctx, get_string_nofail (typelib, blob->name));
  
  if (!blob->unregistered)
    {
      if (!validate_name (typelib, "boxed", typelib->data, blob->gtype_name, error))
	return FALSE; 

      if (!validate_name (typelib, "boxed", typelib->data, blob->gtype_init, error))
	return FALSE; 
    }
  else
    {
      if (blob->gtype_name || blob->gtype_init)
	{
	  g_set_error (error,
		       G_TYPELIB_ERROR,
		       G_TYPELIB_ERROR_INVALID_BLOB,
		       "Gtype data in struct");
	  return FALSE; 
	}
    }

  if (typelib->len < offset + sizeof (StructBlob) + 
            blob->n_fields * sizeof (FieldBlob) +
            blob->n_methods * sizeof (FunctionBlob))
    {
      g_set_error (error,
		   G_TYPELIB_ERROR,
		   G_TYPELIB_ERROR_INVALID,
		   "The buffer is too short");
      return FALSE;
    }

  for (i = 0; i < blob->n_fields; i++)
    {
      if (!validate_field_blob (typelib, 
				offset + sizeof (StructBlob) + 
				i * sizeof (FieldBlob), 
				error))
	return FALSE;
    }

  for (i = 0; i < blob->n_methods; i++)
    {
      if (!validate_function_blob (ctx, 
				   offset + sizeof (StructBlob) + 
				   blob->n_fields * sizeof (FieldBlob) + 
				   i * sizeof (FunctionBlob), 
				   blob_type,
				   error))
	return FALSE;
    }

  pop_context (ctx);

  return TRUE;
}

static gboolean
validate_enum_blob (ValidateContext *ctx,
		    guint32        offset,
		    guint16        blob_type,
		    GError       **error)
{
  GTypelib *typelib = ctx->typelib;
  EnumBlob *blob;
  gint i;

  if (typelib->len < offset + sizeof (EnumBlob))
    {
      g_set_error (error,
		   G_TYPELIB_ERROR,
		   G_TYPELIB_ERROR_INVALID,
		   "The buffer is too short");
      return FALSE;
    }

  blob = (EnumBlob*) &typelib->data[offset];

  if (blob->blob_type != blob_type)
    {
      g_set_error (error,
		   G_TYPELIB_ERROR,
		   G_TYPELIB_ERROR_INVALID_BLOB,
		   "Wrong blob type");
      return FALSE;
    }
  
  if (!blob->unregistered)
    {
      if (!validate_name (typelib, "enum", typelib->data, blob->gtype_name, error))
	return FALSE; 

      if (!validate_name (typelib, "enum", typelib->data, blob->gtype_init, error))
	return FALSE; 
    }
  else
    {
      if (blob->gtype_name || blob->gtype_init)
	{
	  g_set_error (error,
		       G_TYPELIB_ERROR,
		       G_TYPELIB_ERROR_INVALID_BLOB,
		       "Gtype data in unregistered enum");
	  return FALSE; 
	}
    }

  if (!validate_name (typelib, "enum", typelib->data, blob->name, error))
    return FALSE; 
  
  if (typelib->len < offset + sizeof (EnumBlob) + 
      blob->n_values * sizeof (ValueBlob))
    {
      g_set_error (error,
		   G_TYPELIB_ERROR,
		   G_TYPELIB_ERROR_INVALID,
		   "The buffer is too short");
      return FALSE;
    }

  push_context (ctx, get_string_nofail (typelib, blob->name));
  
  for (i = 0; i < blob->n_values; i++)
    {
      if (!validate_value_blob (typelib, 
				offset + sizeof (EnumBlob) + 
				i * sizeof (ValueBlob), 
				error))
	return FALSE;

#if 0
      v1 = (ValueBlob *)&typelib->data[offset + sizeof (EnumBlob) + 
                                        i * sizeof (ValueBlob)];
      for (j = 0; j < i; j++) 
	{
	  v2 = (ValueBlob *)&typelib->data[offset + sizeof (EnumBlob) + 
                                            j * sizeof (ValueBlob)];

	  if (v1->value == v2->value)
	    {

	      /* FIXME should this be an error ? */
	      g_set_error (error,
			   G_TYPELIB_ERROR,
			   G_TYPELIB_ERROR_INVALID_BLOB,
			   "Duplicate enum value");
	      return FALSE;
	    }
	}
#endif      
    }

  pop_context (ctx);
  
  return TRUE;
}

static gboolean
validate_object_blob (ValidateContext *ctx,
		      guint32        offset,
		      GError       **error)
{
  GTypelib *typelib = ctx->typelib;
  Header *header;
  ObjectBlob *blob;
  gint i;
  guint32 offset2;

  header = (Header *)typelib->data;

  if (typelib->len < offset + sizeof (ObjectBlob))
    {
      g_set_error (error,
		   G_TYPELIB_ERROR,
		   G_TYPELIB_ERROR_INVALID,
		   "The buffer is too short");
      return FALSE;
    }

  blob = (ObjectBlob*) &typelib->data[offset];

  if (blob->blob_type != BLOB_TYPE_OBJECT)
    {
      g_set_error (error,
		   G_TYPELIB_ERROR,
		   G_TYPELIB_ERROR_INVALID_BLOB,
		   "Wrong blob type");
      return FALSE;
    }
  
  if (!validate_name (typelib, "object", typelib->data, blob->gtype_name, error))
    return FALSE; 
  
  if (!validate_name (typelib, "object", typelib->data, blob->gtype_init, error))
    return FALSE; 
  
  if (!validate_name (typelib, "object", typelib->data, blob->name, error))
    return FALSE; 

  if (blob->parent > header->n_entries)
    {
      g_set_error (error,
		   G_TYPELIB_ERROR,
		   G_TYPELIB_ERROR_INVALID_BLOB,
		   "Invalid parent index");
      return FALSE; 
    }

  if (blob->parent != 0)
    {
      DirEntry *entry;

      entry = get_dir_entry_checked (typelib, blob->parent, error);
      if (!entry)
        return FALSE;
      if (entry->blob_type != BLOB_TYPE_OBJECT &&
	  (entry->local || entry->blob_type != 0))
	{
	  g_set_error (error,
		       G_TYPELIB_ERROR,
		       G_TYPELIB_ERROR_INVALID_BLOB,
		       "Parent not object");
	  return FALSE; 
	}
    }
  
  if (blob->gtype_struct != 0)
    {
      DirEntry *entry;

      entry = get_dir_entry_checked (typelib, blob->gtype_struct, error);
      if (!entry)
        return FALSE;
      if (entry->blob_type != BLOB_TYPE_STRUCT && entry->local)
        {
          g_set_error (error,
                       G_TYPELIB_ERROR,
                       G_TYPELIB_ERROR_INVALID_BLOB,
                       "Class struct invalid type or not local");
          return FALSE; 
        }
    }  
  
  if (typelib->len < offset + sizeof (ObjectBlob) + 
            (blob->n_interfaces + blob->n_interfaces % 2) * 2 +
            blob->n_fields * sizeof (FieldBlob) +
            blob->n_properties * sizeof (PropertyBlob) +
            blob->n_methods * sizeof (FunctionBlob) +
            blob->n_signals * sizeof (SignalBlob) +
            blob->n_vfuncs * sizeof (VFuncBlob) +
            blob->n_constants * sizeof (ConstantBlob))
     
    {
      g_set_error (error,
		   G_TYPELIB_ERROR,
		   G_TYPELIB_ERROR_INVALID,
		   "The buffer is too short");
      return FALSE;
    }

  offset2 = offset + sizeof (ObjectBlob);

  for (i = 0; i < blob->n_interfaces; i++, offset2 += 2)
    {
      guint16 iface;
      DirEntry *entry;

      iface = *(guint16*)&typelib->data[offset2];
      if (iface == 0 || iface > header->n_entries)
	{
	  g_set_error (error,
		       G_TYPELIB_ERROR,
		       G_TYPELIB_ERROR_INVALID_BLOB,
		       "Invalid interface index");
	  return FALSE; 
	}
      
      entry = get_dir_entry_checked (typelib, iface, error);
      if (!entry)
        return FALSE;

      if (entry->blob_type != BLOB_TYPE_INTERFACE &&
	  (entry->local || entry->blob_type != 0))
	{
	  g_set_error (error,
		       G_TYPELIB_ERROR,
		       G_TYPELIB_ERROR_INVALID_BLOB,
		       "Not an interface");
	  return FALSE; 
	}
    }

  offset2 += 2 * (blob->n_interfaces %2);

  push_context (ctx, get_string_nofail (typelib, blob->name));
  
  for (i = 0; i < blob->n_fields; i++, offset2 += sizeof (FieldBlob))
    {
      if (!validate_field_blob (typelib, offset2, error))
	return FALSE;
    }

  for (i = 0; i < blob->n_properties; i++, offset2 += sizeof (PropertyBlob))
    {
      if (!validate_property_blob (typelib, offset2, error))
	return FALSE;
    }

  for (i = 0; i < blob->n_methods; i++, offset2 += sizeof (FunctionBlob))
    {
      if (!validate_function_blob (ctx, offset2, BLOB_TYPE_OBJECT, error))
	return FALSE;
    }

  for (i = 0; i < blob->n_signals; i++, offset2 += sizeof (SignalBlob))
    {
      if (!validate_signal_blob (typelib, offset2, offset, error))
	return FALSE;
    }

  for (i = 0; i < blob->n_vfuncs; i++, offset2 += sizeof (VFuncBlob))
    {
      if (!validate_vfunc_blob (typelib, offset2, offset, error))
	return FALSE;
    }

  for (i = 0; i < blob->n_constants; i++, offset2 += sizeof (ConstantBlob))
    {
      if (!validate_constant_blob (typelib, offset2, error))
	return FALSE;
    }

  pop_context (ctx);

  return TRUE;
}

static gboolean
validate_interface_blob (ValidateContext *ctx,
			 guint32        offset,
			 GError       **error)
{
  GTypelib *typelib = ctx->typelib;
  Header *header;
  InterfaceBlob *blob;
  gint i;
  guint32 offset2;
  
  header = (Header *)typelib->data;

  if (typelib->len < offset + sizeof (InterfaceBlob))
    {
      g_set_error (error,
		   G_TYPELIB_ERROR,
		   G_TYPELIB_ERROR_INVALID,
		   "The buffer is too short");
      return FALSE;
    }

  blob = (InterfaceBlob*) &typelib->data[offset];

  if (blob->blob_type != BLOB_TYPE_INTERFACE)
    {
      g_set_error (error,
		   G_TYPELIB_ERROR,
		   G_TYPELIB_ERROR_INVALID_BLOB,
		   "Wrong blob type; expected interface, got %d", blob->blob_type);
      return FALSE;
    }
  
  if (!validate_name (typelib, "interface", typelib->data, blob->gtype_name, error))
    return FALSE; 
  
  if (!validate_name (typelib, "interface", typelib->data, blob->gtype_init, error))
    return FALSE; 
  
  if (!validate_name (typelib, "interface", typelib->data, blob->name, error))
    return FALSE; 
  
  if (typelib->len < offset + sizeof (InterfaceBlob) + 
            (blob->n_prerequisites + blob->n_prerequisites % 2) * 2 +
            blob->n_properties * sizeof (PropertyBlob) +
            blob->n_methods * sizeof (FunctionBlob) +
            blob->n_signals * sizeof (SignalBlob) +
            blob->n_vfuncs * sizeof (VFuncBlob) +
            blob->n_constants * sizeof (ConstantBlob))
     
    {
      g_set_error (error,
		   G_TYPELIB_ERROR,
		   G_TYPELIB_ERROR_INVALID,
		   "The buffer is too short");
      return FALSE;
    }

  offset2 = offset + sizeof (InterfaceBlob);

  for (i = 0; i < blob->n_prerequisites; i++, offset2 += 2)
    {
      DirEntry *entry;
      guint16 req;

      req = *(guint16*)&typelib->data[offset2];
      if (req == 0 || req > header->n_entries)
	{
	  g_set_error (error,
		       G_TYPELIB_ERROR,
		       G_TYPELIB_ERROR_INVALID_BLOB,
		       "Invalid prerequisite index");
	  return FALSE; 
	}

      entry = g_typelib_get_dir_entry (typelib, req);
      if (entry->blob_type != BLOB_TYPE_INTERFACE &&
	  entry->blob_type != BLOB_TYPE_OBJECT &&
	  (entry->local || entry->blob_type != 0))
	{
	  g_set_error (error,
		       G_TYPELIB_ERROR,
		       G_TYPELIB_ERROR_INVALID_BLOB,
		       "Not an interface or object");
	  return FALSE; 
	}
    }

  offset2 += 2 * (blob->n_prerequisites % 2);

  push_context (ctx, get_string_nofail (typelib, blob->name));
  
  for (i = 0; i < blob->n_properties; i++, offset2 += sizeof (PropertyBlob))
    {
      if (!validate_property_blob (typelib, offset2, error))
	return FALSE;
    }

  for (i = 0; i < blob->n_methods; i++, offset2 += sizeof (FunctionBlob))
    {
      if (!validate_function_blob (ctx, offset2, BLOB_TYPE_INTERFACE, error))
	return FALSE;
    }
  
  for (i = 0; i < blob->n_signals; i++, offset2 += sizeof (SignalBlob))
    {
      if (!validate_signal_blob (typelib, offset2, offset, error))
	return FALSE;
    }
  
  for (i = 0; i < blob->n_vfuncs; i++, offset2 += sizeof (VFuncBlob))
    {
      if (!validate_vfunc_blob (typelib, offset2, offset, error))
	return FALSE;
    }

  for (i = 0; i < blob->n_constants; i++, offset2 += sizeof (ConstantBlob))
    {
      if (!validate_constant_blob (typelib, offset2, error))
	return FALSE;
    }

  pop_context (ctx);

  return TRUE;
}

static gboolean
validate_errordomain_blob (GTypelib     *typelib,
			   guint32        offset,
			   GError       **error)
{
  return TRUE;
}

static gboolean
validate_union_blob (GTypelib     *typelib,
		     guint32        offset,
		     GError       **error)
{
  return TRUE;
}

static gboolean
validate_blob (ValidateContext *ctx,
	       guint32          offset,
	       GError         **error)
{
  GTypelib *typelib = ctx->typelib;
  CommonBlob *common;

  if (typelib->len < offset + sizeof (CommonBlob))
    {
      g_set_error (error,
		   G_TYPELIB_ERROR,
		   G_TYPELIB_ERROR_INVALID,
		   "The buffer is too short");
      return FALSE;
    }

  common = (CommonBlob*)&typelib->data[offset];
  
  switch (common->blob_type)
    {
    case BLOB_TYPE_FUNCTION:
      if (!validate_function_blob (ctx, offset, 0, error))
	return FALSE;
      break;
    case BLOB_TYPE_CALLBACK:
      if (!validate_callback_blob (ctx, offset, error))
	return FALSE;
      break;
    case BLOB_TYPE_STRUCT:
    case BLOB_TYPE_BOXED:
      if (!validate_struct_blob (ctx, offset, common->blob_type, error))
	return FALSE;
      break;
    case BLOB_TYPE_ENUM:
    case BLOB_TYPE_FLAGS:
      if (!validate_enum_blob (ctx, offset, common->blob_type, error))
	return FALSE;
      break;
    case BLOB_TYPE_OBJECT:
      if (!validate_object_blob (ctx, offset, error))
	return FALSE;
      break;
    case BLOB_TYPE_INTERFACE:
      if (!validate_interface_blob (ctx, offset, error))
	return FALSE;
      break;
    case BLOB_TYPE_CONSTANT:
      if (!validate_constant_blob (typelib, offset, error))
	return FALSE;
      break;
    case BLOB_TYPE_ERROR_DOMAIN:
      if (!validate_errordomain_blob (typelib, offset, error))
	return FALSE;
      break;
    case BLOB_TYPE_UNION:
      if (!validate_union_blob (typelib, offset, error))
	return FALSE;
      break;
    default:
      g_set_error (error, 
		   G_TYPELIB_ERROR,
		   G_TYPELIB_ERROR_INVALID_ENTRY,
		   "Invalid blob type");
      return FALSE;
    }

  return TRUE;
}

static gboolean 
validate_directory (ValidateContext   *ctx,
		    GError            **error)
{
  GTypelib *typelib = ctx->typelib;
  Header *header = (Header *)typelib->data;
  DirEntry *entry;
  gint i;
  
  if (typelib->len < header->directory + header->n_entries * sizeof (DirEntry))
    {
      g_set_error (error,
		   G_TYPELIB_ERROR,
		   G_TYPELIB_ERROR_INVALID,
		   "The buffer is too short");
      return FALSE;
    }

  for (i = 0; i < header->n_entries; i++)
    {
      entry = g_typelib_get_dir_entry (typelib, i + 1);

      if (!validate_name (typelib, "entry", typelib->data, entry->name, error))
	return FALSE; 
      
      if ((entry->local && entry->blob_type == BLOB_TYPE_INVALID) ||
	  entry->blob_type > BLOB_TYPE_UNION)
	{
	  g_set_error (error,
		       G_TYPELIB_ERROR,
		       G_TYPELIB_ERROR_INVALID_DIRECTORY,
		       "Invalid entry type");
	  return FALSE; 
	}

      if (i < header->n_local_entries)
	{
	  if (!entry->local)
	    {
	      g_set_error (error,
			   G_TYPELIB_ERROR,
			   G_TYPELIB_ERROR_INVALID_DIRECTORY,
			   "Too few local directory entries");
	      return FALSE;
	    }

	  if (!is_aligned (entry->offset))
	    {
	      g_set_error (error,
			   G_TYPELIB_ERROR,
			   G_TYPELIB_ERROR_INVALID_DIRECTORY,
			   "Misaligned entry");
	      return FALSE;
	    }

	  if (!validate_blob (ctx, entry->offset, error))
	    return FALSE;
	}
      else
	{
	  if (entry->local)
	    {
	      g_set_error (error,
			   G_TYPELIB_ERROR,
			   G_TYPELIB_ERROR_INVALID_DIRECTORY,
			   "Too many local directory entries");
	      return FALSE;
	    }

	  if (!validate_name (typelib, "namespace", typelib->data, entry->offset, error))
	    return FALSE; 
	}
    }

  return TRUE;
}

static gboolean
validate_attributes (ValidateContext *ctx,
		     GError       **error)
{
  GTypelib *typelib = ctx->typelib;
  Header *header = (Header *)typelib->data;

  if (header->size < header->attributes + header->n_attributes * sizeof (AttributeBlob))
    {
      g_set_error (error,
		   G_TYPELIB_ERROR,
		   G_TYPELIB_ERROR_INVALID,
		   "The buffer is too short");
      return FALSE;      
    }
  
  return TRUE;
}

static void
prefix_with_context (GError **error,
		     const char *section,
		     ValidateContext *ctx)
{
  GString *str = g_string_new (NULL);
  GSList *link;
  char *buf;
  
  link = ctx->context_stack;
  if (!link)
    {
      g_prefix_error (error, "In %s:", section);
      return;
    }

  for (; link; link = link->next)
    {
      g_string_append (str, link->data);
      if (link->next)
	g_string_append_c (str, '/');
    }
  g_string_append_c (str, ')');
  buf = g_string_free (str, FALSE);
  g_prefix_error (error, "In %s (Context: %s): ", section, buf);
  g_free (buf);
}

gboolean 
g_typelib_validate (GTypelib     *typelib,
		     GError       **error)
{
  ValidateContext ctx;
  ctx.typelib = typelib;
  ctx.context_stack = NULL;

  if (!validate_header (&ctx, error))
    {
      prefix_with_context (error, "In header", &ctx);
      return FALSE;
    }

  if (!validate_directory (&ctx, error))
    {
      prefix_with_context (error, "directory", &ctx);
      return FALSE;
    }

  if (!validate_attributes (&ctx, error))
    {
      prefix_with_context (error, "attributes", &ctx);
      return FALSE;
    }

  return TRUE;
}

GQuark
g_typelib_error_quark (void)
{
  static GQuark quark = 0;
  if (quark == 0)
    quark = g_quark_from_static_string ("g-typelib-error-quark");
  return quark;
}

static void
_g_typelib_do_dlopen (GTypelib *typelib)
{
  Header *header;
  const char *shlib_str;
  
  header = (Header *) typelib->data;
  /* note that NULL shlib means to open the main app, which is allowed */
  if (header->shared_library)
    shlib_str = g_typelib_get_string (typelib, header->shared_library);
  else
    shlib_str = NULL;

  if (shlib_str != NULL && shlib_str[0] != '\0')
    {
      gchar **shlibs;
      gint i;

      /* shared-library is a comma-separated list of libraries */
      shlibs = g_strsplit (shlib_str, ",", 0);

       /* We load all passed libs unconditionally as if the same library is loaded
        * again with dlopen(), the same file handle will be returned. See bug:
        * http://bugzilla.gnome.org/show_bug.cgi?id=555294
        */
      for (i = 0; shlibs[i]; i++)
        {
          GModule *module;

          /* Glade's autoconnect feature and OpenGL's extension mechanism
           * as used by Clutter rely on dlopen(NULL) to work as a means of
           * accessing the app's symbols. This keeps us from using
           * G_MODULE_BIND_LOCAL. BIND_LOCAL may have other issues as well;
           * in general libraries are not expecting multiple copies of
           * themselves and are not expecting to be unloaded. So we just
           * load modules globally for now.
           */

          module = g_module_open (shlibs[i], G_MODULE_BIND_LAZY);

          if (module == NULL)
            {
              GString *shlib_full = g_string_new (shlibs[i]);

              /* Prefix with "lib", try both .la and .so */
              if (!g_str_has_prefix (shlib_full->str, "lib"))
                g_string_prepend (shlib_full, "lib");
              g_string_append (shlib_full, ".la");
              module = g_module_open (shlib_full->str, G_MODULE_BIND_LAZY);
              if (module == NULL)
                {
                  g_string_overwrite (shlib_full, strlen (shlib_full->str)-2, SHLIB_SUFFIX);
                  module = g_module_open (shlib_full->str, G_MODULE_BIND_LAZY);
                }

              g_string_free (shlib_full, TRUE);
            }

          if (module == NULL)
            {
              g_warning ("Failed to load shared library '%s' referenced by the typelib: %s",
                         shlibs[i], g_module_error ());
            }
          else
            {
              typelib->modules = g_list_append (typelib->modules, module);
            }
       }

      g_strfreev (shlibs);
    }
  else
    {
      /* If there's no shared-library entry for this module, assume that
       * the module is for the application.  Some of the hand-written .gir files
       * in gobject-introspection don't have shared-library entries, but no one
       * is really going to be calling g_module_symbol on them either.
       */
      GModule *module = g_module_open (NULL, 0);
      if (module == NULL)
        g_warning ("gtypelib.c: Failed to g_module_open (NULL): %s", g_module_error ());
      else
        typelib->modules = g_list_prepend (typelib->modules, module);
    }
}

static inline void
_g_typelib_ensure_open (GTypelib *typelib)
{
  if (typelib->open_attempted)
    return;
  typelib->open_attempted = TRUE;
  _g_typelib_do_dlopen (typelib);
}

/**
 * g_typelib_new_from_memory:
 * @memory: address of memory chunk containing the typelib
 * @len: length of memory chunk containing the typelib
 * 
 * Creates a new #GTypelib from a memory location.  The memory block
 * pointed to by @typelib will be automatically g_free()d when the
 * repository is destroyed.
 * 
 * Return value: the new #GTypelib
 **/
GTypelib *
g_typelib_new_from_memory (guchar *memory, gsize len)
{
  GTypelib *meta;

  meta = g_new0 (GTypelib, 1);
  meta->data = memory;
  meta->len = len;
  meta->owns_memory = TRUE;
  meta->modules = NULL;

  return meta;
}

/**
 * g_typelib_new_from_const_memory:
 * @memory: address of memory chunk containing the typelib
 * @len: length of memory chunk containing the typelib
 * 
 * Creates a new #GTypelib from a memory location.
 * 
 * Return value: the new #GTypelib
 **/
GTypelib *
g_typelib_new_from_const_memory (const guchar *memory, gsize len)
{
  GTypelib *meta;

  meta = g_new0 (GTypelib, 1);
  meta->data = (guchar *) memory;
  meta->len = len;
  meta->owns_memory = FALSE;
  meta->modules = NULL;

  return meta;
}

/**
 * g_typelib_new_from_mapped_file:
 * @mfile: a #GMappedFile, that will be free'd when the repository is destroyed
 * 
 * Creates a new #GTypelib from a #GMappedFile.
 * 
 * Return value: the new #GTypelib
 **/
GTypelib *
g_typelib_new_from_mapped_file (GMappedFile *mfile)
{
  GTypelib *meta;

  meta = g_new0 (GTypelib, 1);
  meta->mfile = mfile;
  meta->owns_memory = FALSE;
  meta->data = (guchar *) g_mapped_file_get_contents (mfile);
  meta->len = g_mapped_file_get_length (mfile);

  return meta;
}

/**
 * g_typelib_free:
 * @typelib: a #GTypelib
 * 
 * Free a #GTypelib.
 **/
void
g_typelib_free (GTypelib *typelib)
{
  if (typelib->mfile)
    g_mapped_file_free (typelib->mfile);
  else
    if (typelib->owns_memory)
      g_free (typelib->data);
  if (typelib->modules)
    {
      g_list_foreach (typelib->modules, (GFunc) g_module_close, NULL);
      g_list_free (typelib->modules);
    }
  g_free (typelib);
}

const gchar *
g_typelib_get_namespace (GTypelib *typelib)
{
  return g_typelib_get_string (typelib, ((Header *) typelib->data)->namespace);
}

/**
 * g_typelib_symbol:
 * @typelib: the typelib
 * @symbol_name: name of symbol to be loaded
 * @symbol: returns a pointer to the symbol value
 *
 * Loads a symbol from #GTypelib.
 *
 * Return value: #TRUE on success
 **/
gboolean
g_typelib_symbol (GTypelib *typelib, const char *symbol_name, gpointer *symbol)
{
  GList *l;
  
  _g_typelib_ensure_open (typelib);

  /*
   * The reason for having multiple modules dates from gir-repository
   * when it was desired to inject code (accessors, etc.) into an
   * existing library.  In that situation, the first module listed
   * will be the custom one, which overrides the main one.  A bit
   * inefficient, but the problem will go away when gir-repository
   * does.
   * 
   * For modules with no shared library, we dlopen'd the current
   * process above.  
   */
  for (l = typelib->modules; l; l = l->next)
    {
      GModule *module = l->data;

      if (g_module_symbol (module, symbol_name, symbol))
        return TRUE;
    }

  return FALSE;
}
