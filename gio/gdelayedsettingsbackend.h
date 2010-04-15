/*
 * Copyright © 2009 Codethink Limited
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of version 3 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * See the included COPYING file for more information.
 */

#ifndef __G_DELAYED_SETTINGS_BACKEND_H__
#define __G_DELAYED_SETTINGS_BACKEND_H__

#include <glib-object.h>

#define G_SETTINGS_ENABLE_BACKEND
#include <gio/gsettingsbackend.h>

#define G_TYPE_DELAYED_SETTINGS_BACKEND                     (g_delayed_settings_backend_get_type ())
#define G_DELAYED_SETTINGS_BACKEND(inst)                    (G_TYPE_CHECK_INSTANCE_CAST ((inst),                     \
                                                             G_TYPE_DELAYED_SETTINGS_BACKEND,                        \
                                                             GDelayedSettingsBackend))
#define G_DELAYED_SETTINGS_BACKEND_CLASS(class)             (G_TYPE_CHECK_CLASS_CAST ((class),                       \
                                                             G_TYPE_DELAYED_SETTINGS_BACKEND,                        \
                                                             GDelayedSettingsBackendClass))
#define G_IS_DELAYED_SETTINGS_BACKEND(inst)                 (G_TYPE_CHECK_INSTANCE_TYPE ((inst),                     \
                                                             G_TYPE_DELAYED_SETTINGS_BACKEND))
#define G_IS_DELAYED_SETTINGS_BACKEND_CLASS(class)          (G_TYPE_CHECK_CLASS_TYPE ((class),                       \
                                                             G_TYPE_DELAYED_SETTINGS_BACKEND))
#define G_DELAYED_SETTINGS_BACKEND_GET_CLASS(inst)          (G_TYPE_INSTANCE_GET_CLASS ((inst),                      \
                                                             G_TYPE_DELAYED_SETTINGS_BACKEND,                        \
                                                             GDelayedSettingsBackendClass))

typedef struct _GDelayedSettingsBackendPrivate              GDelayedSettingsBackendPrivate;
typedef struct _GDelayedSettingsBackendClass                GDelayedSettingsBackendClass;
typedef struct _GDelayedSettingsBackend                     GDelayedSettingsBackend;

struct _GDelayedSettingsBackendClass
{
  GSettingsBackendClass parent_class;
};

struct _GDelayedSettingsBackend
{
  GSettingsBackend parent_instance;
  GDelayedSettingsBackendPrivate *priv;
};

G_BEGIN_DECLS

GType                           g_delayed_settings_backend_get_type     (void);
GSettingsBackend *              g_delayed_settings_backend_new          (GSettingsBackend        *backend);
void                            g_delayed_settings_backend_revert       (GDelayedSettingsBackend *delayed);
void                            g_delayed_settings_backend_apply        (GDelayedSettingsBackend *delayed);
gboolean                        g_delayed_settings_backend_get_has_unapplied (GDelayedSettingsBackend *delayed);

G_END_DECLS

#endif  /* __G_DELAYED_SETTINGS_BACKEND_H__ */
