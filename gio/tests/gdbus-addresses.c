/* GLib testing framework examples and tests
 *
 * Copyright (C) 2008-2009 Red Hat, Inc.
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
 * Author: David Zeuthen <davidz@redhat.com>
 */

#include <gio/gio.h>

#ifdef G_OS_UNIX
#include <gio/gunixsocketaddress.h>
#endif

/* ---------------------------------------------------------------------------------------------------- */

#ifdef G_OS_UNIX
static void
test_unix_address (void)
{
  g_assert (!g_dbus_is_supported_address ("some-imaginary-transport:foo=bar", NULL));
  g_assert (g_dbus_is_supported_address ("unix:path=/tmp/dbus-test", NULL));
  g_assert (g_dbus_is_supported_address ("unix:abstract=/tmp/dbus-another-test", NULL));
  g_assert (g_dbus_is_address ("unix:foo=bar"));
  g_assert (!g_dbus_is_supported_address ("unix:foo=bar", NULL));
  g_assert (!g_dbus_is_address ("unix:path=/foo;abstract=/bar"));
  g_assert (!g_dbus_is_supported_address ("unix:path=/foo;abstract=/bar", NULL));
  g_assert (g_dbus_is_supported_address ("unix:path=/tmp/concrete;unix:abstract=/tmp/abstract", NULL));
  g_assert (g_dbus_is_address ("some-imaginary-transport:foo=bar"));

  g_assert (g_dbus_is_address ("some-imaginary-transport:foo=bar;unix:path=/this/is/valid"));
  g_assert (!g_dbus_is_supported_address ("some-imaginary-transport:foo=bar;unix:path=/this/is/valid", NULL));
}
#endif

static void
test_nonce_tcp_address (void)
{
  g_assert (g_dbus_is_supported_address ("nonce-tcp:host=localhost,port=42,noncefile=/foo/bar", NULL));
  g_assert (g_dbus_is_supported_address ("nonce-tcp:host=localhost,port=42,noncefile=/foo/bar,family=ipv6", NULL));
  g_assert (g_dbus_is_supported_address ("nonce-tcp:host=localhost,port=42,noncefile=/foo/bar,family=ipv4", NULL));

  g_assert (!g_dbus_is_supported_address ("nonce-tcp:host=localhost,port=42,noncefile=/foo/bar,family=blah", NULL));
  g_assert (!g_dbus_is_supported_address ("nonce-tcp:host=localhost,port=420000,noncefile=/foo/bar,family=ipv4", NULL));
  g_assert (!g_dbus_is_supported_address ("nonce-tcp:host=,port=x42,noncefile=/foo/bar,family=ipv4", NULL));
  g_assert (!g_dbus_is_supported_address ("nonce-tcp:host=,port=42x,noncefile=/foo/bar,family=ipv4", NULL));
  g_assert (!g_dbus_is_supported_address ("nonce-tcp:host=,port=420000,noncefile=/foo/bar,family=ipv4", NULL));
}

int
main (int   argc,
      char *argv[])
{
  g_type_init ();
  g_test_init (&argc, &argv, NULL);

#ifdef G_OS_UNIX
  g_test_add_func ("/gdbus/unix-address", test_unix_address);
#endif
  g_test_add_func ("/gdbus/nonce-tcp-address", test_nonce_tcp_address);
  return g_test_run();
}

