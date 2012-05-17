/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright © 2012 Colin Walters <walters@verbum.org>
 * Copyright © 2012 Canonical Limited
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
 * Author: Ryan Lortie <desrt@desrt.ca>
 * Author: Colin Walters <walters@verbum.org>
 */

#if !defined (__GIO_GIO_H_INSIDE__) && !defined (GIO_COMPILATION)
#error "Only <gio/gio.h> can be included directly."
#endif

#ifndef __G_SUBPROCESS_CONTEXT_H__
#define __G_SUBPROCESS_CONTEXT_H__

#include <gio/giotypes.h>

G_BEGIN_DECLS

#define G_TYPE_SUBPROCESS_CONTEXT         (g_subprocess_context_get_type ())
#define G_SUBPROCESS_CONTEXT(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), G_TYPE_SUBPROCESS_CONTEXT, GSubprocessContext))
#define G_IS_SUBPROCESS_CONTEXT(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), G_TYPE_SUBPROCESS_CONTEXT))

GLIB_AVAILABLE_IN_2_36
GType            g_subprocess_context_get_type (void) G_GNUC_CONST;

GLIB_AVAILABLE_IN_2_36
GSubprocessContext * g_subprocess_context_new (gchar           **argv);

/* Argument control */
GLIB_AVAILABLE_IN_2_36
void             g_subprocess_context_set_args (GSubprocessContext           *self,
						gchar                       **args);
#ifdef G_OS_UNIX
GLIB_AVAILABLE_IN_2_36
void             g_subprocess_context_set_args_and_argv0 (GSubprocessContext           *self,
							  const gchar                  *argv0,
							  gchar                       **args);
#endif

/* Environment */

GLIB_AVAILABLE_IN_2_36
void             g_subprocess_context_set_environment (GSubprocessContext           *self,
						       gchar                       **environ);
GLIB_AVAILABLE_IN_2_36
void             g_subprocess_context_set_cwd (GSubprocessContext           *self,
					       const gchar                  *cwd);
GLIB_AVAILABLE_IN_2_36
void             g_subprocess_context_set_keep_descriptors (GSubprocessContext           *self,
							    gboolean                      keep_descriptors);
GLIB_AVAILABLE_IN_2_36
void             g_subprocess_context_set_search_path (GSubprocessContext           *self,
						       gboolean                      search_path,
						       gboolean                      search_path_from_envp);

/* Basic I/O control */

GLIB_AVAILABLE_IN_2_36
void             g_subprocess_context_set_stdin_disposition (GSubprocessContext           *self,
							     GSubprocessStreamDisposition  disposition);
GLIB_AVAILABLE_IN_2_36
void             g_subprocess_context_set_stdout_disposition (GSubprocessContext           *self,
							      GSubprocessStreamDisposition  disposition);
GLIB_AVAILABLE_IN_2_36
void             g_subprocess_context_set_stderr_disposition (GSubprocessContext           *self,
							      GSubprocessStreamDisposition  disposition);

/* Extended I/O control, only available on UNIX */

#ifdef G_OS_UNIX
GLIB_AVAILABLE_IN_2_36
void             g_subprocess_context_set_stdin_file_path (GSubprocessContext           *self,
							   const gchar                  *path);
GLIB_AVAILABLE_IN_2_36
void             g_subprocess_context_set_stdin_fd        (GSubprocessContext           *self,
							   gint                          fd);
GLIB_AVAILABLE_IN_2_36
void             g_subprocess_context_set_stdout_file_path (GSubprocessContext           *self,
							    const gchar                  *path);
GLIB_AVAILABLE_IN_2_36
void             g_subprocess_context_set_stdout_fd        (GSubprocessContext           *self,
							    gint                          fd);
GLIB_AVAILABLE_IN_2_36
void             g_subprocess_context_set_stderr_file_path (GSubprocessContext           *self,
							    const gchar                  *path);
GLIB_AVAILABLE_IN_2_36
void             g_subprocess_context_set_stderr_fd        (GSubprocessContext           *self,
							    gint                          fd);
#endif

/* Child setup, only available on UNIX */
#ifdef G_OS_UNIX
GLIB_AVAILABLE_IN_2_36
void             g_subprocess_context_set_child_setup        (GSubprocessContext           *self,
							      GSpawnChildSetupFunc          child_setup,
							      gpointer                      user_data);
#endif

G_END_DECLS

#endif /* __G_SUBPROCESS_H__ */
