/*
 * Copyright © 2009, 2010 Codethink Limited
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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Ryan Lortie <desrt@desrt.ca>
 */

#ifndef __G_DELAYED_SETTINGS_BACKEND_H__
#define __G_DELAYED_SETTINGS_BACKEND_H__

#include <glib-object.h>

#include <gio/gsettingsbackend.h>

#define G_TYPE_DELAYED_SETTINGS_BACKEND (g_delayed_settings_backend_get_type ())
G_DECLARE_FINAL_TYPE(GDelayedSettingsBackend,
                     g_delayed_settings_backend,
                     G, DELAYED_SETTINGS_BACKEND,
                     GSettingsBackend)

GDelayedSettingsBackend *       g_delayed_settings_backend_new          (GSettingsBackend        *backend,
                                                                         gpointer                 owner,
                                                                         GMainContext            *owner_context);
void                            g_delayed_settings_backend_revert       (GDelayedSettingsBackend *delayed);
void                            g_delayed_settings_backend_apply        (GDelayedSettingsBackend *delayed);
gboolean                        g_delayed_settings_backend_get_has_unapplied (GDelayedSettingsBackend *delayed);

#endif  /* __G_DELAYED_SETTINGS_BACKEND_H__ */
