/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright (C) 2013  Emmanuele Bassi <ebassi@gnome.org>
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
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:gmarkupencoder
 * @Title: GMarkupEncoder
 * @Short_Description: Encodes and decodes data as markup
 *
 * #GMarkupEncoder is a class that allows encoding and decoding data
 * as an XML subset that can be parsed by #GMarkupParser.
 *
 * Note that you can only use #GMarkupEncoder to decode the output
 * of data encoded by a #GMarkupEncoder.
 */

#include "config.h"

#include "gmarkupencoder.h"
#include "gencoder.h"
#include "gioerror.h"
#include "glibintl.h"

#include <string.h>

struct _GMarkupEncoder
{
  GEncoder parent_instance;

  /* parser state */
  char *cur_key;
  char *cur_value;
  char *cur_value_type;

  guint in_entries : 1;
  guint in_entry   : 1;
  guint in_key     : 1;
  guint in_value   : 1;
};

struct _GMarkupEncoderClass
{
  GEncoderClass parent_class;
};

G_DEFINE_TYPE (GMarkupEncoder, g_markup_encoder, G_TYPE_ENCODER)

static void
g_markup_encoder_start_element (GMarkupParseContext *context,
                                const gchar         *element_name,
                                const gchar        **attribute_names,
                                const gchar        **attribute_values,
                                gpointer             user_data,
                                GError             **error)
{
  GMarkupEncoder *self = user_data;

  if (strcmp (element_name, "entries") == 0)
    {
      if (self->in_entries)
        {
          g_set_error_literal (error, G_MARKUP_ERROR,
                               G_MARKUP_ERROR_INVALID_CONTENT,
                               "The 'entries' tag cannot be nested");
          return;
        }

      g_assert (!self->in_entry);
      g_assert (!self->in_key);
      g_assert (!self->in_value);
      self->in_entries = TRUE;

      return;
    }

  if (strcmp (element_name, "entry") == 0)
    {
      if (!self->in_entries)
        {
          g_set_error_literal (error, G_MARKUP_ERROR,
                               G_MARKUP_ERROR_INVALID_CONTENT,
                               "The 'entry' tag can only be used inside an 'entries' tag");
          return;
        }

      g_assert (!self->in_entry);
      g_assert (!self->in_key);
      g_assert (!self->in_value);
      self->in_entry = TRUE;

      g_free (self->cur_key);
      self->cur_key = NULL;
      g_free (self->cur_value_type);
      self->cur_value_type = NULL;
      g_free (self->cur_value);
      self->cur_value = NULL;
      return;
    }

  if (strcmp (element_name, "key") == 0)
    {
      if (!self->in_entry)
        {
          g_set_error_literal (error, G_MARKUP_ERROR,
                               G_MARKUP_ERROR_INVALID_CONTENT,
                               "The 'key' tag can only be used inside an 'entry' tag");
          return;
        }

      g_assert (!self->in_value);
      self->in_key = TRUE;
      return;
    }

  if (strcmp (element_name, "value") == 0)
    {
      gboolean res;

      if (!self->in_entry)
        {
          g_set_error_literal (error, G_MARKUP_ERROR,
                               G_MARKUP_ERROR_INVALID_CONTENT,
                               "The 'value' tag can only be used inside an 'entry' tag");
          return;
        }

      res = g_markup_collect_attributes (element_name,
                                         attribute_names,
                                         attribute_values,
                                         error,
                                         G_MARKUP_COLLECT_STRDUP, "type", &self->cur_value_type,
                                         G_MARKUP_COLLECT_INVALID);
      if (!res)
        return;

      g_assert (!self->in_key);
      self->in_value = TRUE;
      return;
    }
}

static void
g_markup_encoder_add_current_entry (GMarkupEncoder  *self,
                                    GError         **error)
{
  GError *internal_error;
  GVariant *variant;

  if (self->cur_key == NULL)
    {
      if (self->cur_value_type != NULL)
        {
          g_set_error (error, G_IO_ERROR,
                       G_IO_ERROR_INVALID_DATA,
                       "No key defined for entry of type '%s'",
                       self->cur_value_type);
        }
      else
        {
          g_set_error_literal (error, G_IO_ERROR,
                               G_IO_ERROR_INVALID_DATA,
                               "No key defined for entry");
        }

      return;
    }

  if (self->cur_value_type == NULL)
    {
      g_set_error (error, G_IO_ERROR,
                   G_IO_ERROR_INVALID_DATA,
                   "No value type defined for key '%s'",
                   self->cur_key);
      return;
    }

  if (self->cur_value == NULL)
    {
      g_set_error (error, G_IO_ERROR,
                   G_IO_ERROR_INVALID_DATA,
                   "No value defined for key '%s' of type '%s'",
                   self->cur_key,
                   self->cur_value_type);
      return;
    }

  internal_error = NULL;
  variant = g_variant_parse (G_VARIANT_TYPE (self->cur_value_type),
                             self->cur_value,
                             NULL,
                             NULL,
                             &internal_error);
  if (internal_error != NULL)
    {
      g_set_error (error, G_IO_ERROR,
                   G_IO_ERROR_INVALID_DATA,
                   "Unable to parse the entry value: %s",
                   internal_error->message);
      g_error_free (internal_error);
      return;
    }

  g_encoder_add_key (G_ENCODER (self), self->cur_key, variant);
  g_variant_unref (variant);
}

static void
g_markup_encoder_end_element (GMarkupParseContext *context,
                              const gchar         *element_name,
                              gpointer             user_data,
                              GError             **error)
{
  GMarkupEncoder *self = user_data;

  if (strcmp (element_name, "entries") == 0)
    {
      self->in_entries = FALSE;
      return;
    }

  if (strcmp (element_name, "entry") == 0)
    {
      g_assert (self->in_entries);
      self->in_entry = FALSE;

      g_markup_encoder_add_current_entry (self, error);

      g_free (self->cur_key);
      self->cur_key = NULL;
      g_free (self->cur_value_type);
      self->cur_value_type = NULL;
      g_free (self->cur_value);
      self->cur_value = NULL;
      return;
    }

  if (strcmp (element_name, "key") == 0)
    {
      g_assert (self->in_entries);
      g_assert (self->in_entry);
      self->in_key = FALSE;
      return;
    }

  if (strcmp (element_name, "value") == 0)
    {
      g_assert (self->in_entries);
      g_assert (self->in_entry);
      self->in_value = FALSE;
      return;
    }

  g_set_error (error, G_MARKUP_ERROR,
               G_MARKUP_ERROR_UNKNOWN_ELEMENT,
               "Unknown element '%s' in markup",
               element_name);
}

static void
g_markup_encoder_text (GMarkupParseContext *context,
                       const gchar         *text,
                       gsize                text_len,
                       gpointer             user_data,
                       GError             **error)
{
  GMarkupEncoder *self = user_data;

  if (self->in_key)
    {
      g_free (self->cur_key);
      self->cur_key = g_strndup (text, text_len);
      return;
    }

  if (self->in_value)
    {
      g_free (self->cur_value);
      self->cur_value = g_strndup (text, text_len);
      return;
    }
}

static const GMarkupParser markup_parser = {
  /* .start_element = */ g_markup_encoder_start_element,
  /* .end_element   = */ g_markup_encoder_end_element,
  /* .text          = */ g_markup_encoder_text,
  /* .passthrough   = */ NULL,
  /* .error         = */ NULL,
};

static void
clear_parser_state (gpointer data)
{
  GMarkupEncoder *self = data;

  g_free (self->cur_key);
  self->cur_key = NULL;
  g_free (self->cur_value_type);
  self->cur_value_type = NULL;
  g_free (self->cur_value);
  self->cur_value = NULL;

  self->in_entries = FALSE;
  self->in_entry = FALSE;
  self->in_key = FALSE;
  self->in_value = FALSE;
}

static gboolean
g_markup_encoder_read_from_bytes (GEncoder  *encoder,
                                  GBytes    *buffer,
                                  GError   **error)
{
  GMarkupParseContext *context;
  GError *internal_error = NULL;

  clear_parser_state (encoder);

  context = g_markup_parse_context_new (&markup_parser, 0, encoder, clear_parser_state);

  g_markup_parse_context_parse (context,
                                g_bytes_get_data (buffer, NULL),
                                g_bytes_get_size (buffer),
                                &internal_error);

  g_markup_parse_context_free (context);

  if (internal_error)
    {
      g_set_error (error, G_IO_ERROR,
                   G_IO_ERROR_INVALID_DATA,
                   "Unable to read markup data: %s",
                   internal_error->message);
      g_error_free (internal_error);

      return FALSE;
    }

  return TRUE;
}

static GBytes *
g_markup_encoder_write_to_bytes (GEncoder  *encoder,
                                 GError   **error)
{
  GVariant *data = g_encoder_close (encoder);
  GVariant *entry;
  GVariantIter iter;
  GString *buffer;
  gsize len;

  buffer = g_string_sized_new (1024);

  g_string_append (buffer, "<?xml version=\"1.0\"?>\n");
  g_string_append (buffer, "<entries version=\"1.0\">\n");

  g_variant_iter_init (&iter, data);
  while ((entry = g_variant_iter_next_value (&iter)) != NULL)
    {
      GVariant *key = g_variant_get_child_value (entry, 0);
      GVariant *tmp = g_variant_get_child_value (entry, 1);
      GVariant *value;
      char *value_str;

      g_string_append (buffer, "  <entry>\n");

      value = g_variant_get_variant (tmp);
      value_str = g_variant_print (value, FALSE);

      g_string_append_printf (buffer,
                              "    <key>%s</key>\n",
                              g_variant_get_string (key, NULL));

      g_string_append_printf (buffer,
                              "    <value type=\"%s\">%s</value>\n",
                              (const char *) g_variant_get_type (value),
                              value_str);

      g_free (value_str);
      g_variant_unref (value);
      g_variant_unref (tmp);
      g_variant_unref (key);

      g_string_append (buffer, "  </entry>\n");
    }

  g_string_append (buffer, "</entries>");
  len = buffer->len;

  return g_bytes_new_take (g_string_free (buffer, FALSE), len);
}

static void
g_markup_encoder_class_init (GMarkupEncoderClass *klass)
{
  GEncoderClass *encoder_class = G_ENCODER_CLASS (klass);

  encoder_class->read_from_bytes = g_markup_encoder_read_from_bytes;
  encoder_class->write_to_bytes = g_markup_encoder_write_to_bytes;
}

static void
g_markup_encoder_init (GMarkupEncoder *self)
{
}

/**
 * g_markup_encoder_new:
 *
 * Creates a new #GMarkupEncoder instance.
 *
 * Returns: (transfer full): the newly created #GMarkupEncoder instance.
 *   Use g_object_unref() when done.
 *
 * Since: 2.38
 */
GEncoder *
g_markup_encoder_new (void)
{
  return g_object_new (G_TYPE_MARKUP_ENCODER, NULL);
}
