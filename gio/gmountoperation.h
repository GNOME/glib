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

#ifndef __G_MOUNT_OPERATION_H__
#define __G_MOUNT_OPERATION_H__

#include <sys/stat.h>

#include <glib-object.h>

G_BEGIN_DECLS

#define G_TYPE_MOUNT_OPERATION         (g_mount_operation_get_type ())
#define G_MOUNT_OPERATION(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), G_TYPE_MOUNT_OPERATION, GMountOperation))
#define G_MOUNT_OPERATION_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), G_TYPE_MOUNT_OPERATION, GMountOperationClass))
#define G_IS_MOUNT_OPERATION(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), G_TYPE_MOUNT_OPERATION))
#define G_IS_MOUNT_OPERATION_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), G_TYPE_MOUNT_OPERATION))
#define G_MOUNT_OPERATION_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), G_TYPE_MOUNT_OPERATION, GMountOperationClass))

/**
 * GMountOperation:
 * 
 * Class for providing authentication methods for mounting operations, 
 * such as mounting a file locally, or authenticating with a server.
 **/
typedef struct _GMountOperation        GMountOperation;
typedef struct _GMountOperationClass   GMountOperationClass;
typedef struct _GMountOperationPrivate GMountOperationPrivate;

struct _GMountOperation
{
  GObject parent_instance;

  GMountOperationPrivate *priv;
};

/**
 * GPasswordFlags:
 * @G_PASSWORD_FLAGS_NEED_PASSWORD: operation requires a password.
 * @G_PASSWORD_FLAGS_NEED_USERNAME: operation requires a username.
 * @G_PASSWORD_FLAGS_NEED_DOMAIN: operation requires a domain.
 * @G_PASSWORD_FLAGS_SAVING_SUPPORTED: operation supports saving settings.
 * @G_PASSWORD_FLAGS_ANON_SUPPORTED: operation supports anonymous users.
 * 
 * #GPasswordFlags are used to request specific information from the 
 * user, or to notify the user of their choices in an authentication
 * situation. 
 * 
 **/ 
typedef enum {
  G_PASSWORD_FLAGS_NEED_PASSWORD    = 1<<0,
  G_PASSWORD_FLAGS_NEED_USERNAME    = 1<<1,
  G_PASSWORD_FLAGS_NEED_DOMAIN      = 1<<2,
  G_PASSWORD_FLAGS_SAVING_SUPPORTED = 1<<3,
  G_PASSWORD_FLAGS_ANON_SUPPORTED   = 1<<4
} GPasswordFlags;

/**
 * GPasswordSave:
 * @G_PASSWORD_SAVE_NEVER: never save a password.
 * @G_PASSWORD_SAVE_FOR_SESSION: save a password for the session.
 * @G_PASSWORD_SAVE_PERMANENTLY: save a password permanently.
 * 
 * #GPasswordSave is used to indicate the lifespan of a saved password.
 **/ 
typedef enum {
  G_PASSWORD_SAVE_NEVER,
  G_PASSWORD_SAVE_FOR_SESSION,
  G_PASSWORD_SAVE_PERMANENTLY
} GPasswordSave;

struct _GMountOperationClass
{
  GObjectClass parent_class;

  /* signals: */

  gboolean (* ask_password) (GMountOperation *op,
			     const char      *message,
			     const char      *default_user,
			     const char      *default_domain,
			     GPasswordFlags   flags);

  gboolean (* ask_question) (GMountOperation *op,
			     const char      *message,
			     const char      *choices[]);
  
  void     (* reply)        (GMountOperation *op,
			     gboolean         abort);
  
  /*< private >*/
  /* Padding for future expansion */
  void (*_g_reserved1) (void);
  void (*_g_reserved2) (void);
  void (*_g_reserved3) (void);
  void (*_g_reserved4) (void);
  void (*_g_reserved5) (void);
  void (*_g_reserved6) (void);
  void (*_g_reserved7) (void);
  void (*_g_reserved8) (void);
  void (*_g_reserved9) (void);
  void (*_g_reserved10) (void);
  void (*_g_reserved11) (void);
  void (*_g_reserved12) (void);
};

GType g_mount_operation_get_type (void) G_GNUC_CONST;
  
GMountOperation *  g_mount_operation_new (void);

const char *  g_mount_operation_get_username      (GMountOperation *op);
void          g_mount_operation_set_username      (GMountOperation *op,
						   const char      *username);
const char *  g_mount_operation_get_password      (GMountOperation *op);
void          g_mount_operation_set_password      (GMountOperation *op,
						   const char      *password);
gboolean      g_mount_operation_get_anonymous     (GMountOperation *op);
void          g_mount_operation_set_anonymous     (GMountOperation *op,
						   gboolean         anonymous);
const char *  g_mount_operation_get_domain        (GMountOperation *op);
void          g_mount_operation_set_domain        (GMountOperation *op,
						   const char      *domain);
GPasswordSave g_mount_operation_get_password_save (GMountOperation *op);
void          g_mount_operation_set_password_save (GMountOperation *op,
						   GPasswordSave    save);
int           g_mount_operation_get_choice        (GMountOperation *op);
void          g_mount_operation_set_choice        (GMountOperation *op,
						   int              choice);
void          g_mount_operation_reply             (GMountOperation *op,
						   gboolean         abort);

G_END_DECLS

#endif /* __G_MOUNT_OPERATION_H__ */
