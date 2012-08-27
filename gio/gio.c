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

#include "gdbusprivate.h"
#include "gioprivate.h"

void
g_io_deinit (void)
{
  _g_dbus_deinitialize ();
  _g_local_file_deinit ();
  _g_socket_connection_factory_deinit ();
  _g_io_module_deinit ();
  _g_cancellable_deinit ();
  _g_io_scheduler_deinit ();
  _g_proxy_resolver_deinit ();
  _g_resolver_deinit ();
}
