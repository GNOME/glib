/* GLib testing framework examples and tests
 *
 * Copyright © 2009 Canonical Limited
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
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include <glib.h>

#ifdef G_OS_WIN32
#include <stdlib.h>
#endif

int
main (int    argc,
      char **argv)
{
  /* We expect this test to abort, so try to avoid that creating a coredump */
  g_test_disable_crash_reporting ();

#if defined(G_OS_WIN32) && defined(_UCRT)
  /* Without this, abort() terminates the process with a fast-fail exception
   * (0xC0000409) instead of exit code 3. g_test_init() does the same. */
  _set_abort_behavior (0, _CALL_REPORTFAULT);
#endif

  g_assert (42 < 0);
  return 0;
}
