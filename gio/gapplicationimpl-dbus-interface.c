/*
 * Copyright © 2010 Codethink Limited
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the licence, or (at your option) any later version.
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
 *
 * Author: Ryan Lortie <desrt@desrt.ca>
 */

static const GDBusArgInfo platform_data_arg = { -1, (gchar *) "platform_data", (gchar *) "a{sv}" };

static const GDBusArgInfo open_uris_arg = { -1, (gchar *) "uris", (gchar *) "as" };
static const GDBusArgInfo open_hint_arg = { -1, (gchar *) "hint", (gchar *) "s" };

static const GDBusArgInfo invoke_action_name_arg = { -1, (gchar *) "name", (gchar *) "s" };
static const GDBusArgInfo invoke_action_args_arg = { -1, (gchar *) "args", (gchar *) "v" };

static const GDBusArgInfo cmdline_path_arg = { -1, (gchar *) "path", (gchar *) "o" };
static const GDBusArgInfo cmdline_arguments_arg = { -1, (gchar *) "arguments", (gchar *) "aay" };
static const GDBusArgInfo cmdline_exit_status_arg = { -1, (gchar *) "exit_status", (gchar *) "i" };

static const GDBusArgInfo *activate_in[] = { &platform_data_arg, NULL };
static const GDBusArgInfo *activate_out[] = { NULL };

static const GDBusArgInfo *open_in[] = { &open_uris_arg, &open_hint_arg, &platform_data_arg, NULL };
static const GDBusArgInfo *open_out[] = { NULL };

static const GDBusMethodInfo activate_method = {
  -1, (gchar *) "Activate",
  (GDBusArgInfo **) activate_in,
  (GDBusArgInfo **) activate_out
};

static const GDBusMethodInfo open_method = {
  -1, (gchar *) "Open",
  (GDBusArgInfo **) open_in,
  (GDBusArgInfo **) open_out
};

static const GDBusMethodInfo *application_methods[] = {
  &activate_method, &open_method, NULL
};

const GDBusInterfaceInfo org_gtk_Application = {
  -1, (gchar *) "org.gtk.Application",
  (GDBusMethodInfo **) application_methods
};
