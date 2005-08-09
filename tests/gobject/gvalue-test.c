/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
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

/*
 * Modified by the GLib Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GLib Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GLib at ftp://ftp.gtk.org/pub/gtk/. 
 */

#undef G_DISABLE_ASSERT
#undef G_LOG_DOMAIN

#include <string.h>

#include <glib.h>
#include <glib-object.h>

static void
test_enum_transformation (void)
{ 
  GType type; 
  GValue orig = { 0, };
  GValue xform = { 0, }; 
  GEnumValue values[] = { {0,"0","0"}, {1,"1","1"}}; 
  
 type = g_enum_register_static ("TestEnum", values); 
  
 g_value_init (&orig, type); 
 g_value_set_enum (&orig, 1); 

 memset (&xform, 0, sizeof (GValue));
 g_value_init (&xform, G_TYPE_CHAR); 
 g_value_transform (&orig, &xform); 
 g_assert (g_value_get_char (&xform) == 1);

 memset (&xform, 0, sizeof (GValue));
 g_value_init (&xform, G_TYPE_UCHAR); 
 g_value_transform (&orig, &xform); 
 g_assert (g_value_get_uchar (&xform) == 1);

 memset (&xform, 0, sizeof (GValue));
 g_value_init (&xform, G_TYPE_INT); 
 g_value_transform (&orig, &xform); 
 g_assert (g_value_get_int (&xform) == 1);

 memset (&xform, 0, sizeof (GValue));
 g_value_init (&xform, G_TYPE_UINT); 
 g_value_transform (&orig, &xform); 
 g_assert (g_value_get_uint (&xform) == 1);

 memset (&xform, 0, sizeof (GValue));
 g_value_init (&xform, G_TYPE_LONG); 
 g_value_transform (&orig, &xform); 
 g_assert (g_value_get_long (&xform) == 1);

 memset (&xform, 0, sizeof (GValue));
 g_value_init (&xform, G_TYPE_ULONG); 
 g_value_transform (&orig, &xform); 
 g_assert (g_value_get_ulong (&xform) == 1);

 memset (&xform, 0, sizeof (GValue));
 g_value_init (&xform, G_TYPE_INT64); 
 g_value_transform (&orig, &xform); 
 g_assert (g_value_get_int64 (&xform) == 1);

 memset (&xform, 0, sizeof (GValue));
 g_value_init (&xform, G_TYPE_UINT64); 
 g_value_transform (&orig, &xform); 
 g_assert (g_value_get_uint64 (&xform) == 1);
}

int
main (int argc, char *argv[])
{
  g_type_init (); 
  
  test_enum_transformation ();

  return 0;
}
