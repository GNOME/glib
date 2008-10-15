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
#include <string.h>

#include <glib.h>
#include <glib-object.h>
#include <glib/gstdio.h>

#include "girepository.h"
#include "gtypelib.h"

/* FIXME: Avoid global */
static gchar *output = NULL;
gchar **includedirs = NULL;

typedef struct {
  FILE *file;
  GSList *stack;
} Xml;

typedef struct {
  char *name;
  guint has_children : 1;
} XmlElement;

static XmlElement *
xml_element_new (const char *name)
{
  XmlElement *elem;

  elem = g_new (XmlElement, 1);
  elem->name = g_strdup (name);
  elem->has_children = FALSE;
  return elem;
}

static void
xml_element_free (XmlElement *elem)
{
  g_free (elem->name);
  g_free (elem);
}

static void
xml_printf (Xml *xml, const char *fmt, ...)
{
  va_list ap;

  va_start (ap, fmt);
  vfprintf (xml->file, fmt, ap);
  va_end (ap);
}

static void
xml_start_element (Xml *xml, const char *element_name)
{
  XmlElement *parent = NULL;

  if (xml->stack)
    {
      parent = xml->stack->data;

      if (!parent->has_children)
        xml_printf (xml, ">\n");

      parent->has_children = TRUE;
    }

  xml_printf (xml, "%*s<%s", g_slist_length(xml->stack)*2, "", element_name);

  xml->stack = g_slist_prepend (xml->stack, xml_element_new (element_name));
}

static void
xml_end_element (Xml *xml, const char *name)
{
  XmlElement *elem;

  g_assert (xml->stack != NULL);

  elem = xml->stack->data;
  xml->stack = g_slist_delete_link (xml->stack, xml->stack);

  if (name != NULL)
    g_assert_cmpstr (name, ==, elem->name);

  if (elem->has_children)
    xml_printf (xml, "%*s</%s>\n", g_slist_length (xml->stack)*2, "", elem->name);
  else
    xml_printf (xml, "/>\n");

  xml_element_free (elem);
}

static void
xml_end_element_unchecked (Xml *xml)
{
  xml_end_element (xml, NULL);
}

static Xml *
xml_open (FILE *file)
{
  Xml *xml;

  xml = g_new (Xml, 1);
  xml->file = file;
  xml->stack = NULL;

  return xml;
}

static void
xml_close (Xml *xml)
{
  g_assert (xml->stack == NULL);
  if (xml->file != NULL)
    {
      fflush (xml->file);
      if (xml->file != stdout)
        fclose (xml->file);
      xml->file = NULL;
    }
}

static void
xml_free (Xml *xml)
{
  xml_close (xml);
  g_free (xml);
}


static void 
check_unresolved (GIBaseInfo *info)
{
  if (g_base_info_get_type (info) != GI_INFO_TYPE_UNRESOLVED)
    return;

  g_critical ("Found unresolved type '%s' '%s'\n", 
	      g_base_info_get_name (info), g_base_info_get_namespace (info));
}

static void 
write_type_name (const gchar *namespace,
		 GIBaseInfo  *info,
		 Xml         *file)
{
  if (strcmp (namespace, g_base_info_get_namespace (info)) != 0)
    xml_printf (file, "%s.", g_base_info_get_namespace (info));

  xml_printf (file, "%s", g_base_info_get_name (info));
}

static void
write_type_info (const gchar *namespace,
		 GITypeInfo  *info, 
		 Xml         *file)
{
  gint tag;
  gint i;
  GITypeInfo *type;
  gboolean is_pointer;
  
  check_unresolved ((GIBaseInfo*)info);

  tag = g_type_info_get_tag (info);
  is_pointer = g_type_info_is_pointer (info);

  if (tag == GI_TYPE_TAG_VOID) 
    {
      if (is_pointer)
	xml_printf (file, "%s", "any");
      else
	xml_printf (file, "%s", "none");
    } 
  else if (G_TYPE_TAG_IS_BASIC (tag))
    xml_printf (file, "%s", g_type_tag_to_string (tag));
  else if (tag == GI_TYPE_TAG_ARRAY)
    {
      gint length;

      type = g_type_info_get_param_type (info, 0);
      write_type_info (namespace, type, file);
      xml_printf (file, "[");

      length = g_type_info_get_array_length (info);
      
      if (length >= 0)
	xml_printf (file, "length=%d", length);
      
      if (g_type_info_is_zero_terminated (info))
	xml_printf (file, "%szero-terminated=1", length >= 0 ? "," : "");
      
     xml_printf (file, "]");
      g_base_info_unref ((GIBaseInfo *)type);
    }
  else if (tag == GI_TYPE_TAG_INTERFACE)
    {
      GIBaseInfo *iface = g_type_info_get_interface (info);
      write_type_name (namespace, iface, file);
      g_base_info_unref (iface);
    }
  else if (tag == GI_TYPE_TAG_GLIST)
    {
      type = g_type_info_get_param_type (info, 0);
      xml_printf (file, "GLib.List");
      if (type)
	{
	  xml_printf (file, "<");
	  write_type_info (namespace, type, file);
	  xml_printf (file, ">");
	  g_base_info_unref ((GIBaseInfo *)type);
	}
    }
  else if (tag == GI_TYPE_TAG_GSLIST)
    {
      type = g_type_info_get_param_type (info, 0);
      xml_printf (file, "GLib.SList");
      if (type)
	{
	  xml_printf (file, "<");
	  write_type_info (namespace, type, file);
	  xml_printf (file, ">");
	  g_base_info_unref ((GIBaseInfo *)type);
	}
    }
  else if (tag == GI_TYPE_TAG_GHASH)
    {
      type = g_type_info_get_param_type (info, 0);
      xml_printf (file, "GLib.HashTable");
      if (type)
	{
	  xml_printf (file, "<");
	  write_type_info (namespace, type, file);
	  g_base_info_unref ((GIBaseInfo *)type);
	  type = g_type_info_get_param_type (info, 1);
	  xml_printf (file, ",");
	  write_type_info (namespace, type, file);
	  xml_printf (file, ">");
	  g_base_info_unref ((GIBaseInfo *)type);
	}
    }
  else if (tag == GI_TYPE_TAG_ERROR) 
    {
      gint n;

      xml_printf (file, "GLib.Error");
      n = g_type_info_get_n_error_domains (info);
      if (n > 0)
	{
	  xml_printf (file, "<");
	  for (i = 0; i < n; i++)
	    {
	      GIErrorDomainInfo *ed = g_type_info_get_error_domain (info, i);
	      if (i > 0)
		xml_printf (file, ",");
	      write_type_name (namespace, (GIBaseInfo *)ed, file);
	      g_base_info_unref ((GIBaseInfo *)ed);
	    }
	  xml_printf (file, ">");
	}
    }
  else
    {
      g_printerr ("Unhandled type tag %d\n", tag);
      g_assert_not_reached ();
    }
}

static void
write_constant_value (const gchar *namespace, 
		      GITypeInfo *info,
		      GArgument *argument,
		      Xml *file);

static void
write_field_info (const gchar *namespace,
		  GIFieldInfo *info,
		  GIConstantInfo *branch,
		  Xml         *file)
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

  xml_start_element (file, "field");
  xml_printf (file, " name=\"%s\" readable=\"%s\" writable=\"%s\"",
	     name, 
	     flags & GI_FIELD_IS_READABLE ? "1" : "0", 
	     flags & GI_FIELD_IS_WRITABLE ? "1" : "0");
  if (size)
    xml_printf (file, " bits=\"%d\"", size);

  xml_printf (file, " offset=\"%d\"", offset);

  type = g_field_info_get_type (info);

  if (branch)
    {
      xml_printf (file, " branch=\"");
      type = g_constant_info_get_type (branch);
      g_constant_info_get_value (branch, &value);
      write_constant_value (namespace, type, &value, file);
      xml_printf (file, "\"");
    }

  xml_start_element (file, "type");

  xml_printf (file, " name=\"");

  write_type_info (namespace, type, file);
  g_base_info_unref ((GIBaseInfo *)type);

  xml_printf (file, "\"");

  xml_end_element (file, "type");

  xml_end_element (file, "field");
}

static void 
write_callable_info (const gchar    *namespace,
		     GICallableInfo *info,
		     Xml            *file)
{
  GITypeInfo *type;
  gint i;

  type = g_callable_info_get_return_type (info);

  xml_start_element (file, "return-value");

  if (g_type_info_is_pointer (type))
    {
      switch (g_callable_info_get_caller_owns (info))
	{
	case GI_TRANSFER_NOTHING:
	  break;
	case GI_TRANSFER_CONTAINER:
	  xml_printf (file, " transfer-ownership=\"container\"");
	  break;
	case GI_TRANSFER_EVERYTHING:
	  xml_printf (file, " transfer-ownership=\"full\"");
	  break;
	default:
	  g_assert_not_reached ();
	}
    }
  
  if (g_callable_info_may_return_null (info))
    xml_printf (file, " null-ok=\"1\"");

  xml_start_element (file, "type");

  xml_printf (file, " name=\"");

  write_type_info (namespace, type, file);

  xml_printf (file, "\"");

  xml_end_element (file, "type");

  xml_end_element (file, "return-value");
	
  if (g_callable_info_get_n_args (info) <= 0)
    return;

  xml_start_element (file, "parameters");
  for (i = 0; i < g_callable_info_get_n_args (info); i++)
    {
      GIArgInfo *arg = g_callable_info_get_arg (info, i);
      
      xml_start_element (file, "parameter");
      xml_printf (file, " name=\"%s\"",
                  g_base_info_get_name ((GIBaseInfo *) arg));
      
      switch (g_arg_info_get_ownership_transfer (arg))
	{
	case GI_TRANSFER_NOTHING:
	  break;
	case GI_TRANSFER_CONTAINER:
	  xml_printf (file, " transfer=\"container\"");
	  break;
	case GI_TRANSFER_EVERYTHING:
	  xml_printf (file, " transfer=\"full\"");
	  break;
	default:
	  g_assert_not_reached ();
	}
      
      xml_printf (file, " direction=\"");
      switch (g_arg_info_get_direction (arg))
	{
	case GI_DIRECTION_IN:
	  xml_printf (file, "in");
	  break;
	case GI_DIRECTION_OUT:
	  xml_printf (file, "out");
	  break;
	case GI_DIRECTION_INOUT:
	  xml_printf (file, "inout");
	  break;
	}
      xml_printf (file, "\"");
      
      if (g_arg_info_may_be_null (arg))
	xml_printf (file, " null-ok=\"1\"");
      
      if (g_arg_info_is_dipper (arg))
	xml_printf (file, " dipper=\"1\"");
      
      if (g_arg_info_is_return_value (arg))
	xml_printf (file, " retval=\"1\"");
      
      if (g_arg_info_is_optional (arg))
	xml_printf (file, " optional=\"1\"");
      
      xml_start_element (file, "type");
      
      xml_printf (file, " name=\"");

      type = g_arg_info_get_type (arg);
      write_type_info (namespace, type, file);

      xml_printf (file, "\"");

      xml_end_element (file, "type");

      xml_end_element (file, "parameter");

      g_base_info_unref ((GIBaseInfo *)arg);
    }
  
  xml_end_element (file, "parameters");
  g_base_info_unref ((GIBaseInfo *)type);
}

static void
write_function_info (const gchar    *namespace,
		     GIFunctionInfo *info,
		     Xml            *file)
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
	
  xml_start_element (file, tag);
  xml_printf (file, " name=\"%s\" c:identifier=\"%s\"",
              name, symbol);
	
  if (flags & GI_FUNCTION_IS_SETTER)
    xml_printf (file, " type=\"setter\"");
  else if (flags & GI_FUNCTION_IS_GETTER)
    xml_printf (file, " type=\"getter\"");
	  
  if (deprecated)
    xml_printf (file, " deprecated=\"1\"");

  write_callable_info (namespace, (GICallableInfo*)info, file);
  xml_end_element (file, tag);
}

static void
write_callback_info (const gchar    *namespace,
		     GICallbackInfo *info,
		     Xml            *file)
{
  const gchar *name;
  gboolean deprecated;

  name = g_base_info_get_name ((GIBaseInfo *)info);
  deprecated = g_base_info_is_deprecated ((GIBaseInfo *)info);

  xml_start_element (file, "callback");
  xml_printf (file, " name=\"%s\"", name);
	
  if (deprecated)
    xml_printf (file, " deprecated=\"1\"");
	
  write_callable_info (namespace, (GICallableInfo*)info, file);
  xml_end_element (file, "callback");
}

static void
write_struct_info (const gchar  *namespace,
		   GIStructInfo *info,
		   Xml          *file)
{
  const gchar *name;
  const gchar *type_name;
  const gchar *type_init;
  gboolean deprecated;
  gint i;
  int n_elts;

  name = g_base_info_get_name ((GIBaseInfo *)info);
  deprecated = g_base_info_is_deprecated ((GIBaseInfo *)info);

  type_name = g_registered_type_info_get_type_name ((GIRegisteredTypeInfo*)info);
  type_init = g_registered_type_info_get_type_init ((GIRegisteredTypeInfo*)info);
  
  if (g_base_info_get_type ((GIBaseInfo *)info) == GI_INFO_TYPE_BOXED)
    {
      xml_start_element (file, "glib:boxed");
      xml_printf (file, " glib:name=\"%s\"", name);
    }
  else
    {
      xml_start_element (file, "record");
      xml_printf (file, " name=\"%s\"", name);
    }
  
  if (type_name != NULL)
    xml_printf (file, " glib:type-name=\"%s\" glib:get-type=\"%s\"", type_name, type_init);
	  
  if (deprecated)
    xml_printf (file, " deprecated=\"1\"");
	
  n_elts = g_struct_info_get_n_fields (info) + g_struct_info_get_n_methods (info);
  if (n_elts > 0)
    {
      for (i = 0; i < g_struct_info_get_n_fields (info); i++)
	{
	  GIFieldInfo *field = g_struct_info_get_field (info, i);
	  write_field_info (namespace, field, NULL, file);
	  g_base_info_unref ((GIBaseInfo *)field);
	}
      
      for (i = 0; i < g_struct_info_get_n_methods (info); i++)
	{
	  GIFunctionInfo *function = g_struct_info_get_method (info, i);
	  write_function_info (namespace, function, file);
	  g_base_info_unref ((GIBaseInfo *)function);
	}
      
    } 

  xml_end_element_unchecked (file);
}

static void
write_value_info (const gchar *namespace,
		  GIValueInfo *info,
		  Xml         *file)
{
  const gchar *name;
  glong value;
  gboolean deprecated;

  name = g_base_info_get_name ((GIBaseInfo *)info);
  value = g_value_info_get_value (info);
  deprecated = g_base_info_is_deprecated ((GIBaseInfo *)info);

  xml_start_element (file, "member");
  xml_printf (file, " name=\"%s\" value=\"%ld\"", name, value);

  if (deprecated)
    xml_printf (file, " deprecated=\"1\"");
  
  xml_end_element (file, "member");
}

static void
write_constant_value (const gchar *namespace, 
		      GITypeInfo *type,
		      GArgument  *value,
		      Xml        *file)
{
  switch (g_type_info_get_tag (type))
    {
    case GI_TYPE_TAG_BOOLEAN:
      xml_printf (file, "%d", value->v_boolean);
      break;
    case GI_TYPE_TAG_INT8:
      xml_printf (file, "%d", value->v_int8);
      break;
    case GI_TYPE_TAG_UINT8:
      xml_printf (file, "%d", value->v_uint8);
      break;
    case GI_TYPE_TAG_INT16:
      xml_printf (file, "%" G_GINT16_FORMAT, value->v_int16);
      break;
    case GI_TYPE_TAG_UINT16:
      xml_printf (file, "%" G_GUINT16_FORMAT, value->v_uint16);
      break;
    case GI_TYPE_TAG_INT32:
      xml_printf (file, "%" G_GINT32_FORMAT, value->v_int32);
      break;
    case GI_TYPE_TAG_UINT32:
      xml_printf (file, "%" G_GUINT32_FORMAT, value->v_uint32);
      break;
    case GI_TYPE_TAG_INT64:
      xml_printf (file, "%" G_GINT64_FORMAT, value->v_int64);
      break;
    case GI_TYPE_TAG_UINT64:
      xml_printf (file, "%" G_GUINT64_FORMAT, value->v_uint64);
      break;
    case GI_TYPE_TAG_INT:
      xml_printf (file, "%d", value->v_int);
      break;
    case GI_TYPE_TAG_UINT:
      xml_printf (file, "%d", value->v_uint);
      break;
    case GI_TYPE_TAG_LONG:
      xml_printf (file, "%ld", value->v_long);
      break;
    case GI_TYPE_TAG_ULONG:
      xml_printf (file, "%ld", value->v_ulong);
      break;
    case GI_TYPE_TAG_SSIZE:
      xml_printf (file, "%zd", value->v_ssize);
      break;
    case GI_TYPE_TAG_SIZE:
      xml_printf (file, "%zd", value->v_size);
      break;
    case GI_TYPE_TAG_FLOAT:
      xml_printf (file, "%f", value->v_float);
      break;
    case GI_TYPE_TAG_DOUBLE:
      xml_printf (file, "%f", value->v_double);
      break;
    case GI_TYPE_TAG_UTF8:
    case GI_TYPE_TAG_FILENAME:
      xml_printf (file, "%s", value->v_string);
      break;
    default:
      g_assert_not_reached ();
    }
}

static void
write_constant_info (const gchar    *namespace,
		     GIConstantInfo *info,
		     Xml            *file)
{
  GITypeInfo *type;
  const gchar *name;
  gboolean deprecated;
  GArgument value;

  name = g_base_info_get_name ((GIBaseInfo *)info);
  deprecated = g_base_info_is_deprecated ((GIBaseInfo *)info);

  xml_start_element (file, "constant");
  xml_printf (file, " name=\"%s\"", name);

  type = g_constant_info_get_type (info);
  xml_printf (file, " value=\"");

  g_constant_info_get_value (info, &value);
  write_constant_value (namespace, type, &value, file);
  xml_printf (file, "\"");

  xml_start_element (file, "type");
  xml_printf (file, " name=\"");

  write_type_info (namespace, type, file);
  xml_printf (file, "\"");

  xml_end_element (file, "type");

  xml_end_element (file, "constant");
  
  g_base_info_unref ((GIBaseInfo *)type);
}


static void
write_enum_info (const gchar *namespace,
		 GIEnumInfo *info,
		 Xml         *file)
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
    xml_start_element (file, "enumeration");
  else
    xml_start_element (file, "bitfield");
  xml_printf (file, " name=\"%s\"", name);

  if (type_init)
    xml_printf (file, " glib:type-name=\"%s\" glib:get-type=\"%s\"", type_name, type_init);
  
  if (deprecated)
    xml_printf (file, " deprecated=\"1\"");
	

  for (i = 0; i < g_enum_info_get_n_values (info); i++)
    {
      GIValueInfo *value = g_enum_info_get_value (info, i);
      write_value_info (namespace, value, file);
      g_base_info_unref ((GIBaseInfo *)value);
    }

  xml_end_element_unchecked (file);
}

static void
write_signal_info (const gchar  *namespace,
		   GISignalInfo *info,
		   Xml          *file)
{
  GSignalFlags flags;
  const gchar *name;
  gboolean deprecated;

  name = g_base_info_get_name ((GIBaseInfo *)info);
  flags = g_signal_info_get_flags (info);
  deprecated = g_base_info_is_deprecated ((GIBaseInfo *)info);

  xml_start_element (file, "glib:signal");
  xml_printf (file, " name=\"%s\"", name);

  if (deprecated)
    xml_printf (file, " deprecated=\"1\"");
	
  if (flags & G_SIGNAL_RUN_FIRST)
    xml_printf (file, " when=\"FIRST\"");
  else if (flags & G_SIGNAL_RUN_LAST)
    xml_printf (file, " when=\"LAST\"");
  else if (flags & G_SIGNAL_RUN_CLEANUP)
    xml_printf (file, " when=\"CLEANUP\"");

  if (flags & G_SIGNAL_NO_RECURSE)
    xml_printf (file, " no-recurse=\"1\"");

  if (flags & G_SIGNAL_DETAILED)
    xml_printf (file, " detailed=\"1\"");

  if (flags & G_SIGNAL_ACTION)
    xml_printf (file, " action=\"1\"");

  if (flags & G_SIGNAL_NO_HOOKS)
    xml_printf (file, " no-hooks=\"1\"");

  write_callable_info (namespace, (GICallableInfo*)info, file);

  xml_end_element (file, "glib:signal");
}

static void
write_vfunc_info (const gchar *namespace, 
		  GIVFuncInfo *info,
		  Xml         *file)
{
  GIVFuncInfoFlags flags;
  const gchar *name;
  gboolean deprecated;
  gint offset;

  name = g_base_info_get_name ((GIBaseInfo *)info);
  flags = g_vfunc_info_get_flags (info);
  deprecated = g_base_info_is_deprecated ((GIBaseInfo *)info);
  offset = g_vfunc_info_get_offset (info);

  xml_start_element (file, "vfunc");
  xml_printf (file, " name=\"%s\"", name);

  if (deprecated)
    xml_printf (file, " deprecated=\"1\"");
	
  if (flags & GI_VFUNC_MUST_CHAIN_UP)
    xml_printf (file, " must-chain-up=\"1\"");

  if (flags & GI_VFUNC_MUST_OVERRIDE)
    xml_printf (file, " override=\"always\"");
  else if (flags & GI_VFUNC_MUST_NOT_OVERRIDE)
    xml_printf (file, " override=\"never\"");
    
  xml_printf (file, " offset=\"%d\"", offset);

  write_callable_info (namespace, (GICallableInfo*)info, file);

  xml_end_element (file, "vfunc");
}

static void
write_property_info (const gchar    *namespace,
		     GIPropertyInfo *info,
		     Xml            *file)
{
  GParamFlags flags;
  const gchar *name;
  gboolean deprecated;
  GITypeInfo *type;

  name = g_base_info_get_name ((GIBaseInfo *)info);
  flags = g_property_info_get_flags (info);
  deprecated = g_base_info_is_deprecated ((GIBaseInfo *)info);

  xml_start_element (file, "property");
  xml_printf (file, " name=\"%s\"", name);

  if (deprecated)
    xml_printf (file, " deprecated=\"1\"");
	
  if (flags & G_PARAM_READABLE)
    xml_printf (file, " readable=\"1\"");
  else
    xml_printf (file, " readable=\"0\"");

  if (flags & G_PARAM_WRITABLE)
    xml_printf (file, " writable=\"1\"");
  else
    xml_printf (file, " writable=\"0\"");

  if (flags & G_PARAM_CONSTRUCT)
    xml_printf (file, " construct=\"1\"");

  if (flags & G_PARAM_CONSTRUCT_ONLY)
    xml_printf (file, " construct-only=\"1\"");
    
  type = g_property_info_get_type (info);

  xml_start_element (file, "type");

  xml_printf (file, " name=\"");

  write_type_info (namespace, type, file);

  xml_printf (file, "\"");
  
  xml_end_element (file, "type");

  xml_end_element (file, "property");
}

static void
write_object_info (const gchar  *namespace, 
		   GIObjectInfo *info,
		   Xml          *file)
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
  xml_start_element (file, "class");
  xml_printf (file, " name=\"%s\"", name);

  pnode = g_object_info_get_parent (info);
  if (pnode)
    {
      xml_printf (file, " parent=\"");
      write_type_name (namespace, (GIBaseInfo *)pnode, file);
      xml_printf (file, "\""  );
      g_base_info_unref ((GIBaseInfo *)pnode);
    }

  xml_printf (file, " glib:type-name=\"%s\" glib:get-type=\"%s\"", type_name, type_init);

  if (deprecated)
    xml_printf (file, " deprecated=\"1\"");
	

  if (g_object_info_get_n_interfaces (info) > 0)
    {
      for (i = 0; i < g_object_info_get_n_interfaces (info); i++)
	{
	  GIInterfaceInfo *imp = g_object_info_get_interface (info, i);
          xml_start_element (file, "implements");
	  xml_printf (file, " name=\"");
	  write_type_name (namespace, (GIBaseInfo*)imp, file);
	  xml_printf (file,"\"");
          xml_end_element (file, "implements");
	  g_base_info_unref ((GIBaseInfo*)imp);
	}
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
      write_function_info (namespace, function, file);
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
      write_constant_info (namespace, constant, file);
      g_base_info_unref ((GIBaseInfo *)constant);
    }
  
  xml_end_element (file, "class");
}

static void
write_interface_info (const gchar     *namespace,
		      GIInterfaceInfo *info,
		      Xml             *file)
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
  xml_start_element (file, "interface");
  xml_printf (file, " name=\"%s\" glib:type-name=\"%s\" glib:get-type=\"%s\"",
	     name, type_name, type_init);

  if (deprecated)
    xml_printf (file, " deprecated=\"1\"");
	

  if (g_interface_info_get_n_prerequisites (info) > 0)
    {
      xml_start_element (file, "requires");
      for (i = 0; i < g_interface_info_get_n_prerequisites (info); i++)
	{
	  GIBaseInfo *req = g_interface_info_get_prerequisite (info, i);
	  
	  if (g_base_info_get_type (req) == GI_INFO_TYPE_INTERFACE)
            xml_start_element (file, "interface");
	  else
            xml_start_element (file, "object");
          xml_printf (file, " name=\"");
	  write_type_name (namespace, req, file);
          xml_end_element_unchecked (file);
	  g_base_info_unref (req);
	}
      xml_end_element (file, "requires");
    }

  for (i = 0; i < g_interface_info_get_n_methods (info); i++)
    {
      GIFunctionInfo *function = g_interface_info_get_method (info, i);
      write_function_info (namespace, function, file);
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
      write_constant_info (namespace, constant, file);
      g_base_info_unref ((GIBaseInfo *)constant);
    }
  
  xml_end_element (file, "interface");
}

static void
write_error_domain_info (const gchar       *namespace,
			 GIErrorDomainInfo *info,
			 Xml               *file)
{
  GIBaseInfo *enum_;
  const gchar *name, *quark;
  
  name = g_base_info_get_name ((GIBaseInfo *)info);
  quark = g_error_domain_info_get_quark (info);
  enum_ = (GIBaseInfo *)g_error_domain_info_get_codes (info);
  xml_start_element (file, "errordomain");
  xml_printf (file, " name=\"%s\" get-quark=\"%s\" codes=\"",
              name, quark);
  write_type_name (namespace, enum_, file);
  xml_end_element (file, "errordomain");
  g_base_info_unref (enum_);
}

static void
write_union_info (const gchar *namespace, 
		  GIUnionInfo *info, 
		  Xml         *file)
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
  
  xml_start_element (file, "union");
  xml_printf (file, " name=\"%s\"", name);
  
  if (type_name)
    xml_printf (file, " type-name=\"%s\" get-type=\"%s\"", type_name, type_init);
	  
  if (deprecated)
    xml_printf (file, " deprecated=\"1\"");
	

  if (g_union_info_is_discriminated (info))
    {
      gint offset;
      GITypeInfo *type;

      offset = g_union_info_get_discriminator_offset (info);
      type = g_union_info_get_discriminator_type (info);
      
      xml_start_element (file, "discriminator");
      xml_printf (file, " offset=\"%d\" type=\"", offset);
      write_type_info (namespace, type, file);
      xml_end_element (file, "discriminator");
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
      write_function_info (namespace, function, file);
      g_base_info_unref ((GIBaseInfo *)function);
    }

  xml_end_element (file, "union");
}

static void
write_repository (const char   *namespace,
		  gboolean      needs_prefix)
{
  FILE *ofile;
  gchar *ns;
  gint i, j;
  char **dependencies;
  GIRepository *repository;
  Xml *xml;

  repository = g_irepository_get_default ();

  if (output == NULL)
    ofile = stdout;
  else
    {
      gchar *filename;
      
      if (needs_prefix)
	filename = g_strdup_printf ("%s-%s", namespace, output);  
      else
	filename = g_strdup (output);
      ofile = g_fopen (filename, "w");
      
      if (ofile == NULL)
	{
	  g_fprintf (stderr, "failed to open '%s': %s\n",
		     filename, g_strerror (errno));
	  g_free (filename);
	  
	  return;
	}
      
      g_free (filename);
    }

  xml = xml_open (ofile);
  
  xml_printf (xml, "<?xml version=\"1.0\"?>\n");
  xml_start_element (xml, "repository");
  xml_printf (xml, " version=\"1.0\"\n"
	      "            xmlns=\"http://www.gtk.org/introspection/core/1.0\"\n"
	      "            xmlns:c=\"http://www.gtk.org/introspection/c/1.0\"\n"
	      "            xmlns:glib=\"http://www.gtk.org/introspection/glib/1.0\"");

  dependencies = g_irepository_get_dependencies (repository,
						 namespace);
  if (dependencies != NULL)
    {
      for (i = 0; dependencies[i]; i++)
	{
	  xml_start_element (xml, "include");
	  xml_printf (xml, " name=\"%s\"", dependencies[i]);
	  xml_end_element (xml, "include");
	}
    }

  if (TRUE)
    {
      const gchar *shared_library;
      const char *ns = namespace;
      const char *version;

      version = g_irepository_get_version (repository, ns);

      shared_library = g_irepository_get_shared_library (repository, ns);
      xml_start_element (xml, "namespace");
      xml_printf (xml, " name=\"%s\" version=\"%s\"", ns, version);
      if (shared_library)
        xml_printf (xml, " shared-library=\"%s\"", shared_library);
      
      for (j = 0; j < g_irepository_get_n_infos (repository, ns); j++)
	{
	  GIBaseInfo *info = g_irepository_get_info (repository, ns, j);
	  switch (g_base_info_get_type (info))
	    {
	    case GI_INFO_TYPE_FUNCTION:
	      write_function_info (ns, (GIFunctionInfo *)info, xml);
	      break;
	      
	    case GI_INFO_TYPE_CALLBACK:
	      write_callback_info (ns, (GICallbackInfo *)info, xml);
	      break;

	    case GI_INFO_TYPE_STRUCT:
	    case GI_INFO_TYPE_BOXED:
	      write_struct_info (ns, (GIStructInfo *)info, xml);
	      break;

	    case GI_INFO_TYPE_UNION:
	      write_union_info (ns, (GIUnionInfo *)info, xml);
	      break;

	    case GI_INFO_TYPE_ENUM:
	    case GI_INFO_TYPE_FLAGS:
	      write_enum_info (ns, (GIEnumInfo *)info, xml);
	      break;
	      
	    case GI_INFO_TYPE_CONSTANT:
	      write_constant_info (ns, (GIConstantInfo *)info, xml);
	      break;

	    case GI_INFO_TYPE_OBJECT:
	      write_object_info (ns, (GIObjectInfo *)info, xml);
	      break;

	    case GI_INFO_TYPE_INTERFACE:
	      write_interface_info (ns, (GIInterfaceInfo *)info, xml);
	      break;

	    case GI_INFO_TYPE_ERROR_DOMAIN:
	      write_error_domain_info (ns, (GIErrorDomainInfo *)info, xml);
	      break;

	    default:
	      g_error ("unknown info type %d\n", g_base_info_get_type (info));
	    }

	  g_base_info_unref (info);
	}

      xml_end_element (xml, "namespace");
    }

  xml_end_element (xml, "repository");
      
  xml_free (xml);
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
  if (handle == NULL)
    {
      g_printerr ("Could not load typelib from '%s': %s\n", 
		  filename, g_module_error ());
      return NULL;
    }

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
  gboolean shlib = FALSE;
  gchar **input = NULL;
  GOptionContext *context;
  GError *error = NULL;
  gboolean needs_prefix;
  gint i;
  GTypelib *data;
  GOptionEntry options[] = 
    {
      { "shlib", 0, 0, G_OPTION_ARG_NONE, &shlib, "handle typelib embedded in shlib", NULL },
      { "output", 'o', 0, G_OPTION_ARG_FILENAME, &output, "output file", "FILE" }, 
      { "includedir", 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &includedirs, "include directories in GIR search path", NULL }, 
      { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &input, NULL, NULL },
      { NULL, }
    };

  g_log_set_always_fatal (G_LOG_LEVEL_WARNING | G_LOG_LEVEL_CRITICAL);

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

  if (includedirs != NULL)
    for (i = 0; includedirs[i]; i++)
      g_irepository_prepend_search_path (includedirs[i]);

  for (i = 0; input[i]; i++)
    {
      GModule *dlhandle = NULL;
      const guchar *typelib;
      gsize len;
      const char *namespace;

      if (!shlib)
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
	  return 1;
        }
      }
      namespace = g_irepository_load_typelib (g_irepository_get_default (), data, 0,
					      &error);
      if (namespace == NULL)
	{
	  g_printerr ("failed to load typelib: %s\n", error->message);
	  return 1;
	}
	
      write_repository (namespace, needs_prefix);

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
