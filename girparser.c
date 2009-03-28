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
#include "girparser.h"
#include "girmodule.h"
#include "girnode.h"
#include "gtypelib.h"

struct _GIrParser
{
  gchar **includes;
  GList *parsed_modules; /* All previously parsed modules */
};

typedef enum
{
  STATE_START,
  STATE_END,
  STATE_REPOSITORY,
  STATE_INCLUDE,
  STATE_PACKAGE,
  STATE_NAMESPACE, /* 5 */
  STATE_ENUM,
  STATE_BITFIELD,
  STATE_FUNCTION,
  STATE_FUNCTION_RETURN,
  STATE_FUNCTION_PARAMETERS, /* 10 */
  STATE_FUNCTION_PARAMETER,
  STATE_CLASS,
  STATE_CLASS_FIELD,
  STATE_CLASS_PROPERTY,
  STATE_INTERFACE, /* 15 */
  STATE_INTERFACE_PROPERTY,
  STATE_INTERFACE_FIELD,
  STATE_IMPLEMENTS,
  STATE_PREREQUISITE,
  STATE_BOXED,   /* 20 */
  STATE_BOXED_FIELD,
  STATE_STRUCT,
  STATE_STRUCT_FIELD,
  STATE_ERRORDOMAIN,
  STATE_UNION, /* 25 */
  STATE_UNION_FIELD,
  STATE_NAMESPACE_CONSTANT,
  STATE_CLASS_CONSTANT,
  STATE_INTERFACE_CONSTANT,
  STATE_ALIAS,
  STATE_TYPE,
  STATE_ATTRIBUTE,
  STATE_UNKNOWN
} ParseState;

typedef struct _ParseContext ParseContext;
struct _ParseContext
{
  GIrParser *parser;

  ParseState state;
  int unknown_depth;
  ParseState prev_state;

  GList *modules;
  GList *include_modules;
  GList *dependencies;
  GHashTable *aliases;
  GHashTable *disguised_structures;

  const char *namespace;
  const char *c_prefix;
  GIrModule *current_module;
  GSList *node_stack;
  GIrNode *current_typed;
  gboolean is_varargs;
  GList *type_stack;
  GList *type_parameters;
  int type_depth;
};
#define CURRENT_NODE(ctx) ((GIrNode *)((ctx)->node_stack->data))

static void start_element_handler (GMarkupParseContext *context,
				   const gchar         *element_name,
				   const gchar        **attribute_names,
				   const gchar        **attribute_values,
				   gpointer             user_data,
				   GError             **error);
static void end_element_handler   (GMarkupParseContext *context,
				   const gchar         *element_name,
				   gpointer             user_data,
				   GError             **error);
static void text_handler          (GMarkupParseContext *context,
				   const gchar         *text,
				   gsize                text_len,
				   gpointer             user_data,
				   GError             **error);
static void cleanup               (GMarkupParseContext *context,
				   GError              *error,
				   gpointer             user_data);

static GMarkupParser markup_parser = 
{
  start_element_handler,
  end_element_handler,
  text_handler,
  NULL,
  cleanup
};

static gboolean
start_alias (GMarkupParseContext *context,
	     const gchar         *element_name,
	     const gchar        **attribute_names,
	     const gchar        **attribute_values,
	     ParseContext        *ctx,
	     GError             **error);

static const gchar *find_attribute (const gchar  *name, 
				    const gchar **attribute_names,
				    const gchar **attribute_values);


GIrParser *
g_ir_parser_new (void)
{
  GIrParser *parser = g_slice_new0 (GIrParser);

  return parser;
}

void
g_ir_parser_free (GIrParser *parser)
{
  GList *l;

  if (parser->includes)
    g_strfreev (parser->includes);

  for (l = parser->parsed_modules; l; l = l->next)
    g_ir_module_free (l->data);

  g_slice_free (GIrParser, parser);
}

void
g_ir_parser_set_includes (GIrParser          *parser,
			  const gchar *const *includes)
{
  if (parser->includes)
    g_strfreev (parser->includes);

  parser->includes = g_strdupv ((char **)includes);
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
  else if (strcmp (element_name, "record") == 0)
    {
      const gchar *name;
      const gchar *disguised;

      name = find_attribute ("name", attribute_names, attribute_values);
      disguised = find_attribute ("disguised", attribute_names, attribute_values);

      if (disguised && strcmp (disguised, "1") == 0)
	{
	  char *key;

	  key = g_strdup_printf ("%s.%s", ctx->namespace, name);
	  g_hash_table_replace (ctx->disguised_structures, key, GINT_TO_POINTER (1));
	}
    }
}

static void
firstpass_end_element_handler (GMarkupParseContext *context,
			       const gchar         *element_name,
			       gpointer             user_data,
			       GError             **error)
{
}

static GMarkupParser firstpass_parser = 
{
  firstpass_start_element_handler,
  firstpass_end_element_handler,
  NULL,
  NULL,
  NULL,
};

static char *
locate_gir (GIrParser  *parser,
	    const char *girname)
{
  const gchar *const *datadirs;
  const gchar *const *dir;
  char *path = NULL;
      
  datadirs = g_get_system_data_dirs ();
      
  if (parser->includes != NULL)
    {
      for (dir = (const gchar *const *)parser->includes; *dir; dir++) 
	{
	  path = g_build_filename (*dir, girname, NULL);
	  if (g_file_test (path, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR))
	    return path;
	  g_free (path);
	  path = NULL;
	}
    }
  for (dir = datadirs; *dir; dir++) 
    {
      path = g_build_filename (*dir, "gir-1.0", girname, NULL);
      if (g_file_test (path, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR))
	return path;
      g_free (path);
      path = NULL;
    }
  return path;
}

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
#if defined(HAVE_BACKTRACE) && defined(HAVE_BACKTRACE_SYMBOLS)
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
#endif
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

static GIrNode *
pop_node (ParseContext *ctx)
{
  g_assert (ctx->node_stack != 0);
  
  GSList *top = ctx->node_stack;
  GIrNode *node = top->data;
  
  g_debug ("popping node %d %s", node->type, node->name);
  ctx->node_stack = top->next;
  g_slist_free_1 (top);
  return node;
}

static void
push_node (ParseContext *ctx, GIrNode *node)
{
  g_debug ("pushing node %d %s", node->type, node->name);
  ctx->node_stack = g_slist_prepend (ctx->node_stack, node);
}

static GIrNodeType * parse_type_internal (const gchar *str, gchar **next, gboolean in_glib,
					  gboolean in_gobject);

typedef struct {
  const gchar *str;
  gint tag;
  gboolean pointer;
} BasicTypeInfo;

static BasicTypeInfo basic_types[] = {
    { "none",     GI_TYPE_TAG_VOID,    0 },
    { "any",      GI_TYPE_TAG_VOID,    1 },

    { "bool",     GI_TYPE_TAG_BOOLEAN, 0 },
    { "char",     GI_TYPE_TAG_INT8,    0 },
    { "int8",     GI_TYPE_TAG_INT8,    0 },
    { "uint8",    GI_TYPE_TAG_UINT8,   0 },
    { "int16",    GI_TYPE_TAG_INT16,   0 },
    { "uint16",   GI_TYPE_TAG_UINT16,  0 },
    { "int32",    GI_TYPE_TAG_INT32,   0 },
    { "uint32",   GI_TYPE_TAG_UINT32,  0 },
    { "int64",    GI_TYPE_TAG_INT64,   0 },
    { "uint64",   GI_TYPE_TAG_UINT64,  0 },
    { "int",      GI_TYPE_TAG_INT,     0 },
    { "uint",     GI_TYPE_TAG_UINT,    0 },
    { "long",     GI_TYPE_TAG_LONG,    0 },
    { "ulong",    GI_TYPE_TAG_ULONG,   0 },
    { "ssize_t",  GI_TYPE_TAG_SSIZE,   0 },
    { "ssize",    GI_TYPE_TAG_SSIZE,   0 },
    { "size_t",   GI_TYPE_TAG_SIZE,    0 },
    { "size",     GI_TYPE_TAG_SIZE,    0 },
    { "float",    GI_TYPE_TAG_FLOAT,   0 },
    { "double",   GI_TYPE_TAG_DOUBLE,  0 },
    { "time_t",   GI_TYPE_TAG_TIME_T,  0 },
    { "GType",    GI_TYPE_TAG_GTYPE,   0 },
    { "utf8",     GI_TYPE_TAG_UTF8,    1 },  
    { "filename", GI_TYPE_TAG_FILENAME,1 },
};  

static const BasicTypeInfo *
parse_basic (const char *str)
{
  gint i;
  gint n_basic = G_N_ELEMENTS (basic_types);
  
  for (i = 0; i < n_basic; i++)
    {
      if (g_str_has_prefix (str, basic_types[i].str))
	return &(basic_types[i]);
    }  
  return NULL;
}

static GIrNodeType *
parse_type_internal (const gchar *str, char **next, gboolean in_glib,
		     gboolean in_gobject)
{
  const BasicTypeInfo *basic;  
  GIrNodeType *type;
  char *temporary_type = NULL;
  
  type = (GIrNodeType *)g_ir_node_new (G_IR_NODE_TYPE);
  
  type->unparsed = g_strdup (str);

  /* See comment below on GLib.List handling */
  if (in_gobject && strcmp (str, "Type") == 0) 
    {
      temporary_type = g_strdup ("GLib.Type");
      str = temporary_type;
    }
  
  basic = parse_basic (str);
  if (basic != NULL)
    {
      type->is_basic = TRUE;
      type->tag = basic->tag;
      type->is_pointer = basic->pointer;

      str += strlen(basic->str);
    }
  else if (in_glib)
    {
      /* If we're inside GLib, handle "List" etc. by prefixing with
       * "GLib." so the parsing code below doesn't have to get more
       * special. 
       */
      if (g_str_has_prefix (str, "List<") ||
	  strcmp (str, "List") == 0)
	{
	  temporary_type = g_strdup_printf ("GLib.List%s", str + 4);
	  str = temporary_type;
	}
      else if (g_str_has_prefix (str, "SList<") ||
	  strcmp (str, "SList") == 0)
	{
	  temporary_type = g_strdup_printf ("GLib.SList%s", str + 5);
	  str = temporary_type;
	}
      else if (g_str_has_prefix (str, "HashTable<") ||
	  strcmp (str, "HashTable") == 0)
	{
	  temporary_type = g_strdup_printf ("GLib.HashTable%s", str + 9);
	  str = temporary_type;
	}
      else if (g_str_has_prefix (str, "Error<") ||
	  strcmp (str, "Error") == 0)
	{
	  temporary_type = g_strdup_printf ("GLib.Error%s", str + 5);
	  str = temporary_type;
	}
    }

  if (basic != NULL)
    /* found a basic type */;
  else if (g_str_has_prefix (str, "GLib.List") ||
	   g_str_has_prefix (str, "GLib.SList"))
    {
      str += strlen ("GLib.");
      if (g_str_has_prefix (str, "List"))
	{
	  type->tag = GI_TYPE_TAG_GLIST;
	  type->is_glist = TRUE;
	  type->is_pointer = TRUE;
	  str += strlen ("List");
	}
      else
	{
	  type->tag = GI_TYPE_TAG_GSLIST;
	  type->is_gslist = TRUE;
	  type->is_pointer = TRUE;
	  str += strlen ("SList");
	}
    }
  else if (g_str_has_prefix (str, "GLib.HashTable"))
    {
      str += strlen ("GLib.");

      type->tag = GI_TYPE_TAG_GHASH;
      type->is_ghashtable = TRUE;
      type->is_pointer = TRUE;
      str += strlen ("HashTable");
    }
  else if (g_str_has_prefix (str, "GLib.Error"))
    {
      str += strlen ("GLib.");

      type->tag = GI_TYPE_TAG_ERROR;
      type->is_error = TRUE;
      type->is_pointer = TRUE;
      str += strlen ("Error");
      
      if (*str == '<')
	{
	  (str)++;
	  char *tmp, *end;
	  
	  end = strchr (str, '>');
	  tmp = g_strndup (str, end - str);
	  type->errors = g_strsplit (tmp, ",", 0);
	  g_free (tmp);

	  str = end;
	}
    }
  else 
    {
      type->tag = GI_TYPE_TAG_INTERFACE;
      type->is_interface = TRUE; 
      const char *start = str;

      /* must be an interface type */
      while (g_ascii_isalnum (*str) || 
	     *str == '.' || 
	     *str == '-' || 
	     *str == '_' ||
	     *str == ':')
	(str)++;

      type->interface = g_strndup (start, str - start);
    }
  
  if (next)
    *next = (char*)str;
  g_assert (type->tag >= 0 && type->tag <= GI_TYPE_TAG_ERROR);
  g_free (temporary_type);
  return type;

/* error: */
  g_ir_node_free ((GIrNode *)type);
  g_free (temporary_type);  
  return NULL;
}

static const char *
resolve_aliases (ParseContext *ctx, const gchar *type)
{
  gpointer orig;
  gpointer value;
  GSList *seen_values = NULL;
  const gchar *lookup;
  gchar *prefixed;

  if (strchr (type, '.') == NULL)
    {
      prefixed = g_strdup_printf ("%s.%s", ctx->namespace, type);
      lookup = prefixed;
    }
  else
    {
      lookup = type;
      prefixed = NULL;
    }

  seen_values = g_slist_prepend (seen_values, (char*)lookup);
  while (g_hash_table_lookup_extended (ctx->current_module->aliases, lookup, &orig, &value))
    {
      g_debug ("Resolved: %s => %s\n", lookup, (char*)value);
      lookup = value;
      if (g_slist_find_custom (seen_values, lookup,
			       (GCompareFunc)strcmp) != NULL)
	break;
      seen_values = g_slist_prepend (seen_values, (gchar*)lookup);
    }
  g_slist_free (seen_values);

  if (lookup == prefixed)
    lookup = type;

  g_free (prefixed);
  
  return lookup;
}

static gboolean
is_disguised_structure (ParseContext *ctx, const gchar *type)
{
  const gchar *lookup;
  gchar *prefixed;
  gboolean result;

  if (strchr (type, '.') == NULL)
    {
      prefixed = g_strdup_printf ("%s.%s", ctx->namespace, type);
      lookup = prefixed;
    }
  else
    {
      lookup = type;
      prefixed = NULL;
    }

  result = g_hash_table_lookup (ctx->current_module->disguised_structures,
				lookup) != NULL;
  
  g_free (prefixed);
  
  return result;
}

static GIrNodeType *
parse_type (ParseContext *ctx, const gchar *type)
{
  GIrNodeType *node;
  const BasicTypeInfo *basic;
  gboolean in_glib, in_gobject;

  in_glib = strcmp (ctx->namespace, "GLib") == 0;
  in_gobject = strcmp (ctx->namespace, "GObject") == 0;

  /* Do not search aliases for basic types */
  basic = parse_basic (type);
  if (basic == NULL)
    type = resolve_aliases (ctx, type);

  node = parse_type_internal (type, NULL, in_glib, in_gobject);
  if (node)
    g_debug ("Parsed type: %s => %d", type, node->tag);
  else
    g_critical ("Failed to parse type: '%s'", type);

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
  const gchar *name;
  const gchar *typename;
  const gchar *typeinit;
  const gchar *deprecated;
  GIrNodeBoxed *boxed;

  if (!(strcmp (element_name, "glib:boxed") == 0 &&
	ctx->state == STATE_NAMESPACE))
    return FALSE;

  name = find_attribute ("glib:name", attribute_names, attribute_values);
  typename = find_attribute ("glib:type-name", attribute_names, attribute_values);
  typeinit = find_attribute ("glib:get-type", attribute_names, attribute_values);
  deprecated = find_attribute ("deprecated", attribute_names, attribute_values);
  
  if (name == NULL)
    {
      MISSING_ATTRIBUTE (context, error, element_name, "glib:name");
      return FALSE;
    }
  else if (typename == NULL)
    {
      MISSING_ATTRIBUTE (context, error, element_name, "glib:type-name");
      return FALSE;
    }
  else if (typeinit == NULL)
    {
      MISSING_ATTRIBUTE (context, error, element_name, "glib:get-type");
      return FALSE;
    }

  boxed = (GIrNodeBoxed *) g_ir_node_new (G_IR_NODE_BOXED);
	  
  ((GIrNode *)boxed)->name = g_strdup (name);
  boxed->gtype_name = g_strdup (typename);
  boxed->gtype_init = g_strdup (typeinit);
  if (deprecated)
    boxed->deprecated = TRUE;
  else
    boxed->deprecated = FALSE;
	  
  push_node (ctx, (GIrNode *)boxed);
  ctx->current_module->entries = 
    g_list_append (ctx->current_module->entries, boxed);
  
  state_switch (ctx, STATE_BOXED);

  return TRUE;
}

static gboolean
start_function (GMarkupParseContext *context,
		const gchar         *element_name,
		const gchar        **attribute_names,
		const gchar        **attribute_values,
		ParseContext        *ctx,
		GError             **error)
{
  const gchar *name;
  const gchar *symbol;
  const gchar *deprecated;
  const gchar *throws;
  GIrNodeFunction *function;
  gboolean found = FALSE;
  
  switch (ctx->state)
    {
    case STATE_NAMESPACE:
      found = (strcmp (element_name, "function") == 0 ||
	       strcmp (element_name, "callback") == 0);
      break;
    case STATE_CLASS:
      found = strcmp (element_name, "function") == 0;
	/* fallthrough */
    case STATE_BOXED:
    case STATE_STRUCT:
    case STATE_UNION:
      found = (found || strcmp (element_name, "constructor") == 0);
      /* fallthrough */
    case STATE_INTERFACE:
      found = (found ||
	       strcmp (element_name, "method") == 0 ||
	       strcmp (element_name, "callback") == 0);
      break;
    default:
      break;
    }

  if (!found)
    return FALSE;

  name = find_attribute ("name", attribute_names, attribute_values);
  symbol = find_attribute ("c:identifier", attribute_names, attribute_values);
  deprecated = find_attribute ("deprecated", attribute_names, attribute_values);
  throws = find_attribute ("throws", attribute_names, attribute_values);
      
  if (name == NULL)
    {
      MISSING_ATTRIBUTE (context, error, element_name, "name");
      return FALSE;
    }
  else if (strcmp (element_name, "callback") != 0 && symbol == NULL)
    {
      MISSING_ATTRIBUTE (context, error, element_name, "c:identifier");
      return FALSE;
    }

  function = (GIrNodeFunction *) g_ir_node_new (G_IR_NODE_FUNCTION);
      
  ((GIrNode *)function)->name = g_strdup (name);
  function->symbol = g_strdup (symbol);
  function->parameters = NULL;
  if (deprecated)
    function->deprecated = TRUE;
  else
    function->deprecated = FALSE;
  
  if (strcmp (element_name, "method") == 0 ||
      strcmp (element_name, "constructor") == 0)
    {
      function->is_method = TRUE;
      
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

  if (throws && strcmp (throws, "1") == 0)
    function->throws = TRUE;
  else
    function->throws = FALSE;

  if (ctx->node_stack == NULL)
    {
      ctx->current_module->entries = 
	g_list_append (ctx->current_module->entries, function);	      
    }
  else
    switch (CURRENT_NODE (ctx)->type)
      {
      case G_IR_NODE_INTERFACE:
      case G_IR_NODE_OBJECT:
	{
	  GIrNodeInterface *iface;
	  
	  iface = (GIrNodeInterface *)CURRENT_NODE (ctx);
	  iface->members = g_list_append (iface->members, function);
	}
	break;
      case G_IR_NODE_BOXED:
	{
	  GIrNodeBoxed *boxed;
	  
	  boxed = (GIrNodeBoxed *)CURRENT_NODE (ctx);
	  boxed->members = g_list_append (boxed->members, function);
	}
	break;
      case G_IR_NODE_STRUCT:
	{
	  GIrNodeStruct *struct_;
	  
	  struct_ = (GIrNodeStruct *)CURRENT_NODE (ctx);
	  struct_->members = g_list_append (struct_->members, function);		}
	break;
      case G_IR_NODE_UNION:
	{
	  GIrNodeUnion *union_;
	  
	  union_ = (GIrNodeUnion *)CURRENT_NODE (ctx);
	  union_->members = g_list_append (union_->members, function);
	}
	break;
      default:
	g_assert_not_reached ();
      }
  
  push_node(ctx, (GIrNode *)function);
  state_switch (ctx, STATE_FUNCTION);
  
  return TRUE;
}

static void
parse_param_transfer (GIrNodeParam *param, const gchar *transfer)
{
  if (transfer == NULL)
  {
    g_warning ("required attribute 'transfer-ownership' missing");
  }
  else if (strcmp (transfer, "none") == 0)
    {
      param->transfer = FALSE;
      param->shallow_transfer = FALSE;
    }
  else if (strcmp (transfer, "container") == 0)
    {
      param->transfer = FALSE;
      param->shallow_transfer = TRUE;
    }
  else if (strcmp (transfer, "full") == 0)
    {
      param->transfer = TRUE;
      param->shallow_transfer = FALSE;
    }
  else
    {
      g_warning ("Unknown transfer-ownership value: %s", transfer);
    }
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
  const gchar *allow_none;
  const gchar *transfer;
  const gchar *scope;
  const gchar *closure;
  const gchar *destroy;
  GIrNodeParam *param;
      
  if (!(strcmp (element_name, "parameter") == 0 &&
	ctx->state == STATE_FUNCTION_PARAMETERS))
    return FALSE;

  name = find_attribute ("name", attribute_names, attribute_values);
  direction = find_attribute ("direction", attribute_names, attribute_values);
  retval = find_attribute ("retval", attribute_names, attribute_values);
  dipper = find_attribute ("dipper", attribute_names, attribute_values);
  optional = find_attribute ("optional", attribute_names, attribute_values);
  allow_none = find_attribute ("allow-none", attribute_names, attribute_values);
  transfer = find_attribute ("transfer-ownership", attribute_names, attribute_values);
  scope = find_attribute ("scope", attribute_names, attribute_values);
  closure = find_attribute ("closure", attribute_names, attribute_values);
  destroy = find_attribute ("destroy", attribute_names, attribute_values);
  
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

  if (allow_none && strcmp (allow_none, "1") == 0)
    param->allow_none = TRUE;
  else
    param->allow_none = FALSE;

  parse_param_transfer (param, transfer);

  if (scope && strcmp (scope, "call") == 0)
    param->scope = GI_SCOPE_TYPE_CALL;
  else if (scope && strcmp (scope, "async") == 0)
    param->scope = GI_SCOPE_TYPE_ASYNC;
  else if (scope && strcmp (scope, "notified") == 0)
    param->scope = GI_SCOPE_TYPE_NOTIFIED;
  else
    param->scope = GI_SCOPE_TYPE_INVALID;
  
  param->closure = closure ? atoi (closure) : -1;
  param->destroy = destroy ? atoi (destroy) : -1;
  
  ((GIrNode *)param)->name = g_strdup (name);
	  
  switch (CURRENT_NODE (ctx)->type)
    {
    case G_IR_NODE_FUNCTION:
    case G_IR_NODE_CALLBACK:
      {
	GIrNodeFunction *func;

	func = (GIrNodeFunction *)CURRENT_NODE (ctx);
	func->parameters = g_list_append (func->parameters, param);
      }
      break;
    case G_IR_NODE_SIGNAL:
      {
	GIrNodeSignal *signal;

	signal = (GIrNodeSignal *)CURRENT_NODE (ctx);
	signal->parameters = g_list_append (signal->parameters, param);
      }
      break;
    case G_IR_NODE_VFUNC:
      {
	GIrNodeVFunc *vfunc;
		
	vfunc = (GIrNodeVFunc *)CURRENT_NODE (ctx);
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
  const gchar *name;
  const gchar *readable;
  const gchar *writable;
  const gchar *bits;
  const gchar *branch;
  GIrNodeField *field;

  switch (ctx->state)
    {
    case STATE_CLASS:
    case STATE_BOXED:
    case STATE_STRUCT:
    case STATE_UNION:
    case STATE_INTERFACE:
      break;
    default:
      return FALSE;
    }
  
  if (strcmp (element_name, "field") != 0)
    return FALSE;
  
  name = find_attribute ("name", attribute_names, attribute_values);
  readable = find_attribute ("readable", attribute_names, attribute_values);
  writable = find_attribute ("writable", attribute_names, attribute_values);
  bits = find_attribute ("bits", attribute_names, attribute_values);
  branch = find_attribute ("branch", attribute_names, attribute_values);
  
  if (name == NULL)
    {
      MISSING_ATTRIBUTE (context, error, element_name, "name");
      return FALSE;
    }

  field = (GIrNodeField *)g_ir_node_new (G_IR_NODE_FIELD);
  ctx->current_typed = (GIrNode*) field;
  ((GIrNode *)field)->name = g_strdup (name);
  /* Fields are assumed to be read-only.
   * (see also girwriter.py and generate.c)
   */
  field->readable = readable == NULL || strcmp (readable, "0") == 0;
  field->writable = writable != NULL && strcmp (writable, "1") == 0;
  
  if (bits)
    field->bits = atoi (bits);
  else
    field->bits = 0;
  
  switch (CURRENT_NODE (ctx)->type)
    {
    case G_IR_NODE_OBJECT:
      {
	GIrNodeInterface *iface;
	
	iface = (GIrNodeInterface *)CURRENT_NODE (ctx);
	iface->members = g_list_append (iface->members, field);
	state_switch (ctx, STATE_CLASS_FIELD);
      }
      break;
    case G_IR_NODE_INTERFACE:
      {
	GIrNodeInterface *iface;
	
	iface = (GIrNodeInterface *)CURRENT_NODE (ctx);
	iface->members = g_list_append (iface->members, field);
	state_switch (ctx, STATE_INTERFACE_FIELD);
      }
      break;
    case G_IR_NODE_BOXED:
      {
	GIrNodeBoxed *boxed;
	
	boxed = (GIrNodeBoxed *)CURRENT_NODE (ctx);
		boxed->members = g_list_append (boxed->members, field);
		state_switch (ctx, STATE_BOXED_FIELD);
      }
      break;
    case G_IR_NODE_STRUCT:
      {
	GIrNodeStruct *struct_;
	
	struct_ = (GIrNodeStruct *)CURRENT_NODE (ctx);
	struct_->members = g_list_append (struct_->members, field);
	state_switch (ctx, STATE_STRUCT_FIELD);
      }
      break;
    case G_IR_NODE_UNION:
      {
	GIrNodeUnion *union_;
	
	union_ = (GIrNodeUnion *)CURRENT_NODE (ctx);
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
	state_switch (ctx, STATE_UNION_FIELD);
      }
      break;
    default:
      g_assert_not_reached ();
    }
  
  return TRUE;
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
  char *key;
  char *value;

  name = find_attribute ("name", attribute_names, attribute_values);
  if (name == NULL)
    {
      MISSING_ATTRIBUTE (context, error, element_name, "name");
      return FALSE;
    }

  target = find_attribute ("target", attribute_names, attribute_values);
  if (name == NULL)
    {
      MISSING_ATTRIBUTE (context, error, element_name, "target");
      return FALSE;
    }

  value = g_strdup (target);
  key = g_strdup_printf ("%s.%s", ctx->namespace, name);
  if (!strchr (target, '.'))
    {
      const BasicTypeInfo *basic = parse_basic (target);
      if (!basic)
	{
	  g_free (value);
	  /* For non-basic types, re-qualify the interface */
	  value = g_strdup_printf ("%s.%s", ctx->namespace, target);
	}
    }
  g_hash_table_replace (ctx->aliases, key, value);

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
	  if (deprecated)
	    enum_->deprecated = TRUE;
	  else
	    enum_->deprecated = FALSE;

	  push_node (ctx, (GIrNode *) enum_);
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
	  
	  /* Assume properties are readable */
	  if (readable == NULL || strcmp (readable, "1") == 0)
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

	  iface = (GIrNodeInterface *)CURRENT_NODE (ctx);
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
	  
	  if (deprecated)
	    value_->deprecated = TRUE;
	  else
	    value_->deprecated = FALSE;

	  enum_ = (GIrNodeEnum *)CURRENT_NODE (ctx);
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

	  if (deprecated)
	    constant->deprecated = TRUE;
	  else
	    constant->deprecated = FALSE;

	  if (ctx->state == STATE_NAMESPACE)
	    {
	      push_node (ctx, (GIrNode *) constant);
	      ctx->current_module->entries = 
		g_list_append (ctx->current_module->entries, constant);
	    }
	  else
	    {
	      GIrNodeInterface *iface;

	      iface = (GIrNodeInterface *)CURRENT_NODE (ctx);
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

	  if (deprecated)
	    domain->deprecated = TRUE;
	  else
	    domain->deprecated = FALSE;

	  push_node (ctx, (GIrNode *) domain);
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
      const gchar *glib_type_struct;
      
      name = find_attribute ("name", attribute_names, attribute_values);
      typename = find_attribute ("glib:type-name", attribute_names, attribute_values);
      typeinit = find_attribute ("glib:get-type", attribute_names, attribute_values);
      glib_type_struct = find_attribute ("glib:type-struct", attribute_names, attribute_values);
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
          iface->glib_type_struct = g_strdup (glib_type_struct);
	  if (deprecated)
	    iface->deprecated = TRUE;
	  else
	    iface->deprecated = FALSE;
	  
	  push_node (ctx, (GIrNode *) iface);
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
      const gchar *glib_type_struct;
      const gchar *typename;
      const gchar *typeinit;
      const gchar *deprecated;
      const gchar *abstract;
      
      name = find_attribute ("name", attribute_names, attribute_values);
      parent = find_attribute ("parent", attribute_names, attribute_values);
      glib_type_struct = find_attribute ("glib:type-struct", attribute_names, attribute_values);
      typename = find_attribute ("glib:type-name", attribute_names, attribute_values);
      typeinit = find_attribute ("glib:get-type", attribute_names, attribute_values);
      deprecated = find_attribute ("deprecated", attribute_names, attribute_values);
      abstract = find_attribute ("abstract", attribute_names, attribute_values);
      
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
	  iface->glib_type_struct = g_strdup (glib_type_struct);
	  if (deprecated)
	    iface->deprecated = TRUE;
	  else
	    iface->deprecated = FALSE;

	  iface->abstract = abstract && strcmp (abstract, "1") == 0;

	  push_node (ctx, (GIrNode *) iface);
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
  const gchar *ctype;
  gboolean is_array;
  gboolean is_varargs;
  GIrNodeType *typenode;

  is_array = strcmp (element_name, "array") == 0;
  is_varargs = strcmp (element_name, "varargs") == 0;

  if (!(is_array || is_varargs || (strcmp (element_name, "type") == 0)))
    return FALSE;

  if (ctx->state == STATE_TYPE) 
    {
      ctx->type_depth++;
      ctx->type_stack = g_list_prepend (ctx->type_stack, ctx->type_parameters);
      ctx->type_parameters = NULL;
    } 
  else if (ctx->state == STATE_FUNCTION_PARAMETER ||
	   ctx->state == STATE_FUNCTION_RETURN || 
	   ctx->state == STATE_STRUCT_FIELD ||
	   ctx->state == STATE_UNION_FIELD ||
	   ctx->state == STATE_CLASS_PROPERTY ||
	   ctx->state == STATE_CLASS_FIELD ||
	   ctx->state == STATE_INTERFACE_FIELD ||
	   ctx->state == STATE_INTERFACE_PROPERTY ||
	   ctx->state == STATE_BOXED_FIELD ||
	   ctx->state == STATE_NAMESPACE_CONSTANT ||
	   ctx->state == STATE_CLASS_CONSTANT ||
	   ctx->state == STATE_INTERFACE_CONSTANT
	   )
    {
      state_switch (ctx, STATE_TYPE);
      ctx->type_depth = 1;
      if (is_varargs)
	{
	  switch (CURRENT_NODE (ctx)->type)
	    {
	    case G_IR_NODE_FUNCTION:
	    case G_IR_NODE_CALLBACK:
	      {
		GIrNodeFunction *func = (GIrNodeFunction *)CURRENT_NODE (ctx);
		func->is_varargs = TRUE;
	      }
	      break;
	    case G_IR_NODE_VFUNC:
	      {
		GIrNodeVFunc *vfunc = (GIrNodeVFunc *)CURRENT_NODE (ctx);
		vfunc->is_varargs = TRUE;
	      }
	      break;
	    /* list others individually rather than with default: so that compiler
	     * warns if new node types are added without adding them to the switch
	     */
	    case G_IR_NODE_INVALID:
	    case G_IR_NODE_ENUM:
	    case G_IR_NODE_FLAGS:
	    case G_IR_NODE_CONSTANT:
	    case G_IR_NODE_ERROR_DOMAIN:
	    case G_IR_NODE_PARAM:
	    case G_IR_NODE_TYPE:
	    case G_IR_NODE_PROPERTY:
	    case G_IR_NODE_SIGNAL:
	    case G_IR_NODE_VALUE:
	    case G_IR_NODE_FIELD:
	    case G_IR_NODE_XREF:
	    case G_IR_NODE_STRUCT:
	    case G_IR_NODE_BOXED:
	    case G_IR_NODE_OBJECT:
	    case G_IR_NODE_INTERFACE:
	    case G_IR_NODE_UNION:
	      g_assert_not_reached ();
	      break;
	    }
	}
      ctx->type_stack = NULL;
      ctx->type_parameters = NULL;
    }

  if (!ctx->current_typed)
    {
      g_set_error (error,
		   G_MARKUP_ERROR,
		   G_MARKUP_ERROR_INVALID_CONTENT,
		   "The element <type> is invalid here");
      return FALSE;
    }

  if (is_varargs)
    return TRUE;

  if (is_array) 
    {
      const char *zero;
      const char *len;
      const char *size;

      typenode = (GIrNodeType *)g_ir_node_new (G_IR_NODE_TYPE);

      typenode->tag = GI_TYPE_TAG_ARRAY;
      typenode->is_pointer = TRUE;
      typenode->is_array = TRUE;
      
      zero = find_attribute ("zero-terminated", attribute_names, attribute_values);
      len = find_attribute ("length", attribute_names, attribute_values);
      size = find_attribute ("fixed-size", attribute_names, attribute_values);
      
      typenode->zero_terminated = !(zero && strcmp (zero, "1") != 0);
      typenode->has_length = len != NULL;
      typenode->length = typenode->has_length ? atoi (len) : -1;
      
      typenode->has_size = size != NULL;
      typenode->size = typenode->has_size ? atoi (size) : -1;
    }
  else
    {
      int pointer_depth;
      name = find_attribute ("name", attribute_names, attribute_values);

      if (name == NULL)
	MISSING_ATTRIBUTE (context, error, element_name, "name");
      
      pointer_depth = 0;
      ctype = find_attribute ("c:type", attribute_names, attribute_values);
      if (ctype != NULL)
        {
          const char *cp = ctype + strlen(ctype) - 1;
          while (cp > ctype && *cp-- == '*')
            pointer_depth++;
        }
      
      if (ctx->current_typed->type == G_IR_NODE_PARAM &&
          ((GIrNodeParam *)ctx->current_typed)->out &&
          pointer_depth > 0)
        pointer_depth--;
      
      typenode = parse_type (ctx, name);

      /* A 'disguised' structure is one where the c:type is a typedef that
       * doesn't look like a pointer, but is internally.
       */
      if (typenode->tag == GI_TYPE_TAG_INTERFACE &&
	  is_disguised_structure (ctx, typenode->interface))
	pointer_depth++;

      if (pointer_depth > 0)
	typenode->is_pointer = TRUE;
    }

  ctx->type_parameters = g_list_append (ctx->type_parameters, typenode);
  
  return TRUE;
}

static void
end_type_top (ParseContext *ctx)
{
  GIrNodeType *typenode;

  if (!ctx->type_parameters)
    goto out;

  typenode = (GIrNodeType*)ctx->type_parameters->data;

  /* Default to pointer for unspecified containers */
  if (typenode->tag == GI_TYPE_TAG_ARRAY ||
      typenode->tag == GI_TYPE_TAG_GLIST ||
      typenode->tag == GI_TYPE_TAG_GSLIST)
    {
      if (typenode->parameter_type1 == NULL)
	typenode->parameter_type1 = parse_type (ctx, "any");
    }
  else if (typenode->tag == GI_TYPE_TAG_GHASH)
    {
      if (typenode->parameter_type1 == NULL)
	{
	  typenode->parameter_type1 = parse_type (ctx, "any");
	  typenode->parameter_type2 = parse_type (ctx, "any");
	}
    }

  switch (ctx->current_typed->type)
    {
    case G_IR_NODE_PARAM:
      {
	GIrNodeParam *param = (GIrNodeParam *)ctx->current_typed;
	param->type = typenode;
      }
      break;
    case G_IR_NODE_FIELD:
      {
	GIrNodeField *field = (GIrNodeField *)ctx->current_typed;
	field->type = typenode;
      }
      break;
    case G_IR_NODE_PROPERTY:
      {
	GIrNodeProperty *property = (GIrNodeProperty *) ctx->current_typed;
	property->type = typenode;
      }
      break;
    case G_IR_NODE_CONSTANT:
      {
	GIrNodeConstant *constant = (GIrNodeConstant *)ctx->current_typed;
	constant->type = typenode;
      }
      break;
    default:
      g_printerr("current node is %d\n", CURRENT_NODE (ctx)->type);
      g_assert_not_reached ();
    }
  g_list_free (ctx->type_parameters);

 out:  
  ctx->type_depth = 0;
  ctx->type_parameters = NULL;
  ctx->current_typed = NULL;
}

static void
end_type_recurse (ParseContext *ctx)
{
  GIrNodeType *parent;
  GIrNodeType *param = NULL;

  parent = (GIrNodeType *) ((GList*)ctx->type_stack->data)->data;
  if (ctx->type_parameters)
    param = (GIrNodeType *) ctx->type_parameters->data;

  if (parent->tag == GI_TYPE_TAG_ARRAY ||
      parent->tag == GI_TYPE_TAG_GLIST ||
      parent->tag == GI_TYPE_TAG_GSLIST)
    {
      g_assert (param != NULL);

      if (parent->parameter_type1 == NULL)
        parent->parameter_type1 = param;
      else
        g_assert_not_reached ();
    }
  else if (parent->tag == GI_TYPE_TAG_GHASH)
    {
      g_assert (param != NULL);

      if (parent->parameter_type1 == NULL)
        parent->parameter_type1 = param;
      else if (parent->parameter_type2 == NULL)
        parent->parameter_type2 = param;
      else
        g_assert_not_reached ();
    }
  g_list_free (ctx->type_parameters);
  ctx->type_parameters = (GList *)ctx->type_stack->data;
  ctx->type_stack = g_list_delete_link (ctx->type_stack, ctx->type_stack);
}

static void
end_type (ParseContext *ctx)
{
  if (ctx->type_depth == 1)
    {
      end_type_top (ctx);
      state_switch (ctx, ctx->prev_state);
    }
  else
    {
      end_type_recurse (ctx);
      ctx->type_depth--;
    }
}

static gboolean
start_attribute (GMarkupParseContext *context,
                 const gchar         *element_name,
                 const gchar        **attribute_names,
                 const gchar        **attribute_values,
                 ParseContext       *ctx,
                 GError             **error)
{
  const gchar *name;
  const gchar *value;
  GIrNode *curnode;

  if (strcmp (element_name, "attribute") != 0 || ctx->node_stack == NULL)
    return FALSE;

  name = find_attribute ("name", attribute_names, attribute_values);
  value = find_attribute ("value", attribute_names, attribute_values);

  if (name == NULL)
    {
      MISSING_ATTRIBUTE (context, error, element_name, "name");
      return FALSE;
    }
  if (value == NULL)
    {
      MISSING_ATTRIBUTE (context, error, element_name, "value");
      return FALSE;
    }

  state_switch (ctx, STATE_ATTRIBUTE);

  curnode = CURRENT_NODE (ctx);

  g_hash_table_insert (curnode->attributes, g_strdup (name), g_strdup (value));

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
      const gchar  *transfer;

      param = (GIrNodeParam *)g_ir_node_new (G_IR_NODE_PARAM);
      param->in = FALSE;
      param->out = FALSE;
      param->retval = TRUE;

      ctx->current_typed = (GIrNode*) param;

      state_switch (ctx, STATE_FUNCTION_RETURN);

      transfer = find_attribute ("transfer-ownership", attribute_names, attribute_values);
      parse_param_transfer (param, transfer);

      switch (CURRENT_NODE (ctx)->type)
	{
	case G_IR_NODE_FUNCTION:
	case G_IR_NODE_CALLBACK:
	  {
	    GIrNodeFunction *func = (GIrNodeFunction *)CURRENT_NODE (ctx);
	    func->result = param;
	  }
	  break;
	case G_IR_NODE_SIGNAL:
	  {
	    GIrNodeSignal *signal = (GIrNodeSignal *)CURRENT_NODE (ctx);
	    signal->result = param;
	  }
	  break;
	case G_IR_NODE_VFUNC:
	  {
	    GIrNodeVFunc *vfunc = (GIrNodeVFunc *)CURRENT_NODE (ctx);
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
start_implements (GMarkupParseContext *context,
		  const gchar         *element_name,
		  const gchar        **attribute_names,
		  const gchar        **attribute_values,
		  ParseContext       *ctx,
		  GError             **error)
{
  GIrNodeInterface *iface;
  const char *name;

  if (strcmp (element_name, "implements") != 0 ||
      !(ctx->state == STATE_CLASS))
    return FALSE;

  state_switch (ctx, STATE_IMPLEMENTS);
  
  name = find_attribute ("name", attribute_names, attribute_values);
  if (name == NULL)
    {
      MISSING_ATTRIBUTE (context, error, element_name, "name");
      return FALSE;
    }
      
  iface = (GIrNodeInterface *)CURRENT_NODE (ctx);
  iface->interfaces = g_list_append (iface->interfaces, g_strdup (name));

  return TRUE;
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

	  iface = (GIrNodeInterface *)CURRENT_NODE (ctx);
	  iface->members = g_list_append (iface->members, signal);

	  push_node (ctx, (GIrNode *)signal);
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
  if (strcmp (element_name, "virtual-method") == 0 &&
      (ctx->state == STATE_CLASS ||
       ctx->state == STATE_INTERFACE))
    {
      const gchar *name;
      const gchar *must_chain_up;
      const gchar *override;
      const gchar *is_class_closure;
      const gchar *offset;
      const gchar *invoker;
      
      name = find_attribute ("name", attribute_names, attribute_values);
      must_chain_up = find_attribute ("must-chain-up", attribute_names, attribute_values);	  
      override = find_attribute ("override", attribute_names, attribute_values);
      is_class_closure = find_attribute ("is-class-closure", attribute_names, attribute_values);
      offset = find_attribute ("offset", attribute_names, attribute_values);
      invoker = find_attribute ("invoker", attribute_names, attribute_values);
      
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

	  vfunc->invoker = g_strdup (invoker);

	  iface = (GIrNodeInterface *)CURRENT_NODE (ctx);
	  iface->members = g_list_append (iface->members, vfunc);

	  push_node (ctx, (GIrNode *)vfunc);
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
      (ctx->state == STATE_NAMESPACE ||
       ctx->state == STATE_UNION ||
       ctx->state == STATE_STRUCT ||
       ctx->state == STATE_CLASS))
    {
      const gchar *name;
      const gchar *deprecated;
      const gchar *disguised;
      const gchar *gtype_name;
      const gchar *gtype_init;
      const gchar *gtype_struct;
      GIrNodeStruct *struct_;
      
      name = find_attribute ("name", attribute_names, attribute_values);
      deprecated = find_attribute ("deprecated", attribute_names, attribute_values);
      disguised = find_attribute ("disguised", attribute_names, attribute_values);
      gtype_name = find_attribute ("glib:type-name", attribute_names, attribute_values);
      gtype_init = find_attribute ("glib:get-type", attribute_names, attribute_values);
      gtype_struct = find_attribute ("glib:is-gtype-struct-for", attribute_names, attribute_values);

      if (name == NULL && ctx->node_stack == NULL)
	{
	  MISSING_ATTRIBUTE (context, error, element_name, "name");
	  return FALSE;
	}
      if ((gtype_name == NULL && gtype_init != NULL))
	{
	  MISSING_ATTRIBUTE (context, error, element_name, "glib:type-name");
	  return FALSE;
	}
      if ((gtype_name != NULL && gtype_init == NULL))
	{
	  MISSING_ATTRIBUTE (context, error, element_name, "glib:get-type");
	  return FALSE;
	}

      struct_ = (GIrNodeStruct *) g_ir_node_new (G_IR_NODE_STRUCT);
      
      ((GIrNode *)struct_)->name = g_strdup (name ? name : "");
      if (deprecated)
	struct_->deprecated = TRUE;
      else
	struct_->deprecated = FALSE;

      if (disguised && strcmp (disguised, "1") == 0)
	struct_->disguised = TRUE;
      
      struct_->is_gtype_struct = gtype_struct != NULL;

      struct_->gtype_name = g_strdup (gtype_name);
      struct_->gtype_init = g_strdup (gtype_init);

      if (ctx->node_stack == NULL)
        ctx->current_module->entries = 
          g_list_append (ctx->current_module->entries, struct_);
      push_node (ctx, (GIrNode *)struct_);
      
      state_switch (ctx, STATE_STRUCT);
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
      (ctx->state == STATE_NAMESPACE ||
       ctx->state == STATE_UNION ||
       ctx->state == STATE_STRUCT ||
       ctx->state == STATE_CLASS))
    {
      const gchar *name;
      const gchar *deprecated;
      const gchar *typename;
      const gchar *typeinit;
      
      name = find_attribute ("name", attribute_names, attribute_values);
      deprecated = find_attribute ("deprecated", attribute_names, attribute_values);
      typename = find_attribute ("glib:type-name", attribute_names, attribute_values);
      typeinit = find_attribute ("glib:get-type", attribute_names, attribute_values);
      
      if (name == NULL && ctx->node_stack == NULL)
	MISSING_ATTRIBUTE (context, error, element_name, "name");
      else
	{
	  GIrNodeUnion *union_;

	  union_ = (GIrNodeUnion *) g_ir_node_new (G_IR_NODE_UNION);
	  
	  ((GIrNode *)union_)->name = g_strdup (name ? name : "");
	  union_->gtype_name = g_strdup (typename);
	  union_->gtype_init = g_strdup (typeinit);
	  if (deprecated)
	    union_->deprecated = TRUE;
	  else
	    union_->deprecated = FALSE;

          if (ctx->node_stack == NULL)
            ctx->current_module->entries = 
              g_list_append (ctx->current_module->entries, union_);
	  push_node (ctx, (GIrNode *)union_);
	  
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
	  ((GIrNodeUnion *)CURRENT_NODE (ctx))->discriminator_type 
	    = parse_type (ctx, type);
	  ((GIrNodeUnion *)CURRENT_NODE (ctx))->discriminator_offset 
	    = atoi (offset);
	}
      
      return TRUE;
    }

  return FALSE;
}

static gboolean
parse_include (GMarkupParseContext *context,
	       ParseContext        *ctx,
	       const char          *name,
	       const char          *version,
	       GError             **error)
{
  gchar *buffer;
  gsize length;
  gchar *girpath, *girname;
  gboolean success = FALSE;
  GList *modules;
  GList *l;

  for (l = ctx->parser->parsed_modules; l; l = l->next)
    {
      GIrModule *m = l->data;

      if (strcmp (m->name, name) == 0)
	{
	  if (strcmp (m->version, version) == 0)
	    {
	      ctx->include_modules = g_list_prepend (ctx->include_modules, m);

	      return TRUE;
	    }
	  else
	    {
	      g_set_error (error,
			   G_MARKUP_ERROR,
			   G_MARKUP_ERROR_INVALID_CONTENT,
			   "Module '%s' imported with conflicting versions '%s' and '%s'",
			   name, m->version, version);
	      return FALSE;
	    }
	}
    }

  girname = g_strdup_printf ("%s-%s.gir", name, version);
  girpath = locate_gir (ctx->parser, girname);

  if (girpath == NULL)
    {
      g_set_error (error,
		   G_MARKUP_ERROR,
		   G_MARKUP_ERROR_INVALID_CONTENT,
		   "Could not find GIR file '%s'; check XDG_DATA_DIRS or use --includedir",
		   girname);
      g_free (girname);
      return FALSE;
    }
  g_free (girname);

  g_debug ("Parsing include %s", girpath);

  if (!g_file_get_contents (girpath, &buffer, &length, error))
    {
      g_free (girpath);
      return FALSE;
    }
  g_free (girpath);

  modules = g_ir_parser_parse_string (ctx->parser, name, buffer, length, error);
  success = error != NULL;

  ctx->include_modules = g_list_concat (ctx->include_modules,
					modules);

  g_free (buffer);

  return success;
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
      if (start_type (context, element_name,
		      attribute_names, attribute_values,
		      ctx, error))
	goto out;
      else if (start_attribute (context, element_name,
                                attribute_names, attribute_values,
                                ctx, error))
        goto out;
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
      break;

    case 'i':
      if (strcmp (element_name, "include") == 0 &&
	  ctx->state == STATE_REPOSITORY)
	{
	  const gchar *name;
	  const gchar *version;

	  name = find_attribute ("name", attribute_names, attribute_values);
	  version = find_attribute ("version", attribute_names, attribute_values);

	  if (name == NULL)
	    {
	      MISSING_ATTRIBUTE (context, error, element_name, "name");
	      break;
	    }
	  if (version == NULL)
	    {
	      MISSING_ATTRIBUTE (context, error, element_name, "version");
	      break;
	    }

	  if (!parse_include (context, ctx, name, version, error))
	    break;

	  ctx->dependencies = g_list_prepend (ctx->dependencies,
					      g_strdup_printf ("%s-%s", name, version));


	  state_switch (ctx, STATE_INCLUDE);
	  goto out;
	}
      if (start_interface (context, element_name, 
			   attribute_names, attribute_values,
			   ctx, error))
	goto out;
      else if (start_implements (context, element_name,
				 attribute_names, attribute_values,
				 ctx, error))
	goto out;
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
	  const gchar *name, *version, *shared_library, *cprefix;

	  if (ctx->current_module != NULL)
	    {
	      g_set_error (error,
			   G_MARKUP_ERROR,
			   G_MARKUP_ERROR_INVALID_CONTENT,
			   "Only one <namespace/> element is currently allowed per <repository/>");
	      goto out;
	    }

	  name = find_attribute ("name", attribute_names, attribute_values);
	  version = find_attribute ("version", attribute_names, attribute_values);
	  shared_library = find_attribute ("shared-library", attribute_names, attribute_values);
	  cprefix = find_attribute ("c:prefix", attribute_names, attribute_values);

	  if (name == NULL)
	    MISSING_ATTRIBUTE (context, error, element_name, "name");
	  else if (version == NULL)
	    MISSING_ATTRIBUTE (context, error, element_name, "version");
	  else
	    {
	      GList *l;

	      if (strcmp (name, ctx->namespace) != 0)
		g_set_error (error,
			     G_MARKUP_ERROR,
			     G_MARKUP_ERROR_INVALID_CONTENT,
			     "<namespace/> name element '%s' doesn't match file name '%s'",
			     name, ctx->namespace);

	      ctx->current_module = g_ir_module_new (name, version, shared_library, cprefix);

	      ctx->current_module->aliases = ctx->aliases;
	      ctx->aliases = NULL;
	      ctx->current_module->disguised_structures = ctx->disguised_structures;
	      ctx->disguised_structures = NULL;

	      for (l = ctx->include_modules; l; l = l->next)
		g_ir_module_add_include_module (ctx->current_module, l->data);

	      g_list_free (ctx->include_modules);
	      ctx->include_modules = NULL;

	      ctx->modules = g_list_append (ctx->modules, ctx->current_module);
	      ctx->current_module->dependencies = ctx->dependencies;

	      state_switch (ctx, STATE_NAMESPACE);
	      goto out;
	    }
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
      else if (strcmp (element_name, "prerequisite") == 0 &&
	       ctx->state == STATE_INTERFACE)
	{
	  const gchar *name;

	  name = find_attribute ("name", attribute_names, attribute_values);

	  state_switch (ctx, STATE_PREREQUISITE);

	  if (name == NULL)
	    MISSING_ATTRIBUTE (context, error, element_name, "name");
	  else
	    {  
	      GIrNodeInterface *iface;

	      iface = (GIrNodeInterface *)CURRENT_NODE(ctx);
	      iface->prerequisites = g_list_append (iface->prerequisites, g_strdup (name));
	    }
	  goto out;
	}
      else if (strcmp (element_name, "package") == 0 &&
          ctx->state == STATE_REPOSITORY)
        {
          state_switch (ctx, STATE_PACKAGE);
          goto out;
        }
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
      if (start_type (context, element_name,
		      attribute_names, attribute_values,
		      ctx, error))
	goto out;
      break;
    }

  if (ctx->state != STATE_UNKNOWN)
    {
      state_switch (ctx, STATE_UNKNOWN);
      ctx->unknown_depth = 1;
    }
  else
    {
      ctx->unknown_depth += 1;
    }
  
 out:
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
state_switch_end_struct_or_union (GMarkupParseContext *context,
                                  ParseContext *ctx,
                                  const gchar *element_name,
                                  GError **error)
{
  pop_node (ctx);
  if (ctx->node_stack == NULL)
    {
      state_switch (ctx, STATE_NAMESPACE);
    }
  else 
    {
      if (CURRENT_NODE (ctx)->type == G_IR_NODE_STRUCT)
        state_switch (ctx, STATE_STRUCT);
      else if (CURRENT_NODE (ctx)->type == G_IR_NODE_UNION)
        state_switch (ctx, STATE_UNION);
      else if (CURRENT_NODE (ctx)->type == G_IR_NODE_OBJECT)
        state_switch (ctx, STATE_CLASS);
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
          return FALSE;
        }
    }
  return TRUE;
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

    case STATE_INCLUDE:
      if (require_end_element (context, ctx, "include", element_name, error))
	{
          state_switch (ctx, STATE_REPOSITORY);
        }
      break;
      
    case STATE_PACKAGE:
      if (require_end_element (context, ctx, "package", element_name, error))
        {
          state_switch (ctx, STATE_REPOSITORY);
        }
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
      {
        pop_node (ctx);
	if (ctx->node_stack == NULL)
	  {
	    state_switch (ctx, STATE_NAMESPACE);
	  }
	else 
	  {
	    if (CURRENT_NODE (ctx)->type == G_IR_NODE_INTERFACE)
	      state_switch (ctx, STATE_INTERFACE);
	    else if (CURRENT_NODE (ctx)->type == G_IR_NODE_OBJECT) 
	      state_switch (ctx, STATE_CLASS);
	    else if (CURRENT_NODE (ctx)->type == G_IR_NODE_BOXED)
	      state_switch (ctx, STATE_BOXED);
	    else if (CURRENT_NODE (ctx)->type == G_IR_NODE_STRUCT)
	      state_switch (ctx, STATE_STRUCT);
	    else if (CURRENT_NODE (ctx)->type == G_IR_NODE_UNION)
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
	  pop_node (ctx);
	  state_switch (ctx, STATE_NAMESPACE);
	}
      break;

    case STATE_ERRORDOMAIN:
      if (require_end_element (context, ctx, "errordomain", element_name, error))
	{
	  pop_node (ctx);
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
	  pop_node (ctx);
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
	  pop_node (ctx);
	  state_switch (ctx, STATE_NAMESPACE);
	}
      break;

    case STATE_BOXED:
      if (require_end_element (context, ctx, "glib:boxed", element_name, error))
	{
	  pop_node (ctx);
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
	  state_switch_end_struct_or_union (context, ctx, element_name, error);
	}
      break;

    case STATE_UNION_FIELD:
      if (strcmp ("type", element_name) == 0)
	break;
      if (require_end_element (context, ctx, "field", element_name, error))
	{
	  state_switch (ctx, STATE_UNION);
	}
      break;

    case STATE_UNION:
      if (require_end_element (context, ctx, "union", element_name, error))
	{
	  state_switch_end_struct_or_union (context, ctx, element_name, error);
	}
      break;
    case STATE_IMPLEMENTS:
      if (strcmp ("interface", element_name) == 0)
	break;
      if (require_end_element (context, ctx, "implements", element_name, error))
        state_switch (ctx, STATE_CLASS);
      break;
    case STATE_PREREQUISITE:
      if (require_end_element (context, ctx, "prerequisite", element_name, error))
        state_switch (ctx, STATE_INTERFACE);
      break;
    case STATE_NAMESPACE_CONSTANT:
    case STATE_CLASS_CONSTANT:
    case STATE_INTERFACE_CONSTANT:
      if (strcmp ("type", element_name) == 0)
	break;
      if (require_end_element (context, ctx, "constant", element_name, error))
	{
	  switch (ctx->state)
	    {
	    case STATE_NAMESPACE_CONSTANT:
	  	  pop_node (ctx);
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
    case STATE_TYPE:
      if ((strcmp ("type", element_name) == 0) || (strcmp ("array", element_name) == 0) ||
	  (strcmp ("varargs", element_name) == 0))
	{
	  end_type (ctx);
	  break;
	}
    case STATE_ATTRIBUTE:
      if (strcmp ("attribute", element_name) == 0)
        {
          state_switch (ctx, ctx->prev_state);
        }
      break;

    case STATE_UNKNOWN:
      ctx->unknown_depth -= 1;
      if (ctx->unknown_depth == 0)
        state_switch (ctx, ctx->prev_state);
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

  for (m = ctx->modules; m; m = m->next)
    g_ir_module_free (m->data);
  g_list_free (ctx->modules);
  ctx->modules = NULL;
  
  ctx->current_module = NULL;
}

static GList *
post_filter_toplevel_varargs_functions (GList *list, 
					GList **varargs_callbacks_out)
{
  GList *iter;
  GList *varargs_callbacks = *varargs_callbacks_out;
  
  iter = list;
  while (iter)
    {
      GList *link = iter;
      GIrNode *node = iter->data;
      
      iter = iter->next;
      
      if (node->type == G_IR_NODE_FUNCTION)
	{
	  if (((GIrNodeFunction*)node)->is_varargs)
	    {
	      list = g_list_delete_link (list, link);
	    }
	}
      if (node->type == G_IR_NODE_CALLBACK)
	{
	  if (((GIrNodeFunction*)node)->is_varargs)
	    {
	      varargs_callbacks = g_list_append (varargs_callbacks,
					 	 node);
	      list = g_list_delete_link (list, link);
	    }
	}
    }
  
  *varargs_callbacks_out = varargs_callbacks;
  
  return list;
}

static GList *
post_filter_varargs_functions (GList *list, GList ** varargs_callbacks_out)
{
  GList *iter;
  GList *varargs_callbacks;
  
  list = post_filter_toplevel_varargs_functions (list, varargs_callbacks_out);
  
  varargs_callbacks = *varargs_callbacks_out;

  iter = list;
  while (iter)
    {
      GList *link = iter;
      GIrNode *node = iter->data;
      
      iter = iter->next;
      
      if (node->type == G_IR_NODE_FUNCTION)
	{
	  GList *param;
	  gboolean function_done = FALSE;
	  
	  for (param = ((GIrNodeFunction *)node)->parameters;
	       param;
	       param = param->next)
	    {
	      GIrNodeParam *node = (GIrNodeParam *)param->data;
	      
	      if (function_done)
		break;

	      if (node->type->is_interface)
		{
		  GList *callback;
		  for (callback = varargs_callbacks;
		       callback;
		       callback = callback->next)
		    {
		      if (!strcmp (node->type->interface,
				   ((GIrNode *)varargs_callbacks->data)->name))
			{
			  list = g_list_delete_link (list, link);
			  function_done = TRUE;
			  break;
			}
		    }
		}
	    }
	}
    }
  
  *varargs_callbacks_out = varargs_callbacks;
  
  return list;
}

static void
post_filter (GIrModule *module)
{
  GList *iter;
  GList *varargs_callbacks = NULL;
  
  module->entries = post_filter_varargs_functions (module->entries,
						   &varargs_callbacks);
  iter = module->entries;
  while (iter)
    {
      GIrNode *node = iter->data;
      
      iter = iter->next;
      
      if (node->type == G_IR_NODE_OBJECT || 
	  node->type == G_IR_NODE_INTERFACE) 
	{
	  GIrNodeInterface *iface = (GIrNodeInterface*)node;
	  iface->members = post_filter_varargs_functions (iface->members,
							  &varargs_callbacks);
	}
      else if (node->type == G_IR_NODE_BOXED)
	{
	  GIrNodeBoxed *boxed = (GIrNodeBoxed*)node;
	  boxed->members = post_filter_varargs_functions (boxed->members,
							  &varargs_callbacks);
	}
      else if (node->type == G_IR_NODE_STRUCT)
	{
	  GIrNodeStruct *iface = (GIrNodeStruct*)node;
	  iface->members = post_filter_varargs_functions (iface->members,
							  &varargs_callbacks);
	}
      else if (node->type == G_IR_NODE_UNION)
	{
	  GIrNodeUnion *iface = (GIrNodeUnion*)node;
	  iface->members = post_filter_varargs_functions (iface->members,
							  &varargs_callbacks);
	}
    }
  g_list_free (varargs_callbacks);
}

/**
 * g_ir_parser_parse_string:
 * @parser: a #GIrParser
 * @namespace: the namespace of the string
 * @buffer: the data containing the XML
 * @length: length of the data
 * @error: return location for a #GError, or %NULL
 *
 * Parse a string that holds a complete GIR XML file, and return a list of a
 * a #GirModule for each &lt;namespace/&gt; element within the file.
 *
 * Returns: a newly allocated list of #GIrModule. The modules themselves
 *  are owned by the #GIrParser and will be freed along with the parser.
 */
GList *
g_ir_parser_parse_string (GIrParser           *parser,
			  const gchar         *namespace,
			  const gchar         *buffer,
			  gssize               length,
			  GError             **error)
{
  ParseContext ctx = { 0 };
  GMarkupParseContext *context;

  ctx.parser = parser;
  ctx.state = STATE_START;
  ctx.namespace = namespace;
  ctx.include_modules = NULL;
  ctx.aliases = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
  ctx.disguised_structures = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  ctx.type_depth = 0;
  ctx.dependencies = NULL;
  ctx.current_module = NULL;

  context = g_markup_parse_context_new (&firstpass_parser, 0, &ctx, NULL);

  if (!g_markup_parse_context_parse (context, buffer, length, error))
    goto out;

  if (!g_markup_parse_context_end_parse (context, error))
    goto out;

  g_markup_parse_context_free (context);
  
  context = g_markup_parse_context_new (&markup_parser, 0, &ctx, NULL);
  if (!g_markup_parse_context_parse (context, buffer, length, error))
    goto out;

  if (!g_markup_parse_context_end_parse (context, error))
    goto out;

  parser->parsed_modules = g_list_concat (g_list_copy (ctx.modules),
					  parser->parsed_modules);

 out:

  if (ctx.modules == NULL)
    {
      /* An error occurred before we created a module, so we haven't
       * transferred ownership of these hash tables to the module.
       */
      if (ctx.aliases != NULL)
	g_hash_table_destroy (ctx.aliases);
      if (ctx.disguised_structures != NULL)
	g_hash_table_destroy (ctx.disguised_structures);
      g_list_free (ctx.include_modules);
    }
  
  g_markup_parse_context_free (context);
  
  return ctx.modules;
}

/**
 * g_ir_parser_parse_file:
 * @parser: a #GIrParser
 * @filename: filename to parse
 * @error: return location for a #GError, or %NULL
 *
 * Parse GIR XML file, and return a list of a a #GirModule for each
 * &lt;namespace/&gt; element within the file.
 *
 * Returns: a newly allocated list of #GIrModule. The modules themselves
 *  are owned by the #GIrParser and will be freed along with the parser.
 */
GList *
g_ir_parser_parse_file (GIrParser   *parser,
			const gchar *filename,
			GError     **error)
{
  gchar *buffer;
  gsize length;
  GList *modules;
  GList *iter;
  const char *slash;
  char *dash;
  char *namespace;

  if (!g_str_has_suffix (filename, ".gir"))
    {
      g_set_error (error,
		   G_MARKUP_ERROR,
		   G_MARKUP_ERROR_INVALID_CONTENT,
		   "Expected filename to end with '.gir'");
      return NULL;
    }

  g_debug ("[parsing] filename %s", filename);

  slash = g_strrstr (filename, "/");
  if (!slash)
    namespace = g_strdup (filename);
  else
    namespace = g_strdup (slash+1);
  namespace[strlen(namespace)-4] = '\0';

  /* Remove version */
  dash = strstr (namespace, "-");
  if (dash != NULL)
    *dash = '\0';

  if (!g_file_get_contents (filename, &buffer, &length, error))
    return NULL;
  
  modules = g_ir_parser_parse_string (parser, namespace, buffer, length, error);

  for (iter = modules; iter; iter = iter->next) 
    {
      post_filter ((GIrModule*)iter->data);
    }

  g_free (namespace);

  g_free (buffer);

  return modules;
}


