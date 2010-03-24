/* GObject introspection: gen-introspect
 *
 * Copyright (C) 2007  Jürg Billeter
 * Copyright (C) 2007  Johan Dahlin
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
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
 *
 * Author:
 * 	Jürg Billeter <j@bitron.ch>
 */

#include <stdio.h>
#include <glib.h>
#include "scanner.h"
#include "gidlnode.h"

typedef struct {
  int indent;
  FILE *output;
} GIdlWriter;

static void node_generate (GIdlWriter * writer, GIdlNode * node);

static void
g_writer_write_inline (GIdlWriter * writer, const char *s)
{
  fprintf (writer->output, "%s", s);
}

static void
g_writer_write (GIdlWriter * writer, const char *s)
{
  int i;
  for (i = 0; i < writer->indent; i++)
    {
      fprintf (writer->output, "\t");
    }

  g_writer_write_inline (writer, s);
}

static void
g_writer_write_indent (GIdlWriter * writer, const char *s)
{
  g_writer_write (writer, s);
  writer->indent++;
}

static void
g_writer_write_unindent (GIdlWriter * writer, const char *s)
{
  writer->indent--;
  g_writer_write (writer, s);
}

static void
field_generate (GIdlWriter * writer, GIdlNodeField * node)
{
  char *markup =
    g_markup_printf_escaped ("<field name=\"%s\" type=\"%s\"/>\n",
			     node->node.name, node->type->unparsed);
  g_writer_write (writer, markup);
  g_free (markup);
}

static void
value_generate (GIdlWriter * writer, GIdlNodeValue * node)
{
  char *markup =
    g_markup_printf_escaped ("<member name=\"%s\" value=\"%d\"/>\n",
			     node->node.name, node->value);
  g_writer_write (writer, markup);
  g_free (markup);
}

static void
constant_generate (GIdlWriter * writer, GIdlNodeConstant * node)
{
  char *markup =
    g_markup_printf_escaped
    ("<constant name=\"%s\" type=\"%s\" value=\"%s\"/>\n", node->node.name,
     node->type->unparsed, node->value);
  g_writer_write (writer, markup);
  g_free (markup);
}

static void
property_generate (GIdlWriter * writer, GIdlNodeProperty * node)
{
  char *markup =
    g_markup_printf_escaped ("<property name=\"%s\" "
			     "type=\"%s\" "
			     "readable=\"%s\" "
			     "writable=\"%s\" "
			     "construct=\"%s\" "
			     "construct-only=\"%s\"/>\n",
			     node->node.name,
			     node->type->unparsed,
			     node->readable ? "1" : "0",
			     node->writable ? "1" : "0",
			     node->construct ? "1" : "0",
			     node->construct_only ? "1" : "0");
  g_writer_write (writer, markup);
  g_free (markup);
}

static void
function_generate (GIdlWriter * writer, GIdlNodeFunction * node)
{
  const char *tag_name;
  GString *markup_s;
  gchar *markup;

  if (node->node.type == G_IR_NODE_CALLBACK)
    tag_name = "callback";
  else if (node->is_constructor)
    tag_name = "constructor";
  else if (node->is_method)
    tag_name = "method";
  else
    tag_name = "function";

  markup_s = g_string_new ("<");
  g_string_append_printf (markup_s,
			  "%s name=\"%s\"",
			  tag_name, node->node.name);

  if (node->node.type != G_IR_NODE_CALLBACK)
    g_string_append_printf (markup_s,
			    g_markup_printf_escaped (" symbol=\"%s\"", node->symbol));

  if (node->deprecated)
    g_string_append_printf (markup_s, " deprecated=\"1\"");

  g_string_append (markup_s, ">\n");

  g_writer_write_indent (writer, markup_s->str);
  g_string_free (markup_s, TRUE);

  markup_s =
    g_string_new (g_markup_printf_escaped ("<return-type type=\"%s\"",
			     node->result->type->unparsed));

  if (node->result->transfer)
    g_string_append (markup_s, g_markup_printf_escaped (" transfer=\"full\"/>\n"));
  else
    g_string_append (markup_s, "/>\n");

  g_writer_write (writer, markup_s->str);
  g_string_free (markup_s, TRUE);

  if (node->parameters != NULL)
    {
      GList *l;
      g_writer_write_indent (writer, "<parameters>\n");
      for (l = node->parameters; l != NULL; l = l->next)
	{
	  GIdlNodeParam *param = l->data;
	  const gchar *direction = g_idl_node_param_direction_string (param);

	  markup_s = g_string_new ("<parameter");

	  g_string_append_printf (markup_s, " name=\"%s\"", param->node.name);

	  g_string_append (markup_s,
			   g_markup_printf_escaped (" type=\"%s\"",
						    param->type->unparsed));

	  if (param->transfer)
	    g_string_append (markup_s,
			   g_markup_printf_escaped (" transfer=\"full\""));

	  if (param->allow_none)
	    g_string_append (markup_s,
			     g_markup_printf_escaped (" allow-none=\"1\""));

	  if (strcmp (direction, "in") != 0)
	    g_string_append (markup_s,
			     g_markup_printf_escaped (" direction=\"%s\"",
						      direction));

	  g_string_append (markup_s, "/>\n");

	  g_writer_write (writer, markup_s->str);
	  g_string_free (markup_s, TRUE);
	}
      g_writer_write_unindent (writer, "</parameters>\n");
    }
  markup = g_strdup_printf ("</%s>\n", tag_name);
  g_writer_write_unindent (writer, markup);
  g_free (markup);
}

static void
vfunc_generate (GIdlWriter * writer, GIdlNodeVFunc * node)
{
  char *markup =
    g_markup_printf_escaped ("<vfunc name=\"%s\">\n", node->node.name);
  g_writer_write_indent (writer, markup);
  g_free (markup);
  markup =
    g_markup_printf_escaped ("<return-type type=\"%s\"/>\n",
			     node->result->type->unparsed);
  g_writer_write (writer, markup);
  g_free (markup);
  if (node->parameters != NULL)
    {
      GList *l;
      g_writer_write_indent (writer, "<parameters>\n");
      for (l = node->parameters; l != NULL; l = l->next)
	{
	  GIdlNodeParam *param = l->data;
	  markup =
	    g_markup_printf_escaped ("<parameter name=\"%s\" type=\"%s\"/>\n",
				     param->node.name, param->type->unparsed);
	  g_writer_write (writer, markup);
	  g_free (markup);
	}
      g_writer_write_unindent (writer, "</parameters>\n");
    }
  g_writer_write_unindent (writer, "</vfunc>\n");
}

static void
signal_generate (GIdlWriter * writer, GIdlNodeSignal * node)
{
  char *markup;
  const char *when = "LAST";
  if (node->run_first)
    {
      when = "FIRST";
    }
  else if (node->run_cleanup)
    {
      when = "CLEANUP";
    }
  markup =
    g_markup_printf_escaped ("<signal name=\"%s\" when=\"%s\">\n",
			     node->node.name, when);
  g_writer_write_indent (writer, markup);
  g_free (markup);
  markup =
    g_markup_printf_escaped ("<return-type type=\"%s\"/>\n",
			     node->result->type->unparsed);
  g_writer_write (writer, markup);
  g_free (markup);
  if (node->parameters != NULL)
    {
      GList *l;
      g_writer_write_indent (writer, "<parameters>\n");
      for (l = node->parameters; l != NULL; l = l->next)
	{
	  GIdlNodeParam *param = l->data;
	  markup =
	    g_markup_printf_escaped ("<parameter name=\"%s\" type=\"%s\"/>\n",
				     param->node.name, param->type->unparsed);
	  g_writer_write (writer, markup);
	  g_free (markup);
	}
      g_writer_write_unindent (writer, "</parameters>\n");
    }
  g_writer_write_unindent (writer, "</signal>\n");
}

static void
interface_generate (GIdlWriter * writer, GIdlNodeInterface * node)
{
  GList *l;
  char *markup;
  if (node->node.type == G_IR_NODE_OBJECT)
    {
      markup =
	g_markup_printf_escaped ("<object name=\"%s\" "
				 "parent=\"%s\" "
				 "type-name=\"%s\" "
				 "get-type=\"%s\">\n",
				 node->node.name,
				 node->parent,
				 node->gtype_name,
				 node->gtype_init);
    }
  else if (node->node.type == G_IR_NODE_INTERFACE)
    {
      markup =
	g_markup_printf_escaped
	("<interface name=\"%s\" type-name=\"%s\" get-type=\"%s\">\n",
	 node->node.name, node->gtype_name, node->gtype_init);
    }

  g_writer_write_indent (writer, markup);
  g_free (markup);
  if (node->node.type == G_IR_NODE_OBJECT && node->interfaces != NULL)
    {
      GList *l;
      g_writer_write_indent (writer, "<implements>\n");
      for (l = node->interfaces; l != NULL; l = l->next)
	{
	  markup =
	    g_markup_printf_escaped ("<interface name=\"%s\"/>\n",
				     (char *) l->data);
	  g_writer_write (writer, markup);
	  g_free (markup);
	}
      g_writer_write_unindent (writer, "</implements>\n");
    }
  else if (node->node.type == G_IR_NODE_INTERFACE
	   && node->prerequisites != NULL)
    {
      GList *l;
      g_writer_write_indent (writer, "<requires>\n");
      for (l = node->prerequisites; l != NULL; l = l->next)
	{
	  markup =
	    g_markup_printf_escaped ("<interface name=\"%s\"/>\n",
				     (char *) l->data);
	  g_writer_write (writer, markup);
	  g_free (markup);
	}
      g_writer_write_unindent (writer, "</requires>\n");
    }

  for (l = node->members; l != NULL; l = l->next)
    {
      node_generate (writer, l->data);
    }

  if (node->node.type == G_IR_NODE_OBJECT)
    {
      g_writer_write_unindent (writer, "</object>\n");
    }
  else if (node->node.type == G_IR_NODE_INTERFACE)
    {
      g_writer_write_unindent (writer, "</interface>\n");
    }
}

static void
struct_generate (GIdlWriter * writer, GIdlNodeStruct * node)
{
  GList *l;
  char *markup =
    g_markup_printf_escaped ("<struct name=\"%s\">\n", node->node.name);
  g_writer_write_indent (writer, markup);
  g_free (markup);
  for (l = node->members; l != NULL; l = l->next)
    {
      node_generate (writer, l->data);
    }
  g_writer_write_unindent (writer, "</struct>\n");
}

static void
union_generate (GIdlWriter * writer, GIdlNodeUnion * node)
{
  GList *l;
  char *markup =
    g_markup_printf_escaped ("<union name=\"%s\">\n", node->node.name);
  g_writer_write_indent (writer, markup);
  g_free (markup);
  for (l = node->members; l != NULL; l = l->next)
    {
      node_generate (writer, l->data);
    }
  g_writer_write_unindent (writer, "</union>\n");
}

static void
boxed_generate (GIdlWriter * writer, GIdlNodeBoxed * node)
{
  GList *l;
  char *markup =
    g_markup_printf_escaped
    ("<boxed name=\"%s\" type-name=\"%s\" get-type=\"%s\">\n",
     node->node.name, node->gtype_name, node->gtype_init);
  g_writer_write_indent (writer, markup);
  g_free (markup);
  for (l = node->members; l != NULL; l = l->next)
    {
      node_generate (writer, l->data);
    }
  g_writer_write_unindent (writer, "</boxed>\n");
}

static void
enum_generate (GIdlWriter * writer, GIdlNodeEnum * node)
{
  GList *l;
  GString *markup_s;
  char *markup;
  const char *tag_name = NULL;

  if (node->node.type == G_IR_NODE_ENUM)
    {
      tag_name = "enum";
    }
  else if (node->node.type == G_IR_NODE_FLAGS)
    {
      tag_name = "flags";
    }

  markup_s = g_string_new ("<");
  g_string_append_printf (markup_s,
			  "%s name=\"%s\"",
			  tag_name, node->node.name);

  if (node->gtype_name != NULL)
    g_string_append_printf (markup_s,
			    g_markup_printf_escaped (" type-name=\"%s\"", node->gtype_name));

  if (node->gtype_init != NULL)
    g_string_append_printf (markup_s,
			    g_markup_printf_escaped (" get-type=\"%s\"", node->gtype_init));

  if (node->deprecated)
    g_string_append_printf (markup_s, " deprecated=\"1\"");

  g_string_append (markup_s, ">\n");

  g_writer_write_indent (writer, markup_s->str);
  g_string_free (markup_s, TRUE);

  for (l = node->values; l != NULL; l = l->next)
    {
      node_generate (writer, l->data);
    }

  markup = g_strdup_printf ("</%s>\n", tag_name);
  g_writer_write_unindent (writer, markup);
  g_free (markup);
}

static void
node_generate (GIdlWriter * writer, GIdlNode * node)
{
  switch (node->type)
    {
    case G_IR_NODE_FUNCTION:
    case G_IR_NODE_CALLBACK:
      function_generate (writer, (GIdlNodeFunction *) node);
      break;
    case G_IR_NODE_VFUNC:
      vfunc_generate (writer, (GIdlNodeVFunc *) node);
      break;
    case G_IR_NODE_OBJECT:
    case G_IR_NODE_INTERFACE:
      interface_generate (writer, (GIdlNodeInterface *) node);
      break;
    case G_IR_NODE_STRUCT:
      struct_generate (writer, (GIdlNodeStruct *) node);
      break;
    case G_IR_NODE_UNION:
      union_generate (writer, (GIdlNodeUnion *) node);
      break;
    case G_IR_NODE_BOXED:
      boxed_generate (writer, (GIdlNodeBoxed *) node);
      break;
    case G_IR_NODE_ENUM:
    case G_IR_NODE_FLAGS:
      enum_generate (writer, (GIdlNodeEnum *) node);
      break;
    case G_IR_NODE_PROPERTY:
      property_generate (writer, (GIdlNodeProperty *) node);
      break;
    case G_IR_NODE_FIELD:
      field_generate (writer, (GIdlNodeField *) node);
      break;
    case G_IR_NODE_SIGNAL:
      signal_generate (writer, (GIdlNodeSignal *) node);
      break;
    case G_IR_NODE_VALUE:
      value_generate (writer, (GIdlNodeValue *) node);
      break;
    case G_IR_NODE_CONSTANT:
      constant_generate (writer, (GIdlNodeConstant *) node);
      break;
    default:
      g_assert_not_reached ();
    }
}

static void
g_writer_write_module (GIdlWriter * writer, GIdlModule * module)
{
  GList *l;
  char *markup =
    g_markup_printf_escaped ("<namespace name=\"%s\">\n", module->name);
  g_writer_write_indent (writer, markup);
  g_free (markup);
  for (l = module->entries; l != NULL; l = l->next)
    {
      node_generate (writer, l->data);
    }
  g_writer_write_unindent (writer, "</namespace>\n");
}

void
g_idl_writer_save_file (GIdlModule *module,
			const gchar *filename)
{
  GIdlWriter *writer;

  writer = g_new0 (GIdlWriter, 1);

  if (!filename)
    writer->output = stdout;
  else
    writer->output = fopen (filename, "w");

  g_writer_write (writer, "<?xml version=\"1.0\"?>\n");
  g_writer_write_indent (writer, "<repository version=\"1.0\""
			 "xmlns=\"http://www.gtk.org/introspection/core/1.0\""
			 "xmlns:c=\"http://www.gtk.org/introspection/c/1.0\""
			 "xmlns:glib=\"http://www.gtk.org/introspection/glib/1.0\">");
  g_writer_write_module (writer, module);
  g_writer_write_unindent (writer, "</repository>\n");

  if (filename)
    fclose (writer->output);
}
