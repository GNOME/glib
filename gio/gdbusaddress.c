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

#include <stdlib.h>
#include <string.h>

#include "gdbusutils.h"
#include "gdbusaddress.h"
#include "gdbuserror.h"
#include "gioenumtypes.h"
#include "gdbusprivate.h"

#ifdef G_OS_UNIX
#include <gio/gunixsocketaddress.h>
#endif

#include "glibintl.h"
#include "gioalias.h"

/**
 * SECTION:gdbusaddress
 * @title: D-Bus Addresses
 * @short_description: D-Bus connection endpoints
 * @include: gio/gio.h
 *
 * Routines for working with D-Bus addresses. A D-Bus address is a string
 * like "unix:tmpdir=/tmp/my-app-name". The exact format of addresses
 * is explained in detail in the <link linkend="http://dbus.freedesktop.org/doc/dbus-specification.html#addresses">D-Bus specification</link>.
 */

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_dbus_is_address:
 * @string: A string.
 *
 * Checks if @string is a D-Bus address.
 *
 * This doesn't check if @string is actually supported by #GDBusServer
 * or #GDBusConnection - use g_dbus_is_supported_address() to do more
 * checks.
 *
 * Returns: %TRUE if @string is a valid D-Bus address, %FALSE otherwise.
 *
 * Since: 2.26
 */
gboolean
g_dbus_is_address (const gchar *string)
{
  guint n;
  gchar **a;
  gboolean ret;

  ret = FALSE;

  g_return_val_if_fail (string != NULL, FALSE);

  a = g_strsplit (string, ";", 0);
  for (n = 0; a[n] != NULL; n++)
    {
      if (!_g_dbus_address_parse_entry (a[n],
                                        NULL,
                                        NULL,
                                        NULL))
        goto out;
    }

  ret = TRUE;

 out:
  g_strfreev (a);
  return ret;
}

static gboolean
is_valid_unix (const gchar  *address_entry,
               GHashTable   *key_value_pairs,
               GError      **error)
{
  gboolean ret;
  GList *keys;
  GList *l;
  const gchar *path;
  const gchar *tmpdir;
  const gchar *abstract;

  ret = FALSE;
  keys = NULL;
  path = NULL;
  tmpdir = NULL;
  abstract = NULL;

  keys = g_hash_table_get_keys (key_value_pairs);
  for (l = keys; l != NULL; l = l->next)
    {
      const gchar *key = l->data;
      if (g_strcmp0 (key, "path") == 0)
        path = g_hash_table_lookup (key_value_pairs, key);
      else if (g_strcmp0 (key, "tmpdir") == 0)
        tmpdir = g_hash_table_lookup (key_value_pairs, key);
      else if (g_strcmp0 (key, "abstract") == 0)
        abstract = g_hash_table_lookup (key_value_pairs, key);
      else
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_INVALID_ARGUMENT,
                       _("Unsupported key `%s' in address entry `%s'"),
                       key,
                       address_entry);
          goto out;
        }
    }

  if (path != NULL)
    {
      if (tmpdir != NULL || abstract != NULL)
        goto meaningless;
      /* TODO: validate path */
    }
  else if (tmpdir != NULL)
    {
      if (path != NULL || abstract != NULL)
        goto meaningless;
      /* TODO: validate tmpdir */
    }
  else if (abstract != NULL)
    {
      if (path != NULL || tmpdir != NULL)
        goto meaningless;
      /* TODO: validate abstract */
    }
  else
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_ARGUMENT,
                   _("Address `%s' is invalid (need exactly one of path, tmpdir or abstract keys"),
                   address_entry);
      goto out;
    }


  ret= TRUE;
  goto out;

 meaningless:
  g_set_error (error,
               G_IO_ERROR,
               G_IO_ERROR_INVALID_ARGUMENT,
               _("Meaningless key/value pair combination in address entry `%s'"),
               address_entry);

 out:
  g_list_free (keys);

  return ret;
}

static gboolean
is_valid_nonce_tcp (const gchar  *address_entry,
                    GHashTable   *key_value_pairs,
                    GError      **error)
{
  gboolean ret;
  GList *keys;
  GList *l;
  const gchar *host;
  const gchar *port;
  const gchar *family;
  const gchar *nonce_file;
  gint port_num;
  gchar *endp;

  ret = FALSE;
  keys = NULL;
  host = NULL;
  port = NULL;
  family = NULL;
  nonce_file = NULL;

  keys = g_hash_table_get_keys (key_value_pairs);
  for (l = keys; l != NULL; l = l->next)
    {
      const gchar *key = l->data;
      if (g_strcmp0 (key, "host") == 0)
        host = g_hash_table_lookup (key_value_pairs, key);
      else if (g_strcmp0 (key, "port") == 0)
        port = g_hash_table_lookup (key_value_pairs, key);
      else if (g_strcmp0 (key, "family") == 0)
        family = g_hash_table_lookup (key_value_pairs, key);
      else if (g_strcmp0 (key, "noncefile") == 0)
        nonce_file = g_hash_table_lookup (key_value_pairs, key);
      else
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_INVALID_ARGUMENT,
                       _("Unsupported key `%s' in address entry `%s'"),
                       key,
                       address_entry);
          goto out;
        }
    }

  if (port != NULL)
    {
      port_num = strtol (port, &endp, 10);
      if ((*port == '\0' || *endp != '\0') || port_num < 0 || port_num >= 65536)
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_INVALID_ARGUMENT,
                       _("Error in address `%s' - the port attribute is malformed"),
                       address_entry);
          goto out;
        }
    }

  if (family != NULL && !(g_strcmp0 (family, "ipv4") == 0 || g_strcmp0 (family, "ipv6") == 0))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_ARGUMENT,
                   _("Error in address `%s' - the family attribute is malformed"),
                   address_entry);
      goto out;
    }

  ret= TRUE;

 out:
  g_list_free (keys);

  return ret;
}

static gboolean
is_valid_tcp (const gchar  *address_entry,
              GHashTable   *key_value_pairs,
              GError      **error)
{
  gboolean ret;
  GList *keys;
  GList *l;
  const gchar *host;
  const gchar *port;
  const gchar *family;
  gint port_num;
  gchar *endp;

  ret = FALSE;
  keys = NULL;
  host = NULL;
  port = NULL;
  family = NULL;

  keys = g_hash_table_get_keys (key_value_pairs);
  for (l = keys; l != NULL; l = l->next)
    {
      const gchar *key = l->data;
      if (g_strcmp0 (key, "host") == 0)
        host = g_hash_table_lookup (key_value_pairs, key);
      else if (g_strcmp0 (key, "port") == 0)
        port = g_hash_table_lookup (key_value_pairs, key);
      else if (g_strcmp0 (key, "family") == 0)
        family = g_hash_table_lookup (key_value_pairs, key);
      else
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_INVALID_ARGUMENT,
                       _("Unsupported key `%s' in address entry `%s'"),
                       key,
                       address_entry);
          goto out;
        }
    }

  if (port != NULL)
    {
      port_num = strtol (port, &endp, 10);
      if ((*port == '\0' || *endp != '\0') || port_num < 0 || port_num >= 65536)
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_INVALID_ARGUMENT,
                       _("Error in address `%s' - the port attribute is malformed"),
                       address_entry);
          goto out;
        }
    }

  if (family != NULL && !(g_strcmp0 (family, "ipv4") == 0 || g_strcmp0 (family, "ipv6") == 0))
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_ARGUMENT,
                   _("Error in address `%s' - the family attribute is malformed"),
                   address_entry);
      goto out;
    }

  ret= TRUE;

 out:
  g_list_free (keys);

  return ret;
}

/**
 * g_dbus_is_supported_address:
 * @string: A string.
 * @error: Return location for error or %NULL.
 *
 * Like g_dbus_is_address() but also checks if the library suppors the
 * transports in @string and that key/value pairs for each transport
 * are valid.
 *
 * Returns: %TRUE if @string is a valid D-Bus address that is
 * supported by this library, %FALSE if @error is set.
 *
 * Since: 2.26
 */
gboolean
g_dbus_is_supported_address (const gchar  *string,
                             GError      **error)
{
  guint n;
  gchar **a;
  gboolean ret;

  ret = FALSE;

  g_return_val_if_fail (string != NULL, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  a = g_strsplit (string, ";", 0);
  for (n = 0; a[n] != NULL; n++)
    {
      gchar *transport_name;
      GHashTable *key_value_pairs;
      gboolean supported;

      if (!_g_dbus_address_parse_entry (a[n],
                                        &transport_name,
                                        &key_value_pairs,
                                        error))
        goto out;

      supported = FALSE;
      if (g_strcmp0 (transport_name, "unix") == 0)
        supported = is_valid_unix (a[n], key_value_pairs, error);
      else if (g_strcmp0 (transport_name, "tcp") == 0)
        supported = is_valid_tcp (a[n], key_value_pairs, error);
      else if (g_strcmp0 (transport_name, "nonce-tcp") == 0)
        supported = is_valid_nonce_tcp (a[n], key_value_pairs, error);

      g_free (transport_name);
      g_hash_table_unref (key_value_pairs);

      if (!supported)
        goto out;
    }

  ret = TRUE;

 out:
  g_strfreev (a);

  g_assert (ret || (!ret && (error == NULL || *error != NULL)));

  return ret;
}

gboolean
_g_dbus_address_parse_entry (const gchar   *address_entry,
                             gchar        **out_transport_name,
                             GHashTable   **out_key_value_pairs,
                             GError       **error)
{
  gboolean ret;
  GHashTable *key_value_pairs;
  gchar *transport_name;
  gchar **kv_pairs;
  const gchar *s;
  guint n;

  ret = FALSE;
  kv_pairs = NULL;
  transport_name = NULL;
  key_value_pairs = NULL;

  s = strchr (address_entry, ':');
  if (s == NULL)
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_ARGUMENT,
                   _("Address element `%s', does not contain a colon (:)"),
                   address_entry);
      goto out;
    }

  transport_name = g_strndup (address_entry, s - address_entry);
  key_value_pairs = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

  kv_pairs = g_strsplit (s + 1, ",", 0);
  for (n = 0; kv_pairs != NULL && kv_pairs[n] != NULL; n++)
    {
      const gchar *kv_pair = kv_pairs[n];
      gchar *key;
      gchar *value;

      s = strchr (kv_pair, '=');
      if (s == NULL)
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_INVALID_ARGUMENT,
                       _("Key/Value pair %d, `%s', in address element `%s', does not contain an equal sign"),
                       n,
                       kv_pair,
                       address_entry);
          goto out;
        }

      /* TODO: actually validate that no illegal characters are present before and after then '=' sign */
      key = g_uri_unescape_segment (kv_pair, s, NULL);
      value = g_uri_unescape_segment (s + 1, kv_pair + strlen (kv_pair), NULL);
      g_hash_table_insert (key_value_pairs, key, value);
    }

  ret = TRUE;

out:
  g_strfreev (kv_pairs);
  if (ret)
    {
      if (out_transport_name != NULL)
        *out_transport_name = transport_name;
      else
        g_free (transport_name);
      if (out_key_value_pairs != NULL)
        *out_key_value_pairs = key_value_pairs;
      else if (key_value_pairs != NULL)
        g_hash_table_unref (key_value_pairs);
    }
  else
    {
      g_free (transport_name);
      if (key_value_pairs != NULL)
        g_hash_table_unref (key_value_pairs);
    }
  return ret;
}

/* ---------------------------------------------------------------------------------------------------- */

/* TODO: Declare an extension point called GDBusTransport (or similar)
 * and move code below to extensions implementing said extension
 * point. That way we can implement a D-Bus transport over X11 without
 * making libgio link to libX11...
 */
static GIOStream *
g_dbus_address_connect (const gchar    *address_entry,
                        const gchar    *transport_name,
                        GHashTable     *key_value_pairs,
                        GCancellable   *cancellable,
                        GError        **error)
{
  GIOStream *ret;
  GSocketConnectable *connectable;
  const gchar *nonce_file;

  connectable = NULL;
  ret = NULL;
  nonce_file = NULL;

  if (FALSE)
    {
    }
#ifdef G_OS_UNIX
  else if (g_strcmp0 (transport_name, "unix") == 0)
    {
      const gchar *path;
      const gchar *abstract;
      path = g_hash_table_lookup (key_value_pairs, "path");
      abstract = g_hash_table_lookup (key_value_pairs, "abstract");
      if ((path == NULL && abstract == NULL) || (path != NULL && abstract != NULL))
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_INVALID_ARGUMENT,
                       _("Error in address `%s' - the unix transport requires exactly one of the "
                         "keys `path' or `abstract' to be set"),
                       address_entry);
        }
      else if (path != NULL)
        {
          connectable = G_SOCKET_CONNECTABLE (g_unix_socket_address_new (path));
        }
      else if (abstract != NULL)
        {
          connectable = G_SOCKET_CONNECTABLE (g_unix_socket_address_new_with_type (abstract,
                                                                                   -1,
                                                                                   G_UNIX_SOCKET_ADDRESS_ABSTRACT));
        }
      else
        {
          g_assert_not_reached ();
        }
    }
#endif
  else if (g_strcmp0 (transport_name, "tcp") == 0 || g_strcmp0 (transport_name, "nonce-tcp") == 0)
    {
      const gchar *s;
      const gchar *host;
      guint port;
      gchar *endp;
      gboolean is_nonce;

      is_nonce = (g_strcmp0 (transport_name, "nonce-tcp") == 0);

      host = g_hash_table_lookup (key_value_pairs, "host");
      if (host == NULL)
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_INVALID_ARGUMENT,
                       _("Error in address `%s' - the host attribute is missing or malformed"),
                       address_entry);
          goto out;
        }

      s = g_hash_table_lookup (key_value_pairs, "port");
      if (s == NULL)
        s = "0";
      port = strtol (s, &endp, 10);
      if ((*s == '\0' || *endp != '\0') || port < 0 || port >= 65536)
        {
          g_set_error (error,
                       G_IO_ERROR,
                       G_IO_ERROR_INVALID_ARGUMENT,
                       _("Error in address `%s' - the port attribute is missing or malformed"),
                       address_entry);
          goto out;
        }


      if (is_nonce)
        {
          nonce_file = g_hash_table_lookup (key_value_pairs, "noncefile");
          if (nonce_file == NULL)
            {
              g_set_error (error,
                           G_IO_ERROR,
                           G_IO_ERROR_INVALID_ARGUMENT,
                           _("Error in address `%s' - the noncefile attribute is missing or malformed"),
                           address_entry);
              goto out;
            }
        }

      /* TODO: deal with family */
      connectable = g_network_address_new (host, port);
    }
  else
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_ARGUMENT,
                   _("Unknown or unsupported transport `%s' for address `%s'"),
                   transport_name,
                   address_entry);
    }

  if (connectable != NULL)
    {
      GSocketClient *client;
      GSocketConnection *connection;

      g_assert (ret == NULL);
      client = g_socket_client_new ();
      connection = g_socket_client_connect (client,
                                            connectable,
                                            cancellable,
                                            error);
      g_object_unref (connectable);
      g_object_unref (client);
      if (connection == NULL)
        goto out;

      ret = G_IO_STREAM (connection);

      if (nonce_file != NULL)
        {
          gchar *nonce_contents;
          gsize nonce_length;

          /* TODO: too dangerous to read the entire file? (think denial-of-service etc.) */
          if (!g_file_get_contents (nonce_file,
                                    &nonce_contents,
                                    &nonce_length,
                                    error))
            {
              g_prefix_error (error, _("Error reading nonce file `%s':"), nonce_file);
              g_object_unref (ret);
              ret = NULL;
              goto out;
            }

          if (nonce_length != 16)
            {
              g_set_error (error,
                           G_IO_ERROR,
                           G_IO_ERROR_INVALID_ARGUMENT,
                           _("The nonce-file `%s' was %" G_GSIZE_FORMAT " bytes. Expected 16 bytes."),
                           nonce_file,
                           nonce_length);
              g_free (nonce_contents);
              g_object_unref (ret);
              ret = NULL;
              goto out;
            }

          if (!g_output_stream_write_all (g_io_stream_get_output_stream (ret),
                                          nonce_contents,
                                          nonce_length,
                                          NULL,
                                          cancellable,
                                          error))
            {
              g_prefix_error (error, _("Error write contents of nonce file `%s' to stream:"), nonce_file);
              g_object_unref (ret);
              ret = NULL;
              g_free (nonce_contents);
              goto out;
            }
          g_free (nonce_contents);
        }
    }

 out:

  return ret;
}

static GIOStream *
g_dbus_address_try_connect_one (const gchar         *address_entry,
                                gchar              **out_guid,
                                GCancellable        *cancellable,
                                GError             **error)
{
  GIOStream *ret;
  GHashTable *key_value_pairs;
  gchar *transport_name;
  const gchar *guid;

  ret = NULL;
  transport_name = NULL;
  key_value_pairs = NULL;

  if (!_g_dbus_address_parse_entry (address_entry,
                                    &transport_name,
                                    &key_value_pairs,
                                    error))
    goto out;

  ret = g_dbus_address_connect (address_entry,
                                transport_name,
                                key_value_pairs,
                                cancellable,
                                error);
  if (ret == NULL)
    goto out;

  /* TODO: validate that guid is of correct format */
  guid = g_hash_table_lookup (key_value_pairs, "guid");
  if (guid != NULL && out_guid != NULL)
    *out_guid = g_strdup (guid);

out:
  g_free (transport_name);
  if (key_value_pairs != NULL)
    g_hash_table_unref (key_value_pairs);
  return ret;
}


/* ---------------------------------------------------------------------------------------------------- */

typedef struct {
  gchar *address;
  GIOStream *stream;
  gchar *guid;
} GetStreamData;

static void
get_stream_data_free (GetStreamData *data)
{
  g_free (data->address);
  if (data->stream != NULL)
    g_object_unref (data->stream);
  g_free (data->guid);
  g_free (data);
}

static void
get_stream_thread_func (GSimpleAsyncResult *res,
                        GObject            *object,
                        GCancellable       *cancellable)
{
  GetStreamData *data;
  GError *error;

  data = g_simple_async_result_get_op_res_gpointer (res);

  error = NULL;
  data->stream = g_dbus_address_get_stream_sync (data->address,
                                                 &data->guid,
                                                 cancellable,
                                                 &error);
  if (data->stream == NULL)
    {
      g_simple_async_result_set_from_error (res, error);
      g_error_free (error);
    }
}

/**
 * g_dbus_address_get_stream:
 * @address: A valid D-Bus address.
 * @cancellable: A #GCancellable or %NULL.
 * @callback: A #GAsyncReadyCallback to call when the request is satisfied.
 * @user_data: Data to pass to @callback.
 *
 * Asynchronously connects to an endpoint specified by @address and
 * sets up the connection so it is in a state to run the client-side
 * of the D-Bus authentication conversation.
 *
 * When the operation is finished, @callback will be invoked. You can
 * then call g_dbus_address_get_stream_finish() to get the result of
 * the operation.
 *
 * This is an asynchronous failable function. See
 * g_dbus_address_get_stream_sync() for the synchronous version.
 *
 * Since: 2.26
 */
void
g_dbus_address_get_stream (const gchar         *address,
                           GCancellable        *cancellable,
                           GAsyncReadyCallback  callback,
                           gpointer             user_data)
{
  GSimpleAsyncResult *res;
  GetStreamData *data;

  g_return_if_fail (address != NULL);

  res = g_simple_async_result_new (NULL,
                                   callback,
                                   user_data,
                                   g_dbus_address_get_stream);
  data = g_new0 (GetStreamData, 1);
  data->address = g_strdup (address);
  g_simple_async_result_set_op_res_gpointer (res,
                                             data,
                                             (GDestroyNotify) get_stream_data_free);
  g_simple_async_result_run_in_thread (res,
                                       get_stream_thread_func,
                                       G_PRIORITY_DEFAULT,
                                       cancellable);
  g_object_unref (res);
}

/**
 * g_dbus_address_get_stream_finish:
 * @res: A #GAsyncResult obtained from the GAsyncReadyCallback passed to g_dbus_address_get_stream().
 * @out_guid: %NULL or return location to store the GUID extracted from @address, if any.
 * @error: Return location for error or %NULL.
 *
 * Finishes an operation started with g_dbus_address_get_stream().
 *
 * Returns: A #GIOStream or %NULL if @error is set.
 *
 * Since: 2.26
 */
GIOStream *
g_dbus_address_get_stream_finish (GAsyncResult        *res,
                                  gchar              **out_guid,
                                  GError             **error)
{
  GSimpleAsyncResult *simple = G_SIMPLE_ASYNC_RESULT (res);
  GetStreamData *data;
  GIOStream *ret;

  g_return_val_if_fail (G_IS_ASYNC_RESULT (res), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  g_warn_if_fail (g_simple_async_result_get_source_tag (simple) == g_dbus_address_get_stream);

  ret = NULL;

  data = g_simple_async_result_get_op_res_gpointer (simple);
  if (g_simple_async_result_propagate_error (simple, error))
    goto out;

  ret = g_object_ref (data->stream);
  if (out_guid != NULL)
    *out_guid = g_strdup (data->guid);

 out:
  return ret;
}

/**
 * g_dbus_address_get_stream_sync:
 * @address: A valid D-Bus address.
 * @out_guid: %NULL or return location to store the GUID extracted from @address, if any.
 * @cancellable: A #GCancellable or %NULL.
 * @error: Return location for error or %NULL.
 *
 * Synchronously connects to an endpoint specified by @address and
 * sets up the connection so it is in a state to run the client-side
 * of the D-Bus authentication conversation.
 *
 * This is a synchronous failable function. See
 * g_dbus_address_get_stream() for the asynchronous version.
 *
 * Returns: A #GIOStream or %NULL if @error is set.
 *
 * Since: 2.26
 */
GIOStream *
g_dbus_address_get_stream_sync (const gchar         *address,
                                gchar              **out_guid,
                                GCancellable        *cancellable,
                                GError             **error)
{
  GIOStream *ret;
  gchar **addr_array;
  guint n;
  GError *last_error;

  g_return_val_if_fail (address != NULL, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  ret = NULL;
  last_error = NULL;

  addr_array = g_strsplit (address, ";", 0);
  last_error = NULL;
  for (n = 0; addr_array != NULL && addr_array[n] != NULL; n++)
    {
      const gchar *addr = addr_array[n];
      GError *this_error;
      this_error = NULL;
      ret = g_dbus_address_try_connect_one (addr,
                                            out_guid,
                                            cancellable,
                                            &this_error);
      if (ret != NULL)
        {
          goto out;
        }
      else
        {
          g_assert (this_error != NULL);
          if (last_error != NULL)
            g_error_free (last_error);
          last_error = this_error;
        }
    }

 out:
  if (ret != NULL)
    {
      if (last_error != NULL)
        g_error_free (last_error);
    }
  else
    {
      g_assert (last_error != NULL);
      g_propagate_error (error, last_error);
    }
  return ret;
}

/* ---------------------------------------------------------------------------------------------------- */

/* TODO: implement for UNIX, Win32 and OS X */
static gchar *
get_session_address_platform_specific (void)
{
  return NULL;
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_dbus_address_get_for_bus_sync:
 * @bus_type: A #GBusType.
 * @cancellable: A #GCancellable or %NULL.
 * @error: Return location for error or %NULL.
 *
 * Synchronously looks up the D-Bus address for the well-known message
 * bus instance specified by @bus_type. This may involve using various
 * platform specific mechanisms.
 *
 * Returns: A valid D-Bus address string for @bus_type or %NULL if @error is set.
 *
 * Since: 2.26
 */
gchar *
g_dbus_address_get_for_bus_sync (GBusType       bus_type,
                                 GCancellable  *cancellable,
                                 GError       **error)
{
  gchar *ret;
  const gchar *starter_bus;

  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  ret = NULL;

  switch (bus_type)
    {
    case G_BUS_TYPE_SYSTEM:
      ret = g_strdup (g_getenv ("DBUS_SYSTEM_BUS_ADDRESS"));
      if (ret == NULL)
        {
          ret = g_strdup ("unix:path=/var/run/dbus/system_bus_socket");
        }
      break;

    case G_BUS_TYPE_SESSION:
      ret = g_strdup (g_getenv ("DBUS_SESSION_BUS_ADDRESS"));
      if (ret == NULL)
        {
          ret = get_session_address_platform_specific ();
          if (ret == NULL)
            {
              g_set_error (error,
                           G_IO_ERROR,
                           G_IO_ERROR_FAILED,
                           _("Cannot determine session bus address (TODO: run dbus-launch to find out)"));
            }
        }
      break;

    case G_BUS_TYPE_STARTER:
      starter_bus = g_getenv ("DBUS_STARTER_BUS_TYPE");
      if (g_strcmp0 (starter_bus, "session") == 0)
        {
          ret = g_dbus_address_get_for_bus_sync (G_BUS_TYPE_SESSION, cancellable, error);
          goto out;
        }
      else if (g_strcmp0 (starter_bus, "system") == 0)
        {
          ret = g_dbus_address_get_for_bus_sync (G_BUS_TYPE_SYSTEM, cancellable, error);
          goto out;
        }
      else
        {
          if (starter_bus != NULL)
            {
              g_set_error (error,
                           G_IO_ERROR,
                           G_IO_ERROR_FAILED,
                           _("Cannot determine bus address from DBUS_STARTER_BUS_TYPE environment variable"
                             " - unknown value `%s'"),
                           starter_bus);
            }
          else
            {
              g_set_error_literal (error,
                                   G_IO_ERROR,
                                   G_IO_ERROR_FAILED,
                                   _("Cannot determine bus address because the DBUS_STARTER_BUS_TYPE environment "
                                     "variable is not set"));
            }
        }
      break;

    default:
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_FAILED,
                   _("Unknown bus type %d"),
                   bus_type);
      break;
    }

 out:
  return ret;
}

#define __G_DBUS_ADDRESS_C__
#include "gioaliasdef.c"
