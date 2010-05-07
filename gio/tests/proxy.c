/* GLib testing framework examples and tests
 *
 * Copyright (C) 2010 Collabora, Ltd.
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
 * Authors: Nicolas Dufresne <nicolas.dufresne@collabora.co.uk>
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gio/gio.h>
#include <glib.h>

#include "glibintl.h"

static const gchar *uri = NULL;
static GCancellable *cancellable = NULL;
static gint return_value = 0;

static void G_GNUC_NORETURN
usage (void)
{
  fprintf (stderr, "Usage: proxy [-s] uri\n");
  fprintf (stderr, "       Use -s to do synchronous lookups.\n");
  fprintf (stderr, "       Use -c to cancel operation.\n");
  exit (1);
}

static void
print_and_free_error (GError *error)
{
  fprintf (stderr, "Failed to obtain proxies: %s\n", error->message);
  g_error_free (error);
  return_value = 1;
}

static void
print_proxies (const gchar *uri, gchar **proxies)
{
  printf ("Proxies for URI '%s' are:\n", uri);

  if (proxies == NULL || proxies[0] == NULL)
    printf ("\tnone\n");
  else
    for (; proxies[0]; proxies++)
      printf ("\t%s\n", proxies[0]);
}

static void
_proxy_lookup_cb (GObject *source_object,
		  GAsyncResult *result,
		  gpointer user_data)
{
  GError *error = NULL;
  gchar **proxies;
  GMainLoop *loop = user_data;

  proxies = g_proxy_resolver_lookup_finish (G_PROXY_RESOLVER (source_object),
					    result,
					    &error);
  if (error)
    {
      print_and_free_error (error);
    }
  else
    {
      print_proxies (uri, proxies);
      g_strfreev (proxies);
    }

  g_main_loop_quit (loop);
}

static void
use_resolver (gboolean synchronous)
{
  GProxyResolver *resolver;

  resolver = g_proxy_resolver_get_default ();

  if (synchronous)
    {
      GError *error = NULL;
      gchar **proxies;

      proxies = g_proxy_resolver_lookup (resolver, uri, cancellable, &error);

      if (error)
	  print_and_free_error (error);
      else
	  print_proxies (uri, proxies);

      g_strfreev (proxies);
    }
  else
    {
      GMainLoop *loop = g_main_loop_new (NULL, FALSE);

      g_proxy_resolver_lookup_async (resolver,
				     uri,
				     cancellable,
				     _proxy_lookup_cb,
				     loop);

      g_main_loop_run (loop);
      g_main_loop_unref (loop);
    }
}

typedef enum
{
  USE_RESOLVER,
} ProxyTestType;

gint
main (gint argc, gchar **argv)
{
  gboolean synchronous = FALSE;
  gboolean cancel = FALSE;
  ProxyTestType type = USE_RESOLVER;

  g_type_init ();

  while (argc >= 2 && argv[1][0] == '-')
    {
      if (!strcmp (argv[1], "-s"))
        synchronous = TRUE;
      else if (!strcmp (argv[1], "-c"))
        cancel = TRUE;
      else
	usage ();

      argv++;
      argc--;
    }

  if (argc != 2)
    usage ();

  /* Save URI for asyncrhonous callback */
  uri = argv[1];

  if (cancel)
    {
      cancellable = g_cancellable_new ();
      g_cancellable_cancel (cancellable);
    }

  switch (type)
    {
    case USE_RESOLVER:
      use_resolver (synchronous);
      break;
    }

  return return_value;
}
