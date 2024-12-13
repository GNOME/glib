/* Unit test for VEH on Windows
 * Copyright (C) 2019 Руслан Ижбулатов
 *
 * SPDX-License-Identifier: LicenseRef-old-glib-tests
 *
 * This work is provided "as is"; redistribution and modification
 * in whole or in part, in any medium, physical or electronic is
 * permitted without restriction.
 *
 * This work is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * In no event shall the authors or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even
 * if advised of the possibility of such damage.
 */

#include "config.h"

#include <glib.h>
#include <gprintf.h>
#include <stdio.h>
#include <windows.h>

#define COBJMACROS
#include <wincodec.h>

static char *argv0 = NULL;

#include "../gwin32-private.c"

static void
test_subst_pid_and_event (void)
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
  g_assert_false (_g_win32_subst_pid_and_event_w (debugger_3, G_N_ELEMENTS (debugger_3),
                                                  L"%f", 0, 0));

  g_assert_false (_g_win32_subst_pid_and_event_w (debugger_3, G_N_ELEMENTS (debugger_3),
                                                  L"string longer than 10", 0, 0));
  /* 200 is longer than %e, so the string doesn't fit by 1 byte */
  g_assert_false (_g_win32_subst_pid_and_event_w (debugger_not_enough, G_N_ELEMENTS (debugger_not_enough),
                                                  not_enough, 10, 200));

  /* This should fit */
  g_assert_true (_g_win32_subst_pid_and_event_w (debugger_enough, G_N_ELEMENTS (debugger_enough),
                                                 not_enough, 10, 200));
  debugger_enough_utf8 = g_utf16_to_utf8 (debugger_enough, -1, NULL, NULL, NULL);
  g_assert_cmpstr (debugger_enough_utf8, ==, "too long when 200 and 10 are substituted");
  g_free (debugger_enough_utf8);

  g_assert_true (_g_win32_subst_pid_and_event_w (debugger_big, G_N_ELEMENTS (debugger_big),
                                                 L"multipl%e big %e %entries and %pids are %provided here", bp, be));
  debugger_big_utf8 = g_utf16_to_utf8 (debugger_big, -1, NULL, NULL, NULL);
  output = g_strdup_printf ("multipl%llu big %llu %lluntries and %luids are %lurovided here", (guint64) be, (guint64) be, (guint64) be, bp, bp);
  g_assert_cmpstr (debugger_big_utf8, ==, output);
  g_free (debugger_big_utf8);
  g_free (output);
}

/* Crash with access violation */
static void
test_access_violation (void)
{
  int *integer = NULL;
  /* Use SEM_NOGPFAULTERRORBOX to prevent an error dialog
   * from being shown.
   */
  DWORD dwMode = SetErrorMode (SEM_NOGPFAULTERRORBOX);
  SetErrorMode (dwMode | SEM_NOGPFAULTERRORBOX);
  *integer = 1;
  SetErrorMode (dwMode);
}

/* Crash with illegal instruction */
static void
test_illegal_instruction (void)
{
  DWORD dwMode = SetErrorMode (SEM_NOGPFAULTERRORBOX);
  SetErrorMode (dwMode | SEM_NOGPFAULTERRORBOX);
  RaiseException (EXCEPTION_ILLEGAL_INSTRUCTION, 0, 0, NULL);
  SetErrorMode (dwMode);
}

static void
test_veh_crash_access_violation (void)
{
  g_unsetenv ("G_DEBUGGER");
  /* Run a test that crashes */
  g_test_trap_subprocess ("/win32/subprocess/access_violation", 0,
                          G_TEST_SUBPROCESS_DEFAULT);
  g_test_trap_assert_failed ();
}

static void
test_veh_crash_illegal_instruction (void)
{
  g_unsetenv ("G_DEBUGGER");
  /* Run a test that crashes */
  g_test_trap_subprocess ("/win32/subprocess/illegal_instruction", 0,
                          G_TEST_SUBPROCESS_DEFAULT);
  g_test_trap_assert_failed ();
}

static void
test_veh_debug (void)
{
  /* Set up a debugger to be run on crash */
  gchar *command = g_strdup_printf ("%s %s", argv0, "%p %e");
  g_setenv ("G_DEBUGGER", command, TRUE);
  /* Because the "debugger" here is not really a debugger,
   * it can't write into stderr of this process, unless
   * we allow it to inherit our stderr.
   */
  g_setenv ("G_DEBUGGER_OLD_CONSOLE", "1", TRUE);
  g_free (command);
  /* Run a test that crashes and runs a debugger */
  g_test_trap_subprocess ("/win32/subprocess/debuggee", 0,
                          G_TEST_SUBPROCESS_DEFAULT);
  g_test_trap_assert_failed ();
  g_test_trap_assert_stderr ("Debugger invoked, attaching to*");
}

static void
test_veh_debuggee (void)
{
  /* Crash */
  test_access_violation ();
}

static void
veh_debugger (int argc, char *argv[])
{
  char *end;
  DWORD pid = strtoul (argv[1], &end, 10);
  guintptr event = (guintptr) _strtoui64 (argv[2], &end, 10);
  /* Unfreeze the debuggee and announce ourselves */
  SetEvent ((HANDLE) event);
  CloseHandle ((HANDLE) event);
  g_fprintf (stderr, "Debugger invoked, attaching to %lu and signalling %" G_GUINTPTR_FORMAT, pid, event);
}

static void
test_clear_com (void)
{
  IWICImagingFactory *o = NULL;
  IWICImagingFactory *tmp;

  CoInitialize (NULL);
  g_win32_clear_com (&o);
  g_assert_null (o);
  g_assert_true (SUCCEEDED (CoCreateInstance (&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, &IID_IWICImagingFactory, (void **)&tmp)));
  g_assert_nonnull (tmp);
  IWICImagingFactory_QueryInterface (tmp, &IID_IWICImagingFactory, (void **)&o); /* IWICImagingFactory_QueryInterface increments tmp's refcount */
  g_assert_nonnull (o);
  g_assert_cmpint (IWICImagingFactory_AddRef (tmp), ==, 3); /* tmp's refcount incremented, again */
  g_win32_clear_com (&o);  /* tmp's refcount decrements */
  g_assert_null (o);
  g_assert_cmpint (IWICImagingFactory_Release (tmp), ==, 1);   /* tmp's refcount decrements, again */
  g_win32_clear_com (&tmp);
  g_assert_null (tmp);

  CoUninitialize ();
}

int
main (int   argc,
      char *argv[])
{
  argv0 = argv[0];

  g_test_init (&argc, &argv, NULL);

  if (argc > 2)
    {
      veh_debugger (argc, argv);
      return 0;
    }

  g_test_add_func ("/win32/substitute-pid-and-event", test_subst_pid_and_event);

  g_test_add_func ("/win32/veh/access_violation", test_veh_crash_access_violation);
  g_test_add_func ("/win32/veh/illegal_instruction", test_veh_crash_illegal_instruction);
  g_test_add_func ("/win32/veh/debug", test_veh_debug);

  g_test_add_func ("/win32/subprocess/debuggee", test_veh_debuggee);
  g_test_add_func ("/win32/subprocess/access_violation", test_access_violation);
  g_test_add_func ("/win32/subprocess/illegal_instruction", test_illegal_instruction);
  g_test_add_func ("/win32/com/clear", test_clear_com);

  return g_test_run();
}
