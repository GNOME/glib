/*
 *  Copyright 2000, 2003 Red Hat, Inc.
 *  Copyright 2007, 2008 Ryan Lortie <desrt@desrt.ca>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the licence, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __G_MARKUP_PRIVATE_H__
#define __G_MARKUP_PRIVATE_H__

#include "gstring.h"

typedef enum
{
  STATE_START,
  STATE_AFTER_OPEN_ANGLE,
  STATE_AFTER_CLOSE_ANGLE,
  STATE_AFTER_ELISION_SLASH, /* the slash that obviates need for end element */
  STATE_INSIDE_OPEN_TAG_NAME,
  STATE_INSIDE_ATTRIBUTE_NAME,
  STATE_AFTER_ATTRIBUTE_NAME,
  STATE_BETWEEN_ATTRIBUTES,
  STATE_AFTER_ATTRIBUTE_EQUALS_SIGN,
  STATE_INSIDE_ATTRIBUTE_VALUE_SQ,
  STATE_INSIDE_ATTRIBUTE_VALUE_DQ,
  STATE_INSIDE_TEXT,
  STATE_AFTER_CLOSE_TAG_SLASH,
  STATE_INSIDE_CLOSE_TAG_NAME,
  STATE_AFTER_CLOSE_TAG_NAME,
  STATE_INSIDE_PASSTHROUGH,
  STATE_ERROR
} GMarkupParseState;

typedef struct
{
  const char *prev_element;
  const GMarkupParser *prev_parser;
  gpointer prev_user_data;
} GMarkupRecursionTracker;

struct _GMarkupParseContext
{
  const GMarkupParser *parser;

  volatile gint ref_count;

  GMarkupParseFlags flags;

  gint line_number;
  gint char_number;

  GMarkupParseState state;

  gpointer user_data;
  GDestroyNotify dnotify;

  /* A piece of character data or an element that
   * hasn't "ended" yet so we haven't yet called
   * the callback for it.
   */
  GString *partial_chunk;
  GSList *spare_chunks;

  GSList *tag_stack;
  GSList *tag_stack_gstr;
  GSList *spare_list_nodes;

  GString **attr_names;
  GString **attr_values;
  gint cur_attr;
  gint alloc_attrs;

  const gchar *current_text;
  gsize        current_text_len;
  const gchar *current_text_end;

  /* used to save the start of the last interesting thingy */
  const gchar *start;

  const gchar *iter;

  guint document_empty : 1;
  guint parsing : 1;
  guint awaiting_pop : 1;
  gint balance;

  /* subparser support */
  GSList *subparser_stack; /* (GMarkupRecursionTracker *) */
  const char *subparser_element;
  gpointer held_user_data;
};

gboolean
g_markup_parse_context_parse_slightly (GMarkupParseContext  *context,
                                       GError              **error);

gboolean
g_markup_collect_attributesv (const gchar         *element_name,
                              const gchar        **attribute_names,
                              const gchar        **attribute_values,
                              GError             **error,
                              GMarkupCollectType   first_type,
                              const gchar         *first_attr,
                              va_list              ap);

#endif /* __G_MARKUP_PRIVATE_H__ */
