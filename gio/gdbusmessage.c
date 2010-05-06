/* GDBus - GLib D-Bus Library
 *
 * Copyright (C) 2008-2009 Red Hat, Inc.
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
 *
 * Author: David Zeuthen <davidz@redhat.com>
 */

#include "config.h"

#include <glib/gi18n.h>

#include "gdbusutils.h"
#include "gdbusmessage.h"
#include "gdbuserror.h"
#include "gioenumtypes.h"
#include "ginputstream.h"
#include "gdatainputstream.h"
#include "gmemoryinputstream.h"
#include "goutputstream.h"
#include "gdataoutputstream.h"
#include "gmemoryoutputstream.h"
#include "gseekable.h"
#include "gioerror.h"

#ifdef G_OS_UNIX
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#endif


/**
 * SECTION:gdbusmessage
 * @short_description: D-Bus Message
 * @include: gio/gio.h
 *
 * A type for representing D-Bus messages that can be sent or received
 * on a #GDBusConnection.
 */

struct _GDBusMessagePrivate
{
  GDBusMessageType type;
  GDBusMessageFlags flags;
  guchar major_protocol_version;
  guint32 serial;
  GHashTable *headers;
  GVariant *body;
#ifdef G_OS_UNIX
  GUnixFDList *fd_list;
#endif
};

#define g_dbus_message_get_type g_dbus_message_get_gtype
G_DEFINE_TYPE (GDBusMessage, g_dbus_message, G_TYPE_OBJECT);
#undef g_dbus_message_get_type

static void
g_dbus_message_finalize (GObject *object)
{
  GDBusMessage *message = G_DBUS_MESSAGE (object);

  if (message->priv->headers != NULL)
    g_hash_table_unref (message->priv->headers);
  if (message->priv->body != NULL)
    g_variant_unref (message->priv->body);
#ifdef G_OS_UNIX
  if (message->priv->fd_list != NULL)
    g_object_unref (message->priv->fd_list);
#endif

  if (G_OBJECT_CLASS (g_dbus_message_parent_class)->finalize != NULL)
    G_OBJECT_CLASS (g_dbus_message_parent_class)->finalize (object);
}

static void
g_dbus_message_class_init (GDBusMessageClass *klass)
{
  GObjectClass *gobject_class;

  g_type_class_add_private (klass, sizeof (GDBusMessagePrivate));

  gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize     = g_dbus_message_finalize;
}

static void
g_dbus_message_init (GDBusMessage *message)
{
  message->priv = G_TYPE_INSTANCE_GET_PRIVATE (message, G_TYPE_DBUS_MESSAGE, GDBusMessagePrivate);

  message->priv->headers = g_hash_table_new_full (g_direct_hash,
                                                  g_direct_equal,
                                                  NULL,
                                                  (GDestroyNotify) g_variant_unref);
}

/**
 * g_dbus_message_new:
 *
 * Creates a new empty #GDBusMessage.
 *
 * Returns: A #GDBusMessage. Free with g_object_unref().
 *
 * Since: 2.26
 */
GDBusMessage *
g_dbus_message_new (void)
{
  return g_object_new (G_TYPE_DBUS_MESSAGE, NULL);
}

/**
 * g_dbus_message_new_method_call:
 * @name: A valid D-Bus name or %NULL.
 * @path: A valid object path.
 * @interface: A valid D-Bus interface name or %NULL.
 * @method: A valid method name.
 *
 * Creates a new #GDBusMessage for a method call.
 *
 * Returns: A #GDBusMessage. Free with g_object_unref().
 *
 * Since: 2.26
 */
GDBusMessage *
g_dbus_message_new_method_call (const gchar *name,
                                const gchar *path,
                                const gchar *interface,
                                const gchar *method)
{
  GDBusMessage *message;

  g_return_val_if_fail (name == NULL || g_dbus_is_name (name), NULL);
  g_return_val_if_fail (g_variant_is_object_path (path), NULL);
  g_return_val_if_fail (g_dbus_is_member_name (method), NULL);
  g_return_val_if_fail (interface == NULL || g_dbus_is_interface_name (interface), NULL);

  message = g_dbus_message_new ();
  message->priv->type = G_DBUS_MESSAGE_TYPE_METHOD_CALL;

  if (name != NULL)
    g_dbus_message_set_destination (message, name);
  g_dbus_message_set_path (message, path);
  g_dbus_message_set_member (message, method);
  if (interface != NULL)
    g_dbus_message_set_interface (message, interface);

  return message;
}

/**
 * g_dbus_message_new_signal:
 * @path: A valid object path.
 * @interface: A valid D-Bus interface name or %NULL.
 * @signal: A valid signal name.
 *
 * Creates a new #GDBusMessage for a signal emission.
 *
 * Returns: A #GDBusMessage. Free with g_object_unref().
 *
 * Since: 2.26
 */
GDBusMessage *
g_dbus_message_new_signal (const gchar  *path,
                           const gchar  *interface,
                           const gchar  *signal)
{
  GDBusMessage *message;

  g_return_val_if_fail (g_variant_is_object_path (path), NULL);
  g_return_val_if_fail (g_dbus_is_member_name (signal), NULL);
  g_return_val_if_fail (interface == NULL || g_dbus_is_interface_name (interface), NULL);

  message = g_dbus_message_new ();
  message->priv->type = G_DBUS_MESSAGE_TYPE_SIGNAL;
  message->priv->flags = G_DBUS_MESSAGE_FLAGS_NO_REPLY_EXPECTED;

  g_dbus_message_set_path (message, path);
  g_dbus_message_set_member (message, signal);

  if (interface != NULL)
    g_dbus_message_set_interface (message, interface);

  return message;
}


/**
 * g_dbus_message_new_method_reply:
 * @method_call_message: A message of type %G_DBUS_MESSAGE_TYPE_METHOD_CALL to
 * create a reply message to.
 *
 * Creates a new #GDBusMessage that is a reply to @method_call_message.
 *
 * Returns: A #GDBusMessage. Free with g_object_unref().
 *
 * Since: 2.26
 */
GDBusMessage *
g_dbus_message_new_method_reply (GDBusMessage *method_call_message)
{
  GDBusMessage *message;
  const gchar *sender;

  g_return_val_if_fail (G_IS_DBUS_MESSAGE (method_call_message), NULL);
  g_return_val_if_fail (g_dbus_message_get_type (method_call_message) == G_DBUS_MESSAGE_TYPE_METHOD_CALL, NULL);
  g_return_val_if_fail (g_dbus_message_get_serial (method_call_message) != 0, NULL);

  message = g_dbus_message_new ();
  message->priv->type = G_DBUS_MESSAGE_TYPE_METHOD_RETURN;
  message->priv->flags = G_DBUS_MESSAGE_FLAGS_NO_REPLY_EXPECTED;

  g_dbus_message_set_reply_serial (message, g_dbus_message_get_serial (method_call_message));
  sender = g_dbus_message_get_sender (method_call_message);
  if (sender != NULL)
    g_dbus_message_set_destination (message, sender);

  return message;
}

/**
 * g_dbus_message_new_method_error:
 * @method_call_message: A message of type %G_DBUS_MESSAGE_TYPE_METHOD_CALL to
 * create a reply message to.
 * @error_name: A valid D-Bus error name.
 * @error_message_format: The D-Bus error message in a printf() format.
 * @...: Arguments for @error_message_format.
 *
 * Creates a new #GDBusMessage that is an error reply to @method_call_message.
 *
 * Returns: A #GDBusMessage. Free with g_object_unref().
 *
 * Since: 2.26
 */
GDBusMessage *
g_dbus_message_new_method_error (GDBusMessage             *method_call_message,
                                 const gchar              *error_name,
                                 const gchar              *error_message_format,
                                 ...)
{
  GDBusMessage *ret;
  va_list var_args;

  va_start (var_args, error_message_format);
  ret = g_dbus_message_new_method_error_valist (method_call_message,
                                                error_name,
                                                error_message_format,
                                                var_args);
  va_end (var_args);

  return ret;
}

/**
 * g_dbus_message_new_method_error_literal:
 * @method_call_message: A message of type %G_DBUS_MESSAGE_TYPE_METHOD_CALL to
 * create a reply message to.
 * @error_name: A valid D-Bus error name.
 * @error_message: The D-Bus error message.
 *
 * Creates a new #GDBusMessage that is an error reply to @method_call_message.
 *
 * Returns: A #GDBusMessage. Free with g_object_unref().
 *
 * Since: 2.26
 */
GDBusMessage *
g_dbus_message_new_method_error_literal (GDBusMessage  *method_call_message,
                                         const gchar   *error_name,
                                         const gchar   *error_message)
{
  GDBusMessage *message;
  const gchar *sender;

  g_return_val_if_fail (G_IS_DBUS_MESSAGE (method_call_message), NULL);
  g_return_val_if_fail (g_dbus_message_get_type (method_call_message) == G_DBUS_MESSAGE_TYPE_METHOD_CALL, NULL);
  g_return_val_if_fail (g_dbus_message_get_serial (method_call_message) != 0, NULL);
  g_return_val_if_fail (g_dbus_is_name (error_name), NULL);
  g_return_val_if_fail (error_message != NULL, NULL);

  message = g_dbus_message_new ();
  message->priv->type = G_DBUS_MESSAGE_TYPE_ERROR;
  message->priv->flags = G_DBUS_MESSAGE_FLAGS_NO_REPLY_EXPECTED;

  g_dbus_message_set_reply_serial (message, g_dbus_message_get_serial (method_call_message));
  g_dbus_message_set_error_name (message, error_name);
  g_dbus_message_set_body (message, g_variant_new ("(s)", error_message));

  sender = g_dbus_message_get_sender (method_call_message);
  if (sender != NULL)
    g_dbus_message_set_destination (message, sender);

  return message;
}

/**
 * g_dbus_message_new_method_error_valist:
 * @method_call_message: A message of type %G_DBUS_MESSAGE_TYPE_METHOD_CALL to
 * create a reply message to.
 * @error_name: A valid D-Bus error name.
 * @error_message_format: The D-Bus error message in a printf() format.
 * @var_args: Arguments for @error_message_format.
 *
 * Like g_dbus_message_new_method_error() but intended for language bindings.
 *
 * Returns: A #GDBusMessage. Free with g_object_unref().
 *
 * Since: 2.26
 */
GDBusMessage *
g_dbus_message_new_method_error_valist (GDBusMessage             *method_call_message,
                                        const gchar              *error_name,
                                        const gchar              *error_message_format,
                                        va_list                   var_args)
{
  GDBusMessage *ret;
  gchar *error_message;
  error_message = g_strdup_vprintf (error_message_format, var_args);
  ret = g_dbus_message_new_method_error_literal (method_call_message,
                                                 error_name,
                                                 error_message);
  g_free (error_message);
  return ret;
}

/* ---------------------------------------------------------------------------------------------------- */

/* TODO: need GI annotations to specify that any guchar value goes for the type */

/**
 * g_dbus_message_get_type:
 * @message: A #GDBusMessage.
 *
 * Gets the type of @message.
 *
 * Returns: A 8-bit unsigned integer (typically a value from the #GDBusMessageType enumeration).
 *
 * Since: 2.26
 */
GDBusMessageType
g_dbus_message_get_type (GDBusMessage  *message)
{
  g_return_val_if_fail (G_IS_DBUS_MESSAGE (message), G_DBUS_MESSAGE_TYPE_INVALID);
  return message->priv->type;
}

/**
 * g_dbus_message_set_type:
 * @message: A #GDBusMessage.
 * @type: A 8-bit unsigned integer (typically a value from the #GDBusMessageType enumeration).
 *
 * Sets @message to be of @type.
 *
 * Since: 2.26
 */
void
g_dbus_message_set_type (GDBusMessage      *message,
                         GDBusMessageType   type)
{
  g_return_if_fail (G_IS_DBUS_MESSAGE (message));
  g_return_if_fail (type >=0 && type < 256);
  message->priv->type = type;
}

/* ---------------------------------------------------------------------------------------------------- */

/* TODO: need GI annotations to specify that any guchar value goes for flags */

/**
 * g_dbus_message_get_flags:
 * @message: A #GDBusMessage.
 *
 * Gets the flags for @message.
 *
 * Returns: Flags that are set (typically values from the #GDBusMessageFlags enumeration bitwise ORed together).
 *
 * Since: 2.26
 */
GDBusMessageFlags
g_dbus_message_get_flags (GDBusMessage  *message)
{
  g_return_val_if_fail (G_IS_DBUS_MESSAGE (message), G_DBUS_MESSAGE_FLAGS_NONE);
  return message->priv->flags;
}

/**
 * g_dbus_message_set_flags:
 * @message: A #GDBusMessage.
 * @flags: Flags for @message that are set (typically values from the #GDBusMessageFlags
 * enumeration bitwise ORed together).
 *
 * Sets the flags to set on @message.
 *
 * Since: 2.26
 */
void
g_dbus_message_set_flags (GDBusMessage       *message,
                          GDBusMessageFlags   flags)
{
  g_return_if_fail (G_IS_DBUS_MESSAGE (message));
  g_return_if_fail (flags >=0 && flags < 256);
  message->priv->flags = flags;
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_dbus_message_get_serial:
 * @message: A #GDBusMessage.
 *
 * Gets the serial for @message.
 *
 * Returns: A #guint32.
 *
 * Since: 2.26
 */
guint32
g_dbus_message_get_serial (GDBusMessage *message)
{
  g_return_val_if_fail (G_IS_DBUS_MESSAGE (message), 0);
  return message->priv->serial;
}

/**
 * g_dbus_message_set_serial:
 * @message: A #GDBusMessage.
 * @serial: A #guint32.
 *
 * Sets the serial for @message.
 *
 * Since: 2.26
 */
void
g_dbus_message_set_serial (GDBusMessage  *message,
                           guint32        serial)
{
  g_return_if_fail (G_IS_DBUS_MESSAGE (message));
  message->priv->serial = serial;
}

/* ---------------------------------------------------------------------------------------------------- */

/* TODO: need GI annotations to specify that any guchar value goes for header_field */

/**
 * g_dbus_message_get_header:
 * @message: A #GDBusMessage.
 * @header_field: A 8-bit unsigned integer (typically a value from the #GDBusMessageHeaderField enumeration)
 *
 * Gets a header field on @message.
 *
 * Returns: A #GVariant with the value if the header was found, %NULL
 * otherwise. Do not free, it is owned by @message.
 *
 * Since: 2.26
 */
GVariant *
g_dbus_message_get_header (GDBusMessage             *message,
                           GDBusMessageHeaderField   header_field)
{
  g_return_val_if_fail (G_IS_DBUS_MESSAGE (message), NULL);
  g_return_val_if_fail (header_field >=0 && header_field < 256, NULL);
  return g_hash_table_lookup (message->priv->headers, GUINT_TO_POINTER (header_field));
}

/**
 * g_dbus_message_set_header:
 * @message: A #GDBusMessage.
 * @header_field: A 8-bit unsigned integer (typically a value from the #GDBusMessageHeaderField enumeration)
 * @value: A #GVariant to set the header field or %NULL to clear the header field.
 *
 * Sets a header field on @message.
 *
 * If @value is floating, @message assumes ownership of @value.
 *
 * Since: 2.26
 */
void
g_dbus_message_set_header (GDBusMessage             *message,
                           GDBusMessageHeaderField   header_field,
                           GVariant                 *value)
{
  g_return_if_fail (G_IS_DBUS_MESSAGE (message));
  g_return_if_fail (header_field >=0 && header_field < 256);
  if (value == NULL)
    {
      g_hash_table_remove (message->priv->headers, GUINT_TO_POINTER (header_field));
    }
  else
    {
      g_hash_table_insert (message->priv->headers, GUINT_TO_POINTER (header_field), g_variant_ref_sink (value));
    }
}

/**
 * g_dbus_message_get_header_fields:
 * @message: A #GDBusMessage.
 *
 * Gets an array of all header fields on @message that are set.
 *
 * Returns: An array of header fields terminated by
 * %G_DBUS_MESSAGE_HEADER_FIELD_INVALID.  Each element is a
 * #guchar. Free with g_free().
 *
 * Since: 2.26
 */
guchar *
g_dbus_message_get_header_fields (GDBusMessage  *message)
{
  GList *keys;
  guchar *ret;
  guint num_keys;
  GList *l;
  guint n;

  g_return_val_if_fail (G_IS_DBUS_MESSAGE (message), NULL);

  keys = g_hash_table_get_keys (message->priv->headers);
  num_keys = g_list_length (keys);
  ret = g_new (guchar, num_keys + 1);
  for (l = keys, n = 0; l != NULL; l = l->next, n++)
    ret[n] = GPOINTER_TO_UINT (l->data);
  g_assert (n == num_keys);
  ret[n] = G_DBUS_MESSAGE_HEADER_FIELD_INVALID;
  g_list_free (keys);

  return ret;
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_dbus_message_get_body:
 * @message: A #GDBusMessage.
 *
 * Gets the body of a message.
 *
 * Returns: A #GVariant or %NULL if the body is empty. Do not free, it is owned by @message.
 *
 * Since: 2.26
 */
GVariant *
g_dbus_message_get_body (GDBusMessage  *message)
{
  g_return_val_if_fail (G_IS_DBUS_MESSAGE (message), NULL);
  return message->priv->body;
}

/**
 * g_dbus_message_set_body:
 * @message: A #GDBusMessage.
 * @body: Either %NULL or a #GVariant that is a tuple.
 *
 * Sets the body @message. As a side-effect the
 * %G_DBUS_MESSAGE_HEADER_FIELD_SIGNATURE header field is set to the
 * type string of @body (or cleared if @body is %NULL).
 *
 * If @body is floating, @message assumes ownership of @body.
 *
 * Since: 2.26
 */
void
g_dbus_message_set_body (GDBusMessage  *message,
                         GVariant      *body)
{
  g_return_if_fail (G_IS_DBUS_MESSAGE (message));
  g_return_if_fail ((body == NULL) || g_variant_is_of_type (body, G_VARIANT_TYPE_TUPLE));

  if (message->priv->body != NULL)
    g_variant_unref (message->priv->body);
  if (body == NULL)
    {
      message->priv->body = NULL;
      g_dbus_message_set_signature (message, NULL);
    }
  else
    {
      const gchar *type_string;
      gsize type_string_len;
      gchar *signature;

      message->priv->body = g_variant_ref_sink (body);

      type_string = g_variant_get_type_string (body);
      type_string_len = strlen (type_string);
      g_assert (type_string_len >= 2);
      signature = g_strndup (type_string + 1, type_string_len - 2);
      g_dbus_message_set_signature (message, signature);
      g_free (signature);
    }
}

/* ---------------------------------------------------------------------------------------------------- */

#ifdef G_OS_UNIX
/**
 * g_dbus_message_get_unix_fd_list:
 * @message: A #GDBusMessage.
 *
 * Gets the UNIX file descriptors associated with @message, if any.
 *
 * This method is only available on UNIX.
 *
 * Returns: A #GUnixFDList or %NULL if no file descriptors are
 * associated. Do not free, this object is owned by @message.
 *
 * Since: 2.26
 */
GUnixFDList *
g_dbus_message_get_unix_fd_list (GDBusMessage  *message)
{
  g_return_val_if_fail (G_IS_DBUS_MESSAGE (message), NULL);
  return message->priv->fd_list;
}

/**
 * g_dbus_message_set_unix_fd_list:
 * @message: A #GDBusMessage.
 * @fd_list: A #GUnixFDList or %NULL.
 *
 * Sets the UNIX file descriptors associated with @message. As a
 * side-effect the %G_DBUS_MESSAGE_HEADER_FIELD_NUM_UNIX_FDS header
 * field is set to the number of fds in @fd_list (or cleared if
 * @fd_list is %NULL).
 *
 * This method is only available on UNIX.
 *
 * Since: 2.26
 */
void
g_dbus_message_set_unix_fd_list (GDBusMessage  *message,
                                 GUnixFDList   *fd_list)
{
  g_return_if_fail (G_IS_DBUS_MESSAGE (message));
  g_return_if_fail (fd_list == NULL || G_IS_UNIX_FD_LIST (fd_list));
  if (message->priv->fd_list != NULL)
    g_object_unref (message->priv->fd_list);
  if (fd_list != NULL)
    {
      message->priv->fd_list = g_object_ref (fd_list);
      g_dbus_message_set_num_unix_fds (message, g_unix_fd_list_get_length (fd_list));
    }
  else
    {
      message->priv->fd_list = NULL;
      g_dbus_message_set_num_unix_fds (message, 0);
    }
}
#endif

/* ---------------------------------------------------------------------------------------------------- */

static gboolean
ensure_input_padding (GMemoryInputStream   *mis,
                      gsize                 padding_size,
                      GError              **error)
{
  gsize offset;
  gsize wanted_offset;

  offset = g_seekable_tell (G_SEEKABLE (mis));
  wanted_offset = ((offset + padding_size - 1) / padding_size) * padding_size;

  return g_seekable_seek (G_SEEKABLE (mis), wanted_offset, G_SEEK_SET, NULL, error);
}

static gchar *
read_string (GMemoryInputStream    *mis,
             GDataInputStream      *dis,
             gsize                  len,
             GError               **error)
{
  GString *s;
  gchar buf[256];
  gsize remaining;
  guchar nul;
  GError *local_error;

  s = g_string_new (NULL);

  remaining = len;
  while (remaining > 0)
    {
      gsize to_read;
      gssize num_read;

      to_read = MIN (remaining, sizeof (buf));
      num_read = g_input_stream_read (G_INPUT_STREAM (mis),
                                      buf,
                                      to_read,
                                      NULL,
                                      error);
      if (num_read < 0)
        goto fail;
      if (num_read == 0)
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_INVALID_ARGUMENT,
                       _("Wanted to read %" G_GSIZE_FORMAT " bytes but got EOF"),
                       to_read);
          goto fail;
        }

      remaining -= num_read;
      g_string_append_len (s, buf, num_read);
    }

  local_error = NULL;
  nul = g_data_input_stream_read_byte (dis, NULL, &local_error);
  if (local_error != NULL)
    {
      g_propagate_error (error, local_error);
      goto fail;
    }
  if (nul != '\0')
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_ARGUMENT,
                   _("Expected NUL byte after the string `%s' but found `%c' (%d)"),
                   s->str, nul, nul);
      goto fail;
    }

  return g_string_free (s, FALSE);

 fail:
  g_string_free (s, TRUE);
  return NULL;
}

static GVariant *
parse_value_from_blob (GMemoryInputStream    *mis,
                       GDataInputStream      *dis,
                       const GVariantType    *type,
                       GError               **error)
{
  GVariant *ret;
  GError *local_error;

  local_error = NULL;
  if (g_variant_type_equal (type, G_VARIANT_TYPE_BOOLEAN))
    {
      gboolean v;
      if (!ensure_input_padding (mis, 4, &local_error))
        goto fail;
      v = g_data_input_stream_read_uint32 (dis, NULL, &local_error);
      if (local_error != NULL)
        goto fail;
      ret = g_variant_new_boolean (v);
    }
  else if (g_variant_type_equal (type, G_VARIANT_TYPE_BYTE))
    {
      guchar v;
      v = g_data_input_stream_read_byte (dis, NULL, &local_error);
      if (local_error != NULL)
        goto fail;
      ret = g_variant_new_byte (v);
    }
  else if (g_variant_type_equal (type, G_VARIANT_TYPE_INT16))
    {
      gint16 v;
      if (!ensure_input_padding (mis, 2, &local_error))
        goto fail;
      v = g_data_input_stream_read_int16 (dis, NULL, &local_error);
      if (local_error != NULL)
        goto fail;
      ret = g_variant_new_int16 (v);
    }
  else if (g_variant_type_equal (type, G_VARIANT_TYPE_UINT16))
    {
      guint16 v;
      if (!ensure_input_padding (mis, 2, &local_error))
        goto fail;
      v = g_data_input_stream_read_uint16 (dis, NULL, &local_error);
      if (local_error != NULL)
        goto fail;
      ret = g_variant_new_uint16 (v);
    }
  else if (g_variant_type_equal (type, G_VARIANT_TYPE_INT32))
    {
      gint32 v;
      if (!ensure_input_padding (mis, 4, &local_error))
        goto fail;
      v = g_data_input_stream_read_int32 (dis, NULL, &local_error);
      if (local_error != NULL)
        goto fail;
      ret = g_variant_new_int32 (v);
    }
  else if (g_variant_type_equal (type, G_VARIANT_TYPE_UINT32))
    {
      guint32 v;
      if (!ensure_input_padding (mis, 4, &local_error))
        goto fail;
      v = g_data_input_stream_read_uint32 (dis, NULL, &local_error);
      if (local_error != NULL)
        goto fail;
      ret = g_variant_new_uint32 (v);
    }
  else if (g_variant_type_equal (type, G_VARIANT_TYPE_INT64))
    {
      gint64 v;
      if (!ensure_input_padding (mis, 8, &local_error))
        goto fail;
      v = g_data_input_stream_read_int64 (dis, NULL, &local_error);
      if (local_error != NULL)
        goto fail;
      ret = g_variant_new_int64 (v);
    }
  else if (g_variant_type_equal (type, G_VARIANT_TYPE_UINT64))
    {
      guint64 v;
      if (!ensure_input_padding (mis, 8, &local_error))
        goto fail;
      v = g_data_input_stream_read_uint64 (dis, NULL, &local_error);
      if (local_error != NULL)
        goto fail;
      ret = g_variant_new_uint64 (v);
    }
  else if (g_variant_type_equal (type, G_VARIANT_TYPE_DOUBLE))
    {
      guint64 v;
      gdouble *encoded;
      if (!ensure_input_padding (mis, 8, &local_error))
        goto fail;
      v = g_data_input_stream_read_uint64 (dis, NULL, &local_error);
      if (local_error != NULL)
        goto fail;
      /* TODO: hmm */
      encoded = (gdouble *) &v;
      ret = g_variant_new_double (*encoded);
    }
  else if (g_variant_type_equal (type, G_VARIANT_TYPE_STRING))
    {
      guint32 len;
      gchar *v;
      if (!ensure_input_padding (mis, 4, &local_error))
        goto fail;
      len = g_data_input_stream_read_uint32 (dis, NULL, &local_error);
      if (local_error != NULL)
        goto fail;
      v = read_string (mis, dis, (gsize) len, &local_error);
      if (v == NULL)
        goto fail;
      ret = g_variant_new_string (v);
    }
  else if (g_variant_type_equal (type, G_VARIANT_TYPE_OBJECT_PATH))
    {
      guint32 len;
      gchar *v;
      if (!ensure_input_padding (mis, 4, &local_error))
        goto fail;
      len = g_data_input_stream_read_uint32 (dis, NULL, &local_error);
      if (local_error != NULL)
        goto fail;
      v = read_string (mis, dis, (gsize) len, &local_error);
      if (v == NULL)
        goto fail;
      if (!g_variant_is_object_path (v))
        {
          g_set_error (&local_error,
                       G_IO_ERROR,
                       G_IO_ERROR_INVALID_ARGUMENT,
                       _("Parsed value `%s' is not a valid D-Bus object path"),
                       v);
          goto fail;
        }
      ret = g_variant_new_object_path (v);
    }
  else if (g_variant_type_equal (type, G_VARIANT_TYPE_SIGNATURE))
    {
      guchar len;
      gchar *v;
      len = g_data_input_stream_read_byte (dis, NULL, &local_error);
      if (local_error != NULL)
        goto fail;
      v = read_string (mis, dis, (gsize) len, &local_error);
      if (v == NULL)
        goto fail;
      if (!g_variant_is_signature (v))
        {
          g_set_error (&local_error,
                       G_IO_ERROR,
                       G_IO_ERROR_INVALID_ARGUMENT,
                       _("Parsed value `%s' is not a valid D-Bus signature"),
                       v);
          goto fail;
        }
      ret = g_variant_new_signature (v);
    }
  else if (g_variant_type_is_array (type))
    {
      guint32 array_len;
      goffset offset;
      goffset target;
      const GVariantType *element_type;
      GVariantBuilder *builder;

      if (!ensure_input_padding (mis, 4, &local_error))
        goto fail;
      array_len = g_data_input_stream_read_uint32 (dis, NULL, &local_error);

      if (array_len > (2<<26))
        {
          g_set_error (&local_error,
                       G_IO_ERROR,
                       G_IO_ERROR_INVALID_ARGUMENT,
                       _("Encountered array of length %" G_GUINT32_FORMAT " bytes. Maximum length is 2<<26 bytes."),
                       array_len);
          goto fail;
        }

      builder = g_variant_builder_new (type);
      element_type = g_variant_type_element (type);

      /* TODO: optimize array of primitive types */

      offset = g_seekable_tell (G_SEEKABLE (mis));
      target = offset + array_len;
      while (offset < target)
        {
          GVariant *item;
          item = parse_value_from_blob (mis, dis, element_type, &local_error);
          if (item == NULL)
            {
              g_variant_builder_unref (builder);
              goto fail;
            }
          g_variant_builder_add_value (builder, item);
          offset = g_seekable_tell (G_SEEKABLE (mis));
        }

      ret = g_variant_builder_end (builder);
    }
  else if (g_variant_type_is_dict_entry (type))
    {
      const GVariantType *key_type;
      const GVariantType *value_type;
      GVariant *key;
      GVariant *value;

      if (!ensure_input_padding (mis, 8, &local_error))
        goto fail;

      key_type = g_variant_type_key (type);
      key = parse_value_from_blob (mis, dis, key_type, &local_error);
      if (key == NULL)
        goto fail;

      value_type = g_variant_type_value (type);
      value = parse_value_from_blob (mis, dis, value_type, &local_error);
      if (value == NULL)
        {
          g_variant_unref (key);
          goto fail;
        }
      ret = g_variant_new_dict_entry (key, value);
    }
  else if (g_variant_type_is_tuple (type))
    {
      const GVariantType *element_type;
      GVariantBuilder *builder;

      if (!ensure_input_padding (mis, 8, &local_error))
        goto fail;

      builder = g_variant_builder_new (type);
      element_type = g_variant_type_first (type);
      while (element_type != NULL)
        {
          GVariant *item;
          item = parse_value_from_blob (mis, dis, element_type, &local_error);
          if (item == NULL)
            {
              g_variant_builder_unref (builder);
              goto fail;
            }
          g_variant_builder_add_value (builder, item);

          element_type = g_variant_type_next (element_type);
        }
      ret = g_variant_builder_end (builder);
    }
  else if (g_variant_type_is_variant (type))
    {
      guchar siglen;
      gchar *sig;
      GVariantType *variant_type;
      GVariant *value;

      siglen = g_data_input_stream_read_byte (dis, NULL, &local_error);
      if (local_error != NULL)
        goto fail;
      sig = read_string (mis, dis, (gsize) siglen, &local_error);
      if (sig == NULL)
        goto fail;
      if (!g_variant_is_signature (sig))
        {
          g_set_error (&local_error,
                       G_IO_ERROR,
                       G_IO_ERROR_INVALID_ARGUMENT,
                       _("Parsed value `%s' for variant is not a valid D-Bus signature"),
                       sig);
          goto fail;
        }
      variant_type = g_variant_type_new (sig);
      value = parse_value_from_blob (mis, dis, variant_type, &local_error);
      g_variant_type_free (variant_type);
      if (value == NULL)
        goto fail;
      ret = g_variant_new_variant (value);
    }
  else
    {
      gchar *s;
      s = g_variant_type_dup_string (type);
      g_set_error (&local_error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_ARGUMENT,
                   _("Error deserializing GVariant with type-string `%s' from the D-Bus wire format"),
                   s);
      g_free (s);
      goto fail;
    }

  g_assert (ret != NULL);
  return ret;

 fail:
  g_propagate_error (error, local_error);
  return NULL;
}

/* ---------------------------------------------------------------------------------------------------- */

/* message_header must be at least 16 bytes */

/**
 * g_dbus_message_bytes_needed:
 * @blob: A blob represent a binary D-Bus message.
 * @blob_len: The length of @blob (must be at least 16).
 * @error: Return location for error or %NULL.
 *
 * Utility function to calculate how many bytes are needed to
 * completely deserialize the D-Bus message stored at @blob.
 *
 * Returns: Number of bytes needed or -1 if @error is set (e.g. if
 * @blob contains invalid data or not enough data is available to
 * determine the size).
 *
 * Since: 2.26
 */
gssize
g_dbus_message_bytes_needed (guchar  *blob,
                             gsize    blob_len,
                             GError **error)
{
  gssize ret;

  ret = -1;

  g_return_val_if_fail (blob != NULL, -1);
  g_return_val_if_fail (error == NULL || *error == NULL, -1);
  g_return_val_if_fail (blob_len >= 16, -1);

  if (blob[0] == 'l')
    {
      /* core header (12 bytes) + ARRAY of STRUCT of (BYTE,VARIANT) */
      ret = 12 + 4 + GUINT32_FROM_LE (((guint32 *) blob)[3]);
      /* round up so it's a multiple of 8 */
      ret = 8 * ((ret + 7)/8);
      /* finally add the body size */
      ret += GUINT32_FROM_LE (((guint32 *) blob)[1]);
    }
  else if (blob[0] == 'B')
    {
      /* TODO */
      g_assert_not_reached ();
    }
  else
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_ARGUMENT,
                   "Unable to determine message blob length - given blob is malformed");
    }

  if (ret > (2<<27))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_ARGUMENT,
                   "Blob indicates that message exceeds maximum message length (128MiB)");
      ret = -1;
    }

  return ret;
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_dbus_message_new_from_blob:
 * @blob: A blob represent a binary D-Bus message.
 * @blob_len: The length of @blob.
 * @error: Return location for error or %NULL.
 *
 * Creates a new #GDBusMessage from the data stored at @blob.
 *
 * Returns: A new #GDBusMessage or %NULL if @error is set. Free with
 * g_object_unref().
 *
 * Since: 2.26
 */
GDBusMessage *
g_dbus_message_new_from_blob (guchar    *blob,
                              gsize      blob_len,
                              GError   **error)
{
  gboolean ret;
  GMemoryInputStream *mis;
  GDataInputStream *dis;
  GDBusMessage *message;
  guchar endianness;
  guchar major_protocol_version;
  GDataStreamByteOrder byte_order;
  guint32 message_body_len;
  GVariant *headers;
  GVariant *item;
  GVariantIter iter;
  GVariant *signature;

  ret = FALSE;

  g_return_val_if_fail (blob != NULL, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);
  g_return_val_if_fail (blob_len >= 12, NULL);

  message = g_dbus_message_new ();

  mis = G_MEMORY_INPUT_STREAM (g_memory_input_stream_new_from_data (blob, blob_len, NULL));
  dis = g_data_input_stream_new (G_INPUT_STREAM (mis));

  endianness = g_data_input_stream_read_byte (dis, NULL, NULL);
  switch (endianness)
    {
    case 'l':
      byte_order = G_DATA_STREAM_BYTE_ORDER_LITTLE_ENDIAN;
      break;
    case 'B':
      byte_order = G_DATA_STREAM_BYTE_ORDER_BIG_ENDIAN;
      break;
    default:
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_ARGUMENT,
                   _("Invalid endianness value. Expected 'l' or 'B' but found '%c' (%d)"),
                   endianness, endianness);
      goto out;
    }
  g_data_input_stream_set_byte_order (dis, byte_order);

  message->priv->type = g_data_input_stream_read_byte (dis, NULL, NULL);
  message->priv->flags = g_data_input_stream_read_byte (dis, NULL, NULL);
  major_protocol_version = g_data_input_stream_read_byte (dis, NULL, NULL);
  if (major_protocol_version != 1)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_ARGUMENT,
                   _("Invalid major protocol version. Expected 1 but found %d"),
                   major_protocol_version);
      goto out;
    }
  message_body_len = g_data_input_stream_read_uint32 (dis, NULL, NULL);
  message->priv->serial = g_data_input_stream_read_uint32 (dis, NULL, NULL);

  headers = parse_value_from_blob (mis,
                                   dis,
                                   G_VARIANT_TYPE ("a{yv}"),
                                   error);
  if (headers == NULL)
    goto out;
  g_variant_ref_sink (headers);
  g_variant_iter_init (&iter, headers);
  while ((item = g_variant_iter_next_value (&iter)))
    {
      guchar header_field;
      GVariant *value;
      g_variant_get (item,
                     "{yv}",
                     &header_field,
                     &value);
      g_dbus_message_set_header (message, header_field, value);
    }
  g_variant_unref (headers);

  signature = g_dbus_message_get_header (message, G_DBUS_MESSAGE_HEADER_FIELD_SIGNATURE);
  if (signature != NULL)
    {
      const gchar *signature_str;
      gsize signature_str_len;

      signature_str = g_variant_get_string (signature, NULL);
      signature_str_len = strlen (signature_str);

      /* signature but no body */
      if (message_body_len == 0 && signature_str_len > 0)
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_INVALID_ARGUMENT,
                       _("Signature header with signature `%s' found but message body is empty"),
                       signature_str);
          goto out;
        }
      else if (signature_str_len > 0)
        {
          GVariantType *variant_type;
          gchar *tupled_signature_str;

          if (!g_variant_is_signature (signature_str))
            {
              g_set_error (error,
                           G_IO_ERROR,
                           G_IO_ERROR_INVALID_ARGUMENT,
                           _("Parsed value `%s' is not a valid D-Bus signature (for body)"),
                           signature_str);
              goto out;
            }
          tupled_signature_str = g_strdup_printf ("(%s)", signature_str);
          variant_type = g_variant_type_new (tupled_signature_str);
          g_free (tupled_signature_str);
          message->priv->body = parse_value_from_blob (mis,
                                                       dis,
                                                       variant_type,
                                                       error);
          if (message->priv->body == NULL)
            {
              g_variant_type_free (variant_type);
              goto out;
            }
          g_variant_ref_sink (message->priv->body);
          g_variant_type_free (variant_type);
        }
    }
  else
    {
      /* no signature, this is only OK if the body is empty */
      if (message_body_len != 0)
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_INVALID_ARGUMENT,
                       _("No signature header in message but the message body is %" G_GUINT32_FORMAT " bytes"),
                       message_body_len);
          goto out;
        }
    }


  ret = TRUE;

 out:
  g_object_unref (dis);
  g_object_unref (mis);

  if (ret)
    {
      return message;
    }
  else
    {
      if (message != NULL)
        g_object_unref (message);
      return NULL;
    }
}

/* ---------------------------------------------------------------------------------------------------- */

static gsize
ensure_output_padding (GMemoryOutputStream  *mos,
                       GDataOutputStream    *dos,
                       gsize                 padding_size)
{
  gsize offset;
  gsize wanted_offset;
  gsize padding_needed;
  guint n;

  offset = g_memory_output_stream_get_data_size (mos);
  wanted_offset = ((offset + padding_size - 1) / padding_size) * padding_size;
  padding_needed = wanted_offset - offset;

  for (n = 0; n < padding_needed; n++)
    g_data_output_stream_put_byte (dos, '\0', NULL, NULL);

  return padding_needed;
}

static gboolean
append_value_to_blob (GVariant             *value,
                      GMemoryOutputStream  *mos,
                      GDataOutputStream    *dos,
                      gsize                *out_padding_added,
                      GError              **error)
{
  const GVariantType *type;
  gsize padding_added;

  padding_added = 0;

  type = g_variant_get_type (value);
  if (g_variant_type_equal (type, G_VARIANT_TYPE_BOOLEAN))
    {
      gboolean v = g_variant_get_boolean (value);
      padding_added = ensure_output_padding (mos, dos, 4);
      g_data_output_stream_put_uint32 (dos, v, NULL, NULL);
    }
  else if (g_variant_type_equal (type, G_VARIANT_TYPE_BYTE))
    {
      guint8 v = g_variant_get_byte (value);
      g_data_output_stream_put_byte (dos, v, NULL, NULL);
    }
  else if (g_variant_type_equal (type, G_VARIANT_TYPE_INT16))
    {
      gint16 v = g_variant_get_int16 (value);
      padding_added = ensure_output_padding (mos, dos, 2);
      g_data_output_stream_put_int16 (dos, v, NULL, NULL);
    }
  else if (g_variant_type_equal (type, G_VARIANT_TYPE_UINT16))
    {
      guint16 v = g_variant_get_uint16 (value);
      padding_added = ensure_output_padding (mos, dos, 2);
      g_data_output_stream_put_uint16 (dos, v, NULL, NULL);
    }
  else if (g_variant_type_equal (type, G_VARIANT_TYPE_INT32))
    {
      gint32 v = g_variant_get_int32 (value);
      padding_added = ensure_output_padding (mos, dos, 4);
      g_data_output_stream_put_int32 (dos, v, NULL, NULL);
    }
  else if (g_variant_type_equal (type, G_VARIANT_TYPE_UINT32))
    {
      guint32 v = g_variant_get_uint32 (value);
      padding_added = ensure_output_padding (mos, dos, 4);
      g_data_output_stream_put_uint32 (dos, v, NULL, NULL);
    }
  else if (g_variant_type_equal (type, G_VARIANT_TYPE_INT64))
    {
      gint64 v = g_variant_get_int64 (value);
      padding_added = ensure_output_padding (mos, dos, 8);
      g_data_output_stream_put_int64 (dos, v, NULL, NULL);
    }
  else if (g_variant_type_equal (type, G_VARIANT_TYPE_UINT64))
    {
      guint64 v = g_variant_get_uint64 (value);
      padding_added = ensure_output_padding (mos, dos, 8);
      g_data_output_stream_put_uint64 (dos, v, NULL, NULL);
    }
  else if (g_variant_type_equal (type, G_VARIANT_TYPE_DOUBLE))
    {
      guint64 *encoded;
      gdouble v = g_variant_get_double (value);
      padding_added = ensure_output_padding (mos, dos, 8);
      /* TODO: hmm */
      encoded = (guint64 *) &v;
      g_data_output_stream_put_uint64 (dos, *encoded, NULL, NULL);
    }
  else if (g_variant_type_equal (type, G_VARIANT_TYPE_STRING))
    {
      const gchar *v = g_variant_get_string (value, NULL);
      gsize len;
      padding_added = ensure_output_padding (mos, dos, 4);
      len = strlen (v);
      g_data_output_stream_put_uint32 (dos, len, NULL, NULL);
      g_data_output_stream_put_string (dos, v, NULL, NULL);
      g_data_output_stream_put_byte (dos, '\0', NULL, NULL);
    }
  else if (g_variant_type_equal (type, G_VARIANT_TYPE_OBJECT_PATH))
    {
      /* TODO: validate object path */
      const gchar *v = g_variant_get_string (value, NULL);
      gsize len;
      padding_added = ensure_output_padding (mos, dos, 4);
      len = strlen (v);
      g_data_output_stream_put_uint32 (dos, len, NULL, NULL);
      g_data_output_stream_put_string (dos, v, NULL, NULL);
      g_data_output_stream_put_byte (dos, '\0', NULL, NULL);
    }
  else if (g_variant_type_equal (type, G_VARIANT_TYPE_SIGNATURE))
    {
      /* TODO: validate signature (including max len being 255) */
      const gchar *v = g_variant_get_string (value, NULL);
      gsize len;
      len = strlen (v);
      g_data_output_stream_put_byte (dos, len, NULL, NULL);
      g_data_output_stream_put_string (dos, v, NULL, NULL);
      g_data_output_stream_put_byte (dos, '\0', NULL, NULL);
    }
  else if (g_variant_type_is_array (type))
    {
      GVariant *item;
      GVariantIter iter;
      goffset array_len_offset;
      goffset array_payload_begin_offset;
      goffset cur_offset;
      gsize array_len;
      guint n;

      padding_added = ensure_output_padding (mos, dos, 4);

      /* array length - will be filled in later */
      array_len_offset = g_memory_output_stream_get_data_size (mos);
      g_data_output_stream_put_uint32 (dos, 0xF00DFACE, NULL, NULL);

      /* From the D-Bus spec:
       *
       *   "A UINT32 giving the length of the array data in bytes,
       *    followed by alignment padding to the alignment boundary of
       *    the array element type, followed by each array element. The
       *    array length is from the end of the alignment padding to
       *    the end of the last element, i.e. it does not include the
       *    padding after the length, or any padding after the last
       *    element."
       *
       * Thus, we need to count how much padding the first element
       * contributes and subtract that from the array length.
       */
      array_payload_begin_offset = g_memory_output_stream_get_data_size (mos);

      g_variant_iter_init (&iter, value);
      n = 0;
      while ((item = g_variant_iter_next_value (&iter)))
        {
          gsize padding_added_for_item;
          if (!append_value_to_blob (item, mos, dos, &padding_added_for_item, error))
            goto fail;
          if (n == 0)
            {
              array_payload_begin_offset += padding_added_for_item;
            }
          n++;
        }

      cur_offset = g_memory_output_stream_get_data_size (mos);

      array_len = cur_offset - array_payload_begin_offset;

      if (!g_seekable_seek (G_SEEKABLE (mos), array_len_offset, G_SEEK_SET, NULL, error))
        goto fail;

      g_data_output_stream_put_uint32 (dos, array_len, NULL, NULL);

      if (!g_seekable_seek (G_SEEKABLE (mos), cur_offset, G_SEEK_SET, NULL, error))
        goto fail;
    }
  else if (g_variant_type_is_dict_entry (type) || g_variant_type_is_tuple (type))
    {
      GVariant *item;
      GVariantIter iter;

      padding_added = ensure_output_padding (mos, dos, 8);

      g_variant_iter_init (&iter, value);

      while ((item = g_variant_iter_next_value (&iter)))
        {
          if (!append_value_to_blob (item, mos, dos, NULL, error))
            goto fail;
        }
    }
  else if (g_variant_type_is_variant (type))
    {
      GVariant *child;
      const gchar *signature;
      child = g_variant_get_child_value (value, 0);
      signature = g_variant_get_type_string (child);
      /* TODO: validate signature (including max len being 255) */
      g_data_output_stream_put_byte (dos, strlen (signature), NULL, NULL);
      g_data_output_stream_put_string (dos, signature, NULL, NULL);
      g_data_output_stream_put_byte (dos, '\0', NULL, NULL);
      if (!append_value_to_blob (child, mos, dos, NULL, error))
        {
          g_variant_unref (child);
          goto fail;
        }
      g_variant_unref (child);
    }
  else
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_ARGUMENT,
                   _("Error serializing GVariant with type-string `%s' to the D-Bus wire format"),
                   g_variant_get_type_string (value));
      goto fail;
    }

  if (out_padding_added != NULL)
    *out_padding_added = padding_added;

  return TRUE;

 fail:
  return FALSE;
}

static gboolean
append_body_to_blob (GVariant             *value,
                     GMemoryOutputStream  *mos,
                     GDataOutputStream    *dos,
                     GError              **error)
{
  gboolean ret;
  GVariant *item;
  GVariantIter iter;

  ret = FALSE;

  if (!g_variant_is_of_type (value, G_VARIANT_TYPE_TUPLE))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_ARGUMENT,
                   "Expected a tuple for the body of the GDBusMessage.");
      goto fail;
    }

  g_variant_iter_init (&iter, value);
  while ((item = g_variant_iter_next_value (&iter)))
    {
      if (!append_value_to_blob (item, mos, dos, NULL, error))
        goto fail;
    }
  return TRUE;

 fail:
  return FALSE;
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_dbus_message_to_blob:
 * @message: A #GDBusMessage.
 * @out_size: Return location for size of generated blob.
 * @error: Return location for error.
 *
 * Serializes @message to a blob.
 *
 * Returns: A pointer to a valid binary D-Bus message of @out_size bytes
 * generated by @message or %NULL if @error is set. Free with g_free().
 *
 * Since: 2.26
 */
guchar *
g_dbus_message_to_blob (GDBusMessage   *message,
                        gsize          *out_size,
                        GError        **error)
{
  GMemoryOutputStream *mos;
  GDataOutputStream *dos;
  guchar *ret;
  gsize size;
  GDataStreamByteOrder byte_order;
  goffset body_len_offset;
  goffset body_start_offset;
  gsize body_size;
  GVariant *header_fields;
  GVariantBuilder *builder;
  GHashTableIter hash_iter;
  gpointer key;
  GVariant *header_value;
  GVariant *signature;
  const gchar *signature_str;
  gint num_fds_in_message;
  gint num_fds_according_to_header;

  ret = NULL;

  g_return_val_if_fail (G_IS_DBUS_MESSAGE (message), NULL);
  g_return_val_if_fail (out_size != NULL, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  mos = G_MEMORY_OUTPUT_STREAM (g_memory_output_stream_new (NULL, 0, g_realloc, g_free));
  dos = g_data_output_stream_new (G_OUTPUT_STREAM (mos));

  /* TODO: detect endianess... */
  byte_order = G_DATA_STREAM_BYTE_ORDER_LITTLE_ENDIAN;
  g_data_output_stream_set_byte_order (dos, byte_order);

  /* Core header */
  g_data_output_stream_put_byte (dos, byte_order == G_DATA_STREAM_BYTE_ORDER_LITTLE_ENDIAN ? 'l' : 'B', NULL, NULL);
  g_data_output_stream_put_byte (dos, message->priv->type, NULL, NULL);
  g_data_output_stream_put_byte (dos, message->priv->flags, NULL, NULL);
  g_data_output_stream_put_byte (dos, 1, NULL, NULL); /* major protocol version */
  body_len_offset = g_memory_output_stream_get_data_size (mos);
  /* body length - will be filled in later */
  g_data_output_stream_put_uint32 (dos, 0xF00DFACE, NULL, NULL);
  g_data_output_stream_put_uint32 (dos, message->priv->serial, NULL, NULL);

  num_fds_in_message = 0;
#ifdef G_OS_UNIX
  if (message->priv->fd_list != NULL)
    num_fds_in_message = g_unix_fd_list_get_length (message->priv->fd_list);
#endif
  num_fds_according_to_header = g_dbus_message_get_num_unix_fds (message);
  /* TODO: check we have all the right header fields and that they are the correct value etc etc */
  if (num_fds_in_message != num_fds_according_to_header)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_ARGUMENT,
                   _("Message has %d fds but the header field indicates %d fds"),
                   num_fds_in_message,
                   num_fds_according_to_header);
      goto out;
    }

  builder = g_variant_builder_new (G_VARIANT_TYPE ("a{yv}"));//G_VARIANT_TYPE_ARRAY);
  g_hash_table_iter_init (&hash_iter, message->priv->headers);
  while (g_hash_table_iter_next (&hash_iter, &key, (gpointer) &header_value))
    {
      g_variant_builder_add (builder,
                             "{yv}",
                             (guchar) GPOINTER_TO_UINT (key),
                             header_value);
    }
  header_fields = g_variant_new ("a{yv}", builder);

  if (!append_value_to_blob (header_fields, mos, dos, NULL, error))
    {
      g_variant_unref (header_fields);
      goto out;
    }
  g_variant_unref (header_fields);

  /* header size must be a multiple of 8 */
  ensure_output_padding (mos, dos, 8);

  body_start_offset = g_memory_output_stream_get_data_size (mos);

  signature = g_dbus_message_get_header (message, G_DBUS_MESSAGE_HEADER_FIELD_SIGNATURE);
  signature_str = NULL;
  if (signature != NULL)
      signature_str = g_variant_get_string (signature, NULL);
  if (message->priv->body != NULL)
    {
      gchar *tupled_signature_str;
      tupled_signature_str = g_strdup_printf ("(%s)", signature_str);
      if (signature == NULL)
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_INVALID_ARGUMENT,
                       _("Message body has signature `%s' but there is no signature header"),
                       signature_str);
          g_free (tupled_signature_str);
          goto out;
        }
      else if (g_strcmp0 (tupled_signature_str, g_variant_get_type_string (message->priv->body)) != 0)
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_INVALID_ARGUMENT,
                       _("Message body has type signature `%s' but signature in the header field is `%s'"),
                       tupled_signature_str, g_variant_get_type_string (message->priv->body));
          g_free (tupled_signature_str);
          goto out;
        }
      g_free (tupled_signature_str);
      if (!append_body_to_blob (message->priv->body, mos, dos, error))
        goto out;
    }
  else
    {
      if (signature != NULL)
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_INVALID_ARGUMENT,
                       _("Message body is empty but signature in the header field is `(%s)'"),
                       signature_str);
          goto out;
        }
    }

  /* OK, we're done writing the message - set the body length */
  size = g_memory_output_stream_get_data_size (mos);
  body_size = size - body_start_offset;

  if (!g_seekable_seek (G_SEEKABLE (mos), body_len_offset, G_SEEK_SET, NULL, error))
    goto out;

  g_data_output_stream_put_uint32 (dos, body_size, NULL, NULL);

  *out_size = size;
  ret = g_memdup (g_memory_output_stream_get_data (mos), size);

 out:
  g_object_unref (dos);
  g_object_unref (mos);

  return ret;
}

/* ---------------------------------------------------------------------------------------------------- */

static guint32
get_uint32_header (GDBusMessage            *message,
                   GDBusMessageHeaderField  header_field)
{
  GVariant *value;
  guint32 ret;

  ret = 0;
  value = g_hash_table_lookup (message->priv->headers, GUINT_TO_POINTER (header_field));
  if (value != NULL && g_variant_is_of_type (value, G_VARIANT_TYPE_UINT32))
    ret = g_variant_get_uint32 (value);

  return ret;
}

static const gchar *
get_string_header (GDBusMessage            *message,
                   GDBusMessageHeaderField  header_field)
{
  GVariant *value;
  const gchar *ret;

  ret = NULL;
  value = g_hash_table_lookup (message->priv->headers, GUINT_TO_POINTER (header_field));
  if (value != NULL && g_variant_is_of_type (value, G_VARIANT_TYPE_STRING))
    ret = g_variant_get_string (value, NULL);

  return ret;
}

static const gchar *
get_object_path_header (GDBusMessage            *message,
                        GDBusMessageHeaderField  header_field)
{
  GVariant *value;
  const gchar *ret;

  ret = NULL;
  value = g_hash_table_lookup (message->priv->headers, GUINT_TO_POINTER (header_field));
  if (value != NULL && g_variant_is_of_type (value, G_VARIANT_TYPE_OBJECT_PATH))
    ret = g_variant_get_string (value, NULL);

  return ret;
}

static const gchar *
get_signature_header (GDBusMessage            *message,
                      GDBusMessageHeaderField  header_field)
{
  GVariant *value;
  const gchar *ret;

  ret = NULL;
  value = g_hash_table_lookup (message->priv->headers, GUINT_TO_POINTER (header_field));
  if (value != NULL && g_variant_is_of_type (value, G_VARIANT_TYPE_SIGNATURE))
    ret = g_variant_get_string (value, NULL);

  return ret;
}

/* ---------------------------------------------------------------------------------------------------- */

static void
set_uint32_header (GDBusMessage             *message,
                   GDBusMessageHeaderField   header_field,
                   guint32                   value)
{
  g_dbus_message_set_header (message,
                             header_field,
                             g_variant_new_uint32 (value));
}

static void
set_string_header (GDBusMessage             *message,
                   GDBusMessageHeaderField   header_field,
                   const gchar              *value)
{
  g_dbus_message_set_header (message,
                             header_field,
                             value == NULL ? NULL : g_variant_new_string (value));
}

static void
set_object_path_header (GDBusMessage             *message,
                        GDBusMessageHeaderField   header_field,
                        const gchar              *value)
{
  g_dbus_message_set_header (message,
                             header_field,
                             value == NULL ? NULL : g_variant_new_object_path (value));
}

static void
set_signature_header (GDBusMessage             *message,
                      GDBusMessageHeaderField   header_field,
                      const gchar              *value)
{
  g_dbus_message_set_header (message,
                             header_field,
                             value == NULL ? NULL : g_variant_new_signature (value));
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_dbus_message_get_reply_serial:
 * @message: A #GDBusMessage.
 *
 * Convenience getter for the %G_DBUS_MESSAGE_HEADER_FIELD_REPLY_SERIAL header field.
 *
 * Returns: The value.
 *
 * Since: 2.26
 */
guint32
g_dbus_message_get_reply_serial (GDBusMessage  *message)
{
  g_return_val_if_fail (G_IS_DBUS_MESSAGE (message), 0);
  return get_uint32_header (message, G_DBUS_MESSAGE_HEADER_FIELD_REPLY_SERIAL);
}

/**
 * g_dbus_message_set_reply_serial:
 * @message: A #GDBusMessage.
 * @value: The value to set.
 *
 * Convenience setter for the %G_DBUS_MESSAGE_HEADER_FIELD_REPLY_SERIAL header field.
 *
 * Since: 2.26
 */
void
g_dbus_message_set_reply_serial (GDBusMessage  *message,
                                 guint32        value)
{
  g_return_if_fail (G_IS_DBUS_MESSAGE (message));
  set_uint32_header (message, G_DBUS_MESSAGE_HEADER_FIELD_REPLY_SERIAL, value);
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_dbus_message_get_interface:
 * @message: A #GDBusMessage.
 *
 * Convenience getter for the %G_DBUS_MESSAGE_HEADER_FIELD_INTERFACE header field.
 *
 * Returns: The value.
 *
 * Since: 2.26
 */
const gchar *
g_dbus_message_get_interface (GDBusMessage  *message)
{
  g_return_val_if_fail (G_IS_DBUS_MESSAGE (message), NULL);
  return get_string_header (message, G_DBUS_MESSAGE_HEADER_FIELD_INTERFACE);
}

/**
 * g_dbus_message_set_interface:
 * @message: A #GDBusMessage.
 * @value: The value to set.
 *
 * Convenience setter for the %G_DBUS_MESSAGE_HEADER_FIELD_INTERFACE header field.
 *
 * Since: 2.26
 */
void
g_dbus_message_set_interface (GDBusMessage  *message,
                              const gchar   *value)
{
  g_return_if_fail (G_IS_DBUS_MESSAGE (message));
  g_return_if_fail (value == NULL || g_dbus_is_interface_name (value));
  set_string_header (message, G_DBUS_MESSAGE_HEADER_FIELD_INTERFACE, value);
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_dbus_message_get_member:
 * @message: A #GDBusMessage.
 *
 * Convenience getter for the %G_DBUS_MESSAGE_HEADER_FIELD_MEMBER header field.
 *
 * Returns: The value.
 *
 * Since: 2.26
 */
const gchar *
g_dbus_message_get_member (GDBusMessage  *message)
{
  g_return_val_if_fail (G_IS_DBUS_MESSAGE (message), NULL);
  return get_string_header (message, G_DBUS_MESSAGE_HEADER_FIELD_MEMBER);
}

/**
 * g_dbus_message_set_member:
 * @message: A #GDBusMessage.
 * @value: The value to set.
 *
 * Convenience setter for the %G_DBUS_MESSAGE_HEADER_FIELD_MEMBER header field.
 *
 * Since: 2.26
 */
void
g_dbus_message_set_member (GDBusMessage  *message,
                           const gchar   *value)
{
  g_return_if_fail (G_IS_DBUS_MESSAGE (message));
  g_return_if_fail (value == NULL || g_dbus_is_member_name (value));
  set_string_header (message, G_DBUS_MESSAGE_HEADER_FIELD_MEMBER, value);
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_dbus_message_get_path:
 * @message: A #GDBusMessage.
 *
 * Convenience getter for the %G_DBUS_MESSAGE_HEADER_FIELD_PATH header field.
 *
 * Returns: The value.
 *
 * Since: 2.26
 */
const gchar *
g_dbus_message_get_path (GDBusMessage  *message)
{
  g_return_val_if_fail (G_IS_DBUS_MESSAGE (message), NULL);
  return get_object_path_header (message, G_DBUS_MESSAGE_HEADER_FIELD_PATH);
}

/**
 * g_dbus_message_set_path:
 * @message: A #GDBusMessage.
 * @value: The value to set.
 *
 * Convenience setter for the %G_DBUS_MESSAGE_HEADER_FIELD_PATH header field.
 *
 * Since: 2.26
 */
void
g_dbus_message_set_path (GDBusMessage  *message,
                         const gchar   *value)
{
  g_return_if_fail (G_IS_DBUS_MESSAGE (message));
  g_return_if_fail (value == NULL || g_variant_is_object_path (value));
  set_object_path_header (message, G_DBUS_MESSAGE_HEADER_FIELD_PATH, value);
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_dbus_message_get_sender:
 * @message: A #GDBusMessage.
 *
 * Convenience getter for the %G_DBUS_MESSAGE_HEADER_FIELD_SENDER header field.
 *
 * Returns: The value.
 *
 * Since: 2.26
 */
const gchar *
g_dbus_message_get_sender (GDBusMessage *message)
{
  g_return_val_if_fail (G_IS_DBUS_MESSAGE (message), NULL);
  return get_string_header (message, G_DBUS_MESSAGE_HEADER_FIELD_SENDER);
}

/**
 * g_dbus_message_set_sender:
 * @message: A #GDBusMessage.
 * @value: The value to set.
 *
 * Convenience setter for the %G_DBUS_MESSAGE_HEADER_FIELD_SENDER header field.
 *
 * Since: 2.26
 */
void
g_dbus_message_set_sender (GDBusMessage  *message,
                           const gchar   *value)
{
  g_return_if_fail (G_IS_DBUS_MESSAGE (message));
  g_return_if_fail (value == NULL || g_dbus_is_name (value));
  set_string_header (message, G_DBUS_MESSAGE_HEADER_FIELD_SENDER, value);
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_dbus_message_get_destination:
 * @message: A #GDBusMessage.
 *
 * Convenience getter for the %G_DBUS_MESSAGE_HEADER_FIELD_DESTINATION header field.
 *
 * Returns: The value.
 *
 * Since: 2.26
 */
const gchar *
g_dbus_message_get_destination (GDBusMessage  *message)
{
  g_return_val_if_fail (G_IS_DBUS_MESSAGE (message), NULL);
  return get_string_header (message, G_DBUS_MESSAGE_HEADER_FIELD_DESTINATION);
}

/**
 * g_dbus_message_set_destination:
 * @message: A #GDBusMessage.
 * @value: The value to set.
 *
 * Convenience setter for the %G_DBUS_MESSAGE_HEADER_FIELD_DESTINATION header field.
 *
 * Since: 2.26
 */
void
g_dbus_message_set_destination (GDBusMessage  *message,
                                const gchar   *value)
{
  g_return_if_fail (G_IS_DBUS_MESSAGE (message));
  g_return_if_fail (value == NULL || g_dbus_is_name (value));
  set_string_header (message, G_DBUS_MESSAGE_HEADER_FIELD_DESTINATION, value);
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_dbus_message_get_error_name:
 * @message: A #GDBusMessage.
 *
 * Convenience getter for the %G_DBUS_MESSAGE_HEADER_FIELD_ERROR_NAME header field.
 *
 * Returns: The value.
 *
 * Since: 2.26
 */
const gchar *
g_dbus_message_get_error_name (GDBusMessage  *message)
{
  g_return_val_if_fail (G_IS_DBUS_MESSAGE (message), NULL);
  return get_string_header (message, G_DBUS_MESSAGE_HEADER_FIELD_ERROR_NAME);
}

/**
 * g_dbus_message_set_error_name:
 * @message: A #GDBusMessage.
 * @value: The value to set.
 *
 * Convenience setter for the %G_DBUS_MESSAGE_HEADER_FIELD_ERROR_NAME header field.
 *
 * Since: 2.26
 */
void
g_dbus_message_set_error_name (GDBusMessage  *message,
                               const gchar   *value)
{
  g_return_if_fail (G_IS_DBUS_MESSAGE (message));
  g_return_if_fail (value == NULL || g_dbus_is_interface_name (value));
  set_string_header (message, G_DBUS_MESSAGE_HEADER_FIELD_ERROR_NAME, value);
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_dbus_message_get_signature:
 * @message: A #GDBusMessage.
 *
 * Convenience getter for the %G_DBUS_MESSAGE_HEADER_FIELD_SIGNATURE header field.
 *
 * Returns: The value.
 *
 * Since: 2.26
 */
const gchar *
g_dbus_message_get_signature (GDBusMessage  *message)
{
  const gchar *ret;
  g_return_val_if_fail (G_IS_DBUS_MESSAGE (message), NULL);
  ret = get_signature_header (message, G_DBUS_MESSAGE_HEADER_FIELD_SIGNATURE);
  if (ret == NULL)
    ret = "";
  return ret;
}

/**
 * g_dbus_message_set_signature:
 * @message: A #GDBusMessage.
 * @value: The value to set.
 *
 * Convenience setter for the %G_DBUS_MESSAGE_HEADER_FIELD_SIGNATURE header field.
 *
 * Since: 2.26
 */
void
g_dbus_message_set_signature (GDBusMessage  *message,
                              const gchar   *value)
{
  g_return_if_fail (G_IS_DBUS_MESSAGE (message));
  g_return_if_fail (value == NULL || g_variant_is_signature (value));
  set_signature_header (message, G_DBUS_MESSAGE_HEADER_FIELD_SIGNATURE, value);
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_dbus_message_get_arg0:
 * @message: A #GDBusMessage.
 *
 * Convenience to get the first item in the body of @message.
 *
 * Returns: The string item or %NULL if the first item in the body of
 * @message is not a string.
 *
 * Since: 2.26
 */
const gchar *
g_dbus_message_get_arg0 (GDBusMessage  *message)
{
  const gchar *ret;

  g_return_val_if_fail (G_IS_DBUS_MESSAGE (message), NULL);

  ret = NULL;

  if (message->priv->body != NULL && g_variant_is_of_type (message->priv->body, G_VARIANT_TYPE_TUPLE))
    {
      GVariant *item;
      item = g_variant_get_child_value (message->priv->body, 0);
      if (g_variant_is_of_type (item, G_VARIANT_TYPE_STRING))
        ret = g_variant_get_string (item, NULL);
    }

  return ret;
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_dbus_message_get_num_unix_fds:
 * @message: A #GDBusMessage.
 *
 * Convenience getter for the %G_DBUS_MESSAGE_HEADER_FIELD_NUM_UNIX_FDS header field.
 *
 * Returns: The value.
 *
 * Since: 2.26
 */
guint32
g_dbus_message_get_num_unix_fds (GDBusMessage *message)
{
  g_return_val_if_fail (G_IS_DBUS_MESSAGE (message), 0);
  return get_uint32_header (message, G_DBUS_MESSAGE_HEADER_FIELD_NUM_UNIX_FDS);
}

/**
 * g_dbus_message_set_num_unix_fds:
 * @message: A #GDBusMessage.
 * @value: The value to set.
 *
 * Convenience setter for the %G_DBUS_MESSAGE_HEADER_FIELD_NUM_UNIX_FDS header field.
 *
 * Since: 2.26
 */
void
g_dbus_message_set_num_unix_fds (GDBusMessage  *message,
                                 guint32        value)
{
  g_return_if_fail (G_IS_DBUS_MESSAGE (message));
  set_uint32_header (message, G_DBUS_MESSAGE_HEADER_FIELD_NUM_UNIX_FDS, value);
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_dbus_message_to_gerror:
 * @message: A #GDBusMessage.
 * @error: The #GError to set.
 *
 * If @message is not of type %G_DBUS_MESSAGE_TYPE_ERROR does
 * nothing and returns %FALSE.
 *
 * Otherwise this method encodes the error in @message as a #GError
 * using g_dbus_error_set_dbus_error() using the information in the
 * %G_DBUS_MESSAGE_HEADER_FIELD_ERROR_NAME header field of @message as
 * well as the first string item in @message's body.
 *
 * Returns: %TRUE if @error was set, %FALSE otherwise.
 *
 * Since: 2.26
 */
gboolean
g_dbus_message_to_gerror (GDBusMessage   *message,
                          GError        **error)
{
  gboolean ret;
  const gchar *error_name;

  g_return_val_if_fail (G_IS_DBUS_MESSAGE (message), FALSE);

  ret = FALSE;
  if (message->priv->type != G_DBUS_MESSAGE_TYPE_ERROR)
    goto out;

  error_name = g_dbus_message_get_error_name (message);
  if (error_name != NULL)
    {
      GVariant *body;

      body = g_dbus_message_get_body (message);

      if (body != NULL && g_variant_is_of_type (body, G_VARIANT_TYPE ("(s)")))
        {
          const gchar *error_message;
          g_variant_get (body, "(s)", &error_message);
          g_dbus_error_set_dbus_error (error,
                                       error_name,
                                       error_message,
                                       NULL);
        }
      else
        {
          /* these two situations are valid, yet pretty rare */
          if (body != NULL)
            {
              g_dbus_error_set_dbus_error (error,
                                           error_name,
                                           "",
                                           _("Error return with body of type `%s'"),
                                           g_variant_get_type_string (body));
            }
          else
            {
              g_dbus_error_set_dbus_error (error,
                                           error_name,
                                           "",
                                           _("Error return with empty body"));
            }
        }
    }
  else
    {
      /* TOOD: this shouldn't happen - should check this at message serialization
       * time and disconnect the peer.
       */
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   "Error return without error-name header!");
    }

  ret = TRUE;

 out:
  return ret;
}

/* ---------------------------------------------------------------------------------------------------- */

static gchar *
enum_to_string (GType enum_type, gint value)
{
  gchar *ret;
  GEnumClass *klass;
  GEnumValue *enum_value;

  klass = g_type_class_ref (enum_type);
  enum_value = g_enum_get_value (klass, value);
  if (enum_value != NULL)
    ret = g_strdup (enum_value->value_nick);
  else
    ret = g_strdup_printf ("unknown (value %d)", value);
  g_type_class_unref (klass);
  return ret;
}

static gchar *
flags_to_string (GType flags_type, guint value)
{
  GString *s;
  GFlagsClass *klass;
  guint n;

  klass = g_type_class_ref (flags_type);
  s = g_string_new (NULL);
  for (n = 0; n < 32; n++)
    {
      if ((value & (1<<n)) != 0)
        {
          GFlagsValue *flags_value;
          flags_value = g_flags_get_first_value (klass, (1<<n));
          if (s->len > 0)
            g_string_append_c (s, ',');
          if (flags_value != NULL)
            g_string_append (s, flags_value->value_nick);
          else
            g_string_append_printf (s, "unknown (bit %d)", n);
        }
    }
  if (s->len == 0)
    g_string_append (s, "none");
  g_type_class_unref (klass);
  return g_string_free (s, FALSE);;
}

static gint
_sort_keys_func (gconstpointer a,
                 gconstpointer b)
{
  gint ia;
  gint ib;

  ia = GPOINTER_TO_INT (a);
  ib = GPOINTER_TO_INT (b);

  return ia - ib;
}

/**
 * g_dbus_message_print:
 * @message: A #GDBusMessage.
 * @indent: Indentation level.
 *
 * Produces a human-readable multi-line description of @message.
 *
 * The contents of the description has no ABI guarantees, the contents
 * and formatting is subject to change at any time. Typical output
 * looks something like this:
 * <programlisting>
 * Type:    method-call
 * Flags:   none
 * Version: 0
 * Serial:  4
 * Headers:
 *   path -> objectpath '/org/gtk/GDBus/TestObject'
 *   interface -> 'org.gtk.GDBus.TestInterface'
 *   member -> 'GimmeStdout'
 *   destination -> ':1.146'
 * Body: ()
 * UNIX File Descriptors:
 *   (none)
 * </programlisting>
 * or
 * <programlisting>
 * Type:    method-return
 * Flags:   no-reply-expected
 * Version: 0
 * Serial:  477
 * Headers:
 *   reply-serial -> uint32 4
 *   destination -> ':1.159'
 *   sender -> ':1.146'
 *   num-unix-fds -> uint32 1
 * Body: ()
 * UNIX File Descriptors:
 *   fd 12: dev=0:10,mode=020620,ino=5,uid=500,gid=5,rdev=136:2,size=0,atime=1273085037,mtime=1273085851,ctime=1272982635
 * </programlisting>
 *
 * Returns: A string that should be freed with g_free().
 *
 * Since: 2.26
 */
gchar *
g_dbus_message_print (GDBusMessage *message,
                      guint         indent)
{
  GString *str;
  gchar *s;
  GList *keys;
  GList *l;

  g_return_val_if_fail (G_IS_DBUS_MESSAGE (message), NULL);

  str = g_string_new (NULL);

  s = enum_to_string (G_TYPE_DBUS_MESSAGE_TYPE, message->priv->type);
  g_string_append_printf (str, "%*sType:    %s\n", indent, "", s);
  g_free (s);
  s = flags_to_string (G_TYPE_DBUS_MESSAGE_FLAGS, message->priv->flags);
  g_string_append_printf (str, "%*sFlags:   %s\n", indent, "", s);
  g_free (s);
  g_string_append_printf (str, "%*sVersion: %d\n", indent, "", message->priv->major_protocol_version);
  g_string_append_printf (str, "%*sSerial:  %d\n", indent, "", message->priv->serial);

  g_string_append_printf (str, "%*sHeaders:\n", indent, "");
  keys = g_hash_table_get_keys (message->priv->headers);
  keys = g_list_sort (keys, _sort_keys_func);
  if (keys != NULL)
    {
      for (l = keys; l != NULL; l = l->next)
        {
          gint key = GPOINTER_TO_INT (l->data);
          GVariant *value;
          gchar *value_str;

          value = g_hash_table_lookup (message->priv->headers, l->data);
          g_assert (value != NULL);

          s = enum_to_string (G_TYPE_DBUS_MESSAGE_HEADER_FIELD, key);
          value_str = g_variant_print (value, TRUE);
          g_string_append_printf (str, "%*s  %s -> %s\n", indent, "", s, value_str);
          g_free (s);
          g_free (value_str);
        }
    }
  else
    {
      g_string_append_printf (str, "%*s  (none)\n", indent, "");
    }
  g_string_append_printf (str, "%*sBody: ", indent, "");
  if (message->priv->body != NULL)
    {
      g_variant_print_string (message->priv->body,
                              str,
                              TRUE);
    }
  else
    {
      g_string_append (str, "()");
    }
  g_string_append (str, "\n");
#ifdef G_OS_UNIX
  g_string_append_printf (str, "%*sUNIX File Descriptors:\n", indent, "");
  if (message->priv->fd_list != NULL)
    {
      gint num_fds;
      const gint *fds;
      gint n;

      fds = g_unix_fd_list_peek_fds (message->priv->fd_list, &num_fds);
      if (num_fds > 0)
        {
          for (n = 0; n < num_fds; n++)
            {
              GString *fs;
              struct stat statbuf;
              fs = g_string_new (NULL);
              if (fstat (fds[n], &statbuf) == 0)
                {
                  g_string_append_printf (fs, "%s" "dev=%d:%d", fs->len > 0 ? "," : "",
                                          major (statbuf.st_dev), minor (statbuf.st_dev));
                  g_string_append_printf (fs, "%s" "mode=0%o", fs->len > 0 ? "," : "",
                                          statbuf.st_mode);
                  g_string_append_printf (fs, "%s" "ino=%" G_GUINT64_FORMAT, fs->len > 0 ? "," : "",
                                          (guint64) statbuf.st_ino);
                  g_string_append_printf (fs, "%s" "uid=%d", fs->len > 0 ? "," : "",
                                          statbuf.st_uid);
                  g_string_append_printf (fs, "%s" "gid=%d", fs->len > 0 ? "," : "",
                                          statbuf.st_gid);
                  g_string_append_printf (fs, "%s" "rdev=%d:%d", fs->len > 0 ? "," : "",
                                          major (statbuf.st_rdev), minor (statbuf.st_rdev));
                  g_string_append_printf (fs, "%s" "size=%" G_GUINT64_FORMAT, fs->len > 0 ? "," : "",
                                          (guint64) statbuf.st_size);
                  g_string_append_printf (fs, "%s" "atime=%" G_GUINT64_FORMAT, fs->len > 0 ? "," : "",
                                          (guint64) statbuf.st_atime);
                  g_string_append_printf (fs, "%s" "mtime=%" G_GUINT64_FORMAT, fs->len > 0 ? "," : "",
                                          (guint64) statbuf.st_mtime);
                  g_string_append_printf (fs, "%s" "ctime=%" G_GUINT64_FORMAT, fs->len > 0 ? "," : "",
                                          (guint64) statbuf.st_ctime);
                }
              else
                {
                  g_string_append_printf (fs, "(fstat failed: %s)", strerror (errno));
                }
              g_string_append_printf (str, "%*s  fd %d: %s\n", indent, "", fds[n], fs->str);
              g_string_free (fs, TRUE);
            }
        }
      else
        {
          g_string_append_printf (str, "%*s  (empty)\n", indent, "");
        }
    }
  else
    {
      g_string_append_printf (str, "%*s  (none)\n", indent, "");
    }
#endif

  return g_string_free (str, FALSE);
}

