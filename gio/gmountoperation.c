/* GIO - GLib Input, Output and Streaming Library
 * 
 * Copyright (C) 2006-2007 Red Hat, Inc.
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
 * Author: Alexander Larsson <alexl@redhat.com>
 */

#include <config.h>

#include <string.h>

#include "gmountoperation.h"
#include "gio-marshal.h"
#include "glibintl.h"

G_DEFINE_TYPE (GMountOperation, g_mount_operation, G_TYPE_OBJECT);

enum {
  ASK_PASSWORD,
  ASK_QUESTION,
  REPLY,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

struct _GMountOperationPrivate {
  char *password;
  char *user;
  char *domain;
  gboolean anonymous;
  GPasswordSave password_save;
  int choice;
};

static void
g_mount_operation_finalize (GObject *object)
{
  GMountOperation *operation;
  GMountOperationPrivate *priv;

  operation = G_MOUNT_OPERATION (object);

  priv = operation->priv;
  
  g_free (priv->password);
  g_free (priv->user);
  g_free (priv->domain);
  
  if (G_OBJECT_CLASS (g_mount_operation_parent_class)->finalize)
    (*G_OBJECT_CLASS (g_mount_operation_parent_class)->finalize) (object);
}

static gboolean
boolean_handled_accumulator (GSignalInvocationHint *ihint,
			     GValue                *return_accu,
			     const GValue          *handler_return,
			     gpointer               dummy)
{
  gboolean continue_emission;
  gboolean signal_handled;
  
  signal_handled = g_value_get_boolean (handler_return);
  g_value_set_boolean (return_accu, signal_handled);
  continue_emission = !signal_handled;
  
  return continue_emission;
}

static gboolean
ask_password (GMountOperation *op,
	      const char      *message,
	      const char      *default_user,
	      const char      *default_domain,
	      GPasswordFlags   flags)
{
  return FALSE;
}
  
static gboolean
ask_question (GMountOperation *op,
	      const char      *message,
	      const char      *choices[])
{
  return FALSE;
}

static void
g_mount_operation_class_init (GMountOperationClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  
  g_type_class_add_private (klass, sizeof (GMountOperationPrivate));
  
  gobject_class->finalize = g_mount_operation_finalize;
  
  klass->ask_password = ask_password;
  klass->ask_question = ask_question;
  
  signals[ASK_PASSWORD] =
    g_signal_new (I_("ask_password"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GMountOperationClass, ask_password),
		  boolean_handled_accumulator, NULL,
		  _gio_marshal_BOOLEAN__STRING_STRING_STRING_INT,
		  G_TYPE_BOOLEAN, 4,
		  G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_UINT);
  
  signals[ASK_QUESTION] =
    g_signal_new (I_("ask_question"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GMountOperationClass, ask_question),
		  boolean_handled_accumulator, NULL,
		  _gio_marshal_BOOLEAN__STRING_POINTER,
		  G_TYPE_BOOLEAN, 2,
		  G_TYPE_STRING, G_TYPE_POINTER);

  signals[REPLY] =
    g_signal_new (I_("reply"),
		  G_TYPE_FROM_CLASS (gobject_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (GMountOperationClass, reply),
		  NULL, NULL,
		  g_cclosure_marshal_VOID__BOOLEAN,
		  G_TYPE_NONE, 1,
		  G_TYPE_BOOLEAN);
}

static void
g_mount_operation_init (GMountOperation *operation)
{
  operation->priv = G_TYPE_INSTANCE_GET_PRIVATE (operation,
						 G_TYPE_MOUNT_OPERATION,
						 GMountOperationPrivate);
}

/**
 * g_mount_operation_new:
 * 
 * Returns: a new #GMountOperation.
 **/
GMountOperation *
g_mount_operation_new (void)
{
  return g_object_new (G_TYPE_MOUNT_OPERATION, NULL);
}

/**
 * g_mount_operation_get_username
 * @op:
 * 
 * Returns: 
 **/
const char *
g_mount_operation_get_username (GMountOperation *op)
{
  g_return_val_if_fail (G_IS_MOUNT_OPERATION (op), NULL);
  return op->priv->user;
}

/**
 * g_mount_operation_set_username:
 * @op:
 * @username: input username.
 * 
 **/
void
g_mount_operation_set_username (GMountOperation *op,
				const char      *username)
{
  g_return_if_fail (G_IS_MOUNT_OPERATION (op));
  g_free (op->priv->user);
  op->priv->user = g_strdup (username);
}

/**
 * g_mount_operation_get_password:
 * @op:
 * 
 * Returns:  
 **/
const char *
g_mount_operation_get_password (GMountOperation *op)
{
  g_return_val_if_fail (G_IS_MOUNT_OPERATION (op), NULL);
  return op->priv->password;
}

/**
 * g_mount_operation_set_password:
 * @op: the given #GMountOperation.
 * @password: password to set.
 * 
 * Sets the mount operation's password to @password.  
 *
 **/
void
g_mount_operation_set_password (GMountOperation *op,
				const char      *password)
{
  g_return_if_fail (G_IS_MOUNT_OPERATION (op));
  g_free (op->priv->password);
  op->priv->password = g_strdup (password);
}

/**
 * g_mount_operation_get_anonymous:
 * @op:
 * 
 * Returns: %TRUE if mount operation is anonymous. 
 **/
gboolean
g_mount_operation_get_anonymous (GMountOperation *op)
{
  g_return_val_if_fail (G_IS_MOUNT_OPERATION (op), FALSE);
  return op->priv->anonymous;
}

/**
 * g_mount_operation_set_anonymous:
 * @op: the given #GMountOperation.
 * @anonymous: boolean value.
 * 
 **/  
void
g_mount_operation_set_anonymous (GMountOperation *op,
				 gboolean         anonymous)
{
  g_return_if_fail (G_IS_MOUNT_OPERATION (op));
  op->priv->anonymous = anonymous;
}

/**
 * g_mount_operation_get_domain:
 * @op:
 * 
 * Returns: a const string set to the domain.
 **/
const char *
g_mount_operation_get_domain (GMountOperation *op)
{
  g_return_val_if_fail (G_IS_MOUNT_OPERATION (op), NULL);
  return op->priv->domain;
}

/**
 * g_mount_operation_set_domain:
 * @op: the given #GMountOperation.
 * @domain: the domain to set.
 * 
 * Sets the mount operation's domain. 
 **/  
void
g_mount_operation_set_domain (GMountOperation *op,
			      const char      *domain)
{
  g_return_if_fail (G_IS_MOUNT_OPERATION (op));
  g_free (op->priv->domain);
  op->priv->domain = g_strdup (domain);
}

/**
 * g_mount_operation_get_password_save:
 * @op: the given #GMountOperation.
 *
 * Returns: #GPasswordSave. 
 **/  

GPasswordSave
g_mount_operation_get_password_save (GMountOperation *op)
{
  g_return_val_if_fail (G_IS_MOUNT_OPERATION (op), G_PASSWORD_SAVE_NEVER);
  return op->priv->password_save;
}

/**
 * g_mount_operation_set_password_save
 * @op:
 * @save: #GPasswordSave
 * 
 **/   
void
g_mount_operation_set_password_save (GMountOperation *op,
				     GPasswordSave    save)
{
  g_return_if_fail (G_IS_MOUNT_OPERATION (op));
  op->priv->password_save = save;
}

/**
 * g_mount_operation_get_choice:
 * @op:
 * 
 * Returns: 
 **/
int
g_mount_operation_get_choice (GMountOperation *op)
{
  g_return_val_if_fail (G_IS_MOUNT_OPERATION (op), 0);
  return op->priv->choice;
}

/**
 * g_mount_operation_set_choice:
 * @op:
 * @choice:
 *  
 **/
void
g_mount_operation_set_choice (GMountOperation *op,
			      int choice)
{
  g_return_if_fail (G_IS_MOUNT_OPERATION (op));
  op->priv->choice = choice;
}

/**
 * g_mount_operation_reply:
 * @op: #GMountOperation.
 * @abort: boolean.
 * 
 * Emits the #GMountOperation::Reply signal with the abort flag set to
 * @abort.
 **/
void
g_mount_operation_reply (GMountOperation *op,
			 gboolean         abort)
{
  g_return_if_fail (G_IS_MOUNT_OPERATION (op));
  g_signal_emit (op, signals[REPLY], 0, abort);
}
