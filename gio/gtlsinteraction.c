/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright (C) 2011 Collabora, Ltd.
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
 * Author: Stef Walter <stefw@collabora.co.uk>
 */

#include "config.h"

#include <string.h>

#include "gtlsinteraction.h"
#include "gtlspassword.h"
#include "gasyncresult.h"
#include "gcancellable.h"
#include "gsimpleasyncresult.h"
#include "gioenumtypes.h"
#include "glibintl.h"


/**
 * SECTION:gtlsinteraction
 * @short_description: Interaction with the user during TLS operations.
 * @include: gio/gio.h
 *
 * #GTlsInteraction provides a mechanism for the TLS connection and database
 * code to interact with the user. It can be used to ask the user for passwords.
 *
 * To use a #GTlsInteraction with a TLS connection use
 * g_tls_connection_set_interaction().
 *
 * Callers should instantiate a subclass of this that implements all the
 * various callbacks to show the required dialogs, such as
 * #GtkTlsInteraction. If no interaction is desired, usually %NULL can be
 * passed, see each method taking a #GTlsInteraction for details.
 */

/**
 * GTlsInteraction:
 *
 * An object representing interaction that the TLS connection and database
 * might have with the user.
 *
 * Since: 2.30
 */

/**
 * GTlsInteractionClass:
 *
 * The class for #GTlsInteraction.
 *
 * Since: 2.30
 */

G_DEFINE_TYPE (GTlsInteraction, g_tls_interaction, G_TYPE_OBJECT);

static GTlsInteractionResult
g_tls_interaction_default_ask_password (GTlsInteraction    *interaction,
                                        GTlsPassword       *password,
                                        GCancellable       *cancellable,
                                        GError            **error)
{
  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return G_TLS_INTERACTION_FAILED;
  else
    return G_TLS_INTERACTION_UNHANDLED;
}

static void
g_tls_interaction_default_ask_password_async (GTlsInteraction    *interaction,
                                              GTlsPassword       *password,
                                              GCancellable       *cancellable,
                                              GAsyncReadyCallback callback,
                                              gpointer            user_data)
{
  GSimpleAsyncResult *res;
  GError *error = NULL;

  res = g_simple_async_result_new (G_OBJECT (interaction), callback, user_data,
                                   g_tls_interaction_default_ask_password);
  if (g_cancellable_set_error_if_cancelled (cancellable, &error))
    g_simple_async_result_take_error (res, error);
  g_simple_async_result_complete_in_idle (res);
  g_object_unref (res);
}

static GTlsInteractionResult
g_tls_interaction_default_ask_password_finish (GTlsInteraction    *interaction,
                                               GAsyncResult       *result,
                                               GError            **error)
{
  g_return_val_if_fail (g_simple_async_result_is_valid (result, G_OBJECT (interaction),
                        g_tls_interaction_default_ask_password), G_TLS_INTERACTION_UNHANDLED);

  if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result), error))
    return G_TLS_INTERACTION_FAILED;

  return G_TLS_INTERACTION_UNHANDLED;
}

static void
g_tls_interaction_init (GTlsInteraction *interaction)
{
}

static void
g_tls_interaction_class_init (GTlsInteractionClass *klass)
{
  klass->ask_password = g_tls_interaction_default_ask_password;
  klass->ask_password_async = g_tls_interaction_default_ask_password_async;
  klass->ask_password_finish = g_tls_interaction_default_ask_password_finish;
}

/**
 * g_tls_interaction_ask_password:
 * @interaction: a #GTlsInteraction object
 * @password: a #GTlsPassword object
 * @cancellable: an optional #GCancellable cancellation object
 * @error: an optional location to place an error on failure
 *
 * This function is normally called by #GTlsConnection or #GTlsDatabase to
 * ask the user for a password.
 *
 * Derived subclasses usually implement a password prompt, although they may
 * also choose to provide a password from elsewhere. The @password value will
 * be filled in and then @callback will be called. Alternatively the user may
 * abort this password request, which will usually abort the TLS connection.
 *
 * If the interaction is cancelled by the cancellation object, or by the
 * user then %G_TLS_INTERACTION_ABORTED will be returned. Certain
 * implementations may not support immediate cancellation.
 *
 * Returns: The status of the ask password interaction.
 *
 * Since: 2.30
 */
GTlsInteractionResult
g_tls_interaction_ask_password (GTlsInteraction    *interaction,
                                GTlsPassword       *password,
                                GCancellable       *cancellable,
                                GError            **error)
{
  g_return_val_if_fail (G_IS_TLS_INTERACTION (interaction), G_TLS_INTERACTION_UNHANDLED);
  g_return_val_if_fail (G_IS_TLS_PASSWORD (password), G_TLS_INTERACTION_UNHANDLED);
  g_return_val_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable), G_TLS_INTERACTION_UNHANDLED);
  return G_TLS_INTERACTION_GET_CLASS (interaction)->ask_password (interaction,
                                                                  password,
                                                                  cancellable,
                                                                  error);
}

/**
 * g_tls_interaction_ask_password_async:
 * @interaction: a #GTlsInteraction object
 * @password: a #GTlsPassword object
 * @cancellable: an optional #GCancellable cancellation object
 * @callback: (allow-none): will be called when the interaction completes
 * @user_data: (allow-none): data to pass to the @callback
 *
 * This function is normally called by #GTlsConnection or #GTlsDatabase to
 * ask the user for a password.
 *
 * Derived subclasses usually implement a password prompt, although they may
 * also choose to provide a password from elsewhere. The @password value will
 * be filled in and then @callback will be called. Alternatively the user may
 * abort this password request, which will usually abort the TLS connection.
 *
 * The @callback will be invoked on thread-default main context of the thread
 * that called this function. The @callback should call
 * g_tls_interaction_ask_password_finish() to get the status of the user
 * interaction.
 *
 * Certain implementations may not support immediate cancellation.
 *
 * Since: 2.30
 */
void
g_tls_interaction_ask_password_async (GTlsInteraction    *interaction,
                                      GTlsPassword       *password,
                                      GCancellable       *cancellable,
                                      GAsyncReadyCallback callback,
                                      gpointer            user_data)
{
  g_return_if_fail (G_IS_TLS_INTERACTION (interaction));
  g_return_if_fail (G_IS_TLS_PASSWORD (password));
  g_return_if_fail (cancellable == NULL || G_IS_CANCELLABLE (cancellable));
  G_TLS_INTERACTION_GET_CLASS (interaction)->ask_password_async (interaction, password,
                                                                 cancellable,
                                                                 callback, user_data);
}

/**
 * g_tls_interaction_ask_password_finish:
 * @interaction: a #GTlsInteraction object
 * @result: the result passed to the callback
 * @error: an optional location to place an error on failure
 *
 * Complete an ask password user interaction request. This should be once
 * the g_tls_interaction_ask_password() completion callback is called.
 *
 * If %G_TLS_INTERACTION_HANDLED is returned, then the #GTlsPassword passed
 * to g_tls_interaction_ask_password() will have its password filled in.
 *
 * If the interaction is cancelled by the cancellation object, or by the
 * user then %G_TLS_INTERACTION_ABORTED will be returned.
 *
 * Returns: The status of the ask password interaction.
 *
 * Since: 2.30
 */
GTlsInteractionResult
g_tls_interaction_ask_password_finish (GTlsInteraction    *interaction,
                                       GAsyncResult       *result,
                                       GError            **error)
{
  g_return_val_if_fail (G_IS_TLS_INTERACTION (interaction), G_TLS_INTERACTION_UNHANDLED);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (result), G_TLS_INTERACTION_UNHANDLED);
  return G_TLS_INTERACTION_GET_CLASS (interaction)->ask_password_finish (interaction,
                                                                         result,
                                                                         error);
}
