/* GObject introspection: A parser for the XML GIR format
 *
 * Copyright (C) 2008 Philip Van Hoof
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
#include <stdio.h>

#include <glib.h>
#include "girmodule.h"
#include "girnode.h"
#include "gtypelib.h"

typedef enum
{
  STATE_START,    
  STATE_END,        
  STATE_REPOSITORY, 
  STATE_NAMESPACE,  
  STATE_ENUM,        
  STATE_BITFIELD,  /* 5 */  
  STATE_FUNCTION,   
  STATE_FUNCTION_RETURN, 
  STATE_FUNCTION_PARAMETERS,
  STATE_FUNCTION_PARAMETER, 
  STATE_CLASS,   /* 10 */
  STATE_CLASS_FIELD,
  STATE_CLASS_PROPERTY,
  STATE_INTERFACE,
  STATE_INTERFACE_PROPERTY, 
  STATE_INTERFACE_FIELD,  /* 15 */
  STATE_IMPLEMENTS, 
  STATE_REQUIRES,
  STATE_BOXED,  
  STATE_BOXED_FIELD,
  STATE_STRUCT,   /* 20 */
  STATE_STRUCT_FIELD,
  STATE_ERRORDOMAIN, 
  STATE_UNION,
  STATE_NAMESPACE_CONSTANT,
  STATE_CLASS_CONSTANT, /* 25 */
  STATE_INTERFACE_CONSTANT,
  STATE_ALIAS
} ParseState;

typedef struct _ParseContext ParseContext;
struct _ParseContext
{
  ParseState state;
  ParseState prev_state;

  GList *modules;
  GHashTable *aliases;

  GIrModule *current_module;
  GIrNode *current_node;
  GIrNode *current_typed;
};

#define MISSING_ATTRIBUTE(ctx,error,element,attribute)			        \
  do {                                                                          \
    int line_number, char_number;                                                \
    g_markup_parse_context_get_position (context, &line_number, &char_number);  \
    g_set_error (error,                                                         \
   	         G_MARKUP_ERROR,                                                \
	         G_MARKUP_ERROR_INVALID_CONTENT,                                \
	         "Line %d, character %d: The attribute '%s' on the element '%s' must be specified",    \
	         line_number, char_number, attribute, element);		\
  } while (0)

static void
backtrace_stderr (void)
{
  void *array[50];
  int size;
  char **strings;
  size_t i;

  size = backtrace (array, 50);
  strings = (char**) backtrace_symbols (array, size);

  fprintf (stderr, "--- BACKTRACE (%zd frames) ---\n", size);

  for (i = 0; i < size; i++)
    fprintf (stderr, "%s\n", strings[i]);

  fprintf (stderr, "--- END BACKTRACE ---\n", size);

  free (strings);
}


static const gchar *
find_attribute (const gchar  *name, 
		const gchar **attribute_names,
		const gchar **attribute_values)
{
  gint i;
  
  for (i = 0; attribute_names[i] != NULL; i++)
    if (strcmp (attribute_names[i], name) == 0)
      return attribute_values[i];
  
  return 0;
}

static void
state_switch (ParseContext *ctx, ParseState newstate)
{
  g_debug ("State: %d", newstate);
  ctx->prev_state = ctx->state;
  ctx->state = newstate;
}

static GIrNodeType * parse_type_internal (gchar *str, gchar **rest);

static GIrNodeType *
create_pointer ()
{
  char *pointer = g_strdup ("any");
  char *pointer_rest;
  GIrNodeType *ret = parse_type_internal (pointer, &pointer_rest);
  g_free (pointer);
  return ret;
}

static GIrNodeType *
parse_type_internal (gchar *str, gchar **rest)
{
  gint i;

  static struct {
    const gchar *str;
    gint tag;
    gboolean pointer;
  } basic[] = {
    { "void",     GI_TYPE_TAG_VOID,    0 },
    { "none",     GI_TYPE_TAG_VOID,    0 },
    { "any",      GI_TYPE_TAG_VOID,    1 },
    { "gpointer", GI_TYPE_TAG_VOID,    1 },
    { "bool",     GI_TYPE_TAG_BOOLEAN, 0 },
    { "gboolean", GI_TYPE_TAG_BOOLEAN, 0 },
    { "char",     GI_TYPE_TAG_INT8,    0 },
    { "gchar",    GI_TYPE_TAG_INT8,    0 },
    { "guchar",   GI_TYPE_TAG_UINT8,   0 },
    { "gunichar", GI_TYPE_TAG_UINT32,  0 },
    { "int8_t",   GI_TYPE_TAG_INT8,    0 },
    { "int8",     GI_TYPE_TAG_INT8,    0 },
    { "gint8",    GI_TYPE_TAG_INT8,    0 },
    { "uint8_t",  GI_TYPE_TAG_UINT8,   0 },
    { "uint8",    GI_TYPE_TAG_UINT8,   0 },
    { "guint8",   GI_TYPE_TAG_UINT8,   0 },
    { "int16_t",  GI_TYPE_TAG_INT16,   0 },
    { "int16",    GI_TYPE_TAG_INT16,   0 },
    { "gint16",   GI_TYPE_TAG_INT16,   0 },
    { "uint16_t", GI_TYPE_TAG_UINT16,  0 },
    { "uint16",   GI_TYPE_TAG_UINT16,  0 },
    { "guint16",  GI_TYPE_TAG_UINT16,  0 },
    { "int32_t",  GI_TYPE_TAG_INT32,   0 },
    { "int32",    GI_TYPE_TAG_INT32,   0 },
    { "gint32",   GI_TYPE_TAG_INT32,   0 },
    { "uint32_t", GI_TYPE_TAG_UINT32,  0 },
    { "uint32",   GI_TYPE_TAG_UINT32,  0 },
    { "guint32",  GI_TYPE_TAG_UINT32,  0 },
    { "int64_t",  GI_TYPE_TAG_INT64,   0 },
    { "int64",    GI_TYPE_TAG_INT64,   0 },
    { "gint64",   GI_TYPE_TAG_INT64,   0 },
    { "uint64_t", GI_TYPE_TAG_UINT64,  0 },
    { "uint64",   GI_TYPE_TAG_UINT64,  0 },
    { "guint64",  GI_TYPE_TAG_UINT64,  0 },
    { "int",      GI_TYPE_TAG_INT,     0 },
    { "gint",     GI_TYPE_TAG_INT,     0 },
    { "uint",     GI_TYPE_TAG_UINT,    0 },
    { "guint",    GI_TYPE_TAG_UINT,    0 },
    { "long",     GI_TYPE_TAG_LONG,    0 },
    { "glong",    GI_TYPE_TAG_LONG,    0 },
    { "ulong",    GI_TYPE_TAG_ULONG,   0 },
    { "gulong",   GI_TYPE_TAG_ULONG,   0 },
    { "ssize_t",  GI_TYPE_TAG_SSIZE,   0 },
    { "gssize",   GI_TYPE_TAG_SSIZE,   0 },
    { "size_t",   GI_TYPE_TAG_SIZE,    0 },
    { "gsize",    GI_TYPE_TAG_SIZE,    0 },
    { "float",    GI_TYPE_TAG_FLOAT,   0 },
    { "gfloat",   GI_TYPE_TAG_FLOAT,   0 },
    { "double",   GI_TYPE_TAG_DOUBLE,  0 },
    { "gdouble",  GI_TYPE_TAG_DOUBLE,  0 },
    { "utf8",     GI_TYPE_TAG_UTF8,    1 },  
    { "gchar*",   GI_TYPE_TAG_UTF8,    1 },  
    { "filename", GI_TYPE_TAG_FILENAME,1 },
    // FIXME merge - do we still want this?
    { "string",   GI_TYPE_TAG_UTF8,  1 }
  };  

  gint n_basic = G_N_ELEMENTS (basic);
  gchar *start, *end;
  
  GIrNodeType *type;
  
  type = (GIrNodeType *)g_ir_node_new (G_IR_NODE_TYPE);
  
  str = g_strstrip (str);

  type->unparsed = g_strdup (str);
 
  *rest = str;
  for (i = 0; i < n_basic; i++)
    {
      if (g_str_has_prefix (*rest, basic[i].str))
	{
	  type->is_basic = TRUE;
	  type->tag = basic[i].tag;
 	  type->is_pointer = basic[i].pointer;

	  *rest += strlen(basic[i].str);
	  *rest = g_strchug (*rest);
	  if (**rest == '*' && !type->is_pointer)
	    {
	      type->is_pointer = TRUE;
	      (*rest)++;
	    }

	  break;
	}
    }

  if (i < n_basic)
    /* found a basic type */;
  else if (g_str_has_prefix (*rest, "GList") ||
	   g_str_has_prefix (*rest, "GSList"))
    {
      if (g_str_has_prefix (*rest, "GList"))
	{
	  type->tag = GI_TYPE_TAG_GLIST;
	  type->is_glist = TRUE;
	  type->is_pointer = TRUE;
	  *rest += strlen ("GList");
	}
      else
	{
	  type->tag = GI_TYPE_TAG_GSLIST;
	  type->is_gslist = TRUE;
	  type->is_pointer = TRUE;
	  *rest += strlen ("GSList");
	}
      
      *rest = g_strchug (*rest);
      
      if (**rest == '<')
	{
	  (*rest)++;
	  
	  type->parameter_type1 = parse_type_internal (*rest, rest);
	  if (type->parameter_type1 == NULL)
	    goto error;
	  
	  *rest = g_strchug (*rest);
	  
	  if ((*rest)[0] != '>')
	    goto error;
	  (*rest)++;
	}
      else
	{
	  type->parameter_type1 = create_pointer ();
	}
    }
  else if (g_str_has_prefix (*rest, "GHashTable"))
    {
      type->tag = GI_TYPE_TAG_GHASH;
      type->is_ghashtable = TRUE;
      type->is_pointer = TRUE;
      *rest += strlen ("GHashTable");
      
      *rest = g_strchug (*rest);
      
      if (**rest == '<')
	{
	  (*rest)++;
      
	  type->parameter_type1 = parse_type_internal (*rest, rest);
	  if (type->parameter_type1 == NULL)
	    goto error;
      
	  *rest = g_strchug (*rest);
	  
	  if ((*rest)[0] != ',')
	    goto error;
	  (*rest)++;
	  
	  type->parameter_type2 = parse_type_internal (*rest, rest);
	  if (type->parameter_type2 == NULL)
	    goto error;
      
	  if ((*rest)[0] != '>')
	    goto error;
	  (*rest)++;
	}
      else
	{
	  type->parameter_type1 = create_pointer ();
	  type->parameter_type2 = create_pointer ();
	}

    }
  else if (g_str_has_prefix (*rest, "GError"))
    {
      type->tag = GI_TYPE_TAG_ERROR;
      type->is_error = TRUE;
      type->is_pointer = TRUE;
      *rest += strlen ("GError");
      
      *rest = g_strchug (*rest);
      
      if (**rest == '<')
	{
	  (*rest)++;
	  
	  end = strchr (*rest, '>');
	  str = g_strndup (*rest, end - *rest);
	  type->errors = g_strsplit (str, ",", 0);
	  g_free (str);
	  
	  *rest = end + 1;
	}
    }
  else 
    {
      type->tag = GI_TYPE_TAG_INTERFACE;
      type->is_interface = TRUE; 
      start = *rest;

      /* must be an interface type */
      while (g_ascii_isalnum (**rest) || 
	     **rest == '.' || 
	     **rest == '-' || 
	     **rest == '_' ||
	     **rest == ':')
	(*rest)++;

      type->interface = g_strndup (start, *rest - start);

      *rest = g_strchug (*rest);
      if (**rest == '*')
	{
	  type->is_pointer = TRUE;
	  (*rest)++;
	}
    }
  
  *rest = g_strchug (*rest);
  if (g_str_has_prefix (*rest, "["))
    {
      GIrNodeType *array;

      array = (GIrNodeType *)g_ir_node_new (G_IR_NODE_TYPE);

      array->tag = GI_TYPE_TAG_ARRAY;
      array->is_pointer = TRUE;
      array->is_array = TRUE;
      
      array->parameter_type1 = type;

      array->zero_terminated = FALSE;
      array->has_length = FALSE;
      array->length = 0;

      if (!g_str_has_prefix (*rest, "[]"))
	{
	  gchar *end, *str, **opts;
	  
	  end = strchr (*rest, ']');
	  str = g_strndup (*rest + 1, (end - *rest) - 1); 
	  opts = g_strsplit (str, ",", 0);
	  
	  *rest = end + 1;

	  for (i = 0; opts[i]; i++)
	    {
	      gchar **vals;
	      
	      vals = g_strsplit (opts[i], "=", 0);

	      if (strcmp (vals[0], "zero-terminated") == 0)
		array->zero_terminated = (strcmp (vals[1], "1") == 0);
	      else if (strcmp (vals[0], "length") == 0)
		{
		  array->has_length = TRUE;
		  array->length = atoi (vals[1]);
		}

	      g_strfreev (vals);
	    }

	  g_free (str);
	  g_strfreev (opts);
	}
	      
      type = array;
    }

  g_assert (type->tag >= 0 && type->tag <= GI_TYPE_TAG_ERROR);
  return type;

 error:
  g_ir_node_free ((GIrNode *)type);
  
  return NULL;
}

static const char *
resolve_aliases (ParseContext *ctx, const gchar *type)
{
  gpointer orig;
  gpointer value;

  while (g_hash_table_lookup_extended (ctx->aliases, type, &orig, &value))
    {
      g_debug ("Resolved: %s => %s", type, value);
      type = value;
    }
  return type;
}

static GIrNodeType *
parse_type (ParseContext *ctx, const gchar *type)
{
  gchar *str;
  gchar *rest;
  GIrNodeType *node;
  
  type = resolve_aliases (ctx, type);
  str = g_strdup (type);
  node = parse_type_internal (str, &rest);
  g_free (str);

  return node;
}

static gboolean
start_glib_boxed (GMarkupParseContext *context,
		  const gchar         *element_name,
		  const gchar        **attribute_names,
		  const gchar        **attribute_values,
		  ParseContext        *ctx,
		  GError             **error)
{
  if (strcmp (element_name, "glib:boxed") == 0 && 
      ctx->state == STATE_NAMESPACE)
    {
      const gchar *name;
      const gchar *typename;
      const gchar *typeinit;
      const gchar *deprecated;
      
      name = find_attribute ("glib:name", attribute_names, attribute_values);
      typename = find_attribute ("glib:type-name", attribute_names, attribute_values);
      typeinit = find_attribute ("glib:get-type", attribute_names, attribute_values);
      deprecated = find_attribute ("deprecated", attribute_names, attribute_values);
      
      if (name == NULL)
	MISSING_ATTRIBUTE (context, error, element_name, "glib:name");
      else if (typename == NULL)
	MISSING_ATTRIBUTE (context, error, element_name, "glib:type-name");
      else if (typeinit == NULL)
	MISSING_ATTRIBUTE (context, error, element_name, "glib:get-type");
      else
	{
	  GIrNodeBoxed *boxed;

	  boxed = (GIrNodeBoxed *) g_ir_node_new (G_IR_NODE_BOXED);
	  
	  ((GIrNode *)boxed)->name = g_strdup (name);
	  boxed->gtype_name = g_strdup (typename);
	  boxed->gtype_init = g_strdup (typeinit);
	  if (deprecated && strcmp (deprecated, "1") == 0)
	    boxed->deprecated = TRUE;
	  else
	    boxed->deprecated = FALSE;
	  
	  ctx->current_node = (GIrNode *)boxed;
	  ctx->current_module->entries = 
	    g_list_append (ctx->current_module->entries, boxed);
	  
	  state_switch (ctx, STATE_BOXED);
	}
	  
      return TRUE;
    }

  return FALSE;
}

static gboolean
start_function (GMarkupParseContext *context,
		const gchar         *element_name,
		const gchar        **attribute_names,
		const gchar        **attribute_values,
		ParseContext        *ctx,
		GError             **error)
{
  if ((ctx->state == STATE_NAMESPACE &&
       (strcmp (element_name, "function") == 0 ||
	strcmp (element_name, "callback") == 0)) ||
      ((ctx->state == STATE_CLASS ||
	ctx->state == STATE_INTERFACE ||
	ctx->state == STATE_BOXED ||
        ctx->state == STATE_UNION) &&
       (strcmp (element_name, "method") == 0 || 
	strcmp (element_name, "callback") == 0)) ||
      ((ctx->state == STATE_CLASS ||
	ctx->state == STATE_BOXED) &&
       (strcmp (element_name, "constructor") == 0)) ||
      (ctx->state == STATE_STRUCT && strcmp (element_name, "callback") == 0))
    {
      const gchar *name;
      const gchar *symbol;
      const gchar *deprecated;
      const gchar *type;
      
      name = find_attribute ("name", attribute_names, attribute_values);
      symbol = find_attribute ("c:identifier", attribute_names, attribute_values);
      deprecated = find_attribute ("deprecated", attribute_names, attribute_values);
      type = find_attribute ("type", attribute_names, attribute_values);
      
      if (name == NULL)
	MISSING_ATTRIBUTE (context, error, element_name, "name");
      else if (strcmp (element_name, "callback") != 0 && symbol == NULL)
	MISSING_ATTRIBUTE (context, error, element_name, "c:identifier");
      else
	{
	  GIrNodeFunction *function;
	  
	  function = (GIrNodeFunction *) g_ir_node_new (G_IR_NODE_FUNCTION);

	  ((GIrNode *)function)->name = g_strdup (name);
	  function->symbol = g_strdup (symbol);
	  function->parameters = NULL;
	  if (deprecated && strcmp (deprecated, "1") == 0)
	    function->deprecated = TRUE;
	  else
	    function->deprecated = FALSE;
	
	  if (strcmp (element_name, "method") == 0 ||
	      strcmp (element_name, "constructor") == 0)
	    {
	      function->is_method = TRUE;

	      if (type && strcmp (type, "setter") == 0)
		function->is_setter = TRUE;
	      else if (type && strcmp (type, "getter") == 0)
		function->is_getter = TRUE;		  

	      if (strcmp (element_name, "constructor") == 0)
		function->is_constructor = TRUE;
	      else
		function->is_constructor = FALSE;
	    }
	  else
	    {
	      function->is_method = FALSE;
	      function->is_setter = FALSE;
	      function->is_getter = FALSE;
	      function->is_constructor = FALSE;
	      if (strcmp (element_name, "callback") == 0)
		((GIrNode *)function)->type = G_IR_NODE_CALLBACK;
	    }
	  
	  if (ctx->current_node == NULL)
	    {
	      ctx->current_module->entries = 
		g_list_append (ctx->current_module->entries, function);	      
	    }
	  else
	    switch (ctx->current_node->type)
	      {
	      case G_IR_NODE_INTERFACE:
	      case G_IR_NODE_OBJECT:
		{
		  GIrNodeInterface *iface;
		  
		  iface = (GIrNodeInterface *)ctx->current_node;
		  iface->members = g_list_append (iface->members, function);
		}
		break;
	      case G_IR_NODE_BOXED:
		{
		  GIrNodeBoxed *boxed;

		  boxed = (GIrNodeBoxed *)ctx->current_node;
		  boxed->members = g_list_append (boxed->members, function);
		}
		break;
	      case G_IR_NODE_STRUCT:
		{
		  GIrNodeStruct *struct_;
		  
		  struct_ = (GIrNodeStruct *)ctx->current_node;
		  struct_->members = g_list_append (struct_->members, function);		}
		break;
	      case G_IR_NODE_UNION:
		{
		  GIrNodeUnion *union_;
		  
		  union_ = (GIrNodeUnion *)ctx->current_node;
		  union_->members = g_list_append (union_->members, function);
		}
		break;
	      default:
		g_assert_not_reached ();
	      }
	  
	  ctx->current_node = (GIrNode *)function;
	  state_switch (ctx, STATE_FUNCTION);

	  return TRUE;
	}
    }

  return FALSE;
}

static gboolean
start_parameter (GMarkupParseContext *context,
		 const gchar         *element_name,
		 const gchar        **attribute_names,
		 const gchar        **attribute_values,
		 ParseContext        *ctx,
		 GError             **error)
{
  const gchar *name;
  const gchar *direction;
  const gchar *retval;
  const gchar *dipper;
  const gchar *optional;
  const gchar *nullok;
  const gchar *transfer;
  GIrNodeParam *param;
      
  if (!(strcmp (element_name, "parameter") == 0 &&
	ctx->state == STATE_FUNCTION_PARAMETERS))
    return FALSE;

  name = find_attribute ("name", attribute_names, attribute_values);
  direction = find_attribute ("direction", attribute_names, attribute_values);
  retval = find_attribute ("retval", attribute_names, attribute_values);
  dipper = find_attribute ("dipper", attribute_names, attribute_values);
  optional = find_attribute ("optional", attribute_names, attribute_values);
  nullok = find_attribute ("null-ok", attribute_names, attribute_values);
  transfer = find_attribute ("transfer", attribute_names, attribute_values);

  if (name == NULL)
    name = "unknown";

  param = (GIrNodeParam *)g_ir_node_new (G_IR_NODE_PARAM);

  ctx->current_typed = (GIrNode*) param;
  ctx->current_typed->name = g_strdup (name);

  state_switch (ctx, STATE_FUNCTION_PARAMETER);

  if (direction && strcmp (direction, "out") == 0)
    {
      param->in = FALSE;
      param->out = TRUE;
    }
  else if (direction && strcmp (direction, "inout") == 0)
    {
      param->in = TRUE;
      param->out = TRUE;
    }
  else
    {
      param->in = TRUE;
      param->out = FALSE;
    }

  if (retval && strcmp (retval, "1") == 0)
    param->retval = TRUE;
  else
    param->retval = FALSE;

  if (dipper && strcmp (dipper, "1") == 0)
    param->dipper = TRUE;
  else
    param->dipper = FALSE;

  if (optional && strcmp (optional, "1") == 0)
    param->optional = TRUE;
  else
    param->optional = FALSE;

  if (nullok && strcmp (nullok, "1") == 0)
    param->null_ok = TRUE;
  else
    param->null_ok = FALSE;

  if (transfer && strcmp (transfer, "none") == 0)
    {
      param->transfer = FALSE;
      param->shallow_transfer = FALSE;
    }
  else if (transfer && strcmp (transfer, "shallow") == 0)
    {
      param->transfer = FALSE;
      param->shallow_transfer = TRUE;
    }
  else
    {
      param->transfer = TRUE;
      param->shallow_transfer = FALSE;
    }
	  
  ((GIrNode *)param)->name = g_strdup (name);
	  
  switch (ctx->current_node->type)
    {
    case G_IR_NODE_FUNCTION:
    case G_IR_NODE_CALLBACK:
      {
	GIrNodeFunction *func;

	func = (GIrNodeFunction *)ctx->current_node;
	func->parameters = g_list_append (func->parameters, param);
      }
      break;
    case G_IR_NODE_SIGNAL:
      {
	GIrNodeSignal *signal;

	signal = (GIrNodeSignal *)ctx->current_node;
	signal->parameters = g_list_append (signal->parameters, param);
      }
      break;
    case G_IR_NODE_VFUNC:
      {
	GIrNodeVFunc *vfunc;
		
	vfunc = (GIrNodeVFunc *)ctx->current_node;
	vfunc->parameters = g_list_append (vfunc->parameters, param);
      }
      break;
    default:
      g_assert_not_reached ();
    }

  return TRUE;
}

static gboolean
start_field (GMarkupParseContext *context,
	     const gchar         *element_name,
	     const gchar        **attribute_names,
	     const gchar        **attribute_values,
	     ParseContext        *ctx,
	     GError             **error)
{
  if (strcmp (element_name, "field") == 0 &&
      (ctx->state == STATE_CLASS ||
       ctx->state == STATE_BOXED ||
       ctx->state == STATE_STRUCT ||
       ctx->state == STATE_UNION ||
       ctx->state == STATE_INTERFACE))
    {
      const gchar *name;
      const gchar *readable;
      const gchar *writable;
      const gchar *bits;
      const gchar *branch;
      const gchar *offset;
      
      name = find_attribute ("name", attribute_names, attribute_values);
      readable = find_attribute ("readable", attribute_names, attribute_values);
      writable = find_attribute ("writable", attribute_names, attribute_values);
      bits = find_attribute ("bits", attribute_names, attribute_values);
      branch = find_attribute ("branch", attribute_names, attribute_values);
      offset = find_attribute ("offset", attribute_names, attribute_values);
      
      if (name == NULL)
	MISSING_ATTRIBUTE (context, error, element_name, "name");
      else
	{
	  GIrNodeField *field;

	  field = (GIrNodeField *)g_ir_node_new (G_IR_NODE_FIELD);
	  ctx->current_typed = (GIrNode*) field;
	  ((GIrNode *)field)->name = g_strdup (name);
	  if (readable && strcmp (readable, "1") == 0)
	    field->readable = TRUE;
	  else
	    field->readable = FALSE;
	  
	  if (writable && strcmp (writable, "1") == 0)
	    field->writable = TRUE;
	  else
	    field->writable = FALSE;
	  
	  if (bits)
	    field->bits = atoi (bits);
	  else
	    field->bits = 0;

	  if (offset)
	    field->offset = atoi (offset);
	  else
	    field->offset = 0;
	  
	  switch (ctx->current_node->type)
	    {
	    case G_IR_NODE_OBJECT:
	      {
		GIrNodeInterface *iface;

		iface = (GIrNodeInterface *)ctx->current_node;
		iface->members = g_list_append (iface->members, field);
		state_switch (ctx, STATE_CLASS_FIELD);
	      }
	      break;
	    case G_IR_NODE_INTERFACE:
	      {
		GIrNodeInterface *iface;

		iface = (GIrNodeInterface *)ctx->current_node;
		iface->members = g_list_append (iface->members, field);
		state_switch (ctx, STATE_INTERFACE_FIELD);
	      }
	      break;
	    case G_IR_NODE_BOXED:
	      {
		GIrNodeBoxed *boxed;

		boxed = (GIrNodeBoxed *)ctx->current_node;
		boxed->members = g_list_append (boxed->members, field);
		state_switch (ctx, STATE_BOXED_FIELD);
	      }
	      break;
	    case G_IR_NODE_STRUCT:
	      {
		GIrNodeStruct *struct_;

		struct_ = (GIrNodeStruct *)ctx->current_node;
		struct_->members = g_list_append (struct_->members, field);
		state_switch (ctx, STATE_STRUCT_FIELD);
	      }
	      break;
	    case G_IR_NODE_UNION:
	      {
		GIrNodeUnion *union_;

		union_ = (GIrNodeUnion *)ctx->current_node;
		union_->members = g_list_append (union_->members, field);
		if (branch)
		  {
		    GIrNodeConstant *constant;
		    
		    constant = (GIrNodeConstant *) g_ir_node_new (G_IR_NODE_CONSTANT);
		    ((GIrNode *)constant)->name = g_strdup (name);
		    constant->value = g_strdup (branch);	  
		    constant->type = union_->discriminator_type;
		    constant->deprecated = FALSE;

		    union_->discriminators = g_list_append (union_->discriminators, constant);
		  }
	      }
	      break;
	    default:
	      g_assert_not_reached ();
	    }
	}
      return TRUE;
    }
  
  return FALSE;
}

static gboolean
start_alias (GMarkupParseContext *context,
	     const gchar         *element_name,
	     const gchar        **attribute_names,
	     const gchar        **attribute_values,
	     ParseContext        *ctx,
	     GError             **error)
{
  const gchar *name;
  const gchar *target;
  const gchar *type;

  name = find_attribute ("name", attribute_names, attribute_values);
  if (name == NULL) {
    MISSING_ATTRIBUTE (context, error, element_name, "name");
    return FALSE;
  }

  target = find_attribute ("target", attribute_names, attribute_values);
  if (name == NULL) {
    MISSING_ATTRIBUTE (context, error, element_name, "target");
    return FALSE;
  }

  g_hash_table_insert (ctx->aliases, g_strdup (name), g_strdup (target));
  return TRUE;
}

static gboolean
start_enum (GMarkupParseContext *context,
	     const gchar         *element_name,
	     const gchar        **attribute_names,
	     const gchar        **attribute_values,
	     ParseContext        *ctx,
	     GError             **error)
{
  if ((strcmp (element_name, "enumeration") == 0 && ctx->state == STATE_NAMESPACE) ||
      (strcmp (element_name, "bitfield") == 0 && ctx->state == STATE_NAMESPACE))
    {
      const gchar *name;
      const gchar *typename;
      const gchar *typeinit;
      const gchar *deprecated;
      
      name = find_attribute ("name", attribute_names, attribute_values);
      typename = find_attribute ("glib:type-name", attribute_names, attribute_values);
      typeinit = find_attribute ("glib:get-type", attribute_names, attribute_values);
      deprecated = find_attribute ("deprecated", attribute_names, attribute_values);
      
      if (name == NULL)
	MISSING_ATTRIBUTE (context, error, element_name, "name");
      else 
	{	      
	  GIrNodeEnum *enum_;
	  
	  if (strcmp (element_name, "enumeration") == 0)
	    enum_ = (GIrNodeEnum *) g_ir_node_new (G_IR_NODE_ENUM);
	  else
	    enum_ = (GIrNodeEnum *) g_ir_node_new (G_IR_NODE_FLAGS);
	  ((GIrNode *)enum_)->name = g_strdup (name);
	  enum_->gtype_name = g_strdup (typename);
	  enum_->gtype_init = g_strdup (typeinit);
	  if (deprecated && strcmp (deprecated, "1") == 0)
	    enum_->deprecated = TRUE;
	  else
	    enum_->deprecated = FALSE;

	  ctx->current_node = (GIrNode *) enum_;
	  ctx->current_module->entries = 
	    g_list_append (ctx->current_module->entries, enum_);	      
	  
	  state_switch (ctx, STATE_ENUM);
	}
      
      return TRUE;
    }
  return FALSE;
}

static gboolean
start_property (GMarkupParseContext *context,
		const gchar         *element_name,
		const gchar        **attribute_names,
		const gchar        **attribute_values,
		ParseContext        *ctx,
		GError             **error)
{
  if (strcmp (element_name, "property") == 0 &&
      (ctx->state == STATE_CLASS ||
       ctx->state == STATE_INTERFACE))
    {
      const gchar *name;
      const gchar *readable;
      const gchar *writable;
      const gchar *construct;
      const gchar *construct_only;
      
      name = find_attribute ("name", attribute_names, attribute_values);
      readable = find_attribute ("readable", attribute_names, attribute_values);
      writable = find_attribute ("writable", attribute_names, attribute_values);
      construct = find_attribute ("construct", attribute_names, attribute_values);
      construct_only = find_attribute ("construct-only", attribute_names, attribute_values);
      
      if (name == NULL)
	MISSING_ATTRIBUTE (context, error, element_name, "name");
      else 
	{	      
	  GIrNodeProperty *property;
	  GIrNodeInterface *iface;
	  
	  property = (GIrNodeProperty *) g_ir_node_new (G_IR_NODE_PROPERTY);
	  ctx->current_typed = (GIrNode*) property;

	  ((GIrNode *)property)->name = g_strdup (name);
	  
	  if (readable && strcmp (readable, "1") == 0)
	    property->readable = TRUE;
	  else
	    property->readable = FALSE;
	  if (writable && strcmp (writable, "1") == 0)
	    property->writable = TRUE;
	  else
	    property->writable = FALSE;
	  if (construct && strcmp (construct, "1") == 0)
	    property->construct = TRUE;
	  else
	    property->construct = FALSE;
	  if (construct_only && strcmp (construct_only, "1") == 0)
	    property->construct_only = TRUE;
	  else
	    property->construct_only = FALSE;

	  iface = (GIrNodeInterface *)ctx->current_node;
	  iface->members = g_list_append (iface->members, property);

	  if (ctx->state == STATE_CLASS)
	    state_switch (ctx, STATE_CLASS_PROPERTY);
	  else if (ctx->state == STATE_INTERFACE)
	    state_switch (ctx, STATE_INTERFACE_PROPERTY);
	  else
	    g_assert_not_reached ();
	}
      
      return TRUE;
    }
  return FALSE;
}

static gint
parse_value (const gchar *str)
{
  gchar *shift_op;
 
  /* FIXME just a quick hack */
  shift_op = strstr (str, "<<");

  if (shift_op)
    {
      gint base, shift;

      base = strtol (str, NULL, 10);
      shift = strtol (shift_op + 3, NULL, 10);
      
      return base << shift;
    }
  else
    return strtol (str, NULL, 10);

  return 0;
}

static gboolean
start_member (GMarkupParseContext *context,
	      const gchar         *element_name,
	      const gchar        **attribute_names,
	      const gchar        **attribute_values,
	      ParseContext        *ctx,
	      GError             **error)
{
  if (strcmp (element_name, "member") == 0 &&
      ctx->state == STATE_ENUM)
    {
      const gchar *name;
      const gchar *value;
      const gchar *deprecated;
      
      name = find_attribute ("name", attribute_names, attribute_values);
      value = find_attribute ("value", attribute_names, attribute_values);
      deprecated = find_attribute ("deprecated", attribute_names, attribute_values);
      
      if (name == NULL)
	MISSING_ATTRIBUTE (context, error, element_name, "name");
      else 
	{	      
	  GIrNodeEnum *enum_;
	  GIrNodeValue *value_;

	  value_ = (GIrNodeValue *) g_ir_node_new (G_IR_NODE_VALUE);

	  ((GIrNode *)value_)->name = g_strdup (name);
	  
	  value_->value = parse_value (value);
	  
	  if (deprecated && strcmp (deprecated, "1") == 0)
	    value_->deprecated = TRUE;
	  else
	    value_->deprecated = FALSE;

	  enum_ = (GIrNodeEnum *)ctx->current_node;
	  enum_->values = g_list_append (enum_->values, value_);
	}
      
      return TRUE;
    }
  return FALSE;
}

static gboolean
start_constant (GMarkupParseContext *context,
		const gchar         *element_name,
		const gchar        **attribute_names,
		const gchar        **attribute_values,
		ParseContext        *ctx,
		GError             **error)
{
  if (strcmp (element_name, "constant") == 0 &&
      (ctx->state == STATE_NAMESPACE ||
       ctx->state == STATE_CLASS ||
       ctx->state == STATE_INTERFACE))
    {
      const gchar *name;
      const gchar *value;
      const gchar *deprecated;
      
      name = find_attribute ("name", attribute_names, attribute_values);
      value = find_attribute ("value", attribute_names, attribute_values);
      deprecated = find_attribute ("deprecated", attribute_names, attribute_values);
      
      if (name == NULL)
	MISSING_ATTRIBUTE (context, error, element_name, "name");
      else if (value == NULL)
	MISSING_ATTRIBUTE (context, error, element_name, "value");
      else 
	{	      
	  GIrNodeConstant *constant;

	  constant = (GIrNodeConstant *) g_ir_node_new (G_IR_NODE_CONSTANT);

	  ((GIrNode *)constant)->name = g_strdup (name);
	  constant->value = g_strdup (value);

	  ctx->current_typed = (GIrNode*) constant;

	  if (deprecated && strcmp (deprecated, "1") == 0)
	    constant->deprecated = TRUE;
	  else
	    constant->deprecated = FALSE;

	  if (ctx->state == STATE_NAMESPACE)
	    {
	      ctx->current_node = (GIrNode *) constant;
	      ctx->current_module->entries = 
		g_list_append (ctx->current_module->entries, constant);
	    }
	  else
	    {
	      GIrNodeInterface *iface;

	      iface = (GIrNodeInterface *)ctx->current_node;
	      iface->members = g_list_append (iface->members, constant);
	    }

	  switch (ctx->state)
	    {
	    case STATE_NAMESPACE:
	      state_switch (ctx, STATE_NAMESPACE_CONSTANT);
	      break;
	    case STATE_CLASS:
	      state_switch (ctx, STATE_CLASS_CONSTANT);
	      break;
	    case STATE_INTERFACE:
	      state_switch (ctx, STATE_INTERFACE_CONSTANT);
	      break;
	    default:
	      g_assert_not_reached ();
	      break;
	    }
	}
      
      return TRUE;
    }
  return FALSE;
}

static gboolean
start_errordomain (GMarkupParseContext *context,
		   const gchar         *element_name,
		   const gchar        **attribute_names,
		   const gchar        **attribute_values,
		   ParseContext        *ctx,
		   GError             **error)
{
  if (strcmp (element_name, "errordomain") == 0 &&
      ctx->state == STATE_NAMESPACE)
    {
      const gchar *name;
      const gchar *getquark;
      const gchar *codes;
      const gchar *deprecated;
      
      name = find_attribute ("name", attribute_names, attribute_values);
      getquark = find_attribute ("get-quark", attribute_names, attribute_values);
      codes = find_attribute ("codes", attribute_names, attribute_values);
      deprecated = find_attribute ("deprecated", attribute_names, attribute_values);
      
      if (name == NULL)
	MISSING_ATTRIBUTE (context, error, element_name, "name");
      else if (getquark == NULL)
	MISSING_ATTRIBUTE (context, error, element_name, "getquark");
      else if (codes == NULL)
	MISSING_ATTRIBUTE (context, error, element_name, "codes");
      else 
	{	      
	  GIrNodeErrorDomain *domain;

	  domain = (GIrNodeErrorDomain *) g_ir_node_new (G_IR_NODE_ERROR_DOMAIN);

	  ((GIrNode *)domain)->name = g_strdup (name);
	  domain->getquark = g_strdup (getquark);
	  domain->codes = g_strdup (codes);

	  if (deprecated && strcmp (deprecated, "1") == 0)
	    domain->deprecated = TRUE;
	  else
	    domain->deprecated = FALSE;

	  ctx->current_node = (GIrNode *) domain;
	  ctx->current_module->entries = 
	    g_list_append (ctx->current_module->entries, domain);

	  state_switch (ctx, STATE_ERRORDOMAIN);
	}
      
      return TRUE;
    }
  return FALSE;
}

static gboolean
start_interface (GMarkupParseContext *context,
		 const gchar         *element_name,
		 const gchar        **attribute_names,
		 const gchar        **attribute_values,
		 ParseContext        *ctx,
		 GError             **error)
{
  if (strcmp (element_name, "interface") == 0 &&
      ctx->state == STATE_NAMESPACE)
    {
      const gchar *name;
      const gchar *typename;
      const gchar *typeinit;
      const gchar *deprecated;
      
      name = find_attribute ("name", attribute_names, attribute_values);
      typename = find_attribute ("glib:type-name", attribute_names, attribute_values);
      typeinit = find_attribute ("glib:get-type", attribute_names, attribute_values);
      deprecated = find_attribute ("deprecated", attribute_names, attribute_values);
      
      if (name == NULL)
	MISSING_ATTRIBUTE (context, error, element_name, "name");
      else if (typename == NULL)
	MISSING_ATTRIBUTE (context, error, element_name, "glib:type-name");
      else if (typeinit == NULL)
	MISSING_ATTRIBUTE (context, error, element_name, "glib:get-type");
      else
	{
	  GIrNodeInterface *iface;

	  iface = (GIrNodeInterface *) g_ir_node_new (G_IR_NODE_INTERFACE);
	  ((GIrNode *)iface)->name = g_strdup (name);
	  iface->gtype_name = g_strdup (typename);
	  iface->gtype_init = g_strdup (typeinit);
	  if (deprecated && strcmp (deprecated, "1") == 0)
	    iface->deprecated = TRUE;
	  else
	    iface->deprecated = FALSE;
	  
	  ctx->current_node = (GIrNode *) iface;
	  ctx->current_module->entries = 
	    g_list_append (ctx->current_module->entries, iface);	      
	  
	  state_switch (ctx, STATE_INTERFACE);
	  
	}
      
      return TRUE;
    }
  return FALSE;
}

static gboolean
start_class (GMarkupParseContext *context,
	      const gchar         *element_name,
	      const gchar        **attribute_names,
	      const gchar        **attribute_values,
	      ParseContext        *ctx,
	      GError             **error)
{
  if (strcmp (element_name, "class") == 0 &&
      ctx->state == STATE_NAMESPACE)
    {
      const gchar *name;
      const gchar *parent;
      const gchar *typename;
      const gchar *typeinit;
      const gchar *deprecated;
      
      name = find_attribute ("name", attribute_names, attribute_values);
      parent = find_attribute ("parent", attribute_names, attribute_values);
      typename = find_attribute ("glib:type-name", attribute_names, attribute_values);
      typeinit = find_attribute ("glib:get-type", attribute_names, attribute_values);
      deprecated = find_attribute ("deprecated", attribute_names, attribute_values);
      
      if (name == NULL)
	MISSING_ATTRIBUTE (context, error, element_name, "name");
      else if (typename == NULL)
	MISSING_ATTRIBUTE (context, error, element_name, "glib:type-name");
      else if (typeinit == NULL && strcmp (typename, "GObject"))
	MISSING_ATTRIBUTE (context, error, element_name, "glib:get-type");
      else
	{
	  GIrNodeInterface *iface;

	  iface = (GIrNodeInterface *) g_ir_node_new (G_IR_NODE_OBJECT);
	  ((GIrNode *)iface)->name = g_strdup (name);
	  iface->gtype_name = g_strdup (typename);
	  iface->gtype_init = g_strdup (typeinit);
	  iface->parent = g_strdup (parent);
	  if (deprecated && strcmp (deprecated, "1") == 0)
	    iface->deprecated = TRUE;
	  else
	    iface->deprecated = FALSE;
	  
	  ctx->current_node = (GIrNode *) iface;
	  ctx->current_module->entries = 
	    g_list_append (ctx->current_module->entries, iface);	      
	  
	  state_switch (ctx, STATE_CLASS);
	}
      
      return TRUE;
    }
  return  FALSE;
}

static gboolean
start_type (GMarkupParseContext *context,
	    const gchar         *element_name,
	    const gchar        **attribute_names,
	    const gchar        **attribute_values,
	    ParseContext       *ctx,
	    GError             **error)
{
  const gchar *name;

  if (strcmp (element_name, "type") != 0 ||
      !(ctx->state == STATE_FUNCTION_PARAMETER ||
	ctx->state == STATE_FUNCTION_RETURN || 
	ctx->state == STATE_STRUCT_FIELD ||
	ctx->state == STATE_CLASS_PROPERTY ||
	ctx->state == STATE_CLASS_FIELD ||
	ctx->state == STATE_INTERFACE_FIELD ||
	ctx->state == STATE_INTERFACE_PROPERTY ||
	ctx->state == STATE_BOXED_FIELD ||
	ctx->state == STATE_NAMESPACE_CONSTANT ||
	ctx->state == STATE_CLASS_CONSTANT ||
	ctx->state == STATE_INTERFACE_CONSTANT
	))
    return FALSE;

  if (!ctx->current_typed)
    {
      g_set_error (error,
		   G_MARKUP_ERROR,
		   G_MARKUP_ERROR_INVALID_CONTENT,
		   "The element <type> is invalid here");
      return FALSE;
    }

  name = find_attribute ("name", attribute_names, attribute_values);

  if (name == NULL)
    MISSING_ATTRIBUTE (context, error, element_name, "name");
  
  switch (ctx->current_typed->type)
    {
    case G_IR_NODE_PARAM:
      {
	GIrNodeParam *param = (GIrNodeParam *)ctx->current_typed;
	param->type = parse_type (ctx, name);
      }
      break;
    case G_IR_NODE_FIELD:
      {
	GIrNodeField *field = (GIrNodeField *)ctx->current_typed;
	field->type = parse_type (ctx, name);
      }
      break;
    case G_IR_NODE_PROPERTY:
      {
	GIrNodeProperty *property = (GIrNodeProperty *) ctx->current_typed;
	property->type = parse_type (ctx, name);
      }
      break;
    case G_IR_NODE_CONSTANT:
      {
	GIrNodeConstant *constant = (GIrNodeConstant *)ctx->current_typed;
	constant->type = parse_type (ctx, name);
      }
      break;
    default:
      g_printerr("current node is %d\n", ctx->current_node->type);
      g_assert_not_reached ();
    }

  ctx->current_typed = NULL;
  return TRUE;
}

static gboolean
start_return_value (GMarkupParseContext *context,
		    const gchar         *element_name,
		    const gchar        **attribute_names,
		    const gchar        **attribute_values,
		    ParseContext       *ctx,
		    GError             **error)
{
  if (strcmp (element_name, "return-value") == 0 &&
      ctx->state == STATE_FUNCTION)
    {
      GIrNodeParam *param;

      param = (GIrNodeParam *)g_ir_node_new (G_IR_NODE_PARAM);
      param->in = FALSE;
      param->out = FALSE;
      param->retval = TRUE;

      ctx->current_typed = (GIrNode*) param;

      state_switch (ctx, STATE_FUNCTION_RETURN);

      switch (ctx->current_node->type)
	{
	case G_IR_NODE_FUNCTION:
	case G_IR_NODE_CALLBACK:
	  {
	    GIrNodeFunction *func = (GIrNodeFunction *)ctx->current_node;
	    func->result = param;
	  }
	  break;
	case G_IR_NODE_SIGNAL:
	  {
	    GIrNodeSignal *signal = (GIrNodeSignal *)ctx->current_node;
	    signal->result = param;
	  }
	  break;
	case G_IR_NODE_VFUNC:
	  {
	    GIrNodeVFunc *vfunc = (GIrNodeVFunc *)ctx->current_node;
	    vfunc->result = param;
	  }
	  break;
	default:
	  g_assert_not_reached ();
	}
      
      return TRUE;
    }

  return FALSE;
}

static gboolean
start_glib_signal (GMarkupParseContext *context,
		   const gchar         *element_name,
		   const gchar        **attribute_names,
		   const gchar        **attribute_values,
		   ParseContext       *ctx,
		   GError             **error)
{
  if (strcmp (element_name, "glib:signal") == 0 && 
      (ctx->state == STATE_CLASS ||
       ctx->state == STATE_INTERFACE))
    {
      const gchar *name;
      const gchar *when;
      const gchar *no_recurse;
      const gchar *detailed;
      const gchar *action;
      const gchar *no_hooks;
      const gchar *has_class_closure;
      
      name = find_attribute ("name", attribute_names, attribute_values);
      when = find_attribute ("when", attribute_names, attribute_values);
      no_recurse = find_attribute ("no-recurse", attribute_names, attribute_values);
      detailed = find_attribute ("detailed", attribute_names, attribute_values);
      action = find_attribute ("action", attribute_names, attribute_values);
      no_hooks = find_attribute ("no-hooks", attribute_names, attribute_values);
      has_class_closure = find_attribute ("has-class-closure", attribute_names, attribute_values);
      
      if (name == NULL)
	MISSING_ATTRIBUTE (context, error, element_name, "name");
      else
	{
	  GIrNodeInterface *iface;
	  GIrNodeSignal *signal;

	  signal = (GIrNodeSignal *)g_ir_node_new (G_IR_NODE_SIGNAL);
	  
	  ((GIrNode *)signal)->name = g_strdup (name);
	  
	  signal->run_first = FALSE;
	  signal->run_last = FALSE;
	  signal->run_cleanup = FALSE;
	  if (when == NULL || strcmp (when, "LAST") == 0)
	    signal->run_last = TRUE;
	  else if (strcmp (when, "FIRST") == 0)
	    signal->run_first = TRUE;
	  else 
	    signal->run_cleanup = TRUE;
	  
	  if (no_recurse && strcmp (no_recurse, "1") == 0)
	    signal->no_recurse = TRUE;
	  else
	    signal->no_recurse = FALSE;
	  if (detailed && strcmp (detailed, "1") == 0)
	    signal->detailed = TRUE;
	  else
	    signal->detailed = FALSE;
	  if (action && strcmp (action, "1") == 0)
	    signal->action = TRUE;
	  else
	    signal->action = FALSE;
	  if (no_hooks && strcmp (no_hooks, "1") == 0)
	    signal->no_hooks = TRUE;
	  else
	    signal->no_hooks = FALSE;
	  if (has_class_closure && strcmp (has_class_closure, "1") == 0)
	    signal->has_class_closure = TRUE;
	  else
	    signal->has_class_closure = FALSE;

	  iface = (GIrNodeInterface *)ctx->current_node;
	  iface->members = g_list_append (iface->members, signal);

	  ctx->current_node = (GIrNode *)signal;
	  state_switch (ctx, STATE_FUNCTION);
	}
      
      return TRUE;
    }
  return FALSE;
}

static gboolean
start_vfunc (GMarkupParseContext *context,
	     const gchar         *element_name,
	     const gchar        **attribute_names,
	     const gchar        **attribute_values,
	     ParseContext       *ctx,
	     GError             **error)
{
  if (strcmp (element_name, "vfunc") == 0 && 
      (ctx->state == STATE_CLASS ||
       ctx->state == STATE_INTERFACE))
    {
      const gchar *name;
      const gchar *must_chain_up;
      const gchar *override;
      const gchar *is_class_closure;
      const gchar *offset;
      
      name = find_attribute ("name", attribute_names, attribute_values);
      must_chain_up = find_attribute ("must-chain-up", attribute_names, attribute_values);	  
      override = find_attribute ("override", attribute_names, attribute_values);
      is_class_closure = find_attribute ("is-class-closure", attribute_names, attribute_values);
      offset = find_attribute ("offset", attribute_names, attribute_values);
      
      if (name == NULL)
	MISSING_ATTRIBUTE (context, error, element_name, "name");
      else
	{
	  GIrNodeInterface *iface;
	  GIrNodeVFunc *vfunc;

	  vfunc = (GIrNodeVFunc *)g_ir_node_new (G_IR_NODE_VFUNC);
	  
	  ((GIrNode *)vfunc)->name = g_strdup (name);

	  if (must_chain_up && strcmp (must_chain_up, "1") == 0)
	    vfunc->must_chain_up = TRUE;
	  else
	    vfunc->must_chain_up = FALSE;

	  if (override && strcmp (override, "always") == 0)
	    {
	      vfunc->must_be_implemented = TRUE;
	      vfunc->must_not_be_implemented = FALSE;
	    }
	  else if (override && strcmp (override, "never") == 0)
	    {
	      vfunc->must_be_implemented = FALSE;
	      vfunc->must_not_be_implemented = TRUE;
	    }
	  else
	    {
	      vfunc->must_be_implemented = FALSE;
	      vfunc->must_not_be_implemented = FALSE;
	    }
	  
	  if (is_class_closure && strcmp (is_class_closure, "1") == 0)
	    vfunc->is_class_closure = TRUE;
	  else
	    vfunc->is_class_closure = FALSE;
	  
	  if (offset)
	    vfunc->offset = atoi (offset);
	  else
	    vfunc->offset = 0;

	  iface = (GIrNodeInterface *)ctx->current_node;
	  iface->members = g_list_append (iface->members, vfunc);

	  ctx->current_node = (GIrNode *)vfunc;
	  state_switch (ctx, STATE_FUNCTION);
	}
      
      return TRUE;
    }
  return FALSE;
}


static gboolean
start_struct (GMarkupParseContext *context,
	      const gchar         *element_name,
	      const gchar        **attribute_names,
	      const gchar        **attribute_values,
	      ParseContext       *ctx,
	      GError             **error)
{
  if (strcmp (element_name, "record") == 0 && 
      ctx->state == STATE_NAMESPACE)
    {
      const gchar *name;
      const gchar *deprecated;
      
      name = find_attribute ("name", attribute_names, attribute_values);
      deprecated = find_attribute ("deprecated", attribute_names, attribute_values);
      
      if (name == NULL)
	MISSING_ATTRIBUTE (context, error, element_name, "name");
      else
	{
	  GIrNodeStruct *struct_;

	  struct_ = (GIrNodeStruct *) g_ir_node_new (G_IR_NODE_STRUCT);
	  
	  ((GIrNode *)struct_)->name = g_strdup (name);
	  if (deprecated && strcmp (deprecated, "1") == 0)
	    struct_->deprecated = TRUE;
	  else
	    struct_->deprecated = FALSE;

	  ctx->current_node = (GIrNode *)struct_;
	  ctx->current_module->entries = 
	    g_list_append (ctx->current_module->entries, struct_);
	  
	  state_switch (ctx, STATE_STRUCT);
	}
      return TRUE;
    }
  return FALSE;
}
  

static gboolean
start_union (GMarkupParseContext *context,
	     const gchar         *element_name,
	     const gchar        **attribute_names,
	     const gchar        **attribute_values,
	     ParseContext       *ctx,
	     GError             **error)
{
  if (strcmp (element_name, "union") == 0 && 
      ctx->state == STATE_NAMESPACE)
    {
      const gchar *name;
      const gchar *deprecated;
      const gchar *typename;
      const gchar *typeinit;
      
      name = find_attribute ("name", attribute_names, attribute_values);
      deprecated = find_attribute ("deprecated", attribute_names, attribute_values);
      typename = find_attribute ("glib:type-name", attribute_names, attribute_values);
      typeinit = find_attribute ("glib:get-type", attribute_names, attribute_values);
      
      if (name == NULL)
	MISSING_ATTRIBUTE (context, error, element_name, "name");
      else
	{
	  GIrNodeUnion *union_;

	  union_ = (GIrNodeUnion *) g_ir_node_new (G_IR_NODE_UNION);
	  
	  ((GIrNode *)union_)->name = g_strdup (name);
	  union_->gtype_name = g_strdup (typename);
	  union_->gtype_init = g_strdup (typeinit);
	  if (deprecated && strcmp (deprecated, "1") == 0)
	    union_->deprecated = TRUE;
	  else
	    union_->deprecated = FALSE;

	  ctx->current_node = (GIrNode *)union_;
	  ctx->current_module->entries = 
	    g_list_append (ctx->current_module->entries, union_);
	  
	  state_switch (ctx, STATE_UNION);
	}
      return TRUE;
    }
  return FALSE;
}

static gboolean
start_discriminator (GMarkupParseContext *context,
		     const gchar         *element_name,
		     const gchar        **attribute_names,
		     const gchar        **attribute_values,
		     ParseContext       *ctx,
		     GError             **error)
{
  if (strcmp (element_name, "discriminator") == 0 &&
      ctx->state == STATE_UNION)
    {
      const gchar *type;
      const gchar *offset;
      
      type = find_attribute ("type", attribute_names, attribute_values);
      offset = find_attribute ("offset", attribute_names, attribute_values);
      if (type == NULL)
	MISSING_ATTRIBUTE (context, error, element_name, "type");
      else if (offset == NULL)
	MISSING_ATTRIBUTE (context, error, element_name, "offset");
	{
	  ((GIrNodeUnion *)ctx->current_node)->discriminator_type 
	    = parse_type (ctx, type);
	  ((GIrNodeUnion *)ctx->current_node)->discriminator_offset 
	    = atoi (offset);
	}
      
      return TRUE;
    }

  return FALSE;
}
  
extern GLogLevelFlags logged_levels;

static void
start_element_handler (GMarkupParseContext *context,
		       const gchar         *element_name,
		       const gchar        **attribute_names,
		       const gchar        **attribute_values,
		       gpointer             user_data,
		       GError             **error)
{
  ParseContext *ctx = user_data;
  gint line_number, char_number;

  if (logged_levels & G_LOG_LEVEL_DEBUG)
    {
      GString *tags = g_string_new ("");
      int i;
      for (i = 0; attribute_names[i]; i++)
        g_string_append_printf (tags, "%s=\"%s\" ",
				attribute_names[i],
				attribute_values[i]);

      if (i)
        {
          g_string_insert_c (tags, 0, ' ');
          g_string_truncate (tags, tags->len - 1);
        }
      g_debug ("<%s%s>", element_name, tags->str);
      g_string_free (tags, TRUE);
    }

  switch (element_name[0]) 
    {
    case 'a':
      if (ctx->state == STATE_NAMESPACE && strcmp (element_name, "alias") == 0) 
	{
	  state_switch (ctx, STATE_ALIAS);
	  goto out;
	}
      break;
    case 'b':
      if (start_enum (context, element_name, 
		      attribute_names, attribute_values,
		      ctx, error))
	goto out;
      break;
    case 'c':
      if (start_function (context, element_name, 
			  attribute_names, attribute_values,
			  ctx, error))
	goto out;
      else if (start_constant (context, element_name,
			       attribute_names, attribute_values,
			       ctx, error))
	goto out;
      else if (start_class (context, element_name, 
			    attribute_names, attribute_values,
			    ctx, error))
	goto out;
      else if (strcmp (element_name, "class") == 0 &&
	       ctx->state == STATE_REQUIRES)
	{
	  const gchar *name;

	  name = find_attribute ("name", attribute_names, attribute_values);

	  if (name == NULL)
	    MISSING_ATTRIBUTE (context, error, element_name, "name");
	  else
	    {  
	      GIrNodeInterface *iface;

	      iface = (GIrNodeInterface *)ctx->current_node;
	      iface ->prerequisites = g_list_append (iface->prerequisites, g_strdup (name));
	    }

	  goto out;
	}
      break;

    case 'd':
      if (start_discriminator (context, element_name, 
			       attribute_names, attribute_values,
			       ctx, error))
	goto out;
      break;

    case 'e':
      if (start_enum (context, element_name, 
		      attribute_names, attribute_values,
		      ctx, error))
	goto out;
      else if (start_errordomain (context, element_name, 
		      attribute_names, attribute_values,
		      ctx, error))
	goto out;
      break;

    case 'f':
      if (start_function (context, element_name, 
			  attribute_names, attribute_values,
			  ctx, error))
	goto out;
      else if (start_field (context, element_name, 
			    attribute_names, attribute_values,
			    ctx, error))
	goto out;
      break;

    case 'g':
      if (start_glib_boxed (context, element_name,
			    attribute_names, attribute_values,
			    ctx, error))
	goto out;
      else if (start_glib_signal (context, element_name,
			     attribute_names, attribute_values,
			     ctx, error))
	goto out;
      else if (start_glib_boxed (context, element_name,
				 attribute_names, attribute_values,
				 ctx, error))
	goto out;
      break;

    case 'i':
      if (start_interface (context, element_name, 
			   attribute_names, attribute_values,
			   ctx, error))
	goto out;
      if (strcmp (element_name, "implements") == 0 &&
	  ctx->state == STATE_CLASS)
	{
	  state_switch (ctx, STATE_IMPLEMENTS);

	  goto out;
	}
      else if (strcmp (element_name, "interface") == 0 &&
	       ctx->state == STATE_IMPLEMENTS)
	{
	  const gchar *name;

	  name = find_attribute ("name", attribute_names, attribute_values);

	  if (name == NULL)
	    MISSING_ATTRIBUTE (context, error, element_name, "name");
	  else
	    {  
	      GIrNodeInterface *iface;

	      iface = (GIrNodeInterface *)ctx->current_node;
	      iface ->interfaces = g_list_append (iface->interfaces, g_strdup (name));
	    }

	  goto out;
	}
      else if (strcmp (element_name, "interface") == 0 &&
	       ctx->state == STATE_REQUIRES)
	{
	  const gchar *name;

	  name = find_attribute ("name", attribute_names, attribute_values);

	  if (name == NULL)
	    MISSING_ATTRIBUTE (context, error, element_name, "name");
	  else
	    {  
	      GIrNodeInterface *iface;

	      iface = (GIrNodeInterface *)ctx->current_node;
	      iface ->prerequisites = g_list_append (iface->prerequisites, g_strdup (name));
	    }

	  goto out;
	}
      break;

    case 'm':
      if (start_function (context, element_name, 
			  attribute_names, attribute_values,
			  ctx, error))
	goto out;
      else if (start_member (context, element_name, 
			  attribute_names, attribute_values,
			  ctx, error))
	goto out;
      break;

    case 'n':
      if (strcmp (element_name, "namespace") == 0 && ctx->state == STATE_REPOSITORY)
	{
	  const gchar *name, *shared_library;
	  
	  name = find_attribute ("name", attribute_names, attribute_values);
	  shared_library = find_attribute ("shared-library", attribute_names, attribute_values);

	  if (name == NULL)
	    MISSING_ATTRIBUTE (context, error, element_name, "name");
	  else
	    {
	      ctx->current_module = g_ir_module_new (name, shared_library);
	      ctx->modules = g_list_append (ctx->modules, ctx->current_module);

	      state_switch (ctx, STATE_NAMESPACE);
	    }

	  goto out;
	}
      break;

    case 'p':
      if (start_property (context, element_name,
			  attribute_names, attribute_values,
			  ctx, error))
	goto out;
      else if (strcmp (element_name, "parameters") == 0 &&
	       ctx->state == STATE_FUNCTION)
	{
	  state_switch (ctx, STATE_FUNCTION_PARAMETERS);

	  goto out;
	}
      else if (start_parameter (context, element_name,
				attribute_names, attribute_values,
				ctx, error))
	goto out;

      break;

    case 'r':
      if (strcmp (element_name, "repository") == 0 && ctx->state == STATE_START)
	{
	  const gchar *version;

	  version = find_attribute ("version", attribute_names, attribute_values);
	  
	  if (version == NULL)
	    MISSING_ATTRIBUTE (context, error, element_name, "version");
	  else if (strcmp (version, "1.0") != 0)
	    g_set_error (error,
			 G_MARKUP_ERROR,
			 G_MARKUP_ERROR_INVALID_CONTENT,
			 "Unsupported version '%s'",
			 version);
	  else
	    state_switch (ctx, STATE_REPOSITORY);
	  
	  goto out;
	}
      else if (start_return_value (context, element_name,
				   attribute_names, attribute_values,
				   ctx, error))
	goto out;      
      else if (strcmp (element_name, "requires") == 0 &&
	       ctx->state == STATE_INTERFACE)
	{
	  state_switch (ctx, STATE_REQUIRES);
	  
	  goto out;
	}
      else if (start_struct (context, element_name,
			     attribute_names, attribute_values,
			     ctx, error))
	goto out;      
      break;

    case 'u':
      if (start_union (context, element_name,
		       attribute_names, attribute_values,
		       ctx, error))
	goto out;
      break;

    case 't':
      if (start_type (context, element_name,
		      attribute_names, attribute_values,
		      ctx, error))
	goto out;
      break;

    case 'v':
      if (start_vfunc (context, element_name,
		       attribute_names, attribute_values,
		       ctx, error))
	goto out;      
      break;
    }

  g_markup_parse_context_get_position (context, &line_number, &char_number);

  g_set_error (error,
	       G_MARKUP_ERROR,
	       G_MARKUP_ERROR_UNKNOWN_ELEMENT,
	       "Unexpected start tag '%s' on line %d char %d; current state=%d",
	       element_name,
	       line_number, char_number, ctx->state);
  
 out: ;
  if (*error) 
    {
      g_markup_parse_context_get_position (context, &line_number, &char_number);

      fprintf (stderr, "Error at line %d, character %d: %s\n", line_number, char_number, (*error)->message);
      backtrace_stderr ();
    }
}

static gboolean
require_one_of_end_elements (GMarkupParseContext *context,
			     ParseContext        *ctx,
			     const char          *actual_name,
			     GError             **error, 
			     ...)
{
  va_list args;
  int line_number, char_number;
  const char *expected;
  gboolean matched = FALSE;

  va_start (args, error);

  while ((expected = va_arg (args, const char*)) != NULL) 
    {
      if (strcmp (expected, actual_name) == 0)
	{
	  matched = TRUE;
	  break;
	}
    }

  va_end (args);

  if (matched)
    return TRUE;

  g_markup_parse_context_get_position (context, &line_number, &char_number);
  g_set_error (error,
	       G_MARKUP_ERROR,
	       G_MARKUP_ERROR_INVALID_CONTENT,
	       "Unexpected end tag '%s' on line %d char %d; current state=%d",
	       actual_name, 
	       line_number, char_number, ctx->state);
  backtrace_stderr();
  return FALSE;
}

static gboolean
require_end_element (GMarkupParseContext *context,
		     ParseContext        *ctx,
		     const char          *expected_name,
		     const char          *actual_name,
		     GError             **error)
{
  return require_one_of_end_elements (context, ctx, actual_name, error, expected_name, NULL);
}

static void
end_element_handler (GMarkupParseContext *context,
		     const gchar         *element_name,
		     gpointer             user_data,
		     GError             **error)
{
  ParseContext *ctx = user_data;

  g_debug ("</%s>", element_name);

  switch (ctx->state)
    {
    case STATE_START:
    case STATE_END:
      /* no need to GError here, GMarkup already catches this */
      break;

    case STATE_REPOSITORY:
      state_switch (ctx, STATE_END);
      break;

    case STATE_NAMESPACE:
      if (require_end_element (context, ctx, "namespace", element_name, error))
	{
          ctx->current_module = NULL;
          state_switch (ctx, STATE_REPOSITORY);
        }
      break;

    case STATE_ALIAS:
      if (require_end_element (context, ctx, "alias", element_name, error))
	{
	  state_switch (ctx, STATE_NAMESPACE);
	}
      break;

    case STATE_FUNCTION_RETURN:
      if (strcmp ("type", element_name) == 0)
	break;
      if (require_end_element (context, ctx, "return-value", element_name, error))
	{
	  state_switch (ctx, STATE_FUNCTION);
	}
      break;

    case STATE_FUNCTION_PARAMETERS:
      if (require_end_element (context, ctx, "parameters", element_name, error))
	{
	  state_switch (ctx, STATE_FUNCTION);
	}
      break;

    case STATE_FUNCTION_PARAMETER:
      if (strcmp ("type", element_name) == 0)
	break;
      if (require_end_element (context, ctx, "parameter", element_name, error))
	{
	  state_switch (ctx, STATE_FUNCTION_PARAMETERS);
	}
      break;

    case STATE_FUNCTION:
       if (ctx->current_node == g_list_last (ctx->current_module->entries)->data)
	{
	  ctx->current_node = NULL;
	  state_switch (ctx, STATE_NAMESPACE);
	}
      else 
	{ 
	  ctx->current_node = g_list_last (ctx->current_module->entries)->data;
	  if (ctx->current_node->type == G_IR_NODE_INTERFACE)
	    state_switch (ctx, STATE_INTERFACE);
	  else if (ctx->current_node->type == G_IR_NODE_OBJECT)
	    state_switch (ctx, STATE_CLASS);
	  else if (ctx->current_node->type == G_IR_NODE_BOXED)
	    state_switch (ctx, STATE_BOXED);
	  else if (ctx->current_node->type == G_IR_NODE_STRUCT)
	    state_switch (ctx, STATE_STRUCT);
	  else if (ctx->current_node->type == G_IR_NODE_UNION)
	    state_switch (ctx, STATE_UNION);
	  else
	    {
	      int line_number, char_number;
	      g_markup_parse_context_get_position (context, &line_number, &char_number);
	      g_set_error (error,
			   G_MARKUP_ERROR,
			   G_MARKUP_ERROR_INVALID_CONTENT,
			   "Unexpected end tag '%s' on line %d char %d",
			   element_name,
			   line_number, char_number);
	    }
	}
      break;

    case STATE_CLASS_FIELD:
      if (strcmp ("type", element_name) == 0)
	break;
      if (require_end_element (context, ctx, "field", element_name, error))
	{
	  state_switch (ctx, STATE_CLASS);
	}
      break;

    case STATE_CLASS_PROPERTY:
      if (strcmp ("type", element_name) == 0)
	break;
      if (require_end_element (context, ctx, "property", element_name, error))
	{
	  state_switch (ctx, STATE_CLASS);
	}
      break;

    case STATE_CLASS:
      if (require_end_element (context, ctx, "class", element_name, error))
	{
	  ctx->current_node = NULL;
	  state_switch (ctx, STATE_NAMESPACE);
	}
      break;

    case STATE_ERRORDOMAIN:
      if (require_end_element (context, ctx, "errordomain", element_name, error))
	{
	  ctx->current_node = NULL;
	  state_switch (ctx, STATE_NAMESPACE);
	}
      break;

    case STATE_INTERFACE_PROPERTY:
      if (strcmp ("type", element_name) == 0)
	break;
      if (require_end_element (context, ctx, "property", element_name, error))
	{
	  state_switch (ctx, STATE_INTERFACE);
	}
      break;

    case STATE_INTERFACE_FIELD:
      if (strcmp ("type", element_name) == 0)
	break;
      if (require_end_element (context, ctx, "field", element_name, error))
	{
	  state_switch (ctx, STATE_INTERFACE);
	}
      break;

    case STATE_INTERFACE:
      if (require_end_element (context, ctx, "interface", element_name, error))
	{
	  ctx->current_node = NULL;
	  state_switch (ctx, STATE_NAMESPACE);
	}
      break;

    case STATE_ENUM:
      if (strcmp ("member", element_name) == 0)
	break;
      else if (require_one_of_end_elements (context, ctx, 
					    element_name, error, "enumeration", 
					    "bitfield", NULL))
	{
	  ctx->current_node = NULL;
	  state_switch (ctx, STATE_NAMESPACE);
	}
      break;

    case STATE_BOXED:
      if (require_end_element (context, ctx, "glib:boxed", element_name, error))
	{
	  ctx->current_node = NULL;
	  state_switch (ctx, STATE_NAMESPACE);
	}
      break;

    case STATE_BOXED_FIELD:
      if (strcmp ("type", element_name) == 0)
	break;
      if (require_end_element (context, ctx, "field", element_name, error))
	{
	  state_switch (ctx, STATE_BOXED);
	}
      break;

    case STATE_STRUCT_FIELD:
      if (strcmp ("type", element_name) == 0)
	break;
      if (require_end_element (context, ctx, "field", element_name, error))
	{
	  state_switch (ctx, STATE_STRUCT);
	}
      break;

    case STATE_STRUCT:
      if (require_end_element (context, ctx, "record", element_name, error))
	{
	  ctx->current_node = NULL;
	  state_switch (ctx, STATE_NAMESPACE);
	}
      break;
    case STATE_UNION:
      if (require_end_element (context, ctx, "union", element_name, error))
	{
	  ctx->current_node = NULL;
	  state_switch (ctx, STATE_NAMESPACE);
	}
      break;
    case STATE_IMPLEMENTS:
      if (strcmp ("interface", element_name) == 0)
	break;
      if (require_end_element (context, ctx, "implements", element_name, error))
        state_switch (ctx, STATE_CLASS);
      break;
    case STATE_REQUIRES:
      if (require_end_element (context, ctx, "requires", element_name, error))
        state_switch (ctx, STATE_INTERFACE);
      break;
    case STATE_NAMESPACE_CONSTANT:
    case STATE_CLASS_CONSTANT:
    case STATE_INTERFACE_CONSTANT:
      if (strcmp ("type", element_name) == 0)
	break;
      if (require_end_element (context, ctx, "constant", element_name, error))
	{
	  ctx->current_node = NULL;
	  switch (ctx->state)
	    {
	    case STATE_NAMESPACE_CONSTANT:
	      state_switch (ctx, STATE_NAMESPACE);
	      break;
	    case STATE_CLASS_CONSTANT:
	      state_switch (ctx, STATE_CLASS);
	      break;
	    case STATE_INTERFACE_CONSTANT:
	      state_switch (ctx, STATE_INTERFACE);
	      break;
	    default:
	      g_assert_not_reached ();
	      break;
	    }
	}
      break;
    default:
      g_error ("Unhandled state %d in end_element_handler\n", ctx->state);
    }
}

static void 
text_handler (GMarkupParseContext *context,
	      const gchar         *text,
	      gsize                text_len,  
	      gpointer             user_data,
	      GError             **error)
{
  /* FIXME warn about non-whitespace text */
}

static void
cleanup (GMarkupParseContext *context,
	 GError              *error,
	 gpointer             user_data)
{
  ParseContext *ctx = user_data;
  GList *m;
  int line_number, char_number;

  for (m = ctx->modules; m; m = m->next)
    g_ir_module_free (m->data);
  g_list_free (ctx->modules);
  ctx->modules = NULL;
  
  ctx->current_module = NULL;
}

static void
firstpass_start_element_handler (GMarkupParseContext *context,
				 const gchar         *element_name,
				 const gchar        **attribute_names,
				 const gchar        **attribute_values,
				 gpointer             user_data,
				 GError             **error)
{
  ParseContext *ctx = user_data;

  if (strcmp (element_name, "alias") == 0) 
    {
      start_alias (context, element_name, attribute_names, attribute_values,
		   ctx, error);
    }
}

static void
firstpass_end_element_handler (GMarkupParseContext *context,
			       const gchar         *element_name,
			       gpointer             user_data,
			       GError             **error)
{
  ParseContext *ctx = user_data;

}


static GMarkupParser firstpass_parser = 
{
  firstpass_start_element_handler,
  firstpass_end_element_handler,
  NULL,
  NULL,
  NULL,
};


static GMarkupParser parser = 
{
  start_element_handler,
  end_element_handler,
  text_handler,
  NULL,
  cleanup
};

GList * 
g_ir_parse_string (const gchar  *buffer, 
		   gssize        length,
		   GError      **error)
{
  ParseContext ctx = { 0 };
  GMarkupParseContext *context;

  ctx.state = STATE_START;
  ctx.aliases = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

  context = g_markup_parse_context_new (&firstpass_parser, 0, &ctx, NULL);

  if (!g_markup_parse_context_parse (context, buffer, length, error))
    goto out;

  if (!g_markup_parse_context_end_parse (context, error))
    goto out;
  
  context = g_markup_parse_context_new (&parser, 0, &ctx, NULL);
  if (!g_markup_parse_context_parse (context, buffer, length, error))
    goto out;

  if (!g_markup_parse_context_end_parse (context, error))
    goto out;

 out:

  g_hash_table_destroy (ctx.aliases);
  
  g_markup_parse_context_free (context);
  
  return ctx.modules;
}

GList *
g_ir_parse_file (const gchar  *filename,
		 GError      **error)
{
  gchar *buffer;
  gsize length;
  GList *modules;

  g_debug ("[parsing] filename %s", filename);

  if (!g_file_get_contents (filename, &buffer, &length, error))
    return NULL;
  
  modules = g_ir_parse_string (buffer, length, error);

  g_free (buffer);

  return modules;
}


