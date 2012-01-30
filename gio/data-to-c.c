/*
 * Copyright © 2011 Red Hat, Inc
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
 * Author: Alexander Larsson <alexl@redhat.com>
 */

 #include <glib.h>

int
main (int argc, char **argv)
{
  char *content;
  int i;

  if (argc != 3)
    return 1;

  if (!g_file_get_contents (argv[1], &content, NULL, NULL))
    return 1;

  g_print ("const char %s[] = \"", argv[2]);
  
  for (i = 0; content[i] != 0; i++) {
    g_print ("\\x%02x", (int)content[i]);
  }
  
  g_print ("\";\n");
  return 0;
}
