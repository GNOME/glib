/*
 * Copyright © 2020 Endless Mobile, Inc.
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
 *
 * Author: Philip Withnall <withnall@endlessm.com>
 */

/*
 * SECTION:trace
 * @Title: Performance tracing
 * @Short_description: Functions for measuring and tracing performance
 *
 * The performance tracing functions allow for the performance of code using
 * GLib to be measured by passing metrics from the current process to an
 * external measurement process such as `sysprof-cli` or `sysprofd`.
 *
 * They are designed to execute quickly, especially in the common case where no
 * measurement process is connected. They are guaranteed to not block the caller
 * and are guaranteed to have zero runtime cost if tracing support is disabled
 * at configure time.
 *
 * Tracing information can be provided as ‘marks’ with a start time and
 * duration; or as marks with a start time and no duration. Marks with a
 * duration are intended to show the execution time of a piece of code. Marks
 * with no duration are intended to show an instantaneous performance problem,
 * such as an unexpectedly large allocation, or that a slow path has been taken
 * in some code.
 *
 * |[<!-- language="C" -->
 * gint64 begin_time_nsec G_GNUC_UNUSED;
 *
 * begin_time_nsec = G_TRACE_CURRENT_TIME;
 *
 * // some code which might take a while
 *
 * g_trace_mark (begin_time_nsec, G_TRACE_CURRENT_TIME - begin_time_nsec,
 *               "GLib", "GSource.dispatch",
 *               "%s ⇒ %s", g_source_get_name (source), need_destroy ? "destroy" : "keep");
 * ]|
 *
 * The tracing API is currently internal to GLib.
 *
 * Since: 2.66
 */

#include "config.h"

#include "gtrace-private.h"

#include <stdarg.h>

/*
 * g_trace_mark:
 * @begin_time_nsec: start time of the mark, as returned by %G_TRACE_CURRENT_TIME
 * @duration_nsec: duration of the mark, in nanoseconds
 * @group: name of the group for categorising this mark
 * @name: name of the mark
 * @message_format: format for the detailed message for the mark, in `printf()` format
 * @...: arguments to substitute into @message_format; none of these should have
 *    side effects
 *
 * Add a mark to the trace, starting at @begin_time_nsec and having length
 * @duration_nsec (which may be zero). The @group should typically be `GLib`,
 * and the @name should concisely describe the call site.
 *
 * All of the arguments to this function must not have side effects, as the
 * entire function call may be dropped if sysprof support is not available.
 *
 * Since: 2.66
 */
void
(g_trace_mark) (gint64       begin_time_nsec,
                gint64       duration_nsec,
                const gchar *group,
                const gchar *name,
                const gchar *message_format,
                ...)
{
#ifdef HAVE_SYSPROF
  va_list args;

  va_start (args, message_format);
  sysprof_collector_mark_vprintf (begin_time_nsec, duration_nsec, group, name, message_format, args);
  va_end (args);
#endif  /* HAVE_SYSPROF */
}
