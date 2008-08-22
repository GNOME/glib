/* -*- Mode: C; c-file-style: "gnu"; -*- */
/* GObject introspection: IDL generator
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

#include <errno.h>
#include <dlfcn.h>
#include <string.h>

#include <glib.h>
#include <glib-object.h>
#include <glib/gstdio.h>

#include "girepository.h"
#include "gtypelib.h"

/* FIXME: Avoid global */
static gchar *output = NULL;

static void 
write_type_name (const gchar *namespace,
		 GIBaseInfo  *info,
		 FILE        *file)
{
  if (strcmp (namespace, g_base_info_get_namespace (info)) != 0)
    g_fprintf (file, "%s.", g_base_info_get_namespace (info));

  g_fprintf (file, "%s", g_base_info_get_name (info));
}

static void
write_type_info (const gchar *namespace,
		 GITypeInfo  *info, 
		 FILE        *file)
{
  gint tag;
  gint i;
  GITypeInfo *type;
  
  const gchar* basic[] = {
    "none", 
    "boolean", 
    "int8", 
    "uint8", 
    "int16", 
    "uint16", 
    "int32", 
    "uint32", 
    "int64", 
    "uint64", 
    "int",
    "uint",
    "long",
    "ulong",
    "ssize",
    "size",
    "float", 
    "double", 
    "utf8",
    "filename"
  };

  tag = g_type_info_get_tag (info);

  if (tag < 20)
    g_fprintf (file, "%s", basic[tag]);
  else if (tag == 20)
    {
      gint length;

      type = g_type_info_get_param_type (info, 0);
      write_type_info (namespace, type, file);
      g_fprintf (file, "["); 

      length = g_type_info_get_array_length (info);
      
      if (length >= 0)
	g_fprintf (file, "length=%d", length);
      
      if (g_type_info_is_zero_terminated (info))
	g_fprintf (file, "%szero-terminated=1", length >= 0 ? "," : "");
      
     g_fprintf (file, "]"); 
      g_base_info_unref ((GIBaseInfo *)type);
    }
  else if (tag == 21)
    {
      GIBaseInfo *iface = g_type_info_get_interface (info);
      write_type_name (namespace, iface, file);
      g_base_info_unref (iface);
    }
  else if (tag == 22)
    {
      type = g_type_info_get_param_type (info, 0);
      g_fprintf (file, "GList");
      if (type)
	{
	  g_fprintf (file, "<"); 
	  write_type_info (namespace, type, file);
	  g_fprintf (file, ">"); 
	  g_base_info_unref ((GIBaseInfo *)type);
	}
    }
  else if (tag == 23)
    {
      type = g_type_info_get_param_type (info, 0);
      g_fprintf (file, "GSList");
      if (type)
	{
	  g_fprintf (file, "<"); 
	  write_type_info (namespace, type, file);
	  g_fprintf (file, ">"); 
	  g_base_info_unref ((GIBaseInfo *)type);
	}
    }
  else if (tag == 24)
    {
      type = g_type_info_get_param_type (info, 0);
      g_fprintf (file, "GHashTable");
      if (type)
	{
	  g_fprintf (file, "<"); 
	  write_type_info (namespace, type, file);
	  g_base_info_unref ((GIBaseInfo *)type);
	  type = g_type_info_get_param_type (info, 1);
	  g_fprintf (file, ",");
	  write_type_info (namespace, type, file);
	  g_fprintf (file, ">"); 
	  g_base_info_unref ((GIBaseInfo *)type);
	}
    }
  else if (tag == 25) 
    {
      gint n;

      g_fprintf (file, "GError");
      n = g_type_info_get_n_error_domains (info);
      if (n > 0)
	{
	  g_fprintf (file, "<"); 
	  for (i = 0; i < n; i++)
	    {
	      GIErrorDomainInfo *ed = g_type_info_get_error_domain (info, i);
	      if (i > 0)
		g_fprintf (file, ",");
	      write_type_name (namespace, (GIBaseInfo *)ed, file);
	      g_base_info_unref ((GIBaseInfo *)ed);
	    }
	  g_fprintf (file, ">");
	}
    }
}

static void
write_constant_value (const gchar *namespace, 
		      GITypeInfo *info,
		      GArgument *argument,
		      FILE *file);

static void
write_field_info (const gchar *namespace,
		  GIFieldInfo *info,
		  GIConstantInfo *branch,
		  FILE        *file)
{
  const gchar *name;
  GIFieldInfoFlags flags;
  gint size;
  gint offset;
  GITypeInfo *type;
  GArgument value; 

  name = g_base_info_get_name ((GIBaseInfo *)info);
  flags = g_field_info_get_flags (info);
  size = g_field_info_get_size (info);
  offset = g_field_info_get_offset (info);

  g_fprintf (file, 
	     "      <field name=\"%s\" readable=\"%s\" writable=\"%s\"",
	     name, 
	     flags & GI_FIELD_IS_READABLE ? "1" : "0", 
	     flags & GI_FIELD_IS_WRITABLE ? "1" : "0");
  if (size)
    g_fprintf (file, " bits=\"%d\"", size);

  g_fprintf (file, " offset=\"%d\"", offset);

  type = g_field_info_get_type (info);

  if (branch)
    {
      g_fprintf (file, " branch=\"");
      type = g_constant_info_get_type (branch);
      g_constant_info_get_value (branch, &value);
      write_constant_value (namespace, type, &value, file);
      g_fprintf (file, "\"");
    }

  g_fprintf (file,">\n");

  g_fprintf (file, "        <type name=\"");

  write_type_info (namespace, type, file);
  g_base_info_unref ((GIBaseInfo *)type);

  g_fprintf (file, "\"/>\n");

  g_fprintf (file,  "      </field>\n");

}

static void 
write_callable_info (const gchar    *namespace,
		     GICallableInfo *info,
		     FILE           *file,
		     gint            indent)
{
  GITypeInfo *type;
  gint i;

  type = g_callable_info_get_return_type (info);

  if (g_type_info_is_pointer (type))
    {
      switch (g_callable_info_get_caller_owns (info))
	{
	case GI_TRANSFER_NOTHING:
	  break;
	case GI_TRANSFER_CONTAINER:
	  g_fprintf (file, " transfer=\"shallow\"");
	  break;
	case GI_TRANSFER_EVERYTHING:
	  g_fprintf (file, " transfer=\"full\"");
	  break;
	default:
	  g_assert_not_reached ();
	}
    }

  g_fprintf (file, ">\n");

  g_fprintf (file, "%*s  <return-value>\n", indent, "");
  
  if (g_callable_info_may_return_null (info))
    g_fprintf (file, " null-ok=\"1\"");

  g_fprintf (file, "%*s  <type name=\"", indent + 2, "");

  write_type_info (namespace, type, file);

  g_fprintf (file, "\"/>\n");

  g_fprintf (file, "%*s  </return-value>\n", indent, "");
	
  if (g_callable_info_get_n_args (info) <= 0)
    return;

  g_fprintf (file, "%*s  <parameters>\n", indent, "");
  for (i = 0; i < g_callable_info_get_n_args (info); i++)
    {
      GIArgInfo *arg = g_callable_info_get_arg (info, i);
      
      g_fprintf (file, "%*s    <parameter name=\"%s\"",
		 indent, "", g_base_info_get_name ((GIBaseInfo *) arg));
      
      switch (g_arg_info_get_ownership_transfer (arg))
	{
	case GI_TRANSFER_NOTHING:
	  break;
	case GI_TRANSFER_CONTAINER:
	  g_fprintf (file, " transfer=\"shallow\"");
	  break;
	case GI_TRANSFER_EVERYTHING:
	  g_fprintf (file, " transfer=\"full\"");
	  break;
	default:
	  g_assert_not_reached ();
	}
      
      g_fprintf (file, " direction=\"");
      switch (g_arg_info_get_direction (arg))
	{
	case GI_DIRECTION_IN:
	  g_fprintf (file, "in");
	  break;
	case GI_DIRECTION_OUT:
	  g_fprintf (file, "out");
	  break;
	case GI_DIRECTION_INOUT:
	  g_fprintf (file, "inout");
	  break;
	}
      g_fprintf (file, "\"");
      
      if (g_arg_info_may_be_null (arg))
	g_fprintf (file, " null-ok=\"1\"");
      
      if (g_arg_info_is_dipper (arg))
	g_fprintf (file, " dipper=\"1\"");
      
      if (g_arg_info_is_return_value (arg))
	g_fprintf (file, " retval=\"1\"");
      
      if (g_arg_info_is_optional (arg))
	g_fprintf (file, " optional=\"1\"");
      
      g_fprintf (file, ">\n");
      
      g_fprintf (file, "%*s    <type name=\"", indent+2, "");

      type = g_arg_info_get_type (arg);
      write_type_info (namespace, type, file);

      g_fprintf (file, "\"/>\n");
      
      g_fprintf (file, "%*s    </parameter>\n", indent, "");

      g_base_info_unref ((GIBaseInfo *)arg);
    }
  
  g_fprintf (file, "%*s  </parameters>\n", indent, "");
  g_base_info_unref ((GIBaseInfo *)type);
}

static void
write_function_info (const gchar    *namespace,
		     GIFunctionInfo *info,
		     FILE           *file,
		     gint            indent)
{
  GIFunctionInfoFlags flags;
  const gchar *tag;
  const gchar *name;
  const gchar *symbol;
  gboolean deprecated;

  flags = g_function_info_get_flags (info);
  name = g_base_info_get_name ((GIBaseInfo *)info);
  symbol = g_function_info_get_symbol (info);
  deprecated = g_base_info_is_deprecated ((GIBaseInfo *)info);

  if (flags & GI_FUNCTION_IS_CONSTRUCTOR)
    tag = "constructor";
  else if (flags & GI_FUNCTION_IS_METHOD)
    tag = "method";
  else
    tag = "function";
	
  g_fprintf (file, "%*s<%s name=\"%s\" c:identifier=\"%s\"", 
	     indent, "", tag, name, symbol);
	
  if (flags & GI_FUNCTION_IS_SETTER)
    g_fprintf (file, " type=\"setter\"");
  else if (flags & GI_FUNCTION_IS_GETTER)
    g_fprintf (file, " type=\"getter\"");
	  
  if (deprecated)
    g_fprintf (file, " deprecated=\"1\"");
	
  write_callable_info (namespace, (GICallableInfo*)info, file, indent);
  g_fprintf (file, "%*s</%s>\n", indent, "", tag);
}

static void
write_callback_info (const gchar    *namespace,
		     GICallbackInfo *info,
		     FILE           *file,
		     gint            indent)
{
  const gchar *name;
  gboolean deprecated;

  name = g_base_info_get_name ((GIBaseInfo *)info);
  deprecated = g_base_info_is_deprecated ((GIBaseInfo *)info);

  g_fprintf (file, "%*s<callback name=\"%s\"", indent, "", name);
	
  if (deprecated)
    g_fprintf (file, " deprecated=\"1\"");
	
  write_callable_info (namespace, (GICallableInfo*)info, file, indent);
  g_fprintf (file, "%*s</callback>\n", indent, "");
}

static void
write_struct_info (const gchar  *namespace,
		   GIStructInfo *info,
		   FILE         *file)
{
  const gchar *name;
  const gchar *type_name;
  const gchar *type_init;
  gboolean deprecated;
  gint i;
  int n_elts;

  name = g_base_info_get_name ((GIBaseInfo *)info);
  deprecated = g_base_info_is_deprecated ((GIBaseInfo *)info);

  if (g_base_info_get_type ((GIBaseInfo *)info) == GI_INFO_TYPE_BOXED)
    {
      type_name = g_registered_type_info_get_type_name ((GIRegisteredTypeInfo*)info);
      type_init = g_registered_type_info_get_type_init ((GIRegisteredTypeInfo*)info);
	    
      g_fprintf (file, "    <glib:boxed glib:name=\"%s\" glib:type-name=\"%s\" glib:get-type=\"%s\"", name, type_name, type_init);
    }
  else
    g_fprintf (file, "    <record name=\"%s\"", name);
	  
  if (deprecated)
    g_fprintf (file, " deprecated=\"1\"");
	
  n_elts = g_struct_info_get_n_fields (info) + g_struct_info_get_n_methods (info);
  if (n_elts > 0)
    {
      g_fprintf (file, ">\n");
      
      for (i = 0; i < g_struct_info_get_n_fields (info); i++)
	{
	  GIFieldInfo *field = g_struct_info_get_field (info, i);
	  write_field_info (namespace, field, NULL, file);
	  g_base_info_unref ((GIBaseInfo *)field);
	}
      
      for (i = 0; i < g_struct_info_get_n_methods (info); i++)
	{
	  GIFunctionInfo *function = g_struct_info_get_method (info, i);
	  write_function_info (namespace, function, file, 6);
	  g_base_info_unref ((GIBaseInfo *)function);
	}
      
      if (g_base_info_get_type ((GIBaseInfo *)info) == GI_INFO_TYPE_BOXED)
	g_fprintf (file, "    </glib:boxed>\n");
      else
	g_fprintf (file, "    </record>\n");
    } 
  else
    {
      g_fprintf (file, "/>\n");
    }
}

static void
write_value_info (const gchar *namespace,
		  GIValueInfo *info,
		  FILE        *file)
{
  const gchar *name;
  glong value;
  gboolean deprecated;

  name = g_base_info_get_name ((GIBaseInfo *)info);
  value = g_value_info_get_value (info);
  deprecated = g_base_info_is_deprecated ((GIBaseInfo *)info);

  g_fprintf (file, "      <member name=\"%s\" value=\"%ld\"", name, value);

  if (deprecated)
    g_fprintf (file, " deprecated=\"1\"");
  
  g_fprintf (file, " />\n");
}

static void
write_constant_value (const gchar *namespace, 
		      GITypeInfo *type,
		      GArgument  *value,
		      FILE       *file)
{
  switch (g_type_info_get_tag (type))
    {
    case GI_TYPE_TAG_BOOLEAN:
      g_fprintf (file, "%d", value->v_boolean);
      break;
    case GI_TYPE_TAG_INT8:
      g_fprintf (file, "%d", value->v_int8);
      break;
    case GI_TYPE_TAG_UINT8:
      g_fprintf (file, "%d", value->v_uint8);
      break;
    case GI_TYPE_TAG_INT16:
      g_fprintf (file, "%" G_GINT16_FORMAT, value->v_int16);
      break;
    case GI_TYPE_TAG_UINT16:
      g_fprintf (file, "%" G_GUINT16_FORMAT, value->v_uint16);
      break;
    case GI_TYPE_TAG_INT32:
      g_fprintf (file, "%" G_GINT32_FORMAT, value->v_int32);
      break;
    case GI_TYPE_TAG_UINT32:
      g_fprintf (file, "%" G_GUINT32_FORMAT, value->v_uint32);
      break;
    case GI_TYPE_TAG_INT64:
      g_fprintf (file, "%" G_GINT64_FORMAT, value->v_int64);
      break;
    case GI_TYPE_TAG_UINT64:
      g_fprintf (file, "%" G_GUINT64_FORMAT, value->v_uint64);
      break;
    case GI_TYPE_TAG_INT:
      g_fprintf (file, "%d", value->v_int);
      break;
    case GI_TYPE_TAG_UINT:
      g_fprintf (file, "%d", value->v_uint);
      break;
    case GI_TYPE_TAG_LONG:
      g_fprintf (file, "%ld", value->v_long);
      break;
    case GI_TYPE_TAG_ULONG:
      g_fprintf (file, "%ld", value->v_ulong);
      break;
    case GI_TYPE_TAG_SSIZE:
      g_fprintf (file, "%zd", value->v_ssize);
      break;
    case GI_TYPE_TAG_SIZE:
      g_fprintf (file, "%zd", value->v_size);
      break;
    case GI_TYPE_TAG_FLOAT:
      g_fprintf (file, "%f", value->v_float);
      break;
    case GI_TYPE_TAG_DOUBLE:
      g_fprintf (file, "%f", value->v_double);
      break;
    case GI_TYPE_TAG_UTF8:
    case GI_TYPE_TAG_FILENAME:
      g_fprintf (file, "%s", value->v_string);
      break;
    default:
      g_assert_not_reached ();
    }
}

static void
write_constant_info (const gchar    *namespace,
		     GIConstantInfo *info,
		     FILE           *file,
		     gint            indent)
{
  GITypeInfo *type;
  const gchar *name;
  gboolean deprecated;
  GArgument value;

  name = g_base_info_get_name ((GIBaseInfo *)info);
  deprecated = g_base_info_is_deprecated ((GIBaseInfo *)info);

  g_fprintf (file, "%*s<constant name=\"%s\"", indent, "", name);

  type = g_constant_info_get_type (info);
  g_fprintf (file, " value=\"");

  g_constant_info_get_value (info, &value);
  write_constant_value (namespace, type, &value, file);
  g_fprintf (file, "\">\n");

  g_fprintf (file, "%*s<type name=\"", indent + 2, "");

  write_type_info (namespace, type, file);

  g_fprintf (file, "\"/>\n");

  g_fprintf (file, "%*s</constant>\n", indent, "");
  
  g_base_info_unref ((GIBaseInfo *)type);
}


static void
write_enum_info (const gchar *namespace,
		 GIEnumInfo *info,
		 FILE        *file)
{
  const gchar *name;
  const gchar *type_name;
  const gchar *type_init;
  gboolean deprecated;
  gint i;

  name = g_base_info_get_name ((GIBaseInfo *)info);
  deprecated = g_base_info_is_deprecated ((GIBaseInfo *)info);

  type_name = g_registered_type_info_get_type_name ((GIRegisteredTypeInfo*)info);
  type_init = g_registered_type_info_get_type_init ((GIRegisteredTypeInfo*)info);

  if (g_base_info_get_type ((GIBaseInfo *)info) == GI_INFO_TYPE_ENUM)
    g_fprintf (file, "    <enumeration ");
  else
    g_fprintf (file, "    <bitfield ");
  g_fprintf (file, "name=\"%s\"", name);

  if (type_init)
    g_fprintf (file, " glib:type-name=\"%s\" glib:get-type=\"%s\"", type_name, type_init);
  
  if (deprecated)
    g_fprintf (file, " deprecated=\"1\"");
	
  g_fprintf (file, ">\n");

  for (i = 0; i < g_enum_info_get_n_values (info); i++)
    {
      GIValueInfo *value = g_enum_info_get_value (info, i);
      write_value_info (namespace, value, file);
      g_base_info_unref ((GIBaseInfo *)value);
    }

  if (g_base_info_get_type ((GIBaseInfo *)info) == GI_INFO_TYPE_ENUM)
    g_fprintf (file, "    </enumeration>\n");
  else
    g_fprintf (file, "    </bitfield>\n");
}

static void
write_signal_info (const gchar  *namespace,
		   GISignalInfo *info,
		   FILE         *file)
{
  GSignalFlags flags;
  const gchar *name;
  gboolean deprecated;

  name = g_base_info_get_name ((GIBaseInfo *)info);
  flags = g_signal_info_get_flags (info);
  deprecated = g_base_info_is_deprecated ((GIBaseInfo *)info);

  g_fprintf (file, "      <glib:signal name=\"%s\"", name);

  if (deprecated)
    g_fprintf (file, " deprecated=\"1\"");
	
  if (flags & G_SIGNAL_RUN_FIRST)
    g_fprintf (file, " when=\"FIRST\"");
  else if (flags & G_SIGNAL_RUN_LAST)
    g_fprintf (file, " when=\"LAST\"");
  else if (flags & G_SIGNAL_RUN_CLEANUP)
    g_fprintf (file, " when=\"CLEANUP\"");

  if (flags & G_SIGNAL_NO_RECURSE)
    g_fprintf (file, " no-recurse=\"1\"");

  if (flags & G_SIGNAL_DETAILED)
    g_fprintf (file, " detailed=\"1\"");

  if (flags & G_SIGNAL_ACTION)
    g_fprintf (file, " action=\"1\"");

  if (flags & G_SIGNAL_NO_HOOKS)
    g_fprintf (file, " no-hooks=\"1\"");

  write_callable_info (namespace, (GICallableInfo*)info, file, 6);

  g_fprintf (file, "      </glib:signal>\n");
}

static void
write_vfunc_info (const gchar *namespace, 
		  GIVFuncInfo *info,
		  FILE        *file)
{
  GIVFuncInfoFlags flags;
  const gchar *name;
  gboolean deprecated;
  gint offset;

  name = g_base_info_get_name ((GIBaseInfo *)info);
  flags = g_vfunc_info_get_flags (info);
  deprecated = g_base_info_is_deprecated ((GIBaseInfo *)info);
  offset = g_vfunc_info_get_offset (info);

  g_fprintf (file, "      <vfunc name=\"%s\"", name);

  if (deprecated)
    g_fprintf (file, " deprecated=\"1\"");
	
  if (flags & GI_VFUNC_MUST_CHAIN_UP)
    g_fprintf (file, " must-chain-up=\"1\"");

  if (flags & GI_VFUNC_MUST_OVERRIDE)
    g_fprintf (file, " override=\"always\"");
  else if (flags & GI_VFUNC_MUST_NOT_OVERRIDE)
    g_fprintf (file, " override=\"never\"");
    
  g_fprintf (file, " offset=\"%d\"", offset);

  write_callable_info (namespace, (GICallableInfo*)info, file, 6);

  g_fprintf (file, "      </vfunc>\n");
}

static void
write_property_info (const gchar    *namespace,
		     GIPropertyInfo *info,
		     FILE           *file)
{
  GParamFlags flags;
  const gchar *name;
  gboolean deprecated;
  GITypeInfo *type;

  name = g_base_info_get_name ((GIBaseInfo *)info);
  flags = g_property_info_get_flags (info);
  deprecated = g_base_info_is_deprecated ((GIBaseInfo *)info);

  g_fprintf (file, "      <property name=\"%s\"", name);

  if (deprecated)
    g_fprintf (file, " deprecated=\"1\"");
	
  if (flags & G_PARAM_READABLE)
    g_fprintf (file, " readable=\"1\"");
  else
    g_fprintf (file, " readable=\"0\"");

  if (flags & G_PARAM_WRITABLE)
    g_fprintf (file, " writable=\"1\"");
  else
    g_fprintf (file, " writable=\"0\"");

  if (flags & G_PARAM_CONSTRUCT)
    g_fprintf (file, " construct=\"1\"");

  if (flags & G_PARAM_CONSTRUCT_ONLY)
    g_fprintf (file, " construct-only=\"1\"");
    
  type = g_property_info_get_type (info);

  g_fprintf (file, ">\n");

  g_fprintf (file, "        <type name=\"", name);

  write_type_info (namespace, type, file);

  g_fprintf (file, "\"/>\n");
  
  g_fprintf (file, "      </property>\n");

}

static void
write_object_info (const gchar  *namespace, 
		   GIObjectInfo *info,
		   FILE         *file)
{
  const gchar *name;
  const gchar *type_name;
  const gchar *type_init;
  gboolean deprecated;
  GIObjectInfo *pnode;
  gint i;

  name = g_base_info_get_name ((GIBaseInfo *)info);
  deprecated = g_base_info_is_deprecated ((GIBaseInfo *)info);
  
  type_name = g_registered_type_info_get_type_name ((GIRegisteredTypeInfo*)info);
  type_init = g_registered_type_info_get_type_init ((GIRegisteredTypeInfo*)info);
  g_fprintf (file, "    <class name=\"%s\"", name);

  pnode = g_object_info_get_parent (info);
  if (pnode)
    {
      g_fprintf (file, " parent=\"");
      write_type_name (namespace, (GIBaseInfo *)pnode, file);
      g_fprintf (file, "\""  );
      g_base_info_unref ((GIBaseInfo *)pnode);
    }

  g_fprintf (file, " glib:type-name=\"%s\" glib:get-type=\"%s\"", type_name, type_init);

  if (deprecated)
    g_fprintf (file, " deprecated=\"1\"");
	
  g_fprintf (file, ">\n");

  if (g_object_info_get_n_interfaces (info) > 0)
    {
      g_fprintf (file, "      <implements>\n");
      for (i = 0; i < g_object_info_get_n_interfaces (info); i++)
	{
	  GIInterfaceInfo *imp = g_object_info_get_interface (info, i);
	  g_fprintf (file, "        <interface name=\"");
	  write_type_name (namespace, (GIBaseInfo*)imp, file);
	  g_fprintf (file,"\" />\n");
	  g_base_info_unref ((GIBaseInfo*)imp);
	}
      g_fprintf (file, "      </implements>\n");
    }

  for (i = 0; i < g_object_info_get_n_fields (info); i++)
    {
      GIFieldInfo *field = g_object_info_get_field (info, i);
      write_field_info (namespace, field, NULL, file);
      g_base_info_unref ((GIBaseInfo *)field);
    }

  for (i = 0; i < g_object_info_get_n_methods (info); i++)
    {
      GIFunctionInfo *function = g_object_info_get_method (info, i);
      write_function_info (namespace, function, file, 6);
      g_base_info_unref ((GIBaseInfo *)function);
    }

  for (i = 0; i < g_object_info_get_n_properties (info); i++)
    {
      GIPropertyInfo *prop = g_object_info_get_property (info, i);
      write_property_info (namespace, prop, file);
      g_base_info_unref ((GIBaseInfo *)prop);
    }

  for (i = 0; i < g_object_info_get_n_signals (info); i++)
    {
      GISignalInfo *signal = g_object_info_get_signal (info, i);
      write_signal_info (namespace, signal, file);
      g_base_info_unref ((GIBaseInfo *)signal);
    }
  
  for (i = 0; i < g_object_info_get_n_vfuncs (info); i++)
    {
      GIVFuncInfo *vfunc = g_object_info_get_vfunc (info, i);
      write_vfunc_info (namespace, vfunc, file);
      g_base_info_unref ((GIBaseInfo *)vfunc);
    }

  for (i = 0; i < g_object_info_get_n_constants (info); i++)
    {
      GIConstantInfo *constant = g_object_info_get_constant (info, i);
      write_constant_info (namespace, constant, file, 6);
      g_base_info_unref ((GIBaseInfo *)constant);
    }
  
  g_fprintf (file, "    </class>\n");
}

static void
write_interface_info (const gchar     *namespace,
		      GIInterfaceInfo *info,
		      FILE            *file)
{
  const gchar *name;
  const gchar *type_name;
  const gchar *type_init;
  gboolean deprecated;
  gint i;

  name = g_base_info_get_name ((GIBaseInfo *)info);
  deprecated = g_base_info_is_deprecated ((GIBaseInfo *)info);

  type_name = g_registered_type_info_get_type_name ((GIRegisteredTypeInfo*)info);
  type_init = g_registered_type_info_get_type_init ((GIRegisteredTypeInfo*)info);
  g_fprintf (file, "    <interface name=\"%s\" glib:type-name=\"%s\" glib:get-type=\"%s\"",
	     name, type_name, type_init);

  if (deprecated)
    g_fprintf (file, " deprecated=\"1\"");
	
  g_fprintf (file, ">\n");

  if (g_interface_info_get_n_prerequisites (info) > 0)
    {
      g_fprintf (file, "      <requires>\n");
      for (i = 0; i < g_interface_info_get_n_prerequisites (info); i++)
	{
	  GIBaseInfo *req = g_interface_info_get_prerequisite (info, i);
	  
	  if (g_base_info_get_type (req) == GI_INFO_TYPE_INTERFACE)
	    g_fprintf (file, "      <interface name=\"");
	  else
	    g_fprintf (file, "      <object name=\"");
	  write_type_name (namespace, req, file);
	  g_fprintf (file, "\" />\n");
	  g_base_info_unref (req);
	}
      g_fprintf (file, "      </requires>\n");
    }

  for (i = 0; i < g_interface_info_get_n_methods (info); i++)
    {
      GIFunctionInfo *function = g_interface_info_get_method (info, i);
      write_function_info (namespace, function, file, 6);
      g_base_info_unref ((GIBaseInfo *)function);
    }

  for (i = 0; i < g_interface_info_get_n_properties (info); i++)
    {
      GIPropertyInfo *prop = g_interface_info_get_property (info, i);
      write_property_info (namespace, prop, file);
      g_base_info_unref ((GIBaseInfo *)prop);
    }

  for (i = 0; i < g_interface_info_get_n_signals (info); i++)
    {
      GISignalInfo *signal = g_interface_info_get_signal (info, i);
      write_signal_info (namespace, signal, file);
      g_base_info_unref ((GIBaseInfo *)signal);
    }
  
  for (i = 0; i < g_interface_info_get_n_vfuncs (info); i++)
    {
      GIVFuncInfo *vfunc = g_interface_info_get_vfunc (info, i);
      write_vfunc_info (namespace, vfunc, file);
      g_base_info_unref ((GIBaseInfo *)vfunc);
    }

  for (i = 0; i < g_interface_info_get_n_constants (info); i++)
    {
      GIConstantInfo *constant = g_interface_info_get_constant (info, i);
      write_constant_info (namespace, constant, file, 6);
      g_base_info_unref ((GIBaseInfo *)constant);
    }
  
  g_fprintf (file, "    </interface>\n");
}

static void
write_error_domain_info (const gchar       *namespace,
			 GIErrorDomainInfo *info,
			 FILE              *file)
{
  GIBaseInfo *enum_;
  const gchar *name, *quark;
  
  name = g_base_info_get_name ((GIBaseInfo *)info);
  quark = g_error_domain_info_get_quark (info);
  enum_ = (GIBaseInfo *)g_error_domain_info_get_codes (info);
  g_fprintf (file,
	     "    <errordomain name=\"%s\" get-quark=\"%s\" codes=\"",
	     name, quark);
  write_type_name (namespace, enum_, file);
  g_fprintf (file, "\" />\n");
  g_base_info_unref (enum_);
}

static void
write_union_info (const gchar *namespace, 
		  GIUnionInfo *info, 
		  FILE        *file)
{
  const gchar *name;
  const gchar *type_name;
  const gchar *type_init;
  gboolean deprecated;
  gint i;

  name = g_base_info_get_name ((GIBaseInfo *)info);
  deprecated = g_base_info_is_deprecated ((GIBaseInfo *)info);

  type_name = g_registered_type_info_get_type_name ((GIRegisteredTypeInfo*)info);
  type_init = g_registered_type_info_get_type_init ((GIRegisteredTypeInfo*)info);
  
  g_fprintf (file, "    <union name=\"%s\"", name);
  
  if (type_name)
    g_fprintf (file, " type-name=\"%s\" get-type=\"%s\"", type_name, type_init);
	  
  if (deprecated)
    g_fprintf (file, " deprecated=\"1\"");
	
  g_fprintf (file, ">\n");

  if (g_union_info_is_discriminated (info))
    {
      gint offset;
      GITypeInfo *type;

      offset = g_union_info_get_discriminator_offset (info);
      type = g_union_info_get_discriminator_type (info);
      
      g_fprintf (file, "      <discriminator offset=\"%d\" type=\"", offset);
      write_type_info (namespace, type, file);
      g_fprintf (file, "\" />\n");
      g_base_info_unref ((GIBaseInfo *)type);
    }

  for (i = 0; i < g_union_info_get_n_fields (info); i++)
    {
      GIFieldInfo *field = g_union_info_get_field (info, i);
      GIConstantInfo *constant = g_union_info_get_discriminator (info, i);
      write_field_info (namespace, field, constant, file);
      g_base_info_unref ((GIBaseInfo *)field);
      if (constant)
	g_base_info_unref ((GIBaseInfo *)constant);
    }

  for (i = 0; i < g_union_info_get_n_methods (info); i++)
    {
      GIFunctionInfo *function = g_union_info_get_method (info, i);
      write_function_info (namespace, function, file, 6);
      g_base_info_unref ((GIBaseInfo *)function);
    }

  g_fprintf (file, "    </union>\n");
}

static void
write_repository (GIRepository *repository,
		  gboolean      needs_prefix)
{
  FILE *file;
  gchar **namespaces;
  gchar *ns;
  gint i, j;

  namespaces = g_irepository_get_namespaces (repository);

  if (output == NULL)
    file = stdout;
  else
    {
      gchar *filename;
      
      if (needs_prefix)
	filename = g_strdup_printf ("%s-%s", namespaces[0], output);  
      else
	filename = g_strdup (output);
      file = g_fopen (filename, "w");
      
      if (file == NULL)
	{
	  g_fprintf (stderr, "failed to open '%s': %s\n",
		     filename, g_strerror (errno));
	  g_free (filename);
	  
	  return;
	}
      
      g_free (filename);
    }
  
  g_fprintf (file, "<?xml version=\"1.0\"?>\n");
  g_fprintf (file, "<repository version=\"1.0\"\n"
	     "            xmlns=\"http://www.gtk.org/introspection/core/1.0\"\n"
	     "            xmlns:c=\"http://www.gtk.org/introspection/c/1.0\"\n"
	     "            xmlns:glib=\"http://www.gtk.org/introspection/glib/1.0\">\n");

  for (i = 0; namespaces[i]; i++)
    {
      const gchar *shared_library;
      ns = namespaces[i];
      shared_library = g_irepository_get_shared_library (repository, ns);
      if (shared_library)
        g_fprintf (file, "  <namespace name=\"%s\" shared-library=\"%s\">\n",
                   ns, shared_library);
      else
        g_fprintf (file, "  <namespace name=\"%s\">\n", ns);
      
      for (j = 0; j < g_irepository_get_n_infos (repository, ns); j++)
	{
	  GIBaseInfo *info = g_irepository_get_info (repository, ns, j);
	  switch (g_base_info_get_type (info))
	    {
	    case GI_INFO_TYPE_FUNCTION:
	      write_function_info (ns, (GIFunctionInfo *)info, file, 4);
	      break;
	      
	    case GI_INFO_TYPE_CALLBACK:
	      write_callback_info (ns, (GICallbackInfo *)info, file, 4);
	      break;

	    case GI_INFO_TYPE_STRUCT:
	    case GI_INFO_TYPE_BOXED:
	      write_struct_info (ns, (GIStructInfo *)info, file);
	      break;

	    case GI_INFO_TYPE_UNION:
	      write_union_info (ns, (GIUnionInfo *)info, file);
	      break;

	    case GI_INFO_TYPE_ENUM:
	    case GI_INFO_TYPE_FLAGS:
	      write_enum_info (ns, (GIEnumInfo *)info, file);
	      break;
	      
	    case GI_INFO_TYPE_CONSTANT:
	      write_constant_info (ns, (GIConstantInfo *)info, file, 4);
	      break;

	    case GI_INFO_TYPE_OBJECT:
	      write_object_info (ns, (GIObjectInfo *)info, file);
	      break;

	    case GI_INFO_TYPE_INTERFACE:
	      write_interface_info (ns, (GIInterfaceInfo *)info, file);
	      break;

	    case GI_INFO_TYPE_ERROR_DOMAIN:
	      write_error_domain_info (ns, (GIErrorDomainInfo *)info, file);
	      break;

	    default:
	      g_error ("unknown info type %d\n", g_base_info_get_type (info));
	    }

	  g_base_info_unref (info);
	}

      g_fprintf (file, "  </namespace>\n");
    }

  g_fprintf (file, "</repository>\n");
      
  if (output != NULL)
    fclose (file);        

  g_strfreev (namespaces);
}

static const guchar *
load_typelib (const gchar  *filename,
	       GModule     **dlhandle,
               gsize        *len)
{
  guchar *typelib;
  gsize *typelib_size;
  GModule *handle; 

  handle = g_module_open (filename, G_MODULE_BIND_LOCAL|G_MODULE_BIND_LAZY);
  if (!g_module_symbol (handle, "_G_TYPELIB", (gpointer *) &typelib))
    {
      g_printerr ("Could not load typelib from '%s': %s\n", 
		  filename, g_module_error ());
      return NULL;
    }
  
  if (!g_module_symbol (handle, "_G_TYPELIB_SIZE", (gpointer *) &typelib_size))
    {
      g_printerr ("Could not load typelib from '%s': %s\n", 
		  filename, g_module_error ());
      return NULL;
    }

  *len = *typelib_size;
  
  if (dlhandle)
    *dlhandle = handle;

  return typelib;
}

int 
main (int argc, char *argv[])
{  
  gboolean raw = FALSE;
  gchar **input = NULL;
  GOptionContext *context;
  GError *error = NULL;
  gboolean needs_prefix;
  gint i;
  GTypelib *data;
  GOptionEntry options[] = 
    {
      { "raw", 0, 0, G_OPTION_ARG_NONE, &raw, "handle raw typelib", NULL },
      { "output", 'o', 0, G_OPTION_ARG_FILENAME, &output, "output file", "FILE" }, 
      { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &input, NULL, NULL },
      { NULL, }
    };

  g_type_init ();

  g_typelib_check_sanity ();

  context = g_option_context_new ("");
  g_option_context_add_main_entries (context, options, NULL);
  g_option_context_parse (context, &argc, &argv, &error);

  if (!input) 
    { 
      g_fprintf (stderr, "no input files\n"); 
      
      return 1;
    }

  for (i = 0; input[i]; i++)
    {
      GModule *dlhandle = NULL;
      const guchar *typelib;
      gsize len;

      if (raw)
	{
	  if (!g_file_get_contents (input[i], (gchar **)&typelib, &len, &error))
	    {
	      g_fprintf (stderr, "failed to read '%s': %s\n", 
			 input[i], error->message);
	      g_clear_error (&error);
	      continue;
	    }
	}
      else
	{
	  typelib = load_typelib (input[i], &dlhandle, &len);
	  if (!typelib)
	    {
	      g_fprintf (stderr, "failed to load typelib from '%s'\n", 
			 input[i]);
	      continue;
	    }
	}

      if (input[i + 1] && output)
	needs_prefix = TRUE;
      else
	needs_prefix = FALSE;

      data = g_typelib_new_from_const_memory (typelib, len);
      {
        GError *error = NULL;
        if (!g_typelib_validate (data, &error)) {
          g_printerr ("typelib not valid: %s\n", error->message);
          g_clear_error (&error);
        }
      }
      g_irepository_register (g_irepository_get_default (), data);
      write_repository (g_irepository_get_default (), needs_prefix);
      g_irepository_unregister (g_irepository_get_default (),
                                g_typelib_get_namespace (data));

      if (dlhandle)
	{
	  g_module_close (dlhandle);
	  dlhandle = NULL;
	}

      /* when writing to stdout, stop after the first module */
      if (input[i + 1] && !output)
	{
	  g_fprintf (stderr, "warning, %d modules omitted\n",
		     g_strv_length (input) - 1);

	  break;
	}
    }
      
  return 0;
}
