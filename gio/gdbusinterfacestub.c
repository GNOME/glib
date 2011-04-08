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

#include "gdbusinterface.h"
#include "gdbusinterfacestub.h"
#include "gdbusobjectstub.h"
#include "gio-marshal.h"
#include "gioenumtypes.h"
#include "gdbusprivate.h"
#include "gdbusmethodinvocation.h"
#include "gdbusconnection.h"
#include "gioscheduler.h"
#include "gioerror.h"

#include "glibintl.h"

/**
 * SECTION:gdbusinterfacestub
 * @short_description: Service-side D-Bus interface
 * @include: gio/gio.h
 *
 * Abstract base class for D-Bus interfaces on the service side.
 */

struct _GDBusInterfaceStubPrivate
{
  GDBusObject *object;
  GDBusInterfaceStubFlags flags;
  guint registration_id;

  GDBusConnection *connection;
  gchar *object_path;
  GDBusInterfaceVTable *hooked_vtable;
};

enum
{
  G_AUTHORIZE_METHOD_SIGNAL,
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_G_FLAGS
};

static guint signals[LAST_SIGNAL] = {0};

static void dbus_interface_interface_init (GDBusInterfaceIface *iface);

G_DEFINE_ABSTRACT_TYPE_WITH_CODE (GDBusInterfaceStub, g_dbus_interface_stub, G_TYPE_OBJECT,
                                  G_IMPLEMENT_INTERFACE (G_TYPE_DBUS_INTERFACE, dbus_interface_interface_init));

static void
g_dbus_interface_stub_finalize (GObject *object)
{
  GDBusInterfaceStub *stub = G_DBUS_INTERFACE_STUB (object);
  /* unexport if already exported */
  if (stub->priv->registration_id > 0)
    g_dbus_interface_stub_unexport (stub);

  g_assert (stub->priv->connection == NULL);
  g_assert (stub->priv->object_path == NULL);
  g_assert (stub->priv->hooked_vtable == NULL);

  if (stub->priv->object != NULL)
    g_object_remove_weak_pointer (G_OBJECT (stub->priv->object), (gpointer *) &stub->priv->object);
  G_OBJECT_CLASS (g_dbus_interface_stub_parent_class)->finalize (object);
}

static void
g_dbus_interface_stub_get_property (GObject      *object,
                                    guint         prop_id,
                                    GValue       *value,
                                    GParamSpec   *pspec)
{
  GDBusInterfaceStub *stub = G_DBUS_INTERFACE_STUB (object);

  switch (prop_id)
    {
    case PROP_G_FLAGS:
      g_value_set_flags (value, g_dbus_interface_stub_get_flags (stub));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
g_dbus_interface_stub_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  GDBusInterfaceStub *stub = G_DBUS_INTERFACE_STUB (object);

  switch (prop_id)
    {
    case PROP_G_FLAGS:
      g_dbus_interface_stub_set_flags (stub, g_value_get_flags (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static gboolean
g_dbus_interface_stub_g_authorize_method_default (GDBusInterfaceStub    *stub,
                                                  GDBusMethodInvocation *invocation)
{
  return TRUE;
}

static void
g_dbus_interface_stub_class_init (GDBusInterfaceStubClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize     = g_dbus_interface_stub_finalize;
  gobject_class->set_property = g_dbus_interface_stub_set_property;
  gobject_class->get_property = g_dbus_interface_stub_get_property;

  klass->g_authorize_method = g_dbus_interface_stub_g_authorize_method_default;

  /**
   * GDBusInterfaceStub:g-flags:
   *
   * Flags from the #GDBusInterfaceStubFlags enumeration.
   *
   * Since: 2.30
   */
  g_object_class_install_property (gobject_class,
                                   PROP_G_FLAGS,
                                   g_param_spec_flags ("g-flags",
                                                       "g-flags",
                                                       "Flags for the interface stub",
                                                       G_TYPE_DBUS_INTERFACE_STUB_FLAGS,
                                                       G_DBUS_INTERFACE_STUB_FLAGS_NONE,
                                                       G_PARAM_READABLE |
                                                       G_PARAM_WRITABLE |
                                                       G_PARAM_STATIC_STRINGS));

  /**
   * GDBusInterfaceStub::g-authorize-method:
   * @interface: The #GDBusInterfaceStub emitting the signal.
   * @invocation: A #GDBusMethodInvocation.
   *
   * Emitted when a method is invoked by a remote caller and used to
   * determine if the method call is authorized.
   *
   * Note that this signal is emitted in a thread dedicated to
   * handling the method call so handlers are allowed to perform
   * blocking IO. This means that it is appropriate to call
   * e.g. <ulink
   * url="http://hal.freedesktop.org/docs/polkit/PolkitAuthority.html#polkit-authority-check-authorization-sync">polkit_authority_check_authorization_sync()</ulink>
   * with the <ulink
   * url="http://hal.freedesktop.org/docs/polkit/PolkitAuthority.html#POLKIT-CHECK-AUTHORIZATION-FLAGS-ALLOW-USER-INTERACTION:CAPS">POLKIT_CHECK_AUTHORIZATION_FLAGS_ALLOW_USER_INTERACTION</ulink> flag set.
   *
   * If %FALSE is returned then no further handlers are run and the
   * signal handler must take ownership of @invocation and finish
   * handling the call (e.g. return an error via
   * g_dbus_method_invocation_return_error()).
   *
   * Otherwise, if %TRUE is returned, signal emission continues. If no
   * handlers return %FALSE, then the method is dispatched. If
   * @interface has an enclosing #GDBusObjectStub, then the
   * #GDBusObjectStub::authorize-method signal handlers run before the
   * handlers for this signal.
   *
   * The default class handler just returns %TRUE.
   *
   * Please note that the common case is optimized: if no signals
   * handlers are connected and the default class handler isn't
   * overridden (for both @interface and the enclosing
   * #GDBusObjectStub, if any) and #GDBusInterfaceStub:g-flags does
   * not have the
   * %G_DBUS_INTERFACE_STUB_FLAGS_HANDLE_METHOD_INVOCATIONS_IN_THREAD
   * flags set, no dedicated thread is ever used and the call will be
   * handled in the same thread as the object that @interface belongs
   * to was exported in.
   *
   * Returns: %TRUE if the call is authorized, %FALSE otherwise.
   *
   * Since: 2.30
   */
  signals[G_AUTHORIZE_METHOD_SIGNAL] =
    g_signal_new ("g-authorize-method",
                  G_TYPE_DBUS_INTERFACE_STUB,
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GDBusInterfaceStubClass, g_authorize_method),
                  _g_signal_accumulator_false_handled,
                  NULL,
                  _gio_marshal_BOOLEAN__OBJECT,
                  G_TYPE_BOOLEAN,
                  1,
                  G_TYPE_DBUS_METHOD_INVOCATION);

  g_type_class_add_private (klass, sizeof (GDBusInterfaceStubPrivate));
}

static void
g_dbus_interface_stub_init (GDBusInterfaceStub *stub)
{
  stub->priv = G_TYPE_INSTANCE_GET_PRIVATE (stub, G_TYPE_DBUS_INTERFACE_STUB, GDBusInterfaceStubPrivate);
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_dbus_interface_stub_get_flags:
 * @stub: A #GDBusInterfaceStub.
 *
 * Gets the #GDBusInterfaceStubFlags that describes what the behavior
 * of @stub
 *
 * Returns: One or more flags from the #GDBusInterfaceStubFlags enumeration.
 *
 * Since: 2.30
 */
GDBusInterfaceStubFlags
g_dbus_interface_stub_get_flags (GDBusInterfaceStub  *stub)
{
  g_return_val_if_fail (G_IS_DBUS_INTERFACE_STUB (stub), G_DBUS_INTERFACE_STUB_FLAGS_NONE);
  return stub->priv->flags;
}

/**
 * g_dbus_interface_stub_set_flags:
 * @stub: A #GDBusInterfaceStub.
 * @flags: Flags from the #GDBusInterfaceStubFlags enumeration.
 *
 * Sets flags describing what the behavior of @stub should be.
 *
 * Since: 2.30
 */
void
g_dbus_interface_stub_set_flags (GDBusInterfaceStub      *stub,
                                 GDBusInterfaceStubFlags  flags)
{
  g_return_if_fail (G_IS_DBUS_INTERFACE_STUB (stub));
  if (stub->priv->flags != flags)
    {
      stub->priv->flags = flags;
      g_object_notify (G_OBJECT (stub), "g-flags");
    }
}

/**
 * g_dbus_interface_stub_get_info:
 * @stub: A #GDBusInterfaceStub.
 *
 * Gets D-Bus introspection information for the D-Bus interface
 * implemented by @interface.
 *
 * Returns: (transfer none): A #GDBusInterfaceInfo (never %NULL). Do not free.
 *
 * Since: 2.30
 */
GDBusInterfaceInfo *
g_dbus_interface_stub_get_info (GDBusInterfaceStub *stub)
{
  GDBusInterfaceInfo *ret;
  g_return_val_if_fail (G_IS_DBUS_INTERFACE_STUB (stub), NULL);
  ret = G_DBUS_INTERFACE_STUB_GET_CLASS (stub)->get_info (stub);
  g_warn_if_fail (ret != NULL);
  return ret;
}

/**
 * g_dbus_interface_stub_get_vtable:
 * @stub: A #GDBusInterfaceStub.
 *
 * Gets the interface vtable for the D-Bus interface implemented by
 * @interface. The returned function pointers should expect @stub
 * itself to be passed as @user_data.
 *
 * Returns: A #GDBusInterfaceVTable (never %NULL).
 *
 * Since: 2.30
 */
GDBusInterfaceVTable *
g_dbus_interface_stub_get_vtable (GDBusInterfaceStub *stub)
{
  GDBusInterfaceVTable *ret;
  g_return_val_if_fail (G_IS_DBUS_INTERFACE_STUB (stub), NULL);
  ret = G_DBUS_INTERFACE_STUB_GET_CLASS (stub)->get_vtable (stub);
  g_warn_if_fail (ret != NULL);
  return ret;
}

/**
 * g_dbus_interface_stub_get_properties:
 * @stub: A #GDBusInterfaceStub.
 *
 * Gets all D-Bus properties for @stub.
 *
 * Returns: A new, floating, #GVariant. Free with g_variant_unref().
 *
 * Since: 2.30
 */
GVariant *
g_dbus_interface_stub_get_properties (GDBusInterfaceStub *stub)
{
  GVariant *ret;
  g_return_val_if_fail (G_IS_DBUS_INTERFACE_STUB (stub), NULL);
  ret = G_DBUS_INTERFACE_STUB_GET_CLASS (stub)->get_properties (stub);
  g_warn_if_fail (g_variant_is_floating (ret));
  return ret;
}

/**
 * g_dbus_interface_stub_flush:
 * @stub: A #GDBusInterfaceStub.
 *
 * If @stub has outstanding changes, request for these changes to be
 * emitted immediately.
 *
 * For example, an exported D-Bus interface may queue up property
 * changes and emit the
 * <literal>org.freedesktop.DBus.Properties::PropertiesChanged</literal>
 * signal later (e.g. in an idle handler). This technique is useful
 * for collapsing multiple property changes into one.
 *
 * Since: 2.30
 */
void
g_dbus_interface_stub_flush (GDBusInterfaceStub *stub)
{
  g_return_if_fail (G_IS_DBUS_INTERFACE_STUB (stub));
  G_DBUS_INTERFACE_STUB_GET_CLASS (stub)->flush (stub);
}

/* ---------------------------------------------------------------------------------------------------- */

static GDBusInterfaceInfo *
_g_dbus_interface_stub_get_info (GDBusInterface *interface)
{
  GDBusInterfaceStub *stub = G_DBUS_INTERFACE_STUB (interface);
  return g_dbus_interface_stub_get_info (stub);
}

static GDBusObject *
g_dbus_interface_stub_get_object (GDBusInterface *interface)
{
  GDBusInterfaceStub *stub = G_DBUS_INTERFACE_STUB (interface);
  return stub->priv->object;
}

static void
g_dbus_interface_stub_set_object (GDBusInterface *interface,
                                  GDBusObject    *object)
{
  GDBusInterfaceStub *stub = G_DBUS_INTERFACE_STUB (interface);
  if (stub->priv->object != NULL)
    g_object_remove_weak_pointer (G_OBJECT (stub->priv->object), (gpointer *) &stub->priv->object);
  stub->priv->object = object;
  if (object != NULL)
    g_object_add_weak_pointer (G_OBJECT (stub->priv->object), (gpointer *) &stub->priv->object);
}

static void
dbus_interface_interface_init (GDBusInterfaceIface *iface)
{
  iface->get_info = _g_dbus_interface_stub_get_info;
  iface->get_object  = g_dbus_interface_stub_get_object;
  iface->set_object  = g_dbus_interface_stub_set_object;
}

/* ---------------------------------------------------------------------------------------------------- */

typedef struct
{
  volatile gint ref_count;
  GDBusInterfaceStub           *stub;
  GDBusInterfaceMethodCallFunc  method_call_func;
  GDBusMethodInvocation        *invocation;
  GMainContext                 *context;
} DispatchData;

static void
dispatch_data_unref (DispatchData *data)
{
  if (g_atomic_int_dec_and_test (&data->ref_count))
    {
      if (data->context != NULL)
        g_main_context_unref (data->context);
      g_free (data);
    }
}

static DispatchData *
dispatch_data_ref (DispatchData *data)
{
  g_atomic_int_inc (&data->ref_count);
  return data;
}

static gboolean
dispatch_invoke_in_context_func (gpointer user_data)
{
  DispatchData *data = user_data;
  data->method_call_func (g_dbus_method_invocation_get_connection (data->invocation),
                          g_dbus_method_invocation_get_sender (data->invocation),
                          g_dbus_method_invocation_get_object_path (data->invocation),
                          g_dbus_method_invocation_get_interface_name (data->invocation),
                          g_dbus_method_invocation_get_method_name (data->invocation),
                          g_dbus_method_invocation_get_parameters (data->invocation),
                          data->invocation,
                          g_dbus_method_invocation_get_user_data (data->invocation));
  return FALSE;
}

static gboolean
dispatch_in_thread_func (GIOSchedulerJob *job,
                         GCancellable    *cancellable,
                         gpointer         user_data)
{
  DispatchData *data = user_data;
  gboolean authorized;

  /* first check on the enclosing object (if any), then the interface */
  authorized = TRUE;
  if (data->stub->priv->object != NULL)
    {
      g_signal_emit_by_name (data->stub->priv->object,
                             "authorize-method",
                             data->stub,
                             data->invocation,
                             &authorized);
    }
  if (authorized)
    {
      g_signal_emit (data->stub,
                     signals[G_AUTHORIZE_METHOD_SIGNAL],
                     0,
                     data->invocation,
                     &authorized);
    }

  if (authorized)
    {
      gboolean run_in_thread;
      run_in_thread = (data->stub->priv->flags & G_DBUS_INTERFACE_STUB_FLAGS_HANDLE_METHOD_INVOCATIONS_IN_THREAD);
      if (run_in_thread)
        {
          /* might as well just re-use the existing thread */
          data->method_call_func (g_dbus_method_invocation_get_connection (data->invocation),
                                  g_dbus_method_invocation_get_sender (data->invocation),
                                  g_dbus_method_invocation_get_object_path (data->invocation),
                                  g_dbus_method_invocation_get_interface_name (data->invocation),
                                  g_dbus_method_invocation_get_method_name (data->invocation),
                                  g_dbus_method_invocation_get_parameters (data->invocation),
                                  data->invocation,
                                  g_dbus_method_invocation_get_user_data (data->invocation));
        }
      else
        {
          /* bah, back to original context */
          g_main_context_invoke_full (data->context,
                                      G_PRIORITY_DEFAULT,
                                      dispatch_invoke_in_context_func,
                                      dispatch_data_ref (data),
                                      (GDestroyNotify) dispatch_data_unref);
        }
    }
  else
    {
      /* do nothing */
    }

  return FALSE;
}

static void
g_dbus_interface_method_dispatch_helper (GDBusInterfaceStub           *stub,
                                         GDBusInterfaceMethodCallFunc  method_call_func,
                                         GDBusMethodInvocation        *invocation)
{
  gboolean has_handlers;
  gboolean has_default_class_handler;
  gboolean emit_authorized_signal;
  gboolean run_in_thread;

  g_return_if_fail (G_IS_DBUS_INTERFACE_STUB (stub));
  g_return_if_fail (method_call_func != NULL);
  g_return_if_fail (G_IS_DBUS_METHOD_INVOCATION (invocation));

  /* optimization for the common case where
   *
   *  a) no handler is connected and class handler is not overridden (both interface and object); and
   *  b) method calls are not dispatched in a thread
   */
  has_handlers = g_signal_has_handler_pending (stub,
                                               signals[G_AUTHORIZE_METHOD_SIGNAL],
                                               0,
                                               TRUE);
  has_default_class_handler = (G_DBUS_INTERFACE_STUB_GET_CLASS (stub)->g_authorize_method ==
                               g_dbus_interface_stub_g_authorize_method_default);

  emit_authorized_signal = (has_handlers || !has_default_class_handler);
  if (!emit_authorized_signal)
    {
      if (stub->priv->object != NULL)
        emit_authorized_signal = _g_dbus_object_stub_has_authorize_method_handlers (G_DBUS_OBJECT_STUB (stub->priv->object));
    }

  run_in_thread = (stub->priv->flags & G_DBUS_INTERFACE_STUB_FLAGS_HANDLE_METHOD_INVOCATIONS_IN_THREAD);
  if (!emit_authorized_signal && !run_in_thread)
    {
      method_call_func (g_dbus_method_invocation_get_connection (invocation),
                        g_dbus_method_invocation_get_sender (invocation),
                        g_dbus_method_invocation_get_object_path (invocation),
                        g_dbus_method_invocation_get_interface_name (invocation),
                        g_dbus_method_invocation_get_method_name (invocation),
                        g_dbus_method_invocation_get_parameters (invocation),
                        invocation,
                        g_dbus_method_invocation_get_user_data (invocation));
    }
  else
    {
      DispatchData *data;
      data = g_new0 (DispatchData, 1);
      data->stub = stub;
      data->method_call_func = method_call_func;
      data->invocation = invocation;
      data->context = g_main_context_get_thread_default ();
      data->ref_count = 1;
      if (data->context != NULL)
        g_main_context_ref (data->context);
      g_io_scheduler_push_job (dispatch_in_thread_func,
                               data,
                               (GDestroyNotify) dispatch_data_unref,
                               G_PRIORITY_DEFAULT,
                               NULL); /* GCancellable* */
    }
}

static void
stub_intercept_handle_method_call(GDBusConnection *connection,
                                  const gchar *sender,
                                  const gchar *object_path,
                                  const gchar *interface_name,
                                  const gchar *method_name,
                                  GVariant *parameters,
                                  GDBusMethodInvocation *invocation,
                                  gpointer user_data)
{
  GDBusInterfaceStub *stub = G_DBUS_INTERFACE_STUB (user_data);
  g_dbus_interface_method_dispatch_helper (stub,
                                           g_dbus_interface_stub_get_vtable (stub)->method_call,
                                           invocation);
}

/* ---------------------------------------------------------------------------------------------------- */

/**
 * g_dbus_interface_stub_get_connection:
 * @stub: A #GDBusInterfaceStub.
 *
 * Gets the connection that @stub is exported on, if any.
 *
 * Returns: (transfer none): A #GDBusConnection or %NULL if @stub is
 * not exported anywhere. Do not free, the object belongs to @stub.
 *
 * Since: 2.30
 */
GDBusConnection *
g_dbus_interface_stub_get_connection (GDBusInterfaceStub *stub)
{
  g_return_val_if_fail (G_IS_DBUS_INTERFACE_STUB (stub), NULL);
  return stub->priv->connection;
}

/**
 * g_dbus_interface_stub_get_object_path:
 * @stub: A #GDBusInterfaceStub.
 *
 * Gets the object that that @stub is exported on, if any.
 *
 * Returns: A string owned by @stub or %NULL if stub is not exported
 * anywhere. Do not free, the string belongs to @stub.
 *
 * Since: 2.30
 */
const gchar *
g_dbus_interface_stub_get_object_path (GDBusInterfaceStub *stub)
{
  g_return_val_if_fail (G_IS_DBUS_INTERFACE_STUB (stub), NULL);
  return stub->priv->object_path;
}

/**
 * g_dbus_interface_stub_export:
 * @stub: The D-Bus interface to export.
 * @connection: A #GDBusConnection to export @stub on.
 * @object_path: The path to export the interface at.
 * @error: Return location for error or %NULL.
 *
 * Exports @stubs at @object_path on @connection.
 *
 * Use g_dbus_interface_stub_unexport() to unexport the object.
 *
 * Returns: %TRUE if the interface was exported, other %FALSE with
 * @error set.
 *
 * Since: 2.30
 */
gboolean
g_dbus_interface_stub_export (GDBusInterfaceStub  *stub,
                              GDBusConnection     *connection,
                              const gchar         *object_path,
                              GError             **error)
{
  gboolean ret;

  g_return_val_if_fail (G_IS_DBUS_INTERFACE_STUB (stub), 0);
  g_return_val_if_fail (G_IS_DBUS_CONNECTION (connection), 0);
  g_return_val_if_fail (g_variant_is_object_path (object_path), 0);
  g_return_val_if_fail (error == NULL || *error == NULL, 0);

  ret = FALSE;
  if (stub->priv->registration_id > 0)
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_FAILED, /* TODO: new error code */
                           "The object is already exported");
      goto out;
    }

  g_assert (stub->priv->connection == NULL);
  g_assert (stub->priv->object_path == NULL);
  g_assert (stub->priv->hooked_vtable == NULL);

  /* Hook the vtable since we need to intercept method calls for
   * ::g-authorize-method and for dispatching in thread vs
   * context
   */
  stub->priv->hooked_vtable = g_memdup (g_dbus_interface_stub_get_vtable (stub), sizeof (GDBusInterfaceVTable));
  stub->priv->hooked_vtable->method_call = stub_intercept_handle_method_call;

  stub->priv->connection = g_object_ref (connection);
  stub->priv->object_path = g_strdup (object_path);
  stub->priv->registration_id = g_dbus_connection_register_object (connection,
                                                                   object_path,
                                                                   g_dbus_interface_stub_get_info (stub),
                                                                   stub->priv->hooked_vtable,
                                                                   stub,
                                                                   NULL, /* user_data_free_func */
                                                                   error);
  if (stub->priv->registration_id == 0)
    goto out;

  ret = TRUE;

 out:
  return ret;
}

/**
 * g_dbus_interface_stub_unexport:
 * @stub: A #GDBusInterfaceStub.
 *
 * Stops exporting an interface previously exported with
 * g_dbus_interface_stub_export().
 *
 * Since: 2.30
 */
void
g_dbus_interface_stub_unexport (GDBusInterfaceStub *stub)
{
  g_return_if_fail (G_IS_DBUS_INTERFACE_STUB (stub));
  g_return_if_fail (stub->priv->registration_id > 0);

  g_assert (stub->priv->connection != NULL);
  g_assert (stub->priv->object_path != NULL);
  g_assert (stub->priv->hooked_vtable != NULL);

  g_warn_if_fail (g_dbus_connection_unregister_object (stub->priv->connection,
                                                       stub->priv->registration_id));

  g_object_unref (stub->priv->connection);
  g_free (stub->priv->object_path);
  stub->priv->connection = NULL;
  stub->priv->object_path = NULL;
  stub->priv->hooked_vtable = NULL;
  stub->priv->registration_id = 0;
}

/* ---------------------------------------------------------------------------------------------------- */
