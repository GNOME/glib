/*
 * Copyright © 2013 Canonical Limited
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Ryan Lortie <desrt@desrt.ca>
 */

#include "config.h"

#include "gmarkupreader.h"

#include "glib/gmarkup-private.h"

#include <gio.h>

typedef enum
{
  READER_STATE_NONE,
  READER_STATE_EOF,
  READER_STATE_START_ELEMENT,
  READER_STATE_END_ELEMENT,
  READER_STATE_TEXT,
  READER_STATE_PASSTHROUGH,
  READER_STATE_ERROR
} GMarkupReaderState;

struct _GMarkupReader
{
  GObject parent_instance;

  GMarkupParseContext *context;
  GInputStream        *stream;
  GMarkupParser        parser;

  GBytes              *current_buffer;
  gboolean             non_blocking;

  GMarkupReaderState   state;
  gchar               *element_name;
  gchar              **attribute_names;
  gchar              **attribute_values;
  GBytes              *content;
};

typedef GObjectClass GMarkupReaderClass;

G_DEFINE_TYPE (GMarkupReader, g_markup_reader, G_TYPE_OBJECT)

enum
{
  PROP_0,
  PROP_STREAM,
  PROP_FLAGS
};

static void
g_markup_reader_start_element (GMarkupParseContext  *context,
                               const gchar          *element_name,
                               const gchar         **attribute_names,
                               const gchar         **attribute_values,
                               gpointer              user_data,
                               GError              **error)
{
  GMarkupReader *reader = user_data;

  g_assert (reader->state == READER_STATE_NONE);

  reader->element_name = g_strdup (element_name);
  reader->attribute_names = g_strdupv ((gchar **) attribute_names);
  reader->attribute_values = g_strdupv ((gchar **) attribute_values);
  reader->state = READER_STATE_START_ELEMENT;
}

static void
g_markup_reader_end_element (GMarkupParseContext  *context,
                             const gchar          *element_name,
                             gpointer              user_data,
                             GError              **error)
{
  GMarkupReader *reader = user_data;

  g_assert (reader->state == READER_STATE_NONE);

  reader->element_name = g_strdup (element_name);
  reader->state = READER_STATE_END_ELEMENT;
}

static void
g_markup_reader_text (GMarkupParseContext  *context,
                      const gchar          *text,
                      gsize                 text_length,
                      gpointer              user_data,
                      GError              **error)
{
  GMarkupReader *reader = user_data;

  g_assert (reader->state == READER_STATE_NONE);

  reader->content = g_bytes_new (text, text_length);
  reader->state = READER_STATE_TEXT;
}

static void
g_markup_reader_passthrough (GMarkupParseContext  *context,
                             const gchar          *text,
                             gsize                 text_length,
                             gpointer              user_data,
                             GError              **error)
{
  GMarkupReader *reader = user_data;

  g_assert (reader->state == READER_STATE_NONE);

  reader->content = g_bytes_new (text, text_length);
  reader->state = READER_STATE_PASSTHROUGH;
}

static void
g_markup_reader_set_property (GObject *object, guint prop_id,
                              const GValue *value, GParamSpec *pspec)
{
  GMarkupReader *reader = G_MARKUP_READER (object);

  switch (prop_id)
    {
    case PROP_STREAM:
      reader->stream = g_value_dup_object (value);
      break;

    case PROP_FLAGS:
      reader->context->flags = g_value_get_uint (value);
      if (reader->context->flags & G_MARKUP_IGNORE_PASSTHROUGH)
        reader->parser.passthrough = NULL;
      break;

    default:
      g_assert_not_reached ();
    }
}

static void
g_markup_reader_finalize (GObject *object)
{
  GMarkupReader *reader = G_MARKUP_READER (object);

  g_markup_parse_context_free (reader->context);
  g_object_unref (reader->stream);
  g_free (reader->element_name);
  g_strfreev (reader->attribute_names);
  g_strfreev (reader->attribute_values);
  if (reader->current_buffer)
    g_bytes_unref (reader->current_buffer);
  if (reader->content)
    g_bytes_unref (reader->content);

  G_OBJECT_CLASS (g_markup_reader_parent_class)->finalize (object);
}

static void
g_markup_reader_init (GMarkupReader *reader)
{
  reader->parser.start_element = g_markup_reader_start_element;
  reader->parser.end_element = g_markup_reader_end_element;
  reader->parser.text = g_markup_reader_text;
  reader->parser.passthrough = g_markup_reader_passthrough;

  reader->context = g_markup_parse_context_new (&reader->parser, 0, reader, NULL);
}

static void
g_markup_reader_class_init (GMarkupReaderClass *class)
{
  class->set_property = g_markup_reader_set_property;
  class->finalize = g_markup_reader_finalize;

  g_object_class_install_property (class, PROP_STREAM,
                                   g_param_spec_object ("stream", "stream", "input stream",
                                                        G_TYPE_INPUT_STREAM, G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (class, PROP_FLAGS,
                                   g_param_spec_uint ("flags", "flags", "flags",
                                                      0, G_MAXUINT, 0, G_PARAM_WRITABLE |
                                                      G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
}

GMarkupReader *
g_markup_reader_new (GInputStream         *stream,
                     GMarkupParseFlags     flags)
{
  return g_object_new (G_TYPE_MARKUP_READER,
                       "stream", stream,
                       "flags", flags,
                       NULL);
}

static gboolean
g_markup_reader_ensure_data (GMarkupReader  *reader,
                             GCancellable   *cancellable,
                             GError        **error)
{
  g_assert (reader->state == READER_STATE_NONE);

  if (reader->context->iter != reader->context->current_text_end)
    return TRUE;

  if (reader->current_buffer)
    {
      g_bytes_unref (reader->current_buffer);
      reader->context->start = NULL;
      reader->context->iter = NULL;
      reader->context->current_text = NULL;
      reader->context->current_text_len = 0;
      reader->context->current_text_end = NULL;
      reader->current_buffer = NULL;
    }

  if (reader->non_blocking)
    {
      if (!G_IS_POLLABLE_INPUT_STREAM (reader->stream) ||
          !g_pollable_input_stream_is_readable (G_POLLABLE_INPUT_STREAM (reader->stream)))
        {
          g_set_error (error, G_IO_ERROR, G_IO_ERROR_WOULD_BLOCK, "Operation would block");
          return FALSE;
        }
    }

  reader->current_buffer = g_input_stream_read_bytes (reader->stream, 1024 * 1024, cancellable, error);

  if (!reader->current_buffer)
    {
      reader->state = READER_STATE_ERROR;
      return FALSE;
    }

  if (g_bytes_get_size (reader->current_buffer) == 0) /* EOF */
    {
      reader->state = READER_STATE_EOF;
      return TRUE;
    }

  reader->context->current_text = g_bytes_get_data (reader->current_buffer, &reader->context->current_text_len);
  reader->context->current_text_end = reader->context->current_text + reader->context->current_text_len;
  reader->context->iter = reader->context->current_text;
  reader->context->start = reader->context->iter;

  return TRUE;
}

static void
g_markup_reader_clear (GMarkupReader *reader)
{
  g_free (reader->element_name);
  reader->element_name = NULL;
  g_strfreev (reader->attribute_names);
  reader->attribute_names = NULL;
  g_strfreev (reader->attribute_values);
  reader->attribute_values = NULL;

  if (reader->content)
    {
      g_bytes_unref (reader->content);
      reader->content = NULL;
    }

  reader->state = READER_STATE_NONE;
}

gboolean
g_markup_reader_advance (GMarkupReader  *reader,
                         GCancellable   *cancellable,
                         GError        **error)
{
  g_return_val_if_fail (G_IS_MARKUP_READER (reader), FALSE);
  g_return_val_if_fail (reader->state != READER_STATE_ERROR &&
                        reader->state != READER_STATE_EOF, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  g_markup_reader_clear (reader);

  while (reader->state == READER_STATE_NONE)
    {
      if (!g_markup_reader_ensure_data (reader, cancellable, error))
        return FALSE;

      if (!g_markup_parse_context_parse_slightly (reader->context, error))
        return FALSE;
    }

  return TRUE;
}

gboolean
g_markup_reader_is_start_element (GMarkupReader *reader,
                                  const gchar   *element_name)
{
  g_return_val_if_fail (G_IS_MARKUP_READER (reader), FALSE);

  return reader->state == READER_STATE_START_ELEMENT &&
         (!element_name || g_str_equal (reader->element_name, element_name));
}

gboolean
g_markup_reader_is_end_element (GMarkupReader *reader)
{
  g_return_val_if_fail (G_IS_MARKUP_READER (reader), FALSE);

  return reader->state == READER_STATE_END_ELEMENT;
}

gboolean
g_markup_reader_is_passthrough (GMarkupReader *reader)
{
  g_return_val_if_fail (G_IS_MARKUP_READER (reader), FALSE);

  return reader->state == READER_STATE_PASSTHROUGH;
}

gboolean
g_markup_reader_is_text (GMarkupReader *reader)
{
  g_return_val_if_fail (G_IS_MARKUP_READER (reader), FALSE);

  return reader->state == READER_STATE_TEXT;
}

gboolean
g_markup_reader_is_eof (GMarkupReader *reader)
{
  g_return_val_if_fail (G_IS_MARKUP_READER (reader), FALSE);

  return reader->state == READER_STATE_EOF;
}

const gchar *
g_markup_reader_get_element_name (GMarkupReader *reader)
{
  g_return_val_if_fail (G_IS_MARKUP_READER (reader), NULL);
  g_return_val_if_fail (reader->state == READER_STATE_START_ELEMENT ||
                        reader->state == READER_STATE_END_ELEMENT, NULL);

  return reader->element_name;
}

void
g_markup_reader_get_attributes (GMarkupReader        *reader,
                                const gchar * const **attribute_names,
                                const gchar * const **attribute_values)
{
  g_return_if_fail (G_IS_MARKUP_READER (reader));
  g_return_if_fail (reader->state == READER_STATE_START_ELEMENT);

  if (attribute_names)
    *attribute_names = (const gchar * const *) reader->attribute_names;

  if (attribute_values)
    *attribute_values = (const gchar * const *) reader->attribute_values;
}

void
g_markup_reader_collect_attributes (GMarkupReader       *reader,
                                    GError             **error,
                                    GMarkupCollectType   first_type,
                                    const gchar         *first_name,
                                    ...)
{
  g_return_if_fail (G_IS_MARKUP_READER (reader));
  g_return_if_fail (reader->state == READER_STATE_START_ELEMENT);

  g_assert_not_reached ();
}

GBytes *
g_markup_reader_get_content (GMarkupReader *reader)
{
  g_return_val_if_fail (G_IS_MARKUP_READER (reader), NULL);
  g_return_val_if_fail (reader->state == READER_STATE_TEXT || reader->state == READER_STATE_PASSTHROUGH, NULL);

  return reader->content;
}

gboolean
g_markup_reader_unexpected (GMarkupReader  *reader,
                            GError        **error)
{
  const GSList *stack;

  g_return_val_if_fail (reader->state == READER_STATE_START_ELEMENT ||
                        reader->state == READER_STATE_TEXT, FALSE);

  stack = g_markup_parse_context_get_element_stack (reader->context);

  if (reader->state == READER_STATE_START_ELEMENT)
    {
      if (stack->next)
        g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ELEMENT,
                     "Element <%s> is not valid inside of <%s>", reader->element_name, (gchar *) stack->next->data);
      else
        g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ELEMENT,
                     "Element <%s> is not valid at the document toplevel", reader->element_name);
    }
  else /* TEXT */
    {
      g_assert (stack->next);

      g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_UNKNOWN_ELEMENT,
                   "Text content is not valid inside of <%s>", (gchar *) stack->next->data);
    }

  /* always 'fail' */
  return FALSE;
}

gboolean
g_markup_reader_expect_end (GMarkupReader  *reader,
                            GCancellable   *cancellable,
                            GError        **error)
{
  /* Expect either EOF or end tag */
  while (g_markup_reader_advance (reader, cancellable, error))
    {
      if (g_markup_reader_is_end_element (reader))
        return TRUE;

      if (g_markup_reader_is_eof (reader))
        return TRUE;

      if (g_markup_reader_is_passthrough (reader))
        continue;

      if (g_markup_reader_is_text (reader))
        {
          const gchar *data;
          gsize length;
          gsize i;

          data = g_bytes_get_data (reader->content, &length);
          for (i = 0; i < length; i++)
            if (!g_ascii_isspace (data[i]))
              {
                const GSList *stack;

                stack = g_markup_parse_context_get_element_stack (reader->context);
                g_assert (stack->next);

                g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                             "Text content is not valid inside of <%s>", (gchar *) stack->next->data);
                return FALSE;
              }
        }
    }

  return TRUE;
}
