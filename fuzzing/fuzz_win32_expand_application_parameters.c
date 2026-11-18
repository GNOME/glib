/*
 * Copyright 2026 Philip Withnall
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
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
 */

#include "fuzz.h"
#include "giowin32-private.h"

int
LLVMFuzzerTestOneInput (const unsigned char *data, size_t size)
{
  unsigned char *nul_terminated_data = NULL;
  GList *objs = NULL, *remaining_objs;  /* (element-type file_or_uri) */
  int argc = 0;
  char **argv = NULL;
  GError *local_error = NULL;
  gboolean retval;

  fuzz_set_logging_func ();

  /* ignore @size (_g_win32_expand_application_parameters() doesn’t support it);
   * ensure @data is nul-terminated */
  nul_terminated_data = (unsigned char *) g_strndup ((const gchar *) data, size);

  for (unsigned int i = 0; i < 10; i++)
    {
      file_or_uri *obj = g_new0 (file_or_uri, 1);
      obj->file = g_strdup_printf ("/some/path%u", i);
      objs = g_list_prepend (objs, g_steal_pointer (&obj));
    }

  remaining_objs = objs;
  retval = _g_win32_expand_application_parameters ("app-name",
                                                   (char *) nul_terminated_data,
                                                   &remaining_objs,
                                                   &argc,
                                                   &argv,
                                                   &local_error);

  if (retval)
    {
      g_assert_cmpint (argc, >, 0);
      g_assert_nonnull (argv);
      g_assert_cmpstr (argv[0], !=, "");
    }

  g_clear_list (&objs, (GDestroyNotify) free_file_or_uri);
  g_clear_pointer (&argv, g_strfreev);
  g_clear_error (&local_error);
  g_free (nul_terminated_data);

  return 0;
}
