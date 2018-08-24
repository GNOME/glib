/*
 * Copyright 2015 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Matthias Clasen <mclasen@redhat.com>
 */

#include "config.h"

#include <gio/gio.h>
#include <gi18n.h>
#include <locale.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

static void
network_changed (GNetworkMonitor *nm,
                 gboolean available,
                 gpointer data)
{
  g_print ("::network-changed available: %d\n", available);
}

static void
notify (GObject *object,
        GParamSpec *pspec,
        gpointer data)
{
  GNetworkMonitor *nm = G_NETWORK_MONITOR (object);

  g_print ("notify::");
  if (strcmp (pspec->name, "network-available") == 0)
    g_print ("network-available: %d\n", g_network_monitor_get_network_available (nm));
  else if (strcmp (pspec->name, "network-metered") == 0)
    g_print ("network-metered: %d\n", g_network_monitor_get_network_metered (nm));
  else if (strcmp (pspec->name, "connectivity") == 0)
    g_print ("connectivity: %d\n", g_network_monitor_get_connectivity (nm));
}

static gboolean
check_google (gpointer data)
{
  GNetworkMonitor *nm = data;
  g_autoptr(GSocketAddress) address = NULL;
  g_autoptr(GError) error = NULL;
  gboolean reachable;

  address = g_network_address_new ("www.google.com", 8080);
  reachable = g_network_monitor_can_reach (nm, G_SOCKET_CONNECTABLE (address), NULL, &error);
  if (error)
    g_print ("CanReach returned error: %s\n", error->message);
  else
    g_print ("%s:%d is %s\n",
             g_network_address_get_hostname (G_NETWORK_ADDRESS (address)),
             g_network_address_get_port (G_NETWORK_ADDRESS (address)),
             reachable ? "reachable" : "unreachable");
  
  return G_SOURCE_CONTINUE;
}

int
main (int argc, char **argv)
{
  GNetworkMonitor *nm;

  setlocale (LC_ALL, "");
  textdomain (GETTEXT_PACKAGE);
  bindtextdomain (GETTEXT_PACKAGE, GLIB_LOCALE_DIR);

#ifdef HAVE_BIND_TEXTDOMAIN_CODESET
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
#endif

  nm = g_network_monitor_get_default ();
  g_print ("Using %s\n", g_type_name_from_instance (nm));

  g_timeout_add (1000, check_google, nm);
  g_signal_connect (nm, "network-changed", G_CALLBACK (network_changed), NULL);
  g_signal_connect (nm, "notify", G_CALLBACK (notify), NULL);

  while (TRUE)
    g_main_context_iteration (NULL, TRUE);
  
  return 1;
}
