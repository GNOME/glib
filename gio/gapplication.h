/*
 * Copyright © 2010 Codethink Limited
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2 of the licence or (at
 * your option) any later version.
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
 * Authors: Ryan Lortie <desrt@desrt.ca>
 */

#if !defined (__GIO_GIO_H_INSIDE__) && !defined (GIO_COMPILATION)
#error "Only <gio/gio.h> can be included directly."
#endif

#ifndef __G_APPLICATION_H__
#define __G_APPLICATION_H__

#include <gio/giotypes.h>

G_BEGIN_DECLS

#define G_TYPE_APPLICATION                                  (g_application_get_type ())
#define G_APPLICATION(inst)                                 (G_TYPE_CHECK_INSTANCE_CAST ((inst),                     \
                                                             G_TYPE_APPLICATION, GApplication))
#define G_APPLICATION_CLASS(class)                          (G_TYPE_CHECK_CLASS_CAST ((class),                       \
                                                             G_TYPE_APPLICATION, GApplicationClass))
#define G_IS_APPLICATION(inst)                              (G_TYPE_CHECK_INSTANCE_TYPE ((inst), G_TYPE_APPLICATION))
#define G_IS_APPLICATION_CLASS(class)                       (G_TYPE_CHECK_CLASS_TYPE ((class), G_TYPE_APPLICATION))
#define G_APPLICATION_GET_CLASS(inst)                       (G_TYPE_INSTANCE_GET_CLASS ((inst),                      \
                                                             G_TYPE_APPLICATION, GApplicationClass))

typedef struct _GApplicationPrivate                         GApplicationPrivate;
typedef struct _GApplicationClass                           GApplicationClass;

/**
 * GApplication:
 *
 * Since: 2.28
 */
struct _GApplication
{
  /*< private >*/
  GObject parent_instance;

  GApplicationPrivate *priv;
};

struct _GApplicationClass
{
  /*< private >*/
  GObjectClass parent_class;

  /*< public >*/
  /* signals */
  void                      (* startup)             (GApplication              *application);

  void                      (* activate)            (GApplication              *application);

  void                      (* open)                (GApplication              *application,
                                                     GFile                    **files,
                                                     gint                       n_files,
                                                     const gchar               *hint);

  int                       (* command_line)        (GApplication              *application,
                                                     GApplicationCommandLine   *command_line);

  /* vfuncs */
  gboolean                  (* local_command_line)  (GApplication              *application,
                                                     gchar                   ***arguments,
                                                     int                       *exit_status);

  void                      (* before_emit)         (GApplication              *application,
                                                     GVariant                  *platform_data);
  void                      (* after_emit)          (GApplication              *application,
                                                     GVariant                  *platform_data);
  void                      (* add_platform_data)   (GApplication              *application,
                                                     GVariantBuilder           *builder);
  void                      (* quit_mainloop)       (GApplication              *application);
  void                      (* run_mainloop)        (GApplication              *application);
  void                      (* shutdown)            (GApplication              *application);

  gboolean                  (* dbus_register)       (GApplication              *application,
                                                     GDBusConnection           *connection,
                                                     const gchar               *object_path,
                                                     GError                   **error);
  void                      (* dbus_unregister)     (GApplication              *application,
                                                     GDBusConnection           *connection,
                                                     const gchar               *object_path);

  /*< private >*/
  gpointer padding[9];
};

GType                   g_application_get_type                          (void) G_GNUC_CONST;

gboolean                g_application_id_is_valid                       (const gchar              *application_id);

GApplication *          g_application_new                               (const gchar              *application_id,
                                                                         GApplicationFlags         flags);

const gchar *           g_application_get_application_id                (GApplication             *application);
void                    g_application_set_application_id                (GApplication             *application,
                                                                         const gchar              *application_id);

GLIB_AVAILABLE_IN_2_34
GDBusConnection *       g_application_get_dbus_connection               (GApplication             *application);
GLIB_AVAILABLE_IN_2_34
const gchar *           g_application_get_dbus_object_path              (GApplication             *application);

guint                   g_application_get_inactivity_timeout            (GApplication             *application);
void                    g_application_set_inactivity_timeout            (GApplication             *application,
                                                                         guint                     inactivity_timeout);

GApplicationFlags       g_application_get_flags                         (GApplication             *application);
void                    g_application_set_flags                         (GApplication             *application,
                                                                         GApplicationFlags         flags);

GLIB_DEPRECATED
void                    g_application_set_action_group                  (GApplication             *application,
                                                                         GActionGroup             *action_group);

gboolean                g_application_get_is_registered                 (GApplication             *application);
gboolean                g_application_get_is_remote                     (GApplication             *application);

gboolean                g_application_register                          (GApplication             *application,
                                                                         GCancellable             *cancellable,
                                                                         GError                  **error);

void                    g_application_hold                              (GApplication             *application);
void                    g_application_release                           (GApplication             *application);

void                    g_application_activate                          (GApplication             *application);

void                    g_application_open                              (GApplication             *application,
                                                                         GFile                   **files,
                                                                         gint                      n_files,
                                                                         const gchar              *hint);

int                     g_application_run                               (GApplication             *application,
                                                                         int                       argc,
                                                                         char                    **argv);

void                    g_application_quit                              (GApplication             *application);

GLIB_AVAILABLE_IN_2_32
GApplication *          g_application_get_default                       (void);
GLIB_AVAILABLE_IN_2_32
void                    g_application_set_default                       (GApplication             *application);

G_END_DECLS

#endif /* __G_APPLICATION_H__ */
