/* GLib testing framework examples and tests
 *
 * Copyright (C) 2008-2010 Red Hat, Inc.
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

#include <gio/gio.h>

#include <string.h>
#include <unistd.h>
#include <dbus/dbus.h>

/* ---------------------------------------------------------------------------------------------------- */

static void
hexdump (const guchar *str, gsize len)
{
  const guchar *data = (const guchar *) str;
  guint n, m;

  for (n = 0; n < len; n += 16)
    {
      g_printerr ("%04x: ", n);

      for (m = n; m < n + 16; m++)
        {
          if (m > n && (m%4) == 0)
            g_printerr (" ");
          if (m < len)
            g_printerr ("%02x ", data[m]);
          else
            g_printerr ("   ");
        }

      g_printerr ("   ");

      for (m = n; m < len && m < n + 16; m++)
        g_printerr ("%c", g_ascii_isprint (data[m]) ? data[m] : '.');

      g_printerr ("\n");
    }
}

/* ---------------------------------------------------------------------------------------------------- */

static gboolean
append_gv_to_dbus_iter (DBusMessageIter  *iter,
                        GVariant         *value,
                        GError          **error)
{
  const GVariantType *type;

  type = g_variant_get_type (value);
  if (g_variant_type_equal (type, G_VARIANT_TYPE_BOOLEAN))
    {
      dbus_bool_t v = g_variant_get_boolean (value);
      dbus_message_iter_append_basic (iter, DBUS_TYPE_BOOLEAN, &v);
    }
  else if (g_variant_type_equal (type, G_VARIANT_TYPE_BYTE))
    {
      guint8 v = g_variant_get_byte (value);
      dbus_message_iter_append_basic (iter, DBUS_TYPE_BYTE, &v);
    }
  else if (g_variant_type_equal (type, G_VARIANT_TYPE_INT16))
    {
      gint16 v = g_variant_get_int16 (value);
      dbus_message_iter_append_basic (iter, DBUS_TYPE_INT16, &v);
    }
  else if (g_variant_type_equal (type, G_VARIANT_TYPE_UINT16))
    {
      guint16 v = g_variant_get_uint16 (value);
      dbus_message_iter_append_basic (iter, DBUS_TYPE_UINT16, &v);
    }
  else if (g_variant_type_equal (type, G_VARIANT_TYPE_INT32))
    {
      gint32 v = g_variant_get_int32 (value);
      dbus_message_iter_append_basic (iter, DBUS_TYPE_INT32, &v);
    }
  else if (g_variant_type_equal (type, G_VARIANT_TYPE_UINT32))
    {
      guint32 v = g_variant_get_uint32 (value);
      dbus_message_iter_append_basic (iter, DBUS_TYPE_UINT32, &v);
    }
  else if (g_variant_type_equal (type, G_VARIANT_TYPE_INT64))
    {
      gint64 v = g_variant_get_int64 (value);
      dbus_message_iter_append_basic (iter, DBUS_TYPE_INT64, &v);
    }
  else if (g_variant_type_equal (type, G_VARIANT_TYPE_UINT64))
    {
      guint64 v = g_variant_get_uint64 (value);
      dbus_message_iter_append_basic (iter, DBUS_TYPE_UINT64, &v);
    }
  else if (g_variant_type_equal (type, G_VARIANT_TYPE_DOUBLE))
    {
      gdouble v = g_variant_get_double (value);
      dbus_message_iter_append_basic (iter, DBUS_TYPE_DOUBLE, &v);
    }
  else if (g_variant_type_equal (type, G_VARIANT_TYPE_STRING))
    {
      const gchar *v = g_variant_get_string (value, NULL);
      dbus_message_iter_append_basic (iter, DBUS_TYPE_STRING, &v);
    }
  else if (g_variant_type_equal (type, G_VARIANT_TYPE_OBJECT_PATH))
    {
      const gchar *v = g_variant_get_string (value, NULL);
      dbus_message_iter_append_basic (iter, DBUS_TYPE_OBJECT_PATH, &v);
    }
  else if (g_variant_type_equal (type, G_VARIANT_TYPE_SIGNATURE))
    {
      const gchar *v = g_variant_get_string (value, NULL);
      dbus_message_iter_append_basic (iter, DBUS_TYPE_SIGNATURE, &v);
    }
  else if (g_variant_type_is_variant (type))
    {
      DBusMessageIter sub;
      GVariant *child;

      child = g_variant_get_child_value (value, 0);
      dbus_message_iter_open_container (iter, DBUS_TYPE_VARIANT,
                                        g_variant_get_type_string (child),
                                        &sub);
      if (!append_gv_to_dbus_iter (&sub, child, error))
        {
            g_variant_unref (child);
            goto fail;
        }
      dbus_message_iter_close_container (iter, &sub);
      g_variant_unref (child);
    }
  else if (g_variant_type_is_array (type))
    {
      DBusMessageIter dbus_iter;
      const gchar *type_string;
      GVariantIter gv_iter;
      GVariant *item;

      type_string = g_variant_get_type_string (value);
      type_string++; /* skip the 'a' */

      dbus_message_iter_open_container (iter, DBUS_TYPE_ARRAY,
                                        type_string, &dbus_iter);
      g_variant_iter_init (&gv_iter, value);

      while ((item = g_variant_iter_next_value (&gv_iter)))
        {
          if (!append_gv_to_dbus_iter (&dbus_iter, item, error))
            {
              goto fail;
            }
        }

      dbus_message_iter_close_container (iter, &dbus_iter);
    }
  else if (g_variant_type_is_tuple (type))
    {
      DBusMessageIter dbus_iter;
      GVariantIter gv_iter;
      GVariant *item;

      dbus_message_iter_open_container (iter, DBUS_TYPE_STRUCT,
                                        NULL, &dbus_iter);
      g_variant_iter_init (&gv_iter, value);

      while ((item = g_variant_iter_next_value (&gv_iter)))
        {
          if (!append_gv_to_dbus_iter (&dbus_iter, item, error))
            goto fail;
        }

      dbus_message_iter_close_container (iter, &dbus_iter);
    }
  else if (g_variant_type_is_dict_entry (type))
    {
      DBusMessageIter dbus_iter;
      GVariant *key, *val;

      dbus_message_iter_open_container (iter, DBUS_TYPE_DICT_ENTRY,
                                        NULL, &dbus_iter);
      key = g_variant_get_child_value (value, 0);
      if (!append_gv_to_dbus_iter (&dbus_iter, key, error))
        {
          g_variant_unref (key);
          goto fail;
        }
      g_variant_unref (key);

      val = g_variant_get_child_value (value, 1);
      if (!append_gv_to_dbus_iter (&dbus_iter, val, error))
        {
          g_variant_unref (val);
          goto fail;
        }
      g_variant_unref (val);

      dbus_message_iter_close_container (iter, &dbus_iter);
    }
  else
    {
      g_set_error (error,
                   G_IO_ERROR,
                   G_IO_ERROR_INVALID_ARGUMENT,
                   "Error serializing GVariant with type-string `%s' to a D-Bus message",
                   g_variant_get_type_string (value));
      goto fail;
    }

  return TRUE;

 fail:
  return FALSE;
}

static gboolean
append_gv_to_dbus_message (DBusMessage  *message,
                           GVariant     *value,
                           GError      **error)
{
  gboolean ret;
  guint n;

  ret = FALSE;

  if (value != NULL)
    {
      DBusMessageIter iter;
      GVariantIter gv_iter;
      GVariant *item;

      dbus_message_iter_init_append (message, &iter);

      g_variant_iter_init (&gv_iter, value);
      n = 0;
      while ((item = g_variant_iter_next_value (&gv_iter)))
        {
          if (!append_gv_to_dbus_iter (&iter, item, error))
            {
              g_prefix_error (error,
                              "Error encoding in-arg %d: ",
                              n);
              goto out;
            }
          n++;
        }
    }

  ret = TRUE;

 out:
  return ret;
}

static void
print_gv_dbus_message (GVariant *value)
{
  DBusMessage *message;
  char *blob;
  int blob_len;
  GError *error;

  message = dbus_message_new (DBUS_MESSAGE_TYPE_METHOD_CALL);
  dbus_message_set_serial (message, 0x41);
  dbus_message_set_path (message, "/foo/bar");
  dbus_message_set_member (message, "Member");

  error = NULL;
  if (!append_gv_to_dbus_message (message, value, &error))
    {
      g_printerr ("Error printing GVariant as DBusMessage: %s", error->message);
      g_error_free (error);
      goto out;
    }

  dbus_message_marshal (message, &blob, &blob_len);
  g_printerr ("\n");
  hexdump ((guchar *) blob, blob_len);
 out:
  dbus_message_unref (message);
}

/* ---------------------------------------------------------------------------------------------------- */

static void
dbus_1_message_append (GString *s,
                       guint indent,
                       DBusMessageIter *iter)
{
  gint arg_type;
  DBusMessageIter sub;

  g_string_append_printf (s, "%*s", indent, "");

  arg_type = dbus_message_iter_get_arg_type (iter);
  switch (arg_type)
    {
     case DBUS_TYPE_BOOLEAN:
      {
        dbus_bool_t value;
        dbus_message_iter_get_basic (iter, &value);
        g_string_append_printf (s, "bool: %s\n", value ? "true" : "false");
        break;
      }

     case DBUS_TYPE_BYTE:
      {
        guchar value;
        dbus_message_iter_get_basic (iter, &value);
        g_string_append_printf (s, "byte: 0x%02x\n", (guint) value);
        break;
      }

     case DBUS_TYPE_INT16:
      {
        gint16 value;
        dbus_message_iter_get_basic (iter, &value);
        g_string_append_printf (s, "int16: %" G_GINT16_FORMAT "\n", value);
        break;
      }

     case DBUS_TYPE_UINT16:
      {
        guint16 value;
        dbus_message_iter_get_basic (iter, &value);
        g_string_append_printf (s, "uint16: %" G_GUINT16_FORMAT "\n", value);
        break;
      }

     case DBUS_TYPE_INT32:
      {
        gint32 value;
        dbus_message_iter_get_basic (iter, &value);
        g_string_append_printf (s, "int32: %" G_GINT32_FORMAT "\n", value);
        break;
      }

     case DBUS_TYPE_UINT32:
      {
        guint32 value;
        dbus_message_iter_get_basic (iter, &value);
        g_string_append_printf (s, "uint32: %" G_GUINT32_FORMAT "\n", value);
        break;
      }

     case DBUS_TYPE_INT64:
      {
        gint64 value;
        dbus_message_iter_get_basic (iter, &value);
        g_string_append_printf (s, "int64: %" G_GINT64_FORMAT "\n", value);
        break;
      }

     case DBUS_TYPE_UINT64:
      {
        guint64 value;
        dbus_message_iter_get_basic (iter, &value);
        g_string_append_printf (s, "uint64: %" G_GUINT64_FORMAT "\n", value);
        break;
      }

     case DBUS_TYPE_DOUBLE:
      {
        gdouble value;
        dbus_message_iter_get_basic (iter, &value);
        g_string_append_printf (s, "double: %f\n", value);
        break;
      }

     case DBUS_TYPE_STRING:
      {
        const gchar *value;
        dbus_message_iter_get_basic (iter, &value);
        g_string_append_printf (s, "string: `%s'\n", value);
        break;
      }

     case DBUS_TYPE_OBJECT_PATH:
      {
        const gchar *value;
        dbus_message_iter_get_basic (iter, &value);
        g_string_append_printf (s, "object_path: `%s'\n", value);
        break;
      }

     case DBUS_TYPE_SIGNATURE:
      {
        const gchar *value;
        dbus_message_iter_get_basic (iter, &value);
        g_string_append_printf (s, "signature: `%s'\n", value);
        break;
      }

     case DBUS_TYPE_VARIANT:
       g_string_append_printf (s, "variant:\n");
       dbus_message_iter_recurse (iter, &sub);
       while (dbus_message_iter_get_arg_type (&sub))
         {
           dbus_1_message_append (s, indent + 2, &sub);
           dbus_message_iter_next (&sub);
         }
       break;

     case DBUS_TYPE_ARRAY:
       g_string_append_printf (s, "array:\n");
       dbus_message_iter_recurse (iter, &sub);
       while (dbus_message_iter_get_arg_type (&sub))
         {
           dbus_1_message_append (s, indent + 2, &sub);
           dbus_message_iter_next (&sub);
         }
       break;

     case DBUS_TYPE_STRUCT:
       g_string_append_printf (s, "struct:\n");
       dbus_message_iter_recurse (iter, &sub);
       while (dbus_message_iter_get_arg_type (&sub))
         {
           dbus_1_message_append (s, indent + 2, &sub);
           dbus_message_iter_next (&sub);
         }
       break;

     case DBUS_TYPE_DICT_ENTRY:
       g_string_append_printf (s, "dict_entry:\n");
       dbus_message_iter_recurse (iter, &sub);
       while (dbus_message_iter_get_arg_type (&sub))
         {
           dbus_1_message_append (s, indent + 2, &sub);
           dbus_message_iter_next (&sub);
         }
       break;

     default:
       g_printerr ("Error serializing D-Bus message to GVariant. Unsupported arg type `%c' (%d)",
                   arg_type,
                   arg_type);
       g_assert_not_reached ();
       break;
    }
}

static gchar *
dbus_1_message_print (DBusMessage *message)
{
  GString *s;
  guint n;
  DBusMessageIter iter;

  s = g_string_new (NULL);
  n = 0;
  dbus_message_iter_init (message, &iter);
  while (dbus_message_iter_get_arg_type (&iter) != DBUS_TYPE_INVALID)
    {
      g_string_append_printf (s, "value %d: ", n);
      dbus_1_message_append (s, 2, &iter);
      dbus_message_iter_next (&iter);
      n++;
    }

  return g_string_free (s, FALSE);
}

/* ---------------------------------------------------------------------------------------------------- */

static gchar *
get_body_signature (GVariant *value)
{
  const gchar *s;
  gsize len;
  gchar *ret;

  s = g_variant_get_type_string (value);
  len = strlen (s);
  g_assert (len>=2);

  ret = g_strndup (s + 1, len - 2);

  return ret;
}

static void
check_serialization (GVariant *value,
                     const gchar *expected_dbus_1_output)
{
  guchar *blob;
  gsize blob_size;
  DBusMessage *dbus_1_message;
  GDBusMessage *message;
  GDBusMessage *recovered_message;
  GError *error;
  DBusError dbus_error;
  gchar *s;
  gchar *s1;

  message = g_dbus_message_new ();
  g_dbus_message_set_body (message, value);
  g_dbus_message_set_message_type (message, G_DBUS_MESSAGE_TYPE_METHOD_CALL);
  g_dbus_message_set_serial (message, 0x41);
  s = get_body_signature (value);
  g_dbus_message_set_header (message, G_DBUS_MESSAGE_HEADER_FIELD_PATH, g_variant_new_object_path ("/foo/bar"));
  g_dbus_message_set_header (message, G_DBUS_MESSAGE_HEADER_FIELD_MEMBER, g_variant_new_string ("Member"));
  g_dbus_message_set_header (message, G_DBUS_MESSAGE_HEADER_FIELD_SIGNATURE, g_variant_new_signature (s));
  g_free (s);

  /* First check that the serialization to the D-Bus wire format is correct */

  error = NULL;
  blob = g_dbus_message_to_blob (message,
                                 &blob_size,
                                 &error);
  g_assert_no_error (error);
  g_assert (blob != NULL);

  dbus_error_init (&dbus_error);
  dbus_1_message = dbus_message_demarshal ((char *) blob, blob_size, &dbus_error);
  if (dbus_error_is_set (&dbus_error))
    {
      g_printerr ("Error calling dbus_message_demarshal() on this blob: %s: %s\n",
                  dbus_error.name,
                  dbus_error.message);
      hexdump (blob, blob_size);
      dbus_error_free (&dbus_error);

      s = g_variant_print (value, TRUE);
      g_printerr ("\nThe blob was generated from the following GVariant value:\n%s\n\n", s);
      g_free (s);

      g_printerr ("If the blob was encoded using DBusMessageIter, the payload would have been:\n");
      print_gv_dbus_message (value);

      g_assert_not_reached ();
    }

  s = dbus_1_message_print (dbus_1_message);
  dbus_message_unref (dbus_1_message);

  g_assert_cmpstr (s, ==, expected_dbus_1_output);
  g_free (s);

  /* Then serialize back and check that the body is identical */

  error = NULL;
  recovered_message = g_dbus_message_new_from_blob (blob, blob_size, &error);
  g_assert_no_error (error);
  g_assert (recovered_message != NULL);
  g_assert (g_dbus_message_get_body (recovered_message) != NULL);

  if (!g_variant_equal (g_dbus_message_get_body (recovered_message), value))
    {
      s = g_variant_print (g_dbus_message_get_body (recovered_message), TRUE);
      s1 = g_variant_print (value, TRUE);
      g_printerr ("Recovered value:\n%s\ndoes not match given value\n%s\n",
                  s,
                  s1);
      g_free (s);
      g_free (s1);
      g_assert_not_reached ();
    }
  g_object_unref (message);
  g_object_unref (recovered_message);
}

static void
message_serialize_basic (void)
{
  check_serialization (g_variant_new ("(sogybnqiuxtd)",
                                      "this is a string",
                                      "/this/is/a/path",
                                      "sad",
                                      42,
                                      TRUE,
                                      -42,
                                      60000,
                                      -44,
                                      100000,
                                      -G_GINT64_CONSTANT(2)<<34,
                                      G_GUINT64_CONSTANT(0xffffffffffffffff),
                                      42.5),
                       "value 0:   string: `this is a string'\n"
                       "value 1:   object_path: `/this/is/a/path'\n"
                       "value 2:   signature: `sad'\n"
                       "value 3:   byte: 0x2a\n"
                       "value 4:   bool: true\n"
                       "value 5:   int16: -42\n"
                       "value 6:   uint16: 60000\n"
                       "value 7:   int32: -44\n"
                       "value 8:   uint32: 100000\n"
                       "value 9:   int64: -34359738368\n"
                       "value 10:   uint64: 18446744073709551615\n"
                       "value 11:   double: 42.500000\n");
}

/* ---------------------------------------------------------------------------------------------------- */

static void
message_serialize_complex (void)
{
  GError *error;
  GVariant *value;

  error = NULL;
  value = g_variant_parse (G_VARIANT_TYPE ("(aia{ss})"),
                           "([1, 2, 3], {'one': 'white', 'two': 'black'})",
                           NULL, NULL, &error);
  g_assert_no_error (error);
  g_assert (value != NULL);
  check_serialization (value,
                       "value 0:   array:\n"
                       "    int32: 1\n"
                       "    int32: 2\n"
                       "    int32: 3\n"
                       "value 1:   array:\n"
                       "    dict_entry:\n"
                       "      string: `one'\n"
                       "      string: `white'\n"
                       "    dict_entry:\n"
                       "      string: `two'\n"
                       "      string: `black'\n");
}


/* ---------------------------------------------------------------------------------------------------- */

int
main (int   argc,
      char *argv[])
{
  g_type_init ();
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/gdbus/message-serialize-basic", message_serialize_basic);
  g_test_add_func ("/gdbus/message-serialize-complex", message_serialize_complex);
  return g_test_run();
}

