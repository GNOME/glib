/*
 * Copyright © 2019 Руслан Ижбулатов
 * Copyright © 2025 Luca Bacci
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

#include "config.h"

#include <glib.h>
#include <glib/gwin32private.h>

#include <windows.h>

static void
test_substitute_pid_and_event (void)
{
  const wchar_t not_enough[] = L"too long when %e and %p are substituted";
  wchar_t debugger_3[3];
  wchar_t debugger_not_enough[G_N_ELEMENTS (not_enough)];
  wchar_t debugger_enough[G_N_ELEMENTS (not_enough) + 1];
  char *debugger_enough_utf8;
  wchar_t debugger_big[65535] = {0};
  char *debugger_big_utf8;
  gchar *output;
  guintptr be = (guintptr) 0xFFFFFFFF;
  DWORD bp = MAXDWORD;

  /* %f is not valid */
  g_assert_false (g_win32_substitute_pid_and_event (debugger_3, G_N_ELEMENTS (debugger_3),
                                                    L"%f", 0, 0));

  g_assert_false (g_win32_substitute_pid_and_event (debugger_3, G_N_ELEMENTS (debugger_3),
                                                    L"string longer than 10", 0, 0));
  /* 200 is longer than %e, so the string doesn't fit by 1 byte */
  g_assert_false (g_win32_substitute_pid_and_event (debugger_not_enough, G_N_ELEMENTS (debugger_not_enough),
                                                    not_enough, 10, 200));

  /* This should fit */
  g_assert_true (g_win32_substitute_pid_and_event (debugger_enough, G_N_ELEMENTS (debugger_enough),
                                                   not_enough, 10, 200));
  debugger_enough_utf8 = g_utf16_to_utf8 (debugger_enough, -1, NULL, NULL, NULL);
  g_assert_cmpstr (debugger_enough_utf8, ==, "too long when 200 and 10 are substituted");
  g_free (debugger_enough_utf8);

  g_assert_true (g_win32_substitute_pid_and_event (debugger_big, G_N_ELEMENTS (debugger_big),
                                                   L"multipl%e big %e %entries and %pids are %provided here", bp, be));
  debugger_big_utf8 = g_utf16_to_utf8 (debugger_big, -1, NULL, NULL, NULL);
  output = g_strdup_printf ("multipl%llu big %llu %lluntries and %luids are %lurovided here", (guint64) be, (guint64) be, (guint64) be, bp, bp);
  g_assert_cmpstr (debugger_big_utf8, ==, output);
  g_free (debugger_big_utf8);
  g_free (output);
}

int
main (int   argc,
      char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/win32/substitute-pid-and-event", test_substitute_pid_and_event);

  return g_test_run();
}
