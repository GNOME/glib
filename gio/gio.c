/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright (C) 2010 Ole André Vadla Ravnås <oleavr@gmail.com>
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
 */

#include "gio.h"

#include "glib/gconstructor.h"
#include "gioprivate.h"

#ifdef G_DEFINE_CONSTRUCTOR_NEEDS_PRAGMA
#pragma G_DEFINE_CONSTRUCTOR_PRAGMA_ARGS (gio_dtor)
#endif
G_DEFINE_DESTRUCTOR (gio_dtor)

static void
gio_dtor (void)
{
  if (G_LIKELY (!g_mem_do_cleanup))
    return;

  g_cancellable_cleanup ();
  g_dbus_cleanup ();
  g_io_module_cleanup ();
  g_io_scheduler_cleanup ();
  g_local_file_cleanup ();
  g_resolver_cleanup ();
  g_socket_connection_factory_cleanup ();
}
