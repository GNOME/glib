/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 * GObject introspection: typelib validation, auxiliary functions
 * related to the binary typelib format
 *
 * Copyright (C) 2011 Colin Walters
 * Copyright (C) 2020 Gisle Vanem
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

#include "gdump.c"
#ifdef G_OS_WIN32
  #include <windows.h>
  #include <io.h>  /* For _get_osfhandle() */
  #include <gio/gwin32outputstream.h>
#else
  #include <gio/gunixoutputstream.h>
#endif

int
main (int    argc,
      char **argv)
{
  int i;
  GOutputStream *Stdout;
  GModule *self;

#if defined(G_OS_WIN32)
  HANDLE *hnd = (HANDLE) _get_osfhandle (1);

  g_return_val_if_fail (hnd && hnd != INVALID_HANDLE_VALUE, 1);
  Stdout = g_win32_output_stream_new (hnd, FALSE);
#else
  Stdout = g_unix_output_stream_new (1, FALSE);
#endif

  self = g_module_open (NULL, 0);

  for (i = 1; i < argc; i++)
    {
      GError *error = NULL;
      GType type;

      type = invoke_get_type (self, argv[i], &error);
      if (!type)
	{
	  g_printerr ("%s\n", error->message);
	  g_clear_error (&error);
	}
      else
	dump_type (type, argv[i], Stdout);
    }

  return 0;
}
