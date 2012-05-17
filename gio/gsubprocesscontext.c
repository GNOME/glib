/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright © 2012 Red Hat, Inc.
 * Copyright © 2012 Canonical Limited
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2 of the licence or (at
 * your option) any later version.
 *
 * See the included COPYING file for more information.
 *
 * Authors: Colin Walters <walters@verbum.org>
 *          Ryan Lortie <desrt@desrt.ca>
 */

/**
 * SECTION:gsubprocess
 * @title: GSubprocess Context
 * @short_description: Environment options for launching a child process
 *
 * This class contains a set of options for launching child processes,
 * such as where its standard input and output will be directed, the
 * argument list, the environment, and more.
 *
 * While the #GSubprocess class has high level functions covering
 * popular cases, use of this class allows access to more advanced
 * options.  It can also be used to launch multiple subprocesses with
 * a similar configuration.
 *
 * Since: 2.36
 */

#include "config.h"
#include "gsubprocesscontext-private.h"
#include "gsubprocess.h"
#include "gasyncresult.h"
#include "glibintl.h"
#include "glib-private.h"

#include <string.h>

typedef GObjectClass GSubprocessContextClass;

G_DEFINE_TYPE (GSubprocessContext, g_subprocess_context, G_TYPE_OBJECT);

enum
{
  PROP_0,
  PROP_ARGV,
  N_PROPS
};

static GParamSpec *g_subprocess_context_pspecs[N_PROPS];

GSubprocessContext *
g_subprocess_context_new (gchar           **argv)
{
  return g_object_new (G_TYPE_SUBPROCESS_CONTEXT,
		       "argv", argv,
		       NULL);
}

static void
g_subprocess_context_init (GSubprocessContext  *self)
{
  self->argv = g_ptr_array_new_with_free_func (g_free);
  self->stdin_fd = -1;
  self->stdout_fd = -1;
  self->stderr_fd = -1;
}

static void
g_subprocess_context_finalize (GObject *object)
{
  GSubprocessContext *self = G_SUBPROCESS_CONTEXT (object);

  g_ptr_array_unref (self->argv);
  g_strfreev (self->envp);
  g_free (self->cwd);

  g_free (self->stdin_path);
  g_free (self->stdout_path);
  g_free (self->stderr_path);

  if (G_OBJECT_CLASS (g_subprocess_context_parent_class)->finalize != NULL)
    G_OBJECT_CLASS (g_subprocess_context_parent_class)->finalize (object);
}

static void
g_subprocess_context_set_property (GObject      *object,
				   guint         prop_id,
				   const GValue *value,
				   GParamSpec   *pspec)
{
  GSubprocessContext *self = G_SUBPROCESS_CONTEXT (object);

  switch (prop_id)
    {
    case PROP_ARGV:
      g_subprocess_context_set_args (self, (char**)g_value_get_boxed (value));
      break;

    default:
      g_assert_not_reached ();
    }
}

static void
g_subprocess_context_get_property (GObject    *object,
				   guint       prop_id,
				   GValue     *value,
				   GParamSpec *pspec)
{
  GSubprocessContext *self = G_SUBPROCESS_CONTEXT (object);

  switch (prop_id)
    {
    case PROP_ARGV:
      g_value_set_boxed (value, self->argv->pdata);
      break;

    default:
      g_assert_not_reached ();
    }
}

static void
g_subprocess_context_class_init (GSubprocessContextClass *class)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (class);

  gobject_class->finalize = g_subprocess_context_finalize;
  gobject_class->get_property = g_subprocess_context_get_property;
  gobject_class->set_property = g_subprocess_context_set_property;

  /**
   * GSubprocessContext:argv:
   *
   * Array of arguments passed to child process; must have at least
   * one element.  The first element has special handling - if it is
   * an not absolute path ( as determined by g_path_is_absolute() ),
   * then the system search path will be used.  See
   * %G_SPAWN_SEARCH_PATH.
   * 
   * Note that in order to use the Unix-specific argv0 functionality,
   * you must use the setter function
   * g_subprocess_context_set_args_and_argv0().  For more information
   * about this, see %G_SPAWN_FILE_AND_ARGV_ZERO.
   *
   * Since: 2.36
   */
  g_subprocess_context_pspecs[PROP_ARGV] = g_param_spec_boxed ("argv", P_("Arguments"), P_("Arguments for child process"), G_TYPE_STRV,
							       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, N_PROPS, g_subprocess_context_pspecs);
}

/* Only exported on Unix */
#ifndef G_OS_UNIX
static
#endif
void
g_subprocess_context_set_args_and_argv0 (GSubprocessContext    *self,
					 const gchar           *argv0,
					 gchar                **args)
{
  gchar **iter;

  g_ptr_array_set_size (self->argv, 0);

  if (argv0)
    g_ptr_array_add (self->argv, g_strdup (argv0));
  
  for (iter = args; *iter; iter++)
    g_ptr_array_add (self->argv, g_strdup (*iter));
  g_ptr_array_add (self->argv, NULL);
}

void
g_subprocess_context_set_args (GSubprocessContext           *self,
			       gchar                       **args)
{
  g_subprocess_context_set_args_and_argv0 (self, NULL, args);
}

/* Environment */

void
g_subprocess_context_set_environment (GSubprocessContext           *self,
				      gchar                       **environ)
{
  g_strfreev (self->envp);
  self->envp = g_strdupv (environ);
}

void
g_subprocess_context_set_cwd (GSubprocessContext           *self,
			      const gchar                  *cwd)
{
  g_free (self->cwd);
  self->cwd = g_strdup (cwd);
}

void
g_subprocess_context_set_keep_descriptors (GSubprocessContext           *self,
					   gboolean                      keep_descriptors)

{
  self->keep_descriptors = keep_descriptors ? 1 : 0;
}

void
g_subprocess_context_set_search_path (GSubprocessContext           *self,
				      gboolean                      search_path,
				      gboolean                      search_path_from_envp)
{
  self->search_path = search_path ? 1 : 0;
  self->search_path_from_envp = search_path_from_envp ? 1 : 0;
}

void
g_subprocess_context_set_stdin_disposition (GSubprocessContext           *self,
					    GSubprocessStreamDisposition  disposition)
{
  g_return_if_fail (disposition != G_SUBPROCESS_STREAM_DISPOSITION_STDERR_MERGE);
  self->stdin_disposition = disposition;
}

void
g_subprocess_context_set_stdout_disposition (GSubprocessContext           *self,
					     GSubprocessStreamDisposition  disposition)
{
  g_return_if_fail (disposition != G_SUBPROCESS_STREAM_DISPOSITION_STDERR_MERGE);
  self->stdout_disposition = disposition;
}

void
g_subprocess_context_set_stderr_disposition (GSubprocessContext           *self,
					     GSubprocessStreamDisposition  disposition)
{
  self->stderr_disposition = disposition;
}

#ifdef G_OS_UNIX
void
g_subprocess_context_set_stdin_file_path (GSubprocessContext           *self,
					  const gchar                  *path)
{
  self->stdin_disposition = G_SUBPROCESS_STREAM_DISPOSITION_NULL;
  g_free (self->stdin_path);
  self->stdin_path = g_strdup (path);
}

void
g_subprocess_context_set_stdin_fd        (GSubprocessContext           *self,
					  gint                          fd)
{
  self->stdin_disposition = G_SUBPROCESS_STREAM_DISPOSITION_NULL;
  self->stdin_fd = fd;
}

void
g_subprocess_context_set_stdout_file_path (GSubprocessContext           *self,
					   const gchar                  *path)
{
  self->stdout_disposition = G_SUBPROCESS_STREAM_DISPOSITION_NULL;
  g_free (self->stdout_path);
  self->stdout_path = g_strdup (path);
}

void
g_subprocess_context_set_stdout_fd (GSubprocessContext           *self,
				    gint                          fd)
{
  self->stdout_disposition = G_SUBPROCESS_STREAM_DISPOSITION_NULL;
  self->stdout_fd = fd;
}

void
g_subprocess_context_set_stderr_file_path (GSubprocessContext           *self,
					   const gchar                  *path)
{
  self->stderr_disposition = G_SUBPROCESS_STREAM_DISPOSITION_NULL;
  g_free (self->stderr_path);
  self->stderr_path = g_strdup (path);
}

void
g_subprocess_context_set_stderr_fd        (GSubprocessContext           *self,
					   gint                          fd)
{
  self->stderr_disposition = G_SUBPROCESS_STREAM_DISPOSITION_NULL;
  self->stderr_fd = fd;
}
#endif

#ifdef G_OS_UNIX
void
g_subprocess_context_set_child_setup (GSubprocessContext           *self,
				      GSpawnChildSetupFunc          child_setup,
				      gpointer                      user_data)
{
  self->child_setup_func = child_setup;
  self->child_setup_data = user_data;
}
#endif
