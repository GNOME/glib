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
  fprintf (stderr, "       Use -e to use enumerator.\n");
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

static void
print_proxy_address (GSocketAddress *sockaddr)
{
  GProxyAddress *proxy = NULL;

  if (sockaddr == NULL)
    {
      printf ("\tdirect://\n");
      return;
    }

  if (G_IS_PROXY_ADDRESS (sockaddr))
    {
      proxy = G_PROXY_ADDRESS (sockaddr);
      printf ("\t%s://", g_proxy_address_get_protocol(proxy));
    }
  else
    {
      printf ("\tdirect://");
    }

  if (G_IS_INET_SOCKET_ADDRESS (sockaddr))
    {
      GInetAddress *inetaddr;
      guint port;
      gchar *addr;

      g_object_get (sockaddr,
		    "address", &inetaddr,
		    "port", &port,
		    NULL);

      addr = g_inet_address_to_string (inetaddr);

      printf ("%s:%u", addr, port);

      g_free (addr);
    }

  if (proxy && g_proxy_address_get_username(proxy))
    printf ("\t(Username: %s  Password: %s)",
	    g_proxy_address_get_username(proxy),
	    g_proxy_address_get_password(proxy));

  printf ("\n");
}

static void
_proxy_enumerate_cb (GObject *object,
		     GAsyncResult *result,
		     gpointer user_data)
{
  GError *error = NULL;
  GMainLoop *loop = user_data;
  GSocketAddressEnumerator *enumerator = G_SOCKET_ADDRESS_ENUMERATOR (object);
  GSocketAddress *sockaddr;

  sockaddr = g_socket_address_enumerator_next_finish (enumerator,
						      result,
						      &error);
  if (sockaddr)
    {
      print_proxy_address (sockaddr);
      g_socket_address_enumerator_next_async (enumerator,
                                              cancellable,
					      _proxy_enumerate_cb,
					      loop);
      g_object_unref (sockaddr);
    }
  else
    {
      if (error)
	print_and_free_error (error);

      g_main_loop_quit (loop);
    }
}

static void
run_with_enumerator (gboolean synchronous, GSocketAddressEnumerator *enumerator)
{
  GError *error = NULL;

  if (synchronous)
    {
      GSocketAddress *sockaddr;

      while ((sockaddr = g_socket_address_enumerator_next(enumerator,
							  cancellable,
							  &error)))
	{
	  print_proxy_address (sockaddr);
	  g_object_unref (sockaddr);
	}

      if (error)
	print_and_free_error (error);
    }
  else
    {
      GMainLoop *loop = g_main_loop_new (NULL, FALSE);

      g_socket_address_enumerator_next_async (enumerator,
                                              cancellable,
					      _proxy_enumerate_cb,
					      loop);
      g_main_loop_run (loop);
      g_main_loop_unref (loop);
    }
}

static void
use_enumerator (gboolean synchronous)
{
  GSocketAddressEnumerator *enumerator;

  enumerator = g_object_new (G_TYPE_PROXY_ADDRESS_ENUMERATOR,
			     "uri", uri,
			     NULL);

  printf ("Proxies for URI '%s' are:\n", uri);
  run_with_enumerator (synchronous, enumerator);

  g_object_unref (enumerator);
}

typedef enum
{
  USE_RESOLVER,
  USE_ENUMERATOR,
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
      else if (!strcmp (argv[1], "-e"))
        type = USE_ENUMERATOR;
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
    case USE_ENUMERATOR:
      use_enumerator (synchronous);
      break;
    }

  return return_value;
}
