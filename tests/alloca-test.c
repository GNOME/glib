/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#undef G_LOG_DOMAIN

#include <stdio.h>
#include <string.h>
#include "../glib.h"

#define GLIB_TEST_STRING "el dorado "
#define GLIB_TEST_STRING_5 "el do"

typedef struct {
	guint age;
	gchar name[40];
} GlibTestInfo;

int
main (int   argc,
      char *argv[])
{
#ifdef G_HAVE_ALLOCA
  gchar *string;
  GlibTestInfo *gti;
  gint i, j;

  string = g_alloca(80);
  g_assert(string != NULL);
  for (i = 0; i < 80; i++)
    string[i] = 'x';
  string[79] = 0;
  g_assert(strlen(string) == 79);

  gti = g_new_a(GlibTestInfo, 2);
  string = g_alloca(2);
  strcpy(string, "x");
  for (i = 0; i < 2; i++) {
    for (j = 0; j < 40; j++)
      gti[i].name[j] = 'x';
    gti[i].name[39] = 0;
    g_assert(strlen(gti[i].name) == 39);
    gti[i].age = 42;
  }
  g_assert(strcmp(string, "x") == 0);

  string = g_new0_a(char, 40);
  for (i = 0; i < 39; i++)
    string[i] = 'x';
  g_assert(strlen(string) == 39);

  g_strdup_a(string, GLIB_TEST_STRING);
  g_assert(string != NULL);
  g_assert(strcmp(string, GLIB_TEST_STRING) == 0);
  g_strdup_a(string, NULL);
  g_assert(string == NULL);

  g_strndup_a(string, GLIB_TEST_STRING, 5);
  g_assert(string != NULL);
  g_assert(strlen(string) == 5);
  g_assert(strcmp(string, GLIB_TEST_STRING_5) == 0);
  g_strndup_a(string, NULL, 20);
  g_assert(string == NULL);

  g_strconcat3_a(string, GLIB_TEST_STRING, GLIB_TEST_STRING, GLIB_TEST_STRING);
  g_assert(string != NULL);
  g_assert(strcmp(string, GLIB_TEST_STRING GLIB_TEST_STRING
  			  GLIB_TEST_STRING) == 0);

#else
  exit(77); /* tell automake test was skipped */
#endif

  return 0;
}

