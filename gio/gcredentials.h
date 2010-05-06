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

#ifndef __G_CREDENTIALS_H__
#define __G_CREDENTIALS_H__

#include <gio/giotypes.h>

G_BEGIN_DECLS

#define G_TYPE_CREDENTIALS         (g_credentials_get_type ())
#define G_CREDENTIALS(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), G_TYPE_CREDENTIALS, GCredentials))
#define G_CREDENTIALS_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), G_TYPE_CREDENTIALS, GCredentialsClass))
#define G_CREDENTIALS_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), G_TYPE_CREDENTIALS, GCredentialsClass))
#define G_IS_CREDENTIALS(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), G_TYPE_CREDENTIALS))
#define G_IS_CREDENTIALS_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), G_TYPE_CREDENTIALS))

typedef struct _GCredentialsClass   GCredentialsClass;
typedef struct _GCredentialsPrivate GCredentialsPrivate;

/**
 * GCredentials:
 *
 * The #GCredentials structure contains only private data and
 * should only be accessed using the provided API.
 */
struct _GCredentials
{
  /*< private >*/
  GObject parent_instance;
  GCredentialsPrivate *priv;
};

/**
 * GCredentialsClass:
 *
 * Class structure for #GCredentials.
 */
struct _GCredentialsClass
{
  /*< private >*/
  GObjectClass parent_class;

  /* Padding for future expansion */
  void (*_g_reserved1) (void);
  void (*_g_reserved2) (void);
  void (*_g_reserved3) (void);
  void (*_g_reserved4) (void);
  void (*_g_reserved5) (void);
  void (*_g_reserved6) (void);
  void (*_g_reserved7) (void);
  void (*_g_reserved8) (void);
};

GType            g_credentials_get_type           (void) G_GNUC_CONST;

GCredentials    *g_credentials_new                (void);
GCredentials    *g_credentials_new_for_process    (void);
GCredentials    *g_credentials_new_for_string     (const gchar  *str,
                                                   GError      **error);
gchar           *g_credentials_to_string          (GCredentials    *credentials);

gboolean         g_credentials_has_unix_user      (GCredentials    *credentials);
gint64           g_credentials_get_unix_user      (GCredentials    *credentials);
void             g_credentials_set_unix_user      (GCredentials    *credentials,
                                                   gint64           user_id);

gboolean         g_credentials_has_unix_group     (GCredentials    *credentials);
gint64           g_credentials_get_unix_group     (GCredentials    *credentials);
void             g_credentials_set_unix_group     (GCredentials    *credentials,
                                                   gint64           group_id);

gboolean         g_credentials_has_unix_process   (GCredentials    *credentials);
gint64           g_credentials_get_unix_process   (GCredentials    *credentials);
void             g_credentials_set_unix_process   (GCredentials    *credentials,
                                                   gint64           process_id);

gboolean         g_credentials_has_windows_user   (GCredentials    *credentials);
const gchar     *g_credentials_get_windows_user   (GCredentials    *credentials);
void             g_credentials_set_windows_user   (GCredentials    *credentials,
                                                   const gchar     *user_sid);

G_END_DECLS

#endif /* __G_DBUS_PROXY_H__ */
