/* GObject introspection: typelib inspector
 *
 * Copyright (C) 2011-2016 Dominique Leuenberger <dimstar@opensuse.org>
 * Copyright © 2016 Igor Gnatenko <ignatenko@redhat.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
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
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <glib.h>
#include <girepository.h>
#include <stdlib.h>
#include <locale.h>

static void
print_shlibs (GIRepository *repository,
              const gchar  *namespace)
{
  /* Finding the shared library we depend on (if any) */
  const char * const *shlibs = gi_repository_get_shared_libraries (repository, namespace, NULL);
  for (size_t i = 0; shlibs != NULL && shlibs[i] != NULL; i++)
    g_print ("shlib: %s\n", shlibs[i]);
}

static void
print_typelibs (GIRepository *repository,
                const gchar  *namespace)
{
  guint i = 0;

  /* Finding all the typelib-based Requires */
  GStrv deps = gi_repository_get_dependencies (repository, namespace, NULL);
  if (deps)
    {
      for (i = 0; deps[i]; i++)
        g_print ("typelib: %s\n", deps[i]);
      g_strfreev (deps);
    }
}

gint
main (gint   argc,
      gchar *argv[])
{
  gint status = EXIT_SUCCESS;

  GError *error = NULL;
  GIRepository *repository = NULL;
  GITypelib *typelib = NULL;

  gchar *version = NULL;
  gboolean opt_shlibs = FALSE;
  gboolean opt_typelibs = FALSE;
  GStrv namespaces = NULL;
  const gchar *namespace = NULL;
  const GOptionEntry options[] = {
    { "typelib-version", 0, 0, G_OPTION_ARG_STRING, &version, "Typelib version to inspect", "VERSION" },
    { "print-shlibs", 0, 0, G_OPTION_ARG_NONE, &opt_shlibs, "List the shared libraries the typelib requires", NULL },
    { "print-typelibs", 0, 0, G_OPTION_ARG_NONE, &opt_typelibs, "List other typelibs the inspected typelib requires", NULL },
    { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY, &namespaces, "The typelib to inspect", "NAMESPACE" },
    G_OPTION_ENTRY_NULL
  };
  GOptionContext *context = NULL;

  setlocale (LC_ALL, "");

  context = g_option_context_new ("- Inspect GI typelib");
  g_option_context_add_main_entries (context, options, NULL);
  if (!g_option_context_parse (context, &argc, &argv, &error))
    {
      status = EXIT_FAILURE;
      g_printerr ("Failed to parse command line options: %s\n", error->message);
      goto out;
    }

  if (!namespaces)
    {
      status = EXIT_FAILURE;
      g_printerr ("Please specify at least one namespace\n");
      goto out;
    }

  if (g_strv_length (namespaces) > 1)
    {
      status = EXIT_FAILURE;
      g_printerr ("Please specify only one namespace\n");
      goto out;
    }
  namespace = namespaces[0];

  if (!opt_shlibs && !opt_typelibs)
    {
      status = EXIT_FAILURE;
      g_printerr ("Please specify --print-shlibs, --print-typelibs or both.\n");
      goto out;
    }

  repository = gi_repository_new ();
  typelib = gi_repository_require (repository, namespace, version, 0, &error);
  if (!typelib)
    {
      status = EXIT_FAILURE;
      g_printerr ("Failed to load typelib: %s\n", error->message);
      goto out;
    }

  if (opt_shlibs)
    print_shlibs (repository, namespace);
  if (opt_typelibs)
    print_typelibs (repository, namespace);

out:
  g_option_context_free (context);
  if (error)
    g_error_free (error);
  g_clear_object (&repository);
  g_strfreev (namespaces);
  g_free (version);

  return status;
}
