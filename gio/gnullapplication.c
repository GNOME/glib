/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright © 2010 Red Hat, Inc
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
 * Authors: Colin Walters <walters@verbum.org>
 */

#include <string.h>
#include <stdlib.h>

#include "gioerror.h"

static void
_g_application_platform_init (GApplication *app)
{
}

static gboolean
_g_application_platform_acquire_single_instance (GApplication  *app,
                                                 GError       **error)
{
  return TRUE;
}

static void
_g_application_platform_on_actions_changed (GApplication *app)
{
}

static void
_g_application_platform_remote_invoke_action (GApplication  *app,
                                              const gchar   *action,
                                              guint          timestamp)
{
}

static void
_g_application_platform_remote_quit (GApplication *app,
                                     guint         timestamp)
{
}

static void
_g_application_platform_default_quit (void)
{
  exit (0);
}

static void
_g_application_platform_activate (GApplication *app,
                                  GVariant     *data)
{
  exit (0);
}

