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
 * The <structname>GApplication</structname> structure contains private
 * data and should only be accessed using the provided API
 *
 * Since: 2.28
 */
struct _GApplication
{
  /*< private >*/
  GObject parent_instance;

  GApplicationPrivate *priv;
};

/**
 * GApplicationClass:
 * @startup: invoked on the primary instance immediately after registration
 * @activate: invoked on the primary instance when an activation occurs
 * @open: invoked on the primary instance when there are files to open
 * @command_line: invoked on the primary instance when a command-line is
 *   not handled locally
 * @local_command_line: invoked (locally) when the process has been invoked
 *     via commandline execution.  The virtual function has the chance to
 *     inspect (and possibly replace) the list of command line arguments.
 *     See g_application_run() for more information.
 * @before_emit: invoked on the primary instance before 'activate', 'open',
 *     'command-line' or any action invocation, gets the 'platform data' from
 *     the calling instance
 * @after_emit: invoked on the primary instance after 'activate', 'open',
 *     'command-line' or any action invocation, gets the 'platform data' from
 *     the calling instance
 * @add_platform_data: invoked (locally) to add 'platform data' to be sent to
 *     the primary instance when activating, opening or invoking actions
 * @quit_mainloop: invoked on the primary instance when the use count of the
 *     application drops to zero (and after any inactivity timeout, if
 *     requested)
 * @run_mainloop: invoked on the primary instance from g_application_run()
 *     if the use-count is non-zero
 *
 * Since: 2.28
 */
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

  /*< private >*/
  gpointer padding[12];
};

GType                   g_application_get_type                          (void) G_GNUC_CONST;

gboolean                g_application_id_is_valid                       (const gchar              *application_id);

GApplication *          g_application_new                               (const gchar              *application_id,
                                                                         GApplicationFlags         flags);

const gchar *           g_application_get_application_id                (GApplication             *application);
void                    g_application_set_application_id                (GApplication             *application,
                                                                         const gchar              *application_id);

guint                   g_application_get_inactivity_timeout            (GApplication             *application);
void                    g_application_set_inactivity_timeout            (GApplication             *application,
                                                                         guint                     inactivity_timeout);

GApplicationFlags       g_application_get_flags                         (GApplication             *application);
void                    g_application_set_flags                         (GApplication             *application,
                                                                         GApplicationFlags         flags);

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

G_END_DECLS

#endif /* __G_APPLICATION_H__ */
