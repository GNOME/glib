/*
 * Copyright © 2010 Codethink Limited
 * Copyright © 2010 Novell, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the licence, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: Vincent Untz <vuntz@gnome.org>
 *          Ryan Lortie <desrt@desrt.ca>
 */

#ifndef _gkeyfilesettingsbackend_h_
#define _gkeyfilesettingsbackend_h_

#define G_SETTINGS_ENABLE_BACKEND
#include <gio/gsettingsbackend.h>

G_BEGIN_DECLS

#define G_TYPE_KEYFILE_SETTINGS_BACKEND                      (g_keyfile_settings_backend_get_type ())
#define G_KEYFILE_SETTINGS_BACKEND(inst)                     (G_TYPE_CHECK_INSTANCE_CAST ((inst),      \
                                                              G_TYPE_KEYFILE_SETTINGS_BACKEND,         \
                                                              GKeyfileSettingsBackend))
#define G_KEYFILE_SETTINGS_BACKEND_CLASS(class)              (G_TYPE_CHECK_CLASS_CAST ((class),        \
                                                              G_TYPE_KEYFILE_SETTINGS_BACKEND,         \
                                                              GKeyfileSettingsBackendClass))
#define G_IS_KEYFILE_SETTINGS_BACKEND(inst)                  (G_TYPE_CHECK_INSTANCE_TYPE ((inst),      \
                                                              G_TYPE_KEYFILE_SETTINGS_BACKEND))
#define G_IS_KEYFILE_SETTINGS_BACKEND_CLASS(class)           (G_TYPE_CHECK_CLASS_TYPE ((class),        \
                                                              G_TYPE_KEYFILE_SETTINGS_BACKEND))
#define G_KEYFILE_SETTINGS_BACKEND_GET_CLASS(inst)           (G_TYPE_INSTANCE_GET_CLASS ((inst),       \
                                                              G_TYPE_KEYFILE_SETTINGS_BACKEND,         \
                                                              GKeyfileSettingsBackendClass))

/**
 * GKeyfileSettingsBackend:
 *
 * A backend to GSettings that stores the settings in keyfile.
 **/
typedef struct _GKeyfileSettingsBackendPrivate               GKeyfileSettingsBackendPrivate;
typedef struct _GKeyfileSettingsBackendClass                 GKeyfileSettingsBackendClass;
typedef struct _GKeyfileSettingsBackend                      GKeyfileSettingsBackend;

struct _GKeyfileSettingsBackendClass
{
  GSettingsBackendClass parent_class;
};

struct _GKeyfileSettingsBackend
{
  GSettingsBackend parent_instance;

  /*< private >*/
  GKeyfileSettingsBackendPrivate *priv;
};

GType g_keyfile_settings_backend_get_type (void);

G_END_DECLS

#endif /* _gkeyfilesettingsbackend_h_ */
