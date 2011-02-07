/* GIO - GLib Input, Output and Streaming Library
 *
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

#ifndef __G_APPLICATION_COMMAND_LINE_H__
#define __G_APPLICATION_COMMAND_LINE_H__

#include <gio/giotypes.h>

G_BEGIN_DECLS

#define G_TYPE_APPLICATION_COMMAND_LINE                     (g_application_command_line_get_type ())
#define G_APPLICATION_COMMAND_LINE(inst)                    (G_TYPE_CHECK_INSTANCE_CAST ((inst),                     \
                                                             G_TYPE_APPLICATION_COMMAND_LINE,                        \
                                                             GApplicationCommandLine))
#define G_APPLICATION_COMMAND_LINE_CLASS(class)             (G_TYPE_CHECK_CLASS_CAST ((class),                       \
                                                             G_TYPE_APPLICATION_COMMAND_LINE,                        \
                                                             GApplicationCommandLineClass))
#define G_IS_APPLICATION_COMMAND_LINE(inst)                 (G_TYPE_CHECK_INSTANCE_TYPE ((inst),                     \
                                                             G_TYPE_APPLICATION_COMMAND_LINE))
#define G_IS_APPLICATION_COMMAND_LINE_CLASS(class)          (G_TYPE_CHECK_CLASS_TYPE ((class),                       \
                                                             G_TYPE_APPLICATION_COMMAND_LINE))
#define G_APPLICATION_COMMAND_LINE_GET_CLASS(inst)          (G_TYPE_INSTANCE_GET_CLASS ((inst),                      \
                                                             G_TYPE_APPLICATION_COMMAND_LINE,                        \
                                                             GApplicationCommandLineClass))

typedef struct _GApplicationCommandLinePrivate               GApplicationCommandLinePrivate;
typedef struct _GApplicationCommandLineClass                 GApplicationCommandLineClass;

/**
 * GApplicationCommandLine:
 *
 * The <structname>GApplicationCommandLine</structname> structure contains private
 * data and should only be accessed using the provided API
 *
 * Since: 2.28
 */
struct _GApplicationCommandLine
{
  /*< private >*/
  GObject parent_instance;

  GApplicationCommandLinePrivate *priv;
};

/**
 * GApplicationCommandLineClass:
 *
 * The <structname>GApplicationCommandLineClass</structname> structure contains
 * private data only
 *
 * Since: 2.28
 */
struct _GApplicationCommandLineClass
{
  /*< private >*/
  GObjectClass parent_class;

  void (* print_literal)    (GApplicationCommandLine *cmdline,
                             const gchar             *message);
  void (* printerr_literal) (GApplicationCommandLine *cmdline,
                             const gchar             *message);

  gpointer padding[12];
};

GType                   g_application_command_line_get_type             (void) G_GNUC_CONST;

gchar **                g_application_command_line_get_arguments        (GApplicationCommandLine   *cmdline,
                                                                         int                       *argc);

const gchar * const *   g_application_command_line_get_environ          (GApplicationCommandLine   *cmdline);

const gchar *           g_application_command_line_getenv               (GApplicationCommandLine   *cmdline,
                                                                         const gchar               *name);

const gchar *           g_application_command_line_get_cwd              (GApplicationCommandLine   *cmdline);

gboolean                g_application_command_line_get_is_remote        (GApplicationCommandLine   *cmdline);

void                    g_application_command_line_print                (GApplicationCommandLine   *cmdline,
                                                                         const gchar               *format,
                                                                         ...) G_GNUC_PRINTF(2, 3);
void                    g_application_command_line_printerr             (GApplicationCommandLine   *cmdline,
                                                                         const gchar               *format,
                                                                         ...) G_GNUC_PRINTF(2, 3);

int                     g_application_command_line_get_exit_status      (GApplicationCommandLine   *cmdline);
void                    g_application_command_line_set_exit_status      (GApplicationCommandLine   *cmdline,
                                                                         int                        exit_status);

GVariant *              g_application_command_line_get_platform_data    (GApplicationCommandLine   *cmdline);

G_END_DECLS

#endif /* __G_APPLICATION_COMMAND_LINE_H__ */
