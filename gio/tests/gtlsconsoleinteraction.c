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

#include <glib.h>
#include <string.h>

#ifdef G_OS_WIN32
#include <glib/gprintf.h>
#include <conio.h>
#endif

#include "gtlsconsoleinteraction.h"

/*
 * WARNING: This is not the example you're looking for [slow hand wave]. This
 * is not industrial strength, it's just for testing. It uses embarassing
 * functions like getpass() and does lazy things with threads.
 */

G_DEFINE_TYPE (GTlsConsoleInteraction, g_tls_console_interaction, G_TYPE_TLS_INTERACTION);

#ifdef G_OS_WIN32
/* win32 doesn't have getpass() */
static gchar *
getpass (const gchar *prompt)
{
  static gchar buf[BUFSIZ];
  gint i;

  g_printf ("%s", prompt);
  fflush (stdout);

  for (i = 0; i < BUFSIZ - 1; ++i)
    {
      buf[i] = _getch ();
      if (buf[i] == '\r')
        break;
    }
  buf[i] = '\0';

  g_printf ("\n");

  return &buf[0];
}
#endif

static GTlsInteractionResult
g_tls_console_interaction_ask_password (GTlsInteraction    *interaction,
                                        GTlsPassword       *password,
                                        GCancellable       *cancellable,
                                        GError            **error)
{
  const gchar *value;
  gchar *prompt;

  prompt = g_strdup_printf ("Password \"%s\"': ", g_tls_password_get_description (password));
  value = getpass (prompt);
  g_free (prompt);

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return G_TLS_INTERACTION_FAILED;

  g_tls_password_set_value (password, (guchar *)value, -1);
  return G_TLS_INTERACTION_HANDLED;
}

static void
ask_password_with_getpass (GSimpleAsyncResult    *res,
                           GObject               *object,
                           GCancellable          *cancellable)
{
  GTlsPassword *password;
  GError *error = NULL;

  password = g_simple_async_result_get_op_res_gpointer (res);
  g_tls_console_interaction_ask_password (G_TLS_INTERACTION (object), password,
                                          cancellable, &error);
  if (error != NULL)
    g_simple_async_result_take_error (res, error);
}

static void
g_tls_console_interaction_ask_password_async (GTlsInteraction    *interaction,
                                              GTlsPassword       *password,
                                              GCancellable       *cancellable,
                                              GAsyncReadyCallback callback,
                                              gpointer            user_data)
{
  GSimpleAsyncResult *res;

  res = g_simple_async_result_new (G_OBJECT (interaction), callback, user_data,
                                   g_tls_console_interaction_ask_password);
  g_simple_async_result_set_op_res_gpointer (res, g_object_ref (password), g_object_unref);
  g_simple_async_result_run_in_thread (res, ask_password_with_getpass,
                                       G_PRIORITY_DEFAULT, cancellable);
  g_object_unref (res);
}

static GTlsInteractionResult
g_tls_console_interaction_ask_password_finish (GTlsInteraction    *interaction,
                                               GAsyncResult       *result,
                                               GError            **error)
{
  g_return_val_if_fail (g_simple_async_result_is_valid (result, G_OBJECT (interaction),
                        g_tls_console_interaction_ask_password), G_TLS_INTERACTION_FAILED);

  if (g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result), error))
    return G_TLS_INTERACTION_FAILED;

  return G_TLS_INTERACTION_HANDLED;
}

static void
g_tls_console_interaction_init (GTlsConsoleInteraction *interaction)
{

}

static void
g_tls_console_interaction_class_init (GTlsConsoleInteractionClass *klass)
{
  GTlsInteractionClass *interaction_class = G_TLS_INTERACTION_CLASS (klass);
  interaction_class->ask_password = g_tls_console_interaction_ask_password;
  interaction_class->ask_password_async = g_tls_console_interaction_ask_password_async;
  interaction_class->ask_password_finish = g_tls_console_interaction_ask_password_finish;
}

GTlsInteraction *
g_tls_console_interaction_new (void)
{
  return g_object_new (G_TYPE_TLS_CONSOLE_INTERACTION, NULL);
}
