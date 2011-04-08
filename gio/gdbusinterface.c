/* GDBus - GLib D-Bus Library
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

#include "config.h"

#include "gdbusobject.h"
#include "gdbusinterface.h"
#include "gio-marshal.h"

#include "glibintl.h"

/**
 * SECTION:gdbusinterface
 * @short_description: Base type for D-Bus interfaces
 * @include: gio/gio.h
 *
 * The #GDBusInterface type is the base type for D-Bus interfaces both
 * on the service side (see #GDBusInterfaceStub) and client side (see
 * #GDBusProxy).
 */

typedef GDBusInterfaceIface GDBusInterfaceInterface;
G_DEFINE_INTERFACE (GDBusInterface, g_dbus_interface, G_TYPE_OBJECT)

static void
g_dbus_interface_default_init (GDBusInterfaceIface *iface)
{
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_dbus_interface_get_info:
 * @interface: An exported D-Bus interface.
 *
 * Gets D-Bus introspection information for the D-Bus interface
 * implemented by @interface.
 *
 * Returns: (transfer none): A #GDBusInterfaceInfo. Do not free.
 */
GDBusInterfaceInfo *
g_dbus_interface_get_info (GDBusInterface *interface)
{
  g_return_val_if_fail (G_IS_DBUS_INTERFACE (interface), NULL);
  return G_DBUS_INTERFACE_GET_IFACE (interface)->get_info (interface);
}

/**
 * g_dbus_interface_get_object:
 * @interface: An exported D-Bus interface.
 *
 * Gets the #GDBusObject that @interface belongs to, if any.
 *
 * Returns: (transfer none): A #GDBusObject or %NULL. The returned
 * reference belongs to @interface and should not be freed.
 */
GDBusObject *
g_dbus_interface_get_object (GDBusInterface *interface)
{
  g_return_val_if_fail (G_IS_DBUS_INTERFACE (interface), NULL);
  return G_DBUS_INTERFACE_GET_IFACE (interface)->get_object (interface);
}

/**
 * g_dbus_interface_set_object:
 * @interface: An exported D-Bus interface.
 * @object: A #GDBusObject or %NULL.
 *
 * Sets the #GDBusObject for @interface to @object.
 *
 * Note that @interface will hold a weak reference to @object.
 */
void
g_dbus_interface_set_object (GDBusInterface    *interface,
                             GDBusObject       *object)
{
  g_return_if_fail (G_IS_DBUS_INTERFACE (interface));
  g_return_if_fail (object == NULL || G_IS_DBUS_OBJECT (object));
  G_DBUS_INTERFACE_GET_IFACE (interface)->set_object (interface, object);
}

/* Keep it here for now. TODO: move */

#include <string.h>

/**
 * g_dbus_gvariant_to_gvalue:
 * @value: A #GVariant.
 * @out_gvalue: Return location for the #GValue.
 *
 * Convert a #GVariant to a #GValue. If @value is floating, it is consumed.
 *
 * Note that the passed @out_gvalue does not have to have a #GType set.
 *
 * Returns: %TRUE if the conversion succeeded, %FALSE otherwise.
 */
gboolean
g_dbus_gvariant_to_gvalue (GVariant             *value,
                           GValue               *out_gvalue)
{
  gboolean ret;
  const GVariantType *type;
  gchar **array;

  g_return_val_if_fail (value != NULL, FALSE);
  g_return_val_if_fail (out_gvalue != NULL, FALSE);

  ret = FALSE;

  memset (out_gvalue, '\0', sizeof (GValue));

  switch (g_variant_classify (value))
    {
    case G_VARIANT_CLASS_BOOLEAN:
      g_value_init (out_gvalue, G_TYPE_BOOLEAN);
      g_value_set_boolean (out_gvalue, g_variant_get_boolean (value));
      break;

    case G_VARIANT_CLASS_BYTE:
      g_value_init (out_gvalue, G_TYPE_UCHAR);
      g_value_set_uchar (out_gvalue, g_variant_get_byte (value));
      break;

    case G_VARIANT_CLASS_INT16:
      g_value_init (out_gvalue, G_TYPE_INT);
      g_value_set_int (out_gvalue, g_variant_get_int16 (value));
      break;

    case G_VARIANT_CLASS_UINT16:
      g_value_init (out_gvalue, G_TYPE_UINT);
      g_value_set_uint (out_gvalue, g_variant_get_uint16 (value));
      break;

    case G_VARIANT_CLASS_INT32:
      g_value_init (out_gvalue, G_TYPE_INT);
      g_value_set_int (out_gvalue, g_variant_get_int32 (value));
      break;

    case G_VARIANT_CLASS_UINT32:
      g_value_init (out_gvalue, G_TYPE_UINT);
      g_value_set_uint (out_gvalue, g_variant_get_uint32 (value));
      break;

    case G_VARIANT_CLASS_INT64:
      g_value_init (out_gvalue, G_TYPE_INT64);
      g_value_set_int64 (out_gvalue, g_variant_get_int64 (value));
      break;

    case G_VARIANT_CLASS_UINT64:
      g_value_init (out_gvalue, G_TYPE_UINT64);
      g_value_set_uint64 (out_gvalue, g_variant_get_uint64 (value));
      break;

    case G_VARIANT_CLASS_HANDLE:
      g_value_init (out_gvalue, G_TYPE_INT);
      g_value_set_int (out_gvalue, g_variant_get_int32 (value));
      break;

    case G_VARIANT_CLASS_DOUBLE:
      g_value_init (out_gvalue, G_TYPE_DOUBLE);
      g_value_set_double (out_gvalue, g_variant_get_double (value));
      break;

    case G_VARIANT_CLASS_STRING:
      g_value_init (out_gvalue, G_TYPE_STRING);
      g_value_set_string (out_gvalue, g_variant_get_string (value, NULL));
      break;

    case G_VARIANT_CLASS_OBJECT_PATH:
      g_value_init (out_gvalue, G_TYPE_STRING);
      g_value_set_string (out_gvalue, g_variant_get_string (value, NULL));
      break;

    case G_VARIANT_CLASS_SIGNATURE:
      g_value_init (out_gvalue, G_TYPE_STRING);
      g_value_set_string (out_gvalue, g_variant_get_string (value, NULL));
      break;

    case G_VARIANT_CLASS_ARRAY:
      type = g_variant_get_type (value);
      switch (g_variant_type_peek_string (type)[1])
        {
        case G_VARIANT_CLASS_BYTE:
          g_value_init (out_gvalue, G_TYPE_STRING);
          g_value_set_string (out_gvalue, g_variant_get_bytestring (value));
          break;

        case G_VARIANT_CLASS_STRING:
          g_value_init (out_gvalue, G_TYPE_STRV);
          array = g_variant_dup_strv (value, NULL);
          g_value_take_boxed (out_gvalue, array);
          break;

        case G_VARIANT_CLASS_ARRAY:
          switch (g_variant_type_peek_string (type)[2])
            {
            case G_VARIANT_CLASS_BYTE:
              g_value_init (out_gvalue, G_TYPE_STRV);
              array = g_variant_dup_bytestring_array (value, NULL);
              g_value_take_boxed (out_gvalue, array);
              break;

            default:
              g_value_init (out_gvalue, G_TYPE_VARIANT);
              g_value_set_variant (out_gvalue, value);
              break;
            }
          break;

        default:
          g_value_init (out_gvalue, G_TYPE_VARIANT);
          g_value_set_variant (out_gvalue, value);
          break;
        }
      break;

    case G_VARIANT_CLASS_VARIANT:
    case G_VARIANT_CLASS_MAYBE:
    case G_VARIANT_CLASS_TUPLE:
    case G_VARIANT_CLASS_DICT_ENTRY:
      g_value_init (out_gvalue, G_TYPE_VARIANT);
      g_value_set_variant (out_gvalue, value);
      break;
    }

  ret = TRUE;

  return ret;
}


/**
 * g_dbus_gvalue_to_gvariant:
 * @gvalue: A #GValue to convert to a #GVariant.
 * @expected_type: The #GVariantType to create.
 *
 * Convert a #GValue to #GVariant.
 *
 * Returns: A #GVariant (never floating) holding the data from @gvalue
 * or %NULL in case of error. Free with g_variant_unref().
 */
GVariant *
g_dbus_gvalue_to_gvariant (const GValue         *gvalue,
                           const GVariantType   *expected_type)
{
  GVariant *ret;
  const gchar *s;
  const gchar * const *as;
  const gchar *empty_strv[1] = {NULL};

  g_return_val_if_fail (gvalue != NULL, NULL);
  g_return_val_if_fail (expected_type != NULL, NULL);

  ret = NULL;

  /* The expected type could easily be e.g. "s" with the GValue holding a string.
   * because of the UseGVariant annotation
   */
  if (G_VALUE_TYPE (gvalue) == G_TYPE_VARIANT)
    {
      ret = g_value_dup_variant (gvalue);
    }
  else
    {
      switch (g_variant_type_peek_string (expected_type)[0])
        {
        case G_VARIANT_CLASS_BOOLEAN:
          ret = g_variant_ref_sink (g_variant_new_boolean (g_value_get_boolean (gvalue)));
          break;

        case G_VARIANT_CLASS_BYTE:
          ret = g_variant_ref_sink (g_variant_new_byte (g_value_get_uchar (gvalue)));
          break;

        case G_VARIANT_CLASS_INT16:
          ret = g_variant_ref_sink (g_variant_new_int16 (g_value_get_int (gvalue)));
          break;

        case G_VARIANT_CLASS_UINT16:
          ret = g_variant_ref_sink (g_variant_new_uint16 (g_value_get_uint (gvalue)));
          break;

        case G_VARIANT_CLASS_INT32:
          ret = g_variant_ref_sink (g_variant_new_int32 (g_value_get_int (gvalue)));
          break;

        case G_VARIANT_CLASS_UINT32:
          ret = g_variant_ref_sink (g_variant_new_uint32 (g_value_get_uint (gvalue)));
          break;

        case G_VARIANT_CLASS_INT64:
          ret = g_variant_ref_sink (g_variant_new_int64 (g_value_get_int64 (gvalue)));
          break;

        case G_VARIANT_CLASS_UINT64:
          ret = g_variant_ref_sink (g_variant_new_uint64 (g_value_get_uint64 (gvalue)));
          break;

        case G_VARIANT_CLASS_HANDLE:
          ret = g_variant_ref_sink (g_variant_new_handle (g_value_get_int (gvalue)));
          break;

        case G_VARIANT_CLASS_DOUBLE:
          ret = g_variant_ref_sink (g_variant_new_double (g_value_get_double (gvalue)));
          break;

        case G_VARIANT_CLASS_STRING:
          s = g_value_get_string (gvalue);
          if (s == NULL)
            s = "";
          ret = g_variant_ref_sink (g_variant_new_string (s));
          break;

        case G_VARIANT_CLASS_OBJECT_PATH:
          s = g_value_get_string (gvalue);
          if (s == NULL)
            s = "/";
          ret = g_variant_ref_sink (g_variant_new_object_path (s));
          break;

        case G_VARIANT_CLASS_SIGNATURE:
          s = g_value_get_string (gvalue);
          if (s == NULL)
            s = "";
          ret = g_variant_ref_sink (g_variant_new_signature (s));
          break;

        case G_VARIANT_CLASS_ARRAY:
          switch (g_variant_type_peek_string (expected_type)[1])
            {
            case G_VARIANT_CLASS_BYTE:
              s = g_value_get_string (gvalue);
              if (s == NULL)
                s = "";
              ret = g_variant_ref_sink (g_variant_new_bytestring (s));
              break;

            case G_VARIANT_CLASS_STRING:
              as = g_value_get_boxed (gvalue);
              if (as == NULL)
                as = empty_strv;
              ret = g_variant_ref_sink (g_variant_new_strv (as, -1));
              break;

            case G_VARIANT_CLASS_ARRAY:
              switch (g_variant_type_peek_string (expected_type)[2])
                {
                case G_VARIANT_CLASS_BYTE:
                  as = g_value_get_boxed (gvalue);
                  if (as == NULL)
                    as = empty_strv;
                  ret = g_variant_ref_sink (g_variant_new_bytestring_array (as, -1));
                  break;

                default:
                  ret = g_value_dup_variant (gvalue);
                  break;
                }
              break;

            default:
              ret = g_value_dup_variant (gvalue);
              break;
            }
          break;

        default:
        case G_VARIANT_CLASS_VARIANT:
        case G_VARIANT_CLASS_MAYBE:
        case G_VARIANT_CLASS_TUPLE:
        case G_VARIANT_CLASS_DICT_ENTRY:
          ret = g_value_dup_variant (gvalue);
          break;
        }
    }

  /* Could be that the GValue is holding a NULL GVariant - in that case,
   * we return an "empty" GVariant instead of a NULL GVariant
   */
  if (ret == NULL)
    {
      GVariant *untrusted_empty;
      untrusted_empty = g_variant_new_from_data (expected_type, NULL, 0, FALSE, NULL, NULL);
      ret = g_variant_ref_sink (g_variant_get_normal_form (untrusted_empty));
      g_variant_unref (untrusted_empty);
    }

  g_assert (!g_variant_is_floating (ret));

  return ret;
}
